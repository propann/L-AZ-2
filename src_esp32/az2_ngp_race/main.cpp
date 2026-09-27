// AZ-2 Neo Geo Pocket / Color — cible labo séparée.
//
// Le cœur RACE est GPLv2-only et reste volontairement hors de screen_esp.
// Cette cible reprend le câblage AZ-2 :
//   croix -> directions, A/B -> boutons NGP, C/D -> Select/Start.
// Elle sert à valider le port matériel avant toute décision d'intégration.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <AZ2_Protocol.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#define RETRO_COMPAT_IMPLEMENTATION
extern "C" {
#include "race/flash.h"
#include "race/graphics.h"
#include "race/input.h"
#include "race/race-memory.h"
#include "race/sound.h"
#include "race/tlcs900h.h"
#include "race/cz80_support.h"
#include "race/ngpBios.h"
#include "race/neopopsound.h"
#include "race/retro_compat.h"
#include "ngp_storage.h"
}

// Symboles partagés avec le renderer et les CPU RACE.
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

constexpr int16_t kScreen = 480;
constexpr int kBacklight = 38;
constexpr int kSdCs = 47;
constexpr int kSdClk = 45;
constexpr int kSdMiso = 46;
constexpr int kSdMosi = 42;
constexpr int kTeensyTx = 19;
constexpr int kTeensyRx = 20;
constexpr int kNgpWidth = 160;
constexpr int kNgpHeight = 152;
constexpr int kScale = 3;
constexpr int kViewWidth = kNgpWidth * kScale;
constexpr int kViewHeight = kNgpHeight * kScale;
constexpr int kViewY = (kScreen - kViewHeight) / 2;
constexpr uint32_t kNgpCpuClock = 6144000;
constexpr uint32_t kAudioRate = 14000;
constexpr uint32_t kAudioSamples = 234;

Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);
Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    18, 17, 16, 21, 4, 3, 2, 1, 0, 10, 9, 8, 7, 6, 5,
    15, 14, 13, 12, 11, 1, 10, 8, 50, 1, 10, 8, 20, 0,
    12000000, false, 0, 0, 0);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    kScreen, kScreen, panel, 2, true, bus, GFX_NOT_DEFINED,
    gc9503v_type1_init_operations, sizeof(gc9503v_type1_init_operations));

uint8_t *rom = nullptr;
size_t romSize = 0;
char romPath[160] = {};
char savePath[192] = {};
uint16_t *frame = nullptr;
uint16_t *scaled = nullptr;
uint16_t psgSamples[kAudioSamples] = {};
uint16_t dacSamples[kAudioSamples] = {};
uint8_t ngpPad = 0;
uint32_t frames = 0;
uint64_t workUs = 0;
uint32_t lastReportMs = 0;
uint32_t lastSaveMs = 0;
bool ready = false;

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

bool isNgpRom(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".ngp") || lower.endsWith(".ngc");
}

bool findRomIn(File &dir, const String &prefix) {
  File entry;
  while ((entry = dir.openNextFile())) {
    const String full = entry.name();
    const String base = full.substring(full.lastIndexOf('/') + 1);
    const String relative = prefix.length() ? prefix + "/" + base : base;
    if (entry.isDirectory()) {
      if (findRomIn(entry, relative)) {
        entry.close();
        return true;
      }
    } else if (isNgpRom(full)) {
      const String path = String("/games/") + relative;
      if (path.length() >= sizeof(romPath)) {
        Serial.println("NGP:ROM_PATH_TOO_LONG");
      } else {
        strlcpy(romPath, path.c_str(), sizeof(romPath));
        strlcpy(savePath, path.c_str(), sizeof(savePath));
        char *dot = strrchr(savePath, '.');
        if (dot) strlcpy(dot, ".ngf", sizeof(savePath) - (dot - savePath));
        entry.close();
        return true;
      }
    }
    entry.close();
  }
  return false;
}

bool loadRom() {
  File games = SD.open("/games");
  if (!games || !games.isDirectory() || !findRomIn(games, "")) return false;
  games.close();

  File file = SD.open(romPath, FILE_READ);
  if (!file) return false;
  romSize = file.size();
  rom = static_cast<uint8_t *>(heap_caps_malloc(romSize, MALLOC_CAP_SPIRAM));
  if (!rom || file.read(rom, romSize) != romSize) {
    file.close();
    return false;
  }
  file.close();
  az2NgpSetSavePath(savePath);
  return true;
}

void sendAudio() {
  sound_update(psgSamples, kAudioSamples * sizeof(uint16_t));
  dac_update(dacSamples, kAudioSamples * sizeof(uint16_t));
  static uint8_t packet[kAudioSamples];
  for (size_t i = 0; i < kAudioSamples; ++i) {
    const int32_t mixed = (int16_t)psgSamples[i] + (int16_t)dacSamples[i];
    packet[i] = static_cast<uint8_t>((mixed >> 8) + 128);
  }
  Serial1.write(az2::kGbAudioPacketMagic);
  Serial1.write(static_cast<uint8_t>(kAudioSamples));
  Serial1.write(packet, kAudioSamples);
}

void drawFrame() {
  for (int y = 0; y < kNgpHeight; ++y) {
    const uint16_t *src = frame + y * kNgpWidth;
    uint16_t *dst = scaled + y * kScale * kViewWidth;
    for (int x = 0; x < kNgpWidth; ++x) {
      const uint16_t pixel = src[x];
      for (int dx = 0; dx < kScale; ++dx) dst[x * kScale + dx] = pixel;
    }
    for (int dy = 1; dy < kScale; ++dy) {
      memcpy(dst + dy * kViewWidth, dst, kViewWidth * sizeof(uint16_t));
    }
  }
  gfx->draw16bitRGBBitmap(0, kViewY, scaled, kViewWidth, kViewHeight);
}

