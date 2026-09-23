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

**Abandonnee le 2026-09-14** -- ce plan de matrice+multiplexeurs SparkFun
n'a jamais ete construit tel quel et ne le sera pas: remplace par
croix+4 boutons+3 encodeurs directs sur Teensy. Section gardee ci-dessous
pour l'historique seulement (voir le detail dans la mise a jour plus bas
et dans `AZ2_CABLAGE_PICO.md`); ne pas la lire comme un plan actif.

Objectif (a l'epoque): transformer les 16 pads en surface de jeu.

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
| 7 (Retro-Go / Game Boy) | **Non fonctionnel à ce jour.** Walnut-CGB et GNUBOY sont présents comme prototypes, mais aucun chemin complet ROM + vidéo + commandes + audio + sauvegarde n'est validé sur la machine. |

Voir [AZ2_ETAT_DES_LIEUX.md](AZ2_ETAT_DES_LIEUX.md) pour le detail
verifie-en-reel vs seulement-compile, et
[AZ2_BENCHMARK_CONCURRENCE.md](AZ2_BENCHMARK_CONCURRENCE.md) pour la
liste d'ameliorations indispensables priorisee (mute/solo, sauvegarde
de projet complet, swing, volume/pan par piste, sampler, MIDI, accords,
clavier live comme editeur).

## Phase 8 (nouvelle) -- combler les trous face au marche

Reprend la liste priorisee du benchmark concurrence, **meme
numerotation que AZ2_BENCHMARK_CONCURRENCE.md** (corrige le 2026-09-17,
les deux tableaux avaient derive avec des numeros differents pour les
memes items) :

