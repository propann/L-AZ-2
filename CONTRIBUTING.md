# Contribuer à AZ-2

Merci de contribuer à AZ-2. Le projet combine matériel, firmware temps réel, interface ESP32-S3 et émulation GB/GBC ; une modification apparemment locale peut donc avoir des effets sur l'autre firmware.

## Avant toute modification

1. Lire le [README](README.md), le [guide de démarrage](docs/AZ2_DEMARRAGE.md) et l'[architecture double firmware](docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md).
2. Compiler les deux cibles depuis le même SHA :
   `pio run -e master_teensy -e screen_esp`
3. Lancer :
   `python tools/check_firmware_contract.py`
   puis `pio test -e native`.
4. Ne jamais ajouter de ROM commerciale, de sauvegarde privée ou de contenu sous licence incompatible.

## Règle du protocole

`lib/AZ2_Protocol/AZ2_Protocol.h` est le contrat commun Teensy ↔ ESP32-S3.

Toute modification d'un message, d'un paquet binaire, du baudrate, d'un sample rate ou d'une taille doit inclure :
- l'émetteur,
- le récepteur,
- les tests,
- la compilation des deux firmwares,
- un chemin de retour à la version précédente.

## Rapporter un bug

Donner si possible :
- SHA Git,
- modèle matériel,
- environnement concerné,
- logs série,
- étapes exactes,
- comportement attendu / observé,
- photo ou vidéo si le bug est visuel.

Pour un problème GB/LSDJ, utiliser aussi [AZ2_GB_VALIDATION.md](docs/AZ2_GB_VALIDATION.md).

## Style de changements

Préférer :
- petits commits cohérents,
- messages d'erreur explicites,
- aucune perte silencieuse de données,
- commentaires expliquant les contraintes matérielles,
- documentation mise à jour en même temps que le code.

Éviter :
- remplacer une dépendance ou un cœur d'émulation sans mesure comparative,
- annoncer une fonction comme validée uniquement parce qu'elle compile,
- modifier un seul firmware lorsqu'un contrat partagé change.

## Tests matériels

Un résultat matériel doit préciser le SHA exact et la durée du test. Pour l'émulation, distinguer :
- démarre,
- jouable,
- stable,
- conforme sur longue durée.

AZ-2 reste un prototype en développement : les retours reproductibles sont plus utiles que les verdicts généraux.
