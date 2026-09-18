# AZ-2 - Licence et provenance des composants

**[2026-09-17]** Document cree suite a l'audit technique externe du
2026-09-17 (`AZ2_AUDIT_TECHNIQUE_COMPLET_2026-09-17.md`), qui notait a
raison l'absence de licence racine et d'inventaire des composants tiers.

## Licence du projet AZ-2

Le code propre a AZ-2 (`src_teensy/az2_audio/`, `src_esp32/az2_screen/`,
`src_esp32/az2_control/`, `lib/AZ2_Protocol/`, outils, documentation) est
publie sous **GNU GPLv3** (voir `LICENSE` a la racine).

Pourquoi GPLv3 et pas une licence permissive (MIT/Apache) :

1. **Contrainte deja presente, pas un choix arbitraire** -- le moteur EPiano
   (`Synth_MDA_EPiano`, vendored dans `src_teensy/microdexed-touch/third-
   party/`) est sous GPLv3 SEULEMENT (pas de double licence), et il est
   reellement compile dans le binaire `master_teensy` livre (voir
   `#include <synth_mda_epiano.h>` dans `src_teensy/az2_audio/main.cpp`,
   expose via `lib_extra_dirs` dans `platformio.ini`). Un binaire qui
   integre du code GPLv3 est deja, de fait, un travail derive GPLv3 des
   qu'il est distribue -- annoncer une autre licence pour le reste du
   projet aurait ete trompeur.
