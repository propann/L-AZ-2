#!/usr/bin/env python3
"""Mesure non sonore de la charge des moteurs AZ2 sur le Teensy de production."""

import re
import time

import serial


PORT = "/dev/ttyACM0"
BAUD = 921600
ENGINES = ("DEXED", "EPIANO", "BRAIDS", "KARPLUS", "ANALOG", "SAMPLER", "DRUM")
COUNTS = (1, 2, 4, 8)
INITIAL_ENGINES = (4, 4, 1, 2, 4, 4, 1, 2)
STATS_RE = re.compile(
    r"AZ2:RACK:engines=(.*):cpu=([0-9.]+):cpu_max=([0-9.]+):"
    r"mem=(\d+):mem_max=(\d+):mem_total=(\d+)"
)


def send(port, command):
    port.write((command + "\n").encode("ascii"))
    port.flush()


def read_stats(port, timeout=1.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = port.readline().decode("utf-8", "replace").strip()
        match = STATS_RE.fullmatch(raw)
        if match:
            return {
                "rack": match.group(1),
                "cpu": float(match.group(2)),
                "cpu_max": float(match.group(3)),
                "mem": int(match.group(4)),
                "mem_max": int(match.group(5)),
                "mem_total": int(match.group(6)),
            }
    raise RuntimeError("aucune reponse RACKSTATS valide")


def restore(port):
    send(port, "PANIC")
    for track, engine in enumerate(INITIAL_ENGINES):
        send(port, f"ENGINE:{track}:{engine}")
        send(port, f"PATCH:{track}:0")
        send(port, f"SMODE:{track}:0")
        send(port, f"VOL:{track}:127")
    send(port, "PANIC")


def main():
    rows = []
    with serial.Serial(PORT, BAUD, timeout=0.1) as port:
        time.sleep(1.5)
        port.reset_input_buffer()
        send(port, "STOP")
        for track in range(8):
            send(port, f"VOL:{track}:0")

        try:
            for engine_id, engine_name in enumerate(ENGINES):
                for count in COUNTS:
                    send(port, "PANIC")
                    for track in range(8):
                        send(port, f"ENGINE:{track}:{engine_id if track < count else 4}")
                    send(port, "RACKRESETMAX")
                    time.sleep(0.15)

                    for track in range(count):
                        send(port, f"TEST:{track}:{60 + track}:1")

                    # Les deux moteurs a echantillon sont retriggeres afin de
                    # mesurer leur chemin actif et pas leur etat deja termine.
                    if engine_name in ("SAMPLER", "DRUM"):
                        for _ in range(8):
                            time.sleep(0.08)
                            for track in range(count):
                                send(port, f"TEST:{track}:{60 + track}:1")
                    else:
                        time.sleep(0.65)

                    send(port, "RACKSTATS?")
                    stats = read_stats(port)
                    rows.append((engine_name, count, stats))
                    print(
                        f"{engine_name:7} x{count}: cpu={stats['cpu']:4.1f}% "
                        f"max={stats['cpu_max']:4.1f}% mem={stats['mem']:3}/"
                        f"{stats['mem_total']} max={stats['mem_max']:3}"
                    )
        finally:
            restore(port)
            time.sleep(0.2)
            send(port, "RACKSTATS?")
            restored = read_stats(port)
            print("RESTORE:", restored["rack"])


if __name__ == "__main__":
    main()
