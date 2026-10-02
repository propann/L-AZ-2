# AZ-2 — état actuel vérifié au 2 octobre 2026

Ce document est la source de vérité de l’état courant. Les audits datés dans
`docs/` sont historiques et peuvent décrire des états antérieurs.

## Production active

- Branche de travail : `nes-emulation`.
- Teensy 4.1 : maître audio, séquenceur, moteurs locaux, SD et DAC I²S.
  Firmware : `master_teensy_rack_lab`.
- ESP32-S3 écran : interface tactile, page ÉMULATEURS (3 cartes : GAME BOY,
  GAME BOY COLOR, NES). Firmware : `screen_esp_walnut_gbc_core_task`
  (`default_envs` de `platformio.ini`).
- Navigation : le bouton JEUX du menu principal ouvre directement la page
  ÉMULATEURS.

## Rack audio

- Rack audio externe fonctionnel dans la configuration validée : ESP32-S3
  GRANULAR et ESP-WROOM SPECTRAL reliés au Teensy par le protocole rack et le
  retour audio I²S.
- `master_teensy_rack_lab` est le firmware à utiliser avec ce rack ; il ne
  faut pas le confondre avec un banc abandonné.
- Le Teensy conserve le rôle de maître : horloge, commandes de piste, mixage
  final et sortie PCM5102A.
- Les tests et réglages fins des moteurs externes restent à poursuivre, mais
  le chemin rack audio de base est fonctionnel.

## Game Boy et Game Boy Color — validés à pleine vitesse

- Un seul cœur : **Walnut-CGB**, pour les jeux `.gb` comme `.gbc`. Peanut-GB
  n'est plus utilisé en production (`screen_esp_peanut_gb_*` reste buildable
  en labo).
- Chaque carte ne liste que ses ROM : GAME BOY les `.gb`, GAME BOY COLOR les
  `.gbc` (`gbSetRomKind()`), y compris dans les sous-dossiers de `/games`.
- Mesures sur matériel du 1er octobre 2026 (`GB:PERF`, port série écran) :

| Jeu | Mode | Cadence | Cœur moyen |
| --- | --- | --- | --- |
| Zelda: Link's Awakening DX (`.gbc`) | X2 | 59,6–60,0 fps | ~15 ms |
| Zelda: Link's Awakening DX (`.gbc`) | X3 | 59,6–59,8 fps | ~8 ms |
| Zelda: Link's Awakening (`.gb`) | X3 | 59,0–60,0 fps | ~9 ms |

- Affichage : le cœur 1 émule et transmet les lignes source ; le cœur 0
  agrandit et écrit le framebuffer (pool d'une image entière, le cœur 1
  n'attend plus l'affichage). Une image dessinée sur 2 en X2 (30 Hz), sur 3
  en X3 (20 Hz) ; logique et son restent à 59,7 Hz.
- Écran RGB à 10 MHz (au lieu de 12) : supprime les lignes affichées au
  mauvais endroit. Un léger sautillement subsiste parfois en X3 (débit PSRAM
  partagé entre écran, affichage et ROM).
- Sauvegarde automatique toutes les 30 s : n'écrit sur la SD que si la SRAM
  a réellement changé (empreinte CRC32) — plus d'arrêt de 0,4–0,8 s.
- Audio GB : 14 kHz mono vers le Teensy ; le Teensy le tire directement par
  son ISR audio avec asservissement sur le remplissage réel (voir
  `docs/AZ2_BIP_PARASITE_2026-09-28.md`, mise à jour du 1er octobre).
- Capture : WAV sur la SD du Teensy (`/samples/SAMPLE_###.wav`), dernier
  fichier chargeable dans `SAMPLER / GB Capture`.
- Reste à qualifier : matrice de compatibilité plus large, session longue de
  30 min, aller-retour complet des sauvegardes.

## NES

- Intégré : chargement ROM, contrôleur, audio APU, sauvegardes SRAM.
- Mesure de référence : 49–50 fps avec `FRAMESKIP`, soit 81,5–83,2 % de la
  cadence NTSC. Le son est donc lui aussi en dessous du temps réel.
  Détails : `docs/AZ2_NES_VALIDATION_2026-09-27.md`.

## Neo Geo Pocket — retiré le 2 octobre 2026

