#include <Arduino.h>
#include <Audio.h>
#include <synth_dexed.h>

#include "../az2_audio/az2_dexed_bank_data.h"

namespace {

constexpr uint8_t kPolyphony = 4;
AudioSynthDexed dexed(kPolyphony, SAMPLE_RATE);
AudioOutputI2S i2sOut;
AudioConnection patchLeft(dexed, 0, i2sOut, 0);
AudioConnection patchRight(dexed, 0, i2sOut, 1);

void loadPatch(uint8_t patch) {
  uint8_t packed[128];
  memcpy_P(packed, kDexedFullBank[patch % 255], sizeof(packed));
  uint8_t unpacked[156];
  dexed.decodeVoice(unpacked, packed);
  dexed.loadVoiceParameters(unpacked);
}

void reportStats() {
  Serial.printf("LAB:DEXED:CPU:now=%.1f:max=%.1f\n",
                AudioProcessorUsage(), AudioProcessorUsageMax());
  Serial.printf("LAB:DEXED:RAM:now=%u:max=%u:total=96\n",
                AudioMemoryUsage(), AudioMemoryUsageMax());
}

void handleLine(String line) {
  line.trim();
  if (line.startsWith("LAB:NOTE:")) {
    const int note = constrain(line.substring(9).toInt(), 0, 127);
    dexed.keydown(note, 100);
    Serial.println("LAB:DEXED:NOTE:OK");
  } else if (line.startsWith("LAB:OFF:")) {
    const int note = constrain(line.substring(8).toInt(), 0, 127);
    dexed.keyup(note);
    Serial.println("LAB:DEXED:OFF:OK");
  } else if (line == "LAB:PANIC") {
    dexed.panic();
    Serial.println("LAB:DEXED:PANIC:OK");
  } else if (line.startsWith("LAB:PATCH:")) {
    loadPatch(static_cast<uint8_t>(constrain(line.substring(10).toInt(), 0, 254)));
    Serial.println("LAB:DEXED:PATCH:OK");
  } else if (line == "LAB:CORE:MSFA") {
    dexed.panic();
    dexed.setEngineType(MSFA);
    Serial.println("LAB:DEXED:CORE:MSFA:OK");
  } else if (line == "LAB:CORE:MKI") {
    dexed.panic();
    dexed.setEngineType(MKI);
    Serial.println("LAB:DEXED:CORE:MKI:OK");
  } else if (line == "LAB:STATS?") {
    reportStats();
  } else if (line == "LAB:RESETMAX") {
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
    Serial.println("LAB:DEXED:RESETMAX:OK");
  } else {
    Serial.println("LAB:DEXED:ERROR:UNKNOWN_COMMAND");
  }
}

}  // namespace

void setup() {
  Serial.begin(921600);
  AudioMemory(96);
  dexed.setEngineType(MSFA);
  loadPatch(0);
  delay(100);
  Serial.println("LAB:DEXED:READY:core=MSFA:poly=4");
}

void loop() {
  static String line;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        handleLine(line);
        line = "";
      }
    } else if (line.length() < 80) {
      line += c;
    }
  }
}
