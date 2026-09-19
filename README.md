# AZ-2

AZ-2 est une groovebox hardware modulaire a 2 cerveaux, orientee
Game Boy/GBC : un **Teensy 4.1** pour tout le moteur audio temps reel
(6 moteurs audio, filtre + ADSR par piste, sequenceur/tracker) et un
**ESP32-S3** avec un ecran tactile 480x480 pour l'interface, la
sauvegarde SD et l'emulation GB/GBC.

## Ce que la machine fait aujourd'hui

- **8 pistes**, chacune avec son propre moteur au choix (Dexed FM,
  mda ePiano, Braids, Karplus-Strong, oscillateur analogique, sampleur), filtre
  resonant, ADSR et volume editables en direct, avec un oscilloscope
  pour voir l'onde changer en reglant les parametres. Reglages propres
  au Dexed (algorithme/feedback DX7). **Mute/solo par piste.**
- **Tracker** a la LSDJ/M8 : colonnes NOTE/INST/FX/VAL par pas, 8
  patterns, chainage en mode "song", effets par pas (arpege, coupe,
  retrig), gammes (verrouillage a la saisie de note). Le clavier
  tactile peut aussi servir a poser une note directement sur un pas.
- **Sauvegarde** de patch (moteur+patch+filtre+ADSR, plusieurs
  emplacements) ET de projet complet (patterns+song+tempo+gamme+sons,
  4 emplacements), sur la carte SD de l'ESP32.
- **Emulateur Game Boy/GBC** (Walnut-CGB), son du jeu route jusqu'au
  DAC du Teensy, sauvegarde de la RAM de cartouche sur la SD, liste de
  ROM paginee. Capture REC/STOP vers `.wav` sur la SD du Teensy ; moteur
  sampleur one-shot Kick/Snare actif, les WAV captures ne sont pas encore
  injectables dans sa banque.
- **MIDI notes IN** (USB) vers la voix live.
- **Croix + 4 boutons + 3 encodeurs rotatifs** (avec bouton integre)
  cables directement sur le Teensy -- navigation dans les menus,
  manette pour le mode Jeux, reglages volume/reverb/delay.
- Menu ecran en 4 categories (Musique / Jeux / Configuration / Doc),
  ecran de veille configurable, diagnostic materiel visible a l'ecran.

Voir [docs/AZ2_ETAT_DES_LIEUX.md](docs/AZ2_ETAT_DES_LIEUX.md) pour le
detail de ce qui est confirme sur le vrai materiel, par opposition a ce
qui n'est encore que compile.

## Architecture

Deux cartes, deux roles nets :

- **Teensy 4.1** (`src_teensy/az2_audio/`) : synthese, sequenceur/
  tracker, DAC I2S (PCM5102A), croix/boutons/encodeurs physiques.
- **ESP32-S3** (`src_esp32/az2_screen/`) : ecran tactile 480x480 RGB
  parallele (module VIEWE UEDX48480040E-WB), interface, emulateur GB/
  GBC, carte SD (ROMs + patches).
- Liaison UART 921600 bauds entre les deux, protocole texte ligne par
  ligne + paquets binaires (audio GB, oscilloscope) -- partage via
  `lib/AZ2_Protocol/`.
- **Périmètre figé le 2026-09-18 :** le boîtier AZ-2 est plein. Aucun rack multi-ESP ni recâblage de commandes ; les cartouches AZ-BUS/AZ-CHIP/AZ-VA sont reportées à l’AZ-3.

Detail complet : [docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md](docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md).

## Documentation

