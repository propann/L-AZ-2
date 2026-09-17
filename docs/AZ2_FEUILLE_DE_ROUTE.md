# AZ-2 - Feuille de route

Objectif: transformer AZ-2 en groovebox autonome Teensy + ESP32, avec une base propre avant d'empiler les fonctions.

## Phase 0 - Fondations

Statut: en cours.

| Tache | Statut | Fichier |
| --- | --- | --- |
| Definir roles ESP32 / Teensy | Fait | `README.md` |
| Fixer DAC PCM5102A | Fait | `docs/AZ2_DAC_PCM5102A.md` |
| Definir protocole ESP32/Teensy | Base faite | `lib/AZ2_Protocol/AZ2_Protocol.h` |
| Creer firmware ESP32 propre | Base faite | `src_esp32/az2_control/main.cpp` |
| Creer firmware Teensy propre | Base faite | `src_teensy/az2_audio/main.cpp` |
| Sortir le Pico de la ligne principale | Fait | `platformio.ini` |
| Documenter Wi-Fi / SD / Retro-Go | Base faite | `docs/AZ2_ESP32_CONTROLE_WIFI_SD_RETRO.md` |
| Documenter cablage complet v0 | Base faite | `docs/AZ2_CABLAGE_BASE.md` |
| Benchmark concurrence | Base faite | `docs/AZ2_BENCHMARK_CONCURRENCE.md` |
| Architecture double firmware | Base faite | `docs/AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md` |

## Phase 0.5 - Strategie produit et firmware

Objectif: transformer les recherches concurrentes en decisions techniques.

| Priorite | Tache |
| ---: | --- |
| 1 | Figer la promesse AZ-2: rapide, lisible, ouvert, reparable |
| 2 | Garder ESP32 pour humain/ecran/SD/Wi-Fi/pads |
| 3 | Garder Teensy pour audio/clock/sequenceur critique |
| 4 | Mesurer la latence pad -> son sur table |
| 5 | Ajouter heartbeat et etats d'erreur dans le protocole |
| 6 | Prevoir protocole binaire v1 seulement apres v0 jouable |

Critere de sortie: une pression pad declenche un son, une LED, et un retour etat ecran sans ambiguite.

## Phase 1 - Cablage minimal qui fait du son

Objectif: obtenir un son test stable depuis le Teensy vers le PCM5102A.

| Priorite | Tache |
| ---: | --- |
| 1 | Cabler PCM5102A: 3.3 V, GND, BCLK 21, LRCLK 20, DIN 7 |
| 2 | Flasher `master_teensy` |
| 3 | Envoyer `PLAY` / `STOP` en serie |
| 4 | Verifier sortie ligne avec volume bas |
| 5 | Ajouter mute `XSMT` si le module expose la broche |

Critere de sortie: le Teensy produit un son propre sur le PCM5102A sans ESP32.

## Phase 2 - Dialogue ESP32 vers Teensy

Objectif: faire parler les deux cerveaux.

| Priorite | Tache |
| ---: | --- |
| 1 | Choisir deux GPIO UART libres sur ESP32 |
| 2 | Choisir RX/TX sur Teensy |
| 3 | Relier TX/RX croises + GND commun |
| 4 | Envoyer `HELLO:ESP32_CONTROL` |
| 5 | Recevoir `HELLO:TEENSY_AUDIO` |
| 6 | Envoyer `PAD:00:DOWN:vel=110` et declencher un son |

Critere de sortie: un message ESP32 declenche un son Teensy.

## Phase 3 - Matrice SparkFun 4x4

Objectif: transformer les 16 pads en surface de jeu.

| Priorite | Tache |
| ---: | --- |
| 1 | Identifier la reference exacte des multiplexeurs |
| 2 | Tester la matrice en rouge monochrome |
| 3 | Scanner les 16 boutons avec anti-rebond |
| 4 | Envoyer les `PAD` au Teensy |
| 5 | Piloter les LEDs de retour |
| 6 | Passer au RGB si le driver de courant est propre |

Critere de sortie: chaque pad allume sa LED et declenche un son.

## Phase 4 - Ecran ESP32 480x480

Objectif: obtenir une UI AZ-2 lisible sur l'ecran carre.

