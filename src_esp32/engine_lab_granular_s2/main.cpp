#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

namespace {

constexpr uint32_t kSampleRate = 44100;
// 32768 echantillons = 743 ms / 64 Kio : tient dans la RAM interne d'un
// WROOM-32D sans PSRAM, tout en restant assez long pour le banc granulaire.
constexpr size_t kSourceSamples = 32768;
constexpr size_t kBlockSamples = 128;
constexpr uint8_t kGrainCount = 16;
constexpr uint32_t kBenchmarkSeconds = 10;
constexpr size_t kWindowSize = 1024;

struct Grain {
  float position;
  float step;
  uint32_t age;
  uint32_t duration;
  float gain;
};

int16_t *source = nullptr;
Grain grains[kGrainCount] = {};
int16_t outputBlock[kBlockSamples];
float windowTable[kWindowSize];

void resetGrain(uint8_t index, uint32_t seed) {
  Grain &grain = grains[index];
  const uint32_t safeSpan = static_cast<uint32_t>(kSourceSamples - 4096);
  grain.position = static_cast<float>((seed * 7919U + index * 104729U) % safeSpan);
  grain.step = 0.55f + static_cast<float>((index * 37U) % 100U) / 100.0f;
  grain.age = 0;
  grain.duration = 900U + ((seed + index * 251U) % 3100U);
  grain.gain = 0.75f / static_cast<float>(kGrainCount);
}

void renderBlock(uint32_t &generation) {
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    float mix = 0.0f;
    for (uint8_t index = 0; index < kGrainCount; ++index) {
      Grain &grain = grains[index];
      if (grain.age >= grain.duration || grain.position >= kSourceSamples - 2) {
        resetGrain(index, ++generation);
      }

      const uint32_t whole = static_cast<uint32_t>(grain.position);
      const float fraction = grain.position - static_cast<float>(whole);
      const float sample = static_cast<float>(source[whole]) +
                           (static_cast<float>(source[whole + 1]) - source[whole]) * fraction;
      const size_t windowIndex = static_cast<size_t>(grain.age) * (kWindowSize - 1U) / grain.duration;
      const float window = windowTable[windowIndex];
      mix += sample * window * grain.gain;
      grain.position += grain.step;
      ++grain.age;
    }
    outputBlock[frame] = static_cast<int16_t>(constrain(mix, -32768.0f, 32767.0f));
  }
}

void runBenchmark() {
  uint32_t generation = 1;
  for (uint8_t index = 0; index < kGrainCount; ++index) resetGrain(index, generation++);

  constexpr uint32_t blocks = (kSampleRate * kBenchmarkSeconds) / kBlockSamples;
  volatile int32_t checksum = 0;
  const uint32_t startUs = micros();
  for (uint32_t block = 0; block < blocks; ++block) {
    renderBlock(generation);
    checksum += outputBlock[block % kBlockSamples];
  }
  const uint32_t elapsedUs = micros() - startUs;
  const float realtimeUs = 1000000.0f * static_cast<float>(kBenchmarkSeconds);
  const float cpuEstimate = 100.0f * static_cast<float>(elapsedUs) / realtimeUs;

  Serial.printf("GRANULAR:RESULT:grains=%u:rate=%lu:seconds=%lu:render_us=%lu:cpu_est=%.1f:checksum=%ld\n",
                kGrainCount, static_cast<unsigned long>(kSampleRate),
                static_cast<unsigned long>(kBenchmarkSeconds),
                static_cast<unsigned long>(elapsedUs), cpuEstimate,
                static_cast<long>(checksum));
  Serial.printf("GRANULAR:HEAP:internal_free=%u:psram_free=%u\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(ESP.getFreePsram()));
}

}  // namespace

void setup() {
  Serial.begin(921600);
  delay(1500);
  Serial.printf("GRANULAR:BOOT:chip=%s:cores=%u:cpu_mhz=%u:flash=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz(),
                ESP.getFlashChipSize(), ESP.getPsramSize());

  const bool hasPsram = psramFound();
  const uint32_t caps = hasPsram ? MALLOC_CAP_SPIRAM : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  source = static_cast<int16_t *>(heap_caps_malloc(kSourceSamples * sizeof(int16_t), caps));
  if (source == nullptr) {
    Serial.println("GRANULAR:FAIL:SAMPLE_ALLOC");
    return;
  }

  for (size_t index = 0; index < kWindowSize; ++index) {
    const float phase = static_cast<float>(index) / static_cast<float>(kWindowSize - 1U);
    windowTable[index] = 0.5f - 0.5f * cosf(2.0f * PI * phase);
  }

  for (size_t index = 0; index < kSourceSamples; ++index) {
    const float phase = 2.0f * PI * 110.0f * static_cast<float>(index) / kSampleRate;
    const float harmonic = sinf(phase) * 0.65f + sinf(phase * 2.01f) * 0.25f;
    source[index] = static_cast<int16_t>(harmonic * 28000.0f);
  }
  Serial.printf("GRANULAR:SAMPLES:memory=%s:source_bytes=%u\n", hasPsram ? "PSRAM" : "INTERNAL",
                static_cast<unsigned>(kSourceSamples * sizeof(int16_t)));
  runBenchmark();
  Serial.println("GRANULAR:READY");
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runBenchmark();
    Serial.println("GRANULAR:READY");
  }
  delay(20);
}
