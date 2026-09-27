// AZ-2 - Pont entre le coeur NES (nes_core/, Anemoia-ESP32) et le reste du
// firmware -- SD, boutons, telemetrie. Voir nes_emulator.h.
//
// [2026-09-27] Premiere passe fonctionnelle : chargement ROM + rendu +
// manette. PAS encore fait (voir docs/AZ2_ETAT_ACTUEL.md) :
// - sauvegarde SRAM a pile (Cartridge::dumpState/loadState existent mais
//   pas encore branches ici, contrairement a la Game Boy) ;
// - decoupage double coeur (AZ2_GB_DUAL_CORE_BLIT) -- gbBlitLine() et
//   nesBlitBand() partagent le meme motif "bande de 8 lignes + callback",
//   reutilisable plus tard, pas fait dans cette premiere passe ;
// - paquets audio vers le Teensy (nesAudioBufferReady() est un point
//   d'accroche vide pour l'instant, voir plus bas).
#include "nes_emulator.h"

#include <SD.h>

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

// Compare-insensible aux majuscules/minuscules pour l'extension .nes --
// meme raison que gbScanRoms() (SD peut renvoyer les noms dans la casse du
// systeme de fichiers d'origine).
bool endsWithNes(const String &name) {
  return name.endsWith(".nes") || name.endsWith(".NES");
}

}  // namespace

// Trampoline vers le callback de dessin du coeur (voir nes_core/ppu2C02.h,
// DrawCallback) -- reconstruit l'index de bande (0-29, 240/8) a partir du
// compteur de frame courant puisque le coeur ne le transmet pas lui-meme,
// puis delegue a nesBlitBand() (main.cpp, seul endroit qui connait `gfx`).
void nesDrawCallback(uint8_t *buffer, uint32_t size) {
  (void)size;
  nesBlitBand(currentBandIndex, reinterpret_cast<const uint16_t *>(buffer));
  ++currentBandIndex;
}

// Point d'accroche audio (voir nes_core/apu2A03.cpp, writeBuffer()) -- pas
// encore branche sur un paquet serie vers le Teensy, voir le commentaire
// d'en-tete de ce fichier. No-op volontaire pour cette premiere passe
// (le jeu tourne et s'affiche sans le son, comme un premier test GB avant
// que sendGbAudioPacket() existe).
void nesAudioBufferReady(const uint16_t *buffer, size_t count) {
  (void)buffer;
  (void)count;
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
  // TODO(2026-09-27): sauvegarder la SRAM a pile avant de decharger, voir
  // Cartridge::dumpState() -- pas fait dans cette premiere passe.
  romLoaded = false;
  delete nesCart;
  nesCart = nullptr;
  romTitle[0] = '\0';
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
    stats.fpsX10 = static_cast<uint16_t>(
        (static_cast<uint64_t>(statsWindowFrames) * 10000ull + windowUs / 200) / (windowUs / 100));
    stats.avgCoreUs = statsCoreAccumUs / statsWindowFrames;
    stats.maxCoreUs = statsCoreMaxUs;
    statsWindowStartUs = nowUs;
    statsWindowFrames = 0;
    statsCoreAccumUs = 0;
    statsCoreMaxUs = 0;
  }
}

NesRuntimeStats nesRuntimeStats() { return stats; }
