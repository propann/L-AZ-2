# Documentation AZ-2

- [Feuille de route du rack audio physique](AZ2_ROADMAP_RACK_AUDIO_PHYSIQUE.md)
- [Câblage du rack de moteurs externes](AZ2_CABLAGE_RACK_MOTEURS_EXTERNES.md)
- [Pinout canonique Teensy / ESP32-S3 / DAC](AZ2_RACK_PINOUT.md)
- [Compte rendu rack audio du 21 septembre 2026](AZ2_SESSION_RACK_AUDIO_2026-09-21.md)
- [Architecture figée MIDI et moteurs externes](AZ2_ARCHITECTURE_MIDI_ET_MOTEURS_EXTERNES.md)

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
- [Validation NES et bilan d’optimisation du 27 septembre 2026](AZ2_NES_VALIDATION_2026-09-27.md)
- [Audit des émulateurs, vitesses et code dormant du 27 septembre 2026](AZ2_AUDIT_EMULATEURS_2026-09-27.md)
- [Étude Neo Geo Pocket / Color du 27 septembre 2026](AZ2_NGP_ETUDE_2026-09-27.md)
- [Prototype NGPC RACE labo et test des contrôles](AZ2_NGP_RACE_LAB_2026-09-27.md)
- [Bip parasite périodique — journal de diagnostic du 28 septembre 2026](AZ2_BIP_PARASITE_2026-09-28.md)
- [Audit des firmwares et feuille de route du 28 septembre 2026](AZ2_AUDIT_FIRMWARE_ET_FEUILLE_DE_ROUTE_2026-09-28.md)

La référence matérielle actuelle est le couple Teensy 4.1 + écran VIEWE
UEDX48480040E-WB-V1.3. Les commandes sont raccordées directement au Teensy.
Les firmwares de production sont `master_teensy` et `screen_esp`.

## Études et historique

Les fichiers dont le nom contient `AUDIT`, `ETAT`, `ETUDE`, `ROADMAP`,
`PICO`, `BASE` ou une date décrivent une étape du développement. Ils peuvent
mentionner l'ancien Pico, l'environnement `ui_esp`, le débit UART 230400 ou
des composants retirés. Ils sont conservés pour expliquer les décisions et
ne doivent pas servir de nomenclature de fabrication.
