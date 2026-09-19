# AZ-2 — Project guide (English)

[🇫🇷 Français](README.fr.md) · [🇪🇸 Español](README.es.md) · [GitHub home](../../README.md)

> **Alpha prototype.** “Implemented” means the feature exists in the code; “CI build passed” does not mean it has been verified on physical hardware. Consult the [hardware test log (French)](../AZ2_ETAT_DES_LIEUX.md) before claiming that a feature works reliably on the device.

## Build an instrument

AZ-2 combines a groovebox, an eight-track tracker, six synthesis/sample engines and a Game Boy / Game Boy Color console. The goal is to play, compose, capture game audio and turn a recording into a playable instrument. AZ-2 uses **two boards**, without a multiplexer or an extra ESP rack; the rack belongs to the separate AZ-3 project.

| Board | Responsibilities | Storage |
| --- | --- | --- |
| Teensy 4.1 | Tracker timing, audio engines, MIDI, WAV recording, sample playback, I²S output to the PCM5102A DAC | Its own SD card for recordings; PSRAM for the dynamic sample |
| ESP32-S3 VIEWE UEDX48480040E-WB | 480×480 touch display, menus, GB/GBC emulation and ROM browser | Its own SD card for ROMs and game saves |

Commands and Game Boy audio travel over a shared **921600-baud UART** link. The common contract lives in `lib/AZ2_Protocol/AZ2_Protocol.h`: **update both firmwares and their tests together**. The production audio path is still **V1, 14 kHz mono PCM8**. The stereo V2 pilot exists but is disabled.

## Install and update

First read the [wiring guide (French)](../AZ2_CABLAGE_MASTER.md) and [first-boot safety guide (French)](../AZ2_DEMARRAGE.md). Check voltages, common ground and TX/RX wiring with the power disconnected. Back up both SD cards, especially `.sav`, `.rtc`, ROM and `.wav` files, as well as the previous pair of known-working firmware binaries.

```bash
git clone https://github.com/propann/L-AZ-2.git
cd L-AZ-2
python -m pip install platformio==6.1.19
python tools/check_firmware_contract.py
pio test -e native
pio run -e master_teensy -e screen_esp
# Only after checking the hardware and backing up your data:
pio run -e master_teensy -t upload
pio run -e screen_esp -t upload
```

Build both binaries from the **same Git commit**. Do not update only one board after an incompatible protocol change. `ui_esp` is a legacy bring-up environment, not the final display firmware. Commercial game ROMs and private user content are not included.

## Emulation, saves and music

- **GB/GBC:** Walnut-CGB core, ROM selection from the ESP32 SD card and physical controls. Compatibility, actual frame rate and absence of glitches still require per-game testing, including LSDJ, Tetris and Mario.
- **Cartridge SRAM:** periodic and manual saving to `.sav/.bak`. A save failure prevents unloading the cartridge so RAM modifications are not silently discarded.
- **MBC3 RTC:** separate, versioned `.rtc` file with CRC32 and `.bak` recovery. Offline elapsed time is applied only when the ESP32 system clock is valid; power-loss resilience still needs hardware testing.
- **GB audio:** V1 mono PCM8 at 14 kHz to Teensy. In experimental mode, V2 transports interleaved stereo PCM8 L/R at 14 kHz with CRC16, sequence numbers and negotiation; it is **disabled by default**. The Teensy output bus is still mono, even when V2 is used.
- **Capture → sampler:** `REC:START` / `REC:STOP` write a WAV to the Teensy's SD card. The latest valid WAV can be loaded into an approximately 30-second PSRAM slot and played through `SAMPLER / GB Capture` (patch index 2). Patches 0/1 remain Kick/Snare. Playback respects the source sample rate. A reload is also attempted on boot. A multi-recording browser is not yet implemented.

## Delivered work and traceability — September 19, 2026

| Area | Code delivered | Evidence / limitation |
| --- | --- | --- |
| GB persistence | Separate MBC3 RTC state with CRC32, temporary/backup file rotation and conditional offline clock recovery | [RTC commit](https://github.com/propann/L-AZ-2/commit/44d56dac643593e9f499b872050680185dd9274f); physical RTC/power-loss tests pending |
| Coupled audio | Stereo L/R V2 transport; explicit downmix in Teensy receiver; production V1 unchanged | [Sender](https://github.com/propann/L-AZ-2/commit/e08af9c180c42ed0407f55907dedbb00361cf593) · [Receiver](https://github.com/propann/L-AZ-2/commit/9d0c8263c625264b2a9a2ba88eac7778c03a6a74); pilot not enabled |
| Music workflow | Shared GB Capture patch, WAV-to-PSRAM loading, source-rate-aware playback and boot-time reload | [Sampler integration](https://github.com/propann/L-AZ-2/commit/2aa8fbfc99d2483be684f5009462c3f760410ee7); physical audio/SD validation pending |
| Checks | Dynamic patch test, Teensy and ESP32-S3 firmware builds, native shared-protocol tests against the same revision | [Successful CI run](https://github.com/propann/L-AZ-2/actions/runs/35455889760) for `2ea9e03285814f4ebcb6844b7758ccf5f931891d`; **not a hardware test** |

More detailed documentation is available in the [GB/LSDJ roadmap (French)](../AZ2_GB_ROADMAP_IMPLEMENTATION.md), [V2 audio protocol (French)](../AZ2_PROTOCOL_AUDIO_V2.md), [sampler documentation (French)](../AZ2_SAMPLEUR.md) and [user manual (French)](../AZ2_MANUEL_UTILISATEUR.md). Some older planning documents mention abandoned hardware. Use the current wiring guide, `platformio.ini` and this overview when working on the active two-board configuration.

## Remaining work

Test save files and RTC against power interruptions, measure real FPS/audio on the prototype, qualify games and LSDJ with reproducible results, preserve stereo all the way to the DAC without exhausting Teensy's audio memory, and build multi-recording browsing/editing/assignment. Passing CI does not mean these tasks are complete.

## Contributions and licensing

Useful bug reports include the commit SHA, board, reproduction steps, logs and freely distributable test ROMs. Do not publish commercial ROMs, personal save files or secrets. See [CONTRIBUTING.md](../../CONTRIBUTING.md), [LICENSE](../../LICENSE) and [third-party licenses (French)](../AZ2_LICENCES.md).
