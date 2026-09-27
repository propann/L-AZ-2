# AZ-2 — état actuel vérifié au 27 septembre 2026 (soir)

Ce document est la source de vérité de l’état courant. Les audits datés dans
`docs/` sont historiques et peuvent décrire des états antérieurs.

## Production active

- Branche : `research/peanut-gb-prototype`.
- Teensy 4.1 : maître audio, séquenceur, moteurs locaux, SD et DAC I²S.
- ESP32-S3 écran : interface tactile, page EMULATEURS (3 cartes) et Peanut-GB.
- Firmware audio de référence : `master_teensy_rack_lab`.
- Firmware écran de référence : `screen_esp_peanut_gb_core_task` (remplace
  `screen_esp_peanut_gb_lab` -- meme moteur Peanut-GB, plus le blit sur
  l’autre coeur, voir "Game Boy" ci-dessous). `screen_esp_peanut_gb_lab`
  reste buildable (filet de secours) mais n’est plus la cible flashee.
- Navigation : le bouton JEUX du menu principal ouvre directement la page
  EMULATEURS (Screen::EmuPicker) au lieu de passer par une sous-liste a un
  seul choix -- categorie a un seul item sautee automatiquement
  (`enterMenuCategory()`). 3 cartes : GAME BOY (Peanut-GB, seule cible
  reellement jouable sur CE firmware), GAME BOY COLOR (Walnut-CGB, labo
  separe, voir plus bas), NES (pas commence, etude faite).

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

## Game Boy

- Cœur actif : Peanut-GB, DMG-only.
- **Blit sur l’autre cœur (2026-09-27, `AZ2_GB_DUAL_CORE_BLIT`)** : la copie+
  flush du framebuffer (~16,4ms) tourne maintenant sur une tâche FreeRTOS
  dédiée au core 0 (libre, ce firmware n’utilise ni WiFi ni BT), en
  parallèle du cœur CPU+PPU sur le core 1. Double buffer + 2 files FreeRTOS,
  toutes les attentes sont **bornées** (`kGbBandWaitTicks`, 50ms, pas
  `portMAX_DELAY`) pour qu’un déséquilibre ne puisse plus jamais geler
  `loop()` (tactile/boutons restent réactifs même en cas de souci
  d’affichage).
- **X3 : validé à 59,76 fps sur matériel réel** (contre ~40-50 avant) —
  n’est plus "plus lent", X2 et X3 tournent tous les deux plein débit.
  X2 reste dispo si besoin d’une marge supplémentaire.
- Boutons AFFICHAGE (X2/X3) agrandis (150×34, texte taille 2) sur la page
  JEUX.
- Audio GB : transmis vers le Teensy en mono pour la chaîne actuelle.
- Capture : fonctionnelle en X2, jusqu’à 30 secondes, WAV écrit sur la SD du
  Teensy sous `/samples/SAMPLE_###.wav`.
- Le dernier fichier capturé peut être chargé dans le slot dynamique
  `SAMPLER / GB Capture`.
- Matrice de compatibilité ROM DMG, fidélité APU et sauvegardes prolongées
  restent à qualifier plus largement (une seule ROM en session longue à ce
  jour).

## Game Boy Color (labo, pas en production)

- Cœur : Walnut-CGB (`WALNUT_FULL_GBC_SUPPORT=1`), même découpage double
  cœur que Peanut-GB ci-dessus. Firmware séparé :
  `screen_esp_walnut_gbc_core_task` -- **ne peut pas être compilé dans le
  même binaire que Peanut-GB** (collision de symboles, `gb_emulator.cpp` vs
  `gb_emulator_peanut.cpp`) ; le flasher remplace temporairement le firmware
  GAME BOY validé sur la carte, à reflasher explicitement après usage.
