# AZ2 Project (PlatformIO Monorepo)

AZ-2 est une groovebox hardware modulaire: Teensy 4.1 pour le moteur audio adapte de MicroDexed-touch, DAC PCM5102A/I2S pour la sortie son, ESP32-S3 avec ecran 4 pouces pour l'interface, et matrice SparkFun 4x4 bouton + LED pilotee par l'ESP32 via multiplexeurs.

## Documentation projet

- [Lignes ecran, facade et groovebox](docs/AZ2_ECRAN_FACADE.md): direction pour l'ecran ESP32-4848S040C_I 480x480, la navigation, la matrice SparkFun 4x4, les multiplexeurs et le contrat ESP32/Teensy.
- [Strategie de portage MicroDexed-touch](docs/AZ2_PORTAGE_MICRODEXED_TOUCH.md): quoi reprendre du firmware MicroDexed-touch, quoi remplacer, et comment isoler le moteur audio Teensy.
- [Base de cablage](docs/AZ2_CABLAGE_BASE.md): premiere base PlatformIO pour les deux firmwares, le PCM5102A, la matrice et les multiplexeurs.
- [DAC PCM5102A](docs/AZ2_DAC_PCM5102A.md): specifications utiles, cablage Teensy 4.1 et tests audio.
- [ESP32 controle, Wi-Fi, SD et mode retro](docs/AZ2_ESP32_CONTROLE_WIFI_SD_RETRO.md): usage de la carte SD, du Wi-Fi et piste Game Boy/Retro-Go.
- [Benchmark concurrence](docs/AZ2_BENCHMARK_CONCURRENCE.md): analyse des grooveboxes et projets ouverts a battre/integrer intelligemment.
- [Architecture firmware double cerveau](docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md): repartition claire ESP32/Teensy, protocole, cadences et responsabilites.
- [Feuille de route](docs/AZ2_FEUILLE_DE_ROUTE.md): ordre de construction, criteres de sortie et prochains lots.

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

- `master_teensy`: Teensy 4.1 audio core + PCM5102A I2S (build sources from `src_teensy/az2_audio/`)
- `ui_esp`: ESP32-S3 UI + ecran + matrice SparkFun 4x4 + SD/Wi-Fi hooks (build sources from `src_esp32/az2_control/`)
- `ctrl_pico`: RP2040 controller optionnel pour extension facade (build sources from `src_pico/`)

Shared headers go in `lib/` (example: `AZ2_Protocol.h`).

## Build

Use PlatformIO in VS Code:

- Build: select `env:master_teensy`, `env:ui_esp`, or `env:ctrl_pico`
- Upload: connect the target board and click Upload for that environment
