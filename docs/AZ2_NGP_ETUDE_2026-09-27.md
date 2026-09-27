# AZ-2 — étude Neo Geo Pocket / Color (2026-09-27)

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
| RACE | Déjà optimisé pour petits systèmes et déjà adapté à ESP32-S3 | GPLv2-only ; ne peut pas être lié directement au firmware AZ-2 GPLv3 sans clarification de licence | meilleur prototype de performance, dans une cible séparée |
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
- sauvegarde : fichier `.sav` par ROM, avec rotation `.tmp`/`.bak` comme NES ;
- mémoire : conserver les buffers vidéo en DRAM/PSRAM selon les besoins DMA,
  sans modifier le moteur GB/GBC.

## Décision de travail

1. **Prototype performance** : RACE dans une cible firmware NGPC séparée,
   basée sur le port ESP32-S3 Nano-S3, pour vérifier l’affichage AZ-2, les
   boutons, le son et les sauvegardes sans contaminer `screen_esp`.
2. **Intégration production** : uniquement après validation du prototype et
   clarification de licence. Si l’intégration doit rester dans `screen_esp`,
   partir sur NgpCraft MIT avec une mémoire paginée/sparse adaptée à l’ESP32-S3,
   plutôt que de copier son espace plat de 16 Mo.
3. Le bouton Neo Geo Pocket reste visuellement présent mais ne sera pas rendu
   jouable tant qu’un cœur réellement intégré et testé n’existe pas.

## Plan de port du prototype RACE

- isoler le cœur RACE et conserver ses notices GPLv2 ;
- remplacer `retro_compat` par un adaptateur AZ2 minimal : ROM SD, SRAM,
  input, framebuffer 160×152, audio PCM ;
- utiliser une double surface vidéo, puis un blit complet vers 480×480 ;
- mesurer FPS, temps CPU, pics, audio et stabilité sur plusieurs ROM `.ngp` et
  `.ngc` avant toute fusion dans `screen_esp` ;
- documenter séparément le firmware et son mode de flash.

## État

Étude terminée, candidat technique choisi : **RACE en prototype isolé**.
L’intégration dans le firmware de production attend la décision de licence
entre firmware NGPC séparé et port MIT NgpCraft.
