#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "rack_pins.h"
#include "AZ2_Protocol.h"

namespace {
constexpr uint32_t kSampleRate = 44100;
constexpr size_t kBlockSamples = 128;
constexpr uint8_t kMaxPartials = 64;
constexpr uint8_t kVoiceCount = 4;
constexpr uint32_t kBenchmarkSeconds = 3;
constexpr uint32_t kRealtimeTestSeconds = 30;
constexpr uint8_t kRealtimePartials = 16;
constexpr size_t kTableSize = 2048;

float sineTable[kTableSize], phase[kVoiceCount][kMaxPartials] = {};
float phaseIncrement[kVoiceCount][kMaxPartials] = {}, amplitude[kMaxPartials] = {};
float state1[kVoiceCount] = {}, state2[kVoiceCount] = {};
float coreLeft[2][kBlockSamples] = {}, coreRight[2][kBlockSamples] = {};
int16_t outputBlock[kBlockSamples];
int16_t outputInterleaved[kBlockSamples * 2];
#ifdef AZ2_RACK_SLAVE
int32_t rackOutputInterleaved[kBlockSamples * 2];
#endif
TaskHandle_t mainTask = nullptr, workerTask = nullptr;
volatile uint8_t requestedPartials = 16;
I2SClass i2s;

uint8_t spectralParams[17] = {
    96, 52, 18, 64, 64, 12, 90, 28, 54, 16, 4, 32, 100, 42, 112, 18, 100,
};
uint8_t spectralNote = 48;
uint8_t spectralVelocity = 100;
bool spectralGate = false;
bool spectralEnabled = true;
float spectralEnvelope = 0.0f;
float filterLeft = 0.0f, filterRight = 0.0f;
float motionPhase = 0.0f;

uint8_t spectralPresets[az2::kRackPatchCount][az2::kRackSpectralParamCount];

inline float lookup(float p) {
  const uint32_t whole = static_cast<uint32_t>(p);
  const uint32_t next = (whole + 1U) & (kTableSize - 1U);
  const float fraction = p - whole;
  return sineTable[whole] + (sineTable[next] - sineTable[whole]) * fraction;
}

void prepareSpectrum(uint8_t partialCount, float morph) {
  const float root = 440.0f * powf(2.0f, (static_cast<int>(spectralNote) - 69) / 12.0f);
  const float detune = spectralParams[5] / 127.0f * 0.025f;
  const float voiceRatio[kVoiceCount] = {1.0f - detune, 1.0f - detune * 0.33f,
                                         1.0f + detune * 0.33f, 1.0f + detune};
  const float exponent = 0.45f + morph * 2.1f;
  const float stretchAmount = spectralParams[2] / 127.0f * 0.018f;
  const float tilt = (static_cast<int>(spectralParams[3]) - 64) / 64.0f;
  const float oddEven = (static_cast<int>(spectralParams[4]) - 64) / 64.0f;
  for (uint8_t partial = 0; partial < partialCount; ++partial) {
    const float harmonic = partial + 1.0f;
    const float stretch = 1.0f + stretchAmount * partial;
    const float parity = (partial & 1U) ? -oddEven : oddEven;
    amplitude[partial] = max(0.0f, (1.0f + parity * 0.8f) *
                                      powf(harmonic, tilt * 0.35f) /
                                      powf(harmonic, exponent));
    for (uint8_t voice = 0; voice < kVoiceCount; ++voice)
      phaseIncrement[voice][partial] = root * voiceRatio[voice] * harmonic * stretch *
                                       kTableSize / kSampleRate;
  }
}

void renderVoices(uint8_t firstVoice, uint8_t lastVoice, uint8_t partialCount,
                  float *leftDestination, float *rightDestination) {
  const float spread = spectralParams[6] / 127.0f;
  const float character = spectralParams[8] / 127.0f;
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    float left = 0.0f, right = 0.0f;
    for (uint8_t voice = firstVoice; voice < lastVoice; ++voice) {
      float spectrum = 0.0f;
      for (uint8_t partial = 0; partial < partialCount; ++partial) {
        const float motion = 1.0f + sinf(motionPhase + voice * 1.7f + partial * 0.11f) *
                                      (spectralParams[7] / 127.0f) * 0.0025f;
        float nextPhase = phase[voice][partial] + phaseIncrement[voice][partial] * motion;
        if (nextPhase >= kTableSize) nextPhase -= kTableSize;
        phase[voice][partial] = nextPhase;
        spectrum += lookup(nextPhase) * amplitude[partial];
      }
      state1[voice] += 0.075f * (spectrum - state1[voice]);
      state2[voice] += 0.018f * (tanhf(state1[voice] * 0.7f) - state2[voice]);
      const float value = tanhf(spectrum * (0.025f + (1.0f - character) * 0.025f) +
                                 state2[voice] * character);
      const float pan = 0.5f + ((voice / 3.0f) - 0.5f) * spread;
      left += value * (1.0f - pan);
      right += value * pan;
    }
    leftDestination[frame] = left;
    rightDestination[frame] = right;
  }
}

