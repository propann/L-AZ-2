// AZ-2 - Pont entre le coeur Walnut-CGB (walnut_cgb/walnut_cgb.h,
// callbacks purs, licence MIT) et notre materiel : ROM chargee depuis la
// carte SD vers la PSRAM du module ESP32-S3 (8 Mo, meme puce que
// l'ecran -- voir platformio.ini "qio_opi"/"module WROOM-1 N16R8"), puis
// affichee via gbBlitLine() (implementee dans main.cpp, seul endroit qui
// connait `gfx`).
//
// Son : PAS FAIT dans cette premiere version (ENABLE_SOUND=0) -- notre
// architecture audio est centree sur le Teensy (voir
// src_teensy/az2_audio), brancher le son GB dessus demanderait de
// streamer les echantillons APU par l'UART existant (230400 bauds, pas
// concu pour de l'audio temps reel) ou un vrai DAC/ampli sur l'ESP32
// lui-meme -- a designer plus tard, note dans AZ2_EMULATION_JEUX.md.
// Video seule pour l'instant, comme un GB muet.

#include "gb_emulator.h"

#define ENABLE_LCD 1
#define ENABLE_SOUND 0
#include "walnut_cgb/walnut_cgb.h"

#include <Arduino_GFX_Library.h>  // pour la macro RGB565() (palette DMG)
#include <SD.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace {

struct gb_s gb;
bool romLoaded = false;
uint8_t *romData = nullptr;
uint32_t romSize = 0;
uint8_t *cartRam = nullptr;
uint32_t cartRamSize = 0;
char romTitle[17] = {0};
// Chemin de sauvegarde (cart RAM) pour la ROM courante, meme nom que la
// ROM avec l'extension remplacee par .sav, a cote d'elle dans /games --
// convention classique d'emulateur (rom.gb + rom.sav). Vide si aucune
// ROM chargee ou si la cartouche n'a pas de RAM (cartRamSize==0).
char saveRamPath[64] = {0};

uint8_t romRead(struct gb_s *, const uint_fast32_t addr) {
  return (addr < romSize) ? romData[addr] : 0xFF;
}

uint16_t romRead16(struct gb_s *, const uint_fast32_t addr) {
  if (addr + 1 >= romSize) return 0xFFFF;
  return static_cast<uint16_t>(romData[addr]) | (static_cast<uint16_t>(romData[addr + 1]) << 8);
}

uint32_t romRead32(struct gb_s *, const uint_fast32_t addr) {
  if (addr + 3 >= romSize) return 0xFFFFFFFF;
  return static_cast<uint32_t>(romData[addr]) | (static_cast<uint32_t>(romData[addr + 1]) << 8) |
         (static_cast<uint32_t>(romData[addr + 2]) << 16) | (static_cast<uint32_t>(romData[addr + 3]) << 24);
}

uint8_t cartRamRead(struct gb_s *, const uint_fast32_t addr) {
  return (addr < cartRamSize) ? cartRam[addr] : 0xFF;
}

void cartRamWrite(struct gb_s *, const uint_fast32_t addr, const uint8_t val) {
  if (addr < cartRamSize) {
    cartRam[addr] = val;
  }
}

void gbErrorCallback(struct gb_s *, const enum gb_error_e err, const uint16_t addr) {
  Serial.print("GB:ERROR:code=");
  Serial.print(static_cast<int>(err));
  Serial.print(":addr=0x");
  Serial.println(addr, HEX);
}

// Palette DMG (jeux noir et blanc, pas de vraie couleur) -- vert
// classique d'ecran Game Boy original, 4 teintes du plus clair (0) au
// plus fonce (3).
constexpr uint16_t kDmgPalette[4] = {
    RGB565(224, 248, 208), RGB565(136, 192, 112), RGB565(52, 104, 86), RGB565(8, 24, 32),
};

