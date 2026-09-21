#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "rack_pins.h"

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
float coreMix[2][kBlockSamples] = {};
int16_t outputBlock[kBlockSamples];
int16_t outputInterleaved[kBlockSamples * 2];
TaskHandle_t mainTask = nullptr, workerTask = nullptr;
volatile uint8_t requestedPartials = 16;
I2SClass i2s;

inline float lookup(float p) {
  const uint32_t whole = static_cast<uint32_t>(p);
  const uint32_t next = (whole + 1U) & (kTableSize - 1U);
  const float fraction = p - whole;
  return sineTable[whole] + (sineTable[next] - sineTable[whole]) * fraction;
}

void prepareSpectrum(uint8_t partialCount, float morph) {
  constexpr float roots[kVoiceCount] = {55.0f, 73.416f, 82.407f, 110.0f};
  const float exponent = 0.72f + morph * 1.5f;
  for (uint8_t partial = 0; partial < partialCount; ++partial) {
    const float harmonic = partial + 1.0f;
    const float stretch = 1.0f + morph * partial * 0.0009f;
    amplitude[partial] = 1.0f / powf(harmonic, exponent);
    for (uint8_t voice = 0; voice < kVoiceCount; ++voice)
      phaseIncrement[voice][partial] = roots[voice] * harmonic * stretch * kTableSize / kSampleRate;
  }
}

void renderVoices(uint8_t firstVoice, uint8_t lastVoice, uint8_t partialCount, float *destination) {
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    float mix = 0.0f;
    for (uint8_t voice = firstVoice; voice < lastVoice; ++voice) {
      float spectrum = 0.0f;
      for (uint8_t partial = 0; partial < partialCount; ++partial) {
        float nextPhase = phase[voice][partial] + phaseIncrement[voice][partial];
        if (nextPhase >= kTableSize) nextPhase -= kTableSize;
        phase[voice][partial] = nextPhase;
        spectrum += lookup(nextPhase) * amplitude[partial];
      }
      state1[voice] += 0.075f * (spectrum - state1[voice]);
      state2[voice] += 0.018f * (tanhf(state1[voice] * 0.7f) - state2[voice]);
      mix += tanhf(spectrum * 0.035f + state2[voice] * 0.8f);
    }
    destination[frame] = mix;
  }
}

void worker(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    renderVoices(0, 2, requestedPartials, coreMix[0]);
    xTaskNotifyGive(mainTask);
  }
}

void renderBlock(uint8_t partialCount, float morph) {
  prepareSpectrum(partialCount, morph);
  requestedPartials = partialCount;
  xTaskNotifyGive(workerTask);
  renderVoices(2, 4, partialCount, coreMix[1]);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    const float mix = coreMix[0][frame] + coreMix[1][frame];
    outputBlock[frame] = static_cast<int16_t>(constrain(mix * 7000.0f, -32768.0f, 32767.0f));
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
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  for (size_t index = 0; index < kTableSize; ++index)
    sineTable[index] = sinf(2.0f * PI * index / kTableSize);
  mainTask = xTaskGetCurrentTaskHandle();
  xTaskCreatePinnedToCore(worker, "spectral-core0", 4096, nullptr, 3, &workerTask, 0);
  Serial.printf("SPECTRAL:BOOT:chip=%s:revision=%u:cores=%u:cpu_mhz=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                ESP.getCpuFreqMHz(), ESP.getPsramSize());
  runBenchmarkMatrix();
  runRealtimeI2S(kRealtimeTestSeconds);
  Serial.println("SPECTRAL:ALL_TESTS:READY");
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runBenchmarkMatrix();
    runRealtimeI2S(kRealtimeTestSeconds);
    Serial.println("SPECTRAL:ALL_TESTS:READY");
  }
  delay(20);
}
