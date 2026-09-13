# AZ2 Project (PlatformIO Monorepo)

AZ-2 est une groovebox hardware modulaire: Teensy 4.1 pour le moteur audio, DAC I2S pour la sortie son, ESP32-S3 avec ecran 4 pouces pour l'interface, et matrice SparkFun 4x4 bouton + LED pilotee par l'ESP32 via multiplexeurs.

## Documentation projet

- [Lignes ecran, facade et groovebox](docs/AZ2_ECRAN_FACADE.md): direction pour l'ecran ESP32-4848S040C_I 480x480, la navigation, la matrice SparkFun 4x4, les multiplexeurs et le contrat ESP32/Teensy.

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
- `ui_esp`: ESP32-S3 UI + ecran + matrice SparkFun 4x4 (build sources from `src_esp32/`)
- `ctrl_pico`: RP2040 controller optionnel pour extension facade (build sources from `src_pico/`)

Shared headers go in `lib/` (example: `AZ2_Protocol.h`).

## Build

Use PlatformIO in VS Code:

- Build: select `env:master_teensy`, `env:ui_esp`, or `env:ctrl_pico`
- Upload: connect the target board and click Upload for that environment