| Priorite | Tache |
| ---: | --- |
| 1 | Valider driver ST7701 sur ESP32-4848S040C_I |
| 2 | Afficher page boot AZ-2 |
| 3 | Afficher grille 4x4 |
| 4 | Afficher etat Teensy: ready, playing, BPM |
| 5 | Creer pages Home, Performance, Mixer, Sequencer |
| 6 | Ajouter mode test hardware |

Critere de sortie: l'ecran montre l'etat pads/Teensy en temps reel.

## Phase 5 - SD et Wi-Fi

Objectif: utiliser l'ESP32 comme tete moderne sans casser la stabilite musicale.

| Priorite | Tache |
| ---: | --- |
| 1 | Trouver le pinout TF/microSD exact du module ecran |
| 2 | Monter `/az2` au boot si SD presente |
| 3 | Stocker `hardware.json`, `ui.json`, `wifi.json` |
| 4 | Ajouter mode Wi-Fi AP local |
| 5 | Ajouter file manager web minimal |
| 6 | Ajouter export logs / captures |

Critere de sortie: on peut configurer et transferer des fichiers sans ouvrir la machine.

## Phase 6 - Portage MicroDexed-touch

Objectif: recuperer seulement ce qui sert au moteur son.

| Priorite | Tache |
| ---: | --- |
| 1 | Compiler une base MicroDexed-touch en `I2S_AUDIO_ONLY` |
| 2 | Isoler audio graph, synth, mixer, sequencer |
| 3 | Retirer UI ILI9341/touch du chemin principal |
| 4 | Brancher commandes AZ-2 sur moteur MicroDexed |
| 5 | Ajouter presets/banques |
| 6 | Ajouter samples quand la base est stable |

Critere de sortie: AZ-2 joue un moteur MicroDexed pilote par l'ESP32.

## Phase 7 - Retro-Go / Game Boy bonus

Objectif: ajouter le fun sans contaminer la groovebox.

| Priorite | Tache |
| ---: | --- |
| 1 | Garder Retro-Go comme reference dans `src_esp32/retro-go-master` |
| 2 | Etudier target ESP32-S3 proche |
| 3 | Creer target `az2-esp32-4848s040` si utile |
| 4 | Adapter driver ecran ST7701 |
| 5 | Mapper matrice 4x4 vers commandes Game Boy |
| 6 | Utiliser SD: `/roms/gb`, `/roms/gbc`, `/retro-go` |

Critere de sortie: mode Game Boy separable, jamais prioritaire sur l'instrument.

## Regle de projet

- Un lot = un test reel.
- Pas de gros import aveugle dans la compilation principale.
- Tout ce qui touche au son doit rester testable sans UI.
- Tout ce qui touche a l'UI doit rester testable sans moteur audio complet.
- Les docs suivent le cablage reel au fur et a mesure.

## Mise a jour 2026-09-16 -- ou en est reellement AZ-2

Les phases 0-7 ci-dessus datent du tout debut du projet (matrice
SparkFun 4x4, Pico controleur, portage MicroDexed encore a faire,
Retro-Go). **Tout ca a change en pratique** (matrice+Pico abandonnes le
2026-09-14, portage MicroDexed fait autrement -- moteur Dexed/EPiano/
Braids/Karplus/Analog directement dans `src_teensy/az2_audio/`, Retro-Go
ecarte au profit de Walnut-CGB). Statut reel condense :