// Sauvegarde (voir saveRamPath ci-dessus) -- convention .sav a cote de
// la ROM sur la carte SD. Demande 2026-09-15 ("sauvegarde tout") :
// ecrit au moment de decharger la ROM (voir gbUnload()), relu au
// chargement suivant si le fichier existe deja (voir gbLoadRom()). Pas
// d'ecriture pendant le jeu (juste a la sortie) -- suffisant pour une
// sauvegarde a l'extinction/au changement de jeu, pas de risque
// d'ecriture SD en boucle pendant que ca joue.
void gbSaveCartRam() {
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    return;
  }
  File f = SD.open(saveRamPath, FILE_WRITE);
  if (!f) {
    Serial.print("GB:SAVE_OPEN_ERROR:");
    Serial.println(saveRamPath);
    return;
  }
  const size_t written = f.write(cartRam, cartRamSize);
  f.close();
  if (written != cartRamSize) {
    Serial.println("GB:SAVE_WRITE_ERROR");
  } else {
    Serial.print("GB:SAVED:");
    Serial.println(saveRamPath);
  }
}

void gbLoadCartRamIfPresent() {
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    return;
  }
  if (!SD.exists(saveRamPath)) {
    return;  // pas de sauvegarde existante -- cartRam reste a 0xFF (deja initialise), rien d'anormal
  }
  File f = SD.open(saveRamPath);
  if (!f) {
    Serial.print("GB:SAVE_READ_OPEN_ERROR:");
    Serial.println(saveRamPath);
    return;
  }
  const size_t readBytes = f.read(cartRam, cartRamSize);
  f.close();
  Serial.print("GB:SAVE_LOADED:");
  Serial.print(saveRamPath);
  Serial.print(":bytes=");
  Serial.println(readBytes);
}

// lcd_draw_line du coeur : convertit les 160 pixels de la ligne en RGB565
// (CGB : index direct dans gb->cgb.fixPalette deja converti par le coeur
// ; DMG : 2 bits de teinte -> kDmgPalette) et transmet a gbBlitLine()
// (main.cpp) qui fait le rendu a l'ecran.
void lcdDrawLine(struct gb_s *pgb, const uint8_t *pixels, const uint_fast8_t line) {
  static uint16_t row[160];
  const bool cgbMode = pgb->cgb.cgbMode != 0;
  for (uint8_t x = 0; x < 160; ++x) {
    if (cgbMode) {
      row[x] = pgb->cgb.fixPalette[pixels[x] & 0x3F];
    } else {
      row[x] = kDmgPalette[pixels[x] & 0x03];
    }
  }
  gbBlitLine(line, row);
}

}  // namespace

bool gbIsLoaded() {
  return romLoaded;
}

void gbUnload() {
  if (romLoaded) {
    gbSaveCartRam();  // avant de liberer cartRam -- voir saveRamPath
  }
  if (romData != nullptr) {
    heap_caps_free(romData);
    romData = nullptr;
  }
  if (cartRam != nullptr) {
    heap_caps_free(cartRam);
    cartRam = nullptr;
  }
  romLoaded = false;
  romTitle[0] = '\0';
  saveRamPath[0] = '\0';
}

uint8_t gbScanRoms(char names[][kGbRomNameLen]) {
  uint8_t count = 0;

  if (!SD.exists("/games")) {
    Serial.println("GB:NO_GAMES_DIR");
    return 0;
  }
  File dir = SD.open("/games");
  if (!dir || !dir.isDirectory()) {
    Serial.println("GB:NO_GAMES_DIR");
    return 0;
  }

  for (File entry = dir.openNextFile(); entry && count < kGbMaxRoms; entry = dir.openNextFile()) {
    const String name = entry.name();
    if (!entry.isDirectory() && (name.endsWith(".gb") || name.endsWith(".gbc") ||
                                  name.endsWith(".GB") || name.endsWith(".GBC"))) {
      // entry.name() peut renvoyer le chemin complet ("/games/xxx.gb")
      // selon la version de la lib SD -- ne garder que le nom de fichier.
      const int slash = name.lastIndexOf('/');
      const String base = (slash >= 0) ? name.substring(slash + 1) : name;
      strncpy(names[count], base.c_str(), kGbRomNameLen - 1);
      names[count][kGbRomNameLen - 1] = '\0';
      ++count;
    }
    entry.close();
  }
  dir.close();

  if (count == 0) {
    Serial.println("GB:NO_ROM_FOUND");
  }
  return count;
}

