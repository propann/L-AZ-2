// AZ-2 - Pont entre le coeur Peanut-GB (peanut_gb/peanut_gb.h, callbacks
// purs, licence MIT, https://github.com/deltabeard/Peanut-GB) et notre
// materiel. PROTOTYPE (branche research/peanut-gb-prototype) : backend
// alternatif au Walnut-CGB de gb_emulator.cpp, implemente exactement la
// meme interface publique (gb_emulator.h) pour que main.cpp n'ait besoin
// d'aucun changement -- seul l'environnement PlatformIO choisit lequel des
// deux .cpp est compile (voir platformio.ini, env:screen_esp_peanut_gb_lab).
// Ne remplace PAS gb_emulator.cpp : celui-ci reste le backend de
// production, non touche.
//
// Pourquoi ce prototype (voir docs/AZ2_MESURE_EMULATEUR_GB_2026-09-24.md
// pour la mesure complete) : Walnut-CGB verifie l'etat CGB (gb->cgb.*) a
// CHAQUE cycle CPU, dans la boucle d'execution elle-meme -- pas de chemin
// DMG separe qu'un simple filtrage de ROM .gbc pourrait debrancher. Peanut-
// GB est DMG-only depuis sa conception : aucun code CGB a porter ici.
//
// Peanut-GB et Walnut-CGB partagent (meme lignee d'auteur) la quasi-
// totalite de leur API publique : memes noms gb_init_error_e/
// GB_INIT_NO_ERROR, gb_get_save_size_s(), gb_get_rom_name(),
// gb_init_lcd(), struct gb_s, gb->direct.joypad (meme convention active-
// bas), memes noms JOYPAD_*, meme layout union cart_rtc (sec/min/hour/
// yday/high/bytes[5]) -- toute la logique RTC/sauvegarde/scan SD/paquet
// audio ci-dessous est donc portee TELLE QUELLE depuis gb_emulator.cpp,
// sans changement. Les seules differences reelles :
//   - gb_init() prend 6 arguments ici (pas de romRead16/romRead32 -- pas
//     de "dualfetch", Peanut-GB ne lit qu'un octet a la fois) ;
//   - gb_run_frame() (pas gb_run_frame_dualfetch(), qui n'existe pas ici) ;
//   - pas de champ gb->cgb du tout (DMG-only) : lcdDrawLine() n'a plus de
//     branche CGB, toujours la palette DMG.
//
// Son : IDENTIQUE a gb_emulator.cpp -- meme minigb_apu (deltabeard/Peanut-
// GB, vendored dans minigb_apu/), meme pont audio_read()/audio_write(),
// meme reduction mono 8 bits + paquet vers le Teensy.

#include "gb_emulator.h"

#define ENABLE_LCD 1
#define ENABLE_SOUND 1

// Prototypes requis par peanut_gb.h AVANT son #include -- le coeur les
// appelle directement (pas via pointeur de fonction enregistre), donc ils
// doivent deja exister a ce point. Definis plus bas (pont vers
// minigb_apu), une fois apuCtx declare. Meme contrainte que
// gb_emulator.cpp/walnut_cgb.h.
uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "peanut_gb/peanut_gb.h"

// minigb_apu.h/.c est un fichier C pur -- meme raison qu'en production
// (voir gb_emulator.cpp) : sans cet extern "C", l'edition de liens
// echoue sur les symboles mangles C++.
extern "C" {
#include "minigb_apu/minigb_apu.h"
}

#include <AZ2_Protocol.h>
#include <Arduino_GFX_Library.h>  // pour la macro RGB565() (palette DMG)
#include <SD.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cctype>
#include <ctime>

