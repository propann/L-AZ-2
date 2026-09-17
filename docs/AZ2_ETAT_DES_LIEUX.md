# AZ-2 - Etat des lieux

**[Rafraichi le 2026-09-17]** -- ce document a commence comme un topo
ponctuel (2026-09-15) puis est devenu un journal chronologique (sections
datees plus bas, gardees pour l'historique). Les 3 sections qui suivent
resument l'etat REEL actuel ; pour le detail jour par jour, voir le
journal en dessous de "Journal de la nuit".

## Vue d'ensemble

AZ-2 est une groovebox modulaire a 2 cartes, orientee Game Boy/GBC :
- **Teensy 4.1** (`src_teensy/az2_audio/`) : moteur audio temps reel
  (5 synthes, filtre/ADSR/volume/mute/solo par piste), sequenceur/
  tracker, sampler (capture), MIDI IN, DAC I2S (PCM5102A), carte SD
  dediee (samples).
- **ESP32-S3** (`src_esp32/az2_screen/`) : ecran tactile 480x480 RGB
  parallele (carte VIEWE UEDX48480040E-WB), interface utilisateur,
  emulateur Game Boy/GBC, carte SD (ROMs, patches, projets).
- Liaison UART 230400 bauds entre les deux (Serial1 Teensy <->
  GPIO19/20 ESP32), protocole texte ligne par ligne + paquets binaires
  (audio GB, oscilloscope) -- partage via `lib/AZ2_Protocol/`.
- Croix + 4 boutons + 3 encodeurs rotatifs (avec bouton) cables
  directement sur le Teensy -- remplacent le Pico/matrice abandonnes
  le 2026-09-14.

## Ce qui marche et est CONFIRME en reel (verifie sur le vrai materiel)

- **Son** : 8 pistes, 5 moteurs, patch reel par moteur, filtre resonant
  par piste, ADSR par piste, bus d'effets maitre (reverb+delay), sortie
  DAC -- confirme audible.
- **Ecran** : affichage + tactile multi-doigt.
- **Lien UART** Teensy<->ESP32 (HELLO, echo d'etat complet).
- **Croix + boutons A/B/C/D + 3 encodeurs (bouton integre)** : cables
  et testes en reel.
- **Emulateur Game Boy/GBC** (Walnut-CGB) : ROM reelle testee (Tobu
  Tobu Girl), sauvegarde cart RAM sur SD.
- **Sampler, capture (phase 1)** : ecriture sur la carte SD DEDIEE du
  Teensy confirmee (`REC:START`/`REC:STOP` testes en serie).
- **Reglages Dexed (DXP:, algo/feedback)** : testes en serie.
- **Menu ecran** 4 categories, navigable croix+A/B+tactile.
- **Ecran de veille** ("matrix"), configurable.
- **Toolchain** `teensy_loader_cli` recompile depuis la source
  officielle -- fiable, plusieurs flashs reussis.

## Ce qui est CODE mais PAS ENCORE VERIFIE EN REEL

Tout ce qui suit compile mais n'a jamais ete flashe/entendu/vu depuis
son ecriture (la session du 2026-09-17 a ete faite sans aucun board
branche) -- **priorite au reveil du materiel : tout reverifier avant
d'ajouter quoi que ce soit d'autre** :

- **Mute/solo par piste** (boutons C/D, page MOTEURS).
- **Volume par piste** (page PATCH).
- **Sauvegarde/chargement de PROJET complet** (page PROJET).
- **MIDI notes IN** (jamais teste avec un vrai controleur).
- **Clavier tactile comme editeur live** (bouton D, page AUDIO).
- **Pagination de la liste de ROM** (16->40, defilement par pages de 8).
- **Bouton C pour quitter une partie GB**.
- Tracker : ticks/colonnes NOTE-INST-FX-VAL/chainage song/gammes --
  flashes et utilises depuis, stables.

## Bugs connus / limites actuelles

- Colonne **INST** (patch par pas) : stockee et transmise, **pas
  encore appliquee** au son reel (changer de patch Dexed est trop
  couteux pour tourner dans l'ISR du sequenceur).
- **Accords** (plusieurs notes par pas) : pas fait, plus gros que
  prevu -- voir AZ2_BENCHMARK_CONCURRENCE.md.
- **Swing/groove** : pas fait, piege timer/ISR trouve -- voir
  AZ2_FEUILLE_DE_ROUTE.md.
- **Sampler** : capture faite, PAS de lecture (pas de moteur
  `AudioPlaySdWav`/`AudioPlaySdRaw`), pas de vue d'onde/decoupage/
  nommage a l'ecran.
- **Pas de vrai panoramique** (chaine audio mono de bout en bout).
- **GitHub** : remote configure mais authentification pas encore
  etablie sur cette machine -- voir la section GitHub du journal.

## Journal de la nuit -- ce qui a ete fait

Ordre reel (pas tout a fait celui prevu au depart, un bug reel a pris
du temps a chasser -- voir plus bas) :

1. **[FAIT, teste en reel]** Couleurs par effet sur le tracker (grille
   + vue detail) -- une couleur par effet (ARP/CUT/RET), pastille sur
   la grille, fond teinte sur la vue detail. Compile, pas teste a
   l'oeil (ESP32 pas flashe cette nuit).
2. **[FAIT, teste en reel]** Filtre resonant par piste (`FILT:`,
   `AudioFilterStateVariable` -- Chamberlin SVF entier, tres bon
   marche en CPU). Insere entre chaque moteur et son mixeur de groupe.
   Teensy flashe, `CPU?` interroge : 8.6%/9.9% -- essentiellement
   identique a avant (8-15% deja mesure), les 8 filtres n'ajoutent
   presque rien. Grand ouvert par defaut (le constructeur de la lib
   partait a 1000Hz, corrige explicitement sinon toutes les pistes
   auraient ete etouffees des le premier boot).
3. **[FAIT, teste en reel]** ADSR editable par piste (`ENV:`), applique
   a `trackAnalogEnv[]` (seul moteur de la palette avec une vraie
   enveloppe generique exposable -- Dexed/EPiano ont leur propre
   comportement interne au patch, Braids/Karplus n'ont pas
   d'enveloppe). Teste : commande acceptee/relayee, CPU inchange.
4. **[FAIT, BUG REEL trouve et corrige, teste en reel]**
   Oscilloscope Teensy -> ESP32 (`SCOPE:`, meme principe de paquet
   binaire que le son GB mais dans l'autre sens). Premiere version
   **gelait completement le Teensy** des que PLAY et SCOPE tournaient
   en meme temps (plus aucune reponse serie du tout). Diagnostique
   methodiquement par bissection sur les commits precedents (tick
   rewrite seul OK, filtre seul OK, ADSR seul OK -- donc le bug etait
   forcement dans le scope non commite), puis isolation precise
   (connect() seul OK, begin() seul OK, les deux + audio reel qui
   circule = gel garanti). Cause : la boucle de purge appelait
   `freeBuffer()` SANS `readBuffer()` d'abord -- `freeBuffer()` ne fait
   RIEN si aucun bloc n'a ete lu, donc la boucle tournait a l'infini.
   Corrige, reteste plusieurs fois avec le sequenceur actif 5s+ en
   continu : CPU 10.6%, memoire audio saine (135-143/200 blocs), aucun
   gel. Cote ESP32 : page PATCH (selecteur de piste, tracer d'onde,
   6 reglages CUTOFF/RESONANCE/ATTACK/DECAY/SUSTAIN/RELEASE) codee et
   compilee, **jamais vue a l'ecran** (ESP32 pas flashe).
5. **[FAIT]** Gammes (verrouillage a la saisie) : 5 gammes (chromatique/
   majeur/mineur/pentatoniques, racine C fixe), reglage sur la page
   CONFIGURATION. `nextNoteInScale()` remplace le "+/-1 demi-ton" sur
   la croix (grille ET vue detail du sequenceur) -- saute a la
   prochaine note dans la gamme. Chromatique = comportement d'origine,
   retro-compatible par defaut. Cote ESP32 uniquement, rien cote
   Teensy. Compile, pas vu en reel.

Verification finale (fin de nuit) : `pio run -e master_teensy -e
screen_esp` -- les deux compilent ensemble sans erreur. Teensy
reinterroge une derniere fois : `CPU?` -> usage=9.4%/max=9.5%, memoire
132-144/200 blocs -- stable sur toute la nuit de tests.

## Etat au reveil -- ce qui reste

- **ESP32 jamais reflashe cette nuit** (mode boot manuel requis, acces
  physique). Tout ce qui est cote ecran depuis "Tracker : ticks
  internes..." (voir git log) n'a ete verifie qu'a la compilation :
  fix 8 pistes, vue detail tracker, couleurs d'effets, son GB au DAC
  (partie ESP32 qui envoie), page PATCH + oscilloscope, gammes. **Rien
  de tout ca n'a ete vu a l'oeil ni entendu.**
- **Teensy a jour et verifie en reel** pour tout : tick rewrite, filtre,
  ADSR, oscilloscope (bug corrige).
- Prochaine chose utile au reveil : mode boot manuel sur l'ESP32
  (BOOT maintenu + RESET + relacher BOOT), puis `pio run -e screen_esp
  -t upload --upload-port /dev/ttyUSB0`, puis verifier a l'oeil/a
  l'oreille tout ce qui precede.
- Pas commence : gammes/accords, clavier tactile comme editeur live,
  sauvegarde/chargement de projet (etapes 5-7 de
  `AZ2_TRACKER_ETUDE.md`), colonne INST reellement appliquee au son,
  chainage de patterns, role "gachette" pour C/D.

## Mise a jour 2026-09-16

Suite reflex demandee sur le panneau lateral du sequenceur ("on a acces
aux reglages du patch ? ... un bouton pour agrandir ... on doit pouvoir
sauvegarder les patchs ... un sampleur ... a integrer") :

- **Panneau "patch actif"** (`drawTrkSidePanel()`) : affiche desormais
  CUTOFF/RESONANCE + ADSR (ou ALGO/FEEDBACK pour une piste DEXED, voir
  plus bas) de la piste selectionnee, avec un bouton **AGRANDIR** qui
  ouvre la page PATCH complete pour cette piste.
- **Sauvegarde/chargement de patch** : page PATCH, ligne SLOT (0-7) +
  boutons SAVE/LOAD. Ecrit sur la carte SD de l'ESP32 dans
  `/patches/N.txt` (moteur, patch, cutoff, reso, ADSR, + algo/feedback
  Dexed). LOAD renvoie ENGINE:/PATCH:/FILT:/ENV:/DXP: au Teensy.
  **ESP32 reflashe et boot verifie propre en reel** (lien Teensy
  toujours vivant) ; **pas teste manuellement** (SAVE/LOAD pas encore
  touches a l'ecran par un humain).
- **Reglages propres a DEXED** (`DXP:`, demande "on n'a pas de reglages
  dans la fenetre dexed du tracker") : ADSR generique n'a jamais eu
  d'effet sur ce moteur (sa propre EG DX7 la remplace) -- page PATCH
  affiche maintenant ALGORITHME (1-32) et FEEDBACK (0-7) a la place pour
  une piste Dexed, lignes 4-5 grisees ("sans effet sur ce moteur").
  **Cote Teensy (`handleDexedParamCommand`) compile mais PAS FLASHE** --
  le Teensy n'etait pas accessible en USB depuis cette machine au
  moment du dev (relie a l'ESP32 par l'UART Serial1 seulement, ce qui
  explique le lien "TEENSY_AUDIO:READY" toujours vu au reboot ESP32).
  **[2026-09-17, confirme]** carte SD Teensy (`BUILTIN_SDCARD`) **PRESENTE
  et fonctionnelle** -- teste directement en serie (`REC:START` ->
  `REC:STARTED:/samples/SAMPLE_001.wav`, `REC:STOP` ->
  `REC:STOPPED:samples=0`, fichier cree avec succes). Plus de blocage
  materiel pour le sampleur -- reste a verifier le contenu audio reel
  (jouer un jeu pendant l'enregistrement, ecouter le .wav resultant).
  **[2026-09-17] Flashe et verifie en reel** : `CPU:usage=8.6%:max=9.9%`,
  `MEM:blocks=133:max=141/200` (identique aux mesures d'avant, aucune
  regression). `DXP:0:0:5`/`DXP:0:1:3` testes en direct par serie USB --
  bien relayes/appliques. Rechargement de patch (`PATCH:0:0`) confirme
  ecraser l'algo/feedback avec ceux de la banque (`DXP:0:0:4`/
  `DXP:0:1:0`), comportement voulu et documente.
- **Page MOTEURS, navigation croix** (demande "j'ai pas le controle
  joystick pour choisir et regler les moteurs") : HAUT/BAS choisissent
  la piste (surlignage blanc), GAUCHE/DROITE changent la valeur de la
  colonne au focus, BTN:A bascule le focus MOTEUR/PATCH. **ESP32
  reflashe, boot verifie propre**, comportement du tactile inchange
  (toujours utilisable en parallele).
- **Sampleur (moteur audio)** : PAS commence. Bloque tant qu'on n'a pas
  confirme une carte SD reellement presente dans le lecteur
  `BUILTIN_SDCARD` du Teensy (dernier statut connu : `SDTEENSY:
  NOT_PRESENT`) -- aucun fichier son n'existe encore, et un nouveau
  type de moteur (`AudioPlaySdWav`/`AudioPlaySdRaw`) + protocole de
  selection d'echantillon restent a concevoir. **Carte micro SD 58 Go
  preparee en FAT32 le 2026-09-16** (label `AZ2SAMPLES`, dossier
  `/samples/`) -- reste a l'inserer physiquement dans le lecteur du
  Teensy et confirmer `SDTEENSY:READY`.

## Journee du 2026-09-16 -- audit + recherche (utilisateur absent)

Demande : "tu as la journee pour faire des recherches sur nos
concurrents, liste des ameliorations indispensables, etat des lieux
logiciel/firmware, on optimise vitesse, on ameliore l'affichage, on
fait un job sur tout code mort, audit de code, mise a jour du GitHub,
presentation du projet et documentation". Boards absents (ni Teensy ni
ESP32 sur le bus USB de cette machine) -- tout ce qui suit est
**compile-verifie uniquement**, rien vu/entendu sur le vrai materiel
aujourd'hui.

- **Audit de code mort** : supprime `src_pico/` (Pico abandonne,
  cablage deja documente en prose ailleurs), `src_esp32/
  Launcher_lvgl-master/` (75 Mo vendores d'un exemple fournisseur,
  jamais reference dans le build), les stubs vides `src_esp32/main.cpp`
  et `src_teensy/main.cpp`, et `kHelloKeypad` (protocole) devenu
  orphelin. Verifie par grep avant suppression a chaque fois (rien
  d'autre ne referencait ces fichiers), et recompilation des 3
  environnements (`master_teensy`, `screen_esp`, `ui_esp`) apres coup.
  `src_esp32/az2_control/` (env `ui_esp`) et le gros dossier `src_teensy/
  microdexed-touch/` sont volontairement GARDES (reference active/
  bring-up isole documente comme tel, pas du code mort au meme sens que
  le Pico).
- **Petit bug UX corrige** : page MOTEURS, un toucher tactile ne
  deplacait pas le curseur croix -- desynchronisation possible entre
  tactile et croix. Corrige + recompile.
- **Recherche concurrence** : 2 machines ajoutees au comparatif
  (Teenage Engineering OP-XY, nanoloop) + 2 reverifiees (M8 Model:02,
  Polyend Tracker Mini) -- voir AZ2_BENCHMARK_CONCURRENCE.md pour le
  detail et les sources. Liste d'ameliorations indispensables priorisee
  ajoutee (mute/solo, sauvegarde de projet complet, swing, volume/pan
  par piste, sampler, MIDI, accords, clavier live).
- **Feuille de route** : ajout d'une "Phase 8" qui reprend cette liste
  priorisee, plus un tableau qui fait correspondre les vieilles phases
  0-7 (matrice/Pico/Retro-Go, plus d'actualite) a ce qui existe
  reellement aujourd'hui.
- **README.md** : entierement reecrit -- decrivait encore la matrice
  SparkFun 4x4 et le Pico comme actifs, en anglais, sans lien vers la
  moitie des docs recentes (tracker, etat des lieux, cablage master).
- **Performance** : revue du code (Teensy hot-path, boucles de dessin
  ESP32) -- pas de probleme flagrant trouve. Les redessins sont deja
  partiels (ligne par ligne sur MOTEURS/PATCH/sequenceur, tracer
  d'oscilloscope decime+limite en frequence). Usage de `String` pour le
  parsing des commandes serie (cote Teensy et ESP32) : design deja en
  place depuis le debut du projet, fonctionne dans la marge CPU/memoire
  mesuree (~9-10%, 130-144/200 blocs audio) -- **pas touche** sans
  materiel pour verifier un changement aussi central sans risque de
  regression invisible tant que personne ne peut tester en reel.
- **GitHub** : **bloque**. Le remote `origin` (github.com/propann/
  L-AZ-2) est configure mais aucune methode d'authentification
  disponible sur cette machine (pas de `gh auth login`, pas de jeton
  `GH_TOKEN`, pas de credential helper git) -- `git push` echoue avec
  "could not read Username". La branche `az2-screen-engines-sequencer`
  est 33+ commits devant `origin/main`, jamais poussee. Necessite une
  action de l'utilisateur (`gh auth login` ou jeton personnel) pour
  debloquer.

## Journee du 2026-09-17 -- emulateur GB + sampler phase 1

- **Verification du mapping manette GB** : Start/Select (encodeurs 1/2)
  confirmes deja en place depuis le 2026-09-15. Tous les boutons d'une
  vraie Game Boy couverts.
- **[FAIT, flashe, teste en reel]** Bouton **C** pour quitter une
  partie en cours (`goTo(Screen::Menu)`, sauvegarde la RAM cartouche).
  Page A PROPOS mise a jour avec le mapping complet.
- **[FAIT, flashe]** Pagination de la liste de ROM : `kGbMaxRoms` 16 ->
  40, affichage par pages de 8 avec fleches </> -- suite a l'ajout de
  34 jeux perso sur la carte SD ESP32 (Zelda, Mario, Tetris, Pokemon,
  Kirby, Wario, etc., copies depuis une cle USB personnelle, PAS dans
  le depot git).
- **[FAIT, flashe, PARTIELLEMENT teste]** Sampler GB phase 1
  (REC/STOP) : encodeur 0 declenche l'enregistrement, capture le son
  GB natif (8kHz) dans un `.wav` sur la SD DEDIEE du Teensy. **Ecriture
  SD confirmee en reel** (`REC:START`/`REC:STOP` testes directement en
  serie, fichier cree avec succes). **Pas encore teste avec un vrai
  jeu qui joue du son** (0 echantillon capture lors du test, aucune ROM
  chargee a ce moment) -- prochaine etape : enregistrer pendant une
  vraie partie et ecouter le fichier resultant.
- Phase 2/3 du sampler (vue d'onde en direct, decoupage tactile,
  decoupage automatique, clavier de nom, moteur de lecture) : pas
  commencees, detaillees dans AZ2_FEUILLE_DE_ROUTE.md.

## Teensy reflashe et VERIFIE EN REEL (2026-09-17, soir)

Tout le paquet du jour (mute/solo, volume, sauvegarde de projet, MIDI
IN, swing, sampler) flashe sur le Teensy et teste directement en
serie :

- Boot propre, `CPU:usage=8.6%:max=9.9%`, `MEM:blocks=133:max=141/200`
  -- identique a avant tous ces ajouts, aucune regression.
- `SWING:64`, `MUTE:0:1/0`, `SOLO:0:1/0`, `VOL:0:100` : tous acceptes
  et relayes correctement.
- **Test critique swing** : `SWING:96` (quasi maximum) puis `PLAY`
  pendant 5 secondes -- le sequenceur a avance plusieurs mesures sans
  aucun gel, CPU/memoire strictement identiques a l'avant. Tempo moyen
  visuellement correct (progression des CLOCK: coherente avec le BPM
  malgre l'asymetrie forte des pas). **Le mecanisme "ticksForCurrentStep
  variable, timer jamais reconfigure" tient la route en reel.**
- `REC:START`/`REC:STOP` (sampler) toujours fonctionnel apres tous ces
  ajouts (`SAMPLE_002.wav` cree).

**Mute/solo sur une piste Braids en train de jouer une note tenue**
(le cas precis que le piege d'origine aurait casse) : PAS ENCORE testE
manuellement (demande de jouer une note et de mute/demute pendant
qu'elle sonne, pas juste d'envoyer les commandes a vide) -- a faire au
prochain test avec le son.

## Audit complet du code (2026-09-17)

Relecture ligne par ligne des 2 firmwares + protocole (~6800 lignes)
demandee par l'utilisateur pour recouper avec des analyses d'autres IA.
3 corrections rapides appliquees (sans risque, pas besoin de materiel) :

- **Message trompeur a l'ecran** : la page JEUX sans ROM disait encore
  "Pas de son pour l'instant" -- corrige (le son GB fonctionne depuis
  longtemps).
- **Code mort** : `kRecToggle` (AZ2_Protocol.h) jamais utilise nulle
  part, decrivait en plus une convention ("toggle") differente de celle
  reellement implementee (REC:START/REC:STOP explicites) -- supprime.
- **Commentaire trompeur** : `handlePatternCommand()` disait qu'un
  changement de pattern edite est "effectif au prochain pas" -- en
  realite `advanceTick()` ne bascule qu'au prochain redemarrage de
  pattern (jusqu'a 16 pas plus tard), pas au pas suivant. Comportement
  voulu, juste le commentaire corrige.

3 points restants, notes pour plus tard (pas de risque immediat) :
setup() ESP32 s'arrete net si l'ecran ne s'initialise pas (Teensy/SD
jamais inities dans ce cas) ; aucune commande Teensy hors bornes ne
renvoie d'erreur (silencieusement ignoree) ; a swing maximum, les pas
raccourcis (1 tick) ne laissent plus de fenetre aux effets CUT/RETRIG.

Piste specifiquement verifiee et ECARTEE : `frame_skip` (Walnut-CGB)
pourrait desynchroniser l'audio de la vitesse du jeu -- verifie dans le
code source (`walnut_cgb.h`, VBlank pose `gb->gb_frame=true` a CHAQUE
frame reelle quel que soit `frame_skip`, qui ne saute QUE le rendu
pixel) : pas de bug la-dedans, confirme.

### Menage supplementaire : mentions du pad 4x4 (demande separee)

Passage dedie sur toutes les mentions de la matrice SparkFun 4x4/Pico
(hardware abandonne le 2026-09-14) pour verifier qu'aucune ne pretend a
tort etre le hardware actuel :

- Code et docs actifs (`main.cpp` ESP32/Teensy, `AZ2_Protocol.h`,
  `README.md`, `AZ2_CABLAGE_MASTER.md`, `platformio.ini`) : deja propres
  ou deja correctement marques "ABANDONNE" -- rien a faire.
- `AZ2_FEUILLE_DE_ROUTE.md` : la section "Phase 3 - Matrice SparkFun
  4x4" (plan d'origine) n'etait annotee comme abandonnee que bien plus
  bas dans le document (section "Mise a jour 2026-09-16") -- un lecteur
  qui lit dans l'ordre pouvait la croire encore active. Ajoute un
  avertissement direct sur la section elle-meme.
- `AZ2_PORTAGE_MICRODEXED_TOUCH.md` : contrairement aux autres docs de
  planification precoce (ECRAN_FACADE, ARCHITECTURE_FIRMWARE_DOUBLE,
  ESP32_CONTROLE_WIFI_SD_RETRO), celui-ci n'avait PAS recu de note
  "perime" alors qu'il presente la matrice 4x4 comme le controle retenu
  -- note ajoutee en tete de fichier.
- `src_esp32/az2_control/main.cpp` (environnement `ui_esp`, garde comme
  reference isolee, hors `default_envs`) : implemente reellement le
  scan mux 4x4 (`AZ2:FEATURE:SPARKFUN_4X4_MATRIX` etc.), donc le code
  n'est pas faux en soi, mais rien dans le fichier ne disait que ce
  hardware est abandonne et que ce n'est pas le firmware ESP32 shippe --
  commentaire d'entete ajoute pour clarifier. Compile verifie
  (`pio run -e ui_esp`), pas de changement de comportement.

## Bugs reels trouves en jouant (2026-09-17, utilisateur de retour)

Premier vrai test manette en main apres le flash de l'ESP32 -- 2 bugs
reels rapportes et corriges dans la foulee :

1. **"je peux pas selectionner une ROM avec la croix et A/B, avoir le
   nom en surbrillance"** -- la croix sur la page JEUX etait TOUJOURS
   routee vers `gbSetButton()` (boutons du jeu), meme quand aucune ROM
   n'etait encore chargee : aucune navigation clavier n'atteignait
   jamais la liste. Corrige : `selectedRomIndex` + surbrillance (meme
   convention que la piste choisie sur la page MOTEURS), HAUT/BAS
   deplacent la selection (suit le defilement automatiquement), A
   charge la ROM selectionnee. `gbSetButton()`/le mapping A/B ne
   s'activent plus que si une partie est REELLEMENT en cours
   (`gbIsLoaded()`), pas juste "page JEUX affichee".
2. **"bug d'affichage quand ca commence a demarrer l'emulation"** --
   trouve : `drawLinkStatus()` (bandeau "TEENSY: relie" en bas de
   l'ecran, `kStatusY = 450`) chevauche le bas de l'image du jeu
   (celle-ci va de y=24 a y=456, voir `gbBlitLine()`). Cette fonction
   est appelee a CHAQUE ligne recue du Teensy, y compris `STATUS:`
   envoye ~1x/seconde en continu -- une barre noire + texte
   s'incrustait donc sur le bas de l'ecran de jeu toutes les secondes
   pendant qu'une partie tournait. Corrige : exclue quand
   `currentScreen==Screen::Retro && gbIsLoaded()`, meme famille
   d'exclusion que Controls/Links deja en place.

Compile verifie, **pas encore reflashe** au moment d'ecrire cette
entree -- ESP32 deconnecte entre le rapport de bug et le fix.

## Suite du 2026-09-17 -- 5 des 8 priorites indispensables faites

Utilisateur parti travailler ("on attaque la feuille de route, avance
un max"). Boards absents de cette machine tout le long -- **tout ce qui
suit est compile-verifie uniquement**, rien vu/entendu sur le vrai
materiel :

1. **Mute/solo par piste [FAIT]** -- le piege Braids (gain de groupe =
   porte note-on/off) resolu proprement via `trackNoteHeld[]` +
   `applyGroupGainNow()` : mute/demute immediat meme sur une note
   Braids tenue. Boutons C/D (page MOTEURS).
2. **Sauvegarde/chargement de projet complet [FAIT]** -- page PROJET,
   4 emplacements, patterns+song+tempo+gamme+moteurs+mute (pas solo).
   Purement ESP32, zero risque audio.
3. Swing/groove -- **pas fait** (piege timer trouve, voir feuille de
   route).
4. **Volume par piste [FAIT]** -- meme mecanisme que mute/solo (memes
   fonctions `applyGroupGainNow()`/`trackEffectiveGain()`). Pan pas
   fait (chaine mono de bout en bout).
5. Sampler -- **phase 1 confirmee en reel hier** (ecriture SD testee),
   phases 2/3 (vue d'onde, decoupage, nommage) pas commencees.
6. **MIDI notes IN [FAIT]** -- MVP, route vers la voix live
   (`liveVoice`), `USB_MIDI_SERIAL` dormait dans platformio.ini depuis
   le debut du projet. Pas de synchro horloge, pas de MIDI OUT.
7. Accords -- **pas fait** (plus gros que prevu, touche le meme code
   que le bug de gel deja rencontre cette session).
8. **Clavier tactile comme editeur live [FAIT]** -- bouton D (page
   AUDIO) bascule "jouer en direct"/"poser sur le pas selectionne".

Reste vraiment a faire, dans l'ordre : tester TOUT ce qui precede en
reel des que les boards sont disponibles (rien n'a ete verifie
aujourd'hui, seulement compile), puis swing et accords (les deux
demandent du soin sur le vrai materiel a cause des pieges trouves),
puis le sampler phase 2/3 (design UI a faire avec l'ecran sous les
yeux).
