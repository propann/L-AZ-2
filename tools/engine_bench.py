#!/usr/bin/env python3
"""Banc de test des moteurs audio AZ-2, pilote par le port USB du Teensy.

Pour chaque moteur et quelques patches, sur la piste 1 :
  - joue une note (TEST:), mesure la crete de sortie pendant la note ;
  - relache, attend la fin de release, mesure la crete residuelle
    (note bloquee / souffle) ;
  - releve CPU et memoire audio (CPU?) au pic.

Mesure = AudioAnalyzePeak sur la sortie finale (RACK:STATUS, champ final_l),
donc firmware master_teensy_rack_lab requis. read() rend la crete depuis la
lecture precedente : chaque mesure commence par une lecture de purge.

Usage : python tools/engine_bench.py [--port /dev/ttyACM0] [--track 0]
Le sequenceur doit etre a l'arret. Volume general (encodeur 1) inchange.
"""
import argparse
import re
import sys
import time

import serial

# id moteur -> (nom, patches a tester). Comptes : lib/AZ2_Protocol/AZ2_Protocol.h
ENGINES = {
    0: ("DEXED", [0, 10, 100, 200]),
    1: ("EPIANO", [0, 1, 50, 104]),
    2: ("BRAIDS", [0, 10, 20, 42]),
    3: ("KARPLUS", [0, 25, 50, 99]),
    4: ("ANALOG", [0, 3, 6, 10]),
    5: ("SAMPLER", [0, 1, 2]),
    6: ("DRUM", [0, 1, 2, 3, 4, 5]),
    7: ("GRANULAR", [0, 4]),
    8: ("SPECTRAL", [0, 4]),
}

NOTE = 60
HOLD_S = 0.6
RELEASE_WAIT_S = 2.5
TAIL_S = 0.5
SILENT = 0.002   # sous ce niveau la note est consideree muette
STUCK = 0.01     # au-dessus apres release : note bloquee ou souffle
CLIP = 0.98      # crete proche de la pleine echelle


class Teensy:
    def __init__(self, port):
        self.s = serial.Serial(port, 115200, timeout=0.2)
        time.sleep(0.3)
        self.s.reset_input_buffer()

    def cmd(self, line, wait=0.15):
        self.s.write((line + "\n").encode())
        time.sleep(wait)
        return self.s.read(self.s.in_waiting or 1).decode(errors="replace")

    def ask(self, line, pattern, timeout=1.5):
        self.s.reset_input_buffer()
        self.s.write((line + "\n").encode())
        end = time.time() + timeout
        buf = ""
        while time.time() < end:
            buf += self.s.read(self.s.in_waiting or 1).decode(errors="replace")
            m = re.search(pattern, buf)
            if m:
                return m
        return None

    def peak(self):
        m = self.ask("RACK:STATUS", r"final_l=(-?[\d.]+):final_r=(-?[\d.]+)")
        return max(float(m.group(1)), float(m.group(2))) if m else None

    def cpu(self):
        m = self.ask("CPU?", r"usage=([\d.]+)%:max=([\d.]+)%\s*AZ2:MEM:blocks=(\d+):max=(\d+)")
        return m.groups() if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--track", type=int, default=0)
    args = ap.parse_args()
    t = Teensy(args.port)
    tr = args.track

    t.cmd("PANIC", 0.5)
    t.peak()
    time.sleep(0.5)
    floor = t.peak()
    print(f"plancher de bruit (rien ne joue) : {floor:.5f}")
    print(f"{'moteur':9} {'patch':>5} {'note':>8} {'apres':>8} {'cpu%':>6} {'blocs':>6}  verdict")

    for eid, (name, patches) in ENGINES.items():
        r = t.cmd(f"ENGINE:{tr}:{eid}", 0.4)
        if "ERR" in r:
            print(f"{name:9} indisponible ({r.strip()})")
            continue
        for p in patches:
            t.cmd(f"PATCH:{tr}:{p}", 0.4)
            t.cmd("RACKRESETMAX", 0.05)
            t.peak()
            t.cmd(f"TEST:{tr}:{NOTE}:1", HOLD_S)
            on = t.peak()
            t.cmd(f"TEST:{tr}:{NOTE}:0", RELEASE_WAIT_S)
            t.peak()
            time.sleep(TAIL_S)
            tail = t.peak()
            c = t.cpu()
            cpu_max, blocks_max = (c[1], c[3]) if c else ("?", "?")
            verdict = []
            if on is None or on < SILENT:
                verdict.append("MUET")
            if on is not None and on >= CLIP:
                verdict.append("SATURE")
            if tail is not None and tail > max(STUCK, (floor or 0) * 3):
                verdict.append("NOTE BLOQUEE/SOUFFLE")
            print(f"{name:9} {p:5d} {on if on is not None else -1:8.4f} "
                  f"{tail if tail is not None else -1:8.4f} {cpu_max:>6} {blocks_max:>6}  "
                  f"{', '.join(verdict) or 'ok'}")
        t.cmd("PANIC", 0.5)

    t.cmd(f"ENGINE:{tr}:4", 0.3)  # remet ANALOG, moteur de reference
    t.cmd(f"PATCH:{tr}:0", 0.3)
    return 0


if __name__ == "__main__":
    sys.exit(main())
