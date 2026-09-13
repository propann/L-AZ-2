#include <Arduino.h>
#include <Audio.h>
#include <math.h>
#include <AZ2_Protocol.h>

namespace {

AudioSynthWaveformSine sineVoice;
AudioAmplifier masterAmp;
AudioOutputI2S i2sOut;
AudioConnection patchCord1(sineVoice, 0, masterAmp, 0);
AudioConnection patchCord2(masterAmp, 0, i2sOut, 0);
AudioConnection patchCord3(masterAmp, 0, i2sOut, 1);

String inputLine;
uint32_t lastStatusMs = 0;
bool playing = false;

float padToFrequency(uint8_t pad) {
  constexpr float kBaseFrequency = 55.0f;
  return kBaseFrequency * powf(2.0f, static_cast<float>(pad) / 12.0f);
}

void setPlaying(bool enabled) {
  playing = enabled;
  masterAmp.gain(enabled ? 0.18f : 0.0f);
}

void handlePadCommand(const String &line) {
  const int firstColon = line.indexOf(':');
  const int secondColon = line.indexOf(':', firstColon + 1);
  if (firstColon < 0 || secondColon < 0) {
    return;
  }

  const uint8_t pad = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
  if (!az2::validPad(pad)) {
    return;
  }

  const bool pressed = line.indexOf(":DOWN") > 0;
  if (pressed) {
    sineVoice.frequency(padToFrequency(pad));
    setPlaying(true);
    az2::printLedEvent(Serial, pad, "ON");
  } else {
    setPlaying(false);
    az2::printLedEvent(Serial, pad, "OFF");
  }
}

void handleCommand(const String &line) {
  if (line == az2::kPlay) {
    setPlaying(true);
    Serial.println("STATE:PLAYING");
    return;
  }

  if (line == az2::kStop) {
    setPlaying(false);
    Serial.println("STATE:STOPPED");
    return;
  }

  if (line.startsWith("PAD:")) {
    handlePadCommand(line);
    return;
  }

  if (line == az2::kHelloControl) {
    Serial.println(az2::kHelloAudio);
    return;
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      inputLine.trim();
      if (inputLine.length() > 0) {
        handleCommand(inputLine);
      }
      inputLine = "";
      continue;
    }

    if (inputLine.length() < 96) {
      inputLine += c;
    }
  }
}

void sendStatus() {
  const uint32_t now = millis();
  if (now - lastStatusMs < 1000) {
    return;
  }

  lastStatusMs = now;
  Serial.print("STATUS:TEENSY_AUDIO:");
  Serial.println(playing ? "PLAYING" : "READY");
}

} // namespace

void setup() {
  Serial.begin(az2::kControlBaud);
  AudioMemory(12);

  sineVoice.begin(0.8f, 110.0f, WAVEFORM_SINE);
  setPlaying(false);

  delay(300);
  Serial.println(az2::kHelloAudio);
}

void loop() {
  readSerialCommands();
  sendStatus();
}
