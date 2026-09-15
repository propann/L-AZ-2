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
3. **[FAIT]** ROM cherchee dans `/games/*.gb`/`.gbc` sur la carte SD
   (premier fichier trouve) -- chargee en entrant sur la page JEUX,
   dechargee en la quittant (libere la PSRAM). Echec propre (pas de
   crash) si pas de carte/dossier/fichier -- page d'attente avec le
   message exact.
4. **[FAIT]** Entrees mappees sur la croix + boutons A/B/C/D deja cables
   sur le Teensy (`NAV:`/`BTN:`, voir AZ2_CABLAGE_MASTER.md) : croix ->
   directions Game Boy, A/B -> A/B, C/D -> SELECT/START.
5. **[FAIT]** Page JEUX (`Screen::Retro`) mise a jour : affiche le jeu
   si une ROM est chargee (`gbRunFrame()` appelee depuis `loop()`,
   cadencee a ~59,7 images/s -- cible theorique, PAS mesuree en reel
   faute de ROM disponible), sinon la raison exacte de l'echec + "touche
   pour reessayer" (utile apres avoir insere une carte SD).
6. **PAS FAIT** : le son (`ENABLE_SOUND=0` dans `gb_emulator.cpp`) --
   notre architecture audio est centree sur le Teensy, brancher l'APU
   Game Boy dessus demande de reflechir a un chemin (streamer par l'UART
   existant, pas concu pour ca, ou un DAC/ampli propre a l'ESP32) --
   remis a plus tard, video seule pour l'instant.
7. **PAS FAIT** : sauvegardes cart RAM persistees sur SD (actuellement
   perdues a l'extinction).
8. **Reste a faire pour du vrai jeu** : une ROM GB/GBC **legale**
   (homebrew ou domaine public -- pas de ROM commerciale fournie) sur
   une carte SD formatee FAT32, dossier `/games/`. Sans ca, la page
   affiche juste l'etat "pas de ROM" -- **rien de tout ce qui precede
   n'a ete verifie en conditions de jeu reelles**, seulement compile et
   flashe sans crash (voir le firmware, testable des que SD+ROM
   existent).

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
