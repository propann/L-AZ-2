# Documentation AZ-2

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