void applyInput(const String &line) {
  if (!line.startsWith("BTN:") || line.length() < 7) return;
  const bool down = line.endsWith("DOWN");
  const bool up = line.endsWith("UP");
  if (!down && !up) return;
  uint8_t bit = 0;
  if (line.indexOf("A:") >= 0) bit = 1 << 4;
  else if (line.indexOf("B:") >= 0) bit = 1 << 5;
  else if (line.indexOf("UP:") >= 0) bit = 1 << 0;
  else if (line.indexOf("DOWN:") >= 0) bit = 1 << 1;
  else if (line.indexOf("LEFT:") >= 0) bit = 1 << 2;
  else if (line.indexOf("RIGHT:") >= 0) bit = 1 << 3;
  else if (line.indexOf("C:") >= 0 || line.indexOf("D:") >= 0) bit = 1 << 6;
  if (down) ngpPad |= bit;
  else ngpPad &= static_cast<uint8_t>(~bit);
  ngpInputState = ngpPad;
}

void readControls() {
  static String line;
  while (Serial1.available()) {
    const char c = static_cast<char>(Serial1.read());
    if (c == '\n') {
      applyInput(line);
      line = "";
    } else if (line.length() < 96 && c != '\r') {
      line += c;
    }
  }
}

}  // namespace

const char *az2NgpSavePath() { return savePath; }

bool az2NgpReadFile(const char *path, void **data, size_t *size) {
  if (!path || !data || !size) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  *size = file.size();
  *data = malloc(*size);
  if (!*data || file.read(static_cast<uint8_t *>(*data), *size) != *size) {
    if (*data) free(*data);
    *data = nullptr;
    *size = 0;
    file.close();
    return false;
  }
  file.close();
  return true;
}

bool az2NgpWriteFile(const char *path, const void *data, size_t size) {
  if (!path || !data) return false;
  // FILE_WRITE est append dans Arduino-ESP32 ; le NGF doit remplacer le
  // snapshot precedent, sinon chaque autosave grossit et devient invalide.
  SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  const bool ok = file.write(static_cast<const uint8_t *>(data), size) == size;
  file.close();
  return ok;
}

void az2NgpSetSavePath(const char *path) {
  strlcpy(savePath, path ? path : "", sizeof(savePath));
}

void setup() {
  Serial.begin(230400);
  Serial1.begin(az2::kControlBaud, SERIAL_8N1, kTeensyRx, kTeensyTx);
  pinMode(kBacklight, OUTPUT);
  digitalWrite(kBacklight, HIGH);
  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);

  SPI.begin(kSdClk, kSdMiso, kSdMosi, kSdCs);
  if (!SD.begin(kSdCs, SPI)) {
    Serial.println("NGP:SD_ERROR");
    return;
  }
  if (!loadRom()) {
    Serial.println("NGP:NO_ROM_NGP_NGC");
    return;
  }

  frame = static_cast<uint16_t *>(heap_caps_malloc(kNgpWidth * kNgpHeight * 2, MALLOC_CAP_SPIRAM));
  scaled = static_cast<uint16_t *>(heap_caps_malloc(kViewWidth * kViewHeight * 2, MALLOC_CAP_SPIRAM));
  if (!frame || !scaled || !ngp_mem_set_rom(rom, romSize)) {
    Serial.println("NGP:ALLOC_ERROR");
    return;
  }

  m_emuInfo.machine = NGPC;
  m_emuInfo.romSize = static_cast<int>(romSize);
  is_mono_game = romSize > 0x23 && rom[0x23] == 0x00;
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
  sound_init(kAudioRate);
  setDefaultsAfterBoot();
  drawBuffer = frame;
  graphics_init();
  if (wndSizeX && wndSizeY && (*wndSizeX == 0 || *wndSizeY == 0)) {
    *wndTopLeftX = 0; *wndTopLeftY = 0; *wndSizeX = 160; *wndSizeY = 152;
  }
  if (bgSelect) *bgSelect |= 0x80;
  if (frame0Pri) *frame0Pri |= 0xC0;
  finscan = 198;
  ready = true;
  Serial.print("NGP:READY:ROM=");
  Serial.println(romPath);
}

void loop() {
  readControls();
  if (!ready) {
    delay(10);
    return;
  }
  const uint32_t start = micros();
  g_frame_ready = 0;
  tlcs_execute(kNgpCpuClock / 60);
  const uint32_t elapsed = micros() - start;
  workUs += elapsed;
  ++frames;
  if (g_frame_ready) {
    drawFrame();
    sendAudio();
  }
  if (needToWriteFile && millis() - lastSaveMs >= 2000) {
    writeSaveGameFile();
    lastSaveMs = millis();
  }
  const uint32_t now = millis();
  if (now - lastReportMs >= 1000) {
    Serial.print("NGP:PERF:fps=");
    Serial.print(frames);
    Serial.print(":frame_us=");
    Serial.print(frames ? workUs / frames : 0);
    Serial.print(":heap_kb=");
    Serial.print(ESP.getFreeHeap() / 1024U);
    Serial.print(":psram_kb=");
    Serial.println(ESP.getFreePsram() / 1024U);
    frames = 0;
    workUs = 0;
    lastReportMs = now;
  }
}
