# Documentation AZ-2

- [Feuille de route du rack audio physique](AZ2_ROADMAP_RACK_AUDIO_PHYSIQUE.md)
- [Câblage du rack de moteurs externes](AZ2_CABLAGE_RACK_MOTEURS_EXTERNES.md)
- [Pinout canonique Teensy / ESP32-S3 / DAC](AZ2_RACK_PINOUT.md)
- [Compte rendu rack audio du 21 septembre 2026](AZ2_SESSION_RACK_AUDIO_2026-09-21.md)

## Pour reconstruire et utiliser la machine actuelle

- [Pièces, références, câblage, cartes SD et compilation](AZ2_REPRODUCTION.md)
- [Câblage maître vérifié](AZ2_CABLAGE_MASTER.md)
- [Premier démarrage](AZ2_DEMARRAGE.md)
- [Flash des deux firmwares](AZ2_FLASH_MODULES.md)
- [Manuel utilisateur](AZ2_MANUEL_UTILISATEUR.md)
- [Architecture des deux firmwares](AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md)
- [DAC PCM5102A](AZ2_DAC_PCM5102A.md)
- [Sampleur](AZ2_SAMPLEUR.md)
- [Licences et provenance](AZ2_LICENCES.md)

La référence matérielle actuelle est le couple Teensy 4.1 + écran VIEWE
UEDX48480040E-WB-V1.3. Les commandes sont raccordées directement au Teensy.
Les firmwares de production sont `master_teensy` et `screen_esp`.

## Études et historique

Les fichiers dont le nom contient `AUDIT`, `ETAT`, `ETUDE`, `ROADMAP`,
`PICO`, `BASE` ou une date décrivent une étape du développement. Ils peuvent
mentionner l'ancien Pico, l'environnement `ui_esp`, le débit UART 230400 ou
des composants retirés. Ils sont conservés pour expliquer les décisions et
ne doivent pas servir de nomenclature de fabrication.