namespace {

struct gb_s gb;
minigb_apu_ctx apuCtx;
bool romLoaded = false;
uint8_t *romData = nullptr;
uint32_t romSize = 0;
uint8_t *cartRam = nullptr;
uint32_t cartRamSize = 0;
bool cartRamDirty = false;
// La copie .bak est l'unique sauvegarde valide apres recuperation.
bool cartRamRecoveredFromBackup = false;

// RTC MBC3 (cartouches 0x0F/0x10) : etat separe de la SRAM. Peanut-GB
// (comme Walnut-CGB) fait avancer le RTC pendant l'emulation ; AZ-2
// persiste les 5 registres et, si l'horloge systeme ESP32 est valide,
// rattrape aussi le temps ecoule machine eteinte.
bool cartHasRtc = false;
bool rtcRecoveredFromBackup = false;
uint8_t rtcLastSaved[5] = {0};
char rtcPath[96] = {0};

uint32_t gbLastAutosaveMs = 0;
GbRuntimeStats runtimeStats;
uint32_t statsWindowStartUs = 0;
uint32_t statsWorkAccumUs = 0;
uint32_t statsCoreAccumUs = 0;
uint32_t statsAudioAccumUs = 0;
uint32_t statsDisplayAccumUs = 0;
uint32_t statsCoreMaxUs = 0;
uint32_t statsDisplayMaxUs = 0;
uint32_t statsAudioMaxUs = 0;
uint32_t statsWindowFrames = 0;
uint32_t statsWindowMaxUs = 0;
constexpr uint8_t kStatsSamplesCapacity = 64;
uint32_t statsWorkSamples[kStatsSamplesCapacity] = {};
uint8_t statsWorkSampleCount = 0;
char romTitle[17] = {0};
constexpr size_t kSavePathCapacity = 96;
static_assert(kGbRomNameLen + sizeof("/games/") <= kSavePathCapacity,
              "GB: ROM filename capacity exceeds save path capacity");
char saveRamPath[kSavePathCapacity] = {0};
static_assert(sizeof(rtcPath) == kSavePathCapacity,
              "GB: RTC/save path capacities must match");

IRAM_ATTR uint8_t romRead(struct gb_s *, const uint_fast32_t addr) {
  return (addr < romSize) ? romData[addr] : 0xFF;
}

IRAM_ATTR uint8_t cartRamRead(struct gb_s *, const uint_fast32_t addr) {
  return (addr < cartRamSize) ? cartRam[addr] : 0xFF;
}

IRAM_ATTR void cartRamWrite(struct gb_s *, const uint_fast32_t addr, const uint8_t val) {
  if (addr < cartRamSize) {
    if (cartRam[addr] != val) {
      cartRam[addr] = val;
      cartRamDirty = true;
    }
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
// plus fonce (3). Peanut-GB est DMG-only : pas de branche CGB ici
// (contrairement a gb_emulator.cpp/lcdDrawLine()).
constexpr uint16_t kDmgPalette[4] = {
    RGB565(224, 248, 208), RGB565(136, 192, 112), RGB565(52, 104, 86), RGB565(8, 24, 32),
};

uint32_t crc32Buffer(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool verifyFileMatchesBuffer(const char *path, const uint8_t *data, size_t len) {
  File f = SD.open(path);
  if (!f || f.size() != len) {
    if (f) f.close();
    return false;
  }
  uint32_t crc = 0xFFFFFFFFu;
  uint8_t block[128];
  size_t remaining = len;
  while (remaining > 0) {
    const size_t wanted = remaining < sizeof(block) ? remaining : sizeof(block);
    const size_t got = f.read(block, wanted);
    if (got != wanted) {
      f.close();
      return false;
    }
    for (size_t i = 0; i < got; ++i) {
      crc ^= block[i];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
      }
    }
    remaining -= got;
  }
  f.close();
  return ~crc == crc32Buffer(data, len);
}

// Ecriture ATOMIQUE d'un buffer brut -- meme raison/sequence que
// gb_emulator.cpp (tmp -> verifie -> .bak -> rename final).
bool atomicSaveRaw(const char *path, const uint8_t *data, size_t len) {
  char tmpPath[kSavePathCapacity + 5];
  char bakPath[kSavePathCapacity + 5];
  const int tmpLen = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  const int bakLen = snprintf(bakPath, sizeof(bakPath), "%s.bak", path);
  if (tmpLen < 0 || static_cast<size_t>(tmpLen) >= sizeof(tmpPath) ||
      bakLen < 0 || static_cast<size_t>(bakLen) >= sizeof(bakPath)) {
    Serial.println("GB:SAVE_PATH_TOO_LONG");
    return false;
  }

  if (SD.exists(tmpPath) && !SD.remove(tmpPath)) {
    Serial.println("GB:SAVE_TMP_REMOVE_ERROR");
    return false;
  }
  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) {
    return false;
  }
  const size_t written = f.write(data, len);
  f.close();
  if (written != len) {
    SD.remove(tmpPath);
    return false;
  }
  if (!verifyFileMatchesBuffer(tmpPath, data, len)) {
    Serial.println("GB:SAVE_TMP_VERIFY_ERROR");
    SD.remove(tmpPath);
    return false;
  }

  const bool hadSave = SD.exists(path);
  if (cartRamRecoveredFromBackup) {
    if (hadSave && !SD.remove(path)) {
      Serial.println("GB:SAVE_RECOVERY_REMOVE_ERROR");
      SD.remove(tmpPath);
      return false;
    }
    if (!SD.rename(tmpPath, path)) {
      Serial.println("GB:SAVE_RECOVERY_RENAME_ERROR");
      SD.remove(tmpPath);
      return false; // .bak intact
    }
    return true;
  }
  if (hadSave) {
    if (SD.exists(bakPath) && !SD.remove(bakPath)) {
      Serial.println("GB:SAVE_BACKUP_REMOVE_ERROR");
      SD.remove(tmpPath);
      return false;
    }
    if (!SD.rename(path, bakPath)) {
      Serial.println("GB:SAVE_BACKUP_RENAME_ERROR");
      SD.remove(tmpPath);
      return false;
    }
  }
  if (!SD.rename(tmpPath, path)) {
    if (hadSave && !SD.rename(bakPath, path)) {
      Serial.println("GB:SAVE_RESTORE_ERROR");
    }
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

bool gbSaveCartRam() {
  if (!cartRamDirty) return true;
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    Serial.println("GB:SAVE_UNAVAILABLE");
    return false;
  }
  if (atomicSaveRaw(saveRamPath, cartRam, cartRamSize)) {
    cartRamDirty = false;
    cartRamRecoveredFromBackup = false;
    Serial.print("GB:SAVED:");
    Serial.println(saveRamPath);
    return true;
  }
  Serial.print("GB:SAVE_WRITE_ERROR:");
  Serial.println(saveRamPath);
  return false;
}

constexpr size_t kRtcRecordBytes = 22;
constexpr uint8_t kRtcRecordVersion = 1;
constexpr int64_t kUnixTimeFloor = 1577836800LL;  // 2020-01-01

int64_t validUnixTimeNow() {
  const time_t now = time(nullptr);
  return static_cast<int64_t>(now) >= kUnixTimeFloor ? static_cast<int64_t>(now) : 0;
}

void writeLe64(uint8_t *dst, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) dst[i] = static_cast<uint8_t>(value >> (8 * i));
}

uint64_t readLe64(const uint8_t *src) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(src[i]) << (8 * i);
  return value;
}

void advanceRtcBySeconds(uint64_t elapsed) {
  if (!cartHasRtc || elapsed == 0 || (gb.rtc_real.reg.high & 0x40)) return; // halted
  const uint16_t day = static_cast<uint16_t>(gb.rtc_real.reg.yday) |
                       (static_cast<uint16_t>(gb.rtc_real.reg.high & 0x01) << 8);
  uint64_t total = static_cast<uint64_t>(gb.rtc_real.reg.sec) +
                   static_cast<uint64_t>(gb.rtc_real.reg.min) * 60ULL +
                   static_cast<uint64_t>(gb.rtc_real.reg.hour) * 3600ULL +
                   static_cast<uint64_t>(day) * 86400ULL + elapsed;
  const uint64_t daysTotal = total / 86400ULL;
  const uint32_t secOfDay = static_cast<uint32_t>(total % 86400ULL);
  const uint16_t newDay = static_cast<uint16_t>(daysTotal & 0x1FFULL);
  const bool overflow = daysTotal > 0x1FFULL;

  gb.rtc_real.reg.hour = static_cast<uint8_t>(secOfDay / 3600U);
  gb.rtc_real.reg.min = static_cast<uint8_t>((secOfDay % 3600U) / 60U);
  gb.rtc_real.reg.sec = static_cast<uint8_t>(secOfDay % 60U);
  gb.rtc_real.reg.yday = static_cast<uint8_t>(newDay & 0xFFU);
  uint8_t high = static_cast<uint8_t>(gb.rtc_real.reg.high & 0xC0U); // halt/carry
  high = static_cast<uint8_t>(high | ((newDay >> 8) & 0x01U));
  if (overflow) high |= 0x80U;
  gb.rtc_real.reg.high = high;
  memcpy(gb.rtc_latched.bytes, gb.rtc_real.bytes, sizeof(gb.rtc_real.bytes));
}

bool encodeRtcRecord(uint8_t out[kRtcRecordBytes]) {
  if (!cartHasRtc) return false;
  memset(out, 0, kRtcRecordBytes);
  out[0] = 'A'; out[1] = 'Z'; out[2] = 'R'; out[3] = 'T';
  out[4] = kRtcRecordVersion;
  memcpy(out + 5, gb.rtc_real.bytes, 5);
  const int64_t unixNow = validUnixTimeNow();
  writeLe64(out + 10, unixNow > 0 ? static_cast<uint64_t>(unixNow) : 0ULL);
  const uint32_t crc = crc32Buffer(out, 18);
  out[18] = static_cast<uint8_t>(crc);
  out[19] = static_cast<uint8_t>(crc >> 8);
  out[20] = static_cast<uint8_t>(crc >> 16);
  out[21] = static_cast<uint8_t>(crc >> 24);
  return true;
}

bool decodeRtcRecord(const uint8_t in[kRtcRecordBytes], uint8_t rtcBytes[5], uint64_t &savedUnix) {
  if (in[0] != 'A' || in[1] != 'Z' || in[2] != 'R' || in[3] != 'T' ||
      in[4] != kRtcRecordVersion) return false;
  const uint32_t expected = static_cast<uint32_t>(in[18]) |
      (static_cast<uint32_t>(in[19]) << 8) |
      (static_cast<uint32_t>(in[20]) << 16) |
      (static_cast<uint32_t>(in[21]) << 24);
  if (crc32Buffer(in, 18) != expected) return false;
  memcpy(rtcBytes, in + 5, 5);
  // Defensive masks matching the core's RTC register write masks.
  rtcBytes[0] &= 0x3F; rtcBytes[1] &= 0x3F; rtcBytes[2] &= 0x1F;
  rtcBytes[4] &= 0xC1;
  savedUnix = readLe64(in + 10);
  return true;
}

bool readRtcFile(const char *path, uint8_t rtcBytes[5], uint64_t &savedUnix) {
  if (!SD.exists(path)) return false;
  File f = SD.open(path);
  if (!f || f.size() != kRtcRecordBytes) {
    if (f) f.close();
    return false;
  }
  uint8_t record[kRtcRecordBytes];
  const size_t got = f.read(record, sizeof(record));
  f.close();
  return got == sizeof(record) && decodeRtcRecord(record, rtcBytes, savedUnix);
}

bool atomicSaveRtcRaw(const uint8_t *record) {
  char tmpPath[kSavePathCapacity + 5];
  char bakPath[kSavePathCapacity + 5];
  if (snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", rtcPath) < 0 ||
      snprintf(bakPath, sizeof(bakPath), "%s.bak", rtcPath) < 0) return false;
  if (SD.exists(tmpPath) && !SD.remove(tmpPath)) return false;
  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) return false;
  const size_t written = f.write(record, kRtcRecordBytes);
  f.close();
  if (written != kRtcRecordBytes || !verifyFileMatchesBuffer(tmpPath, record, kRtcRecordBytes)) {
    SD.remove(tmpPath);
    return false;
  }

