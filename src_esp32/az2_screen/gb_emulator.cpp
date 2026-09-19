// AZ-2 - Pont entre le coeur Walnut-CGB (walnut_cgb/walnut_cgb.h,
// callbacks purs, licence MIT) et notre materiel : ROM chargee depuis la
// carte SD vers la PSRAM du module ESP32-S3 (8 Mo, meme puce que
// l'ecran -- voir platformio.ini "qio_opi"/"module WROOM-1 N16R8"), puis
// affichee via gbBlitLine() (implementee dans main.cpp, seul endroit qui
// connait `gfx`).
//
// Son : demande le 2026-09-15 ("il faut un emulateur complet classe" +
// "envoyer sous forme de paquet ... pour que le DAC le joue"). Walnut-
// CGB (comme Peanut-GB dont il derive) n'emet PAS le son lui-meme -- il
// appelle juste audio_read()/audio_write() sur les acces aux registres
// APU (0xFF10-0xFF3F) et attend d'une lib externe qu'elle les
// transforme en PCM. On utilise minigb_apu (deltabeard/Peanut-GB,
// vendored dans minigb_apu/, licence MIT) pour ca -- meme famille de
// projet que Walnut-CGB. Notre architecture audio reste centree sur le
// Teensy (le DAC PCM5102A y est cable) : chaque frame GB, on recupere
// un buffer PCM stereo 16 bits de minigb_apu, on le reduit a du mono 8
// bits a kGbAudioSampleRate Hz (voir AZ2_Protocol.h) et on l'envoie en
// paquet binaire sur le MEME lien Serial1 que le reste du protocole
// (voir sendGbAudioPacket() plus bas) -- volontairement basse
// resolution pour tenir large dans le budget du lien 230400 bauds.
// Cote Teensy : src_teensy/az2_audio/main.cpp recoit ces paquets,
// re-echantillonne vers 44.1kHz et les joue via un AudioPlayQueue
// branche sur le bus d'effets maitre (meme chemin que les moteurs de
// synthese), donc avec reverb/delay/volume si les potards sont
// tournes.

#include "gb_emulator.h"

#define ENABLE_LCD 1
#define ENABLE_SOUND 1

// Prototypes requis par walnut_cgb.h AVANT son #include -- le coeur les
// appelle directement au fil de l'emulation (pas via pointeur de
// fonction enregistre comme lcd_draw_line/gb_rom_read), donc ils
// doivent deja exister a ce point. Definis plus bas (pont vers
// minigb_apu), une fois apuCtx declare.
uint8_t audio_read(const uint16_t addr);
void audio_write(const uint16_t addr, const uint8_t val);

#include "walnut_cgb/walnut_cgb.h"

// minigb_apu.h/.c est un fichier C pur (pas de garde extern "C" dans le
// header -- verifie directement dedans, absent), compile en C par la
// LDF PlatformIO (detection automatique par extension .c) -- ses
// symboles ont donc un linkage C plat, pas le name-mangling C++. Sans
// ce extern "C", l'edition de liens echoue ("undefined reference" aux
// noms mangles C++ style _Z21minigb_apu_audio_read..., trouve en
// compilant une premiere fois).
extern "C" {
#include "minigb_apu/minigb_apu.h"
}

#include <AZ2_Protocol.h>
#include <Arduino_GFX_Library.h>  // pour la macro RGB565() (palette DMG)
#include <SD.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace {

struct gb_s gb;
minigb_apu_ctx apuCtx;
bool romLoaded = false;
uint8_t *romData = nullptr;
uint32_t romSize = 0;
uint8_t *cartRam = nullptr;
uint32_t cartRamSize = 0;
bool cartRamDirty = false;
// Sauvegarde periodique (2026-09-19, voir gbRunFrame()) -- remis a
// zero a chaque chargement de ROM (voir gbLoadRom()) pour que le
// premier autosave d'une nouvelle partie tombe bien kGbAutosaveIntervalMs
// apres le CHARGEMENT, pas selon millis() depuis le boot (qui pourrait
// deja depasser l'intervalle, declenchant une sauvegarde inutile a la
// toute premiere frame).
uint32_t gbLastAutosaveMs = 0;
char romTitle[17] = {0};
// Chemin de sauvegarde (cart RAM) pour la ROM courante, meme nom que la
// ROM avec l'extension remplacee par .sav, a cote d'elle dans /games --
// convention classique d'emulateur (rom.gb + rom.sav). Vide si aucune
// ROM chargee ou si la cartouche n'a pas de RAM (cartRamSize==0).
constexpr size_t kSavePathCapacity = 96;
char saveRamPath[kSavePathCapacity] = {0};

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
// plus fonce (3).
constexpr uint16_t kDmgPalette[4] = {
    RGB565(224, 248, 208), RGB565(136, 192, 112), RGB565(52, 104, 86), RGB565(8, 24, 32),
};

