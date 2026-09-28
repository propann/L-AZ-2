// AZ-2 - Pont entre le coeur NES (nes_core/, Anemoia-ESP32) et le reste du
// firmware -- SD, boutons, telemetrie. Voir nes_emulator.h.
//
// [2026-09-27] Passe fonctionnelle : chargement ROM, rendu, manette, audio
// APU, sauvegarde SRAM et télémétrie sont raccordés. La cible courante active
// aussi AZ2_NES_DUAL_CORE pour exécuter l’APU sur le core 0. Restent à
// qualifier : matrice de mappers/ROMs, sessions longues, compatibilité des
// sauvegardes et optimisation du budget vidéo (49-50 fps mesurés, FRAMESKIP).
#include "nes_emulator.h"

#include <SD.h>

#ifdef AZ2_NES_DUAL_CORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

#include "AZ2_Protocol.h"
#include "nes_core/cpu6502.h"

namespace {

Cpu6502 nesCpu;
Cartridge *nesCart = nullptr;
bool romLoaded = false;
char romTitle[17] = "";
uint8_t nesButtonState = 0;

NesRuntimeStats stats;
uint32_t statsWindowStartUs = 0;
uint32_t statsWindowFrames = 0;
uint32_t statsCoreAccumUs = 0;
uint32_t statsCoreMaxUs = 0;

int currentBandIndex = 0;
uint8_t nesAudioPacket[az2::kGbAudioSamplesPerPacket] = {};
uint8_t nesAudioPacketSize = 0;
constexpr uint32_t kNesSaveMagic = 0x325A4E53;  // "SNZ2"
constexpr uint16_t kNesSaveVersion = 1;
constexpr size_t kNesSavePathCapacity = kNesRomNameLen + sizeof("/games/") + sizeof(".sav");
char nesSavePath[kNesSavePathCapacity] = {};

#ifdef AZ2_NES_DUAL_CORE
TaskHandle_t nesApuTaskHandle = nullptr;
SemaphoreHandle_t nesSerial1Mutex = nullptr;

void nesApuTask(void *) {
  // Les blocs courts limitent le temps pendant lequel l'APU verrouille ses
  // registres lorsqu'un jeu ecrit dans $4000-$4017 depuis le core 1.
  // 512 cycles restent sous 0,3 ms a l'horloge CPU NES et reduisent fortement
  // le nombre de prises de verrou/atomiques pendant chaque frame.
  constexpr uint32_t kApuBatchCycles = 512;
  for (;;) {
    const uint32_t target = nesCpu.apu.scheduledCycles();
    const uint32_t completed = nesCpu.apu.completedCycles();
    const uint32_t pending = target - completed;
    if (pending == 0) {
      vTaskDelay(1);
      continue;
    }
    const uint16_t batch = static_cast<uint16_t>(pending > kApuBatchCycles ?
                                                  kApuBatchCycles : pending);
    nesCpu.apu.clock(batch);
    nesCpu.apu.markCyclesComplete(batch);
  }
}
#endif

struct NesSaveHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t romCrc;
  uint32_t ramSize;
};

// Compare-insensible aux majuscules/minuscules pour l'extension .nes --
// meme raison que gbScanRoms() (SD peut renvoyer les noms dans la casse du
// systeme de fichiers d'origine).
bool endsWithNes(const String &name) {
  return name.endsWith(".nes") || name.endsWith(".NES");
}

}  // namespace

void nesSerial1Lock() {
#ifdef AZ2_NES_DUAL_CORE
  if (nesSerial1Mutex != nullptr) {
    xSemaphoreTake(nesSerial1Mutex, portMAX_DELAY);
  }
#endif
}

void nesSerial1Unlock() {
#ifdef AZ2_NES_DUAL_CORE
  if (nesSerial1Mutex != nullptr) {
    xSemaphoreGive(nesSerial1Mutex);
  }
#endif
}

#ifdef AZ2_NES_DUAL_CORE
void nesInitDualCore() {
  if (nesApuTaskHandle != nullptr) return;
  nesSerial1Mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(nesApuTask, "NES_APU", 6144, nullptr, 2,
                          &nesApuTaskHandle, 0);
  Serial.println("NES:DUAL_CORE:APU=CORE0:CPU_PPU=CORE1");
}
#endif

