#!/usr/bin/env python3
"""Banc de charge AZ2: 16 voix DEXED + effets, sans écriture sur les SD."""

import re
import time

import serial


PORT = "/dev/ttyACM0"
BAUD = 921600
DURATION_SECONDS = 60
INITIAL_ENGINES = (4, 4, 1, 2, 4, 4, 1, 2)
STATS_RE = re.compile(
    r"AZ2:RACK:engines=(.*):cpu=([0-9.]+):cpu_max=([0-9.]+):"
    r"mem=(\d+):mem_max=(\d+):mem_total=(\d+)"
)


def send(port, command):
    port.write((command + "\n").encode("ascii"))
    port.flush()


def wait_for(port, pattern, timeout=1.5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", "replace").strip()
        match = pattern.fullmatch(line)
        if match:
            return match
    raise RuntimeError(f"reponse absente: {pattern.pattern}")


def restore(port):
    send(port, "PANIC")
    send(port, "FX:reverb:0")
    send(port, "FX:delay:0")
    for track, engine in enumerate(INITIAL_ENGINES):
        send(port, f"ENGINE:{track}:{engine}")
        send(port, f"PATCH:{track}:0")
        send(port, f"SMODE:{track}:0")
        send(port, f"VOL:{track}:127")
    send(port, "PANIC")


def main():
    samples = []
    with serial.Serial(PORT, BAUD, timeout=0.1) as port:
        time.sleep(1.0)
        port.reset_input_buffer()
        send(port, "STOP")
        try:
            for track in range(8):
                send(port, f"VOL:{track}:0")
                send(port, f"ENGINE:{track}:0")
                send(port, f"PATCH:{track}:0")
            send(port, "FX:reverb:100")
            send(port, "FX:delay:100")
            send(port, "RACKRESETMAX")
            time.sleep(0.2)

            # Deux notes par piste = la polyphonie interne maximale actuelle.
            for track in range(8):
                send(port, f"TEST:{track}:{48 + track}:1")
                send(port, f"TEST:{track}:{60 + track}:1")

            started = time.monotonic()
            while time.monotonic() - started < DURATION_SECONDS:
                send(port, "RACKSTATS?")
                match = wait_for(port, STATS_RE)
                sample = {
                    "cpu": float(match.group(2)),
                    "cpu_max": float(match.group(3)),
                    "mem": int(match.group(4)),
                    "mem_max": int(match.group(5)),
                    "mem_total": int(match.group(6)),
                }
                samples.append(sample)
                print(
                    f"t={time.monotonic() - started:5.1f}s "
                    f"cpu={sample['cpu']:4.1f}% max={sample['cpu_max']:4.1f}% "
                    f"mem={sample['mem']}/{sample['mem_total']} "
                    f"max={sample['mem_max']}",
                    flush=True,
                )
                time.sleep(1.0)
        finally:
            restore(port)
            time.sleep(0.2)
            send(port, "RACKSTATS?")
            restored = wait_for(port, STATS_RE)
            print("RESTORE:", restored.group(1), flush=True)

    print(
        "RESULT: "
        f"samples={len(samples)} cpu_peak={max(s['cpu_max'] for s in samples):.1f}% "
        f"mem_peak={max(s['mem_max'] for s in samples)}/{samples[0]['mem_total']}",
        flush=True,
    )


if __name__ == "__main__":
    main()
