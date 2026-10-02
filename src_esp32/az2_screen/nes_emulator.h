// AZ-2 - Emulateur NES (mode JEUX, carte NES de la page EMULATEURS).
// Coeur : Anemoia-ESP32 (nes_core/, licence GPLv3 -- voir
// nes_core/LICENSE-anemoia, compatible avec ce depot contrairement a
// Nofrendo/esp32-nesemu qui est GPLv2-only). Ce module fait le pont entre
// le coeur (callback de dessin par bandes de 8 lignes, pas d'affichage
// integre) et notre Arduino_GFX -- voir nes_emulator.cpp et
// docs/AZ2_ETAT_ACTUEL.md pour l'etude complete.
//
// [2026-09-27] Premiere passe : blit synchrone (comme le tout premier GB
// avant le decoupage double coeur). Le decoupage AZ2_GB_DUAL_CORE_BLIT
// est directement reutilisable plus tard -- ppu2C02 appelle son callback
// de dessin tous les SCANLINES_PER_BUFFER=8 lignes (256 px large), exact
// meme motif que gbBlitLine(), voir nes_core/ppu2C02.h/.cpp.
#pragma once

#include <Arduino.h>

// Meme convention que kGbRomNameLen/kGbMaxRoms (gb_emulator.h).
constexpr uint8_t kNesRomNameLen = 160;
constexpr uint8_t kNesMaxRoms = 100;

// Scanne recursivement /games pour les fichiers .nes (jusqu'a
// kNesMaxRoms), remplit `names` avec les chemins relatifs a /games
// (ex. "NES/jeu.nes"). Renvoie le nombre trouve.
uint8_t nesScanRoms(char names[][kNesRomNameLen]);

// Charge et demarre /games/<filename>. Renvoie false (message Serial clair)
// si le mapper de la cartouche n'est pas supporte (voir nes_core/cartridge.cpp,
// mappers 0/1/2/3/4/69 seulement) ou si le fichier est illisible.
bool nesLoadRom(const char *filename);

bool nesIsLoaded();

// Execute UNE frame (~16,7ms de temps de jeu NES, NTSC ~60,0988 Hz) et
// dessine son resultat a l'ecran. A appeler en boucle depuis loop() tant
// que la carte NES est active.
void nesRunFrame();

bool nesUnload();

// Sauvegarde SRAM de la cartouche dans /games/<rom>.sav.
bool nesSaveRam();

#ifdef AZ2_NES_DUAL_CORE
// Lance la tache APU sur le core 0. Le core 1 garde CPU/PPU et l'interface.
void nesInitDualCore();
#endif

// Serial1 est partage entre les commandes UI et les paquets audio NES.
// Ces deux petites fonctions serialisent les ecritures quand l'APU est sur
// l'autre coeur ; elles sont no-op sur une cible mono-coeur.
void nesSerial1Lock();
void nesSerial1Unlock();

const char *nesRomTitle();

// Meme ordre de bits que le registre controleur NES standard ($4016) :
// A=bit0, B=bit1, Select=bit2, Start=bit3, Up=bit4, Down=bit5, Left=bit6,
// Right=bit7 -- voir nes_core/bus.cpp (Bus::controller).
enum class NesButton : uint8_t { A, B, Select, Start, Up, Down, Left, Right };
void nesSetButton(NesButton button, bool pressed);

struct NesRuntimeStats {
  uint16_t fpsX10 = 0;
  uint32_t avgCoreUs = 0;
  uint32_t avgDisplayUs = 0;
  uint32_t maxCoreUs = 0;
  uint32_t totalFrames = 0;
  uint32_t missed = 0;
};
NesRuntimeStats nesRuntimeStats();

// Implementee dans main.cpp (seul endroit qui connait `gfx`, meme raison
// que gbBlitLine()) : dessine une bande de 8 lignes NES (256 pixels RGB565
// par ligne, deja convertis par le coeur) a l'ecran, mise a l'echelle.
// bandIndex va de 0 a 29 (240/8 bandes par frame), remis a 0 par
// nesRunFrame() avant chaque nesCpu.clockFrame(). Appelee par le coeur via
// son draw_callback (voir nes_core/ppu2C02.h/.cpp), jamais a appeler
// directement en dehors de nes_emulator.cpp.
void nesBlitBand(int bandIndex, const uint16_t *pixels);