void worker(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    renderVoices(0, 2, requestedPartials, coreLeft[0], coreRight[0]);
    xTaskNotifyGive(mainTask);
  }
}

void renderBlock(uint8_t partialCount, float morph) {
  prepareSpectrum(partialCount, morph);
  requestedPartials = partialCount;
  xTaskNotifyGive(workerTask);
  renderVoices(2, 4, partialCount, coreLeft[1], coreRight[1]);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  const float attackSeconds = 0.002f + spectralParams[10] * spectralParams[10] *
                              (3.0f / (127.0f * 127.0f));
  const float decaySeconds = 0.005f + spectralParams[11] * spectralParams[11] *
                             (4.0f / (127.0f * 127.0f));
  const float releaseSeconds = 0.005f + spectralParams[13] * spectralParams[13] *
                               (6.0f / (127.0f * 127.0f));
  const float sustain = spectralParams[12] / 127.0f;
  const float cutoff = spectralParams[14] / 127.0f;
  const float filterCoefficient = 0.002f + cutoff * cutoff * 0.65f;
  const float resonance = spectralParams[15] / 127.0f * 0.65f;
  const float drive = 1.0f + spectralParams[9] / 127.0f * 7.0f;
  const float level = spectralParams[16] / 127.0f;
  const float velocity = spectralVelocity / 127.0f;
  const float attackStep = 1.0f / (kSampleRate * attackSeconds);
  const float releaseStep = 1.0f / (kSampleRate * releaseSeconds);
  const float decayDivisor = kSampleRate * decaySeconds;
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    if (spectralGate && spectralEnabled) {
      spectralEnvelope = min(1.0f, spectralEnvelope + attackStep);
      if (spectralEnvelope >= 0.999f)
        spectralEnvelope += (sustain - spectralEnvelope) / decayDivisor;
    } else {
      spectralEnvelope = max(0.0f, spectralEnvelope - releaseStep);
    }
    float samples[2] = {coreLeft[0][frame] + coreLeft[1][frame],
                        coreRight[0][frame] + coreRight[1][frame]};
    for (uint8_t channel = 0; channel < 2; ++channel) {
      float &filter = channel ? filterRight : filterLeft;
      const float raw = samples[channel] * 10500.0f * spectralEnvelope * velocity;
      filter += filterCoefficient * (raw - filter);
      float value = filter + (raw - filter) * resonance;
      const float driven = value / 32768.0f * drive;
      value = driven / (1.0f + fabsf(driven)) * (32768.0f / drive) * level;
      outputInterleaved[frame * 2 + channel] =
          static_cast<int16_t>(constrain(value, -32768.0f, 32767.0f));
    }
    outputBlock[frame] = static_cast<int16_t>((static_cast<int32_t>(outputInterleaved[frame * 2]) +
                                               outputInterleaved[frame * 2 + 1]) / 2);
    motionPhase += 0.00001f + spectralParams[7] / 127.0f * 0.0012f;
    if (motionPhase > 2.0f * PI) motionPhase -= 2.0f * PI;
  }
}

void runOneBenchmark(uint8_t partialCount) {
  const uint32_t blocks = (kSampleRate * kBenchmarkSeconds) / kBlockSamples;
  volatile int32_t checksum = 0;
  const uint32_t startUs = micros();
  for (uint32_t block = 0; block < blocks; ++block) {
    renderBlock(partialCount, 0.5f + 0.5f * sinf(block * 0.003f));
    checksum += outputBlock[block % kBlockSamples];
  }
  const uint32_t elapsedUs = micros() - startUs;
  const float realtimeLoad = 100.0f * elapsedUs / (1000000.0f * kBenchmarkSeconds);
  Serial.printf("SPECTRAL:RESULT:voices=%u:partials=%u:oscillators=%u:rate=%lu:seconds=%lu:render_us=%lu:realtime_load=%.1f:checksum=%ld\n",
                kVoiceCount, partialCount, kVoiceCount * partialCount,
                static_cast<unsigned long>(kSampleRate), static_cast<unsigned long>(kBenchmarkSeconds),
                static_cast<unsigned long>(elapsedUs), realtimeLoad, static_cast<long>(checksum));
}