  const bool hadPrimary = SD.exists(rtcPath);
  if (rtcRecoveredFromBackup) {
    if (hadPrimary && !SD.remove(rtcPath)) { SD.remove(tmpPath); return false; }
    if (!SD.rename(tmpPath, rtcPath)) { SD.remove(tmpPath); return false; }
    return true; // keep valid .bak until next normal rotation
  }
  if (hadPrimary) {
    if (SD.exists(bakPath) && !SD.remove(bakPath)) { SD.remove(tmpPath); return false; }
    if (!SD.rename(rtcPath, bakPath)) { SD.remove(tmpPath); return false; }
  }
  if (!SD.rename(tmpPath, rtcPath)) {
    if (hadPrimary) SD.rename(bakPath, rtcPath);
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

bool gbSaveRtc() {
  if (!cartHasRtc || rtcPath[0] == '\0') return true;
  if (!rtcRecoveredFromBackup && memcmp(rtcLastSaved, gb.rtc_real.bytes, 5) == 0) return true;
  uint8_t record[kRtcRecordBytes];
  if (!encodeRtcRecord(record) || !atomicSaveRtcRaw(record)) {
    Serial.println("GB:RTC_SAVE_ERROR");
    return false;
  }
  memcpy(rtcLastSaved, gb.rtc_real.bytes, 5);
  rtcRecoveredFromBackup = false;
  Serial.print("GB:RTC_SAVED:");
  Serial.println(rtcPath);
  return true;
}

bool gbLoadRtcIfPresent() {
  if (!cartHasRtc || rtcPath[0] == '\0') return true;
  const bool primaryExists = SD.exists(rtcPath);
  uint8_t rtcBytes[5];
  uint64_t savedUnix = 0;
  bool loaded = readRtcFile(rtcPath, rtcBytes, savedUnix);

  char bakPath[kSavePathCapacity + 5];
  const int bakLen = snprintf(bakPath, sizeof(bakPath), "%s.bak", rtcPath);
  const bool bakValid = bakLen >= 0 && static_cast<size_t>(bakLen) < sizeof(bakPath);
  const bool backupExists = bakValid && SD.exists(bakPath);
  if (!loaded && backupExists) {
    loaded = readRtcFile(bakPath, rtcBytes, savedUnix);
    if (loaded) {
      rtcRecoveredFromBackup = true;
      Serial.println("GB:RTC_RECOVERED_FROM_BACKUP");
    }
  }
  if (!loaded) {
    if (primaryExists || backupExists || !bakValid) {
      Serial.println("GB:RTC_EXISTING_FILES_INVALID");
      return false;
    }
    memset(rtcLastSaved, 0, sizeof(rtcLastSaved));
    return true;
  }

  memcpy(gb.rtc_real.bytes, rtcBytes, 5);
  memcpy(gb.rtc_latched.bytes, rtcBytes, 5);
  const int64_t now = validUnixTimeNow();
  if (savedUnix >= static_cast<uint64_t>(kUnixTimeFloor) && now > static_cast<int64_t>(savedUnix)) {
    advanceRtcBySeconds(static_cast<uint64_t>(now) - savedUnix);
  }
  memcpy(rtcLastSaved, gb.rtc_real.bytes, 5);
  Serial.print("GB:RTC_LOADED:");
  Serial.println(rtcPath);
  return true;
}

bool gbLoadCartRamFile(const char *path) {
  if (!SD.exists(path)) {
    return false;
  }
  File f = SD.open(path);
  if (!f) {
    Serial.print("GB:SAVE_READ_OPEN_ERROR:");
    Serial.println(path);
    return false;
  }
  const size_t fileSize = f.size();
  if (fileSize != cartRamSize) {
    f.close();
    Serial.print("GB:SAVE_SIZE_ERROR:");
    Serial.print(path);
    Serial.print(":expected=");
    Serial.print(cartRamSize);
    Serial.print(":actual=");
    Serial.println(fileSize);
    return false;
  }
  const size_t readBytes = f.read(cartRam, cartRamSize);
  f.close();
  if (readBytes != cartRamSize) {
    Serial.print("GB:SAVE_READ_ERROR:");
    Serial.print(path);
    Serial.print(":expected=");
    Serial.print(cartRamSize);
    Serial.print(":actual=");
    Serial.println(readBytes);
    return false;
  }
  Serial.print("GB:SAVE_LOADED:");
  Serial.print(path);
  Serial.print(":bytes=");
  Serial.println(readBytes);
  return true;
}

bool gbLoadCartRamIfPresent() {
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    return true;
  }
  const bool primaryExists = SD.exists(saveRamPath);
  if (gbLoadCartRamFile(saveRamPath)) {
    cartRamDirty = false;
    cartRamRecoveredFromBackup = false;
    return true;
  }

  char backupPath[kSavePathCapacity + 5];
  const int backupLen = snprintf(backupPath, sizeof(backupPath), "%s.bak", saveRamPath);
  const bool backupPathValid = backupLen >= 0 &&
      static_cast<size_t>(backupLen) < sizeof(backupPath);
  const bool backupExists = backupPathValid && SD.exists(backupPath);
  if (backupExists && gbLoadCartRamFile(backupPath)) {
    cartRamDirty = false;
    cartRamDirty = true;
    cartRamRecoveredFromBackup = true;
    Serial.println("GB:SAVE_RECOVERED_FROM_BACKUP");
    return true;
  }
  if (primaryExists || backupExists || !backupPathValid) {
    Serial.println("GB:SAVE_EXISTING_FILES_INVALID");
    return false;
  }
  return true;  // nouvelle cartouche, aucune sauvegarde sur la SD
}

// lcd_draw_line du coeur : convertit les 160 pixels de la ligne en RGB565.
// Peanut-GB est DMG-only -- toujours 2 bits de teinte -> kDmgPalette, pas
// de branche CGB (contrairement a gb_emulator.cpp).
void lcdDrawLine(struct gb_s *, const uint8_t *pixels, const uint_fast8_t line) {
  static uint16_t row[160];
  for (uint8_t x = 0; x < 160; ++x) {
    row[x] = kDmgPalette[pixels[x] & 0x03];
  }
  gbBlitLine(line, row);
}

}  // namespace