| # | Tache | Etat |
| ---: | --- | --- |
| 1 | Mute/solo par piste | **[FAIT le 2026-09-17]** -- voir section dediee plus bas (le piege Braids trouve le 2026-09-16 est resolu, mute/demute immediat meme sur Braids) |
| 2 | Sauvegarde/chargement de projet complet (patterns+song+BPM+scale) | **[FAIT le 2026-09-17]** page PROJET, 4 emplacements, `/projects/N.proj` sur la SD de l'ESP32 -- voir section dediee plus bas |
| 3 | Swing/groove global | **[FAIT le 2026-09-17]** -- voir section dediee plus bas. Piege timer du 2026-09-16 EVITE (jamais de `sequencerTimer.update()` pour le swing) |
| 4 | Volume par piste | **[FAIT le 2026-09-17]**, voir section dediee plus bas. **Pan : PAS FAIT** -- chaine mono de bout en bout (patchOutL/patchOutR dupliquent le meme mixMaster), un vrai pan demanderait de refaire les bus en stereo |
| 5 | Sampler (moteur audio a partir d'echantillons) | **Capture (phase 1) FAITE et confirmee en reel** (carte SD Teensy presente/fonctionnelle, teste par serie le 2026-09-17). Reste : vue d'onde/decoupage/nommage (phase 2), puis le moteur de LECTURE `AudioPlaySdWav`/`AudioPlaySdRaw` (phase 3, pas commence) |
| 6 | MIDI notes IN | **[FAIT le 2026-09-17]** -- voir section dediee plus bas. Sync horloge/MIDI OUT/routage vers une piste du sequenceur : pas fait |
| 7 | Accords (plusieurs notes par pas depuis l'ecran) | **PAS FAIT, plus gros que prevu** -- `kNotesPerTrack` est la polyphonie interne du moteur, pas une 2e note par pas dans les donnees du sequenceur. Touche le meme code que le bug de gel deja rencontre cette session -- voir la correction 2026-09-16 dans AZ2_BENCHMARK_CONCURRENCE.md |
| 8 | Clavier tactile comme editeur live de note | **[FAIT le 2026-09-17]** -- voir section dediee plus bas |
| 9 | Wi-Fi (config web, transfert fichiers) | **PAS FAIT**, pas dans le classement du benchmark -- rien de bloquant, jamais redemande depuis la Phase 5, reste bas dans la pile tant que le musical n'est pas complet |

Etat au 2026-09-17 (fin de journee) : **6/8 faits** (#1, #2, #3, #4, #6, #8), tous
compiles mais **PAS ENCORE verifies en reel** (aucun board branche ce
jour-la). Restent #3 (swing), #5 phase 2/3 (sampler), #7 (accords) --
les 3 demandent le materiel sous la main (pieges timer/donnees trouves
a l'avance, ou design d'ecran a faire les yeux dessus).

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

**Swing/groove (item 3) -- piege evite, FAIT le 2026-09-17.** Le piege
identifie le 2026-09-16 (reconfigurer `sequencerTimer` DEPUIS sa propre
ISR pour alterner 2 durees de tick) a ete contourne completement plutot
que resolu au prix du risque initial :

- `sequencerTimer` garde sa periode FIXE **pour toujours** -- jamais un
  seul appel a `.update()` lie au swing. L'ISR (`advanceTick()`) tourne
  exactement a la meme cadence qu'avant.
- A la place, seul le **seuil logiciel de fin de pas** varie :
  `ticksForCurrentStep` (nouvelle variable, recalculee UNE fois au
  debut de chaque pas) vaut `kTicksPerStep - swingAmount` pour les pas
  pairs et `kTicksPerStep + swingAmount` pour les pas impairs -- classique
  "shuffle" de boite a rythme (delai des contretemps), tempo moyen
  exact sur toute paire de pas puisque le total de ticks sur 2 pas ne
  change jamais.
- `SWING:<0-127>` (convention UI) mappe en interne sur 0 a
  `kTicksPerStep-1` (3 max, seule resolution utile vu le decoupage a 4
  ticks/pas) -- `handleSwingCommand()`.
- ESP32 : nouvelle ligne SWING sur la page CONFIGURATION (memes +/-
  que l'ecran de veille/la gamme juste au-dessus), pas de 32 (127/4)
  pour que chaque appui change reellement quelque chose. Inclus dans
  la sauvegarde de projet.

Compile verifie (3 environnements), **PAS ENCORE flashe ni teste en
reel** -- ecrit sans materiel branche. A verifier en priorite : le
tempo moyen reste-t-il exact sur plusieurs mesures (pas de derive), et
la sensation de swing est-elle audible/musicale aux positions
extremes (32, 64, 96, 127) ?

## Étude reportée à AZ-3 — rack de moteurs interchangeables

**Décision matérielle du 2026-09-18 : hors périmètre AZ-2.** Les photos du prototype montrent que le boîtier est arrivé à sa capacité pratique : écran, Teensy, ESP32-S3, PCM5102A, haut-parleur, commandes et faisceau occupent déjà l’espace utile. Aucun ESP8266/ESP32 moteur, connecteur AZ-BUS ou carte porteuse supplémentaire ne sera ajouté à cette machine.

L’étude est conservée comme base officielle de l’AZ-3. Elle ne doit déclencher aucun recâblage de l’AZ-2, ne déplace pas le bouton B de la broche 8 et ne réserve aucune nouvelle broche sur le prototype actuel.

Architecture retenue :

- Teensy 4.1 maître : horloge, séquenceur, rack, mixage, effets et PCM5102A ;
- ESP32-S3 écran : UI, SD, émulation GB/GBC et installation des firmwares ;
- ESP32 moteur : cartouches de synthèse évoluée AZ-VA/AZ-WT ;
- ESP8266/ESP-12F nus : cartouches AZ-CHIP dédiées aux moteurs de consoles et au lo-fi ;
- AZ-BUS : UART de contrôle/flash et retour audio adapté à la famille de module.

### AZ-3 / Étude A — banc AZ-BUS

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Inventorier les modules nus : référence, flash, dimensions, quantité | tableau matériel confirmé |
| 2 | Valider UART matériel Teensy 28/29 | boucle locale sans erreur à 1 Mbaud |
| 3 | Valider RESET/BOOT sur 30/31 | dix passages boot normal/chargeur |
| 4 | Déplacer le bouton B de la broche 8 vers 26 ou 27 | contrôle physique toujours fonctionnel |
| 5 | Ajouter AudioInputI2S sur la broche 8 pour les moteurs ESP32 | sinusoïde externe propre |
| 6 | Créer HELLO/DESCRIBE/HEARTBEAT/METRICS | module détecté et panne non bloquante |
| 7 | Valider mute, limiteur et timeout du slot | aucun bruit au boot/reset |

Critère de sortie : un module de test se détecte, se réinitialise, produit une sinusoïde et peut tomber en panne sans arrêter l’AZ-2.

### AZ-3 / Étude B — cartouche ESP8266 AZ-CHIP

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Dessiner la porteuse ESP-12F : 5 V→3,3 V, straps, découplage, antenne, header | schéma relu avant PCB |
| 2 | Flasher un firmware de détection AZ-BUS | flash initial par programmateur |
| 3 | Porter minigb_apu comme AZ-CHIP GB | motif de test puis notes |
| 4 | Transporter le PCM par UART 1 Mbaud | 16 kHz/8 bits sans perte |
| 5 | Tester 22,05 kHz et choisir le meilleur compromis | mesures de pertes/latence |
| 6 | Ajouter patches/macros et sauvegarde | moteur utilisable dans une piste |
| 7 | Ajouter PSG puis AY | catalogue de trois moteurs validés |

Critère de sortie : AZ-CHIP GB joue 30 minutes sans coupure, se reflashe depuis la machine et reste silencieux au repos.

### AZ-3 / Étude C — cartouche ESP32 AZ-VA1

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Identifier ESP32/S3 et PSRAM disponibles | environnement PlatformIO figé |
| 2 | I2S esclave 44,1 kHz vers Teensy | sinusoïde stéréo stable |
| 3 | AZ-VA1 minimal : 4 voix, 2 oscillateurs, ADSR, filtre | notes et paramètres stables |
| 4 | Mesurer CPU/RAM/xruns | métriques visibles à l’écran |
| 5 | Étendre vers 8 voix, wavetable, unisson, LFO, saturation, chorus | uniquement selon budget mesuré |
| 6 | Ajouter patches et écran d’édition | sauvegarde/restauration cohérente |

Critère de sortie : moteur polyphonique jouable, sans dérive d’horloge, avec latence stable et aucune allocation dans la boucle audio.

### AZ-3 / Étude D — installation depuis la machine

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Manifestes firmware sur SD avec cible/version/SHA-256 | mauvaise image refusée |
| 2 | Fenêtre Moteurs > Installer | module et compatibilité visibles |
| 3 | FW_BEGIN/FW_CHUNK/FW_END/FW_COMMIT | progression et CRC |
| 4 | Mute et arrêt transport automatiques | aucun bruit pendant flash |
| 5 | Reboot, handshake et retour arrière | interruption volontaire récupérable |

Le premier flash reste réalisé par le programmateur ou USB/SWD. La machine prend ensuite en charge les mises à jour AZ-BUS.

## Phase 9 séparée — émulation GB/GBC expérimentale

Des cœurs et des briques de frontend existent, mais **aucun émulateur GB/GBC
n'est actuellement fonctionnel et validé de bout en bout sur la machine**.
Cette phase reste séparée du chantier audio : aucune ROM ne doit être annoncée
jouable avant validation conjointe vidéo, commandes, audio et sauvegarde.

### Lot 9.1 — instrumentation et vitesse

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Mesurer FPS logique, FPS vidéo, temps CPU, temps blit et retard cumulé | statistiques une fois/seconde |
| 2 | Compter frames sautées et audio underrun/overflow | diagnostic visible |
| 3 | Isoler l’émulation dans une tâche dédiée | UI/tactile ne ralentissent plus le jeu |
| 4 | Double buffering et frameskip Auto/Off/1 | 59,7275 Hz logique stable |
| 5 | Exécuter ROM de test CPU/timers/PPU | résultats consignés |

### Lot 9.2 — audio console

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Passer le lien écran↔Teensy à 921600 après test réel | aucune erreur série |
| 2 | Paquet audio V2 avec longueur 16 bits, séquence et pertes | protocole robuste |
| 3 | Buffer audio non bloquant séparé | aucune attente UART dans la frame |
| 4 | Tester 22,05 kHz mono 16 bits ou stéréo 8 bits | choix par écoute et mesures |
| 5 | Conserver 8 kHz comme mode secours | repli automatique possible |
| 6 | Conserver l’étude AZ-CHIP GB pour AZ-3 | aucun module externe dans AZ-2 |

### Lot 9.3 — interface JEUX complète

| Priorité | Tâche | Validation |
| ---: | --- | --- |
| 1 | Bibliothèque Tous/GB/GBC/Favoris/Récents | navigation tactile et croix |
| 2 | Index SD paginé sans limite fixe à 40 | grande collection testée |
| 3 | Bouton D = menu pause | Reprendre/Sauver/Reset/Réglages/Quitter |
| 4 | Palettes DMG et frameskip configurable | réglages persistants |
| 5 | Performance overlay optionnel | aucun coût quand masqué |
| 6 | Sauvegarde RAM manuelle + périodique sûre | reprise après coupure contrôlée |
| 7 | Jaquettes chargées à la demande | aucun accès SD pendant le jeu |

Critère final émulation : 59,7275 Hz logique stable pendant 30 minutes, audio sans coupure, dix jeux homebrew GB/GBC testés, sauvegardes fiables et aucune fuite PSRAM après dix changements de ROM.

Priorités matérielles AZ-2 : fiabiliser le câblage existant, ranger et immobiliser le faisceau, préserver l’accès USB/SD, terminer le sampler et l’émulation. Ne plus ajouter de carte dans ce boîtier.

Documents associés : AZ2_EMULATION_JEUX.md pour l’AZ-2 actif ; AZ2_BUS_RACK_MOTEURS.md, AZ2_FLASH_MODULES.md, AZ2_MODULE_ESP32_AZ_VA1.md et AZ2_MODULE_ESP8266_AZ_CHIP.md comme études AZ-3.

## Phase 10 active AZ-2 — fiabilité avant nouvelles fonctions (19 septembre 2026)

L'audit actualisé est dans [AZ2_AUDIT_CONTINU_2026-09-19.md](AZ2_AUDIT_CONTINU_2026-09-19.md). Les corrections `INST:255`, sauvegarde temporaire et sampleur à 16 pads existent déjà ; les critères ci-dessous portent sur leur validation et leurs risques restants.

| Ordre | Travail | Critère de sortie |
| ---: | --- | --- |
| 1 | Projets et patches : version, contrôle d'intégrité, validation complète avant application, reprise `.bak` | 20 cycles save/load et essais de coupure à plusieurs points sans perte du dernier projet valide |
| 2 | Sampleur : permutation sûre des buffers, niveaux du bus pads, essais simultanés | 16 pads déclenchés sans lecture de buffer en cours d'écriture ni saturation non maîtrisée |
| 3 | RAM GB : sauvegarde manuelle et périodique atomique | progression conservée après coupure contrôlée, ancien `.sav` toujours récupérable |
| 4 | Séquenceur : durée maximale ISR et accès partagés | 30 minutes de lecture avec édition live, aucun dépassement ni état incohérent |
| 5 | Émulation : matrice GB/GBC et métriques du lot 9.1 | 10 ROMs documentées, FPS logique, pertes audio et reprise de sauvegarde mesurés |

Le passage au lot suivant dépend des essais sur la machine réelle. Les builds et tests natifs ne remplacent pas les mesures audio, SD et temps réel.

Avancement du lot 1 : les nouvelles sauvegardes projet/patch utilisent `AZ2V2` et un CRC32 ; le chargement tente le `.bak` si le fichier principal est absent ou invalide. Les projets sont contrôlés avant chargement et avant remplacement du fichier précédent : lignes obligatoires, indices uniques et plages principales. Restent les essais de coupure et d'erreur SD sur matériel, ainsi que la validation exhaustive des champs optionnels.

Avancement du lot 2 : l'arrêt du pad et la mise à jour de ses métadonnées sont synchronisés avec l'interruption audio ; le chargement WAV ne bloque pas les mises à jour audio. Les deux cartes ont été flashées et leur liaison vérifiée par journaux série. Restent l'essai d'un pad remplacé pendant sa lecture et la mesure du niveau avec plusieurs pads simultanés.

### Éditeur SAMPLEUR et pages SONG / PROJETS

Le travail d'interface demandé le 19 septembre est intégré au code : AUDIO ouvre SAMPLEUR ; celui-ci propose 16 pads compacts, la liste paginée des WAV de la SD Teensy, l'affectation au toucher ou à la croix/A, l'écoute avec B et quatre kits sauvegardables. SONG et PROJETS sont maintenant deux pages distinctes : SONG garde le chaînage, tandis que PROJETS présente les quatre slots avec CHARGER et SAUVER. La sauvegarde de projet comprend déjà les chemins des samples affectés.

Validation : les deux cartes sont flashées. Le Teensy renvoie `SAMPLELIST`/`PADSAMPLE?` sur le matériel ; la navigation simulée ouvre la nouvelle page, et le pad 8 accepte un WAV puis reçoit bien `PAD:08:DOWN/UP` avec confirmation LED. Restent l'écoute humaine, le contrôle tactile/visuel et un cycle sauver/recharger un kit puis un projet dans des slots choisis sans écraser de données utiles. Mesurer aussi la latence du parcours SD avec une bibliothèque volumineuse.

Extension arborescence demandée ensuite : le Teensy flashé liste maintenant le dossier courant (`SAMPLEDIR` ou `SAMPLEFILE`) avec pagination bornée. La racine et `/samples/BASS` sont confirmés sur la vraie SD. L'écran dispose du chemin courant, de l'ouverture au toucher/A et de la remontée par C/« < » ; le nouveau binaire écran compile et attend son flash en mode BOOT manuel. Après flash, parcourir plusieurs niveaux et vérifier le retour parent et l'affectation d'un WAV dans un sous-dossier.

Mise à jour du 20 septembre : [état vérifié et répartition des cartes SD](AZ2_ETAT_2026-09-20.md). Les projets de démonstration ont été complétés au format chargé par l'ESP32 ; les slots 1 à 3 sont copiés sur sa carte SD, le slot 0 existant est préservé. Le séquenceur actif utilise désormais `sequencer.h` et `ISRLOAD?` permet de lire sa durée maximale de tick. Les validations sur le prototype restent nécessaires.

## Point de bascule du 20 septembre — AZ-2 vers l'interface et le rack logiciel

| Bloc | État réel | Décision |
| --- | --- | --- |
| Interface écran, tactile, croix/boutons, tracker, PATCH, SONG, PROJETS | Très avancé et flashé sur le prototype ; une passe page par page reste nécessaire | Stabiliser les retours, zones tactiles et redessins |
| Rack audio local Teensy | Présent : 8 pistes, six moteurs, mixage, séquenceur partagé et contrôle moteur/patch | Mesurer la charge avec `projects/4.proj` |
| Sampleur | Pads et moteur SAMPLER par piste présents ; banque WAV, découpage et édition avancée restent à faire | Qualifier les 16 pads et la permutation des samples |
| Émulation GB/GBC | Non fonctionnelle et non validée actuellement ; plusieurs briques et prototypes existent sans former une fonction utilisable de bout en bout | La garder hors du périmètre audio immédiat et reprendre sa validation séparément |
| Rack général multi-cartes | Architecture AZ-BUS étudiée pour AZ-3 ; le boîtier AZ-2 est déjà plein | Ne pas ajouter de matériel dans AZ-2 |

Ordre retenu : (1) tester chaque page au tactile et aux boutons, (2) jouer le projet de charge et mesurer ISR/audio, (3) qualifier le sampler, (4) établir la matrice de dix ROMs GB/GBC, puis (5) reprendre les fonctions d'émulation qui apportent un gain musical direct. La synchronisation LSDJ ↔ tracker et le rack multi-cartes restent hors du chemin critique AZ-2.
### Idée à conserver — longueur variable des patterns

Ajouter dans le tracker un mini-menu ouvert par l'appui sur l'encodeur 3. Une
ligne de ce menu permettra de régler la longueur du pattern en cours, avec
validation par l'encodeur. Cette fonction est notée pour une passe séquenceur
ultérieure ; elle ne doit pas interrompre l'optimisation de l'émulation X3.
