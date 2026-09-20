#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <AZ2_Protocol.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <AZ2_Protocol.h>

extern "C" {
#include "gnuboy/gnuboy.h"
}

namespace {
constexpr int16_t kScreen = 480;
constexpr int kBacklight = 38;
constexpr int kSdCs = 47, kSdClk = 45, kSdMiso = 46, kSdMosi = 42;
constexpr int kTeensyTx = 19, kTeensyRx = 20;
Arduino_DataBus *bus = new Arduino_SWSPI(GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);
Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    18, 17, 16, 21, 4, 3, 2, 1, 0, 10, 9, 8, 7, 6, 5, 15, 14, 13, 12, 11,
    1, 10, 8, 50, 1, 10, 8, 20, 0, 12000000, false, 0, 0, 0);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    kScreen, kScreen, panel, 2, true, bus, GFX_NOT_DEFINED,
    gc9503v_type1_init_operations, sizeof(gc9503v_type1_init_operations));

uint8_t *rom = nullptr;
size_t romSize = 0;
uint16_t *frame = nullptr;
uint16_t *scaled = nullptr;
int16_t *sound = nullptr;
uint8_t pad = 0;
uint32_t lastReport = 0;
uint32_t frames = 0;
uint64_t workUs = 0;
uint32_t audioPackets = 0;
uint32_t audioSamples = 0;

void videoCallback(void *buffer) {
  memcpy(frame, buffer, 160U * 144U * sizeof(uint16_t));
  for (int y = 0; y < 144; ++y) {
    const uint16_t *src = frame + y * 160;
    uint16_t *dst = scaled + y * 2 * 320;
    for (int x = 0; x < 160; ++x) {
      dst[x * 2] = src[x];
      dst[x * 2 + 1] = src[x];
    }
    memcpy(dst + 320, dst, 320 * sizeof(uint16_t));
  }
  gfx->draw16bitRGBBitmap(80, 96, scaled, 320, 288);
}

void audioCallback(void *buffer, size_t length) {
  // GNUBOY emits signed 16-bit mono. The existing Teensy receiver expects
  // fixed-size unsigned 8-bit PCM packets at the shared GB sample rate.
  if (buffer == nullptr || length != az2::kGbAudioSamplesPerPacket) return;
  static uint8_t packet[az2::kGbAudioSamplesPerPacket];
  const int16_t *src = static_cast<const int16_t *>(buffer);
  for (size_t i = 0; i < length; ++i) {
    packet[i] = static_cast<uint8_t>((src[i] >> 8) + 128);
  }
  Serial1.write(az2::kGbAudioPacketMagic);
  Serial1.write(static_cast<uint8_t>(length));
  Serial1.write(packet, length);
  ++audioPackets;
  audioSamples += static_cast<uint32_t>(length);
}

bool loadFirstRom() {
  File dir = SD.open("/games");
  if (!dir || !dir.isDirectory()) return false;
  String zeldaGb;
  String zeldaGbc;
  String genericGb;
  String fallback;
  File scanEntry;
  while ((scanEntry = dir.openNextFile())) {
    String name = scanEntry.name();
    if (!scanEntry.isDirectory()) {
      String lower = name;
      lower.toLowerCase();
      const String path = name.startsWith("/") ? name : String("/games/") + name;
      if (lower.endsWith(".gb")) {
        if (genericGb.isEmpty()) genericGb = path;
        if (lower.indexOf("zelda") >= 0 && zeldaGb.isEmpty()) zeldaGb = path;
      } else if (lower.endsWith(".gbc")) {
        if (fallback.isEmpty()) fallback = path;
        if (lower.indexOf("zelda") >= 0 && zeldaGbc.isEmpty()) zeldaGbc = path;
      }
    }
    scanEntry.close();
  }
  dir.close();
  String preferred = !zeldaGb.isEmpty() ? zeldaGb :
                     !zeldaGbc.isEmpty() ? zeldaGbc :
                     !genericGb.isEmpty() ? genericGb : fallback;
  if (preferred.isEmpty()) return false;

  dir = SD.open("/games");
  if (!dir || !dir.isDirectory()) return false;
  File entry;
  while ((entry = dir.openNextFile())) {
    String name = entry.name();
    String path = name.startsWith("/") ? name : String("/games/") + name;
    if (!entry.isDirectory() && path == preferred) {
      romSize = entry.size();
      rom = static_cast<uint8_t *>(heap_caps_malloc(romSize, MALLOC_CAP_SPIRAM));
      if (!rom) return false;
      entry.seek(0);
      if (entry.read(rom, romSize) != romSize) return false;
      Serial.print("GNUBOY:ROM:");
      Serial.println(path);
      entry.close();
      dir.close();
      return true;
    }
    entry.close();
  }
  dir.close();
  return false;
}

