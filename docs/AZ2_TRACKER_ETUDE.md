# AZ-2 - Etude tracker (2026-09-15)

Demande : "pour le sequenceur on peut faire un tracker a la LSDJ mais
plus complet genre M8, on fait un tracker 8 pistes" puis "les meilleurs
tracker, on fait une etude sur le sujet, on reunit les meilleures
fonctions/organisation". Sources consultees ci-dessous (recherche web,
pas de resume IA non verifie -- chiffres/structures recoupes entre
plusieurs pages officielles/communautaires par tracker).

## Comparatif structure (les 3 references citees/pertinentes)

| | **LSDJ** (Game Boy) | **Dirtywave M8** | **Polyend Tracker** |
| --- | --- | --- | --- |
| Niveau haut (arrangement) | **Song** : 4 colonnes (1 par canal GB), chacune une liste de **chains** jouees de haut en bas | **Song** : jusqu'a 256 lignes, chaque ligne choisit une **chain** (0-255) par piste (8 pistes) | **Song mode** : jusqu'a 255 "slots", chaque slot = 8 cellules (1 pattern par piste) |
| Niveau moyen (reutilisable) | **Chain** : liste de phrases + transposition optionnelle par phrase, reutilisable entre canaux | **Chain** : jusqu'a 16 **phrases** | *(pas de niveau intermediaire distinct -- le pattern EST l'unite, jusqu'a 128 pas)* |
| Niveau bas (le detail) | **Phrase** : lignes avec Note + Instrument + commandes | **Phrase** : 16 pas | **Pattern** : jusqu'a 128 pas x 8 pistes |
| Colonnes par pas/ligne | Note, Instrument, 2 emplacements de commande (effet+valeur) | **N**ote, **V**olume, **I**nstrument, **FX1/FX2/FX3** (3 effets) | Note, Instrument, **FX1/FX2** (2 effets) |
| Banque d'instruments | Oui -- wave/synth/noise/kit, **partages entre pistes**, pas lies a un canal fixe | Oui -- macro-synth/sample/FM/MIDI, **partages entre pistes** | Oui -- 48 emplacements (sample ou MIDI externe), **partages entre pistes** |
| Reglage de groove/swing | **Groove screen** dediee : motifs de duree de pas reutilisables (groove 0 = defaut), assignable par phrase (commande `G`) | Reglage BPM + swing global | Swing par pattern |
| Controle de flux | `H` (Hop -- saute a une position) | **HOP** (saute a une ligne), **NTH** (skip conditionnel) | Probabilite/Chance par pas (declenchement aleatoire) |
| Nombre d'effets | ~20 commandes (1 lettre + 2 valeurs hexa) | **94 commandes** (mnemonique 3 lettres + valeur) | Liste moyenne : volume, pan, tune, gate length, swing, trigger probability, rolls, slides, randomiseurs, sample reverse/slice/start, filtre, delay/reverb send |

Sources : [lsdj-doc (screens.tex)](https://github.com/jkotlinski/lsdj-doc/blob/master/screens.tex), [lsdj-doc (commands.tex)](https://github.com/jkotlinski/lsdj-doc/blob/master/commands.tex), [Little Sound Dj Manual v9.2.6](https://www.littlesounddj.com/lsd/latest/documentation/LSDj_9_2_6.pdf), [M8 Tracker Tips - Song](https://sites.google.com/view/m8tracker/song), [Dirtywave M8 Operation Manual (ManualsLib)](https://www.manualslib.com/manual/3543344/Dirtywave-M8.html?page=10), [M8Firmware changelog](https://github.com/Dirtywave/M8Firmware/blob/main/changelog.txt), [Polyend Tracker Manual v1.5.0](https://files.kraftmusic.com/media/ownersmanual/Polyend_Tracker_Manual_v1_5.pdf), [Sound on Sound - Polyend Tracker](https://www.soundonsound.com/reviews/polyend-tracker).

## Ce qui revient dans les 3 (le "coeur" a retenir)

1. **Hierarchie a 2-3 niveaux** : un bloc court et reutilisable (chain/
   pattern, 16-128 pas) qu'on enchaine dans un arrangement long (song).
   Notre sequenceur actuel n'a QU'UN pattern de 16 pas en boucle -- pas
   de chainage, pas de "chanson" au sens tracker. C'est la plus grosse
   difference structurelle avec un vrai tracker.
2. **Colonnes par pas : Note + Instrument + 1-3 effets (commande+valeur)**.
   C'est deja la tranche choisie pour la premiere etape (voir echange
   precedent). Confirme par les 3 references -- structure quasi
   identique partout (LSDJ = 2 emplacements d'effet, M8 = 3, Polyend =
   2 ; on part sur **1 effet par pas pour la v1**, extensible).
