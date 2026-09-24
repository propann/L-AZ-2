# AZ-2 — Documentation

La documentation distingue trois niveaux pour éviter toute ambiguïté :

- **CURRENT** : présent dans le code ou le prototype actuel ; la page précise ce qui a été testé sur matériel.
- **ROADMAP** : architecture ou fonction prévue, non présentée comme livrée.
- **HISTORY** : choix antérieur conservé pour comprendre l'évolution.

## Utiliser la machine

- [Manuel utilisateur](../AZ2_MANUEL_UTILISATEUR.md)
- [Premier démarrage](../AZ2_DEMARRAGE.md)
- [Galerie et référence des écrans](SCREENS.md)

## Comprendre le système actuel

- [Architecture firmware actuelle](../AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md)
- [Câblage maître](../AZ2_CABLAGE_MASTER.md)
- [État des lieux](../AZ2_ETAT_DES_LIEUX.md)
- [Protocole](../../lib/AZ2_Protocol/AZ2_Protocol.h)

## Audio et interface

- [UI moteurs et mixer vivants](../AZ2_UI_MOTEURS_MIXER_VIVANTS.md)
- [Sampleur](../AZ2_SAMPLEUR.md)
- [Feuille de route moteur](../AZ2_FEUILLE_DE_ROUTE_MOTEUR.md)

## Game Boy / GBC

- [Roadmap Game Boy](../AZ2_GB_ROADMAP_IMPLEMENTATION.md)
- [Validation](../AZ2_GB_VALIDATION.md)
- [Audit émulation](../AZ2_AUDIT_EMULATION_LSDJ_TETRIS_MARIO.md)
- [Étude des cœurs](../AZ2_ETUDE_COEURS_EMULATION_GB.md)

## Architecture future

- [Rack ESP : prototype 2 modules, extension possible à 4](../rack/AZ2_RACK_MOTEURS_ESP.md) — **ROADMAP**
- [Community Hardware Lab](../community/HARDWARE_LAB.md) — **EXPERIMENTAL**
- Flash des cartouches depuis l'écran — **ROADMAP**, décrit dans la page rack
- Extension MIDI — **ROADMAP**

## Qualité

- [Audit code — 24 septembre 2026](../AZ2_AUDIT_CODE_2026-09-24.md)
- [CI](../../.github/workflows/ci.yml)
- [Contribution](../../CONTRIBUTING.md)
- [Licences](../AZ2_LICENCES.md)

Les documents historiques restent utiles, mais toute décision matérielle ou firmware doit être vérifiée contre l'état actuel du code et du prototype.