void runBenchmarkMatrix() {
  constexpr uint8_t counts[] = {16, 24, 32, 48, 64};
  Serial.println("SPECTRAL:MATRIX:BEGIN");
  for (const uint8_t count : counts) runOneBenchmark(count);
  Serial.printf("SPECTRAL:HEAP:internal_free=%u:psram=%u\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(ESP.getPsramSize()));
  Serial.println("SPECTRAL:MATRIX:END");
}

void runRealtimeI2S(uint32_t seconds) {
  i2s.setPins(az2::spectral::kI2sBclkPin, az2::spectral::kI2sWsPin,
              az2::spectral::kI2sDataOutPin);
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO)) {
    Serial.printf("SPECTRAL:I2S:FAIL:error=%d\n", i2s.lastError());
    return;
  }

  constexpr uint32_t budgetUs = (1000000UL * kBlockSamples) / kSampleRate;
  const uint32_t blocks = (kSampleRate * seconds) / kBlockSamples;
  uint32_t late = 0, shortWrites = 0, maxRenderUs = 0;
  const uint32_t started = millis();
  for (uint32_t block = 0; block < blocks; ++block) {
    const uint32_t renderStart = micros();
    renderBlock(kRealtimePartials, 0.5f + 0.5f * sinf(block * 0.003f));
    for (size_t frame = 0; frame < kBlockSamples; ++frame) {
      outputInterleaved[frame * 2] = outputBlock[frame];
      outputInterleaved[frame * 2 + 1] = outputBlock[frame];
    }
    const uint32_t renderUs = micros() - renderStart;
    if (renderUs > maxRenderUs) maxRenderUs = renderUs;
    if (renderUs > budgetUs) ++late;
    if (i2s.write(outputInterleaved, sizeof(outputInterleaved)) != sizeof(outputInterleaved))
      ++shortWrites;
  }
  Serial.printf("SPECTRAL:I2S:RESULT:voices=%u:partials=%u:seconds=%lu:elapsed_ms=%lu:blocks=%lu:late=%lu:short=%lu:max_render_us=%lu:budget_us=%lu\n",
                kVoiceCount, kRealtimePartials, static_cast<unsigned long>(seconds),
                static_cast<unsigned long>(millis() - started), static_cast<unsigned long>(blocks),
                static_cast<unsigned long>(late), static_cast<unsigned long>(shortWrites),
                static_cast<unsigned long>(maxRenderUs), static_cast<unsigned long>(budgetUs));
  i2s.end();
}

#ifdef AZ2_RACK_SLAVE
HardwareSerial rackControl(2);

void applySpectralPreset(uint8_t slot) {
  if (slot < 8) memcpy(spectralParams, spectralPresets[slot], sizeof(spectralParams));
}

void handleRackCommand(const String &line) {
  if (line == "RACK_ENGINE:SPECTRAL:ON") {
    spectralEnabled = true;
    rackControl.println("READY:ENGINE:ON");
  } else if (line == "RACK_ENGINE:SPECTRAL:OFF") {
    spectralEnabled = false;
    spectralGate = false;
    rackControl.println("READY:ENGINE:OFF");
  } else if (line == "RACK:PANIC") {
    spectralGate = false;
    spectralEnvelope = 0.0f;
    rackControl.println("READY:PANIC");
  } else if (line.startsWith("RACK_NOTE_ON:SPECTRAL:")) {
    const int split = line.lastIndexOf(':');
    const int previous = line.lastIndexOf(':', split - 1);
    spectralNote = constrain(line.substring(previous + 1, split).toInt(), 0, 127);
    spectralVelocity = constrain(line.substring(split + 1).toInt(), 1, 127);
    spectralGate = true;
    spectralEnabled = true;
    rackControl.printf("READY:NOTE_ON:%u:%u\n", spectralNote, spectralVelocity);
  } else if (line.startsWith("RACK_NOTE_OFF:SPECTRAL:")) {
    spectralGate = false;
    rackControl.printf("READY:NOTE_OFF:%u\n", spectralNote);
  } else if (line.startsWith("RACK_PARAM:SPECTRAL:")) {
    const int split = line.lastIndexOf(':');
    const int previous = line.lastIndexOf(':', split - 1);
    const int parameter = line.substring(previous + 1, split).toInt();
    const int value = line.substring(split + 1).toInt();
    if (parameter >= 0 && parameter < 17 && value >= 0 && value <= 127) {
      spectralParams[parameter] = static_cast<uint8_t>(value);
      rackControl.printf("READY:PARAM:%d:%d\n", parameter, value);
    } else {
      rackControl.println("ERROR:PARAM");
    }
  } else if (line.startsWith("RACK_PATCH:SPECTRAL:")) {
    const int slot = line.substring(line.lastIndexOf(':') + 1).toInt();
    if (slot >= 0 && slot < 8) {
      applySpectralPreset(slot);
      rackControl.printf("READY:PATCH:%d\n", slot);
    } else {
      rackControl.println("ERROR:PATCH");
    }
  } else if (line.startsWith("RACK_PATCH_SAVE:SPECTRAL:")) {
    const int slot = line.substring(line.lastIndexOf(':') + 1).toInt();
    if (slot >= 0 && slot < 8) {
      memcpy(spectralPresets[slot], spectralParams, sizeof(spectralParams));
      rackControl.printf("READY:PATCH_SAVED:%d\n", slot);
    } else {
      rackControl.println("ERROR:PATCH_SAVE");
    }
  } else if (line == "RACK:STATUS") {
    rackControl.printf("READY:STATUS:gate=%u:env=%.3f:note=%u:partials=%u:heap=%u\n",
                       spectralGate, spectralEnvelope, spectralNote,
                       1U + spectralParams[0] * 15U / 127U,
                       static_cast<unsigned>(ESP.getFreeHeap()));
  }
}