3. **Banque d'instruments partagee, PAS un moteur fixe par piste**.
   Difference architecturale importante avec AZ-2 actuel : chez nous,
   chaque PISTE a un moteur fixe (`ENGINE:track:moteur`) et un patch
   par piste. Dans un vrai tracker, n'importe quel pas de n'importe
   quelle piste peut jouer n'importe quel instrument de la banque. Two
   options pour nous :
   - **Option A (retenue pour la v1)** : garder le moteur fixe par
     piste (deja code, deja teste), mais laisser la colonne INST
     choisir le **patch** (parmi ceux du moteur de cette piste) pas par
     pas au lieu d'un patch fixe pour toute la piste. Ecart raisonnable
     avec l'esprit tracker, gros gain (melodies avec patch qui change),
     zero risque sur l'architecture moteur deja stable.
   - **Option B (v2 eventuelle)** : vraie banque d'instruments
     partagee inter-pistes (n'importe quel moteur sur n'importe quelle
     piste, pas par pas) -- gros chantier (routage audio dynamique par
     PAS et non plus par piste, la lib Audio Teensy ne fait pas ca
     nativement), a ne pas sous-estimer.
4. **Effets = mnemonique court + valeur**, tous batis sur un
   **decoupage du pas en "ticks"** (sous-unites de temps) : arpege
   (change de note a chaque tick), retrig (redeclenche a intervalle de
   ticks), delay/cut (declenche/coupe apres N ticks). Notre sequenceur
   actuel (`advanceSequencer()`, IntervalTimer) ne declenche qu'aux
   FRONTIERES de pas -- il faut une horloge plus fine (ticks internes
   au pas) pour ces effets, pas juste le pas lui-meme.
5. **Controle de flux avancee (Hop/Nth/Chance)** : puissant mais
   clairement une V2+ -- pas prioritaire tant que le chainage de base
   (item 1) n'existe pas.

## Effets recommandes pour la v1 (realistes sur notre architecture)

En s'appuyant sur ce que LSDJ/M8/Polyend ont TOUS les trois d'une
maniere ou d'une autre, et ce qui est realiste avec un decoupage en
ticks par pas sur l'IntervalTimer deja en place :

| Code AZ-2 | Inspire de | Effet | Valeur |
| --- | --- | --- | --- |
| `ARP` | LSDJ `C`, M8 `ARP` | Arpege : alterne racine -> +x demi-tons -> +y demi-tons a chaque tick du pas | xy (2 demi-tons, 0-15 chacun) |
| `CUT` | LSDJ `K` (kill), Polyend "gate length" | Coupe la note avant la fin naturelle du pas | nombre de ticks avant coupure |
| `RET` | LSDJ `R`, M8 `RET`, Polyend "rolls" | Redeclenche la meme note plusieurs fois dans le pas | intervalle en ticks entre 2 declenchements |

Ecartes pour la v1 (pas urgents, ou demandent plus de travail) :
delay/hop/nth (controle de flux -- attend le chainage), vibrato/pitch
slide (demande une modulation continue, pas juste discrete par tick),
probabilite/chance (facile a ajouter plus tard, pure RNG au moment du
declenchement, independant du reste).

