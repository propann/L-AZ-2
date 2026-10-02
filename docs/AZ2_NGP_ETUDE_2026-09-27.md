# AZ-2 — étude Neo Geo Pocket / Color (2026-09-27)

> **Historique — RACE retiré du dépôt le 2 octobre 2026.** Le cœur RACE est sous GPLv2 seule, incompatible avec la GPLv3 d'AZ-2 ; le code, l'adaptateur et la cible labo décrits ici ont été supprimés (consultables dans l'historique Git). Voir `AZ2_ETAT_ACTUEL.md` et `AZ2_LICENCES.md`.

## Mise à jour de clôture de session

Le prototype RACE a finalement été raccordé au firmware `screen_esp` pour
aligner le bouton jaune, les commandes, le rendu, l’audio et la sauvegarde
avec les autres émulateurs. Cette intégration est compilée, mais elle n’est
pas encore acceptée comme production : la validation matérielle a été
suspendue après une sonde d’affichage direct qui a été retirée, puis le
firmware standard a été restauré. La sonde double cœur NGP reste désactivée.

La mesure disponible reste celle de la cible labo isolée : environ 17 fps,
soit 28,3 % d’une cadence 60 Hz. Il ne faut pas présenter cette valeur comme
la vitesse de l’intégration `screen_esp` tant qu’un test dédié n’a pas été
refait.

## Résultat court

Le candidat technique le plus adapté à l’ESP32-S3 est **RACE** : il possède
un cœur TLCS-900H + Z80 + vidéo K2GE, il fournit NGP et NGPC, et un port
ESP32-S3/Nano-S3 existe déjà avec double buffering, optimisations IRAM et
audio asynchrone. La documentation Libretro donne une cadence cœur de 60 Hz,
les extensions `.ngp`/`.ngc`, les sauvegardes et les états.

Sources de référence : [RACE Libretro](https://github.com/libretro/RACE),
[documentation RACE](https://docs.libretro.com/library/race/) et
[port Retro-Go Nano-S3](https://github.com/nod3011/retro-go-nano-s3).

## Comparatif

| Cœur | Avantage | Limite pour AZ-2 | Décision |
| --- | --- | --- | --- |
| RACE | Déjà optimisé pour petits systèmes et déjà adapté à ESP32-S3 | GPLv2-only ; statut juridique à clarifier avant diffusion | prototype intégré techniquement, qualification et décision de licence en attente |
| Beetle NeoPop / Mednafen | Mature, NGP + NGPC, sauvegardes et états | GPLv2 ; plus lourd et aucune preuve ESP32-S3 dans notre cible | écarté pour le premier port |
| NgpCraft | MIT, cœur C++ moderne avec ABI C et audio/vidéo séparés | espace mémoire plat de 16 Mo, `std::vector`, environ 9 600 lignes C++ ; dépassement direct de la contrainte 8 Mo PSRAM | seule voie juridiquement simple pour `screen_esp`, mais nécessite une vraie refonte mémoire |

RACE est sous GPLv2-only dans son dépôt et dans le port Nano-S3. Le projet
AZ-2 est GPLv3 à cause notamment de composants déjà présents dans le firmware
Teensy ; on ne copie donc pas RACE dans `screen_esp` sans autorisation ou
sans garder un firmware séparé sous GPLv2.

## Matériel à prendre en compte

- écran AZ-2 : 480×480 RGB parallèle ;
- NGP/NGPC : image logique autour de 160×152, à centrer et agrandir ;
- contrôleur AZ-2 : croix, A, B, Select/Start et sortie dédiée ;
- audio : même chaîne Teensy que NES/GB, mais le cœur RACE produit son propre
  flux PSG à convertir en paquets AZ-2 ;
- sauvegarde : fichier `.ngf` par ROM dans l’adaptateur actuel ; la rotation
  atomique et la compatibilité avec les autres formats restent à qualifier ;
- mémoire : conserver les buffers vidéo en DRAM/PSRAM selon les besoins DMA,
  sans modifier le moteur GB/GBC.

## Décision de travail

1. **Référence performance** : conserver la cible firmware NGPC séparée,
   basée sur le port ESP32-S3 Nano-S3, pour reproduire les mesures sans
   perturber le firmware standard.
2. **Intégration production** : la branche technique RACE existe dans
   `screen_esp`, mais son acceptation dépend encore de la qualification
   matérielle et de la clarification de licence. Si RACE est écarté, étudier
   NgpCraft MIT avec une mémoire paginée/sparse adaptée à l’ESP32-S3.
3. Le bouton Neo Geo Pocket est intégré en jaune dans `screen_esp` et ouvre
   le navigateur NGP ; la cible labo reste disponible pour reproduire la
   mesure de performance sans toucher au firmware standard.

## Plan de port du prototype RACE

- isoler le cœur RACE et conserver ses notices GPLv2 ;
- remplacer `retro_compat` par un adaptateur AZ2 minimal : ROM SD, SRAM,
  input, framebuffer 160×152, audio PCM ;
- utiliser une double surface vidéo, puis un blit complet vers 480×480 ;
- mesurer FPS, temps CPU, pics, audio et stabilité sur plusieurs ROM `.ngp` et
  `.ngc` avant toute fusion dans `screen_esp` ;
- documenter séparément le firmware et son mode de flash.

## État

Étude technique terminée, candidat choisi : **RACE**. Le cœur et son
adaptateur sont maintenant présents dans le code de `screen_esp`, et la
cible labo compile toujours séparément. Le premier test matériel de la cible
labo charge une ROM `.ngp/.ngc`, reconnaît la PSRAM et produit environ 17 fps
avec le rendu 3×. Restent la qualification de l’intégration standard, la
validation audio/contrôles/sauvegarde sur plusieurs ROMs et la décision de
licence GPLv2.
