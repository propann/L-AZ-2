# AZ2 Project (PlatformIO Monorepo)

AZ-2 est une base d'instrument hardware modulaire: Teensy 4.1 pour le moteur audio, ESP32-S3 pour l'ecran et l'interface, RP2040 Pico pour la facade physique.

## Documentation projet

- [Lignes ecran et facade](docs/AZ2_ECRAN_FACADE.md): premiere direction pour l'ecran 4 pouces, la navigation, la facade et le contrat entre cartes.

## Structure

```
AZ2_PROJECT/
├── platformio.ini
├── lib/
├── docs/
├── src_teensy/
├── src_esp32/
└── src_pico/
```

## Environments

- `master_teensy`: Teensy 4.1 audio core (build sources from `src_teensy/`)
- `ui_esp`: ESP32-S3 UI + games (build sources from `src_esp32/`)
- `ctrl_pico`: RP2040 controller (build sources from `src_pico/`)

Shared headers go in `lib/` (example: `AZ2_Protocol.h`).

## Build

Use PlatformIO in VS Code:

- Build: select `env:master_teensy`, `env:ui_esp`, or `env:ctrl_pico`
- Upload: connect the target board and click Upload for that environment