| Phase historique | Statut reel aujourd'hui |
| --- | --- |
| 0-2 (fondations, cablage minimal, dialogue ESP32<->Teensy) | **Fait**, mais sur une architecture differente (croix+4 boutons+3 encodeurs directs sur Teensy, pas de matrice/Pico) |
| 3 (matrice SparkFun 4x4) | **Abandonnee** -- voir AZ2_CABLAGE_PICO.md, remplacee par croix+boutons+encodeurs |
| 4 (ecran ESP32 480x480) | **Fait et tres etendu** -- ecran reel VIEWE UEDX48480040E-WB (pas ESP32-4848S040C_I suppose au debut), menu 4 categories, tracker, page PATCH+oscilloscope, page SONG, emulateur GB |
| 5 (SD et Wi-Fi) | **SD fait** (roms GB + patches). **Wi-Fi jamais commence** (pas prioritaire, personne ne l'a redemande depuis) |
| 6 (portage MicroDexed-touch) | **Fait autrement** -- moteur Dexed integre directement (pas de portage "boite noire"), + 4 autres moteurs ajoutes en plus (pas prevu au depart) |
| 7 (Retro-Go / Game Boy) | **Fait, mais avec Walnut-CGB pas Retro-Go** (Retro-Go etudie et ecarte le 2026-09-14, ESP-IDF natif incompatible avec notre ecran RGB parallele) -- emulateur GB/GBC fonctionnel, son du jeu route vers le DAC Teensy |

Voir [AZ2_ETAT_DES_LIEUX.md](AZ2_ETAT_DES_LIEUX.md) pour le detail
verifie-en-reel vs seulement-compile, et
[AZ2_BENCHMARK_CONCURRENCE.md](AZ2_BENCHMARK_CONCURRENCE.md) pour la
liste d'ameliorations indispensables priorisee (mute/solo, sauvegarde
de projet complet, swing, volume/pan par piste, sampler, MIDI, accords,
clavier live comme editeur).

## Phase 8 (nouvelle) -- combler les trous face au marche

Reprend la liste priorisee du benchmark concurrence, dans le meme
ordre :

| Priorite | Tache | Bloque par |
| ---: | --- | --- |
| 1 | Mute/solo par piste | **[FAIT le 2026-09-17]** -- voir section dediee plus bas (le piege Braids trouve le 2026-09-16 est resolu, mute/demute immediat meme sur Braids) |
| 2 | Sauvegarde/chargement de projet complet (patterns+song+BPM+scale) | **[FAIT le 2026-09-17]** page PROJET, 4 emplacements, `/projects/N.proj` sur la SD de l'ESP32 -- voir section dediee plus bas |
| 3 | Swing/groove global | Piege timer trouve le 2026-09-16, voir note plus bas |
| 4 | Volume par piste | **[FAIT le 2026-09-17]**, voir section dediee plus bas. **Pan : PAS FAIT** -- chaine mono de bout en bout (patchOutL/patchOutR dupliquent le meme mixMaster), un vrai pan demanderait de refaire les bus en stereo |
| 5 | Accords (plusieurs notes par pas depuis l'ecran) | Rien cote Teensy (`kNotesPerTrack=2` deja la) -- juste l'UI colonne NOTE a etendre |
| 6 | Clavier tactile comme editeur live de note | **[FAIT le 2026-09-17]** -- voir section dediee plus bas |
| 7 | Sampler (moteur audio a partir d'echantillons) | **Capture (phase 1) faite le 2026-09-17**, voir section dediee plus bas. Reste : verifier en reel (carte SD Teensy inseree ?), puis vue d'onde/decoupage/nommage (phase 2), puis le moteur de LECTURE `AudioPlaySdWav`/`AudioPlaySdRaw` (phase 3, pas commence) |
| 8 | MIDI notes IN | **[FAIT le 2026-09-17]** -- voir section dediee plus bas. Sync horloge/MIDI OUT/routage vers une piste du sequenceur : pas fait |
| 9 | Wi-Fi (config web, transfert fichiers) | Rien de bloquant, jamais redemande depuis la Phase 5 -- rester bas dans la pile tant que le musical n'est pas complet |

Critere de sortie de cette phase : AZ-2 n'a plus aucun "trou" flagrant
par rapport a une groovebox d'entree de gamme (mute/solo + sauvegarde +
swing sont les 3 attendus partout, meme sur les machines les moins
cheres etudiees).

### Piege trouve en preparant le mute/solo (2026-09-16, pas implemente)

En regardant `trackNoteOn()`/`trackNoteOff()` pour brancher un mute
simple ("juste mettre le gain du groupe a 0"), une decouverte : pour le
moteur **BRAIDS**, le gain du mixeur de groupe (`trackGroupMixer(track)
.gain(trackGroupChannel(track), ...)`) sert DEJA de porte note-on/note-
off (Braids est un oscillateur qui tourne en continu, sans enveloppe
propre -- voir `kBraidsActiveGain` a l'allumage, `0.0f` a l'extinction,
dans `trackNoteOn()`/`trackNoteOff()`). Ecrire un mute qui met
brutalement ce meme gain a 0 volerait la place de cette porte : a la
prochaine note, `trackNoteOn()` remettrait `kBraidsActiveGain` sans
tenir compte du mute, et demuter une piste Braids en cours de note ne
la ferait pas revenir (le code ne re-applique le gain qu'aux
transitions note-on/note-off, jamais en dehors).

Facon correcte de le faire (a ecrire, PAS fait) :

1. Un tableau `bool trackMuted[kTrackCount]` + `bool trackSoloed[kTrackCount]`.
2. Une fonction `float effectiveGain(track)` = 0 si mute ou (un solo actif
   ET cette piste pas soloed), sinon le gain "normal" du moteur actif
   (0.0f pour Braids au repos, 0.5f pour les autres -- meme valeurs que
   `setTrackEngine()` aujourd'hui).
3. `trackNoteOn()` (cas Braids) doit utiliser `kBraidsActiveGain *
   (effectiveGain(track) > 0 ? 1 : 0)` au lieu du gain fixe -- pas
   `kBraidsActiveGain` tout court.
4. Un changement de mute/solo (`MUTE:`/`SOLO:`) doit **recalculer et
   reappliquer le gain du groupe pour la piste concernee** -- mais SEULEMENT
   si son moteur n'est pas Braids EN TRAIN DE JOUER une note (sinon on
   ecraserait la porte au mauvais moment). Le plus sur : garder aussi un
   `bool trackNoteHeld[kTrackCount]` mis a jour par trackNoteOn()/Off(),
   et ne toucher au gain depuis MUTE:/SOLO: QUE si `!trackNoteHeld[track]`
   pour une piste Braids (sinon laisser la prochaine transition note-on/
   off appliquer le bon gain).
5. Tester en reel avec une piste Braids qui joue en boucle pendant qu'on
   mute/demute plusieurs fois de suite -- c'est le cas qui casserait
   silencieusement sans le point 4.

Rien de tout ca n'a ete ecrit aujourd'hui (pas de materiel branche pour
verifier un changement sur le chemin audio) -- juste le piege identifie
pour ne pas le decouvrir en prod la prochaine fois.

### Sampler GB -- capture phase 1 (2026-09-17)

Demande : "il faut un bouton, un des encodeurs celui qui est libre,
lance un [enregistrement] avec un bouton REC/STOP ... il nous faut une
routine pour capter les sons de l'emulateur, sans la SD du Teensy [sur
la SD du Teensy]".

