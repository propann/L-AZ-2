// AZ-2 - Emulateur Game Boy / Game Boy Color (mode JEUX), demande le
// 2026-09-15 ("y'a rien dans jeux, c'est le moment de mettre
// l'emulateur"). Coeur : Walnut-CGB (vendored dans walnut_cgb/, licence
// MIT -- voir docs/AZ2_EMULATION_JEUX.md pour la decision technique
// complete). Ce module fait le pont entre le coeur (callbacks purs, pas
// d'affichage integre) et notre Arduino_GFX -- voir gb_emulator.cpp.
//
// GBA ecarte (voir AZ2_EMULATION_JEUX.md, chiffres de perf ~20fps sur
// ESP32-S3, pas fluide, pas de coeur single-header propre disponible) --
// seul GB/GBC est implemente ici.
#pragma once

#include <Arduino.h>

// Cherche un fichier .gb/.gbc dans /games sur la carte SD, le charge en
// PSRAM et demarre l'emulation. Renvoie false (avec un message d'erreur
// clair sur Serial) si aucune carte/ROM n'est trouvee -- pas de crash,
// juste un echec propre (voir docs/AZ2_EMULATION_JEUX.md, "reste a
// faire": pas de ROM legale ni de carte SD physiquement testee encore).
bool gbLoadFirstRom();

// Renvoie true si une ROM est chargee et prete a tourner (gbRunFrame()
// peut etre appelee).
bool gbIsLoaded();

// Execute UNE frame d'emulation (~16,7ms de temps de jeu Game Boy) et
// dessine son resultat a l'ecran (voir gbBlitLine() dans le .cpp, appele
// en interne par le coeur via lcd_draw_line). A appeler en boucle depuis
// loop() tant que la page JEUX est affichee.
void gbRunFrame();

// Decharge la ROM courante (appele en quittant la page JEUX) -- libere
// la PSRAM utilisee.
void gbUnload();

// Direction/bouton Game Boy standard, correspond a NAV:/BTN: du Teensy
// (voir handleTeensyLine() dans main.cpp) -- reutilise la croix/boutons
// physiques deja cables, voir AZ2_CABLAGE_MASTER.md.
enum class GbButton : uint8_t { Up, Down, Left, Right, A, B, Select, Start };
void gbSetButton(GbButton button, bool pressed);

// Nom (titre ROM, 16 caracteres max + fin de chaine) de la ROM chargee,
// pour affichage sur la page JEUX. Chaine vide si rien de charge.
const char *gbRomTitle();

// Implementee dans main.cpp (seul endroit qui connait `gfx`) : dessine
// une ligne Game Boy (160 pixels RGB565 deja convertis) a l'ecran,
// mise a l'echelle. Appelee par le coeur via lcd_draw_line, jamais a
// appeler directement en dehors de gb_emulator.cpp.
void gbBlitLine(int line, const uint16_t *row);
