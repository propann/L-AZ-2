# AZ-2 — reste à faire (au 4 octobre 2026)

Liste unique et à jour de ce qui n'est **pas** fini. Elle remplace les
listes « reste à faire » dispersées dans les audits datés et le « plan
actif » du 23 septembre de la [feuille de route](AZ2_FEUILLE_DE_ROUTE.md).
Ce qui marche déjà : [état actuel vérifié](AZ2_ETAT_ACTUEL.md) et
[CHANGELOG](../CHANGELOG.md).

Légende : **[bug]** défaut constaté · **[à finir]** commencé, pas livré ·
**[à tester]** présent dans le code, jamais qualifié sur la machine ·
**[idée]** pas commencé.

## Son et moteurs

- **[bug] SPECTRAL ~12 dB trop bas** par rapport aux autres moteurs, mesuré
  au banc (`tools/engine_bench.py`). À corriger dans le firmware du rack
  SPECTRAL, pas dans le Teensy.
- **[à tester] Rack GRANULAR + SPECTRAL en endurance** : notes, arrêt, PANIC,
  presets et tous les paramètres, séparément puis ensemble, 30 à 60 min ;
  redémarrage ou déconnexion d'un ESP sans bloquer les moteurs locaux.
- **[à finir] Samples du granulaire** : choisir un WAV de la SD du Teensy
  depuis PATCH, afficher nom/durée/transfert/erreur, restauration avec le
  projet.
- **[à finir] Bibliothèque multi-captures GB** et explorateur intégré dans
  PATCH (la capture GB → WAV → SAMPLER fonctionne, la gestion de plusieurs
  captures non).
- **[bug non prouvé] Bip parasite ~1 Hz** : plus entendu depuis le 1er
  octobre, cause racine jamais prouvée. À la prochaine occurrence, relever
  `RACK:STATUS` et `GB:AUDIO_RX` avant toute coupure (voir
  [enquête](AZ2_BIP_PARASITE_2026-09-28.md)).

## Interface

- **[idée] Page MOTEURS animée** : y reprendre la vue animée par moteur de
  la page PATCH (aujourd'hui seule une petite icône bouge).
- **[à finir] Croix sur la page PATCH** : depuis la ligne PISTE, BAS passe
  d'abord par la liste des presets avant la grille. Proposition : BAS entre
  directement dans la grille, DROITE va aux presets.
- **[à finir] Retirer la trace de diagnostic `TOUCH:N=`** du tactile,
  maintenant que le multipoint est validé.
- **[à tester] Écran de veille** : fluidité, lisibilité et absence
  d'interférence avec les jeux sur l'écran réel.
- **[connu] Léger sautillement** de l'écran en GB X2 lors des gros
  redessins (limite de bande passante PSRAM ; X3 et les pages musique sont
  propres).

## Jeux

- **[à finir] NES à pleine vitesse** : 49–50 fps aujourd'hui (~82 %). Un
  essai « dessin sur le cœur 0 » est rangé dans un stash git (`essai NES
  coeur 0`) : il redémarre la machine sur START, à reprendre.
- **[à tester] Sauvegardes SRAM et RTC face aux coupures de courant.**
- **[à tester] Matrice de ROMs** homebrew légales (image, commandes, son,
  sauvegarde, session de 30 min) et LSDJ.
- **[idée] Synchronisation musicale LSDJ ↔ tracker** et stéréo jusqu'au
  DAC (transport V2 présent mais désactivé, sortie Teensy mono).

## Fiabilité et code

- **[à tester] Sauvegarde/chargement de projet** sous coupure pendant
  l'écriture : 20 cycles sans corruption ; stress de 30 min séquenceur +
  9 moteurs sans gel.
- **[à finir] CI** : compiler les quatre firmwares de production (Teensy,
  écran, granulaire, spectral) à chaque PR, avec les tests natifs.
- **[à finir] Découper les gros `main.cpp`** (écran ~11 000 lignes, Teensy
  ~6 000) par pages et par sous-systèmes, sans changer le comportement.

## Matériel

- **[bug] Le Teensy disparaît parfois de l'USB** pendant le flash (3 fois le
  4 octobre) : tester un autre câble et le hub USB.
- **[idée] MIDI DIN** : le 6N138 n'est pas monté et le code MIDI a été
  supprimé. À reprendre seulement après montage et validation électrique
  (puis horloge MIDI 24 PPQN).
- **[idée] Publication** : photos du prototype, schéma reproductible,
  démonstrations audio et vidéo.
