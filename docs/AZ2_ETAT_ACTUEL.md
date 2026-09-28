# AZ-2 — état actuel vérifié au 27 septembre 2026 (soir)

Ce document est la source de vérité de l’état courant. Les audits datés dans
`docs/` sont historiques et peuvent décrire des états antérieurs.

## Production active

- Branche de travail : `nes-emulation`.
- Teensy 4.1 : maître audio, séquenceur, moteurs locaux, SD et DAC I²S.
- ESP32-S3 écran : interface tactile, page EMULATEURS (4 cartes) et les
  backends Walnut-CGB, NES et NGP RACE intégrés dans la cible courante.
- Firmware audio de référence : `master_teensy_rack_lab`.
- Firmware écran de référence pour la passe actuelle : `screen_esp`.
  Les variantes GB/GBC séparées restent buildables pour les essais de cœur,
  mais `screen_esp` est la cible flashee qui regroupe le menu et le support
  NES.
- Navigation : le bouton JEUX du menu principal ouvre directement la page
  EMULATEURS (Screen::EmuPicker) au lieu de passer par une sous-liste a un
  seul choix -- categorie a un seul item sautee automatiquement
  (`enterMenuCategory()`). 4 cartes : GAME BOY, GAME BOY COLOR, NES et NEO
  GEO POCKET. Le build flashé reste le `screen_esp` standard restauré après
  la sonde d’affichage direct ; aucune sonde expérimentale n’est active.

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

- Cœur de la cible courante : Walnut-CGB (`gb_emulator.cpp`). Le backend
  Peanut-GB (`gb_emulator_peanut.cpp`) reste disponible dans les
  environnements labo mais est explicitement exclu de `screen_esp`.
- La sonde `AZ2_GB_DUAL_CORE_BLIT` déporte la copie+flush du framebuffer sur
  le core 0 avec double buffer et attentes bornées. Elle est validée dans les
  environnements labo core-task, mais n’est pas activée dans le build standard
  actuellement flashé.
- **X3 : validé à 59,76 fps sur matériel réel** dans le backend Peanut-GB de
  labo (référence 59,73 fps, soit **100,1 %**). Cette mesure ne doit pas être
  attribuée automatiquement au backend Walnut de la cible courante.
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

## Game Boy Color

- Cœur : Walnut-CGB (`WALNUT_FULL_GBC_SUPPORT=1`), même découpage double
  cœur que Peanut-GB ci-dessus. Firmware de validation séparé :
  `screen_esp_walnut_gbc_core_task` reste une sonde de validation du blit sur
  l’autre cœur ; **il ne faut pas le flasher pendant cette pause**.
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
  la cible courante (`#ifndef AZ2_GB_CORE_PEANUT`).
- Reste à qualifier : matrice de compatibilité ROM plus large (une seule
  ROM testée), session longue (30 min), aller-retour sauvegarde complet,
  confirmation visuelle utilisateur détaillée (couleurs/scintillement).

## NES

- Support intégré dans `screen_esp` : chargement ROM, contrôleur, audio APU,
  sauvegardes SRAM et rendu 256×240 vers l’écran 480×480.
- Mesure matérielle de référence : 49–50 FPS d’émulation stable, avec
  `FRAMESKIP` actif ; la cible NTSC reste 60,1 FPS, soit **81,5–83,2 %**.
- Le reboot watchdog et le débordement du blit vidéo ont été corrigés.
- Le pipeline d’affichage Core 0 a été essayé puis retiré après une mesure
  régressive à 36,8 FPS. Les détails sont dans
  `docs/AZ2_NES_VALIDATION_2026-09-27.md`.

## Neo Geo Pocket / Color

- Le cœur RACE est maintenant compilé dans `screen_esp` avec son adaptateur
  AZ-2 (`src_esp32/az2_screen/ngp_emulator.cpp`) et le bouton jaune NEO GEO
  POCKET est raccordé à la navigation, aux commandes, au son et aux fichiers
  de sauvegarde.
- Le firmware standard a été restauré après la sonde Core 0 / framebuffer
  direct. L’intégration NGP compilée n’est **pas encore qualifiée en
  production** ; la seule mesure reproductible est la cible labo isolée :
  environ 17 fps, soit **28,3 %** d’une cadence 60 Hz.
- Le pipeline NGP double cœur/direct est désactivé dans `screen_esp` après
  régression d’affichage et de commandes. Il reste du code conditionnel pour
  une reprise séparée, mais il ne fait pas partie du firmware flashé.
- RACE est GPLv2-only : la présence de ses sources dans le build courant est
  un point juridique à traiter avant une diffusion de production. Voir
  l’audit et `docs/AZ2_NGP_ETUDE_2026-09-27.md`.

## Cœurs archivés / étudiés

- GNUBOY : probe séparée, non retenue pour la production.
- RACE conserve aussi une cible labo isolée `screen_esp_ngp_race_lab`, utile
  pour reproduire la mesure des 17 fps sans toucher aux autres émulateurs.

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

## Bip parasite périodique — NON RÉSOLU

- Bip d'environ une fois par seconde, présent dans **toute** l'application
  (pas seulement les émulateurs), qui survit à STOP et à PANIC et que **seule
  une coupure d'alimentation** fait disparaître.
- Absent sur la version figée du 28 septembre 2026, après redémarrage. **La
  cause n'est pas identifiée.**
- C'est un état **verrouillé** : `panicAllAudio()` ne coupe que les voix,
  aucun chemin logiciel ne remet le bus audio à zéro (anneau GB,
  rééchantillonneur, reverb, delay, retour rack).
- Trois tentatives sont épuisées et ne doivent pas être recommencées
  (métronome, MIDI, retour rack). Le MIDI est **définitivement écarté**.
- À la prochaine occurrence : relever `RACK:STATUS` et `GB:AUDIO_RX`
  **avant** de redémarrer — le redémarrage détruit la preuve.
- Détail complet, mesures et étapes suivantes :
  `docs/AZ2_BIP_PARASITE_2026-09-28.md`.

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
- `pio run -e screen_esp` : OK après restauration du build standard ; le
  firmware a redémarré avec `DISPLAY:ALIVE:TICK`, audio prêt et événements de
  boutons confirmés sur le port série.
- `pio run -e master_teensy_rack_lab` : OK.
- `pio test -e native` : 21/21 tests réussis lors de la dernière campagne.
- Bug d'entrelacement série trouvé (des lignes `GB:PERF`/`TOUCH:...` se
  mélangent sous forte charge) : confirmé indépendant du code firmware
  (persiste même sur un simple `Serial.println()` isolé et sur un
  `Serial.write()` unique) -- probablement le lien USB/CH340 sous rafale,
  pas un bug AZ-2. N'affecte que la lisibilité du débogage série, pas le
  jeu réel. Non corrigé, pas de piste firmware restante à essayer.
