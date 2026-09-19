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

// Noms complets utilises comme identifiants de fichier SD, PAS les libelles
// tronques pour l'affichage : 87 octets max pour rester dans le chemin
// /games/<name> (kSavePathCapacity=96 cote emulation). Les noms plus
// longs sont exclus du scan avec un diagnostic, jamais tronques.
constexpr uint8_t kGbRomNameLen = 88;
// Nombre maximum d'entrees du navigateur ROM. 100 reste sous la limite
// int8_t du curseur UI et coute ~8,8 Ko pour les noms complets.
constexpr uint8_t kGbMaxRoms = 100;

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

// Telemetrie legere mise a jour pendant l'emulation. Les valeurs servent
// au diagnostic sur materiel ; elles ne constituent pas a elles seules
// une certification de compatibilite d'une ROM.
struct GbRuntimeStats {
  uint16_t fpsX10 = 0;          // cadence observee x10 sur ~1 seconde
  uint32_t avgWorkUs = 0;       // CPU emulation + paquet audio, moyenne fenetre
  uint32_t maxWorkUs = 0;       // pire frame de la fenetre
  uint32_t totalFrames = 0;
  uint16_t autosaveFailures = 0;
};
GbRuntimeStats gbRuntimeStats();

// Tente de sauvegarder la RAM modifiee puis decharger la ROM.
// Renvoie false si la SD refuse la sauvegarde : la ROM reste chargee
// pour permettre un nouvel essai sans perdre la progression LSDJ.
bool gbUnload();

// Sauvegarde manuelle de la SRAM de la cartouche courante. No-op reussi
// si rien n'a change ou si la cartouche n'a pas de RAM persistante.
bool gbSaveNow();

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
