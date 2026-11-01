#!/usr/bin/env bash
# run_can_tests.sh --> runs every Renode CAN demo headless and checks the output
#
#   renode/run_can_tests.sh          (or: make -C firmware test)
#
# tests:
#   1 f4_dual      one F405, CAN1 --> CAN2 through a CANHub
#   2 two_boards   obc + experiment on bus A
#   3 redundant    bus A + bus B, duplicates dropped, bus A cut half way
#   4 g431         NUCLEO-G431KB FDCAN1 internal loopback (16 MHz HSI)
#     g431_ext     same in external loopback (g431_ext_loopback.elf)
#     g431_170mhz  internal loopback with the PLL at 170 MHz --> checks the printed
#                  clock and bit timing follow the real clock
#   5 bridge       bus A bridged to Linux vcan0 (skipped if it can't be done)
#
# environment: RENODE = path to renode (default: renode on PATH)
#              CROSS  = toolchain prefix for the build (default: arm-none-eabi- on PATH)
# output: build/renode/*.log
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RENODE="${RENODE:-renode}"
if ! command -v "$RENODE" >/dev/null 2>&1; then
    echo "renode not found: put it on PATH or set RENODE=/path/to/renode" >&2
    exit 1
fi

# Renode workaround: 1.17 can't read absolute paths with spaces (this script
# uses absolute ones) --> work through a symlink
if [[ "$ROOT" == *" "* ]]; then
    LINK="${TMPDIR:-/tmp}/can-sim-${USER:-$(id -u)}"
    ln -sfn "$ROOT" "$LINK"
    ROOT="$LINK"
fi
cd "$ROOT"
OUT="$ROOT/build/renode"
CHECK="python3 -I $ROOT/renode/check_can_logs.py"
mkdir -p "$OUT"

make -s -C firmware >/dev/null

fail=0

# run_renode NAME --> runs $OUT/NAME.resc headless, Renode's own log goes to NAME.renode.log
run_renode() {
    echo "== $1"
    "$RENODE" --disable-gui --console -e "include @$OUT/$1.resc" > "$OUT/$1.renode.log" 2>&1 || true
}

# 1. f4_dual
rm -f "$OUT/f4_dual.uart.log"
cat > "$OUT/f4_dual.resc" <<EOF
include @renode/f4_dual.resc
usart2 CreateFileBackend @$OUT/f4_dual.uart.log true
emulation RunFor "1.25"
q
EOF
run_renode f4_dual
$CHECK f4_dual "$OUT/f4_dual.uart.log" || fail=1

# 2. two boards, one bus
rm -f "$OUT"/two_boards.*.uart.log
cat > "$OUT/two_boards.resc" <<EOF
include @renode/two_boards.resc
mach set "obc"
usart2 CreateFileBackend @$OUT/two_boards.obc.uart.log true
mach set "experiment"
usart2 CreateFileBackend @$OUT/two_boards.experiment.uart.log true
emulation RunFor "2.1"
q
EOF
run_renode two_boards
$CHECK two_boards "$OUT/two_boards.experiment.uart.log" || fail=1

# 3. two redundant buses, bus A cut after 2.05 s
CUT=2.05
rm -f "$OUT"/redundant.*.uart.log
cat > "$OUT/redundant.resc" <<EOF
include @renode/redundant.resc
mach set "obc"
usart2 CreateFileBackend @$OUT/redundant.obc.uart.log true
mach set "experiment"
usart2 CreateFileBackend @$OUT/redundant.experiment.uart.log true
emulation RunFor "$CUT"
# broken bus A --> unplug the OBC from it
mach set "obc"
connector Disconnect sysbus.can1 busA
emulation RunFor "1.5"
q
EOF
run_renode redundant
$CHECK redundant "$OUT/redundant.obc.uart.log" "$OUT/redundant.experiment.uart.log" "$CUT" || fail=1

# 4. G431 FDCAN loopback
# run_g431 NAME ELF [monitor commands to run before starting]
run_g431() {
    rm -f "$OUT/$1.uart.log"
    cat > "$OUT/$1.resc" <<EOF
\$elf=@$ROOT/firmware/build/$2
include @renode/g431_loopback.resc
usart2 CreateFileBackend @$OUT/$1.uart.log true
${3:-}
emulation RunFor "2.3"
q
EOF
    run_renode "$1"
}
run_g431 g431 g431_loopback.elf
$CHECK g431 "$OUT/g431.uart.log" internal 16000000 || fail=1
run_g431 g431_ext g431_ext_loopback.elf
$CHECK g431 "$OUT/g431_ext.uart.log" external 16000000 || fail=1
# PLL from HSI16: /M 4, xN 85, /R 2 = 170 MHz, made SYSCLK like CubeMX would
# for full speed. written from the monitor before the firmware starts
#   RCC_PLLCFGR = PLLREN | N 85 | M-1 3 | PLLSRC HSI16,  RCC_CFGR SW = PLL
run_g431 g431_170mhz g431_loopback.elf "sysbus WriteDoubleWord 0x4002100C 0x01005532
sysbus WriteDoubleWord 0x40021008 0x3"
$CHECK g431 "$OUT/g431_170mhz.uart.log" internal 170000000 || fail=1

# 5. SocketCAN bridge
# needs a vcan0. if the PC has one (sudo commands in socketcan_bridge.resc)
# it gets used. if not, the test makes a private network namespace with its
# own vcan0 (no sudo, nothing changes on the PC's network, but the kernel may
# load the vcan module), and if that can't be done either it's skipped
rm -f "$OUT"/bridge.*.log
cat > "$OUT/bridge.resc" <<EOF
include @renode/socketcan_bridge.resc
mach set "experiment"
usart2 CreateFileBackend @$OUT/bridge.experiment.uart.log true
emulation RunFor "3"
q
EOF
# what runs inside the namespace (or directly): candump, a cansend after 6 s, Renode
cat > "$OUT/bridge.sh" <<EOF
set -e
ip link show vcan0 >/dev/null 2>&1 || { ip link add dev vcan0 type vcan; ip link set up vcan0; }
python3 -I $ROOT/renode/can_tool.py dump vcan0 120 > $OUT/bridge.candump.log &
DUMP=\$!
( sleep 6; python3 -I $ROOT/renode/can_tool.py send vcan0 100#06C8EFBEADDE ) &
"$RENODE" --disable-gui --console -e "include @$OUT/bridge.resc" > $OUT/bridge.renode.log 2>&1 || true
kill \$DUMP
EOF
echo "== bridge"
if ip link show vcan0 >/dev/null 2>&1; then
    bash "$OUT/bridge.sh"
    $CHECK bridge "$OUT/bridge.experiment.uart.log" "$OUT/bridge.candump.log" || fail=1
elif unshare -rn sh -c 'ip link add dev vcan0 type vcan' 2>/dev/null; then
    echo "(no vcan0 on this PC: using a private network namespace)"
    unshare -rn bash "$OUT/bridge.sh"
    $CHECK bridge "$OUT/bridge.experiment.uart.log" "$OUT/bridge.candump.log" || fail=1
else
    echo "SKIP no vcan0, and no private network namespace possible (see socketcan_bridge.resc)"
fi

if grep -h "\[ERROR\]" "$OUT"/*.renode.log >/dev/null 2>&1; then
    echo "WARN: Renode reported errors, see $OUT/*.renode.log"
fi
echo
if (( fail )); then echo "CAN tests: FAILED"; else echo "CAN tests: all passed"; fi
exit $fail
