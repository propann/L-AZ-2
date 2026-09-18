# AZ-2 - Mode JEUX (emulation retro) : etude et decision (2026-09-14)

Contexte : demande explicite de trouver "un emulateur top compatible ESP"
et de choisir entre un double demarrage ou une entree de menu pour lancer
l'emulation console. Pico et Teensy debranches pendant cette etude --
travail de recherche/architecture uniquement, rien flashe ni teste en
reel ci-dessous tant que ce n'est pas marque "teste".

## Ce qui etait suppose au depart (a corriger)

Le depot vendait deja `src_esp32/retro-go-master` (voir
AZ2_BENCHMARK_CONCURRENCE.md, "Retro-Go: bonus isole, jamais melange au
firmware musical principal") en supposant qu'on pourrait l'integrer
directement. Ce n'est **pas** aussi simple :

- Retro-Go (https://github.com/ducalex/retro-go) est un firmware
  **ESP-IDF natif** (CMake, "components"), pas un sketch/lib Arduino --
  incompatible avec notre build PlatformIO `framework = arduino` tel
  quel. L'integrer voudrait dire un **vrai firmware separe**, pas un
  simple appel depuis `az2_screen`.
- Ses pilotes d'ecran officiels ciblent des dalles **SPI** classiques
  (ILI9341 -- ODROID-GO, MRGC-G32, "Cheap Yellow Display" ESP32-2432S028R
  reste la cible la plus optimisee). Notre ecran VIEWE
  UEDX48480040E-WB (GC9503V) est un panneau **RGB parallele**, pas SPI --
  aucun pilote officiel Retro-Go pretquer pour ce type de dalle a ete
  trouve (recherche faite, rien de concluant).
- Donc "integrer Retro-Go" = vrai double demarrage OTA (2 partitions, 2
  firmwares completement separes) **+** ecrire un pilote d'affichage RGB
  parallele pour Retro-Go (faisable techniquement -- ESP-IDF a
  `esp_lcd_rgb_panel` -- mais un vrai chantier, pas fait ici).

## Alternative etudiee (1er passage) : Anemoia-ESP32 (NES)

https://github.com/Shim06/Anemoia-ESP32 -- emulateur NES, licence GPLv3.
Arduino natif, ~400 lignes de coeur, acces framebuffer brut via
`nes.connectFramebuffer(cv_framebuffer)` (a cote du chemin
`nes.connectScreen(&screen)` couple a TFT_eSPI) -- integrable comme une
page dans notre firmware sans double demarrage. Retenu dans un premier
temps, **puis reoriente** suite a la precision du 2026-09-14 : "l'idee
c'est plutot GB/GBA/GBC, ca suffira" -- la NES n'est plus la priorite.

## Recherche 2 (2026-09-14, suite a la demande GB/GBA/GBC) : coeur retenu

### GB + GBC : Peanut-GB / Walnut-CGB -- tres bon choix

- **Peanut-GB** (https://github.com/deltabeard/Peanut-GB) : emulateur
  Game Boy (DMG) en **un seul header C99**, licence **MIT**. Assez rapide
  pour tourner a pleine vitesse sur un Raspberry Pi Pico (bien plus
  faible que notre ESP32-S3 a 240MHz) -- passe la suite de tests CPU de
  Blargg (bonne precision). Architecture 100% a base de callbacks
  fournis par l'hote : `gb_rom_read()` (lecture ROM -- flash, PSRAM ou SD,
  au choix), `gb_cart_ram_read/write()` (sauvegardes), et surtout
  `lcd_draw_line()` qui recoit une LIGNE de pixels a la fois (donnees
  indexees/palette) -- **aucun code d'affichage propre au projet**, donc
  aucun TFT_eSPI a arracher : on branche directement notre pilote
  Arduino_GFX existant dans le callback. C'est exactement le type
  d'architecture qu'on cherchait.
- **Walnut-CGB** (https://github.com/Mr-PauI/Walnut-CGB) : "remplacement
  quasi direct" de Peanut-GB (meme style de callbacks, quelques
  differences d'API a la migration), licence **MIT**, ajoute le **vrai
  support Game Boy Color** (palettes BG/OBJ0/OBJ1, 3 couches x 4 teintes).
  **Support ESP32-S3 explicite**, demos qui tournent sur M5Stack
  Cardputer (meme famille de puce que la notre) avec des jeux jouables
  (Mario, Zelda cites). Note utile : "le stockage flash interne
  ESP32/ESP32-S3 est utilisable [pour les ROM] tant que les regles
  d'alignement sont respectees" -- donc testable SANS carte SD au debut
  (petite ROM embarquee dans la flash), contrairement a ce qu'on pensait
  pour Anemoia/NES.
- **Licence MIT** (Peanut-GB et Walnut-CGB) au lieu de GPLv3
  (Anemoia/44gba) -- bien plus simple pour un projet perso, aucune
  obligation de copyleft a gerer.

### GBA : possible mais faible sur ESP32 -- pas prioritaire

- `44670/44gba` et `44670/44vba` (GPL-3.0) : **~20 fps avec frameskip=1**
  sur ESP32-S3-WROOM-1-N8R8 d'apres les chiffres trouves. La GBA (CPU
  ARM7TDMI, PPU bien plus complexe que GB/GBC) reste dure pour ce genre
  de microcontroleur -- "ca demarre et ca joue a peu pres", pas une
  experience fluide comme GB/GBC.
- **Recommandation** : ne PAS viser la GBA pour le v0 du mode JEUX.
  GB + GBC (Peanut-GB/Walnut-CGB) couvrent deja une tres large
  ludotheque avec une bien meilleure experience sur notre materiel. La
  GBA reste une option "bonus" a tenter plus tard si vraiment voulue,
  sans promettre une bonne fluidite.

## Decision (mise a jour)

- **v0 du mode JEUX** : porter **Walnut-CGB** (GB + GBC, superset de
  Peanut-GB) comme une page de plus dans `src_esp32/az2_screen` (menu ->
  "JEUX"), pas de double demarrage -- meme logique que decidee pour la
  NES, mais avec un coeur mieux adapte (callbacks purs, MIT, deja
  demontre sur ESP32-S3). Le `lcd_draw_line()` alimente directement notre
  `Arduino_GFX` existant, ligne par ligne ou via un framebuffer complet
  qu'on blitte d'un coup.
- **GBA** : pas dans le v0, chiffres de performance trop faibles sur
  cette puce -- a reconsiderer plus tard seulement si demande.
- **NES (Anemoia)** et **Retro-Go** (multi-console, ESP-IDF + double
  demarrage) restent des options pour APRES, si on veut elargir une fois
  GB/GBC solides -- rien a jeter, juste pas la priorite.

## Etat 2026-09-15 : implemente, pas encore joue en reel

Suite a "y'a rien dans jeux, c'est le moment de mettre l'emulateur" :

1. **[FAIT]** Walnut-CGB vendore dans
   `src_esp32/az2_screen/walnut_cgb/walnut_cgb.h` (header C99 unique,
   ~9900 lignes, licence MIT verifiee directement dans le fichier --
   compile avec notre toolchain pioarduino/Arduino-ESP32 core 3.x sans
   probleme.
2. **[FAIT]** Callbacks hote dans `gb_emulator.cpp` : `gb_rom_read`/
   `_16bit`/`_32bit` lisent directement une ROM chargee en PSRAM (module
   ESP32-S3 WROOM-1 N16R8, 8 Mo -- meme puce que l'ecran) ; cart RAM
   (sauvegardes) allouee dynamiquement selon `gb->num_ram_banks`, aussi
   en PSRAM, mais **pas encore persistee sur SD** (perdue a l'extinction
   -- a faire plus tard). `lcd_draw_line` convertit chaque ligne (CGB :
   index direct dans `gb->cgb.fixPalette`, deja en RGB565 ; DMG : 2 bits
   de teinte vers une palette verte classique Game Boy) et appelle
   `gbBlitLine()` (dans `main.cpp`, seul endroit qui connait `gfx`) qui
   fait un rendu a l'echelle x3 (160x144 -> 480x432, centre verticalement)
   via `draw16bitRGBBitmap()`.
3. **[FAIT]** ROM cherchee dans `/games/*.gb`/`.gbc` sur la carte SD --
   `gbScanRoms()` liste TOUS les fichiers trouves (jusqu'a 16) en
   entrant sur la page JEUX, `drawRetroPage()` affiche une liste
   tactile (demande 2026-09-15 : "il nous faut un menu pour demarrer la
   rom qu'on choisit dans une liste, pas uniquement un jeux") --
   toucher une ligne appelle `gbLoadRom(nom)` pour cette ROM precise.
   ROM dechargee en quittant la page (libere la PSRAM). Echec propre
   (pas de crash) si pas de carte/dossier/fichier -- page d'attente
   avec le message exact.
4. **[FAIT, remappe le 2026-09-15]** Entrees mappees sur la croix +
   boutons A/B + encodeurs deja cables sur le Teensy (`NAV:`/`BTN:`/
   `ENC:`, voir AZ2_CABLAGE_MASTER.md) : croix -> directions Game Boy,
   A/B -> A/B, encodeur 1 (Reverb) -> SELECT, encodeur 2 (Delay) ->
   START (demande : "faut les config sur les encodeurs ... comme ca on
   a a/b, start/select et les gachettes"). Tous les boutons d'une vraie
   Game Boy (pas de L/R sur le materiel original) sont donc couverts --
   verifie le 2026-09-17 apres relecture complete du mapping. C reste
   libre pendant le jeu (la GB d'origine n'a pas de bouton C) : **[FAIT,
   2026-09-17]** utilise pour quitter proprement la partie
   (`goTo(Screen::Menu)`, sauvegarde la RAM cartouche via `gbUnload()`
   avant de liberer la ROM -- demande "il faut un truc pour sortir de
   l'emulateur cote code", B etant deja pris par le bouton B du jeu).
   D reste libre (futur role de gachette, pas encore assigne) ;
   encodeur 0 (Volume) reserve au declencheur d'enregistrement de
   sample (voir plus bas, capture audio).
5. **[FAIT]** Page JEUX (`Screen::Retro`) mise a jour : affiche le jeu
   si une ROM est chargee (`gbRunFrame()` appelee depuis `loop()`,
   cadencee a ~59,7 images/s -- cible theorique, PAS mesuree en reel
   faute de ROM disponible), sinon la raison exacte de l'echec + "touche
   pour reessayer" (utile apres avoir insere une carte SD).
6. **[FAIT, 2026-09-15]** Le son : demande explicite ("il faut un
   emulateur complet classe ... envoyer sous forme de paquet ... pour
   que le DAC le joue"). Walnut-CGB n'emet pas l'audio lui-meme
   (`audio_read`/`audio_write` = simples hooks registre) -- ajout de
   **minigb_apu** (deltabeard/Peanut-GB, vendored dans `minigb_apu/`,
   licence MIT) qui transforme ces acces en PCM. `ENABLE_SOUND=1`
   desormais ; chaque frame GB, le buffer stereo 16 bits de minigb_apu
   est reduit a du mono 8 bits a 8kHz (delibere, tient large dans le
   budget du lien serie 230400 bauds existant vers le Teensy) et envoye
   en paquet binaire sur Serial1 (`kGbAudioPacketMagic`, voir
   AZ2_Protocol.h) mele au reste du protocole texte. Cote Teensy :
   re-echantillonnage vers 44.1kHz et lecture via un `AudioPlayQueue`
   branche sur le bus d'effets maitre (meme chemin que les moteurs de
   synthese -- reverb/delay/volume s'appliquent donc dessus aussi).
   Compile et flashe sans crash sur les deux cartes -- **son reel pas
   encore entendu/valide**, a confirmer en chargeant une ROM.
7. **[FAIT, 2026-09-15]** Sauvegardes cart RAM persistees sur SD :
   `<rom>.sav` a cote de la ROM dans `/games/`, ecrit en quittant la
   page JEUX, relu au chargement suivant si present.
8. **[FAIT, 2026-09-15]** ROM legale testee en reel : *Tobu Tobu Girl*
   (`tobu.gb`, homebrew, licence MIT/CC-BY-SA, cartouche MBC1+RAM+BATT --
   type supporte) recuperee depuis archive.org (metadata JSON verifiee
   directement, pas de fetch resume par IA -- voir la lecon "verification
   directe" dans les autres docs), verifiee (`file` confirme un en-tete
   GB valide), copiee sur la carte SD dans `/games/`. **Confirme par
   l'utilisateur : la ROM demarre et tourne reellement sur le materiel**
   -- premier succes reel du sous-systeme emulateur, video seule (pas de
   son, voir point 6).

Reste a faire : sauvegardes cart RAM persistees sur SD (point 7 ci-dessus),
le son (point 6), et verifier la liste (point 3) en reel avec plusieurs
ROM presentes en meme temps sur la carte -- teste jusqu'ici avec une
seule ROM (`tobu.gb`).

## Sources consultees

- Retro-Go: https://github.com/ducalex/retro-go
- Anemoia-ESP32: https://github.com/Shim06/Anemoia-ESP32
- Peanut-GB: https://github.com/deltabeard/Peanut-GB (README:
  https://raw.githubusercontent.com/deltabeard/Peanut-GB/master/README.md)
- Walnut-CGB: https://github.com/Mr-PauI/Walnut-CGB
- 44gba / 44vba (GBA, chiffres de perf): https://github.com/44670/44gba,
  https://github.com/44670/44vba
- Autres projets GB/GBC ESP32 releves en recherche (non retenus, moins
  bien adaptes) : GBCCAT (gnuboy, ESP32 WROVER, ST7789) --
  https://github.com/Djamal-UK/GBCCAT ; lualiliu/esp32-gameboy --
  https://github.com/lualiliu/esp32-gameboy

## Audit 2026-09-18 — objectif « console complète »

Le mode JEUX est un vrai émulateur GB/GBC, mais il n’est pas encore validé comme une console finie. Une ROM démarre réellement ; cela ne prouve ni la cadence officielle sur la durée, ni la fidélité audio, ni la compatibilité d’une bibliothèque de jeux.

### État exact du chemin actuel

| Élément | Implémentation actuelle | Limite |
| --- | --- | --- |
| Cœur | Walnut-CGB | Bon candidat, mais aucune suite de ROM de test exécutée sur l’AZ-2 |
| Cadence | appel de gbRunFrame depuis loop, cible 16 742 µs | dépend encore du tactile, de l’UI, de l’UART et du rendu |
| Vidéo | framebuffer 480×432 en PSRAM, un transfert par image dessinée | frame_skip actif : affichage d’une image sur deux |
| Audio APU | minigb_apu stéréo 16 bits en interne | réduit en mono 8 bits/8 kHz avant transport |
| Transport audio | paquets sur UART 230400 partagé avec le contrôle | qualité limitée et appels Serial susceptibles de bloquer |
| Sortie | rééchantillonnage Teensy vers 44,1 kHz, DAC PCM5102A | continuité et niveau pas encore validés à l’oreille |
| Sauvegarde | RAM cartouche .sav sur SD à la sortie | pas de sauvegarde instantanée d’état |
| Interface | liste paginée de 40 ROM maximum | pas de favoris, recherche, jaquette, détails ni menu pause |

### Définition de « vitesse officielle »

La référence GB est d’environ 59,7275 frames logiques par seconde, soit 16 742 µs par frame. Le nombre important n’est pas seulement le FPS affiché :

- l’émulation doit produire 59,7275 frames logiques/s à 100 % ;
- aucune rafale ne doit accélérer brutalement le jeu après un retard ;
- le rendu peut omettre une image si nécessaire, jamais le CPU, les timers ou l’APU ;
- l’audio doit rester continu et servir d’horloge de stabilité ;
- mesurer moyenne, minimum, maximum, retard cumulé, frames vidéo sautées, sous-alimentations audio et temps de blit.

### Architecture d’exécution V1

1. Tâche émulation dédiée, priorité stable, séparée du tactile et des menus.
2. Horloge monotone en microsecondes ; aucune cadence basée sur delay.
3. Double framebuffer 160×144 ou 480×432 selon la mesure la plus rapide.
4. File audio circulaire : le rendu APU ne doit jamais attendre que l’UART se vide.
5. Tâche transport audio séparée avec compteur d’underrun/overflow.
6. UI et journal série à fréquence réduite pendant le jeu.
7. Frameskip automatique uniquement si le budget de 16 742 µs est dépassé ; option Off/Auto/1 dans le menu.

Le passage à une tâche dédiée n’est validé qu’après vérification que les callbacks Walnut-CGB et le pilote d’écran ne sont pas appelés simultanément depuis deux contextes.

### Audio à améliorer

#### Palier A — sans nouveau câblage

- porter l’UART écran↔Teensy à 921600 bauds, après test d’erreurs sur câble réel ;
- protocole audio V2 avec longueur 16 bits, numéro de séquence et compteur de pertes ;
- cible 22,05 kHz mono 16 bits ou stéréo 8 bits ;
- tampon TX côté ESP32 et tampon RX côté Teensy ;
- réglage volume jeu, mute et mesure des paquets perdus ;
- conserver le flux 8 kHz actuel comme mode secours.

#### Palier B — recherche AZ-3 uniquement

Cette piste est exclue du boîtier AZ-2 et conservée pour l’AZ-3 : étudier un moteur APU Game Boy sur une cartouche AZ-BUS. l’écran enverrait les écritures de registres APU horodatées et le module générerait du 44,1 kHz stéréo renvoyé au Teensy par I2S. Cette voie peut produire un son bien supérieur sans envoyer du PCM sur l’UART écran, mais elle vient après le rack AZ-VA1 et exige une synchronisation précise.

Le module externe ne doit pas exécuter toute la console : renvoyer 160×144×16 bits à 59,7 Hz demanderait environ 2,75 Mo/s hors overhead, incompatible avec l’UART actuel. CPU et vidéo restent donc sur l’ESP32-S3 de l’écran.

### Vidéo à améliorer

- conserver l’échelle entière ×3 : 160×144 devient exactement 480×432 ;
- rendre le frameskip configurable et mesurer son besoin réel ;
- proposer palettes DMG : verte, gris neutre, ambre et bleu AZ-2 ;
- option scanlines légère, désactivée par défaut ;
- capture d’écran PNG/BMP vers SD hors chemin temps réel ;
- overlay facultatif : FPS logique, FPS vidéo, temps frame et audio underruns ;
- ne pas ajouter de filtre bilinéaire coûteux : l’esthétique pixel nette correspond mieux à l’écran et au projet.

### Nouveau menu JEUX

#### Bibliothèque

- onglets Tous, GB, GBC, Favoris et Récents ;
- tri alphabétique ;
- nom complet, type de cartouche, présence d’une sauvegarde et dernière ouverture ;
- pagination sans limite arbitraire à 40 : index SD paginé ou dynamique ;
- jaquettes optionnelles chargées à la demande, jamais pendant l’émulation ;
- écran d’erreur précis pour ROM, mapper, PSRAM ou sauvegarde.

#### Menu en jeu sur le bouton D

- Reprendre ;
- Sauvegarder la RAM cartouche maintenant ;
- Réinitialiser la console ;
- Palette DMG ;
- Frameskip Off/Auto/1 ;
- Son : volume/mute/qualité ;
- Afficher les performances ;
- Sampler REC/STOP et accès au dernier sample ;
- Quitter vers la bibliothèque.

Le bouton C garde la sortie rapide actuelle. D ouvre le menu pause afin de ne pas sacrifier A, B, Start ou Select.

### Sauvegardes

Trois niveaux à distinguer :

1. RAM cartouche .sav : existe, ajouter sauvegarde périodique sûre et commande manuelle.
2. État instantané : nécessite une sérialisation explicite du cœur, de la RAM, des registres et de l’APU ; ne jamais écrire brutalement la structure C contenant des pointeurs.
3. Reprise récente : mémoriser dernière ROM, palette, réglages et présence du .sav, sans démarrage automatique imposé.

### Validation et décision sur le cœur

Walnut-CGB reste le cœur V1 parce qu’il est déjà intégré, supporte GB/GBC et fournit les callbacks nécessaires. Il n’est remplacé ou doublé qu’après résultats mesurés.

Tests requis :

- ROM de test CPU/timers/instructions ;
- ROM de test PPU et palettes CGB ;
- ROM de test APU ;
- au moins 5 homebrews GB et 5 GBC de tailles/mappers différents ;
- session continue de 30 minutes ;
- sauvegarde, sortie, recharge et vérification du .sav ;
- stress tactile/boutons/REC pendant le jeu ;
- mesure à 100 %, frameskip Off puis Auto.

Critère de sortie : 59,7275 Hz logique stable, audio sans coupure, commandes complètes, sauvegarde fiable, aucune fuite PSRAM après dix chargements et menu pause utilisable.

### Ordre de livraison

| Priorité | Lot | Résultat attendu |
| ---: | --- | --- |
| 1 | Instrumentation | statistiques réelles au lieu d’impressions |
| 2 | Ordonnanceur | vitesse logique officielle stable |
| 3 | Audio UART V2 | son nettement meilleur et tamponné |
| 4 | Menu pause D | réglages accessibles sans quitter brutalement |
| 5 | Bibliothèque V2 | ROMs classées, récentes, favorites et diagnostics |
| 6 | Validation | matrice de compatibilité GB/GBC |
| 7 | Recherche AZ-3 séparée | éventuel APU externe, sans modifier l’AZ-2 |

### Hors périmètre immédiat

- GBA : ne pas la promettre sur cet ESP32-S3 tant que GB/GBC ne sont pas parfaits ;
- NES/multi-console : après validation GB/GBC ;
- ROM commerciales : aucune fournie dans le dépôt ; utiliser homebrews, domaine public ou dumps personnels.

## Décision matérielle 2026-09-18

Les photos du prototype confirment que le boîtier AZ-2 est plein. L’amélioration de l’émulation doit utiliser exclusivement l’ESP32-S3 écran, le Teensy 4.1, leur UART existant, la SD et le PCM5102A déjà montés. Aucun ESP8266, ESP32 moteur, rack AZ-BUS, nouvelle carte ou déplacement de commande n’entre dans ce chantier. Les idées d’APU externe sont reportées à l’AZ-3.
