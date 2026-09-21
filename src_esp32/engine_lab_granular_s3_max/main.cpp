#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

namespace {

constexpr uint32_t kSampleRate = 44100;
constexpr size_t kSourceSamples = kSampleRate * 30U;  // 30 s mono, 2.52 Mio
constexpr size_t kBlockSamples = 128;
constexpr size_t kWindowSize = 2048;
constexpr uint16_t kMaxGrains = 192;
constexpr uint8_t kBenchmarkSeconds = 3;
constexpr uint16_t kTestCounts[] = {64, 80, 96, 112, 128, 160, 192};

struct Grain {
  float position;
  float step;
  uint32_t age;
  uint32_t duration;
  float gain;
  float pan;
};

int16_t *source = nullptr;
Grain grains[kMaxGrains] = {};
float windowTable[kWindowSize];
int16_t oscillatorTable[kWindowSize];
int16_t outputInterleaved[kBlockSamples * 2];
float partialLeft[2][kBlockSamples];
float partialRight[2][kBlockSamples];
TaskHandle_t mainTask = nullptr;
TaskHandle_t workerTask = nullptr;
volatile uint16_t workerCount = 0;
uint32_t generations[2] = {1, 0x6d2b79f5U};

uint32_t fastHash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

void resetGrain(uint16_t index, uint32_t generation, uint16_t activeGrains) {
  Grain &grain = grains[index];
  const uint32_t random = fastHash(generation + index * 104729U);
  grain.position = static_cast<float>(random % (kSourceSamples - 8192U));
  grain.step = 0.35f + static_cast<float>((random >> 9) & 2047U) / 1024.0f;
  grain.age = 0;
  grain.duration = 700U + ((random >> 20) % 7500U);
  grain.gain = 0.82f / sqrtf(static_cast<float>(activeGrains));
  grain.pan = static_cast<float>((random >> 4) & 1023U) / 1023.0f;
}

void initialiseGrains(uint16_t count) {
  generations[0] = 1;
  generations[1] = 0x6d2b79f5U;
  for (uint16_t index = 0; index < count; ++index)
    resetGrain(index, ++generations[index >= count / 2U], count);
}

void renderRange(uint16_t first, uint16_t last, uint16_t count, uint8_t lane) {
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    float left = 0.0f;
    float right = 0.0f;
    for (uint16_t index = first; index < last; ++index) {
      Grain &grain = grains[index];
      if (grain.age >= grain.duration || grain.position >= kSourceSamples - 2U)
        resetGrain(index, ++generations[lane], count);

      const uint32_t whole = static_cast<uint32_t>(grain.position);
      const float fraction = grain.position - whole;
      const float sample = source[whole] + (source[whole + 1] - source[whole]) * fraction;
      const size_t windowIndex = static_cast<size_t>(grain.age) * (kWindowSize - 1U) / grain.duration;
      const float value = sample * windowTable[windowIndex] * grain.gain;
      left += value * (1.0f - grain.pan);
      right += value * grain.pan;
      grain.position += grain.step;
      ++grain.age;
    }
    partialLeft[lane][frame] = left;
    partialRight[lane][frame] = right;
  }
}

void workerLoop(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const uint16_t count = workerCount;
    renderRange(0, count / 2U, count, 0);
    xTaskNotifyGive(mainTask);
  }
}

void renderBlockDual(uint16_t count) {
  workerCount = count;
  xTaskNotifyGive(workerTask);
  renderRange(count / 2U, count, count, 1);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  for (size_t frame = 0; frame < kBlockSamples; ++frame) {
    const float left = partialLeft[0][frame] + partialLeft[1][frame];
    const float right = partialRight[0][frame] + partialRight[1][frame];
    outputInterleaved[frame * 2] = static_cast<int16_t>(constrain(left, -32768.0f, 32767.0f));
    outputInterleaved[frame * 2 + 1] = static_cast<int16_t>(constrain(right, -32768.0f, 32767.0f));
  }
}

void benchmark(uint16_t count) {
  initialiseGrains(count);
  constexpr uint32_t blocks = (kSampleRate * kBenchmarkSeconds) / kBlockSamples;
  volatile int32_t checksum = 0;
  const uint32_t startUs = micros();
  for (uint32_t block = 0; block < blocks; ++block) {
    renderBlockDual(count);
    checksum += outputInterleaved[(block % kBlockSamples) * 2];
  }
  const uint32_t elapsedUs = micros() - startUs;
  const float realtimeUs = 1000000.0f * kBenchmarkSeconds;
  Serial.printf("GMAX:RESULT:grains=%u:render_us=%lu:wall_load=%.1f:checksum=%ld\n",
                count, static_cast<unsigned long>(elapsedUs),
                100.0f * elapsedUs / realtimeUs, static_cast<long>(checksum));
}

void runMatrix() {
  Serial.println("GMAX:MATRIX:BEGIN");
  for (uint16_t count : kTestCounts) benchmark(count);
  Serial.printf("GMAX:MEM:heap_free=%u:psram_free=%u:psram_total=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(ESP.getPsramSize()));
  Serial.println("GMAX:READY");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("GMAX:BOOT:chip=%s:cores=%u:cpu_mhz=%u:flash=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz(),
                ESP.getFlashChipSize(), ESP.getPsramSize());
  if (!psramFound() || ESP.getPsramSize() < kSourceSamples * sizeof(int16_t)) {
    Serial.println("GMAX:FAIL:PSRAM");
    return;
  }
  source = static_cast<int16_t *>(heap_caps_malloc(kSourceSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM));
  if (source == nullptr) {
    Serial.println("GMAX:FAIL:ALLOC");
    return;
  }
  for (size_t index = 0; index < kWindowSize; ++index) {
    const float phase = static_cast<float>(index) / (kWindowSize - 1U);
    windowTable[index] = 0.5f - 0.5f * cosf(2.0f * PI * phase);
    oscillatorTable[index] = static_cast<int16_t>(sinf(2.0f * PI * phase) * 28000.0f);
  }
  for (size_t index = 0; index < kSourceSamples; ++index) {
    const int32_t a = oscillatorTable[(index * 3U) & (kWindowSize - 1U)];
    const int32_t b = oscillatorTable[(index * 7U + (index >> 7)) & (kWindowSize - 1U)];
    const int32_t c = oscillatorTable[(index * 13U + (index >> 5)) & (kWindowSize - 1U)];
    source[index] = static_cast<int16_t>((a * 5 + b * 3 + c * 2) / 12);
    if ((index & 4095U) == 0) delay(1);  // nourrit le watchdog pendant les 30 s de source
  }
  Serial.printf("GMAX:SAMPLE:seconds=30:bytes=%u:memory=PSRAM\n",
                static_cast<unsigned>(kSourceSamples * sizeof(int16_t)));
  mainTask = xTaskGetCurrentTaskHandle();
  if (xTaskCreatePinnedToCore(workerLoop, "grain_core0", 4096, nullptr, 24, &workerTask, 0) != pdPASS) {
    Serial.println("GMAX:FAIL:WORKER_TASK");
    return;
  }
  Serial.println("GMAX:DUALCORE:READY:main=1:worker=0");
  runMatrix();
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runMatrix();
  }
  delay(20);
}
