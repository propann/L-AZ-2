# AZ-2 — Menu triches/save/load + optimisations Peanut-GB (matin), résultats mesurés

Date : 25 septembre 2026. Fait suite à `docs/AZ2_MESURE_EMULATEUR_GB_2026-09-24.md`
(branche `research/peanut-gb-prototype`), qui concluait sur un gain modeste
(~15%) de Peanut-GB sur Walnut-CGB et listait le pipeline d'affichage comme
piste non encore attaquée.

## 1. Menu réglages/triches/save/load en jeu

Ajouté (encodeurs 0+1 maintenus simultanément pour ouvrir/fermer) : un menu
en jeu avec Continuer / Sauvegarder / Charger / Triches / Quitter, plus un
sous-menu Triches avec un moteur de cheats codé en dur par titre de ROM
(Tetris, Super Mario Land, Zelda) qui écrit directement en WRAM/HRAM/RAM
cartouche à chaque frame quand activé (`gb_emulator.cpp` ET
`gb_emulator_peanut.cpp`, interface identique dans les deux cœurs).

**Bug trouvé et corrigé avant validation matérielle** : le combo d'ouverture
(encodeurs 0+1) déclenche toujours l'action individuelle de l'un des deux
AVANT que le combo ne soit reconnu (impossible de presser pile au même
instant) — sans correction, ouvrir le menu pouvait soit laisser le bouton
Select émulé bloqué "enfoncé" indéfiniment (si l'encodeur 1/Select était
pressé en premier), soit déclencher un enregistrement audio involontaire via
REC:START (si l'encodeur 0/REC était pressé en premier). Corrigé en
neutralisant explicitement Select/Start et en envoyant REC:STOP si besoin
dès que le combo ouvre le menu (`main.cpp`, gestionnaire `ENC:`).

**Navigation revue à la demande utilisateur** (la croix/A/B fonctionnait en
théorie mais jugée peu pratique en usage réel) : l'encodeur 2 (index 1) sert
maintenant à naviguer à la molette (message `TURN:1:direction`, même
mécanisme déjà utilisé pour `padMenuOpen`) et son bouton poussoir valide ;
l'encodeur 3 (index 2) sort du menu. Croix + A/B laissés en place en
secours. Logique de confirmation/sortie factorisée dans
`gbSettingsMenuConfirm()`/`gbSettingsMenuBack()` (réutilisées par les deux
chemins d'entrée).

**Validé sur matériel réel** : menu, triches (activation visible en jeu),
save, load, sortie propre, navigation encodeur — tout confirmé fonctionnel
par test utilisateur direct sur la carte ESP32-S3 (`screen_esp_peanut_gb_lab`).

## 2. Quatre leviers de vitesse, tous prototype-only (`screen_esp_peanut_gb_lab`)

Aucun des changements ci-dessous ne touche `screen_esp_gb_direct`
(production Walnut-CGB) — isolés dans `platformio.ini` sous l'environnement
prototype.

1. **`gb.direct.frame_skip` réactivé** (`gb_emulator_peanut.cpp`) — avait été
   mis à `false` le 24/09 uniquement pour une mesure A/B équitable, pas pour
   une raison de bug. Le cœur/tempo restent corrects à chaque frame, seul
   l'affichage visuel est sauté une frame sur deux.
2. **`-O2` au lieu de `-Os`** — le framework Arduino/ESP-IDF compile en `-Os`
   (taille) par défaut ; flash à moins de 10% d'occupation, largement de la
   marge. Piège trouvé par mesure (`pio run -v`) : PlatformIO place son
   propre `-Os` **après** les `build_flags` du projet dans la ligne de
   commande, donc ajouter `-O2` seul ne suffit pas (GCC prend le dernier
   `-O`) — il faut le retirer explicitement via `build_unflags`.
3. **`-fno-jump-tables` et `-fno-tree-switch-conversion` retirés** — ESP-IDF
   les force par défaut, ce qui transforme le grand `switch(opcode)` à 256
   cas du dispatcheur CPU (`__gb_step_cpu`, exécuté des millions de fois par
   seconde) et les `switch` d'adressage mémoire (`__gb_read`/`__gb_write`)
   en chaînes de comparaisons séquentielles au lieu d'une table de saut O(1).
   Levier le plus net des quatre, purement génération de code, aucun
   changement de comportement.
4. **`PEANUT_GB_HIGH_LCD_ACCURACY=0`** — bascule officielle upstream
   Peanut-GB (pas un hack local), désactive des détails fins de timing PPU
   (priorité sprite/fenêtre en milieu de ligne) contre de la vitesse. Petit
   risque de glitches visuels rares sur certains jeux, aucun observé jusqu'ici.

### Mesures réelles (télémétrie `GB:PERF`, pas l'estimation visuelle)

Progression mesurée sur matériel, ROM Zelda: Link's Awakening, au fil des
quatre leviers ajoutés un par un :

| Étape | fps observé |
|---|---|
| Avant (référence 24/09, frame_skip=false) | ~24 |
| + frame_skip + `-O2` | ~38-40 |
| + jump tables restaurées | ~44 |
| + `PEANUT_GB_HIGH_LCD_ACCURACY=0` | ~48-50 |

Lecture fine par télémétrie série (une seule capture, pas de reconnexions
répétées — voir §4) en régime de jeu réel :

| Champ | Scène légère | Scène chargée |
|---|---|---|
| `fps_x100` | 5976-5976 (**59,76 fps**, quasi natif) | 4678-4965 (~47-50 fps) |
| `core_avg_us` | ~12 600-13 200 | ~16 700-16 900 |
| `display_avg_us` | ~16 400-16 600 | ~16 400-16 600 (**identique**) |
| `missed` | 0 | 6-8 |

**`display_avg_us` (blit_copy_us ~8 700 + blit_flush_us ~7 700) est resté
strictement identique à la mesure du 24/09**, avant tout changement du jour
— aucun des quatre leviers ne l'a touché. C'est maintenant le seul poste
constant, et proportionnellement plus gros en scène légère que le cœur
lui-même. La variance 47-50 vs ~60 fps vient uniquement du coût du cœur
(PPU), qui dépend de la charge réelle de la scène (sprites actifs, fenêtre) —
comportement attendu, pas un bug.

## 3. Deux tentatives ratées sur le flush d'affichage, toutes deux annulées

Piste identifiée le 24/09 (§9 du doc précédent) : `esp_cache_msync()` coûte
~7 700µs/frame en 18 appels (un par bande de 8 lignes source), l'analyse à
froid supposait que le coût FIXE par appel dominait.

- **Tentative 1 — un seul flush en fin de frame** (regrouper les 18 bandes,
  ne flusher qu'une fois à `line==143`) : mesuré **pire** sur matériel réel
  (plus lent ET scintillement visible), pas meilleur. Hypothèse : un flush
  unique et tardif a plus de chances de tomber en pleine lecture DMA
  continue de l'écran par le panneau qu'une série de petits flushs
  fréquents — la fenêtre de "vieilles données visibles" dure alors toute la
  frame au lieu d'être découpée en 18 petits bouts furtifs.
- **Tentative 2 — version modérée, bandes de 16 lignes** (9 flushs/frame au
  lieu de 18, sans aller jusqu'à un flush unique) : toujours scintillant sur
  matériel réel. Confirme que ce pipeline est sensible à la **fréquence** de
  synchronisation avec le scan DMA de l'écran, pas seulement à son coût CPU
  cumulé — réduire le nombre d'appels ne suffit pas, il faut garder des
  flushs fréquents.

Les deux ont été annulées ; le code de `gbBlitLine()` est revenu strictement
à sa version du 24/09 (bandes de 8 lignes, flush par bande). **À ne pas
retenter sous cette forme.** Le vrai levier qui reste pour ce poste est le
PPA (Pixel Processing Accelerator) matériel de l'ESP32-S3 pour la rotation
180°, encore non exploré — chantier plus lourd (nouvelle API matérielle),
pas un réglage ponctuel.

## 4. Leçon opérationnelle : lecture série sur cette carte (CH340)

Ouvrir `/dev/ttyUSB0` (`cat`, `pio device monitor`, pyserial) **redémarre la
carte à chaque fois** — circuit auto-reset du CH340 déclenché par le pulse
DTR/RTS à l'ouverture. `pio device monitor` échoue en plus dans un shell non
interactif (pas de tty). Pour lire la télémétrie sans perturber un test en
cours :

- Une seule ouverture, jamais de reconnexions répétées pendant qu'un test
  utilisateur est en cours (ça a fait croire à un "plantage" plusieurs fois
  ce matin alors que c'était uniquement le reset d'ouverture).
- Fixer le débit AVANT de lire (`stty -F /dev/ttyUSB0 230400 raw -echo`) —
  le port peut se retrouver à un autre débit après un flash (esptool
  reconfigure le port pour son propre usage), sinon on lit du charabia.
- Rediriger en direct vers un fichier (`cat ... > fichier`), PAS via un
  filtre bufferisé type `tr` sans `stdbuf -oL` : `tr` bufferise
  entièrement sa sortie quand elle ne va pas vers un terminal, et un
  `timeout` qui tue le process avant un flush perd toutes les données
  accumulées — plusieurs captures se sont retrouvées vides à cause de ça,
  pas d'un vrai problème matériel.

## 5. État actuel / pas encore fait

- Overlay de performance à l'écran (`kGbPerfOverlay`) réactivé pour ce
  prototype — permet de lire fps/frames manquées sans toucher au port
  série. À reconsidérer si on package ce cœur au-delà du stade prototype
  (légèrement intrusif visuellement, coût négligeable).
- Retrait de Walnut-CGB : **toujours pas fait**, malgré une demande
  explicite ce matin. Les garde-fous posés le 24/09 (§8 du doc précédent —
  LSDJ jamais testé sur Peanut-GB, une seule ROM réellement validée en jeu
  {Zelda}, aucun vrai cycle sauvegarde→coupure→rechargement testé) restent
  non remplis. Reporté, pas refusé — à traiter explicitement avant toute
  suppression de fichiers.
- Cheats actuellement codés en dur pour 3 ROM (Tetris, Super Mario Land,
  Zelda) — pas de mécanisme générique/chargement externe de codes.

## 6. Session du soir : le cœur CPU n'est PAS le goulot, contrairement à l'intuition

Trois pistes explorées, dans l'ordre :

1. **PPA (Pixel Processing Accelerator) matériel de l'ESP32-S3** — semblait la
   solution idéale pour le pipeline d'affichage (rotation/scale en DMA,
   remplacerait `blit_scale_us`+`blit_copy_us`, potentiellement aussi
   `blit_flush_us` puisque ni la source ni la destination ne passeraient plus
   par le cache CPU). **Bloqué** : `driver/ppa.h` est bien présent dans le
   framework Arduino-ESP32 installé (`framework-arduinoespressif32-libs`),
   mais recherche du symbole `ppa_do_scale_rotate_mirror` (`nm`) dans TOUTES
   les bibliothèques précompilées de cette distribution — absent partout.
   Le pilote n'a pas été compilé dans cette distribution ; aucune source
   ESP-IDF correspondante disponible localement pour le compiler nous-mêmes.
2. **Table de correspondance pour le rendu fond/fenêtre** (dépalettisation
   8 pixels d'un coup au lieu d'un décalage de bits par pixel) — écartée à
   l'analyse avant même d'être codée : la conversion palette et l'écriture
   finale se font de toute façon pixel par pixel quel que soit le mode
   d'extraction des bits, donc pas de gain net attendu, pour un vrai risque
   de casser le rendu du défilement (SCX/SCY ne s'aligne pas sur les limites
   de tuiles).
3. **Cache SRAM interne pour la banque ROM 0** (0x0000-0x3FFF, fixe, copiée
   une fois au chargement au lieu d'être lue depuis la PSRAM à chaque fetch
   d'opcode) — codée et testée sur matériel réel : **aucun gain mesurable**.
   Probablement déjà couvert par le cache matériel PSRAM de l'ESP32-S3 pour
   une zone aussi petite et chaude. Annulée (le code est revenu à l'état
   d'avant, `gb_emulator_peanut.cpp`).

### Instrumentation ajoutée : `cpu_only_avg_us`

`core_avg_us` (télémétrie existante) englobe déjà le rendu, puisque
`gbBlitLine()` est appelée de manière synchrone à l'intérieur de
`gb_run_frame()` (via le callback `lcd_draw_line`) — il ne mesurait donc pas
le cœur CPU+PPU isolément. Ajouté un nouveau champ `cpu_only_avg_us`
(`gb_emulator_peanut.cpp`, `gGbDisplayHappenedThisFrame` dans `main.cpp` pour
détecter si un rendu a eu lieu cette frame précise et soustraire son coût).

Mesure réelle sur matériel, même ROM/scènes que précédemment :

| Champ | Plage mesurée |
|---|---|
| `cpu_only_avg_us` (cœur seul, sans le rendu) | **4 000 à 9 100µs** |
| `display_avg_us` (rendu, quand il a lieu) | **16 400-16 600µs, constant** |
| Budget frame | 16 743µs |

**Conclusion inverse de l'intuition de ce soir** : le cœur CPU+PPU, même dans
son pire cas mesuré (9 100µs), reste sous la moitié du budget d'une frame —
ce n'est pas le poste dominant. C'est le pipeline d'affichage qui l'est,
largement (16 400-16 600µs à lui seul dépasse le pire cas du cœur). Les
tentatives d'optimisation du cœur CPU ce soir étaient donc mal ciblées dès le
départ ; le vrai goulot reste exactement celui identifié le 24/09 et déjà
attaqué (sans succès, §3 ci-dessus) ce soir. Sans le PPA (bloqué, point 1),
pas de piste sérieuse identifiée pour aller plus loin sur l'affichage non
plus pour l'instant.

**Instrumentation `cpu_only_avg_us` gardée** dans le code (peu coûteuse, une
comparaison + soustraction par frame) — utile pour ne pas reproduire cette
confusion cœur/affichage à l'avenir.
