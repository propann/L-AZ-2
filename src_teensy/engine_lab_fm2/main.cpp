// AZ-2 Engine Lab — FM2 minimal
//
// Banc isolé monophonique pour mesurer une FM deux opérateurs avant promotion
// dans le rack principal. Le moteur utilise uniquement les objets Audio
// natifs du Teensy : aucune dépendance supplémentaire et aucun effet de bord
// sur le firmware stable.

#include <Arduino.h>
#include <Audio.h>

AudioSynthWaveform fmModulator;
AudioSynthWaveformModulated fmCarrier;
AudioEffectEnvelope fmEnvelope;
AudioOutputI2S i2sOut;
AudioConnection patchModToCarrier(fmModulator, 0, fmCarrier, 0);
AudioConnection patchCarrierToEnv(fmCarrier, 0, fmEnvelope, 0);
AudioConnection patchEnvToLeft(fmEnvelope, 0, i2sOut, 0);
AudioConnection patchEnvToRight(fmEnvelope, 0, i2sOut, 1);

static float carrierHz = 261.6256f;
static float ratio = 2.0f;
static float modIndex = 2.0f;
static bool noteHeld = false;

static float midiToHz(int note) {
  return 440.0f * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

static void applyFm() {
  fmModulator.frequency(carrierHz * ratio);
  fmModulator.amplitude(noteHeld ? 0.35f : 0.0f);
  fmCarrier.frequency(carrierHz);
  fmCarrier.frequencyModulation(modIndex);
  fmCarrier.amplitude(0.8f);
}

static void panic() {
  noteHeld = false;
  fmEnvelope.noteOff();
  fmModulator.amplitude(0.0f);
}

static void handleLine(String line) {
  line.trim();
  if (line.startsWith("LAB:NOTE:")) {
    const int note = constrain(line.substring(9).toInt(), 0, 127);
    carrierHz = midiToHz(note);
    noteHeld = true;
    applyFm();
    fmEnvelope.noteOn();
    Serial.println("LAB:FM2:NOTE:OK");
  } else if (line == "LAB:OFF" || line == "LAB:PANIC") {
    panic();
    Serial.println("LAB:FM2:OFF:OK");
  } else if (line.startsWith("LAB:PARAM:")) {
    const int sep = line.indexOf(':', 10);
    if (sep < 0) {
      Serial.println("LAB:FM2:PARAM:ERROR");
      return;
    }
    const int id = line.substring(10, sep).toInt();
    const float value = constrain(line.substring(sep + 1).toFloat(), 0.0f, 12.0f);
    if (id == 0) {
      ratio = constrain(value, 0.25f, 12.0f);
    } else if (id == 1) {
      modIndex = constrain(value, 0.0f, 12.0f);
    } else {
      Serial.println("LAB:FM2:PARAM:OUT_OF_RANGE");
      return;
    }
    applyFm();
    Serial.println("LAB:FM2:PARAM:OK");
  } else if (line == "LAB:CPU?") {
    Serial.print("LAB:FM2:CPU:");
    Serial.println(AudioProcessorUsageMax(), 1);
    AudioProcessorUsageMaxReset();
  } else if (line == "LAB:RAM?") {
    Serial.print("LAB:FM2:RAM:");
    Serial.println(AudioMemoryUsageMax());
    AudioMemoryUsageMaxReset();
  }
}

void setup() {
  Serial.begin(921600);
  AudioMemory(24);
  fmModulator.begin(WAVEFORM_SINE);
  fmCarrier.begin(WAVEFORM_SINE);
  fmEnvelope.attack(3.0f);
  fmEnvelope.decay(80.0f);
  fmEnvelope.sustain(0.7f);
  fmEnvelope.release(180.0f);
  applyFm();
  Serial.println("LAB:FM2:READY");
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
