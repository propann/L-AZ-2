# AZ-2 — état actuel vérifié au 27 septembre 2026

Ce document est la source de vérité de l’état courant. Les audits datés dans
`docs/` sont historiques et peuvent décrire des états antérieurs.

## Production active

- Branche : `research/peanut-gb-prototype`.
- Teensy 4.1 : maître audio, séquenceur, moteurs locaux, SD et DAC I²S.
- ESP32-S3 écran : interface tactile et Peanut-GB.
- Firmware audio de référence : `master_teensy_rack_lab`.
- Firmware écran de référence : `screen_esp_peanut_gb_lab`.

## Game Boy

- Cœur actif : Peanut-GB, DMG-only.
- X2 : fonctionnel et validé sur le matériel réel.
- X3 : utilisable, mais plus lent ; X2 reste le mode recommandé.
- Audio GB : transmis vers le Teensy en mono pour la chaîne actuelle.
- Capture : fonctionnelle en X2, jusqu’à 30 secondes, WAV écrit sur la SD du
  Teensy sous `/samples/SAMPLE_###.wav`.
- Le dernier fichier capturé peut être chargé dans le slot dynamique
  `SAMPLER / GB Capture`.
- GBC complet, matrice de compatibilité ROM, fidélité APU et sauvegardes
  prolongées restent à qualifier.

## Cœurs archivés

- Walnut-CGB : archivé, non retenu.
- GNUBOY : probe séparée, non retenue pour la production.

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

- `pio run -e screen_esp_peanut_gb_lab` : OK.
- `pio run -e master_teensy_rack_lab` : OK.
- `pio test -e native` : 21/21 tests réussis lors de la dernière campagne.
- Aucun flash matériel n’est requis par cette mise à jour documentaire.
