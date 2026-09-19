# Audit ciblé émulation AZ-2 — LSDJ, Tetris et Mario

Date : 19 septembre 2026  
Révision auditée : [410f492](https://github.com/propann/L-AZ-2/commit/410f492e0c37c935c7da7dca982374a5c0316ee6)

## Verdict

Le matériel actuel peut faire fonctionner **LSDJ, Tetris et Mario sans ajouter de microcontrôleur**. Le cœur Walnut-CGB couvre le Game Boy monochrome et le Game Boy Color, les MBC1/2/3/5, le RTC, les palettes CGB et la double vitesse.

Cependant, le niveau actuel est encore celui d’un **émulateur intégré fonctionnel**, pas celui d’une console de référence :

- la logique vise bien 59,7275 images/s ;
- le rendu visible est limité à environ 30 images/s par le frame-skip ;
- le chemin CPU rapide de Walnut-CGB n’est pas utilisé ;
- l’audio est réduit de stéréo 16 bits à mono 8 bits/14 kHz ;
- MiniGB APU n’est pas assez précis pour faire de LSDJ un instrument fidèle ;
- les sauvegardes ont été sécurisées, mais plusieurs cas cartouche/RTC restent incomplets ;
- aucun protocole de qualification ROM par ROM ne prouve encore la vitesse officielle.

**Conclusion courte :**

- Tetris DMG : devrait être atteignable rapidement à vitesse normale.
- Super Mario Land DMG : devrait être atteignable rapidement.
- Tetris DX / Super Mario Bros. Deluxe GBC : plausibles, mais doivent être qualifiés sur le vrai écran.
- LSDJ : devrait démarrer et sauvegarder, mais la qualité audio actuelle n’est pas acceptable pour prétendre à une intégration musicale haut de gamme.

## Niveau visé : émulation de référence

Dans AZ-2, « au top » devient une exigence mesurable et non une simple impression :

- fidélité CPU, interruptions, timers, DMA et MBC validée par ROMs de test ;
- cadence logique et affichée native de 59,7275 Hz en mode normal ;
- audio APU stéréo à timing précis, sans réduction mono et sans dérive de hauteur ;
- latence totale commande → jeu inférieure à une image ;
- zéro frame logique perdue, zéro underrun audio et zéro corruption de sauvegarde ;
- compatibilité déclarée uniquement après essai prolongé sur le vrai AZ-2 ;
- le mode 30 FPS reste éventuellement disponible comme mode économie, jamais comme mode normal.

Tant que tous ces seuils ne sont pas validés, le projet doit parler d’« émulation expérimentale » et non d’émulation certifiée.

## Cibles de référence

Pour couvrir les chemins réellement importants, la qualification doit utiliser au minimum :

| Cible | Mode | Ce qu’elle valide |
|---|---|---|
| LSDJ 9.4.2 | DMG, MBC5 + SRAM | APU, wave channel, samples, panoramique, sauvegarde lourde, commandes combinées |
| Tetris | DMG | timing de base, contrôles, rendu simple, audio |
| Super Mario Land | DMG | scrolling, sprites, collisions, latence |
| Tetris DX | GBC | palettes, mode couleur, sauvegarde |
| Super Mario Bros. Deluxe | GBC | scrolling GBC, double vitesse, MBC et sauvegarde |

LSDJ est distribué officiellement comme ROM et l’éditeur recommande un émulateur fiable. Le site officiel liste notamment quatre canaux, lecture de samples, synthèse wavetable et synchronisation : [Little Sound Dj](https://www.littlesounddj.com/).

## Ce qui est déjà bon

### Cœur d’émulation

Le dépôt embarque Walnut-CGB avec :

- support DMG et CGB ;
- MBC1, MBC2, MBC3/MBC30 et MBC5 ;
- RTC ;
- double vitesse CGB ;
- DMA 32 bits ;
- rendu couleur ;
- contrôles Game Boy complets.

Le projet amont confirme ces fonctions, mais précise aussi que certains jeux restent incompatibles, que le rendu ligne par ligne a des défauts et que MiniGB APU n’a pas un timing exact : [documentation Walnut-CGB](https://github.com/Mr-PauI/Walnut-CGB).

### Cadence

La constante de 16 742 µs est très proche de la cadence réelle Game Boy, environ 59,7275 Hz. L’ancienne limite à 17 ms a bien été supprimée.

### Contrôles

Le mapping couvre :

- croix physique : directions ;
- A/B : boutons A/B ;
- clics encodeurs : Select/Start ;
- C : sortie de la partie ;
- bouton encodeur 0 : capture audio.

Le chemin est direct Teensy → UART → ESP32 ; il ne passe pas par le tactile pendant le jeu.

### ROMs et mémoire

Les ROMs sont chargées en PSRAM, ce qui convient aux tailles GB/GBC visées. La liste paginée accepte jusqu’à 40 ROMs.

### Sauvegardes

Depuis le correctif du 19 septembre :

- écriture atomique temporaire/backup ;
- sauvegarde à la sortie ;
- autosave toutes les 30 secondes.

C’est une amélioration majeure par rapport au premier audit.

## Blocages prioritaires

### P0 produit — Audio insuffisant pour LSDJ

Le pipeline actuel fait :

1. MiniGB APU génère du stéréo 16 bits ;
2. l’ESP32 additionne gauche et droite ;
3. conversion en mono 8 bits à 14 kHz ;
4. transfert UART ;
5. répétition des échantillons vers 44,1 kHz sur le Teensy.

Cela entraîne :

- perte totale du panoramique LSDJ ;
- bruit de quantification 8 bits ;
- bande passante limitée ;
- transitoires et samples dégradés ;
- rééchantillonnage sans interpolation ;
- timing APU non fidèle selon l’auteur de Walnut-CGB.

Pour Mario/Tetris, cela peut rester sympathique. Pour LSDJ, non : on entendrait l’ombre du morceau plutôt que le morceau.

**Décision recommandée :**

- remplacer MiniGB APU par une APU cycle-accurate, par exemple le moteur de Blargg/Gb_Snd_Emu, après validation licence et charge CPU ;
- créer un protocole audio V2 ;
- conserver les deux canaux ;
- premier palier réaliste sans recâblage : stéréo 8 bits à 32 kHz ;
- cible haut de gamme : stéréo IMA-ADPCM à 44,1 kHz ou autre compression légère ;
- interpolation linéaire au minimum côté Teensy ;
- mesurer underruns, paquets perdus et remplissage de l’anneau.

Le débit UART brut à 921600 bauds est d’environ 92 ko/s. Le PCM stéréo 16 bits/44,1 kHz demanderait environ 176 ko/s : impossible tel quel. Il faut donc compresser ou choisir un format plus léger.

### P0 qualité — Seulement 30 images visibles par seconde

`gb.direct.frame_skip = true` conserve environ 60 frames logiques mais n’en dessine qu’une sur deux. Le gameplay peut garder sa vitesse si l’ESP32 tient le rythme, mais l’animation de Mario et Tetris n’est pas à 60 Hz.

Pour être « au top », il faut :

- retirer le frame-skip après optimisation ;
- viser 59,7 frames logiques **et affichées** ;
- préparer une image native 160×144 ;
- faire la conversion RGB565 et le scaling par blocs/DMA ;
- synchroniser l’échange de framebuffer avec le panneau RGB ;
- éviter 144 appels de dessin séparés par image.

La tentative antérieure d’envoyer un grand framebuffer PSRAM a scintillé. Cela ne condamne pas le double buffering : cela montre que l’échange n’était pas synchronisé avec le contrôleur RGB.

### P1 performance — Le chemin rapide Walnut-CGB n’est pas utilisé

Le projet appelle :

```cpp
gb_run_frame(&gb);
```

Walnut-CGB indique que `gb_run_frame_dualfetch()` est son chemin rapide recommandé. Le cœur vendored possède déjà les accès ROM 16/32 bits et le DMA 32 bits nécessaires.

**Plan :**

1. ajouter un choix de backend 8-bit/dualfetch ;
2. mesurer le temps de frame sur les cinq ROMs cibles ;
3. conserver une liste de compatibilité ;
4. utiliser dualfetch par défaut si les tests passent ;
5. revenir au moteur classique ROM par ROM si nécessaire.

Ne pas activer aveuglément les optimisations 16 bits signalées comme incompatibles dans le header. Le dualfetch doit être validé avec les ROMs et les tests CPU.

### P1 audio — Le pool Teensy est presque saturé au pire cas

Le nouveau stress test des effets atteint **694 blocs sur 700**. Or l’audio Game Boy utilise aussi `AudioPlayQueue`. Six blocs libres ne constituent pas une réserve sérieuse ; le commentaire parlant de 80 blocs de marge n’est plus vrai.

Résultat possible lorsque huit pistes utilisent leurs délais et qu’un jeu tourne :

- `getBuffer()` retourne `nullptr` ;
- paquets audio ignorés ;
- coupures/glitches Game Boy.

**Solutions possibles :**

- réserver au moins 80–100 blocs au chemin Game Boy ;
- augmenter prudemment `AudioMemory` après mesure de RAM réelle ;
- ou créer un mode console qui réduit/suspend les délais de pistes ;
- afficher un compteur `GB_AUDIO_UNDERRUN`.

### P1 sauvegarde — Autosave synchrone dans la frame

Pour LSDJ, la SRAM peut être grande. Toutes les 30 secondes, `gbRunFrame()` écrit et renomme le fichier directement. Une écriture SD de plusieurs millisecondes peut faire manquer une ou plusieurs frames et couper l’audio.

**Correction d’architecture :**

- marquer la SRAM sale dans `cartRamWrite()` ;
- prendre un snapshot ;
- déléguer l’écriture à une tâche FreeRTOS basse priorité sur l’autre cœur ;
- ne sauvegarder que si des données ont changé ;
- conserver l’écriture atomique et le backup.

### P1 sauvegarde — Buffers de chemin trop courts

`saveRamPath` mesure 64 octets, mais les chemins temporaires utilisent :

```cpp
char tmpPath[40];
char bakPath[40];
```

La liste autorise des noms de ROM jusqu’à 39 caractères. Avec `/games/`, `.sav`, `.tmp` ou `.bak`, le chemin peut dépasser 40 caractères et être tronqué.

Cela peut faire échouer l’autosave ou provoquer une collision de noms.

**Correction recommandée :** mêmes dimensions que `saveRamPath` plus les suffixes, idéalement 72 ou 80 octets, avec contrôle du retour de `snprintf`.

### P1 compatibilité — RTC non persisté

Walnut-CGB émule le RTC MBC3, mais le frontend AZ-2 ne sauvegarde que la RAM cartouche. L’heure et l’état RTC ne sont pas restaurés après redémarrage.

Ce n’est pas bloquant pour LSDJ/Tetris/Mario, mais cela empêchera une compatibilité propre avec Pokémon, Zelda Oracle et d’autres jeux temporels déjà visibles dans la collection.

Il faut un fichier RTC versionné ou un bloc annexe contenant :

- registres RTC ;
- timestamp hôte ;
- état halt/carry ;
- checksum/version.

### P2 compatibilité — MBC2 mal alloué par le frontend

Le cœur précise que MBC2 possède 512 demi-octets même si `num_ram_banks` vaut zéro. AZ-2 calcule uniquement :

```cpp
cartRamSize = gb.num_ram_banks * CRAM_BANK_SIZE;
```

Les sauvegardes MBC2 ne seront donc pas allouées correctement. Ce point n’affecte pas les cinq cibles principales, mais doit être corrigé pour une compatibilité générale.

### P2 robustesse — Fichier .sav partiel accepté

Le chargement affiche le nombre d’octets lus mais ne vérifie pas qu’il correspond à `cartRamSize`. Un fichier tronqué est accepté silencieusement.

Il faut :

- refuser ou réparer un fichier de taille incorrecte ;
- tenter le `.bak` ;
- journaliser clairement la récupération.

### P2 mesure — Le compteur FPS est trop rudimentaire

Le compteur incrémente les appels à `gbRunFrame()`, mais ne mesure pas :

- temps exact de la fenêtre ;
- durée moyenne/maximale d’une frame ;
- deadlines manquées ;
- temps du rendu ;
- temps APU ;
- temps UART ;
- pauses autosave ;
- underruns audio.

Pour qualifier « vitesse normale », ajouter :

- FPS réel en dixièmes ;
- `FRAME_US_AVG`, `FRAME_US_MAX` ;
- `MISSED_FRAMES` ;
- `AUDIO_UNDERRUN` / `AUDIO_OVERFLOW` ;
- `UART_PACKET_ERROR` ;
- histogramme ou min/max sur 60 secondes.

### P2 ergonomie LSDJ

LSDJ utilise beaucoup les combinaisons de touches. Select et Start sur les clics d’encodeur sont fonctionnels mais moins naturels qu’avec de vrais boutons.

Recommandation en mode LSDJ :

- A/B restent A/B ;
- D devient Start ;
- C maintenu devient Select ;
- sortie du jeu par appui long C+D ou par écran ;
- conserver la croix ;
- option de remappage dans le menu.

L’objectif est de pouvoir maintenir Select et utiliser la croix sans gymnastique de poulpe.

### P2 synchronisation musicale

Le port série Game Boy du cœur n’est pas relié. LSDJ peut fonctionner seul, mais ne peut pas encore se synchroniser comme une vraie brique musicale avec le tracker AZ-2.

À terme :

- implémenter les callbacks série de Walnut ;
- pont vers clock interne/MIDI ;
- définir maître/esclave ;
- mesurer la gigue ;
- garder cette fonction après la stabilité de base.

## Architecture cible sans nouveau matériel

```text
Walnut-CGB dualfetch
        │ événements APU horodatés
        ▼
APU précise stéréo
        │ ring buffer ESP32
        ▼
codec léger / paquets V2 + compteurs
        │ UART 921600 full duplex
        ▼
ring buffer Teensy + interpolation
        │ priorité/réserve audio garantie
        ▼
mixeur maître → PCM5102A
```

En parallèle :

```text
LCD 160×144
   ▼
framebuffer natif
   ▼
conversion/scaling par blocs
   ▼
double buffer synchronisé panneau RGB
   ▼
480×432 à 59,7 Hz
```

## Tests obligatoires

### Tests automatiques du cœur

Utiliser au minimum :

- [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite) ;
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) ;
- tests CPU/timer/interrupt/DMA/MBC ;
- test CGB graphique équivalent ;
- tests APU spécialisés si leur redistribution est permise.

Les ROMs de test ne doivent pas forcément être commitées si leur licence ne le permet pas ; la CI peut documenter leur installation.

### Matrice matérielle

| Test | Durée | Critère |
|---|---:|---|
| LSDJ édition + lecture | 2 h | aucun freeze, sauvegarde restaurée, stéréo correcte |
| LSDJ samples/wave | 30 min | pas de note manquante, pitch stable |
| Tetris DMG | 30 min | 59,7 Hz, contrôles sans latence perceptible |
| Super Mario Land | 30 min | scrolling fluide, aucune collision anormale |
| Tetris DX | 30 min | couleurs et sauvegarde correctes |
| SMB Deluxe | 60 min | scènes GBC, double vitesse et sauvegarde |
| jeu + 8 pistes AZ-2 | 30 min | aucun underrun, mémoire sous plafond |
| autosave LSDJ | 100 cycles | aucune frame audio/vidéo perdue visible |
| coupure simulée | 20 cycles | récupération du dernier .sav ou .bak |

### Seuils d’acceptation

- cadence moyenne : 59,72–59,74 Hz ;
- aucune frame logique perdue pendant 30 minutes ;
- 59,72–59,74 images affichées par seconde en mode normal ; le 30 FPS n’est admis que dans un mode « économie » explicitement choisi ;
- aucune erreur UART ;
- aucun underrun audio ;
- sauvegarde restaurée bit à bit ;
- latence bouton → émulation inférieure à une frame ;
- aucune pause visible à l’autosave ;
- audio stéréo, sans souffle et sans dérive de pitch.

## Ordre de travail conseillé

### Palier 1 — Vitesse mesurée

1. instrumenter durée de frame et deadlines ;
2. comparer `gb_run_frame` et `gb_run_frame_dualfetch` ;
3. tester Tetris DMG et Mario Land ;
4. corriger le scheduler avec accumulateur fractionnaire ;
5. conserver un fallback par ROM.

### Palier 2 — Vidéo 60 Hz

1. framebuffer 160×144 ;
2. batch des lignes ;
3. double buffer synchronisé ;
4. désactiver frame-skip ;
5. tester Tetris DX et SMB Deluxe.

### Palier 3 — LSDJ audio

1. remplacer/valider l’APU ;
2. préserver la stéréo ;
3. protocole audio V2 ;
4. interpolation et compteurs ;
5. réserver la mémoire Teensy.

### Palier 4 — Sauvegardes

1. corriger les chemins ;
2. dirty flag ;
3. autosave asynchrone ;
4. fallback `.bak` ;
5. RTC et MBC2.

### Palier 5 — Certification

1. Mooneye + tests graphiques/APU ;
2. matrice cinq ROMs ;
3. rapport de compatibilité ;
4. badge « AZ-2 verified » par ROM/version.

## Décision finale

Il ne faut pas changer de microcontrôleur ni ajouter un ESP pour atteindre la cible demandée. L’ESP32-S3 actuel est assez puissant, mais il faut arrêter de traiter l’émulation comme une fonction secondaire.

Le meilleur chemin est :

1. Walnut-CGB dualfetch et instrumentation ;
2. vidéo 60 Hz sans frame-skip ;
3. APU précise ;
4. audio stéréo compressé sur UART ;
5. sauvegardes asynchrones ;
6. certification LSDJ/Tetris/Mario.

Avec ces travaux, AZ-2 peut devenir plus intéressante qu’une simple console rétro : **une Game Boy musicale intégrée à une groovebox, capable de jouer LSDJ, de le sampler et de le mélanger avec les moteurs internes.**
