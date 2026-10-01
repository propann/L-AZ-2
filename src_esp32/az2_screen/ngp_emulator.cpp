// Pont AZ-2 vers le coeur RACE NGP/NGPC.
// Le coeur garde sa notice GPLv2 dans az2_ngp_race/race/license.txt.

#include "ngp_emulator.h"

#include <AZ2_Protocol.h>
#include <SD.h>
#include <esp_heap_caps.h>

#define RETRO_COMPAT_IMPLEMENTATION
extern "C" {
#include "../az2_ngp_race/race/flash.h"
#include "../az2_ngp_race/race/graphics.h"
#include "../az2_ngp_race/race/input.h"
#include "../az2_ngp_race/race/race-memory.h"
#include "../az2_ngp_race/race/sound.h"
#include "../az2_ngp_race/race/tlcs900h.h"
#include "../az2_ngp_race/race/cz80_support.h"
#include "../az2_ngp_race/race/neopopsound.h"
#include "../az2_ngp_race/race/retro_compat.h"
#include "../az2_ngp_race/ngp_storage.h"
}

// Symboles attendus par RACE.
unsigned short *drawBuffer = nullptr;
volatile unsigned g_frame_ready = 0;
int is_mono_game = 0;
int tipo_consola = 0;
char retro_save_directory[3] = {};
int gfx_hacks = 0;
int m_bIsActive = 1;

unsigned char *rasterY = nullptr;
unsigned char *frame0Pri = nullptr;
unsigned char *frame1Pri = nullptr;
unsigned char *color_switch = nullptr;
unsigned char *scanlineY = nullptr;
unsigned char *scrollSpriteX = nullptr;
unsigned char *scrollSpriteY = nullptr;
unsigned char *sprite_palette_numbers = nullptr;
unsigned char *sprite_table = nullptr;
unsigned short *patterns = nullptr;
unsigned char *oowSelect = nullptr;
unsigned short *oowTable = nullptr;
unsigned char *wndTopLeftY = nullptr;
unsigned char *wndSizeY = nullptr;
unsigned char *wndSizeX = nullptr;
unsigned char *bgSelect = nullptr;
unsigned char *bw_palette_table = nullptr;
unsigned short *palette_table = nullptr;
unsigned short *bgTable = nullptr;
unsigned char *wndTopLeftX = nullptr;
unsigned char *scrollFrontY = nullptr;
unsigned char *scrollFrontX = nullptr;
unsigned short *tile_table_front = nullptr;
unsigned char *scrollBackY = nullptr;
unsigned short *tile_table_back = nullptr;
unsigned char *scrollBackX = nullptr;
unsigned char *pattern_table = nullptr;

extern "C" int Cz80_allocate_flag_tables(void);
extern "C" int finscan;

