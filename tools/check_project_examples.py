#!/usr/bin/env python3
"""Check that bundled project examples can pass the firmware's full snapshot gate."""

from pathlib import Path
import re


PROJECTS = tuple(Path(f"projects/{slot}.proj") for slot in range(5)) + (
    Path("projects/stress_test.proj"),
)
EXPECTED_STEPS = {(pattern, track, step) for pattern in range(8)
                  for track in range(8) for step in range(16)}
PROTOCOL = Path("lib/AZ2_Protocol/AZ2_Protocol.h").read_text()


def _protocol_int(name: str) -> int:
    match = re.search(rf"\bconstexpr\s+uint8_t\s+{name}\s*=\s*(\d+)", PROTOCOL)
    assert match, f"{name} introuvable dans AZ2_Protocol.h"
    return int(match.group(1))


ENGINE_COUNT = _protocol_int("kEngineCount")
# Un compte de patches par moteur, DANS L'ORDRE de az2::kEngine* (0=DEXED..
# 8=SPECTRAL, voir kEngineNames[] dans AZ2_Protocol.h -- meme convention
# que engineName()/enginePatchCount() cote C++). GRANULAR/SPECTRAL
# partagent kRackPatchCount (pas de kGranularPatchCount/kSpectralPatchCount
# dedie, voir enginePatchCount()).
#
# Avant le 2026-09-22 (audit complet), cette liste s'arretait a SAMPLER (6
# entrees) alors que az2::kEngineCount valait deja 9 (DRUM, puis
# GRANULAR/SPECTRAL) -- la verification de borne ci-dessous utilisait donc
# un plafond perime, invisible tant qu'aucun projet fourni n'utilisait un
# moteur >= 6 (CI verte par absence de cas de test, pas par correction).
# L'assertion juste en dessous fait desormais echouer ce script bruyamment,
# plutot que de sous-valider en silence, si un moteur est ajoute/retire
# dans AZ2_Protocol.h sans repercuter le changement ici.
ENGINE_PATCH_COUNT_NAMES = (
    "kDexedPatchCount", "kEPianoPatchCount", "kBraidsPatchCount",
    "kKarplusPatchCount", "kAnalogPatchCount", "kSamplerPatchCount",
    "kDrumPatchCount", "kRackPatchCount", "kRackPatchCount",
)
assert len(ENGINE_PATCH_COUNT_NAMES) == ENGINE_COUNT, (
    f"az2::kEngineCount = {ENGINE_COUNT} mais ENGINE_PATCH_COUNT_NAMES n'a "
    f"que {len(ENGINE_PATCH_COUNT_NAMES)} entrees dans "
    f"{Path(__file__).name} -- mets a jour cette liste.")
ENGINE_PATCH_COUNTS = tuple(_protocol_int(name) for name in ENGINE_PATCH_COUNT_NAMES)


def check_project(path: Path) -> None:
    lines = path.read_text().splitlines()
    assert sum(line.startswith("BPM:") for line in lines) == 1, path
    assert sum(line.startswith("SONGLEN:") for line in lines) == 1, path
    tracks = [[int(value) for value in line[6:].split(",")]
              for line in lines if line.startswith("TRACK:")]
    assert sorted(track[0] for track in tracks) == list(range(8)), path
    for track in tracks:
        assert len(track) == 13, (path, track)
        _, engine, patch = track[:3]
        assert 0 <= engine < len(ENGINE_PATCH_COUNTS), (path, track)
        assert 0 <= patch < ENGINE_PATCH_COUNTS[engine], (path, track)
        assert 0 <= track[9] <= 31 and 0 <= track[10] <= 7, (path, track)
        assert 0 <= track[11] <= 127 and track[12] in (0, 1), (path, track)
    steps = [[int(value) for value in line[5:].split(",")]
             for line in lines if line.startswith("STEP:")]
    keys = [tuple(step[:3]) for step in steps]
    assert len(keys) == len(EXPECTED_STEPS) and set(keys) == EXPECTED_STEPS, path
    for step in steps:
        assert len(step) == 10, (path, step)
        assert step[3] in (0, 1) and 0 <= step[4] <= 127, (path, step)
        assert 0 <= step[5] <= 255 and 0 <= step[6] <= 3, (path, step)
        assert 0 <= step[7] <= 255 and 0 <= step[8] <= 100 and 0 <= step[9] <= 255, (path, step)
    print(f"PASS {path}: 8 tracks, {len(steps)} unique steps")


if __name__ == "__main__":
    for project in PROJECTS:
        check_project(project)