// Ecriture ATOMIQUE d'un buffer brut (2026-09-19, defaut P1 signale
// par l'audit de code du meme jour : "la sauvegarde Game Boy depend de
// la sortie propre du jeu") -- SD.open(path, FILE_WRITE) ecrivait
// directement PAR-DESSUS le .sav existant, sans protection : une
// coupure de courant ou un plantage en cours d'ecriture corrompait la
// sauvegarde de progression EN PLACE (contrairement a savePatchSlot()/
// saveProject() cote main.cpp, deja corriges le meme jour -- meme
// defaut, meme classe de fix, mais un fichier .sav est un buffer brut,
// pas du texte, donc une fonction dediee ici plutot que reutiliser
// atomicSaveFile() -- template defini dans l'autre unite de
// compilation, pas partage sans header commun). Meme sequence :
// ecrit dans "<path>.tmp", verifie la taille, deplace l'ancien vers
// "<path>.bak", renomme le tmp vers le nom final -- a aucun moment le
// fichier final n'est absent ou tronque.
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

  // Ne jamais ecraser le seul backup si sa suppression ou le
  // deplacement de la sauvegarde courante echoue.
  const bool hadSave = SD.exists(path);
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
    // En cas d'echec, conserver le backup meme si la restauration
    // echoue : gbLoadCartRamIfPresent() peut encore lire .bak.
    if (hadSave && !SD.rename(bakPath, path)) {
      Serial.println("GB:SAVE_RESTORE_ERROR");
    }
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

// Sauvegarde (voir saveRamPath ci-dessus) -- convention .sav a cote de
// la ROM sur la carte SD. Demande 2026-09-15 ("sauvegarde tout") :
// ecrit au moment de decharger la ROM (voir gbUnload()), relu au
// chargement suivant si le fichier existe deja (voir gbLoadRom()). Pas
// d'ecriture pendant le jeu (juste a la sortie) -- suffisant pour une
// sauvegarde a l'extinction/au changement de jeu, pas de risque
// d'ecriture SD en boucle pendant que ca joue. Ecriture rendue
// atomique le 2026-09-19 (voir atomicSaveRaw() ci-dessus) -- ne
// protege PAS contre une coupure DIRECTE de l'alimentation pendant la
// partie (rien n'est ecrit avant gbUnload(), voir l'audit de code du
// meme jour pour la recommandation de sauvegarde periodique, pas
// faite ici).
bool gbSaveCartRam() {
  if (!cartRamDirty) return true;
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    Serial.println("GB:SAVE_UNAVAILABLE");
    return false;
  }
  if (atomicSaveRaw(saveRamPath, cartRam, cartRamSize)) {
    cartRamDirty = false;
    Serial.print("GB:SAVED:");
    Serial.println(saveRamPath);
    return true;
  }
  Serial.print("GB:SAVE_WRITE_ERROR:");
  Serial.println(saveRamPath);
  return false;
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