namespace {

uint8_t *rom = nullptr;
size_t romSize = 0;
uint16_t *frame = nullptr;
char savePath[kNgpRomNameLen + sizeof("/games/") + sizeof(".ngf")] = {};
char romTitle[17] = {};
uint8_t buttonState = 0;
bool loaded = false;

uint32_t statsWindowStartUs = 0;
uint32_t statsFrames = 0;
uint32_t statsCoreAccumUs = 0;
uint32_t statsCoreMaxUs = 0;
uint32_t statsTotalFrames = 0;
NgpRuntimeStats stats;

void mapVdpTables() {
  sprite_table = (unsigned char *)get_address(0x00008800);
  pattern_table = (unsigned char *)get_address(0x0000A000);
  patterns = (unsigned short *)pattern_table;
  tile_table_front = (unsigned short *)get_address(0x00009000);
  tile_table_back = (unsigned short *)get_address(0x00009800);
  palette_table = (unsigned short *)get_address(0x00008200);
  bw_palette_table = (unsigned char *)get_address(0x00008100);
  sprite_palette_numbers = (unsigned char *)get_address(0x00008C00);
  scanlineY = (unsigned char *)get_address(0x00008009);
  frame0Pri = (unsigned char *)get_address(0x00008000);
  frame1Pri = (unsigned char *)get_address(0x00008030);
  wndTopLeftX = (unsigned char *)get_address(0x00008002);
  wndTopLeftY = (unsigned char *)get_address(0x00008003);
  wndSizeX = (unsigned char *)get_address(0x00008004);
  wndSizeY = (unsigned char *)get_address(0x00008005);
  scrollSpriteX = (unsigned char *)get_address(0x00008020);
  scrollSpriteY = (unsigned char *)get_address(0x00008021);
  scrollFrontX = (unsigned char *)get_address(0x00008032);
  scrollFrontY = (unsigned char *)get_address(0x00008033);
  scrollBackX = (unsigned char *)get_address(0x00008034);
  scrollBackY = (unsigned char *)get_address(0x00008035);
  bgSelect = (unsigned char *)get_address(0x00008118);
  bgTable = (unsigned short *)get_address(0x000083E0);
  oowSelect = (unsigned char *)get_address(0x00008012);
  oowTable = (unsigned short *)get_address(0x000083F0);
  color_switch = (unsigned char *)get_address(0x00006F91);
  rasterY = scanlineY;
}

void setDefaultsAfterBoot() {
  const uint8_t consoleType = tlcsMemReadB(0x00200023);
  tlcsMemWriteB(0x00006F91, consoleType == 0x00 ? 0x10 : consoleType);
  tlcsMemWriteB(0x00006F87, 0x01);
  tlcsMemWriteB(0x00004000, tlcsMemReadB(0x00004000) | 0xC0);
  tlcsMemWriteB(0x00006F84, 0x40);
  tlcsMemWriteB(0x00006F85, 0x00);
  tlcsMemWriteB(0x00006F86, 0x00);
}

bool endsWithNgp(const String &name) {
  return name.endsWith(".ngp") || name.endsWith(".NGP") ||
         name.endsWith(".ngc") || name.endsWith(".NGC");
}

void scanDir(File &dir, const String &prefix, char names[][kNgpRomNameLen], uint8_t &count) {
  for (File entry = dir.openNextFile(); entry && count < kNgpMaxRoms;
       entry = dir.openNextFile()) {
    const String full = entry.name();
    const int slash = full.lastIndexOf('/');
    const String base = slash >= 0 ? full.substring(slash + 1) : full;
    const String relative = prefix.length() ? prefix + "/" + base : base;
    if (entry.isDirectory()) {
      scanDir(entry, relative, names, count);
    } else if (endsWithNgp(full) && relative.length() < kNgpRomNameLen) {
      strncpy(names[count], relative.c_str(), kNgpRomNameLen - 1);
      names[count][kNgpRomNameLen - 1] = '\0';
      ++count;
    }
    entry.close();
  }
}

void mapRomPath(const char *filename) {
  snprintf(savePath, sizeof(savePath), "/games/%s", filename);
  char *dot = strrchr(savePath, '.');
  if (dot) strcpy(dot, ".ngf");
}

void mapButtons() { ngpInputState = buttonState; }

}  // namespace

const char *az2NgpSavePath() { return savePath; }

bool az2NgpReadFile(const char *path, void **data, size_t *size) {
  if (!path || !data || !size) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  *size = file.size();
  *data = malloc(*size);
  const bool ok = *data && file.read(static_cast<uint8_t *>(*data), *size) == *size;
  file.close();
  if (!ok) {
    free(*data);
    *data = nullptr;
    *size = 0;
  }
  return ok;
}

bool az2NgpWriteFile(const char *path, const void *data, size_t size) {
  if (!path || !data) return false;
  SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  const bool ok = file.write(static_cast<const uint8_t *>(data), size) == size;
  file.close();
  return ok;
}

void az2NgpSetSavePath(const char *path) { (void)path; }

uint8_t ngpScanRoms(char names[][kNgpRomNameLen]) {
  uint8_t count = 0;
  File dir = SD.open("/games");
  if (!dir || !dir.isDirectory()) return 0;
  scanDir(dir, "", names, count);
  dir.close();
  return count;
}