- Bug de blocage trouvé et corrigé (2026-09-27) : `gb_run_frame_dualfetch()`
  saute `gbBlitLine()` entièrement quand `gb->lcd_blank` est vrai
  (walnut_cgb.h) ; si ça arrive en plein milieu d’une bande de 8 lignes, le
  buffer correspondant restait "sorti" du pool indéfiniment -- après 2
  bandes abandonnées, `loop()` se figeait totalement (tactile/boutons
  compris), nécessitant une récupération en mode BOOT physique. Corrigé par
  `gbBlitEndOfFrame()` (récupère un buffer abandonné à la fin de chaque
  frame, appelé depuis `gbRunFrame()`) + attentes bornées partout dans le
  pipeline de blit (voir ci-dessus) au lieu de `portMAX_DELAY`.
- Validé sur matériel réel après correctif : *Zelda: Link’s Awakening DX*
  charge et tourne à **59,64-59,82 fps** en jeu normal, `core_avg_us`
  ~12,4ms. Ralentissement à ~47-51fps observé pendant l’intro du jeu,
  identifié comme le mode double-vitesse du GBC (comportement authentique
  de la cartouche, pas un bug AZ-2) : `core_avg_us` grimpe à ~16,4-17,2ms
  pendant l’intro puis redescend en jeu normal.
- Page EMULATEURS : la carte GAME BOY COLOR lance réellement une partie sur
  CE firmware (`#ifndef AZ2_GB_CORE_PEANUT`) ; sur le firmware stable
  Peanut-GB, elle reste un message informatif (pas de moteur GBC compilé).
- Reste à qualifier : matrice de compatibilité ROM plus large (une seule
  ROM testée), session longue (30 min), aller-retour sauvegarde complet,
  confirmation visuelle utilisateur détaillée (couleurs/scintillement).

## Cœurs archivés / étudiés

- GNUBOY : probe séparée, non retenue pour la production.
- **NES (étude faite, pas commencé)** : candidat identifié —
  [Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32), licence GPLv3
  (compatible, contrairement à Nofrendo/`esp32-nesemu` qui est GPLv2-only).
  Cœur bien séparé du matériel (`src/core/` : cpu6502, ppu2C02, apu2A03,
  bus, cartridge, mapper000-004+069, ~4450 lignes) ; `ppu2C02` expose
  `connectFramebuffer(uint8_t*)`, découplé de tout pilote d’écran
  (`TFT_eSPI` n’apparaît que dans le wrapper optionnel `nes.cpp`, pas dans
  le cœur). 256×240, 60fps natif mesuré, pas de PSRAM requise, 6 mappers
  (~79% de la ludothèque), son + save states (format SD/CRC32 déjà
  compatible avec notre `SD.h`). Prochaine grosse session de travail.

## MIDI

- USB MIDI : conservé.
- MIDI DIN : désactivé tant que les composants MIDI ne sont pas soudés et
  validés électriquement.
- RX MIDI est maintenu dans un état fixe pour éviter les déclenchements
  parasites.

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

## Vérifications

- Préparation SD ROM documentée dans `docs/AZ2_SD_ROM_SETUP.md`. Le format
  produit attendu est MBR + une partition FAT32 unique avec `/games/gb`,
  `/games/gbc`, `/games/ngpc` et `/nes`. La validation complète de copie reste
  bloquée par des erreurs du lecteur Broadcom interne du MacBook Pro pendant
  les transferts (`ADMA`, timeout et remount FAT en lecture seule).

- `pio run -e screen_esp_peanut_gb_lab` : OK.
- `pio run -e screen_esp_peanut_gb_core_task` : OK, flashé et validé sur
  matériel réel (59,76fps X3, boutons/tactile confirmés).
- `pio run -e screen_esp_walnut_gbc_core_task` : OK, flashé et validé sur
  matériel réel (voir "Game Boy Color" ci-dessus).
- `pio run -e master_teensy_rack_lab` : OK.
- `pio test -e native` : 21/21 tests réussis lors de la dernière campagne.
- Bug d'entrelacement série trouvé (des lignes `GB:PERF`/`TOUCH:...` se
  mélangent sous forte charge) : confirmé indépendant du code firmware
  (persiste même sur un simple `Serial.println()` isolé et sur un
  `Serial.write()` unique) -- probablement le lien USB/CH340 sous rafale,
  pas un bug AZ-2. N'affecte que la lisibilité du débogage série, pas le
  jeu réel. Non corrigé, pas de piste firmware restante à essayer.
