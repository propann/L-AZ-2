# Étude des cœurs Game Boy / Game Boy Color pour AZ-2

Date : 19 septembre 2026
Cible matérielle : ESP32-S3 (émulation, écran, SD) + Teensy 4.1 (mixage et DAC)

## Décision

AZ-2 conserve **Walnut-CGB `master`** comme cœur principal. Le fichier déjà
embarqué est identique, hors fins de ligne, au commit amont `a42c186` du
17 août 2026. Le remplacer par un autre cœur aujourd'hui ajouterait beaucoup
de risque sans résoudre le véritable goulet d'étranglement : le rendu écran et
le transport audio entre les deux cartes.

Les choix retenus sont :

1. Walnut-CGB `gb_run_frame_dualfetch()` pour le chemin CPU rapide officiel ;
2. rendu de chaque frame, sans frame-skip en mode normal ;
3. transferts écran groupés par bandes ;
4. SameBoy comme oracle de précision et référence pour les tests ;
5. Gb_Snd_Emu comme premier candidat APU précis ;
6. aucun code de la branche expérimentale Walnut tant que son API n'est pas
   stabilisée ; ses horodatages APU servent cependant de modèle au futur pont
   audio.

## Comparatif étudié

| Projet | Licence | Forces | Limites pour AZ-2 | Rôle retenu |
|---|---|---|---|---|
| [Walnut-CGB](https://github.com/Mr-PauI/Walnut-CGB) | MIT | GB/GBC, MBC1/2/3/5, RTC, DMA 32 bits, dual-fetch, pensé MCU/ESP32-S3 | LCD ligne par ligne, précision incomplète, APU externe | Cœur embarqué |
| [Peanut-GB](https://github.com/deltabeard/Peanut-GB) | MIT | Très compact, portable, tests CPU Blargg | Moins rapide et moins ambitieux que son dérivé Walnut ; précision non garantie | Référence historique seulement |
| [SameBoy](https://github.com/LIJI32/SameBoy) | Expat/MIT | Très haute précision, tests Blargg/Mooneye, LCD T-cycle, APU stéréo de qualité | Cœur beaucoup plus volumineux et fortement intégré ; port ESP32-S3 à mesurer avant tout remplacement | Oracle de conformité |
| [Gearboy](https://github.com/drhelius/Gearboy) | GPLv3 | Cœur C++ précis, GB/GBC/SGB, bonne architecture de bureau/libretro | Plus lourd ; aucune preuve qu'il cohabite à 60 Hz avec l'UI 480×480 AZ-2 | Référence secondaire |
| [Retro-Go](https://github.com/ducalex/retro-go) / gnuboy | GPLv2 et licences composantes | Techniques ESP32 éprouvées, audio stéréo et statistiques de frames | Firmware complet à remplacer, cœur ancien ; GPLv2-only potentiellement incompatible avec le dépôt GPLv3 | Étude d'architecture, aucun code copié |
| [Game_Music_Emu / Gb_Snd_Emu](https://github.com/libgme/game-music-emu) | LGPL-2.1 | Écritures de registres horodatées, synthèse band-limited, stéréo, portable | Adaptation Walnut et mesure CPU/RAM nécessaires ; obligations LGPL à documenter | Candidat APU précis |

Versions inspectées :

- Walnut-CGB `a42c186` (`master`) et `dcc8092` (`experimental`) ;
- Peanut-GB `8e65698` ;
- SameBoy `213a12c` ;
- Gearboy `340ebe3` ;
- Retro-Go `4ced120` ;
- Game_Music_Emu `f68963b`.

## Pourquoi Walnut reste le bon châssis

Walnut-CGB est un dérivé de Peanut-GB réécrit autour d'un dispatch
**dual-fetch** et de lectures/transferts 16/32 bits. Son auteur indique
explicitement que `gb_run_frame_dualfetch()` est le chemin normal rapide. AZ-2
appelait pourtant le chemin historique `gb_run_frame()`, revenant de fait aux
performances Peanut-GB.

Le cœur couvre déjà les cinq cibles principales :

- LSDJ ;
- Tetris DMG ;
- Super Mario Land ;
- Tetris DX ;
- Super Mario Bros. Deluxe.

Changer tout le cœur avant d'avoir mesuré le dual-fetch et supprimé les 144
transactions écran par image aurait masqué la cause réelle du ralentissement.

## Ce que la branche expérimentale Walnut apporte

La branche `experimental` ajoute notamment :

- un compteur APU dans le domaine 4,194304 MHz ;
- un timestamp sur chaque `audio_write()` ;
- plusieurs corrections LCD ;
- MBC7 ;
- des annotations de fonctions chaudes.

L'idée d'horodater les écritures APU est la bonne : MiniGB APU reçoit
actuellement seulement « registre + valeur », sans savoir à quel cycle de la
frame le changement s'est produit. Mais la branche est annoncée comme
expérimentale et modifie aussi des zones sans rapport avec les jeux cibles.
AZ-2 reprendra seulement le contrat temporel nécessaire au nouveau moteur
audio, après un test A/B reproductible.

## Audio : verdict technique

MiniGB APU est rapide mais ne convient pas comme cible finale LSDJ :

- les écritures de registres ne sont pas horodatées ;
- le rendu se fait une fois par frame ;
- AZ-2 écrase ensuite la stéréo en mono ;
- la conversion 16 bits vers 8 bits/14 kHz détruit encore une partie du signal.

Deux chemins ont été étudiés :

### Gb_Snd_Emu

Le sous-ensemble utile représente environ 68 Kio de source et repose sur
`Gb_Apu`, `Gb_Oscs`, `Blip_Buffer` et `Stereo_Buffer`. Il accepte exactement le
modèle nécessaire à Walnut : écritures de registres horodatées, fin de frame,
puis lecture d'un tampon stéréo band-limited.

Il est le meilleur candidat pour le premier prototype audio précis, sous
réserve de :

- benchmark ESP32-S3 ;
- vérification RAM des trois Blip buffers ;
- transport stéréo compatible avec l'UART 921600 bauds ;
- ajout de la licence LGPL-2.1 et des sources modifiables.

### APU SameBoy

SameBoy est plus précis et constitue l'objectif qualitatif, mais son APU est
liée à un gros état interne `GB_gameboy_t` et à l'ordonnanceur du cœur. En
extraire quelques fichiers ne suffit pas. Un port complet serait un deuxième
projet d'émulateur au milieu du premier ; il ne sera tenté que si Gb_Snd_Emu
échoue aux tests LSDJ.

## Changements intégrés dans le premier lot

- passage à `gb_run_frame_dualfetch()` ;
- suppression du frame-skip par défaut ;
- rendu 160×144 agrandi par bandes de 8 lignes, soit 18 appels écran par frame
  au lieu de 144 ;
- ordonnanceur exact 16 742/16 743 µs avec accumulateur fractionnaire ;
- journal `GB:PERF` : FPS en centièmes, durée moyenne/maximale et frames
  manquées ;
- taille SRAM déterminée par `gb_get_save_size_s()` : MBC2 fonctionne ;
- refus des `.sav` tronqués et récupération automatique depuis `.sav.bak` ;
- chemins de sauvegarde non tronqués ;
- dirty flag : aucune écriture SD si la SRAM n'a pas changé.

## Étapes suivantes

1. flasher l'ESP32-S3 et relever `GB:PERF` pour les cinq ROMs ;
2. comparer stabilité visuelle et temps maximum de frame ;
3. construire le protocole audio stéréo V2 dans la marge de l'UART ;
4. intégrer Gb_Snd_Emu avec écritures APU horodatées ;
5. lancer Mooneye, dmg-acid2 et les ROMs APU autorisées ;
6. conserver SameBoy comme sortie de référence pour comparer captures audio
   et captures d'écran.

## Règle de qualification

Une ROM n'est marquée « AZ-2 verified » qu'après 30 minutes sur la machine
réelle avec : 59,72–59,74 images/s, `missed=0`, aucune coupure audio, commandes
en moins d'une frame et sauvegarde restaurée bit à bit.
