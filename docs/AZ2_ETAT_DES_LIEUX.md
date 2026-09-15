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

## Prochaines etapes (cette nuit, dans l'ordre)

1. Couleurs pour les effets sur la vue detail (visuel).
2. Filtre par piste (AudioFilterStateVariable, deja dans la lib Audio
   Teensy -- cutoff/resonance).
3. ADSR editable (au moins moteur Analog, deja structure autour d'une
   enveloppe).
4. Vue "oscilloscope" : reutilise le meme mecanisme de paquets binaires
   que le son GB (voir AZ2_Protocol.h), mais Teensy -> ESP32 cette
   fois, pour visualiser la forme d'onde en direct pendant l'edition
   d'un patch.
5. Continuer le tracker (gammes/accords, clavier live) si le temps le
   permet.

Rien de tout ca ne sera flashe/teste sur l'ESP32 cette nuit (acces
physique requis) -- tout sera compile-verifie (`pio run`) a chaque
etape, marque clairement "pas teste en reel" tant que ce n'est pas
confirme.

## Journal de la nuit

(rempli au fil de l'eau)