void applyLine(const String &line) {
  if (!line.startsWith("BTN:") || !line.endsWith("DOWN")) return;
  if (line.indexOf("A:") >= 0) pad |= GB_PAD_A;
  else if (line.indexOf("B:") >= 0) pad |= GB_PAD_B;
  else if (line.indexOf("UP:") >= 0) pad |= GB_PAD_UP;
  else if (line.indexOf("DOWN:") >= 0) pad |= GB_PAD_DOWN;
  else if (line.indexOf("LEFT:") >= 0) pad |= GB_PAD_LEFT;
  else if (line.indexOf("RIGHT:") >= 0) pad |= GB_PAD_RIGHT;
  else if (line.indexOf("C:") >= 0) pad |= GB_PAD_SELECT;
  else if (line.indexOf("D:") >= 0) pad |= GB_PAD_START;
  gnuboy_set_pad(pad);
}
}

void setup() {
  Serial.begin(230400);
  Serial1.begin(az2::kControlBaud, SERIAL_8N1, kTeensyRx, kTeensyTx);
  pinMode(kBacklight, OUTPUT);
  digitalWrite(kBacklight, HIGH);
  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  SPI.begin(kSdClk, kSdMiso, kSdMosi, kSdCs);
  if (!SD.begin(kSdCs, SPI) || !loadFirstRom()) {
    Serial.println("GNUBOY:ROM_ERROR");
    return;
  }
  frame = static_cast<uint16_t *>(heap_caps_malloc(160U * 144U * 2U, MALLOC_CAP_SPIRAM));
  scaled = static_cast<uint16_t *>(heap_caps_malloc(320U * 288U * 2U, MALLOC_CAP_SPIRAM));
  sound = static_cast<int16_t *>(heap_caps_malloc(
      az2::kGbAudioSamplesPerPacket * sizeof(int16_t), MALLOC_CAP_SPIRAM));
  if (!frame || !scaled || !sound ||
      gnuboy_init(az2::kGbAudioSampleRate, GB_AUDIO_MONO_S16, GB_PIXEL_565_LE,
                  videoCallback, audioCallback) != 0 ||
      gnuboy_load_rom(rom, romSize) != 0) {
    Serial.println("GNUBOY:INIT_ERROR");
    return;
  }
  gnuboy_set_framebuffer(frame);
  gnuboy_set_soundbuffer(sound, az2::kGbAudioSamplesPerPacket);
  gnuboy_reset(true);
  Serial.print("GNUBOY:HW:");
  Serial.println(gnuboy_get_hwtype() == GB_HW_CGB ? "CGB" : "DMG/GB");
  Serial.println("GNUBOY:READY");
}

void loop() {
  while (Serial1.available()) {
    static String line;
    const char c = static_cast<char>(Serial1.read());
    if (c == '\n') {
      applyLine(line);
      line = "";
    } else if (line.length() < 80) {
      line += c;
    }
  }
  if (!rom || !frame || !scaled) return;
  const uint32_t start = micros();
  gnuboy_run(true);
  workUs += micros() - start;
  ++frames;
  const uint32_t now = millis();
  if (now - lastReport >= 1000) {
    Serial.print("GNUBOY:PERF:fps=");
    Serial.print(frames);
    Serial.print(":frame_us=");
    Serial.print(frames ? workUs / frames : 0);
    Serial.print(":heap_kb=");
    Serial.print(ESP.getFreeHeap() / 1024U);
    Serial.print(":psram_kb=");
    Serial.print(ESP.getFreePsram() / 1024U);
    Serial.print(":audio_packets=");
    Serial.print(audioPackets);
    Serial.print(":audio_samples=");
    Serial.println(audioSamples);
    frames = 0;
    workUs = 0;
    lastReport = now;
  }
}
