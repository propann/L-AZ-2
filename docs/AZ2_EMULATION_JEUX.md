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

## Alternative etudiee : Anemoia-ESP32

https://github.com/Shim06/Anemoia-ESP32 -- emulateur NES, licence GPLv3.

| Critere | Retro-Go | Anemoia-ESP32 |
| --- | --- | --- |
| Framework | ESP-IDF (CMake) | **Arduino** (sketch .ino) -- meme framework que nous |
| Consoles | NES/SNES/GB/GBC/Megadrive/PCEngine/Lynx/DOOM... | NES seulement |
| Ecran attendu | SPI (ILI9341 family) | SPI (ST7789/ILI9341) via TFT_eSPI |
| PSRAM requise | Selon coeur | **Non requise** (confirme par le depot) |
| Taille du coeur | Gros projet complet | ~400 lignes pour le fichier principal -- tres compact |
| Acces framebuffer brut | Pas verifie | **Oui** : `nes.connectFramebuffer(cv_framebuffer)` existe (chemin video composite), a cote du chemin `nes.connectScreen(&screen)` couple a TFT_eSPI |
| Integrable sans double demarrage | Non (ESP-IDF) | **Oui, potentiellement** -- en utilisant `connectFramebuffer()` a la place de `connectScreen()`, on recupere une image brute a blitter nous-memes via Arduino_GFX (notre pilote deja fonctionnel), sans jamais toucher a TFT_eSPI |

**Conclusion** : aucun des deux ne tourne "out of the box" sur notre
ecran RGB parallele -- mais Anemoia-ESP32 a un point d'accroche
(`connectFramebuffer()`) qui permet de rester dans NOTRE firmware Arduino
existant, sans double demarrage ni deuxieme toolchain. Retro-Go reste le
plus complet (8+ consoles) mais coute un vrai chantier (OTA + pilote
ecran ESP-IDF) largement plus lourd.

## Decision

- **v0 du mode JEUX** : porter Anemoia-ESP32 (NES seul) comme une PAGE de
  plus dans `src_esp32/az2_screen` (menu -> "JEUX"), pas un double
  demarrage. On recupere son coeur CPU/PPU/APU, on jette sa partie
  TFT_eSPI/UI, on branche `connectFramebuffer()` sur un buffer qu'on
  blitte via `gfx->draw16bitRGBBitmap()` (ou equivalent Arduino_GFX)
  chaque frame. Pas de deuxieme partition, pas de reboot -- juste un
  ecran de plus, coherent avec l'architecture actuelle.
- **Retro-Go** reste une option pour PLUS TARD (vrai double demarrage
  OTA + pilote RGB parallele ecrit a la main) si on veut plus que la NES
  un jour -- pas prioritaire tant que le coeur groovebox (moteurs,
  sequenceur, Pico) n'est pas solide. Le dossier vendored reste dans le
  depot pour reference mais n'est PAS ce qu'on va porter en premier.

## Ce qu'il reste a faire (rien fait ci-dessous, hardware debranche)

1. Choisir/obtenir une ROM NES **legale** pour les tests (homebrew ou
   domaine public -- pas de ROM commerciale dans le depot/le firmware).
2. Vendorer le coeur Anemoia-ESP32 (extraire CPU/PPU/APU, retirer
   TFT_eSPI/SdFat/UI), l'adapter en bibliotheque utilisable depuis
   `az2_screen` (namespace/fichiers propres, pas un .ino monolithique).
3. Ecrire l'adaptateur framebuffer -> `Arduino_GFX` (probablement
   `draw16bitRGBBitmap()`, a verifier le format de pixel expose par
   `connectFramebuffer()` -- RGB565 direct ou palette a convertir).
4. Mapper les entrees : pads Pico (matrice 4x4) ou tactile ecran vers les
   boutons NES (croix directionnelle + A/B/Start/Select = 6 boutons,
   tient sur les 16 pads avec de la marge).
5. Chargement ROM : depend de la carte SD ESP32 (pas encore presente,
   voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md etape 7) -- sans SD, on ne peut
   tester qu'avec une ROM embarquee en PROGMEM (petite, homebrew).
6. Nouvelle entree menu "JEUX" dans `kMenuItems[]`, page dediee
   (`Screen::Retro` ou similaire).
7. Tester en reel une fois le hardware rebranche -- rien ci-dessus n'est
   flashe/verifie, uniquement de la recherche/architecture.

## Sources consultees

- Retro-Go: https://github.com/ducalex/retro-go
- Retro-Go supported devices / CYD: recherche web (pas de source unique
  autoritative trouvee au-dela du README du depot)
- Anemoia-ESP32: https://github.com/Shim06/Anemoia-ESP32
