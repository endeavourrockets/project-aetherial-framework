"""decode_log.py - turn a payload log (GALnnnnn.BIN) into CSV files, a summary
and optionally plots, applying a calibration file.

Usage:
    python tools/decode_log.py GAL00001.BIN [--cal tools/nominal_cal.json] [--out DIR] [--plot]

Outputs in DIR (default: next to the log, folder <name>_decoded/):
    header.json   header record(s)
    science.csv   one row per science record with E at the window and,
                  if the calibration gives a vehicle factor, the ambient field
    cycles.csv    per-cycle values (100 Hz) for high-resolution work
    events.csv    event records
    flight.png    field, altitude and acceleration against time (--plot)
"""
import argparse
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from galvan_log import K_BLOCKS, field_from_xy, read_log  # noqa: E402

DEFAULT_CAL = os.path.join(HERE, "nominal_cal.json")


def decode(path, cal_path=DEFAULT_CAL, out=None, plot=False, quiet=False):
    headers, sci, ev, stats = read_log(path)
    cal = json.load(open(cal_path)) if cal_path else None
    out = out or os.path.splitext(path)[0] + "_decoded"
    os.makedirs(out, exist_ok=True)

    with open(os.path.join(out, "header.json"), "w") as f:
        json.dump({"headers": headers, "stats": stats, "calibration": cal}, f, indent=2)

    f_ax = (cal or {}).get("vehicle", {}).get("F_axial")
    ref = (cal or {}).get("index_phase_ref", 0)
    cols = ["seq", "t_s", "state", "range", "flag_names", "n_valid", "x", "y", "E_window_vpm",
            "Q_vpm", "E_ambient_vpm", "raw_min", "raw_max", "index_phase", "slip_count",
            "pressure_pa", "alt_m", "vz_mps", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps",
            "baro_temp_c", "vbat_v", "rotor_period_us"]
    rows = []
    for r in sci:
        r = dict(r)
        r["E_window_vpm"] = r["Q_vpm"] = r["E_ambient_vpm"] = ""
        if cal and r["n_valid"] > 0 and "MOTOR_OFF" not in r["flag_names"]:
            E, Q = field_from_xy(r["x"], r["y"], cal["ranges"][r["range"]], r["index_phase"], ref)
            r["E_window_vpm"], r["Q_vpm"] = round(E, 3), round(Q, 3)
            if f_ax:
                r["E_ambient_vpm"] = round(E / f_ax, 3)
        rows.append(r)

    with open(os.path.join(out, "science.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)

    with open(os.path.join(out, "cycles.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "state", "range", "cycle", "x", "y", "E_window_vpm"])
        for r in rows:
            for k in range(K_BLOCKS):
                if r["cx"][k] is None:
                    continue
                t = r["t_s"] - (K_BLOCKS - 1 - k) * 0.010
                E = ""
                if cal and "MOTOR_OFF" not in r["flag_names"]:
                    E = round(field_from_xy(r["cx"][k], r["cy"][k], cal["ranges"][r["range"]],
                                            r["index_phase"], ref)[0], 2)
                w.writerow([round(t, 3), r["state"], r["range"], k, r["cx"][k], r["cy"][k], E])

    with open(os.path.join(out, "events.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["seq", "t_s", "code", "arg8", "arg16", "a", "b", "text"])
        w.writeheader()
        w.writerows(ev)

    summary = {
        "records_good": stats["good"], "bad_crc": stats["bad_crc"], "padding": stats["padding"],
        "science": len(sci), "events": len(ev), "headers": len(headers),
        "states": [e["text"] for e in ev if e["code"] == "STATE_CHANGE"],
        "range_switches": sum(1 for e in ev if e["code"] == "RANGE_SWITCH"),
        "duration_s": round(sci[-1]["t_s"] - sci[0]["t_s"], 1) if sci else 0,
        "max_alt_m": max((r["alt_m"] for r in sci), default=0),
        "out": out,
    }
    if plot and rows:
        _plot(rows, out)
    if not quiet:
        for k, v in summary.items():
            print(f"{k:15s} {v}")
    return summary, rows, ev


def _plot(rows, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    t = [r["t_s"] for r in rows]
    E = [r["E_window_vpm"] if r["E_window_vpm"] != "" else float("nan") for r in rows]
    fig, ax = plt.subplots(3, 1, sharex=True, figsize=(9, 8))
    ax[0].plot(t, E, lw=0.8)
    ax[0].set_ylabel("E at window (V/m)")
    ax[1].plot(t, [r["alt_m"] for r in rows], lw=0.8)
    ax[1].set_ylabel("Altitude above pad (m)")
    ax[2].plot(t, [r["ax_g"] for r in rows], lw=0.6, label="x")
    ax[2].plot(t, [r["az_g"] for r in rows], lw=0.6, label="z")
    ax[2].set_ylabel("Acceleration (g)")
    ax[2].set_xlabel("Time since power-on (s)")
    ax[2].legend()
    for a in ax:
        a.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "flight.png"), dpi=120)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--cal", default=DEFAULT_CAL, help="calibration JSON (default: nominal)")
    ap.add_argument("--out", default=None)
    ap.add_argument("--plot", action="store_true")
    a = ap.parse_args()
    decode(a.log, a.cal, a.out, a.plot)


if __name__ == "__main__":
    main()
