"""galvan_log.py --> reads the payload log files

every record is 256 bytes, little endian, starts with sync 0xA55A and ends
with the CRC-16/CCITT-FALSE of bytes 0..253. two records per 512 byte
sector, the zero padding records have no sync word so they get skipped

using it from other code:
    from galvan_log import read_log, field_from_xy
    hdr, sci, ev, stats = read_log("GAL00001.BIN")
"""
import math
import struct

RECORD_SIZE = 256
SYNC = 0xA55A
TYPE_HEADER, TYPE_SCIENCE, TYPE_EVENT = 0, 1, 2
K_BLOCKS = 10 # same as tools/params.py and app/config.h
M_SAMPLES = 80
INVALID_CYCLE = -32768

STATES = ["INIT", "SELFTEST", "SAFE", "ARMED", "BOOST", "COAST", "APOGEE", "DESCENT", "LANDED"]
RANGES = ["HI", "LO"]
FLAGS = {
    0: "SATURATED", 1: "SETTLING", 2: "INDEX_MISSING", 3: "STEP_SLIP", 4: "BARO_STALE",
    5: "IMU_STALE", 6: "LOW_BATT", 7: "MOTOR_OFF", 8: "QUEUE_OVERFLOW", 9: "SD_RETRY",
    10: "RESUMED", 11: "IMU_CLIP", 12: "DEPLOYED",
}
EVENTS = {1: "STATE_CHANGE", 2: "RANGE_SWITCH", 3: "STEP_SLIP", 4: "STORAGE_ERROR", 5: "SELFTEST",
          6: "RESUMED", 7: "MOTOR", 8: "LOW_BATT", 9: "COMMAND", 10: "DEPLOY",
          11: "POWER_RECOVERY"}
DEPLOY_EVENTS = {1: "COMMANDED", 2: "EXTENDED", 3: "FAILED"} # EV_DEPLOY arg8
RECOVERY_VERDICTS = {0: "PAD", 1: "BOOST", 2: "IN_FLIGHT", 3: "LANDED", 4: "UNKNOWN"} # EV_POWER_RECOVERY arg8
SELFTEST_BITS = {0: "IMU", 1: "BARO", 2: "STORAGE", 3: "VBAT", 4: "ADC_MID", 5: "MOTOR", 6: "INDEX",
                 7: "NO_SPIN"}


def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def flag_names(flags):
    return [name for bit, name in FLAGS.items() if flags & (1 << bit)]


def _cstr(b):
    return b.split(b"\0", 1)[0].decode("ascii", "replace")


def parse_header(b):
    return {
        "seq": struct.unpack_from("<I", b, 4)[0],
        "t_ms": struct.unpack_from("<I", b, 8)[0],
        "magic": _cstr(b[12:20]),
        "fw_version": f"{b[20]}.{b[21]}",
        "git": _cstr(b[22:30]),
        "f_mod_hz": struct.unpack_from("<H", b, 30)[0],
        "fs_hz": struct.unpack_from("<H", b, 32)[0],
        "m_samples": b[34],
        "k_blocks": b[35],
        "steps_per_rev": b[36],
        "apertures": b[37],
        "adc_full_scale": struct.unpack_from("<H", b, 38)[0],
        "reset_cause": struct.unpack_from("<I", b, 40)[0],
        "boot_count": struct.unpack_from("<I", b, 44)[0],
        "resumed_state": None if b[48] == 0xFF else STATES[b[48]] if b[48] < len(STATES) else b[48],
        "selftest_faults": [n for bit, n in SELFTEST_BITS.items() if struct.unpack_from("<H", b, 50)[0] & (1 << bit)],
        "p_ref_pa": struct.unpack_from("<i", b, 52)[0],
        "build": f"{_cstr(b[56:67])} {_cstr(b[68:76])}",
    }