bool gbLoadRom(const char *filename) {
  gbUnload();

  char path[64];
  snprintf(path, sizeof(path), "/games/%s", filename);
  File romFile = SD.open(path);
  if (!romFile) {
    Serial.print("GB:ROM_OPEN_ERROR:");
    Serial.println(path);
    return false;
  }

  romSize = romFile.size();
  romData = static_cast<uint8_t *>(heap_caps_malloc(romSize, MALLOC_CAP_SPIRAM));
  if (romData == nullptr) {
    Serial.println("GB:ROM_TOO_BIG_FOR_PSRAM");
    romFile.close();
    return false;
  }
  const size_t readBytes = romFile.read(romData, romSize);
  romFile.close();
  if (readBytes != romSize) {
    Serial.println("GB:ROM_READ_ERROR");
    heap_caps_free(romData);
    romData = nullptr;
    return false;
  }

  const enum gb_init_error_e initErr =
      gb_init(&gb, romRead, romRead16, romRead32, cartRamRead, cartRamWrite, gbErrorCallback, nullptr);
  if (initErr != GB_INIT_NO_ERROR) {
    Serial.print("GB:INIT_ERROR:code=");
    Serial.println(static_cast<int>(initErr));
    heap_caps_free(romData);
    romData = nullptr;
    return false;
  }

  cartRamSize = static_cast<uint32_t>(gb.num_ram_banks) * CRAM_BANK_SIZE;
  if (cartRamSize > 0) {
    cartRam = static_cast<uint8_t *>(heap_caps_malloc(cartRamSize, MALLOC_CAP_SPIRAM));
    if (cartRam != nullptr) {
      memset(cartRam, 0xFF, cartRamSize);
      // Chemin de sauvegarde = meme nom que la ROM, extension .sav (voir
      // saveRamPath) -- tronque a la premiere extension trouvee, gere
      // .gb comme .gbc.
      snprintf(saveRamPath, sizeof(saveRamPath), "/games/%s", filename);
      char *dot = strrchr(saveRamPath, '.');
      if (dot != nullptr) {
        strcpy(dot, ".sav");
      }
      gbLoadCartRamIfPresent();
    } else {
      cartRamSize = 0;  // pas de sauvegarde possible, mais on continue sans planter
    }
  }

  gb_init_lcd(&gb, lcdDrawLine);
  gb.direct.joypad = 0xFF;  // rien de presse (voir gbSetButton() -- 0=presse, 1=relache)
  // frame_skip=true : le coeur continue d'emuler CHAQUE frame a vitesse
  // normale (logique/timing du jeu corrects), mais n'appelle
  // lcd_draw_line() qu'une frame sur deux -- demande le 2026-09-15
  // ("on a des sauts d'images, on peut stabiliser") : le rendu (appels
  // vers le bus RGB parallele, voir gbBlitLine() dans main.cpp) est le
  // gros cout, pas l'emulation CPU -- diviser son volume par 2 stabilise
  // la cadence sans ralentir le jeu.
  gb.direct.frame_skip = true;

  gb_get_rom_name(&gb, romTitle);
  romLoaded = true;

  Serial.print("GB:LOADED:title=");
  Serial.print(romTitle);
  Serial.print(":size_kb=");
  Serial.print(romSize / 1024);
  Serial.print(":ram_banks=");
  Serial.println(gb.num_ram_banks);
  return true;
}

void gbRunFrame() {
  if (romLoaded) {
    gb_run_frame(&gb);
  }
}

void gbSetButton(GbButton button, bool pressed) {
  if (!romLoaded) {
    return;
  }
  uint8_t mask = 0;
  switch (button) {
    case GbButton::Up: mask = JOYPAD_UP; break;
    case GbButton::Down: mask = JOYPAD_DOWN; break;
    case GbButton::Left: mask = JOYPAD_LEFT; break;
    case GbButton::Right: mask = JOYPAD_RIGHT; break;
    case GbButton::A: mask = JOYPAD_A; break;
    case GbButton::B: mask = JOYPAD_B; break;
    case GbButton::Select: mask = JOYPAD_SELECT; break;
    case GbButton::Start: mask = JOYPAD_START; break;
  }
  // Registre joypad Game Boy actif-bas : 0 = presse, 1 = relache.
  if (pressed) {
    gb.direct.joypad &= static_cast<uint8_t>(~mask);
  } else {
    gb.direct.joypad |= mask;
  }
}

const char *gbRomTitle() {
  return romTitle;
}