void readRackCommands() {
  static String command;
  while (rackControl.available()) {
    const char c = static_cast<char>(rackControl.read());
    if (c == '\r') continue;
    if (c == '\n') {
      command.trim();
      if (command.length()) handleRackCommand(command);
      command = "";
    } else if (command.length() < 95) {
      command += c;
    } else {
      command = "";
    }
  }
}

void runRackSlave() {
  rackControl.begin(az2::spectral::kControlBaud, SERIAL_8N1,
                    az2::spectral::kControlRxPin, az2::spectral::kControlTxPin);
  i2s.setPins(az2::spectral::kI2sBclkPin, az2::spectral::kI2sWsPin,
              az2::spectral::kI2sDataOutPin);
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_STEREO, -1, I2S_ROLE_SLAVE)) {
    Serial.printf("SPECTRAL:RACK:FAIL:I2S:error=%d\n", i2s.lastError());
    return;
  }
  Serial.printf("SPECTRAL:RACK:READY:role=slave:bclk=%d:ws=%d:dout=%d\n",
                az2::spectral::kI2sBclkPin, az2::spectral::kI2sWsPin,
                az2::spectral::kI2sDataOutPin);
  uint32_t blocks = 0, shortWrites = 0, lastReport = millis();
  for (;;) {
    readRackCommands();
    const uint8_t partialCount = 1U + spectralParams[0] * 15U / 127U;
    renderBlock(partialCount, spectralParams[1] / 127.0f);
    for (size_t frame = 0; frame < kBlockSamples; ++frame) {
      rackOutputInterleaved[frame * 2] = static_cast<int32_t>(outputInterleaved[frame * 2]) << 16;
      rackOutputInterleaved[frame * 2 + 1] =
          static_cast<int32_t>(outputInterleaved[frame * 2 + 1]) << 16;
    }
    if (i2s.write(rackOutputInterleaved, sizeof(rackOutputInterleaved)) != sizeof(rackOutputInterleaved))
      ++shortWrites;
    ++blocks;
    const uint32_t now = millis();
    if (now - lastReport >= 1000) {
      Serial.printf("SPECTRAL:RACK:STREAM:blocks=%lu:short=%lu:heap=%u\n",
                    static_cast<unsigned long>(blocks),
                    static_cast<unsigned long>(shortWrites),
                    static_cast<unsigned>(ESP.getFreeHeap()));
      lastReport = now;
    }
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  for (size_t index = 0; index < kTableSize; ++index)
    sineTable[index] = sinf(2.0f * PI * index / kTableSize);
  memcpy(spectralPresets, az2::kRackSpectralPresets, sizeof(spectralPresets));
  mainTask = xTaskGetCurrentTaskHandle();
  xTaskCreatePinnedToCore(worker, "spectral-core0", 4096, nullptr, 3, &workerTask, 0);
  Serial.printf("SPECTRAL:BOOT:chip=%s:revision=%u:cores=%u:cpu_mhz=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                ESP.getCpuFreqMHz(), ESP.getPsramSize());
#ifdef AZ2_RACK_SLAVE
  runRackSlave();
#else
  runBenchmarkMatrix();
  runRealtimeI2S(kRealtimeTestSeconds);
  Serial.println("SPECTRAL:ALL_TESTS:READY");
#endif
}

void loop() {
#ifndef AZ2_RACK_SLAVE
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runBenchmarkMatrix();
    runRealtimeI2S(kRealtimeTestSeconds);
    Serial.println("SPECTRAL:ALL_TESTS:READY");
  }
#endif
  delay(20);
}
