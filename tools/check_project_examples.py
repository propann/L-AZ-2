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
ENGINE_NAMES = ("Dexed", "EPiano", "Braids", "Karplus", "Analog", "Sampler")
ENGINE_PATCH_COUNTS = tuple(
    int(re.search(rf"k{name}PatchCount\s*=\s*(\d+)", PROTOCOL).group(1))
    for name in ENGINE_NAMES
)


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