## Recommandation de mise en oeuvre (ordre)

1. **Ticks internes au pas** cote Teensy (`kTicksPerStep`, ex. 4) sur
   la meme `IntervalTimer` -- fondation necessaire pour ARP/CUT/RET.
2. **Colonnes NOTE/INST/FX/VAL par pas**, stockage etendu
   (`stepPatch[]`, `stepFx[]`, `stepFxVal[]` par piste), protocole
   `INST:` et un nouveau prefixe pour les effets de pas (`SFX:` --
   `FX:` deja pris par les effets du bus maitre reverb/delay).
3. **Vue "detail piste"** cote ecran (colonnes NOTE/INST/FX/VAL pour LA
   piste selectionnee, comme l'ecran phrase de LSDJ) en plus de la
   grille 8 pistes existante -- navigable croix/encodeurs.
4. Chainage de patterns (song) -- **repousse a une etape suivante**,
   une fois le detail par pas solide et teste en reel.

## Extension demandee le 2026-09-15 (suite a l'etude) : gammes/accords,
## clavier live, projets

Apres validation du plan ci-dessus, 3 ajouts explicites ("on ajoute les
gammes/accords, que notre clavier tactile serve a editer en live les
sons, qu'on puisse creer facilement un projet, le sauvegarder") :

5. **Gammes/accords** : verrouillage de gamme a la saisie (courant chez
   Polyend/Elektron -- "scale lock") -- une gamme choisie (majeure,
   mineure, pentatonique, etc.) filtre/aligne les notes entrees sur la
   page SEQUENCEUR (croix haut/bas) pour rester dans la tonalite, sans
   empecher la saisie chromatique libre si on veut. Accords : poser
   plusieurs notes sur le meme pas (deja possible via `kNotesPerTrack`
   cote Teensy, 2 notes/piste -- juste pas exploitable depuis l'ecran
   actuellement, colonne NOTE d'un pas a etendre a 2 notes).
6. **Page AUDIO (clavier tactile 16 pads) comme editeur live** : au
   lieu de juste "jouer" une voix live independante, taper un pad
   pendant qu'un pas de sequenceur est selectionne doit pouvoir POSER
   cette note sur le pas (au lieu de/en plus de croix haut/bas pour
   transposer) -- plus rapide et plus musical pour composer que
   incrementer/decrementer un demi-ton a la fois.
7. **Projets : creer/sauvegarder** : serialiser tout l'etat compose
   (BPM/division, moteur+patch par piste, contenu des pas -- note(s)/
   instrument/effet, plus tard le chainage) dans un fichier sur une
   carte SD (celle de l'ESP32, deja en place pour les ROM GB, ou celle
   du Teensy une fois montee pour les samples -- a trancher a
   l'implementation) et pouvoir le recharger. Gros morceau a part,
   plutot une fois le detail par pas (etapes 1-3 plus haut) solide et
   teste, pour ne pas serialiser un format qui va encore bouger.

Ordre retenu : **1-2-3 (fondation tick + colonnes) d'abord**, puis 5-6
(gammes/accords + clavier live, s'appuient directement sur les colonnes
NOTE/INST) une fois la fondation testee, puis 7 (projets) une fois le
format de donnees stabilise. 4 (chainage de patterns) et le reste de
l'etude passent apres.

## Ce qu'on ne copie PAS (hors-sujet pour AZ-2)

- Le systeme de **wave/synth Game Boy natif de LSDJ** (formes d'onde
  4-bit, canaux PU1/PU2/WAVE/NOISE) -- non pertinent, nos moteurs
  (Dexed/EPiano/Braids/Karplus/Analog) sont deja plus riches.
- Le **sequenceur MIDI externe/sample** de Polyend -- pas de sampler
  encore sur AZ-2 (roadmap etape 6, bloque par le materiel).
- Les **94 commandes du M8** -- bien trop pour une v1, 3 effets cibles
  couvrent deja l'essentiel de ce qui rend un tracker "vivant"
  (arpege, coupe, retrig).