// Trampoline vers le callback de dessin du coeur (voir nes_core/ppu2C02.h,
// DrawCallback) -- reconstruit l'index de bande (0-29, 240/8) a partir du
// compteur de frame courant puisque le coeur ne le transmet pas lui-meme,
// puis delegue a nesBlitBand() (main.cpp, seul endroit qui connait `gfx`).
void nesDrawCallback(uint8_t *buffer, uint32_t size) {
  (void)size;
  nesBlitBand(currentBandIndex, reinterpret_cast<const uint16_t *>(buffer));
  ++currentBandIndex;
}

// Meme paquet audio V1 que la Game Boy : l'APU NES fournit des blocs de 128
// echantillons stereo PCM16 decales (0..65535). Le Teensy attend exactement
// az2::kGbAudioSamplesPerPacket echantillons par paquet (234 a 14 kHz) ; on
// accumule donc plusieurs blocs avant de transmettre du mono PCM8.
void nesAudioBufferReady(const uint16_t *buffer, size_t count) {
  if (buffer == nullptr || count < 2) return;
  const size_t samples = count / 2;
  for (size_t i = 0; i < samples; ++i) {
    const uint32_t mixed = static_cast<uint32_t>(buffer[i * 2]) + buffer[i * 2 + 1];
    // L'APU NES produit une amplitude 0..255 dont le silence vaut 0.
    // Le protocole Teensy attend un PCM8 non signe centre en 128 ; sans
    // cet offset, le silence devenait un enorme signal continu (-32768).
    const uint16_t amplitude = static_cast<uint16_t>((mixed / 2) >> 8);
    const uint8_t centered = static_cast<uint8_t>(128u + (amplitude > 127u ? 127u : amplitude));
    nesAudioPacket[nesAudioPacketSize++] = centered;
    if (nesAudioPacketSize == az2::kGbAudioSamplesPerPacket) {
      uint8_t wire[az2::kGbAudioSamplesPerPacket + 2] = {};
      wire[0] = az2::kGbAudioPacketMagic;
      wire[1] = nesAudioPacketSize;
      memcpy(wire + 2, nesAudioPacket, nesAudioPacketSize);
      nesSerial1Lock();
      Serial1.write(wire, sizeof(wire));
      nesSerial1Unlock();
      nesAudioPacketSize = 0;
    }
  }
}

bool buildNesSavePath(const char *filename) {
  const int pathLen = snprintf(nesSavePath, sizeof(nesSavePath), "/games/%s", filename);
  if (pathLen < 0 || static_cast<size_t>(pathLen) >= sizeof(nesSavePath)) return false;
  char *dot = strrchr(nesSavePath, '.');
  if (dot == nullptr || static_cast<size_t>(dot - nesSavePath) + sizeof(".sav") > sizeof(nesSavePath)) {
    return false;
  }
  strcpy(dot, ".sav");
  return true;
}

bool loadNesSaveFromPath(const char *path) {
  if (path == nullptr || !SD.exists(path) || nesCart == nullptr || !nesCart->hasBatteryRam()) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  NesSaveHeader header = {};
  const size_t expectedSize = sizeof(header) + nesCart->batteryRamSize();
  const bool headerRead = f.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) == sizeof(header);
  const bool valid = headerRead && f.size() == expectedSize && header.magic == kNesSaveMagic &&
                     header.version == kNesSaveVersion && header.romCrc == nesCart->CRC32 &&
                     header.ramSize == nesCart->batteryRamSize();
  const bool loaded = valid && nesCart->loadBatteryRam(f);
  f.close();
  return loaded;
}

bool loadNesSaveIfPresent() {
  if (nesSavePath[0] == '\0' || nesCart == nullptr || !nesCart->hasBatteryRam()) return true;
  if (loadNesSaveFromPath(nesSavePath)) {
    Serial.print("NES:SAVE_LOADED:");
    Serial.println(nesSavePath);
    return true;
  }

  char backupPath[kNesSavePathCapacity + 5];
  const int backupLen = snprintf(backupPath, sizeof(backupPath), "%s.bak", nesSavePath);
  if (backupLen >= 0 && static_cast<size_t>(backupLen) < sizeof(backupPath) &&
      loadNesSaveFromPath(backupPath)) {
    Serial.print("NES:SAVE_RECOVERED:");
    Serial.println(backupPath);
    return true;
  }

  if (SD.exists(nesSavePath)) {
    Serial.print("NES:SAVE_INVALID:");
    Serial.println(nesSavePath);
  }
  return false;
}

