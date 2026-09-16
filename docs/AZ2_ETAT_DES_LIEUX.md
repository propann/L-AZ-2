# AZ-2 - Etat des lieux (2026-09-15, soir)

Topo demande ("on fait un topo de la situation, on documente") avant
une session de developpement autonome de nuit ("tu as toute la nuit,
avance tout ce que tu peux"). Point de depart pour la suite -- mis a
jour au fil de la nuit dans la section "Journal de la nuit" en bas.

## Vue d'ensemble

AZ-2 est une groovebox modulaire a 2 cartes :
- **Teensy 4.1** (`src_teensy/az2_audio/`) : moteur audio temps reel,
  sequenceur, DAC I2S (PCM5102A).
- **ESP32-S3** (`src_esp32/az2_screen/`) : ecran tactile 480x480 RGB
  parallele (carte VIEWE UEDX48480040E-WB), interface utilisateur,
  emulateur Game Boy/GBC, carte SD (ROMs).
- Liaison UART 230400 bauds entre les deux (Serial1 Teensy <->
  GPIO19/20 ESP32), protocole texte ligne par ligne + paquets binaires
  (audio GB, voir plus bas) -- partage via `lib/AZ2_Protocol/`.
- Croix + 4 boutons + 3 encodeurs rotatifs (avec bouton) cables
  directement sur le Teensy -- remplacent le Pico/matrice abandonnes
  le 2026-09-14.

## Ce qui marche et est CONFIRME en reel

- **Son** : 8 pistes, 5 moteurs par piste (Dexed FM, mda EPiano,
  Braids, Karplus-Strong, Analog), patch reel par moteur, bus d'effets
  maitre (reverb + delay), sortie DAC -- confirme audible.
- **Ecran** : affichage + tactile multi-doigt confirmes.
- **Lien UART** Teensy<->ESP32 confirme (HELLO, echo d'etat complet a
  la reconnexion).
- **Sequenceur** (avant ce soir) : 16 pas, note par pas, tempo/division
  reglables, horloge sur IntervalTimer (jitter-free, mesure).
- **Encodeurs rotatifs (3) + bouton integre** : cables et testes en
  reel -- pilotent volume/reverb/delay (rotation) + boutons GB Select/
  Start (encodeurs 2/3) en mode JEUX.
- **Croix + boutons A/B/C/D** : cables et testes en reel.
- **Emulateur Game Boy/GBC** (Walnut-CGB) : confirme, une ROM reelle
  (Tobu Tobu Girl, homebrew MIT) demarre et tourne. Sauvegarde cart RAM
  (`.sav`) sur la carte SD ESP32. Liste de ROM (plusieurs fichiers dans
  `/games`) -- backend fait, UI faite, **pas encore testee avec
  plusieurs ROM en meme temps sur la carte**.
- **Menu ecran** reorganise en 4 categories (MUSIQUE/JEUX/CONFIG/DOC),
  navigable croix+A/B+tactile -- confirme en reel.
- **Ecran de veille** ("matrix"), configurable, confirme en reel.

## Ce qui est CODE ce soir mais PAS ENCORE TESTE EN REEL

(l'ESP32 attend un flash -- mode boot manuel requis, utilisateur
absent ; le Teensy est a jour et flashe)

- **Son de l'emulateur GB -> DAC Teensy** : minigb_apu vendore, paquets
  PCM 8kHz mono envoyes sur Serial1, reçus et rejoues via
  `AudioPlayQueue` cote Teensy (bus d'effets maitre). Compile, flashe
  cote Teensy, **jamais entendu en vrai**.
- **Tracker, fondation** (voir `AZ2_TRACKER_ETUDE.md`) :
  - Ticks internes au pas (4/pas) cote Teensy, effets ARP/CUT/RETRIG
    par pas -- compile, flashe cote Teensy, **jamais entendu**.
  - Colonnes NOTE/INST/FX/VAL par pas, vue "detail" cote ecran (comme
    l'ecran phrase LSDJ/M8) -- **ESP32 pas reflashe, jamais vu/teste**.
  - **Bug trouve et corrige** : la grille sequenceur ET la page MOTEURS
    etaient figees a 4 pistes affichees alors que le Teensy en joue 8
    depuis le 2026-09-14 -- corrige (8 partout), pas encore verifie a
    l'oeil sur le vrai ecran.
- **Toolchain** : `teensy_loader_cli` du paquet PlatformIO (bugue sur
  les gros firmwares Teensy 4.1) remplace par une recompilation depuis
  la source officielle a jour (`tools/`) -- confirme fonctionnel
  (plusieurs flashs Teensy reussis depuis).

## Bugs connus / limites actuelles

- Colonne **INST** (patch par pas) : stockee et transmise, **pas
  encore appliquee** au son reel (changer de patch Dexed est trop
  couteux pour tourner dans l'ISR du sequenceur) -- a resoudre plus
  tard (patch pre-charge en avance ? limite a certains moteurs ?).
- **Pas de chainage de patterns** (song/chain) -- un seul pattern de 16
  pas en boucle, pas d'arrangement long.
- **Pas de gammes/accords**, pas de clavier tactile comme editeur live
  de note, pas de sauvegarde/chargement de projet -- demandes,
  documentees dans `AZ2_TRACKER_ETUDE.md` (etapes 5-7), pas commencees.
- Boutons **C/D** liberes du mapping GB (etaient Select/Start, main-
  tenant sur les encodeurs) -- pas encore de role assigne ("gachette").
- Pas de filtre par piste, pas d'ADSR editable (l'ADSR de l'Analog est
  fixe en dur dans `setup()`), pas de vue d'onde/oscilloscope.

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
  Tant que ce flash n'est pas fait, les commandes DXP: envoyees par
  l'ESP32 sont silencieusement ignorees par le Teensy (prefixe inconnu),
  sans consequence audio -- juste pas encore audible.
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
