#!/usr/bin/env python3
"""Mesure la vitesse NES depuis les lignes NES:PERF du firmware."""
import argparse
import re
import statistics
import time

import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", nargs="?", default="/dev/ttyUSB0")
    parser.add_argument("--seconds", type=float, default=15.0)
    args = parser.parse_args()

    rows = []
    pattern = re.compile(r"NES:PERF:fps_x10=(\d+):core_avg_us=(\d+):core_max_us=(\d+):frames=(\d+)")
    with serial.Serial(args.port, 230400, timeout=0.25) as port:
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            line = port.readline().decode(errors="replace").strip()
            match = pattern.search(line)
            if match:
                fps10, avg, peak, frames = map(int, match.groups())
                rows.append((fps10 / 10, avg, peak, frames))
                print(f"NES {fps10 / 10:5.1f} FPS | CPU {avg:6d} us | max {peak:6d} us | frames {frames}")

    if not rows:
        raise SystemExit("Aucune mesure. Lance une ROM NES puis relance l'outil.")
    print("--- bilan ---")
    print(f"FPS moyen : {statistics.mean(row[0] for row in rows):.1f}")
    print(f"CPU moyen : {statistics.mean(row[1] for row in rows):.0f} us")
    print(f"CPU max   : {max(row[2] for row in rows)} us")


if __name__ == "__main__":
    main()