void scanNesDir(File &dir, const String &prefix, char names[][kNesRomNameLen],
               uint8_t &count) {
  for (File entry = dir.openNextFile(); entry && count < kNesMaxRoms;
       entry = dir.openNextFile()) {
    const String fullName = entry.name();
    const int slash = fullName.lastIndexOf('/');
    const String base = (slash >= 0) ? fullName.substring(slash + 1) : fullName;
    const String relative = prefix.length() ? prefix + "/" + base : base;
    if (entry.isDirectory()) {
      scanNesDir(entry, relative, names, count);
    } else if (endsWithNes(fullName)) {
      if (relative.length() >= kNesRomNameLen) {
        Serial.print("NES:ROM_PATH_TOO_LONG:");
        Serial.println(relative);
      } else {
        strncpy(names[count], relative.c_str(), kNesRomNameLen);
        ++count;
      }
    }
    entry.close();
  }
}

uint8_t nesScanRoms(char names[][kNesRomNameLen]) {
  uint8_t count = 0;

  if (!SD.exists("/games")) {
    Serial.println("NES:NO_ROMS_DIR");
    return 0;
  }
  File dir = SD.open("/games");
  if (!dir || !dir.isDirectory()) {
    Serial.println("NES:NO_ROMS_DIR");
    return 0;
  }

  scanNesDir(dir, "", names, count);
  dir.close();

  if (count == 0) {
    Serial.println("NES:NO_ROM_FOUND");
  }
  return count;
}

bool nesLoadRom(const char *filename) {
  if (filename == nullptr || filename[0] == '\0' || filename[0] == '/' ||
      strstr(filename, "..") != nullptr) {
    Serial.println("NES:ROM_INVALID_NAME");
    return false;
  }

  char path[kNesRomNameLen + sizeof("/games/")];
  const int pathLen = snprintf(path, sizeof(path), "/games/%s", filename);
  if (pathLen < 0 || static_cast<size_t>(pathLen) >= sizeof(path)) {
    Serial.println("NES:ROM_PATH_TOO_LONG");
    return false;
  }
  if (!SD.exists(path)) {
    Serial.print("NES:ROM_OPEN_ERROR:");
    Serial.println(path);
    return false;
  }

  nesUnload();

  Cartridge *candidate = new Cartridge(path);
  if (!candidate->isValid()) {
    Serial.println("NES:ROM_UNSUPPORTED_MAPPER");
    delete candidate;
    return false;
  }

  nesCart = candidate;
  nesCpu.bus.insertCartridge(nesCart);
  static bool drawCallbackSet = false;
  if (!drawCallbackSet) {
    nesCpu.bus.ppu.setDrawCallback(nesDrawCallback);
    drawCallbackSet = true;
  }
  nesCpu.reset();
  romLoaded = true;
  nesButtonState = 0;
  nesAudioPacketSize = 0;
  nesSavePath[0] = '\0';
  if (!buildNesSavePath(filename)) {
    Serial.println("NES:SAVE_PATH_ERROR");
  } else {
    loadNesSaveIfPresent();
  }

  strncpy(romTitle, filename, sizeof(romTitle) - 1);
  romTitle[sizeof(romTitle) - 1] = '\0';

  stats = NesRuntimeStats();
  statsWindowStartUs = micros();
  statsWindowFrames = 0;
  statsCoreAccumUs = 0;
  statsCoreMaxUs = 0;

  Serial.print("NES:LOADED:title=");
  Serial.println(romTitle);
  return true;
}

bool nesIsLoaded() { return romLoaded; }


bool nesUnload() {
  if (!romLoaded) return true;
  nesSaveRam();
  romLoaded = false;
  delete nesCart;
  nesCart = nullptr;
  romTitle[0] = '\0';
  nesSavePath[0] = '\0';
  return true;
}

