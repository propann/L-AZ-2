# AZ-2 — Guide du projet (français)

[🇬🇧 English](README.en.md) · [🇪🇸 Español](README.es.md) · [Accueil GitHub](../../README.md)

> **Prototype alpha.** « Intégré » signifie présent dans le code ; « compilation CI réussie » ne constitue pas une validation sur le matériel réel. Vérifier le [journal d'essais](../AZ2_ETAT_DES_LIEUX.md) avant de déclarer une fonction validée.

## Construire un instrument

AZ-2 réunit une groovebox, un tracker huit pistes, sept moteurs de synthèse/lecture et une console Game Boy / Game Boy Color. L'objectif est de jouer, composer, capturer le son du jeu et réutiliser une capture comme instrument. AZ-2 conserve **deux cartes**, sans multiplexeur ni rack ESP supplémentaire : le rack appartient au projet AZ-3.

| Carte | Rôle | Stockage |
| --- | --- | --- |
| Teensy 4.1 | Timing du tracker, moteurs audio, MIDI, enregistrement WAV, lecture de samples, sortie I²S vers PCM5102A | Sa propre SD pour les captures ; PSRAM pour le sample dynamique |
| ESP32-S3 VIEWE UEDX48480040E-WB | Écran tactile 480×480, menus, émulation GB/GBC, navigation des ROMs | Sa propre SD pour les ROMs et sauvegardes |

Les commandes et l'audio Game Boy circulent sur un UART partagé à **921600 bauds**. Le contrat commun est défini dans `lib/AZ2_Protocol/AZ2_Protocol.h` : **modifier les deux firmwares et leurs tests ensemble**. Le mode de production reste **V1, PCM8 mono 14 kHz** ; le pilote V2 stéréo est présent, mais désactivé.

## Installation et mise à jour

Lire d'abord le [guide de câblage](../AZ2_CABLAGE_MASTER.md) et [les précautions de premier démarrage](../AZ2_DEMARRAGE.md). Vérifier la tension, la masse commune et les connexions TX/RX avant alimentation. Sauvegarder séparément les deux cartes SD, notamment les fichiers `.sav`, `.rtc`, ROMs et `.wav`, ainsi que la paire précédente de firmwares fonctionnels.

```bash
git clone https://github.com/propann/L-AZ-2.git
cd L-AZ-2
python -m pip install platformio==6.1.19
python tools/check_firmware_contract.py
pio test -e native
pio run -e master_teensy -e screen_esp
# Après vérification du matériel et des sauvegardes :
pio run -e master_teensy -t upload
pio run -e screen_esp -t upload
```

Les deux compilations proviennent du **même SHA Git**. Ne pas mettre à jour une seule carte avec un changement de protocole incompatible. Aucun jeu commercial ou contenu utilisateur privé n'est inclus.

## Émulation, sauvegardes et musique

- **GB/GBC :** cœur Walnut-CGB, sélection des ROMs depuis la SD ESP32 et commandes physiques ; compatibilité, vitesse réelle et absence de défauts encore à qualifier jeu par jeu (dont LSDJ, Tetris et Mario).
- **SRAM :** sauvegarde périodique et manuelle, fichiers `.sav/.bak`. Une erreur de sauvegarde bloque la décharge de la cartouche pour éviter de perdre des changements en RAM.
- **RTC MBC3 :** fichier `.rtc` distinct, versionné avec CRC32 et secours `.bak`. L'horloge n'avance hors tension que si l'heure système ESP32 est valide ; la fiabilité lors de coupures doit encore être testée sur matériel.
- **Audio GB :** V1 PCM8 mono 14 kHz vers le Teensy ; V2 transporte en mode expérimental L/R PCM8 stéréo 14 kHz avec CRC16, séquence et négociation, **désactivé par défaut**. Le bus de sortie Teensy reste mono pour l'instant, même avec V2.
- **Capture → sampleur :** `REC:START` / `REC:STOP` créent un WAV sur la SD du Teensy. Le dernier WAV valide peut être chargé dans un slot PSRAM de 30 s environ et joué via le patch `SAMPLER / GB Capture` (index 2). Les patches 0/1 restent Kick/Snare. Le sample rate d'origine est respecté. Le chargement est également tenté au démarrage. Il n'existe pas encore de navigateur multi-captures.

## Travail réalisé et traçabilité — 19 septembre 2026

| Domaine | Modification livrée dans le code | Preuve / limite |
| --- | --- | --- |
| Persistance GB | RTC MBC3 distinct avec CRC32, rotation temporaire/secours et reprise horaire conditionnelle | [Commit RTC](https://github.com/propann/L-AZ-2/commit/44d56dac643593e9f499b872050680185dd9274f) ; tests coupure/RTC réels à faire |
| Audio couplé | Transport V2 stéréo L/R, récepteur Teensy avec downmix explicite, V1 inchangé en production | [Émetteur](https://github.com/propann/L-AZ-2/commit/e08af9c180c42ed0407f55907dedbb00361cf593) · [Récepteur](https://github.com/propann/L-AZ-2/commit/9d0c8263c625264b2a9a2ba88eac7778c03a6a74) ; pilote non activé |
| Création musicale | Patch partagé GB Capture, lecture WAV → PSRAM, adaptation au sample rate et reprise au boot | [Intégration sampleur](https://github.com/propann/L-AZ-2/commit/2aa8fbfc99d2483be684f5009462c3f760410ee7) ; qualification audio/SD requise |
| Vérification | Test du patch dynamique, compilation Teensy + ESP32-S3 et tests natifs du contrat partagé sur la même révision | [Exécution CI verte](https://github.com/propann/L-AZ-2/actions/runs/35455889760) pour `2ea9e03285814f4ebcb6844b7758ccf5f931891d` ; **pas un test physique** |

La documentation détaillée est dans [la roadmap GB/LSDJ](../AZ2_GB_ROADMAP_IMPLEMENTATION.md), [le protocole V2](../AZ2_PROTOCOL_AUDIO_V2.md), [le sampleur](../AZ2_SAMPLEUR.md) et [le manuel utilisateur](../AZ2_MANUEL_UTILISATEUR.md). Les textes historiques de planification peuvent mentionner du matériel abandonné ; prendre le câblage actuel, `platformio.ini` et ce guide comme références pour la paire active.

## Reste à faire

Qualifier les sauvegardes/RTC avec coupures de courant, mesurer FPS/audio sur le prototype, tester les jeux et LSDJ avec résultats reproductibles, conserver la stéréo jusqu'au DAC sans dépasser la mémoire audio du Teensy, puis développer navigation, édition et affectation multi-captures. Ne pas annoncer ces points comme achevés parce que la CI compile.

## Contribution et licences

Les signalements utiles précisent le SHA, la carte, les étapes de reproduction, les logs et une ROM de test libre. Ne pas publier de ROM commerciales, de sauvegardes personnelles ni de secrets. Voir [CONTRIBUTING.md](../../CONTRIBUTING.md), [LICENSE](../../LICENSE) et [licences tierces](../AZ2_LICENCES.md).

## Écran de veille — styles et commandes

Dans **CONFIGURATION**, choisissez entre **Matrix** (caractères classiques), **Pluie de notes** (notes issues des pas du pattern courant), **8 pistes** (une colonne par piste du tracker) et **Dashboard** (BPM, pattern, pas, état PLAY/STOP, moteurs et notes des huit pistes). Les notes et événements proviennent du miroir de tracker déjà tenu sur l'ESP32, sans ajouter de trafic UART pour l'animation.

**Croix :** HAUT/BAS choisissent la ligne de réglage ; GAUCHE/DROITE modifient le style, le délai, la gamme ou le swing. **A :** valide le choix en passant à la valeur suivante. **Tactile :** touchez les flèches de la même ligne pour effectuer exactement la même action. Un bouton ou une touche réveille l'écran. Le style et le délai sont conservés dans la mémoire NVS de l'ESP32 ; un délai de zéro désactive la veille.

La veille ne suspend pas le tracker ni l'audio. Son rendu doit encore être vérifié sur l'écran réel pour la fluidité, la lisibilité et l'absence d'interférence avec le jeu GB.