Le cœur RACE (GPLv2 seule) était incompatible avec la licence GPLv3 d'AZ-2.
Il a été **supprimé** du dépôt avec son adaptateur, sa cible labo et la carte
de la page ÉMULATEURS. Il ne tournait de toute façon qu'à ~17 fps (28 %).
Les études restent consultables pour l'historique
(`AZ2_NGP_ETUDE_2026-09-27.md`). Une reprise demanderait un cœur sous licence
compatible (piste : NgpCraft, MIT).

## MIDI

- Le composant MIDI (6N138) n'est PAS monté. Tant qu'il ne l'est pas, le
  firmware ne contient plus aucun code MIDI : le code USB MIDI et le parseur
  MIDI DIN ont été supprimés, pas seulement désactivés par un `#ifdef`.
- USB reste un port série de commandes AZ2 uniquement ; `usbMIDI` n'est
  jamais lu.
- MIDI DIN : `Serial8` n'est **jamais ouvert**. L'UART n'échantillonne donc
  même pas la broche — aucun octet ne peut devenir une note, même si la
  broche du 6N138 absent capte du bruit électrique.
- Les deux broches du circuit absent sont figées par pull-down interne :
  pin 34 (RX8) et pin 35 (TX8), dans `setup()`. Une entrée CMOS laissée
  flottante oscille au gré du bruit et injecte du courant de commutation
  dans l'alimentation partagée avec le PCM5102A.
- Réactivation : seulement après montage **et** validation électrique du
  6N138 (voir `docs/AZ2_RACK_PINOUT.md`). Le code MIDI supprimé reste
  consultable dans l'historique git.

## Bip parasite périodique — non reproduit depuis le 1er octobre

- Bip d'environ une fois par seconde, présent dans toute l'application, qui
  survivait à STOP et PANIC et que seule une coupure d'alimentation faisait
  disparaître.
- 1er octobre 2026 : chemin audio GB refait (source tirée par l'ISR, PANIC
  vide désormais l'anneau GB). Validé à l'écoute : plus de bruit parasite,
  séquenceur et moteurs OK. **La cause racine n'est pas prouvée** : le bip
  disparaissait déjà après chaque coupure de courant.
- Indice matériel du même soir : déconnexions USB en cascade et hub tombé —
  l'alimentation reste un suspect.
- À la prochaine occurrence : relever `RACK:STATUS` et `GB:AUDIO_RX` avant
  de redémarrer, puis redémarrer les cartes une par une.
- Détail complet : `docs/AZ2_BIP_PARASITE_2026-09-28.md`.

## Audio et sampleur

- Les moteurs Teensy et la lecture de samples SD compilent.
- Les captures GB créent un nouveau nom libre et ne doivent pas écraser un WAV
  existant.
- La bibliothèque multi-captures et l’explorateur intégré dans PATCH ne sont
  pas finalisés ; ne pas les annoncer comme livrés.

## Pico / LED

Le chantier Pico, Button Pad et matrice LED est archivé. Les documents
`AZ2_CABLAGE_PICO*.md` sont historiques et ne sont pas des références de
câblage de production.

## Vérifications du 2 octobre 2026

- Compilation :

| Environnement | Rôle | Résultat |
| --- | --- | --- |
| `master_teensy_rack_lab` | Teensy production | OK |
| `master_teensy` | Teensy sans rack | OK |
| `screen_esp_walnut_gbc_core_task` | écran production | OK |
| `screen_esp` | écran sans affichage double cœur | OK |
| `screen_esp_peanut_gb_lab` / `_core_task` | labo Peanut-GB | OK |
| `engine_rack_granular_s3_teensy_slave` | rack GRANULAR | OK |
| `engine_rack_spectral_esp32` | rack SPECTRAL | OK |
| `screen_esp_gb_direct` | sonde RGB directe | **ÉCHEC** : exclut `nes_core/` mais compile `nes_emulator.cpp` (`Cpu6502` non défini) |

- `pio test -e native` : 21/21. `tools/check_firmware_contract.py` : PASS.
- Préparation SD ROM : `docs/AZ2_SD_ROM_SETUP.md`.
- Bug d'entrelacement série du port de debug (`GB:PERF` mélangé) : lié au
  lien USB/CH340 sous rafale, pas au firmware ; n'affecte que la lisibilité.