def parse_science(b):
    acc = struct.unpack_from("<3h", b, 46)
    gyr = struct.unpack_from("<3h", b, 52)
    cx = struct.unpack_from(f"<{K_BLOCKS}h", b, 60)
    cy = struct.unpack_from(f"<{K_BLOCKS}h", b, 100)
    state = b[12]
    flags = struct.unpack_from("<H", b, 14)[0]
    mcu_t = struct.unpack_from("<h", b, 38)[0]
    return {
        "seq": struct.unpack_from("<I", b, 4)[0],
        "t_s": struct.unpack_from("<I", b, 8)[0] / 1000.0,
        "state": STATES[state] if state < len(STATES) else str(state),
        "range": RANGES[b[13]] if b[13] < 2 else str(b[13]),
        "flags": flags,
        "flag_names": "|".join(flag_names(flags)),
        "x": struct.unpack_from("<f", b, 16)[0],
        "y": struct.unpack_from("<f", b, 20)[0],
        "raw_min": struct.unpack_from("<H", b, 24)[0],
        "raw_max": struct.unpack_from("<H", b, 26)[0],
        "n_valid": b[28],
        "slip_count": b[29],
        "vbat_v": struct.unpack_from("<H", b, 30)[0] / 1000.0,
        "pressure_pa": struct.unpack_from("<i", b, 32)[0],
        "baro_temp_c": struct.unpack_from("<h", b, 36)[0] / 100.0,
        "mcu_temp_c": None if mcu_t == -32768 else mcu_t / 100.0,
        "alt_m": struct.unpack_from("<i", b, 40)[0] / 100.0,
        "vz_mps": struct.unpack_from("<h", b, 44)[0] / 10.0,
        "ax_g": acc[0] / 1000.0, "ay_g": acc[1] / 1000.0, "az_g": acc[2] / 1000.0,
        "gx_dps": gyr[0] / 10.0, "gy_dps": gyr[1] / 10.0, "gz_dps": gyr[2] / 10.0,
        "rotor_period_us": struct.unpack_from("<H", b, 58)[0],
        "cx": [None if v == INVALID_CYCLE else v for v in cx],
        "cy": [None if v == INVALID_CYCLE else v for v in cy],
        "p_ref_pa": struct.unpack_from("<i", b, 140)[0],
        "t_launch_s": struct.unpack_from("<I", b, 144)[0] / 1000.0,
        "index_phase": None if b[148] == 0xFF else b[148],
        # orientation quaternion w, x, y, z (app/attitude.h), None until its set up
        "quat": [v / 30000.0 for v in struct.unpack_from("<4h", b, 150)] if b[158] else None,
    }


def parse_event(b):
    code = b[12]
    return {
        "seq": struct.unpack_from("<I", b, 4)[0],
        "t_s": struct.unpack_from("<I", b, 8)[0] / 1000.0,
        "code": EVENTS.get(code, str(code)),
        "arg8": b[13],
        "arg16": struct.unpack_from("<H", b, 14)[0],
        "a": struct.unpack_from("<i", b, 16)[0],
        "b": struct.unpack_from("<i", b, 20)[0],
        "text": _cstr(b[24:56]),
    }


def read_log(path):
    """gives back (headers, science, events, stats). finds its way again after corrupted bits"""
    data = open(path, "rb").read()
    headers, science, events = [], [], []
    stats = {"bytes": len(data), "good": 0, "bad_crc": 0, "padding": 0, "resync_bytes": 0}
    i = 0
    while i + RECORD_SIZE <= len(data):
        b = data[i:i + RECORD_SIZE]
        sync = b[0] | (b[1] << 8)
        if sync != SYNC:
            if not any(b):
                stats["padding"] += 1
                i += RECORD_SIZE
                continue
            # corrupted or shifted data --> look byte by byte for the next sync
            # word (lost bytes can leave records at any alignment, annoying)
            stats["resync_bytes"] += 1
            i += 1
            continue
        crc = b[254] | (b[255] << 8)
        if crc != crc16_ccitt(b[:254]):
            stats["bad_crc"] += 1
            i += RECORD_SIZE
            continue
        stats["good"] += 1
        t = b[2]
        if t == TYPE_HEADER:
            headers.append(parse_header(b))
        elif t == TYPE_SCIENCE:
            science.append(parse_science(b))
        elif t == TYPE_EVENT:
            events.append(parse_event(b))
        i += RECORD_SIZE
    return headers, science, events, stats


def field_from_xy(x, y, cal_range, index_phase=None, index_phase_ref=0, m_samples=M_SAMPLES):
    """field at the electrode (V/m) and normalised quadrature from the lock-in outputs

    cal_range: dict with S_counts_per_vpm, phi_deg, E0_vpm for the record's range
    model: v_ac = -S E cos(theta + phi), so
        E = (X sin(phi) - Y cos(phi)) / S - E0,  Q = (X cos(phi) + Y sin(phi)) / S
    the stepper power up phase gets corrected with the index sensor
    phi = phi_cal - 2 pi (index_phase - index_phase_ref) / M
    """
    phi = math.radians(cal_range["phi_deg"])
    if index_phase is not None:
        phi -= 2.0 * math.pi * (index_phase - index_phase_ref) / m_samples
    S = cal_range["S_counts_per_vpm"]
    E = (x * math.sin(phi) - y * math.cos(phi)) / S - cal_range.get("E0_vpm", 0.0)
    Q = (x * math.cos(phi) + y * math.sin(phi)) / S
    return E, Q