bool ngpLoadRom(const char *filename) {
  if (loaded) ngpUnload();
  if (!filename || filename[0] == '/' || strstr(filename, "..")) return false;
  char path[kNgpRomNameLen + sizeof("/games/")];
  snprintf(path, sizeof(path), "/games/%s", filename);
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  romSize = file.size();
  rom = static_cast<uint8_t *>(heap_caps_malloc(romSize, MALLOC_CAP_SPIRAM));
  if (!rom || file.read(rom, romSize) != romSize) {
    file.close();
    if (rom) free(rom);
    rom = nullptr;
    return false;
  }
  file.close();
  mapRomPath(filename);
  az2NgpSetSavePath(savePath);
  m_emuInfo.machine = NGPC;
  m_emuInfo.romSize = static_cast<int>(romSize);
  is_mono_game = romSize > 0x23 && rom[0x23] == 0x00;
  ngp_mem_set_rom(rom, romSize);
  ngp_mem_init();
  mapVdpTables();
  setFlashSize(static_cast<unsigned int>(romSize));
  flashStartup();
  Cz80_allocate_flag_tables();
  tlcs_init();
  tlcs_reinit();
  Z80_Init();
  Z80_Reset();
  audio_dac_init();
  sound_init(14000);
  setDefaultsAfterBoot();
  frame = static_cast<uint16_t *>(heap_caps_malloc(160U * 152U * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  if (!frame) return false;
  drawBuffer = frame;
  graphics_init();
  if (wndSizeX && wndSizeY && (*wndSizeX == 0 || *wndSizeY == 0)) {
    *wndTopLeftX = 0; *wndTopLeftY = 0; *wndSizeX = 160; *wndSizeY = 152;
  }
  if (bgSelect) *bgSelect |= 0x80;
  if (frame0Pri) *frame0Pri |= 0xC0;
  finscan = 198;
  // Titre court : nom de fichier sans le sous-dossier de /games.
  const char *titleBase = strrchr(filename, '/');
  strncpy(romTitle, titleBase != nullptr ? titleBase + 1 : filename, sizeof(romTitle) - 1);
  romTitle[sizeof(romTitle) - 1] = '\0';
  buttonState = 0;
  mapButtons();
  stats = {};
  statsWindowStartUs = millis() * 1000UL;
  statsFrames = statsCoreAccumUs = statsCoreMaxUs = 0;
  statsTotalFrames = 0;
  loaded = true;
  Serial.print("NGP:LOADED:");
  Serial.println(path);
  return true;
}

bool ngpIsLoaded() { return loaded; }

void ngpRunFrame() {
  if (!loaded) return;
  const uint32_t start = micros();
  g_frame_ready = 0;
  tlcs_execute(6144000 / 60, 0);
  const uint32_t elapsed = micros() - start;
  if (g_frame_ready) {
    ngpBlitFrame(frame);
    static uint8_t packet[az2::kGbAudioSamplesPerPacket];
    static uint16_t psg[az2::kGbAudioSamplesPerPacket];
    static uint16_t dac[az2::kGbAudioSamplesPerPacket];
    sound_update(psg, sizeof(psg));
    dac_update(dac, sizeof(dac));
    for (size_t i = 0; i < az2::kGbAudioSamplesPerPacket; ++i) {
      const int32_t mixed = (int16_t)psg[i] + (int16_t)dac[i];
      packet[i] = static_cast<uint8_t>((mixed >> 8) + 128);
    }
    Serial1.write(az2::kGbAudioPacketMagic);
    Serial1.write(static_cast<uint8_t>(az2::kGbAudioSamplesPerPacket));
    Serial1.write(packet, az2::kGbAudioSamplesPerPacket);
  }
  ++statsFrames;
  ++statsTotalFrames;
  statsCoreAccumUs += elapsed;
  if (elapsed > statsCoreMaxUs) statsCoreMaxUs = elapsed;
  const uint32_t now = micros();
  if (now - statsWindowStartUs >= 1000000UL) {
    stats.fpsX10 = static_cast<uint16_t>(statsFrames * 10UL * 1000000UL /
                                         (now - statsWindowStartUs));
    stats.avgCoreUs = statsFrames ? statsCoreAccumUs / statsFrames : 0;
    stats.maxCoreUs = statsCoreMaxUs;
    stats.totalFrames = statsTotalFrames;
    statsWindowStartUs = now;
    statsFrames = statsCoreAccumUs = statsCoreMaxUs = 0;
  }
}

bool ngpUnload() {
  if (!loaded) return true;
  ngpBlitWaitIdle();
  flashShutdown();
  graphics_free();
  audio_dac_free();
  ngp_mem_free();
  if (frame) free(frame);
  if (rom) free(rom);
  frame = nullptr;
  rom = nullptr;
  loaded = false;
  return true;
}

const char *ngpRomTitle() { return romTitle; }

void ngpSetButton(NgpButton button, bool pressed) {
  if (!loaded) return;
  uint8_t mask = 0;
  switch (button) {
    case NgpButton::Up: mask = 1 << 0; break;
    case NgpButton::Down: mask = 1 << 1; break;
    case NgpButton::Left: mask = 1 << 2; break;
    case NgpButton::Right: mask = 1 << 3; break;
    case NgpButton::A: mask = 1 << 4; break;
    case NgpButton::B: mask = 1 << 5; break;
    case NgpButton::Select:
    case NgpButton::Start: mask = 1 << 6; break;
  }
  if (pressed) buttonState |= mask;
  else buttonState &= static_cast<uint8_t>(~mask);
  mapButtons();
}

NgpRuntimeStats ngpRuntimeStats() { return stats; }