// Pont registres APU <-> minigb_apu -- signature exigee telle quelle par
// peanut_gb.h (voir le prototype avant son #include, plus haut). Identique
// a gb_emulator.cpp.
uint8_t audio_read(const uint16_t addr) {
  return minigb_apu_audio_read(&apuCtx, addr);
}

void audio_write(const uint16_t addr, const uint8_t val) {
  minigb_apu_audio_write(&apuCtx, addr, val);
}

namespace {

int16_t gbAudioStereoBuf[AUDIO_SAMPLES_TOTAL];
uint8_t gbAudioMonoBuf[AUDIO_SAMPLES];
uint8_t gbAudioStereo8Buf[AUDIO_SAMPLES * 2];
static_assert(AUDIO_SAMPLES * 2 <= az2::kGbAudioV2MaxPayload,
              "GB: V2 stereo payload exceeds shared protocol capacity");
bool gbAudioV2Ready = false;
uint16_t gbAudioV2Sequence = 0;
az2::GbAudioV2Frame gbAudioV2Tx;
uint8_t gbAudioV2Wire[az2::kGbAudioV2HeaderBytes +
                      az2::kGbAudioV2MaxPayload + az2::kGbAudioV2CrcBytes];

static_assert(AUDIO_SAMPLE_RATE == az2::kGbAudioSampleRate,
              "GB: AUDIO_SAMPLE_RATE must match AZ2_Protocol.h");
static_assert(AUDIO_SAMPLES == az2::kGbAudioSamplesPerPacket,
              "GB: audio packet size must match the Teensy protocol");
static_assert(AUDIO_SAMPLES > 0 && AUDIO_SAMPLES <= 255,
              "GB: V1 packet length field is one byte");

void sendGbAudioPacket() {
  minigb_apu_audio_callback(&apuCtx, gbAudioStereoBuf);

  for (unsigned i = 0; i < AUDIO_SAMPLES; ++i) {
    const int32_t mixed = static_cast<int32_t>(gbAudioStereoBuf[i * 2]) +
                           static_cast<int32_t>(gbAudioStereoBuf[i * 2 + 1]);
    const int16_t mono = static_cast<int16_t>(mixed / 2);
    gbAudioMonoBuf[i] = static_cast<uint8_t>((mono >> 8) + 128);
    gbAudioStereo8Buf[i * 2] =
        static_cast<uint8_t>((gbAudioStereoBuf[i * 2] >> 8) + 128);
    gbAudioStereo8Buf[i * 2 + 1] =
        static_cast<uint8_t>((gbAudioStereoBuf[i * 2 + 1] >> 8) + 128);
  }

  if (az2::kGbAudioV2PilotEnabled && gbAudioV2Ready) {
    gbAudioV2Tx.sequence = gbAudioV2Sequence++;
    gbAudioV2Tx.sampleRate = az2::kGbAudioSampleRate;
    gbAudioV2Tx.format = az2::kGbAudioV2FormatPcmU8;
    gbAudioV2Tx.flags = az2::kGbAudioV2FlagStereo;
    gbAudioV2Tx.payloadLen = static_cast<uint16_t>(AUDIO_SAMPLES * 2);
    memcpy(gbAudioV2Tx.payload, gbAudioStereo8Buf, AUDIO_SAMPLES * 2);
    const size_t packetBytes = az2::encodeGbAudioV2(
        gbAudioV2Wire, sizeof(gbAudioV2Wire), gbAudioV2Tx);
    if (packetBytes != 0) {
      Serial1.write(gbAudioV2Wire, packetBytes);
      return;
    }
    Serial.println("GB:V2_ENCODE_ERROR:FALLBACK_V1");
  }
  Serial1.write(az2::kGbAudioPacketMagic);
  Serial1.write(static_cast<uint8_t>(AUDIO_SAMPLES));
  Serial1.write(gbAudioMonoBuf, AUDIO_SAMPLES);
}

}  // namespace

