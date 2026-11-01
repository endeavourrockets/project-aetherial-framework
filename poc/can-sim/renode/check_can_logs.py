#!/usr/bin/env python3
"""check_can_logs.py --> checks the console logs of the Renode CAN demos
run_can_tests.sh calls it. prints "ok ..." / "FAIL ..." lines and exits 1 if
anything failed

    check_can_logs.py f4_dual UART_LOG
    check_can_logs.py two_boards EXP_LOG
    check_can_logs.py redundant OBC_LOG EXP_LOG CUT_SECONDS
    check_can_logs.py g431 UART_LOG internal|external CLOCK_HZ
    check_can_logs.py bridge EXP_LOG CANDUMP_LOG
"""
import re
import sys

failures = 0


def check(cond, text):
    global failures
    print(("ok   " if cond else "FAIL ") + text)
    if not cond:
        failures += 1


def read(path):
    with open(path, errors="replace") as f:
        return f.read()


def t_of(line):
    """time stamp '[  1.234]' at the start of a line --> seconds"""
    return float(re.match(r"\[\s*([\d.]+)\]", line).group(1))


def phase_rx(log):
    """(time, bus, seq) of every FLIGHT_PHASE frame the experiment used (not the DUPs)"""
    return [(t_of(l), m.group(1), int(m.group(2)))
            for l in log.splitlines()
            for m in [re.search(r"RX ([AB]) FLIGHT_PHASE .* seq=(\d+)", l)] if m]


def seq_continuous(seqs):
    return seqs == list(range(seqs[0], seqs[0] + len(seqs))) if seqs else False


def f4_dual(path):
    log = read(path)
    seqs = [int(s) for s in re.findall(r"CAN2 RX id=0x100 dlc=6 .* seq=(\d+)", log)]
    check(len(seqs) >= 12, "CAN2 received %d FLIGHT_PHASE frames at 10 Hz (need >= 12)" % len(seqs))
    check(seq_continuous(seqs) and seqs[0] == 0, "sequence numbers 0..%d with no gap or repeat" % (len(seqs) - 1))
    check("id=0x101" not in log, "filter blocked the 0x101 noise frames")
    check("ERROR" not in log and "full" not in log, "no init errors, no full mailboxes")


def two_boards(path):
    log = read(path)
    rx = phase_rx(log)
    check(len(rx) >= 20, "experiment used %d FLIGHT_PHASE frames over bus A (need >= 20)" % len(rx))
    check(all(b == "A" for _, b, _ in rx), "all came on bus A (bus B not connected)")
    check(seq_continuous([s for _, _, s in rx]), "no gap or repeat in seq")
    check("DUP" not in log, "no duplicates with a single bus")
    check(len(re.findall(r"RX A TIME_SYNC", log)) >= 2, "TIME_SYNC received at 1 Hz")
    check("PHASE -> BOOST" in log, "experiment saw the phase change to BOOST")


def redundant(obc_path, exp_path, cut):
    exp = read(exp_path)
    obc = read(obc_path)
    rx = phase_rx(exp)
    seqs = [s for _, _, s in rx]
    dups = [(t_of(l), l) for l in exp.splitlines() if " DUP " in l]
    before = [r for r in rx if r[0] < cut]
    after = [r for r in rx if r[0] > cut + 0.05]
    check(len(rx) >= 30, "experiment used %d FLIGHT_PHASE frames" % len(rx))
    check(seq_continuous(seqs), "each seq used exactly once, no gap, also across the bus A cut")
    check({b for _, b, _ in before} == {"A", "B"}, "before the cut, first copies arrived on both A and B")
    check(len([d for d in dups if d[0] < cut and "0x100" in d[1]]) == len(before),
          "before the cut, every FLIGHT_PHASE had exactly one duplicate, which was ignored")
    check(len(after) >= 10 and all(b == "B" for _, b, _ in after), "after the cut, data kept coming on bus B")
    check(not [d for d in dups if d[0] > cut + 0.05], "after the cut, no more duplicates")
    order = re.findall(r"PHASE -> (\w+)", exp)
    check(order == ["PAD", "ARMED", "BOOST", "COAST", "APOGEE", "DESCENT", "LANDED"],
          "experiment followed every OBC phase in order: " + " ".join(order))
    check(re.search(r"TIME_SYNC obc_ms=\d+ utc=0 offset_ms=0", exp) is not None,
          "TIME_SYNC offset 0 ms (both virtual boards start at t = 0)")
    check("ERROR" not in obc + exp, "no init errors")


def g431(path, mode, clock):
    log = read(path)
    clock = int(clock)
    check(("%s loopback" % mode) in log, "firmware runs in %s loopback mode" % mode)
    check(("FDCAN1 clock %d Hz" % clock) in log, "FDCAN clock worked out from the RCC registers: %d Hz" % clock)
    m = re.search(r"prescaler (\d+) x (\d+) tq.*\nbit rate (\d+) bit/s \(requested 500000, error (-?\d+) ppm\)", log)
    check(m is not None and m.group(3) == "500000" and m.group(4) == "0",
          "bit rate from the timing registers: " + (m.group(0).replace("\n", "; ") if m else "not printed"))
    if m:
        p, tq = int(m.group(1)), int(m.group(2))
        check(p * tq * 500000 == clock, "prescaler x tq x 500 kbit/s = clock (%d x %d)" % (p, tq))
    ok = len(re.findall(r"loopback RX id=0x100 dlc=6 seq=\d+ OK", log))
    check(ok >= 20, "%d frames came back through the loopback (need 20)" % ok)
    check("id=0x101" not in log, "filter blocked the 0x101 noise frames")
    check("LOOPBACK PASS" in log, "firmware reported LOOPBACK PASS")


def bridge(exp_path, dump_path):
    exp = read(exp_path)
    dump = read(dump_path)
    check(len(re.findall(r"vcan0\s+100\s+\[6\]", dump)) >= 10, "PC saw FLIGHT_PHASE (0x100) frames on vcan0")
    check(re.search(r"vcan0\s+080\s+\[8\]", dump) is not None, "PC saw TIME_SYNC (0x080) frames on vcan0")
    check("seq=200 obc_ms=3735928559" in exp, "frame sent from the PC (cansend) reached the experiment board")


if __name__ == "__main__":
    cmd, args = sys.argv[1], sys.argv[2:]
    {"f4_dual": f4_dual, "two_boards": two_boards, "g431": g431, "bridge": bridge,
     "redundant": lambda o, e, c: redundant(o, e, float(c))}[cmd](*args)
    sys.exit(1 if failures else 0)
