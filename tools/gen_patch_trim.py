#!/usr/bin/env python3
"""Genere src_teensy/az2_audio/az2_patch_level_trim.h depuis les mesures du
banc moteurs (tools/engine_bench.py --csv).

Pour chaque patch : gain = cible / crete mesuree, borne a [0,5 ; 6,0], code en
uint8 (x32 : 16 = 0,5, 32 = 1,0, 192 = 6,0). Plusieurs CSV peuvent etre
fournis : la crete retenue est la plus haute (une note tenue plus longtemps
mesure mieux les attaques lentes).

Les mesures doivent avoir ete faites AVEC kEngineLevelTrim deja applique :
ce tableau corrige l'ecart restant entre patches d'un meme moteur.

Usage : python tools/gen_patch_trim.py balayage.csv [remesure.csv ...]
"""
import csv
import sys
from pathlib import Path

TARGET = 0.15
MIN_GAIN, MAX_GAIN = 0.5, 6.0
ENGINES = {"DEXED": 255, "EPIANO": 105, "BRAIDS": 43, "KARPLUS": 100}
OUT = Path(__file__).resolve().parent.parent / "src_teensy/az2_audio/az2_patch_level_trim.h"


def main():
    peaks = {}
    for path in sys.argv[1:]:
        for row in csv.DictReader(open(path)):
            if row["moteur"] not in ENGINES:
                continue
            key = (row["moteur"], int(row["patch"]))
            peaks[key] = max(peaks.get(key, 0.0), float(row["note"]))

    lines = [
        "// GENERE par tools/gen_patch_trim.py -- ne pas editer a la main.",
        "// Compensation de niveau par patch (x32 : 32 = gain 1,0), cible crete",
        f"// {TARGET} a note 60, bornee [{MIN_GAIN} ; {MAX_GAIN}]. Mesures : banc moteurs",
        "// tools/engine_bench.py, sorties applyGroupGainNow() en plus de",
        "// kEngineLevelTrim. Patch non mesure = 32 (neutre).",
        "#pragma once",
        "#include <stdint.h>",
        "",
    ]
    for name, count in ENGINES.items():
        values = []
        for patch in range(count):
            peak = peaks.get((name, patch))
            gain = 1.0 if not peak or peak <= 0 else min(MAX_GAIN, max(MIN_GAIN, TARGET / peak))
            values.append(round(gain * 32))
        lines.append(f"constexpr uint8_t k{name.title()}PatchTrim[{count}] = {{")
        for i in range(0, count, 16):
            lines.append("    " + ", ".join(str(v) for v in values[i:i + 16]) + ",")
        lines.append("};")
        lines.append("")
    OUT.write_text("\n".join(lines))
    print(f"ecrit {OUT} ({len(peaks)} patches mesures)")


if __name__ == "__main__":
    main()
