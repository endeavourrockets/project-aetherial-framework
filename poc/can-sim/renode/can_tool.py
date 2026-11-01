#!/usr/bin/env python3
"""can_tool.py --> tiny stand-in for candump / cansend (can-utils), only uses
the python standard library so the SocketCAN bridge can be tested on a PC
without can-utils

    python3 renode/can_tool.py dump vcan0 [seconds]      print frames like candump
    python3 renode/can_tool.py send vcan0 100#0306AABB   send one frame like cansend

frame format for send --> hex id, '#', then 0..8 data bytes in hex
Linux only (SocketCAN). the vcan interface has to exist already (sudo
commands in renode/socketcan_bridge.resc)
"""
import socket
import struct
import sys
import time

# struct can_frame from <linux/can.h> --> u32 id, u8 len, 3 pad bytes, 8 data bytes
CAN_FRAME = struct.Struct("<IB3x8s")
CAN_EFF_FLAG = 0x80000000 # extended (29 bit) id ٩(ˊᗜˋ*)و
CAN_RTR_FLAG = 0x40000000 # remote frame


def open_bus(iface):
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((iface,))
    return s


def dump(iface, seconds=None):
    s = open_bus(iface)
    s.settimeout(0.2)
    end = time.monotonic() + seconds if seconds else None
    while end is None or time.monotonic() < end:
        try:
            raw = s.recv(16)
        except socket.timeout:
            continue
        can_id, dlc, data = CAN_FRAME.unpack(raw)
        width = 8 if can_id & CAN_EFF_FLAG else 3
        ident = format(can_id & 0x1FFFFFFF, "0%dX" % width)
        print("  %s  %s   [%d]  %s" % (iface, ident, dlc, " ".join("%02X" % b for b in data[:dlc])), flush=True)


def send(iface, text):
    ident, _, hexdata = text.partition("#")
    data = bytes.fromhex(hexdata)
    if len(data) > 8:
        sys.exit("classic CAN carries at most 8 data bytes")
    can_id = int(ident, 16)
    if len(ident) > 3:
        can_id |= CAN_EFF_FLAG
    open_bus(iface).send(CAN_FRAME.pack(can_id, len(data), data.ljust(8, b"\0")))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "dump":
        dump(sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else None)
    elif len(sys.argv) == 4 and sys.argv[1] == "send":
        send(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)


"""
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⣀⡀⠀⠀⠀⡀⢀⣠⣀⡄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢀⣵⡶⢶⣶⣦⣄⣠⠀⠀⡀⠀⠀⠀⠀⠀⢀⣠⣾⣿⢿⡿⣿⣦⣤⣶⡿⠟⠛⠛⢿⣆⠄⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣀⣿⠋⠀⠀⠀⠈⠙⠻⠿⣿⠿⠿⠿⠿⠿⠿⣿⣿⡿⣯⢿⣽⣻⢿⣿⠁⠀⠀⠀⠀⠈⣿⣀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢺⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣽⢻⣿⣿⣿⣿⣿⣿⣶⣦⢠⣤⣤⣿⣯⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⣸⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣷⣻⢿⣿⣿⣿⡿⣯⣟⡿⣿⣿⣿⢿⡿⣿⣷⡀⠀⠀⠀
⠀⠀⠀⠀⠀⠙⣿⡾⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣾⣯⣟⣿⣿⣿⣽⣷⣻⣽⣿⣿⣿⣿⣻⣽⣿⠁⠀⠀⠀
⠀⠀⠀⠀⠀⣰⡿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠛⠉⠉⠙⠿⠾⠿⣾⣿⣿⣿⡿⣽⣾⢏⠁⠀⠀⠀
⠀⠀⠀⠀⢤⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣾⣽⡿⣿⣀⠀⠀⠀⠀
⠀⠀⠀⠀⣾⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠀⢻⡇⠀⠀⠀⠀
⣀⣀⣄⣀⣿⣇⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⡴⣾⣿⡶⢶⡶⢆
⠚⠉⠉⠙⢻⡏⠉⠉⠀⠀⠀⢠⣿⣿⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⣷⠀⠀⠀⠀⠀⠀⢸⡇⠂⠀⠀⠀
⠀⠀⠀⢠⣺⣿⣤⣤⠀⠀⠀⠈⠻⠟⠀⠀⠀⠀⠀⠀⣀⣤⣤⣄⠀⠀⠀⠀⠀⠘⠻⠋⠀⠀⠀⠀⠐⢳⣿⠷⡾⠦⠔⠀
⠀⠀⠘⠋⠙⠹⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣄⣠⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣼⣏⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣸⣿⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⣿⣿⣿⣎⡀⠀⠀⠀
⠀⠀⠀⢠⣴⠿⠋⠉⠙⢿⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣴⢿⠏⠀⠀⠈⠙⠃⠀⠀
⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠈⠹⣻⣷⣶⣤⣤⣤⣀⣀⣀⣀⣀⣀⣀⣀⣀⣤⣤⣤⣶⣾⠟⡫⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⡀⢀⣴⡾⠛⣹⣿⣿⣿⣿⣿⠉⠉⠉⠉⠉⠉⢩⣿⣿⣿⣿⣯⠙⠻⣾⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢀⣨⡾⠟⠟⠻⣷⣴⣿⣿⣿⣟⣿⣿⣧⣀⠀⠀⢀⣠⣿⣿⣿⣟⣿⣿⣧⣴⡿⠛⠻⢷⣆⠄⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⢠⢿⡅⠀⠀⠀⢸⣿⣿⣿⣟⣾⣿⣻⣿⣿⣿⣿⣿⣿⣿⣿⣯⣿⣟⣿⣿⡇⠀⠀⠀⢠⣿⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠈⣿⡇⠀⠀⠀⣾⣿⣿⢿⣾⣿⣽⡿⣯⣿⣟⣿⣿⣻⣿⣾⣟⣷⣿⣟⣿⣷⠀⠀⠀⢸⣿⡀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠘⢿⣦⣀⣸⣿⣿⡿⣿⣻⣾⣟⣿⣿⣽⣿⣻⣾⡿⣷⣿⣻⣿⣾⢿⣻⣿⣧⣠⣴⡿⠃⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠘⠉⢿⣿⣿⢿⣟⣿⣽⣿⣻⣾⢿⣾⣿⣷⣿⢿⣽⣿⣳⣿⡿⣿⣿⣿⠍⠁⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⡟⠿⠿⠿⢿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠿⠿⠿⢿⣿⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠼⣷⡄⠀⠀⠀⠀⠀⠀⠀⠀⠐⣿⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⠧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⢷⣦⣤⣤⣀⣤⣤⣤⣼⣿⣦⣤⣤⣠⣤⣤⣴⡶⠟⠁⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠈⠉⠉⠉⠉⠁⠀⠀⠀⠉⠉⠉⠉⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
"""