void gbLoadCartRamIfPresent() {
  if (cartRam == nullptr || cartRamSize == 0 || saveRamPath[0] == '\0') {
    return;
  }
  if (gbLoadCartRamFile(saveRamPath)) {
    cartRamDirty = false;
    return;
  }

  // Une sauvegarde finale absente ou tronquee peut etre recuperee depuis
  // le backup conserve par atomicSaveRaw(). Ne jamais accepter un fichier
  // partiel : LSDJ manipule une SRAM importante et une lecture courte peut
  // ressembler a une sauvegarde valide tout en ayant perdu des morceaux.
  char backupPath[kSavePathCapacity + 5];
  const int backupLen = snprintf(backupPath, sizeof(backupPath), "%s.bak", saveRamPath);
  if (backupLen >= 0 && static_cast<size_t>(backupLen) < sizeof(backupPath) &&
      gbLoadCartRamFile(backupPath)) {
    cartRamDirty = false;
    Serial.println("GB:SAVE_RECOVERED_FROM_BACKUP");
  }
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

// Pont registres APU <-> minigb_apu -- signature exigee telle quelle
// par walnut_cgb.h (voir le prototype avant son #include, plus haut).
uint8_t audio_read(const uint16_t addr) {
  return minigb_apu_audio_read(&apuCtx, addr);
}

void audio_write(const uint16_t addr, const uint8_t val) {
  minigb_apu_audio_write(&apuCtx, addr, val);
}

namespace {

// Un buffer PCM stereo 16 bits (AUDIO_SAMPLES_TOTAL echantillons, voir
// minigb_apu.h) par frame GB -- reduit a mono 8 bits avant l'envoi (voir
// AZ2_Protocol.h, kGbAudioPacketMagic) pour tenir dans le budget serie.
int16_t gbAudioStereoBuf[AUDIO_SAMPLES_TOTAL];
uint8_t gbAudioMonoBuf[AUDIO_SAMPLES];

// Le firmware ecran et le firmware audio compilent avec le MEME contrat
// AZ2_Protocol.h. Empêcher un changement de fréquence uniquement dans
// platformio.ini : sinon le Teensy refuserait les paquets ou lirait une
// longueur erronée sans avertissement au build.
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
    // 16 bits signe -> 8 bits non signe (PCM8 standard, offset binaire
    // +128 -- meme convention que la plupart des lecteurs WAV 8 bits).
    gbAudioMonoBuf[i] = static_cast<uint8_t>((mono >> 8) + 128);
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
  // Ne jamais liberer une cartouche dont les modifications n'ont pas
  // pu etre sauvegardees. Le joueur peut reessayer apres avoir retabli
  // la carte SD, plutot que perdre silencieusement son morceau LSDJ.
  if (romLoaded && !gbSaveCartRam()) {
    Serial.println("GB:UNLOAD_BLOCKED_UNSAVED_RAM");
    return false;
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
  cartRamDirty = false;
  return true;
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
  // Ouvrir et valider le fichier avant de liberer la partie precedente.
  // La sauvegarde de l'ancienne cartouche est toujours effectuee ici.
  if (!gbUnload()) {
    romFile.close();
    return false;
  }
  romSize = static_cast<uint32_t>(requestedRomSize);
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

  // Ne pas deduire la taille depuis num_ram_banks : MBC2 expose 512
  // demi-octets de RAM alors que ce compteur vaut zero. Le coeur connait
  // deja les tailles exactes de tous les types de cartouche supportes.
  size_t detectedSaveSize = 0;
  if (gb_get_save_size_s(&gb, &detectedSaveSize) != 0 || detectedSaveSize > UINT32_MAX) {
    Serial.println("GB:SAVE_SIZE_UNSUPPORTED");
    gbUnload();
    return false;
  }
  cartRamSize = static_cast<uint32_t>(detectedSaveSize);
  cartRamDirty = false;
  gbLastAutosaveMs = millis();  // reparti a zero pour cette partie, voir gbRunFrame()
  if (cartRamSize > 0) {
    cartRam = static_cast<uint8_t *>(heap_caps_malloc(cartRamSize, MALLOC_CAP_SPIRAM));
    if (cartRam != nullptr) {
      memset(cartRam, 0xFF, cartRamSize);
      // Chemin de sauvegarde = meme nom que la ROM, extension .sav (voir
      // saveRamPath) -- tronque a la premiere extension trouvee, gere
      // .gb comme .gbc.
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
      gbLoadCartRamIfPresent();
    } else {
      Serial.println("GB:CART_RAM_ALLOCATION_ERROR");
      gbUnload();
      return false;
    }
  }

  gb_init_lcd(&gb, lcdDrawLine);
  minigb_apu_audio_init(&apuCtx);  // etat APU frais -- pas de bruit/note residuelle de la ROM precedente
  gb.direct.joypad = 0xFF;  // rien de presse (voir gbSetButton() -- 0=presse, 1=relache)
  // Le mode normal AZ-2 affiche chaque image. gbBlitLine() regroupe les
  // lignes en bandes pour supprimer l'ancien cout de 144 transactions par
  // frame. Un mode economie pourra etre ajoute plus tard, mais ne doit pas
  // etre le comportement par defaut d'une machine visant l'emulation native.
  gb.direct.frame_skip = false;

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

// Sauvegarde PERIODIQUE (2026-09-19, recommandation de l'audit de code
// du meme jour : "la sauvegarde Game Boy depend de la sortie propre du
// jeu ... une coupure directe de l'alimentation pendant une partie
// perd toute la progression depuis le dernier chargement") -- en plus
// de la sauvegarde a la sortie propre (gbUnload()), ecrit aussi
// periodiquement PENDANT la partie (voir gbLastAutosaveMs plus haut).
// 30s : assez rare pour ne pas solliciter la carte SD en continu (une
// ecriture SD bloque quelques ms), assez frequent pour limiter la
// perte reelle en cas de coupure brutale a "au plus 30s de jeu", pas
// "toute la session".
constexpr uint32_t kGbAutosaveIntervalMs = 30000;

void gbRunFrame() {
  if (romLoaded) {
    // Walnut-CGB recommande ce chemin : deux opcodes sont recuperes par
    // chaine de dispatch et les transferts DMA 32 bits restent actifs. Les
    // optimisations 16 bits experimentales connues pour casser des jeux
    // restent, elles, desactivees dans walnut_cgb.h.
    gb_run_frame_dualfetch(&gb);
    sendGbAudioPacket();

    const uint32_t now = millis();
    if (now - gbLastAutosaveMs >= kGbAutosaveIntervalMs) {
      gbLastAutosaveMs = now;
      gbSaveCartRam();  // conserve dirty=true si l'ecriture echoue, pour reessayer
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
