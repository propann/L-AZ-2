# AZ-2 — validation NES et bilan d’optimisation (2026-09-27)

Ce compte rendu fige l’état réellement testé sur l’ESP32-S3 de l’écran avant
le passage à l’étude Neo Geo Pocket.

## Cible et firmware

- Carte : ESP32-S3 N16R8, PSRAM 8 Mo, 240 MHz, écran RGB parallèle 480×480.
- Port de flash : `/dev/ttyUSB0`.
- Branche : `nes-emulation`.
- Environnement PlatformIO : `screen_esp`.
- Le firmware de production conserve le cœur Walnut-CGB/Peanut-GB existant.
  Les modifications NES sont isolées par `AZ2_NES_ENABLED`.

## Fonctionnalités NES livrées

- Chargement des ROM `.nes` depuis `/games` et sous-dossiers.
- Cœur 6502/PPU/APU Anemoia-ESP32 intégré avec mappers 0, 1, 2, 3, 4 et 69.
- Boutons NES : A, B, Select et Start ; Select+Start quitte la partie.
- Sauvegarde SRAM batterie dans `/games/<rom>.sav`, avec fichier temporaire,
  backup, en-tête de version, CRC ROM et contrôle de taille.
- Touche D pour sauvegarder manuellement.
- Audio APU NES cadencé réellement, regroupé en paquets PCM8 centrés sur 128
  pour le protocole audio existant vers le Teensy.
- APU déplacée sur le Core 0 ; CPU/PPU restent ordonnés sur le Core 1.
- Échelle NES 256×240 vers 480×480 avec rotation 180° conservée.
- Cartes GB, GBC et NES affichées en jaune dans le sélecteur.

## Bugs rencontrés et corrections

### Reboot au lancement d’une ROM

Le log indiquait un `Interrupt wdt timeout on CPU1`. Deux causes ont été
confirmées :

1. l’écriture UART audio était appelée à l’intérieur d’une section critique
   APU ;
2. le rendu de 30 bandes NES s’enchaînait sans céder la main au watchdog.

L’écriture série est maintenant faite après la section critique et un `yield()`
contrôlé est effectué pendant le rendu des bandes. Le reboot n’a plus été
observé pendant les mesures suivantes.

### Artefacts d’affichage

L’ancien blit écrivait 256×2 = 512 pixels dans une ligne de framebuffer de
480 pixels. Les 32 pixels de débordement expliquaient les petits défauts
visuels. Le nouveau chemin calcule correctement le mapping 256→480, duplique
verticalement les lignes et ne déborde plus du framebuffer.

### Compteur de vitesse

La formule du compteur divisait le FPS réel par 10 : les valeurs affichées
`4,7` correspondaient à environ `47 FPS`. La formule est corrigée en FPS×10.

## Mesures matérielles

Commande utilisée :

```text
/tmp/az2-tools-venv/bin/python tools/test_nes_speed.py /dev/ttyUSB0 --seconds 15
```

Mesure avant le pipeline d’affichage Core 0, après correction du mapping et
de l’APU :

- régime stable : **49–50 FPS d’émulation** ;
- temps CPU/PPU + blit inclus : **environ 19,5 ms par frame** ;
- pics habituels : **34 ms**, avec des pointes occasionnelles de **55–75 ms** ;
- cible NES NTSC : **60,1 FPS**.

`FRAMESKIP` reste activé dans `screen_esp` : une frame sur deux saute le rendu
PPU lourd. Le compteur mesure les frames d’émulation, pas le nombre d’images
complètes effectivement dessinées.

## Optimisations conservées

- `-O3` pour le firmware écran ; `-flto` a été essayé puis retiré car le
  linker Arduino/ESP-IDF échouait.
- Optimisations GCC ciblées sur les fichiers du cœur NES uniquement.
- APU sur Core 0 par lots de 512 cycles, avec verrou court et écriture UART
  hors verrou.
- Table horizontale 480→256 pré-calculée, supprimant les divisions du chemin
  chaud du blit.
- `esp_cache_msync()` par bande de 16 lignes, conservé pour éviter écran noir
  et tearing sur le panneau RGB.

## Essai écarté

Un pipeline de trois buffers pour déplacer le blit sur le Core 0 a été testé.
Il a ajouté une copie de 4 Ko par bande et les files se sont remplies : la
mesure est descendue à **36,7–36,8 FPS**. Cette version a été retirée et le
firmware stable à ~50 FPS a été reflashé.

## État après flash final

- Build `screen_esp` réussi.
- `git diff --check` réussi.
- Flash et vérification SHA réussis.
- Utilisation firmware stable : environ **66,3 % RAM**, **11,2 % flash**.
- Aucune modification du comportement GB/GBC dans cette passe.

## Suite décidée

Avant toute nouvelle optimisation NES, conserver ce point de référence et
étudier séparément un cœur Neo Geo Pocket / Neo Geo Pocket Color compatible
avec l’ESP32-S3 et la licence du projet. L’intégration devra reprendre le
même principe : cœur isolé, callback vidéo, contrôleur, sauvegardes et mesure
matérielle avant activation dans le menu.