- [Audit de code du 19 septembre 2026](docs/AZ2_AUDIT_CODE_2026-09-19.md) : verdict, risques prioritaires et plan de stabilisation.
- [Audit émulation LSDJ, Tetris et Mario](docs/AZ2_AUDIT_EMULATION_LSDJ_TETRIS_MARIO.md) : performances, fidélité audio/vidéo, sauvegardes et plan de qualification.
- [Étude des cœurs d'émulation GB/GBC](docs/AZ2_ETUDE_COEURS_EMULATION_GB.md) : comparaison Walnut, Peanut, SameBoy, Gearboy, Retro-Go et choix APU.
- [Etat des lieux](docs/AZ2_ETAT_DES_LIEUX.md) : ce qui est verifie en
  reel vs seulement compile, mis a jour au fil de l'eau.
- [Feuille de route](docs/AZ2_FEUILLE_DE_ROUTE.md) : ordre de
  construction, ce qui reste a faire.
- [Benchmark concurrence](docs/AZ2_BENCHMARK_CONCURRENCE.md) : analyse
  des grooveboxes/trackers du marche, ameliorations indispensables.
- [Etude tracker](docs/AZ2_TRACKER_ETUDE.md) : comparatif LSDJ/M8/
  Polyend et choix retenus pour notre tracker.
- [Architecture firmware double cerveau](docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md)
- [Cablage complet (croix/boutons/encodeurs/DAC)](docs/AZ2_CABLAGE_MASTER.md)
- [DAC PCM5102A](docs/AZ2_DAC_PCM5102A.md)
- [Ecran, facade et navigation](docs/AZ2_ECRAN_FACADE.md)
- [Emulation Game Boy/GBC](docs/AZ2_EMULATION_JEUX.md)
- [ESP32 : SD, Wi-Fi, retro](docs/AZ2_ESP32_CONTROLE_WIFI_SD_RETRO.md)
- [Portage MicroDexed-touch](docs/AZ2_PORTAGE_MICRODEXED_TOUCH.md)
- [Feuille de route du moteur audio](docs/AZ2_FEUILLE_DE_ROUTE_MOTEUR.md)
- [Architecture multi-moteurs et sampler](docs/AZ2_ARCHITECTURE_MULTI_MOTEURS.md)
- [Étude AZ-3 — rack de moteurs interchangeables](docs/AZ2_BUS_RACK_MOTEURS.md)
- [Étude AZ-3 — flash des modules moteurs](docs/AZ2_FLASH_MODULES.md)
- [Étude AZ-3 — module ESP32 AZ-VA1](docs/AZ2_MODULE_ESP32_AZ_VA1.md)
- [Étude AZ-3 — cartouche ESP8266 AZ-CHIP](docs/AZ2_MODULE_ESP8266_AZ_CHIP.md)
- [Cablage de base (historique)](docs/AZ2_CABLAGE_BASE.md)
- [Pico -- tentative abandonnee](docs/AZ2_CABLAGE_PICO.md) /
  [TODO Pico (clos)](docs/AZ2_TODO_PICO.md)

## Structure

```
L-AZ-2/
├── platformio.ini
├── lib/
│   └── AZ2_Protocol/       # protocole partage ESP32 <-> Teensy
├── docs/
├── src_teensy/
│   ├── az2_audio/          # firmware Teensy actif (env master_teensy)
│   └── microdexed-touch/   # source vendored, reference pour le portage
└── src_esp32/
    ├── az2_screen/         # firmware ESP32 actif (env screen_esp)
    └── az2_control/        # bring-up historique, garde en reference (env ui_esp)
```

## Environnements PlatformIO

- `master_teensy` (par defaut) : Teensy 4.1, moteur audio complet --
  sources `src_teensy/az2_audio/`.
- `screen_esp` (par defaut) : ESP32-S3 + ecran reel VIEWE
  UEDX48480040E-WB, interface complete -- sources `src_esp32/az2_screen/`.
- `ui_esp` : ESP32-S3 devkit nu, bring-up historique garde comme
  reference/tests isoles (pas dans `default_envs`) -- sources
  `src_esp32/az2_control/`.

```
pio run -e master_teensy -e screen_esp
pio run -e master_teensy -t upload
pio run -e screen_esp -t upload
```

Headers partages dans `lib/` (`AZ2_Protocol.h`).

## Licence

Code AZ-2 sous **GNU GPLv3** (voir `LICENSE`). Voir
[`docs/AZ2_LICENCES.md`](docs/AZ2_LICENCES.md) pour la licence et la
provenance de chaque composant tiers vendored (moteurs synthese,
pilote ecran, emulateur Game Boy).