bool nesSaveRam() {
  if (nesCart == nullptr || !nesCart->hasBatteryRam() || nesSavePath[0] == '\0') {
    Serial.println("NES:SAVE_UNAVAILABLE");
    return false;
  }

  char tmpPath[kNesSavePathCapacity + 5];
  char backupPath[kNesSavePathCapacity + 5];
  if (snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", nesSavePath) < 0 ||
      snprintf(backupPath, sizeof(backupPath), "%s.bak", nesSavePath) < 0) {
    Serial.println("NES:SAVE_PATH_ERROR");
    return false;
  }
  SD.remove(tmpPath);

  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) {
    Serial.print("NES:SAVE_OPEN_ERROR:");
    Serial.println(tmpPath);
    return false;
  }
  const NesSaveHeader header = {kNesSaveMagic, kNesSaveVersion, 0, nesCart->CRC32,
                                static_cast<uint32_t>(nesCart->batteryRamSize())};
  const bool headerOk = f.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header);
  const bool ramOk = headerOk && nesCart->dumpBatteryRam(f);
  f.close();
  File verify = SD.open(tmpPath, FILE_READ);
  const bool sizeOk = verify && verify.size() == sizeof(header) + nesCart->batteryRamSize();
  if (verify) verify.close();
  if (!ramOk || !sizeOk) {
    SD.remove(tmpPath);
    Serial.print("NES:SAVE_WRITE_ERROR:");
    Serial.println(nesSavePath);
    return false;
  }

  const bool hadPrimary = SD.exists(nesSavePath);
  if (hadPrimary) {
    SD.remove(backupPath);
    if (!SD.rename(nesSavePath, backupPath)) {
      SD.remove(tmpPath);
      Serial.println("NES:SAVE_BACKUP_ERROR");
      return false;
    }
  }
  if (!SD.rename(tmpPath, nesSavePath)) {
    if (hadPrimary) SD.rename(backupPath, nesSavePath);
    SD.remove(tmpPath);
    Serial.println("NES:SAVE_RENAME_ERROR");
    return false;
  }

  Serial.print("NES:SAVED:");
  Serial.println(nesSavePath);
  return true;
}

const char *nesRomTitle() { return romTitle; }

void nesSetButton(NesButton button, bool pressed) {
  if (!romLoaded) return;
  uint8_t mask = 0;
  switch (button) {
    case NesButton::A: mask = 1 << 0; break;
    case NesButton::B: mask = 1 << 1; break;
    case NesButton::Select: mask = 1 << 2; break;
    case NesButton::Start: mask = 1 << 3; break;
    case NesButton::Up: mask = 1 << 4; break;
    case NesButton::Down: mask = 1 << 5; break;
    case NesButton::Left: mask = 1 << 6; break;
    case NesButton::Right: mask = 1 << 7; break;
  }
  if (pressed) {
    nesButtonState |= mask;
  } else {
    nesButtonState &= static_cast<uint8_t>(~mask);
  }
  nesCpu.bus.setController(nesButtonState);
}

void nesRunFrame() {
  if (!romLoaded) return;

  currentBandIndex = 0;
  const uint32_t startUs = micros();
  nesCpu.clockFrame();
  const uint32_t coreUs = micros() - startUs;

  ++stats.totalFrames;
  ++statsWindowFrames;
  statsCoreAccumUs += coreUs;
  if (coreUs > statsCoreMaxUs) statsCoreMaxUs = coreUs;

  const uint32_t nowUs = micros();
  const uint32_t windowUs = nowUs - statsWindowStartUs;
  if (windowUs >= 1000000u && statsWindowFrames > 0) {
    // FPS x10: frames * 10 s / window_s, avec arrondi en microsecondes.
    // L'ancienne formule divisait le résultat par 10 et affichait 4.7 au
    // lieu d'environ 47 FPS.
    stats.fpsX10 = static_cast<uint16_t>(
        (static_cast<uint64_t>(statsWindowFrames) * 10000000ull + windowUs / 2) /
        windowUs);
    stats.avgCoreUs = statsCoreAccumUs / statsWindowFrames;
    stats.maxCoreUs = statsCoreMaxUs;
    statsWindowStartUs = nowUs;
    statsWindowFrames = 0;
    statsCoreAccumUs = 0;
    statsCoreMaxUs = 0;
  }
}

NesRuntimeStats nesRuntimeStats() { return stats; }