bool gbIsLoaded() {
  return romLoaded;
}

bool gbUnload() {
  if (romLoaded) {
    const bool ramOk = gbSaveCartRam();
    const bool rtcOk = gbSaveRtc();
    if (!ramOk || !rtcOk) {
      Serial.println("GB:UNLOAD_BLOCKED_UNSAVED_STATE");
      return false;
    }
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
  rtcPath[0] = '\0';
  cartRamDirty = false;
  cartRamRecoveredFromBackup = false;
  cartHasRtc = false;
  rtcRecoveredFromBackup = false;
  memset(rtcLastSaved, 0, sizeof(rtcLastSaved));
  runtimeStats = GbRuntimeStats{};
  statsWindowStartUs = 0;
  statsWorkAccumUs = 0;
  statsWindowFrames = 0;
  statsWindowMaxUs = 0;
  statsCoreAccumUs = 0;
  statsAudioAccumUs = 0;
  statsDisplayAccumUs = 0;
  statsCoreMaxUs = 0;
  statsDisplayMaxUs = 0;
  statsAudioMaxUs = 0;
  statsWorkSampleCount = 0;
  return true;
}

bool gbSaveNow() {
  if (!romLoaded) return true;
  const bool ramOk = gbSaveCartRam();
  const bool rtcOk = gbSaveRtc();
  return ramOk && rtcOk;
}

void gbSetAudioV2Ready(bool ready) {
  gbAudioV2Ready = az2::kGbAudioV2PilotEnabled && ready;
  gbAudioV2Sequence = 0;
  Serial.println(gbAudioV2Ready ? "GB:AUDIO_V2_READY" : "GB:AUDIO_V1_ACTIVE");
}

int compareRomNamesCaseInsensitive(const char *a, const char *b) {
  while (*a != '\0' && *b != '\0') {
    const int ca = std::tolower(static_cast<unsigned char>(*a));
    const int cb = std::tolower(static_cast<unsigned char>(*b));
    if (ca != cb) return ca - cb;
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

void sortRomNames(char names[][kGbRomNameLen], uint8_t count) {
  char tmp[kGbRomNameLen];
  for (uint8_t i = 1; i < count; ++i) {
    memcpy(tmp, names[i], kGbRomNameLen);
    uint8_t j = i;
    while (j > 0 && compareRomNamesCaseInsensitive(names[j - 1], tmp) > 0) {
      memcpy(names[j], names[j - 1], kGbRomNameLen);
      --j;
    }
    memcpy(names[j], tmp, kGbRomNameLen);
  }
}

// Peanut-GB est DMG-only : les ROM .gbc sont exclues du navigateur ici
// (contrairement a gb_emulator.cpp qui liste .gb ET .gbc). Une ROM .gbc
// chargee sur ce coeur ne demarrerait pas dans le mode attendu -- refus
// net des le scan plutot qu'un mauvais mode silencieux.
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
    if (!entry.isDirectory() && (name.endsWith(".gb") || name.endsWith(".GB"))) {
      const int slash = name.lastIndexOf('/');
      const String base = (slash >= 0) ? name.substring(slash + 1) : name;
      if (base.length() >= kGbRomNameLen) {
        Serial.print("GB:ROM_NAME_TOO_LONG:");
        Serial.println(base);
      } else {
        strncpy(names[count], base.c_str(), kGbRomNameLen);
        ++count;
      }
    }
    entry.close();
  }
  dir.close();

  if (count == 0) {
    Serial.println("GB:NO_ROM_FOUND");
  } else {
    sortRomNames(names, count);
  }
  return count;
}

bool gbLoadRom(const char *filename) {
  if (filename == nullptr || filename[0] == '\0' || strchr(filename, '/') != nullptr ||
      strchr(filename, '\\') != nullptr || strcmp(filename, ".") == 0 ||
      strcmp(filename, "..") == 0) {
    Serial.println("GB:ROM_INVALID_NAME");
    return false;
  }

  char path[kSavePathCapacity];
  const int pathLen = snprintf(path, sizeof(path), "/games/%s", filename);
  if (pathLen < 0 || static_cast<size_t>(pathLen) >= sizeof(path)) {
    Serial.println("GB:ROM_PATH_TOO_LONG");
    return false;
  }
  File romFile = SD.open(path);
  if (!romFile) {
    Serial.print("GB:ROM_OPEN_ERROR:");
    Serial.println(path);
    return false;
  }

  const size_t requestedRomSize = romFile.size();
  if (requestedRomSize < 0x150 || requestedRomSize > 8U * 1024U * 1024U) {
    Serial.println("GB:ROM_INVALID_SIZE");
    romFile.close();
    return false;
  }
  uint8_t *candidateRomData =
      static_cast<uint8_t *>(heap_caps_malloc(requestedRomSize, MALLOC_CAP_SPIRAM));
  if (candidateRomData == nullptr) {
    Serial.println("GB:ROM_TOO_BIG_FOR_PSRAM");
    romFile.close();
    return false;
  }
  const size_t readBytes = romFile.read(candidateRomData, requestedRomSize);
  romFile.close();
  if (readBytes != requestedRomSize) {
    Serial.println("GB:ROM_READ_ERROR");
    heap_caps_free(candidateRomData);
    return false;
  }

  if (!gbUnload()) {
    heap_caps_free(candidateRomData);
    return false;
  }
  romSize = static_cast<uint32_t>(requestedRomSize);
  romData = candidateRomData;

  // 6 arguments ici (pas de romRead16/romRead32 -- voir le commentaire
  // d'en-tete du fichier) : seule vraie difference d'API avec
  // gb_emulator.cpp/gb_init().
  const enum gb_init_error_e initErr =
      gb_init(&gb, romRead, cartRamRead, cartRamWrite, gbErrorCallback, nullptr);
  if (initErr != GB_INIT_NO_ERROR) {
    Serial.print("GB:INIT_ERROR:code=");
    Serial.println(static_cast<int>(initErr));
    heap_caps_free(romData);
    romData = nullptr;
    return false;
  }

  size_t detectedSaveSize = 0;
  if (gb_get_save_size_s(&gb, &detectedSaveSize) != 0 || detectedSaveSize > UINT32_MAX) {
    Serial.println("GB:SAVE_SIZE_UNSUPPORTED");
    gbUnload();
    return false;
  }
  cartRamSize = static_cast<uint32_t>(detectedSaveSize);
  cartRamDirty = false;
  cartRamRecoveredFromBackup = false;

  const uint8_t cartridgeType = romData[0x147];
  cartHasRtc = (cartridgeType == 0x0F || cartridgeType == 0x10);
  rtcRecoveredFromBackup = false;
  rtcPath[0] = '\0';
  memset(rtcLastSaved, 0, sizeof(rtcLastSaved));
  if (cartHasRtc) {
    const int rtcPathLen = snprintf(rtcPath, sizeof(rtcPath), "/games/%s", filename);
    if (rtcPathLen < 0 || static_cast<size_t>(rtcPathLen) >= sizeof(rtcPath)) {
      Serial.println("GB:RTC_PATH_TOO_LONG");
      gbUnload();
      return false;
    }
    char *rtcDot = strrchr(rtcPath, '.');
    if (rtcDot == nullptr || static_cast<size_t>(rtcDot - rtcPath) + sizeof(".rtc") > sizeof(rtcPath)) {
      Serial.println("GB:RTC_PATH_INVALID");
      gbUnload();
      return false;
    }
    strcpy(rtcDot, ".rtc");
  }
  gbLastAutosaveMs = millis();
  runtimeStats = GbRuntimeStats{};
  statsWindowStartUs = micros();
  statsCoreAccumUs = 0;
  statsAudioAccumUs = 0;
  statsWorkSampleCount = 0;
  if (cartRamSize > 0) {
    cartRam = static_cast<uint8_t *>(heap_caps_malloc(cartRamSize, MALLOC_CAP_SPIRAM));
    if (cartRam != nullptr) {
      memset(cartRam, 0xFF, cartRamSize);
      const int savePathLen = snprintf(saveRamPath, sizeof(saveRamPath), "/games/%s", filename);
      if (savePathLen < 0 || static_cast<size_t>(savePathLen) >= sizeof(saveRamPath)) {
        Serial.println("GB:SAVE_PATH_TOO_LONG");
        gbUnload();
        return false;
      }
      char *dot = strrchr(saveRamPath, '.');
      if (dot == nullptr || static_cast<size_t>(dot - saveRamPath) + sizeof(".sav") > sizeof(saveRamPath)) {
        Serial.println("GB:SAVE_PATH_INVALID");
        gbUnload();
        return false;
      }
      strcpy(dot, ".sav");
      if (!gbLoadCartRamIfPresent()) {
        gbUnload();
        return false;
      }
    } else {
      Serial.println("GB:CART_RAM_ALLOCATION_ERROR");
      gbUnload();
      return false;
    }
  }

  if (cartHasRtc && !gbLoadRtcIfPresent()) {
    gbUnload();
    return false;
  }

  gb_init_lcd(&gb, lcdDrawLine);
  minigb_apu_audio_init(&apuCtx);
  gb.direct.joypad = 0xFF;  // rien de presse (voir gbSetButton() -- 0=presse, 1=relache)
  // Meme intention que gb_emulator.cpp : un rendu LCD sur deux pour rester
  // fluide. A REPASSER A false pour une mesure GB:PERF comparable une fois
  // ce prototype capable de tourner sur materiel (voir
  // docs/AZ2_MESURE_EMULATEUR_GB_2026-09-24.md, etape 5 "comparer avec la
  // meme telemetrie").
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

constexpr uint32_t kGbAutosaveIntervalMs = 30000;

void gbRunFrame() {
  if (!romLoaded) return;

  const uint32_t workStartUs = micros();
  // gb_run_frame() -- pas de variante "dualfetch" dans Peanut-GB (celle-ci
  // est une optimisation propre a Walnut-CGB, voir le commentaire
  // d'en-tete du fichier).
  gb_run_frame(&gb);
  const uint32_t coreUs = micros() - workStartUs;
  extern volatile uint32_t gGbDisplayLastUs;
  const uint32_t displayUs = gGbDisplayLastUs;
  const uint32_t audioStartUs = micros();
  sendGbAudioPacket();
  const uint32_t audioUs = micros() - audioStartUs;
  const uint32_t workUs = coreUs + audioUs;

  ++runtimeStats.totalFrames;
  ++statsWindowFrames;
  statsWorkAccumUs += workUs;
  statsCoreAccumUs += coreUs;
  statsDisplayAccumUs += displayUs;
  statsAudioAccumUs += audioUs;
  if (coreUs > statsCoreMaxUs) statsCoreMaxUs = coreUs;
  if (displayUs > statsDisplayMaxUs) statsDisplayMaxUs = displayUs;
  if (audioUs > statsAudioMaxUs) statsAudioMaxUs = audioUs;
  if (workUs > statsWindowMaxUs) statsWindowMaxUs = workUs;
  if (statsWorkSampleCount < kStatsSamplesCapacity) {
    statsWorkSamples[statsWorkSampleCount++] = workUs;
  }

  const uint32_t nowUs = micros();
  const uint32_t windowUs = nowUs - statsWindowStartUs;
  if (windowUs >= 1000000u && statsWindowFrames > 0) {
    runtimeStats.fpsX10 = static_cast<uint16_t>(
        (static_cast<uint64_t>(statsWindowFrames) * 10000000ull + windowUs / 2) / windowUs);
    runtimeStats.avgWorkUs = statsWorkAccumUs / statsWindowFrames;
    runtimeStats.avgCoreUs = statsCoreAccumUs / statsWindowFrames;
    runtimeStats.avgDisplayUs = statsDisplayAccumUs / statsWindowFrames;
    runtimeStats.avgAudioUs = statsAudioAccumUs / statsWindowFrames;
    runtimeStats.maxCoreUs = statsCoreMaxUs;
    runtimeStats.maxDisplayUs = statsDisplayMaxUs;
    runtimeStats.maxAudioUs = statsAudioMaxUs;
    runtimeStats.maxWorkUs = statsWindowMaxUs;
    for (uint8_t i = 1; i < statsWorkSampleCount; ++i) {
      const uint32_t value = statsWorkSamples[i];
      uint8_t j = i;
      while (j > 0 && statsWorkSamples[j - 1] > value) {
        statsWorkSamples[j] = statsWorkSamples[j - 1];
        --j;
      }
      statsWorkSamples[j] = value;
    }
    if (statsWorkSampleCount > 0) {
      const uint8_t p99Index = static_cast<uint8_t>((statsWorkSampleCount * 99U) / 100U);
      runtimeStats.p99WorkUs = statsWorkSamples[p99Index >= statsWorkSampleCount ?
                                                   statsWorkSampleCount - 1 : p99Index];
    } else {
      runtimeStats.p99WorkUs = 0;
    }
    statsWindowStartUs = nowUs;
    statsWorkAccumUs = 0;
    statsCoreAccumUs = 0;
    statsDisplayAccumUs = 0;
    statsAudioAccumUs = 0;
    statsCoreMaxUs = 0;
    statsDisplayMaxUs = 0;
    statsAudioMaxUs = 0;
    statsWindowFrames = 0;
    statsWindowMaxUs = 0;
    statsWorkSampleCount = 0;
  }

  const uint32_t now = millis();
  if (now - gbLastAutosaveMs >= kGbAutosaveIntervalMs) {
    gbLastAutosaveMs = now;
    const bool ramOk = gbSaveCartRam();
    const bool rtcOk = gbSaveRtc();
    if (!ramOk || !rtcOk) {
      ++runtimeStats.autosaveFailures;
    }
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
  // Registre joypad Game Boy actif-bas : 0 = presse, 1 = relache. Meme
  // convention, memes noms JOYPAD_* que gb_emulator.cpp (valeurs de bits
  // differentes entre les deux coeurs, d'ou l'usage des constantes
  // nommees plutot que des hex en dur -- voir le "modele" boutons de
  // gb_emulator.cpp).
  if (pressed) {
    gb.direct.joypad &= static_cast<uint8_t>(~mask);
  } else {
    gb.direct.joypad |= mask;
  }
}

const char *gbRomTitle() {
  return romTitle;
}

GbRuntimeStats gbRuntimeStats() {
  return runtimeStats;
}