2. **Coherent avec "on reste libre"** -- le copyleft GPLv3 garantit que
   AZ-2 et ses evolutions (y compris par d'autres personnes) restent
   ouverts, ce qui correspond a l'esprit du projet.

## Composants tiers REELLEMENT compiles dans un binaire livre

| Composant | Utilise par | Licence | Source |
| --- | --- | --- | --- |
| Synth_Dexed | `master_teensy` (moteur DEXED) | Apache-2.0 OU GPLv3 (double licence upstream) | `src_teensy/microdexed-touch/third-party/Synth_Dexed/` |
| Synth_MDA_EPiano | `master_teensy` (moteur EPIANO) | **GPLv3 uniquement** | `src_teensy/microdexed-touch/third-party/Synth_MDA_EPiano/` |
| Synth_Braids | `master_teensy` (moteur BRAIDS) | **MIT** (verifie le 2026-09-17 : chaque fichier du coeur DSP -- `macro_oscillator.*`, `digital_oscillator.*`, `analog_oscillator.*`, `resources.*`, `settings.*`, `dsp.h`, `stmlib.h`, `svf.h`, `random.*`, `excitation.h`, `parameter_interpolation.h` -- porte l'entete MIT original "Copyright 2012/2013 Emilie/Olivier Gillet" de Mutable Instruments ; `murmurhash3.h` est domaine public. Seul le fin wrapper Teensy `synth_braids.h/.cpp` n'a pas d'entete propre, mais son `README.md` cite ses sources -- `github.com/pichenettes/eurorack` (MIT) et `github.com/modlfo/teensy-braids` -- confirmant la meme origine MIT.) | `src_teensy/microdexed-touch/third-party/Synth_Braids/` |
| MIDI (FortySevenEffects/Arduino MIDI Library) | `master_teensy` (`lib_deps`, USB MIDI IN) | MIT | Registre PlatformIO |
| Teensy Audio Library, Encoder, SD (SdFat) | `master_teensy` (framework Teensy) | Majoritairement MIT/PJRC (voir `framework-arduinoteensy`) | Fournis par la plateforme `teensy`, pas vendored dans ce depot |
| GFX Library for Arduino (moononournation, ex-Arduino_GFX) | `screen_esp` (pilote ecran RGB parallele + tactile) | MIT | Registre PlatformIO, version figee `@1.6.7` |
| Walnut-CGB (`walnut_cgb.h`, additions par Mr. Paul sur la base Peanut-GB de Mahyar Koshkouei, elle-meme via le fork de Lior Halphon) | `screen_esp` (emulateur GB/GBC) | MIT (chaque couche : voir l'entete du fichier) | `src_esp32/az2_screen/walnut_cgb/walnut_cgb.h` (vendored, un seul fichier) |
| minigb_apu (Alex Baines, Mahyar Koshkouei) | `screen_esp` (son de l'emulateur GB) | MIT | `src_esp32/az2_screen/minigb_apu/` (vendored), voir son `LICENSE` |
| Kick_1_Simple.wav, Snare_1_Simple.wav | `master_teensy` (moteur SAMPLER, 2026-09-18) | **GPLv3** (meme couverture globale que le reste de MicroDexed-touch -- readme.md du projet : "MicroDexed is licensed on the GPL v3", aucune mention separee pour ce contenu SD, pas de LICENSE dedie dans `addon/SD/`) | `src_teensy/microdexed-touch/addon/SD/CUSTOM/` (vendored), convertis en tableaux PCM 16 bits dans `src_teensy/az2_audio/az2_sampler_data.h` |

## Composants vendored mais PAS compiles (reference uniquement)

Le reste de `src_teensy/microdexed-touch/` (le sketch `.ino` d'origine,
`addon/` -- samples et presets JSON de l'appli originale (SAUF
Kick_1_Simple.wav/Snare_1_Simple.wav, voir le tableau ci-dessus,
compiles depuis le 2026-09-18), et les
bibliotheques `third-party/` non listees ci-dessus : Bounce2, MCP23008,
MCP23017, MCP4728, LCDMenuLib2, MD_REncoder, Regexp, ArduinoJson,
TeensyTimerTool, wav2sketch) n'est PAS inclus dans le binaire
`master_teensy` -- `platformio.ini` ne compile que
`src_teensy/az2_audio/` (`build_src_filter`), et `lib_extra_dirs`
n'expose que les 3 moteurs synthese ci-dessus a la LDF (PlatformIO ne
lie que ce qui est reellement `#include`). Ce code reste dans le depot
comme matiere premiere de reference (voir
`docs/AZ2_PORTAGE_MICRODEXED_TOUCH.md`), chaque sous-dossier garde sa
propre licence d'origine (voir son fichier `LICENSE`/`LICENSE-*.txt`) --
MicroDexed-touch lui-meme est publie en double licence Apache-2.0 /
GPLv3 (voir `src_teensy/microdexed-touch/LICENSE-APACHE2.txt` et
`LICENSE-GPL3.txt`).

Nettoyage fait le 2026-09-17 (audit de code) : le manuel PDF
(`doc/MicroDexed-touch-manual.pdf`, 66 Mo), l'ancien manuel de build
(`doc/manuals_old/Build-Manual.pdf`, 3.2 Mo) et les samples de batterie
non utilises (`MicroDexed-touch/drumsamples.h`, 20 Mo) ont ete supprimes
du depot (confirmes non references par aucun `#include`/`build_src_
filter`) -- ~89 Mo de moins, aucun impact sur la compilation.

`src_esp32/retro-go-master/` (134 Mo) et `src_esp32/Launcher_lvgl-
master/` avaient deja ete supprimes dans une session precedente (aucun
des deux n'a jamais ete lie a un binaire AZ-2) ; `include/lv_conf.h` et
la dependance `lvgl` de `screen_esp` ont ete retires le 2026-09-17 (0
reference a `lv_`/`lvgl.h` dans `az2_screen/` -- le menu/tracker sont
dessines a la main via GFX Library for Arduino, pas LVGL).

## Ce qui reste a faire

- Si un jour AZ-2 distribue des binaires precompiles (pas seulement le
  code source), rappeler dans les notes de version que le firmware
  Teensy integre du code GPLv3 (Synth_MDA_EPiano) et que le firmware
  complet est donc lui-meme sous GPLv3.
