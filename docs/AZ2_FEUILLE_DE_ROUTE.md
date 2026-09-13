# AZ-2 - Feuille de route

Objectif: transformer AZ-2 en groovebox autonome Teensy + ESP32, avec une base propre avant d'empiler les fonctions.

## Phase 0 - Fondations

Statut: en cours.

| Tache | Statut | Fichier |
| --- | --- | --- |
| Definir roles ESP32 / Teensy | Fait | `README.md` |
| Fixer DAC PCM5102A | Fait | `docs/AZ2_DAC_PCM5102A.md` |
| Definir protocole ESP32/Teensy | Base faite | `lib/AZ2_Protocol/AZ2_Protocol.h` |
| Creer firmware ESP32 propre | Base faite | `src_esp32/az2_control/main.cpp` |
| Creer firmware Teensy propre | Base faite | `src_teensy/az2_audio/main.cpp` |
| Sortir le Pico de la ligne principale | Fait | `platformio.ini` |
| Documenter Wi-Fi / SD / Retro-Go | Base faite | `docs/AZ2_ESP32_CONTROLE_WIFI_SD_RETRO.md` |
| Documenter cablage complet v0 | Base faite | `docs/AZ2_CABLAGE_BASE.md` |

## Phase 1 - Cablage minimal qui fait du son

Objectif: obtenir un son test stable depuis le Teensy vers le PCM5102A.

| Priorite | Tache |
| ---: | --- |
| 1 | Cabler PCM5102A: 3.3 V, GND, BCLK 21, LRCLK 20, DIN 7 |
| 2 | Flasher `master_teensy` |
| 3 | Envoyer `PLAY` / `STOP` en serie |
| 4 | Verifier sortie ligne avec volume bas |
| 5 | Ajouter mute `XSMT` si le module expose la broche |

Critere de sortie: le Teensy produit un son propre sur le PCM5102A sans ESP32.

## Phase 2 - Dialogue ESP32 vers Teensy

Objectif: faire parler les deux cerveaux.

| Priorite | Tache |
| ---: | --- |
| 1 | Choisir deux GPIO UART libres sur ESP32 |
| 2 | Choisir RX/TX sur Teensy |
| 3 | Relier TX/RX croises + GND commun |
| 4 | Envoyer `HELLO:ESP32_CONTROL` |
| 5 | Recevoir `HELLO:TEENSY_AUDIO` |
| 6 | Envoyer `PAD:00:DOWN:vel=110` et declencher un son |

Critere de sortie: un message ESP32 declenche un son Teensy.

## Phase 3 - Matrice SparkFun 4x4

Objectif: transformer les 16 pads en surface de jeu.

| Priorite | Tache |
| ---: | --- |
| 1 | Identifier la reference exacte des multiplexeurs |
| 2 | Tester la matrice en rouge monochrome |
| 3 | Scanner les 16 boutons avec anti-rebond |
| 4 | Envoyer les `PAD` au Teensy |
| 5 | Piloter les LEDs de retour |
| 6 | Passer au RGB si le driver de courant est propre |

Critere de sortie: chaque pad allume sa LED et declenche un son.

## Phase 4 - Ecran ESP32 480x480

Objectif: obtenir une UI AZ-2 lisible sur l'ecran carre.

| Priorite | Tache |
| ---: | --- |
| 1 | Valider driver ST7701 sur ESP32-4848S040C_I |
| 2 | Afficher page boot AZ-2 |
| 3 | Afficher grille 4x4 |
| 4 | Afficher etat Teensy: ready, playing, BPM |
| 5 | Creer pages Home, Performance, Mixer, Sequencer |
| 6 | Ajouter mode test hardware |

Critere de sortie: l'ecran montre l'etat pads/Teensy en temps reel.

## Phase 5 - SD et Wi-Fi

Objectif: utiliser l'ESP32 comme tete moderne sans casser la stabilite musicale.

| Priorite | Tache |
| ---: | --- |
| 1 | Trouver le pinout TF/microSD exact du module ecran |
| 2 | Monter `/az2` au boot si SD presente |
| 3 | Stocker `hardware.json`, `ui.json`, `wifi.json` |
| 4 | Ajouter mode Wi-Fi AP local |
| 5 | Ajouter file manager web minimal |
| 6 | Ajouter export logs / captures |

Critere de sortie: on peut configurer et transferer des fichiers sans ouvrir la machine.

## Phase 6 - Portage MicroDexed-touch

Objectif: recuperer seulement ce qui sert au moteur son.

| Priorite | Tache |
| ---: | --- |
| 1 | Compiler une base MicroDexed-touch en `I2S_AUDIO_ONLY` |
| 2 | Isoler audio graph, synth, mixer, sequencer |
| 3 | Retirer UI ILI9341/touch du chemin principal |
| 4 | Brancher commandes AZ-2 sur moteur MicroDexed |
| 5 | Ajouter presets/banques |
| 6 | Ajouter samples quand la base est stable |

Critere de sortie: AZ-2 joue un moteur MicroDexed pilote par l'ESP32.

## Phase 7 - Retro-Go / Game Boy bonus

Objectif: ajouter le fun sans contaminer la groovebox.

| Priorite | Tache |
| ---: | --- |
| 1 | Garder Retro-Go comme reference dans `src_esp32/retro-go-master` |
| 2 | Etudier target ESP32-S3 proche |
| 3 | Creer target `az2-esp32-4848s040` si utile |
| 4 | Adapter driver ecran ST7701 |
| 5 | Mapper matrice 4x4 vers commandes Game Boy |
| 6 | Utiliser SD: `/roms/gb`, `/roms/gbc`, `/retro-go` |

Critere de sortie: mode Game Boy separable, jamais prioritaire sur l'instrument.

## Regle de projet

- Un lot = un test reel.
- Pas de gros import aveugle dans la compilation principale.
- Tout ce qui touche au son doit rester testable sans UI.
- Tout ce qui touche a l'UI doit rester testable sans moteur audio complet.
- Les docs suivent le cablage reel au fur et a mesure.
