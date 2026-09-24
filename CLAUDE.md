# CLAUDE.md — AZ-2 collaboration notes

This repository is an active hardware/firmware prototype. **Do not panic when documentation, hardware experiments and roadmap ideas move quickly.** Preserve the distinction between what exists, what has been tested, and what is only planned.

## Read this first

Before making architectural changes, read:

1. `README.md`
2. `docs/user/README.md`
3. `docs/AZ2_AUDIT_CODE_2026-09-24.md`
4. `docs/rack/AZ2_RACK_MOTEURS_ESP.md`
5. `lib/AZ2_Protocol/AZ2_Protocol.h`
6. `.github/workflows/ci.yml`

## Vocabulary

- **CURRENT** — present in the current code/prototype. Hardware validation must still be stated precisely.
- **ROADMAP** — planned; never describe it as shipped.
- **EXPERIMENTAL** — idea/prototype that still needs measurement or reproduction.
- **HISTORY** — retained to explain previous design choices.

## Current physical prototype

The current build is intentionally maker-friendly and uses development modules rather than a final compact PCB:

- Teensy 4.1 for the current tracker/audio side;
- ESP32-S3 in the current screen/UI/GB architecture;
- an ESP32-WROOM-32 DevKit is also part of the physical prototyping work;
- DAC/audio chain, amplifier and headphone output;
- prototype enclosure made from LEGO parts, with some pieces adapted using a Dremel.

Do not silently rewrite the documentation as if a compact production board already exists.

## Future hardware

The rack direction is incremental:

- start with **two ESP engine modules**;
- stress-test the complete machine and measure CPU, memory, latency, audio stability, links, power, temperature and noise;
- only then decide whether slots 3 and 4 are justified;
- smaller ESP module formats are a later miniaturization step;
- safe flashing of engine modules from the AZ-2 screen is ROADMAP;
- a MIDI jack is ROADMAP;
- SparkFun 4×4 + Raspberry Pi Pico and LMN-3 keyboard + Pico are community/experimental controller ideas.

The rack bus, pinout, audio transport and flashing mechanism are **not frozen**. Do not invent them just to complete a diagram.

## Firmware safety rules

The currently visible CI builds `master_teensy` and `screen_esp`. The shared contract lives in `lib/AZ2_Protocol/AZ2_Protocol.h`.

When changing the shared protocol, inspect both endpoints in the same change. Avoid a large behavioral rewrite while splitting the very large `main.cpp` files: refactor incrementally, compile/test after each extraction, and preserve behavior unless the task explicitly calls for a functional change.

Do not confuse:

- implemented;
- compiles;
- tested in native CI;
- tested on real AZ-2 hardware.

Say which one is true.

## Documentation and presentation

The goal is a repository that is both technically credible and visually strong. Prefer the **real prototype** over fake product renders. Firmware screen reconstructions are welcome when they are faithful to code and labelled as reconstructions. Concepts must be labelled CONCEPT/ROADMAP.

Do not erase the LEGO/Dremel stage: it is part of the project's reproducible maker identity.

## Open-project principle

> AZ-2 is not only an open-source instrument. It is a base for inventing your own.

Community variants are welcome. Keep a stable reference architecture while allowing documented alternatives. New hardware ideas belong in `docs/community/` until they are sufficiently tested to graduate into validated documentation.

## Working style

- Prefer small, reviewable commits.
- Keep CI green.
- Do not delete historical research just because the architecture evolved; mark it HISTORY or superseded when appropriate.
- Update documentation alongside behavior/protocol changes.
- Never add commercial ROMs, personal saves, secrets or credentials.
- When uncertain about physical wiring, measured performance or the owner's intended hardware direction, leave a TODO/question rather than fabricating certainty.

The project moves fast, but the rule is simple: **build freely, measure seriously, document truthfully.**
