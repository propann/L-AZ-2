# AZ-2 — audit des émulateurs et alignement de session

Date de gel : 27 septembre 2026, après restauration du firmware standard
`screen_esp`. Aucun nouveau flash n’est prévu pendant la pause.

## Synthèse vitesse

Le pourcentage compare la cadence mesurée à une cadence de référence de 60 Hz
(59,73 fps pour le cycle natif GB/GBC, 60,1 fps pour NTSC NES). Il s’agit d’un
indicateur de vitesse, pas d’un score de compatibilité.

| Émulateur / cible | Mesure disponible | Référence | Vitesse | Statut |
| --- | ---: | ---: | ---: | --- |
| GB Peanut-GB labo X3 | 59,76 fps | 59,73 fps | **100,1 %** | validé sur matériel |
| GBC Walnut-CGB jeu normal | 59,64–59,82 fps | 59,73 fps | **99,8–100,2 %** | validé sur matériel |
| GBC intro double-vitesse | 47–51 fps | 59,73 fps | **78,7–85,4 %** | comportement observé, à distinguer d’un bug |
| NES `screen_esp` | 49–50 fps | 60,1 fps | **81,5–83,2 %** | stable avec `FRAMESKIP` |
| NGP RACE labo isolé | environ 17 fps | 60 Hz | **28,3 %** | premier boot, non pleine vitesse |
| NGP RACE intégré `screen_esp` | non mesuré après restauration | 60 Hz | **N/D** | qualification suspendue |

La cible à poursuivre pour le NES est donc environ 10 à 11 fps, mais seulement
après avoir conservé le son, les sauvegardes, les boutons et l’affichage. Pour
NGP, il ne faut pas extrapoler la mesure du firmware labo à l’intégration
standard.

## État par émulateur

### Game Boy / Game Boy Color

- `gb_emulator.cpp` est le backend compilé par `screen_esp` : Walnut-CGB avec
  support DMG/GBC.
- `gb_emulator_peanut.cpp` est un backend alternatif Peanut-GB, exclu de
  `screen_esp` pour éviter la collision d’interface ; il reste utilisé par
  les environnements labo Peanut.
- Chargement ROM, rendu, navigation, audio mono, capture et sauvegarde sont
  présents dans les adaptateurs AZ-2. La vitesse Walnut validée en jeu normal
  est proche de 100 %.
- À qualifier encore : plusieurs ROMs, session d’au moins 30 minutes,
  aller-retour complet des sauvegardes, couleurs et scintillement sur une
  matrice plus large.

### NES

- `nes_emulator.cpp` est intégré à `screen_esp` avec CPU/PPU, contrôleur,
  audio APU, SRAM et télémétrie. `AZ2_NES_DUAL_CORE` lance l’APU sur le core 0.
- `nes_core/` fournit le CPU, le PPU et les mappers retenus par le cœur ; un
  fichier ne doit pas être classé mort uniquement parce qu’il n’est pas appelé
  directement depuis `main.cpp`.
- Le niveau actuel est stable à 49–50 fps avec `FRAMESKIP`, mais inférieur à
  la cadence NTSC. Le probe d’affichage Core 0 a été retiré après une mesure
  régressive à 36,8 fps.
- À qualifier encore : matrice de mappers, plusieurs ROMs, durée longue,
  sauvegarde SRAM après redémarrage et qualité audio sans artefact.

### Neo Geo Pocket / Color

- Le cœur RACE est ajouté par le `build_src_filter` de `screen_esp` et
  raccordé par `ngp_emulator.cpp` : scan `.ngp/.ngc`, PSRAM, rendu, commandes,
  paquets audio et fichiers `.ngf`.
- La cible `screen_esp_ngp_race_lab` reste reproductible et a mesuré environ
  17 fps. Cette cible a été flashée pendant les essais, puis le firmware
  standard `screen_esp` a été restauré.
- L’intégration standard n’a pas encore reçu une campagne matérielle complète
  après l’abandon de la sonde d’affichage direct. Son pourcentage est donc
  volontairement **N/D**.
- Risque à traiter avant diffusion : RACE et sa notice sont GPLv2-only alors
  que le firmware AZ-2 est documenté GPLv3. Il faut obtenir une clarification,
  isoler juridiquement la cible, ou remplacer le cœur avant de déclarer NGP
  production.

## Code dormant ou potentiellement mort

Ce tableau identifie des candidats à vérifier ; rien n’est supprimé pendant
cette pause. Les environnements labo comptent comme des utilisateurs du code.

| Zone | Indice | Confiance | Action sûre |
| --- | --- | --- | --- |
| `gb_emulator_peanut.cpp` dans `screen_esp` | explicitement exclu du filtre de production | haute pour la cible courante, nulle pour les labos Peanut | conserver tant que les environnements Peanut existent |
| Branches `AZ2_DIRECT_PANEL` | absentes du build standard, utilisées par des probes écran | haute pour `screen_esp` | ne pas supprimer avant archivage des probes |
| `AZ2_GB_DUAL_CORE_BLIT` | absent de la cible standard, actif dans les envs core-task | haute pour `screen_esp` | conserver pour les validations GB/GBC séparées |
| `AZ2_NGP_DUAL_CORE_BLIT` + pipeline Core 0 | conditionnel et désactivé après régression visuelle/commandes | haute | conserver comme expérience documentée, retester isolément avant décision |
| `ngpBlitWaitIdle()` | no-op lorsque le pipeline NGP dual-core n’est pas défini | haute | simplification possible après qualification NGP standard |
| `az2NgpSetSavePath()` | adaptateur actuellement no-op ; le chemin global est construit ailleurs | haute | soit brancher réellement l’API, soit supprimer après test save/load |
| `nes_core/profiler.cpp`, `flash_mmap.cpp` | utilitaires internes, non prouvés morts par une recherche d’appels | faible | vérifier la map/linker et les références du cœur avant suppression |
| symboles/globals RACE | plusieurs sont requis par les macros C du cœur | faible | ne pas nettoyer manuellement sans build + test ROM |

La prochaine vérification utile est un audit de map du linker par cible, puis
un test de référence après chaque suppression candidate. `rg` seul ne suffit
pas pour les mappers, les macros de compilation et les callbacks C.

## Point de reprise

1. Garder le firmware standard actuellement restauré ; ne pas flasher pendant
   la pause.
2. Vérifier une dernière fois démarrage NES, boutons Start/Select, audio et
   sauvegarde sur la carte avant toute optimisation.
3. Mesurer NGP intégré avec le framebuffer standard uniquement, sans réactiver
   le pipeline Core 0/direct.
4. Produire les maps de linker `screen_esp`, `screen_esp_ngp_race_lab` et les
   variantes GB, puis classer les symboles réellement non référencés.
5. Décider le traitement de la licence RACE avant de publier une version NGP.
6. Reprendre ensuite l’optimisation NES vers 60 Hz, avec une mesure séparée
   pour CPU/PPU, audio et affichage.

