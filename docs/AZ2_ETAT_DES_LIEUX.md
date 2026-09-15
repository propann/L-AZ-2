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

## Etat au reveil -- ce qui reste

- **ESP32 jamais reflashe cette nuit** (mode boot manuel requis, acces
  physique). Tout ce qui est cote ecran depuis "Tracker : ticks
  internes..." (voir git log) n'a ete verifie qu'a la compilation :
  fix 8 pistes, vue detail tracker, couleurs d'effets, son GB au DAC
  (partie ESP32 qui envoie), page PATCH + oscilloscope. **Rien de tout
  ca n'a ete vu a l'oeil ni entendu.**
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
