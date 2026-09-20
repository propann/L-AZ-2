# AZ-2 — Candidats multiémulateur

## Intention

Idée à conserver pour une phase ultérieure : ajouter plusieurs cœurs
d’émulation dans le firmware ESP32-S3, avec un menu commun et un seul cœur
actif à la fois. Le moteur audio, le séquenceur et le Teensy restent hors du
cœur d’émulation. Cette note ne lance aucune intégration.

## Base matérielle

Le projet utilise un ESP32-S3 avec PSRAM et un écran RGB parallèle 480×480.
Le Game Boy / Game Boy Color actuel (Walnut-CGB) reste la référence. Les
mesures récentes montrent que son principal coût est actuellement le transfert
et l’agrandissement de l’image, pas seulement le calcul CPU. Un nouveau cœur
doit donc être mesuré sur le matériel réel avec audio actif et écran actif.

## Liste de candidats

| Système | Candidat | Vitesse attendue | Priorité | Commentaire |
|---|---|---:|---:|---|
| Game Boy | Walnut-CGB actuel | proche de la cible, à stabiliser | Référence | Garder le cœur intégré et corriger les sauts d’image. |
| Game Boy / GBC | GNUBOY Retro-Go | GB mesuré plus rapide, GBC à qualifier | Comparaison | Déjà testé en probe ; il n’a pas remplacé Walnut-CGB. |
| NES / Famicom | cœur NES de Retro-Go | probablement plein débit | Haute | Bon premier candidat après GB ; mapper et rendu à tester par lots de ROM. |
| Sega Master System / SG-1000 | cœur SMS de Retro-Go | probablement plein débit | Haute | Z80 et vidéo plus légers que GBC ; mêmes besoins d’intégration écran/audio. |
| Game Gear | cœur SMS/GG de Retro-Go | probablement plein débit | Moyenne | Même famille que SMS, mais résolution et palette à valider. |
| ColecoVision | cœur Coleco de Retro-Go | probablement plein débit | Moyenne | Candidat léger ; intérêt secondaire pour AZ-2. |
| Atari Lynx | cœur Lynx de Retro-Go | à mesurer | Basse | Possible sur ESP32, mais affichage et mémoire demandent un port dédié. |
| PC Engine | cœur PCE de Retro-Go | possible, à mesurer | Basse | Retro-Go annonce des améliorations de performance et des jeux à 100 %, mais le coût vidéo/audio doit être mesuré sur notre écran. |
| Mega Drive / Genesis | cœur de Retro-Go | incertain | Exploratoire | Ne pas promettre le plein débit sans test ; plus lourd et très dépendant du jeu. |
| SNES | cœur de Retro-Go | insuffisant / lent | Écarté pour l’instant | Retro-Go le décrit lui-même comme lent ; ne pas investir avant une preuve sur le matériel. |
| Game & Watch | application Retro-Go | plein débit attendu | Bonus | Très léger, mais intérêt ludique plutôt que musical. |

Retro-Go documente officiellement les systèmes NES, SNES (lent), Game Boy,
GBC, Game & Watch, SG-1000, Master System, Mega Drive, Game Gear, ColecoVision,
PC Engine et Lynx. Son framework demande un ESP32 avec PSRAM, ce qui correspond
à notre ESP32-S3 équipé de PSRAM, mais cela ne garantit pas un port direct sur
notre contrôleur RGB.

## Architecture proposée, à garder pour plus tard

1. Un contrat commun `EmuCore` : init, chargement ROM, reset, frame, audio,
   entrées, sauvegarde et arrêt.
2. Un seul cœur chargé et actif à la fois ; aucun émulateur concurrent en RAM.
3. Une couche frontend partagée : navigateur ROM, contrôles, écran, boutons,
   statistiques et retour au menu.
4. Un dossier par cœur avec ses crédits et sa licence. Les composants GPLv2
   doivent rester identifiés et leurs notices conservées.
5. Un test de qualification par système : cadence, image sans saut, audio,
   sauvegarde, sortie propre et consommation mémoire.

## Ordre recommandé si l’idée devient un chantier

1. Stabiliser Walnut-CGB sur l’écran réel.
2. Porter le cœur NES de Retro-Go dans une cible isolée.
3. Mesurer SMS/Game Gear avec le même protocole de performance.
4. Ajouter seulement les cœurs qui tiennent la vitesse avec audio et affichage
   actifs.
5. Construire ensuite le menu multiémulateur ; ne pas mélanger les essais avec
   le firmware musical principal.

## Sources et licences à vérifier avant intégration

- Retro-Go : https://github.com/ducalex/retro-go
- Framework Retro-Go et liste des appareils :
  https://github.com/ducalex/retro-go/tree/master/components/retro-go
- Licence annoncée par Retro-Go : GPLv2. Chaque cœur importé devra être
  vérifié séparément avant redistribution.

