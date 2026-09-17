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

// Selecteur de ROM (demande le 2026-09-15, "il nous faut un menu pour
// demarrer la rom qu'on choisit dans une liste"). Longueur de nom
// generereuse (255.3 = format 8.3 le plus long en FAT court, mais les
// vraies cartes exposent des noms longs -- 40 caracteres suffit pour
// rester lisible a l'ecran de toute facon, tronque au-dela).
constexpr uint8_t kGbRomNameLen = 40;
// Releve de 16 a 40 le 2026-09-17 (demande "met en plus des trucs cool
// ... genre 20 30") -- 16 coupait silencieusement le scan avant de
// trouver toute une collection perso plus fournie. Voir kRomVisibleRows
// dans main.cpp pour la pagination a l'ecran (8 lignes visibles a la
// fois, un peu de marge au-dela de kGbMaxRoms).
constexpr uint8_t kGbMaxRoms = 40;

// Scanne /games sur la carte SD pour les fichiers .gb/.gbc (jusqu'a
// kGbMaxRoms), remplit `names` (kGbMaxRoms x kGbRomNameLen, deja
// alloue par l'appelant) avec les noms de fichiers trouves. Renvoie le
// nombre trouve (0 si pas de carte/dossier/fichier).
uint8_t gbScanRoms(char names[][kGbRomNameLen]);

// Charge et demarre le fichier /games/<filename> (nom tel que renvoye
// par gbScanRoms()). Renvoie false (message Serial clair) en cas
// d'echec (fichier illisible, cartouche non supportee, PSRAM
// insuffisante...) -- pas de crash.
bool gbLoadRom(const char *filename);

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