Fait, compile verifie, **PAS ENCORE teste en reel** (ni Teensy ni ESP32
branches au moment d'ecrire ce code) :

- **Teensy** (`gbRecStart()`/`gbRecPush()`/`gbRecStop()`/
  `writeWavHeader()`, pres de `handleGbAudioPacket()`) : capture le son
  GB tel qu'il arrive (8kHz mono, AVANT le sur-echantillonnage vers
  44.1kHz fait pour la lecture -- fichier plus petit, fidele a la
  vraie qualite source), ecrit un `.wav` PCM 16 bits standard dans
  `/samples/SAMPLE_NNN.wav` sur la carte SD DEDIEE du Teensy
  (`BUILTIN_SDCARD`, PAS celle de l'ESP32 qui garde les ROM/patches).
  Nouvelles commandes protocole `REC:START`/`REC:STOP`
  (`handleRecCommand()`), echo `REC:STARTED:<fichier>` /
  `REC:STOPPED:samples=<n>` / `REC:ERROR:<fichier>`. Garde-fou : arret
  automatique a 30s (`kGbRecMaxSamples`) si jamais on oublie STOP.
- **ESP32** : encodeur 0 (Volume, bouton integre -- libre jusqu'ici,
  reserve pour ca depuis le 2026-09-15) bascule REC/STOP en jeu (page
  JEUX, ROM chargee). Indicateur "REC" rouge discret en haut a gauche
  pendant l'ecran de jeu, mis a jour uniquement par l'echo confirme du
  Teensy (meme prudence que FILT:/ENV: -- le Teensy peut aussi arreter
  tout seul via le garde-fou 30s, l'ESP32 doit le refleter).

**Explicitement PAS fait (phase 2/3, plus gros, demande de la vue en
reel pour bien faire)** :

- Vue d'onde EN DIRECT pendant l'enregistrement ("un truc pour couper
  l'onde comme on veut en tactile"). Piste retenue : reutiliser
  l'infrastructure SCOPE deja construite et verifiee pour la page
  PATCH (meme decimation/paquet binaire), juste une nouvelle source
  (le flux GB au lieu d'une piste) et un nouvel affichage (page JEUX au
  lieu de PATCH).
- Decoupage tactile du debut/fin de l'onde (glisser des marqueurs sur
  le tracer).
- Decoupage automatique (detection de silence, a definir : seuil
  d'amplitude ? duree minimale de silence ?).
- Clavier tactile a l'ecran pour nommer le sample (actuellement
  `SAMPLE_001.wav`, `SAMPLE_002.wav`... auto-incremente, pas de nom
  personnalise).
- Le moteur de LECTURE du sample dans le sequenceur (item 7 du tableau
  plus haut) -- capturer un sample et le JOUER comme un 6e moteur sont
  deux chantiers distincts, celui-ci ne fait que la capture.

Prochaine etape utile : verifier `SDTEENSY:READY` au boot (carte SD
Teensy inseree ?), puis tester REC/START en jouant a un jeu, confirmer
le fichier `.wav` cree/lisible (taille coherente avec la duree, joue
correctement dans un lecteur audio classique sur ordinateur).

**[2026-09-17, confirme]** carte SD Teensy testee directement en serie
(`REC:START`/`REC:STOP`) -- presente et fonctionnelle, fichier `.wav`
cree avec succes. Reste a verifier le contenu audio avec un vrai jeu
qui joue du son pendant l'enregistrement.

### MIDI notes IN (2026-09-17)

Priorite #6, MVP volontairement modeste -- `USB_MIDI_SERIAL` etait deja
dans `platformio.ini` depuis le debut du projet (jamais exploite avant
aujourd'hui) : c'est le mode USB "Serial + MIDI" du Teensy, l'objet
global `usbMIDI` est fourni automatiquement par le core des que ce mode
est choisi, rien a cabler physiquement (MIDI arrive par le meme cable
USB que le firmware/le moniteur serie).

- `updateMidiIn()` (boucle `usbMIDI.read()`) route les messages
  NoteOn/NoteOff MIDI (n'importe quel canal) vers `liveVoice` -- EXACT
  MEME chemin que les pads tactiles de l'ecran (`liveVoice.keydown()/
  keyup()`), donc zero risque nouveau sur le graphe audio.
- NoteOn avec velocite 0 traite comme NoteOff (convention MIDI standard
  utilisee par beaucoup de controleurs).
- Explicitement PAS fait : synchro d'horloge MIDI (Start/Stop/Clock),
  MIDI OUT, routage vers une piste du sequenceur au lieu de la voix
  live (les pistes ont un moteur fixe chacune -- router une note MIDI
  vers UNE piste precise demanderait de choisir laquelle, pas encore
  concu).

Compile verifie (master_teensy), **PAS ENCORE flashe ni teste en
reel** -- ecrit sans materiel branche, et surtout **jamais teste avec
un vrai controleur MIDI branche** (aucun disponible pendant l'ecriture).

### Mute/solo par piste (2026-09-17) -- resolution du piege Braids

Priorite #1, LE piege le plus serieux trouve le 2026-09-16 -- resolu
proprement, mieux que prevu a l'origine :

- Idee cle : `trackNoteHeld[]` suit si une note Braids est REELLEMENT
  en train de sonner sur chaque piste (mis a jour dans
  `trackNoteOn()`/`trackNoteOff()`). Avec cette info, une fonction
  unique `applyGroupGainNow(track)` sait calculer le bon gain de groupe
  dans TOUS les cas (Braids tenu, Braids au repos, autre moteur) et
  peut etre appelee a N'IMPORTE QUEL moment sans jamais fausser la
  porte note-on/off -- pas besoin de "differer" quoi que ce soit.
  Resultat : **mute/demute est immediat, meme sur une note Braids deja
  tenue** -- meilleur que la premiere approche (volume seul, la veille)
  qui acceptait un decalage sur ce moteur avant que ce mecanisme plus
  general soit trouve.
- `trackEffectiveGain(track)` combine volume + mute + solo en un seul
  multiplicateur : mute gagne toujours ; si au moins une piste est
  soloed, seules les soloed sont audibles ; sinon volume normal.
- Commandes `MUTE:<piste>:<0|1>` et `SOLO:<piste>:<0|1>` (Teensy). Un
  changement de SOLO recalcule le gain de TOUTES les pistes (une piste
  peut devenir muette parce qu'une AUTRE vient d'etre soloed) ; un
  changement de MUTE ne recalcule que la piste concernee (mute
  n'affecte jamais l'audibilite des autres pistes).
- ESP32 : page MOTEURS, boutons **C** (mute) / **D** (solo) pour la
  piste choisie par la croix -- indicateurs "M" rouge / "S" jaune sur
  la ligne. Reutilise C/D (libres, meme famille que le C de sortie GB
  et le D d'edition de pas -- chacun scope a son propre ecran, aucune
  collision).
- Mute inclus dans la sauvegarde de projet (13e champ des lignes
  TRACK:, retro-compatible). **Solo volontairement PAS sauvegarde**
  (convention habituelle DAW/mixeurs : le solo est un outil de
  monitoring live, pas une decision de composition).
- Ancien code de `handleVolCommand()` simplifie au passage (appelle
  maintenant `applyGroupGainNow()` au lieu de dupliquer sa propre
  logique de calcul de gain).

Compile verifie (3 environnements), **PAS ENCORE flashe ni teste en
reel** -- ecrit sans materiel branche. A tester en priorite : muter/
demuter une piste BRAIDS pendant qu'elle joue une note tenue (le cas
que le piege d'origine aurait casse).

### Volume par piste (2026-09-17)

Priorite #4, implementee EN RESPECTANT le piege trouve plus haut (le
gain du mixeur de groupe sert de porte note-on/off pour BRAIDS) :

- `VOL:<piste>:<0-127>` (Teensy, `handleVolCommand()`), `trackVolume[]`
  par piste (defaut 127 = plein volume, comportement d'origine).
- Pour les moteurs AUTRES que Braids : le gain de groupe est statique
  hors note-on/off, donc `handleVolCommand()` le recalcule et le
  reapplique immediatement (`0.5f * volume/127`).
- Pour Braids : `handleVolCommand()` ne touche PAS le gain -- le
  nouveau volume n'est applique qu'a la PROCHAINE note jouee
  (`trackNoteOn()`, `kBraidsActiveGain * volume/127`). Limite connue et
  acceptee : si une note Braids est deja en train de sonner (tenue),
  changer le volume ne s'entend qu'a la prochaine transition note-on/
  off, pas immediatement. Pas un bug -- documente ici pour ne pas le
  redecouvrir en pensant que c'est casse.
- Page PATCH : nouvelle ligne VOLUME (7e ligne, meme style +/- que les
  6 au-dessus mais VOLONTAIREMENT hors du systeme patchParamRef() --
  le volume ne depend pas du moteur, pas la peine de le meler a cette
  logique conditionnelle). Page redessinee pour laisser la place
  (verifie tenir sur les 480px de haut, calcul fait a la main -- PAS vu
  a l'oeil, aucun ecran branche en ecrivant ce code).
- Ajoute aussi au format de sauvegarde de PROJET (12e champ des lignes
  TRACK:, retro-compatible avec les fichiers a 11 champs sauvegardes
  avant cet ajout).

Compile verifie (master_teensy + screen_esp), **PAS ENCORE flashe ni
teste en reel** -- ecrit sans materiel branche, y compris la mise en
page de la ligne VOLUME (calcul de coordonnees seulement, jamais vue
sur l'ecran reel).

### Clavier tactile comme editeur live (2026-09-17)

Priorite #8, etape 6 de AZ2_TRACKER_ETUDE.md ("taper un pad pendant
qu'un pas de sequenceur est selectionne doit pouvoir poser cette note
sur le pas"). Purement ESP32, aucun changement Teensy.

- Probleme trouve en l'ecrivant : `selectedSeqTrack`/`selectedSeqStep`
  valent TOUJOURS quelque chose depuis la refonte du tracker (plus
  jamais -1) -- impossible de deviner "un pas est selectionne" a partir
  de leur seule valeur, contrairement a ce que la formulation d'origine
  laissait supposer.
- Solution retenue : bouton **D** (libre -- ni la manette GB ni aucune
  page ne l'utilisait) bascule explicitement la page AUDIO entre "jouer
  en direct" et "poser sur piste/pas" (indique dans le titre de la
  page). Evite d'ecraser une composition par accident en jouant
  simplement sur les pads.
- Taper un pad en mode "poser" allume le pas (STEP: ON) ET y ecrit la
  note du pad (NOTE:) -- sans l'allumer, la note posee ne s'entendrait
  jamais en lecture.
- Petit refactor au passage : `kPadBaseNote` (48, note du pad 0) etait
  duplique en dur cote Teensy seulement -- deplace dans AZ2_Protocol.h
  (partage), l'ESP32 en a maintenant besoin pour calculer la note a
  ecrire sans repasser par le Teensy.

Compile verifie (3 environnements), **PAS ENCORE flashe ni teste en
reel**.

### Sauvegarde de projet complet (2026-09-17)

Priorite #2 de la liste indispensable, implementee -- pure gestion de
donnees ESP32, **aucun changement cote Teensy** (toutes les commandes
protocole utilisees existaient deja : STEP:/NOTE:/INST:/SFX:/ENGINE:/
PATCH:/FILT:/ENV:/DXP:/BPM:/DIV:/PATTERN:/SONGSET:/SONGLEN:/SONGMODE:).

- Nouvelle page `Screen::Project` (menu MUSIQUE) : selecteur
  d'emplacement (4) + SAVE/LOAD, meme motif que la page PATCH.
- `saveProject()` ecrit `/projects/N.proj` sur la SD de l'ESP32 (fichier
  texte simple, lisible a l'oeil) : BPM, division, gamme, song
  (mode/longueur/chainage), moteur+patch+filtre+ADSR+algo/feedback des
  8 pistes, ET les 8×8×16 = 1024 pas (note/etat/patch/effet/valeur) des
  8 patterns -- tout, pas seulement ce qui est actif.
  `loadProject()` relit le fichier et renvoie chaque commande normale
  au Teensy (meme principe que `loadPatchSlot()`, a plus grande
  echelle) tout en mettant a jour les tableaux locaux de l'ESP32.
- Bug trouve et corrige AVANT le premier flash (relecture du code) :
  `lastPattern` etait declare `static` dans la boucle de lecture --
  aurait garde sa valeur d'un chargement a l'autre et pu sauter le
  changement de pattern necessaire au tout debut d'un second
  chargement. Corrige en variable locale normale, reinitialisee a
  chaque appel de `loadProject()`.
- Compile verifie (master_teensy + screen_esp), **PAS ENCORE flashe ni
  teste en reel** -- ecrit sans materiel branche. A tester : sauvegarder
  un petit morceau, modifier des pas, charger le meme emplacement,
  verifier que tout revient exactement comme avant (patterns, song,
  tempo, sons).

**Le meme piege s'applique au volume/pan par piste (item 4)** si
implemente en multipliant le meme gain de groupe -- meme prudence
requise (verifier avec une piste Braids qui joue en continu pendant
qu'on bouge le volume).

**Swing/groove (item 3) a aussi un piege different, trouve en y
regardant** : le tempo est pilote par un seul `IntervalTimer` a periode
FIXE (`tickIntervalUs()`, recalculee seulement quand BPM/division
changent). Un vrai swing doit alterner 2 durees de tick (pas
long/court) -- ca veut dire rappeler `.update()` sur le timer DEPUIS
L'ISR elle-meme selon la parite du pas courant, ce qui est plus delicat
a rendre fiable (jitter, sécurité de reconfigurer un timer depuis sa
propre interruption) qu'un simple "decalage" comme note initialement.
A verifier en reel avec un oscilloscope/analyseur logique sur l'horloge
avant de considerer ca "fait", pas juste a l'oreille.
