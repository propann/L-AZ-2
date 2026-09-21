#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

namespace {

constexpr uint32_t kSampleRate = 44100;
constexpr size_t kBlockSamples = 128;
constexpr uint8_t kPartialCount = 64;
constexpr uint8_t kVoiceCount = 4;
constexpr uint32_t kBenchmarkSeconds = 10;
constexpr size_t kTableSize = 2048;

float sineTable[kTableSize];
float phase[kVoiceCount][kPartialCount] = {};
float state1[kVoiceCount] = {};
float state2[kVoiceCount] = {};
int16_t outputBlock[kBlockSamples];

float lookup(float p) {
  while (p >= kTableSize) p -= kTableSize;
  const uint32_t whole = static_cast<uint32_t>(p);
  const uint32_t next = (whole + 1U) & (kTableSize - 1U);
  const float fraction = p - whole;
  return sineTable[whole] + (sineTable[next] - sineTable[whole]) * fraction;
}

void renderBlock(float morph) {
  constexpr float roots[kVoiceCount] = {55.0f, 73.416f, 82.407f, 110.0f};
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    float mix = 0.0f;
    for (uint8_t voice = 0; voice < kVoiceCount; ++voice) {
      float spectrum = 0.0f;
      for (uint8_t partial = 0; partial < kPartialCount; ++partial) {
        const float harmonic = partial + 1.0f;
        const float stretch = 1.0f + morph * partial * 0.0009f;
        const float frequency = roots[voice] * harmonic * stretch;
        phase[voice][partial] += frequency * kTableSize / kSampleRate;
        if (phase[voice][partial] >= kTableSize) phase[voice][partial] -= kTableSize;
        const float tilt = 1.0f / powf(harmonic, 0.72f + morph * 1.5f);
        spectrum += lookup(phase[voice][partial]) * tilt;
      }
      // Deux integrateurs non lineaires : texture resonante mouvante.
      state1[voice] += 0.075f * (spectrum - state1[voice]);
      state2[voice] += 0.018f * (tanhf(state1[voice] * 0.7f) - state2[voice]);
      mix += tanhf((spectrum * 0.035f + state2[voice] * 0.8f));
    }
    outputBlock[frame] = static_cast<int16_t>(constrain(mix * 7000.0f, -32768.0f, 32767.0f));
  }
}

void runBenchmark() {
  constexpr uint32_t blocks = (kSampleRate * kBenchmarkSeconds) / kBlockSamples;
  volatile int32_t checksum = 0;
  const uint32_t startUs = micros();
  for (uint32_t block = 0; block < blocks; ++block) {
    const float morph = 0.5f + 0.5f * sinf(block * 0.003f);
    renderBlock(morph);
    checksum += outputBlock[block % kBlockSamples];
  }
  const uint32_t elapsedUs = micros() - startUs;
  const float cpuEstimate = 100.0f * elapsedUs / (1000000.0f * kBenchmarkSeconds);
  Serial.printf("SPECTRAL:RESULT:voices=%u:partials=%u:rate=%lu:seconds=%lu:render_us=%lu:cpu_est=%.1f:checksum=%ld\n",
                kVoiceCount, kPartialCount, static_cast<unsigned long>(kSampleRate),
                static_cast<unsigned long>(kBenchmarkSeconds), static_cast<unsigned long>(elapsedUs),
                cpuEstimate, static_cast<long>(checksum));
  Serial.printf("SPECTRAL:HEAP:internal_free=%u:psram=%u\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(ESP.getPsramSize()));
}

}  // namespace

void setup() {
  Serial.begin(921600);
  delay(1500);
  for (size_t index = 0; index < kTableSize; ++index)
    sineTable[index] = sinf(2.0f * PI * index / kTableSize);
  Serial.printf("SPECTRAL:BOOT:chip=%s:cpu_mhz=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getPsramSize());
  runBenchmark();
  Serial.println("SPECTRAL:READY");
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runBenchmark();
    Serial.println("SPECTRAL:READY");
  }
  delay(20);
}
