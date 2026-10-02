# AZ-2 — prototype Neo Geo Pocket / Color RACE (2026-09-27)

> **Historique — RACE retiré du dépôt le 2 octobre 2026.** Le cœur RACE est sous GPLv2 seule, incompatible avec la GPLv3 d'AZ-2 ; le code, l'adaptateur et la cible labo décrits ici ont été supprimés (consultables dans l'historique Git). Voir `AZ2_ETAT_ACTUEL.md` et `AZ2_LICENCES.md`.

## Cible et licence

La cible labo reste isolée dans l’environnement PlatformIO
`screen_esp_ngp_race_lab`. Elle utilise le cœur RACE provenant du port
ESP32-S3/Nano-S3, sous GPLv2-only, avec sa notice conservée dans
`src_esp32/az2_ngp_race/race/license.txt`.

Le même cœur est désormais présent dans le build `screen_esp` pour aligner
le bouton, les contrôles, le rendu, l’audio et les sauvegardes. Cette présence
technique n’est pas encore une validation de production : le statut GPLv2 et
la campagne matérielle d’intégration restent à traiter.

## Raccordement des commandes

Le prototype lit les mêmes messages `NAV:/BTN:` que les autres firmwares AZ-2
sur la liaison Teensy :

| Commande AZ-2 | Entrée RACE |
| --- | --- |
| NAV:UP/DOWN/LEFT/RIGHT | croix directionnelle |
| BTN:A | bouton NGP A |
| BTN:B | bouton NGP B |
| BTN:C | Select |
| BTN:D | Start |

Les ROMs `.ngp` et `.ngc` sont cherchées récursivement dans `/games` ; la
première trouvée est chargée.

## Vidéo, audio et sauvegardes

- framebuffer logique RACE : 160×152 RGB565 ;
- affichage AZ-2 : agrandissement nearest-neighbor 3×, 480×456 centré ;
- audio RACE : génération PSG/DAC puis paquets PCM8 sur la chaîne Teensy
  existante ;
- sauvegarde flash : fichier `.ngf` à côté de la ROM, remplacé au lieu d’être
  ajouté à chaque écriture.

## Flash et premier test matériel

```text
pio run -e screen_esp_ngp_race_lab -t upload --upload-port /dev/ttyUSB0
```

Résultat du 27 septembre 2026 : boot et flash vérifiés sur l’ESP32-S3 N16R8,
ROM chargée, cœur RACE démarré, PSRAM reconnue avec environ 6682 KiB libres.
La première mesure est d’environ 17 fps avec `frame_us` autour de 17,5 ms.

Cette cadence est un point de départ, pas une validation pleine vitesse. Le
prochain travail porte sur le budget CPU RACE, le rendu écran et la validation
audio/contrôles/sauvegarde sur plusieurs ROMs, sans modifier GB/GBC/NES.
