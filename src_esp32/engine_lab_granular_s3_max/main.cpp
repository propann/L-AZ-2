#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "rack_pins.h"
#include "../../src_teensy/az2_audio/az2_sampler_data.h"

namespace {

constexpr uint32_t kSampleRate = 44100;
constexpr size_t kSourceCapacityBytes = 5U * 1024U * 1024U / 2U;  // 2,5 Mio par banque
constexpr size_t kSourceCapacitySamples = kSourceCapacityBytes / sizeof(int16_t);
constexpr size_t kBlockSamples = 128;
constexpr size_t kWindowSize = 2048;
constexpr uint16_t kMaxGrains = 192;
constexpr uint16_t kRealtimeGrains = 64;
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
int16_t *stagingSource = nullptr;
size_t sourceSampleCount = kSourceCapacitySamples;
float sourceNormalization = 1.0f;
Grain grains[kMaxGrains] = {};
float windowTable[kWindowSize];
int16_t outputInterleaved[kBlockSamples * 2];
int16_t spectralInterleaved[kBlockSamples * 2];
#ifdef AZ2_TEENSY_CLOCK_SLAVE
int32_t rackInput32[kBlockSamples * 2];
int32_t rackOutput32[kBlockSamples * 2];
#endif
float partialLeft[2][kBlockSamples];
float partialRight[2][kBlockSamples];
TaskHandle_t mainTask = nullptr;
TaskHandle_t workerTask = nullptr;
volatile uint16_t workerCount = 0;
uint32_t generations[2] = {1, 0x6d2b79f5U};
I2SClass i2s;
void initialiseGrains(uint16_t count);
uint8_t granularParams[18] = {
    64, 52, 60, 64, 8, 32, 100, 0, 64, 0,
    4, 35, 100, 45, 110, 20, 18, 100,
};
uint8_t granularNote = 60;
uint8_t granularVelocity = 100;
bool granularGate = false;
float granularEnvelope = 0.0f;
float granularFilterL = 0.0f;
float granularFilterR = 0.0f;
#ifdef AZ2_RACK_AGGREGATOR
size_t sampleRxBytesRemaining = 0;
size_t sampleRxBytesExpected = 0;
size_t sampleRxOffset = 0;
size_t sampleRxSamples = 0;
uint32_t sampleRxRate = 44100;
uint32_t sampleRxExpectedCrc = 0;
uint32_t sampleRxCrc = 0xFFFFFFFFU;

uint32_t crc32Byte(uint32_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8; ++bit)
    crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  return crc;
}
#endif
#ifdef AZ2_RACK_AGGREGATOR
HardwareSerial rackControl(1);
HardwareSerial spectralControl(2);
float rackGain = 0.0f;
float rackTargetGain = 0.0f;
float granularGain = 1.0f;
float granularTargetGain = 1.0f;
float spectralGain = 1.0f;
float spectralTargetGain = 1.0f;

void handleRackCommand(const String &line, Stream &reply) {
  if (line == "RACK:ON" || line.startsWith("NOTE_ON:")) {
    rackTargetGain = 1.0f;
    reply.println("GMAX:RACK:AUDIO:ON");
  } else if (line == "RACK:OFF" || line == "RACK:PANIC" ||
             line == "PANIC" || line.startsWith("NOTE_OFF:")) {
    rackTargetGain = 0.0f;
    if (line == "RACK:PANIC" || line == "PANIC") spectralControl.println("RACK:PANIC");
    reply.println("GMAX:RACK:AUDIO:OFF");
  } else if (line == "RACK_ENGINE:GRANULAR:ON") {
    granularTargetGain = 1.0f;
    reply.println("GMAX:ENGINE:GRANULAR:ON");
  } else if (line == "RACK_ENGINE:GRANULAR:OFF") {
    granularTargetGain = 0.0f;
    reply.println("GMAX:ENGINE:GRANULAR:OFF");
  } else if (line == "RACK_ENGINE:SPECTRAL:ON") {
    spectralTargetGain = 1.0f;
    spectralControl.println(line);
    reply.println("GMAX:ENGINE:SPECTRAL:ON");
  } else if (line == "RACK_ENGINE:SPECTRAL:OFF") {
    spectralTargetGain = 0.0f;
    spectralControl.println(line);
    reply.println("GMAX:ENGINE:SPECTRAL:OFF");
  } else if (line.startsWith("RACK_NOTE_ON:SPECTRAL:") ||
             line.startsWith("RACK_NOTE_OFF:SPECTRAL:") ||
             line.startsWith("RACK_PARAM:SPECTRAL:") ||
             line.startsWith("RACK_PATCH:SPECTRAL:") ||
             line.startsWith("RACK_PATCH_SAVE:SPECTRAL:")) {
    spectralControl.println(line);
    if (line.startsWith("RACK_NOTE_ON:SPECTRAL:")) {
      spectralTargetGain = 1.0f;
      rackTargetGain = 1.0f;
    }
    reply.print("GMAX:SPECTRAL:FORWARDED:");
    reply.println(line);
  } else if (line.startsWith("RACK_NOTE_ON:GRANULAR:")) {
    const int split = line.lastIndexOf(':');
    const int previous = line.lastIndexOf(':', split - 1);
    granularNote = constrain(line.substring(previous + 1, split).toInt(), 0, 127);
    granularVelocity = constrain(line.substring(split + 1).toInt(), 1, 127);
    granularGate = true;
    granularTargetGain = 1.0f;
    rackTargetGain = 1.0f;
    reply.printf("GMAX:NOTE_ON:GRANULAR:%u:%u\n", granularNote, granularVelocity);
  } else if (line.startsWith("RACK_NOTE_OFF:GRANULAR:")) {
    granularGate = false;
    reply.printf("GMAX:NOTE_OFF:GRANULAR:%u\n", granularNote);
  } else if (line.startsWith("RACK_PARAM:GRANULAR:")) {
    const int split = line.lastIndexOf(':');
    const int previous = line.lastIndexOf(':', split - 1);
    const int parameter = line.substring(previous + 1, split).toInt();
    const int value = line.substring(split + 1).toInt();
    if (parameter >= 0 && parameter < 18 && value >= 0 && value <= 127) {
      granularParams[parameter] = static_cast<uint8_t>(value);
      reply.printf("GMAX:PARAM:GRANULAR:%d:%d\n", parameter, value);
    } else {
      reply.println("GMAX:PARAM:ERROR");
    }
  } else if (line.startsWith("RACK_SAMPLE_BEGIN:")) {
    const int p3 = line.lastIndexOf(':');
    const int p2 = line.lastIndexOf(':', p3 - 1);
    const int p1 = line.lastIndexOf(':', p2 - 1);
    const size_t samples = static_cast<size_t>(strtoul(line.substring(p1 + 1, p2).c_str(), nullptr, 10));
    const uint32_t rate = strtoul(line.substring(p2 + 1, p3).c_str(), nullptr, 10);
    const uint32_t crc = strtoul(line.substring(p3 + 1).c_str(), nullptr, 16);
    if (samples >= 2 && samples <= kSourceCapacitySamples && rate >= 8000 && rate <= 48000) {
      rackTargetGain = 0.0f;
      sampleRxSamples = samples;
      sampleRxRate = rate;
      sampleRxBytesExpected = samples * sizeof(int16_t);
      sampleRxBytesRemaining = sampleRxBytesExpected;
      sampleRxOffset = 0;
      sampleRxExpectedCrc = crc;
      sampleRxCrc = 0xFFFFFFFFU;
      reply.printf("GMAX:SAMPLE:READY:bytes=%u\n", static_cast<unsigned>(sampleRxBytesExpected));
    } else {
      reply.println("GMAX:SAMPLE:REJECTED");
    }
  } else if (line == "RACK:STATUS") {
    spectralControl.println(line);
    reply.printf("GMAX:RACK:STATUS:gain=%.3f:target=%.1f:granular=%.3f/%.1f:spectral=%.3f/%.1f\n",
                 rackGain, rackTargetGain, granularGain, granularTargetGain,
                 spectralGain, spectralTargetGain);
  }
}

void readRackCommands(Stream &input, String &buffer) {
  while (input.available()) {
    if (sampleRxBytesRemaining > 0) {
      const uint8_t value = static_cast<uint8_t>(input.read());
      reinterpret_cast<uint8_t *>(stagingSource)[sampleRxOffset++] = value;
      sampleRxCrc = crc32Byte(sampleRxCrc, value);
      --sampleRxBytesRemaining;
      if (sampleRxBytesRemaining == 0) {
        sampleRxCrc ^= 0xFFFFFFFFU;
        if (sampleRxCrc == sampleRxExpectedCrc) {
          int32_t peak = 1;
          for (size_t i = 0; i < sampleRxSamples; ++i) {
            const int32_t value = stagingSource[i];
            const int32_t magnitude = value < 0 ? -value : value;
            if (magnitude > peak) peak = magnitude;
          }
          sourceNormalization = min(8.0f, 30000.0f / peak);
          int16_t *oldSource = source;
          source = stagingSource;
          stagingSource = oldSource;
          sourceSampleCount = sampleRxSamples;
          initialiseGrains(static_cast<uint16_t>(8U + granularParams[2] * 56U / 127U));
          input.printf("GMAX:SAMPLE:ACTIVE:samples=%u:rate=%lu:crc=%08lX:norm=%.2f\n",
                       static_cast<unsigned>(sourceSampleCount),
                       static_cast<unsigned long>(sampleRxRate),
                       static_cast<unsigned long>(sampleRxCrc), sourceNormalization);
        } else {
          input.printf("GMAX:SAMPLE:CRC_ERROR:got=%08lX:expected=%08lX\n",
                       static_cast<unsigned long>(sampleRxCrc),
                       static_cast<unsigned long>(sampleRxExpectedCrc));
        }
      }
      continue;
    }
    const char c = static_cast<char>(input.read());
    if (c == '\r') continue;
    if (c == '\n') {
      buffer.trim();
      if (buffer.length()) handleRackCommand(buffer, input);
      buffer = "";
    } else if (buffer.length() < 95) {
      buffer += c;
    } else {
      buffer = "";
    }
  }
}
#endif

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
  const size_t safeCount = max(sourceSampleCount, static_cast<size_t>(4));
  const float usableSpan = static_cast<float>(safeCount - 3U);
  const float centre = 1.0f + granularParams[0] / 127.0f * usableSpan;
  const float positionSpray = granularParams[5] / 127.0f * (usableSpan * 0.45f);
  const float positionNoise = (static_cast<float>((random >> 3) & 2047U) / 2047.0f - 0.5f) * 2.0f;
  grain.position = constrain(centre + positionNoise * positionSpray, 2.0f,
                             static_cast<float>(safeCount - 3U));
  const float noteRatio = powf(2.0f, (static_cast<int>(granularNote) - 60) / 12.0f);
  const float coarseRatio = powf(2.0f, (static_cast<int>(granularParams[3]) - 64) / 24.0f);
  const float pitchSpray = granularParams[4] / 127.0f * 0.75f;
  const float pitchNoise = (static_cast<float>((random >> 14) & 1023U) / 1023.0f - 0.5f) * 2.0f;
  const float sourceRateRatio = sampleRxRate / static_cast<float>(kSampleRate);
  grain.step = sourceRateRatio * noteRatio * coarseRatio * (1.0f + pitchNoise * pitchSpray);
  if ((random & 127U) < granularParams[7]) grain.step = -grain.step;
  grain.age = 0;
  const uint32_t baseDuration = 128U + static_cast<uint32_t>(granularParams[1]) * 80U;
  const uint32_t proposedDuration = baseDuration + ((random >> 20) % (baseDuration / 2U + 1U));
  const uint32_t maximumDuration = static_cast<uint32_t>(safeCount > 67U ? safeCount - 3U : 64U);
  grain.duration = proposedDuration < maximumDuration ? proposedDuration : maximumDuration;
  // Compensation de sommation adaptee aux vrais WAV courts. La valeur de
  // benchmark (0,82) rendait Bass_26 pratiquement inaudible après fenêtrage,
  // panoramique et marge du mixeur Teensy. Le rendu est ensuite borné en
  // int16 et passe par le soft-clip, donc ce gain ne peut pas dépasser le bus.
  grain.gain = 5.0f / sqrtf(static_cast<float>(activeGrains));
  const float spread = granularParams[6] / 127.0f;
  grain.pan = 0.5f + (static_cast<float>((random >> 4) & 1023U) / 1023.0f - 0.5f) * spread;
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
      if (grain.age >= grain.duration || grain.position >= sourceSampleCount - 2U || grain.position < 1.0f)
        resetGrain(index, ++generations[lane], count);

      const uint32_t whole = static_cast<uint32_t>(grain.position);
      const float fraction = grain.position - whole;
      const float sample = (source[whole] + (source[whole + 1] - source[whole]) * fraction) *
                           sourceNormalization;
      const size_t windowIndex = static_cast<size_t>(grain.age) * (kWindowSize - 1U) / grain.duration;
      const float baseWindow = windowTable[windowIndex];
      const float squareMix = granularParams[8] / 127.0f;
      const float window = baseWindow + (baseWindow * baseWindow - baseWindow) * squareMix;
      const float value = sample * window * grain.gain;
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

void runRealtimeI2S(uint32_t seconds) {
  constexpr uint32_t blockBudgetUs = (1000000UL * kBlockSamples) / kSampleRate;
  i2s.setPins(az2::rack::kI2sBclkPin, az2::rack::kI2sWsPin, az2::rack::kI2sDataOutPin);
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.printf("GMAX:I2S:FAIL:error=%d\n", i2s.lastError());
    return;
  }

  initialiseGrains(kRealtimeGrains);
  const uint32_t targetBlocks = (kSampleRate * seconds) / kBlockSamples;
  uint32_t lateBlocks = 0;
  uint32_t shortWrites = 0;
  uint32_t maxRenderUs = 0;
  const uint32_t started = millis();
  for (uint32_t block = 0; block < targetBlocks; ++block) {
    const uint32_t renderStart = micros();
    renderBlockDual(kRealtimeGrains);
    const uint32_t renderUs = micros() - renderStart;
    if (renderUs > maxRenderUs) maxRenderUs = renderUs;
    if (renderUs > blockBudgetUs) ++lateBlocks;
    const size_t bytes = sizeof(outputInterleaved);
    if (i2s.write(outputInterleaved, bytes) != bytes) ++shortWrites;
  }
  const uint32_t elapsed = millis() - started;
  Serial.printf("GMAX:I2S:RESULT:grains=%u:seconds=%lu:elapsed_ms=%lu:blocks=%lu:late=%lu:short=%lu:max_render_us=%lu:budget_us=%lu\n",
                kRealtimeGrains, static_cast<unsigned long>(seconds),
                static_cast<unsigned long>(elapsed), static_cast<unsigned long>(targetBlocks),
                static_cast<unsigned long>(lateBlocks), static_cast<unsigned long>(shortWrites),
                static_cast<unsigned long>(maxRenderUs), static_cast<unsigned long>(blockBudgetUs));
  i2s.end();
}

#ifdef AZ2_RACK_AGGREGATOR
void runRackAggregator() {
  rackControl.begin(az2::rack::kControlBaud, SERIAL_8N1,
                    az2::rack::kControlRxPin, az2::rack::kControlTxPin);
  spectralControl.begin(az2::rack::kControlBaud, SERIAL_8N1,
                        az2::rack::kSpectralControlRxPin,
                        az2::rack::kSpectralControlTxPin);
  i2s.setPins(az2::rack::kI2sBclkPin, az2::rack::kI2sWsPin,
              az2::rack::kI2sDataOutPin, az2::rack::kSpectralDataInPin);
#ifdef AZ2_TEENSY_CLOCK_SLAVE
  constexpr i2s_role_t rackRole = I2S_ROLE_SLAVE;
  constexpr const char *rackRoleName = "slave-teensy-clock";
  constexpr i2s_data_bit_width_t rackBitWidth = I2S_DATA_BIT_WIDTH_32BIT;
#else
  constexpr i2s_role_t rackRole = I2S_ROLE_MASTER;
  constexpr const char *rackRoleName = "master-bench";
  constexpr i2s_data_bit_width_t rackBitWidth = I2S_DATA_BIT_WIDTH_16BIT;
#endif
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, rackBitWidth,
                 I2S_SLOT_MODE_STEREO, -1, rackRole)) {
    Serial.printf("GMAX:RACK:FAIL:I2S:error=%d\n", i2s.lastError());
    return;
  }
  initialiseGrains(kRealtimeGrains);
#ifdef AZ2_TEENSY_CLOCK_SLAVE
  memset(rackOutput32, 0, sizeof(rackOutput32));
  i2s.write(rackOutput32, sizeof(rackOutput32));
#else
  memset(outputInterleaved, 0, sizeof(outputInterleaved));
  i2s.write(outputInterleaved, sizeof(outputInterleaved));
#endif
  Serial.printf("GMAX:RACK:READY:role=%s:bclk=%d:ws=%d:dout=%d:din=%d\n",
                rackRoleName, az2::rack::kI2sBclkPin, az2::rack::kI2sWsPin,
                az2::rack::kI2sDataOutPin, az2::rack::kSpectralDataInPin);

  uint32_t blocks = 0, shortReads = 0, shortWrites = 0, maxRenderUs = 0;
  int64_t spectralEnergy = 0;
  uint32_t lastReport = millis();
  String usbCommand, teensyCommand;
  String spectralReply;
  for (;;) {
    readRackCommands(Serial, usbCommand);
    readRackCommands(rackControl, teensyCommand);
    while (spectralControl.available()) {
      const char c = static_cast<char>(spectralControl.read());
      if (c == '\r') continue;
      if (c == '\n') {
        spectralReply.trim();
        if (spectralReply.length()) {
          Serial.print("GMAX:SPECTRAL:REPLY:");
          Serial.println(spectralReply);
          rackControl.print("SPECTRAL:");
          rackControl.println(spectralReply);
        }
        spectralReply = "";
      } else if (spectralReply.length() < 95) {
        spectralReply += c;
      } else {
        spectralReply = "";
      }
    }
#ifdef AZ2_TEENSY_CLOCK_SLAVE
    const size_t received = i2s.readBytes(reinterpret_cast<char *>(rackInput32), sizeof(rackInput32));
    if (received != sizeof(rackInput32)) ++shortReads;
#else
    const size_t received = i2s.readBytes(reinterpret_cast<char *>(spectralInterleaved), sizeof(spectralInterleaved));
    if (received != sizeof(spectralInterleaved)) ++shortReads;
#endif
    const uint32_t renderStart = micros();
    const uint16_t activeGrains = static_cast<uint16_t>(8U + granularParams[2] * 56U / 127U);
    renderBlockDual(activeGrains);
    const float attackSeconds = 0.002f + (granularParams[10] * granularParams[10]) *
                                (3.0f / (127.0f * 127.0f));
    const float decaySeconds = 0.005f + (granularParams[11] * granularParams[11]) *
                               (4.0f / (127.0f * 127.0f));
    const float releaseSeconds = 0.005f + (granularParams[13] * granularParams[13]) *
                                 (6.0f / (127.0f * 127.0f));
    const float attackStep = 1.0f / (kSampleRate * attackSeconds * 2.0f);
    const float releaseStep = 1.0f / (kSampleRate * releaseSeconds * 2.0f);
    const float sustain = granularParams[12] / 127.0f;
    const float decayDivisor = kSampleRate * decaySeconds * 2.0f;
    const float cutoffNorm = granularParams[14] / 127.0f;
    const float filterCoefficient = 0.002f + cutoffNorm * cutoffNorm * 0.65f;
    const float resonance = granularParams[15] / 127.0f * 0.65f;
    const float drive = 1.0f + granularParams[16] / 127.0f * 7.0f;
    const float level = granularParams[17] / 127.0f;
    const float velocityGain = granularVelocity / 127.0f;
    for (size_t sample = 0; sample < kBlockSamples * 2; ++sample) {
#ifdef AZ2_TEENSY_CLOCK_SLAVE
      const int32_t spectral = received == sizeof(rackInput32) ? rackInput32[sample] >> 16 : 0;
#else
      const int32_t spectral = received == sizeof(spectralInterleaved) ? spectralInterleaved[sample] : 0;
#endif
      spectralEnergy += spectral < 0 ? -spectral : spectral;
      rackGain += (rackTargetGain - rackGain) * 0.0015f;
      granularGain += (granularTargetGain - granularGain) * 0.0015f;
      spectralGain += (spectralTargetGain - spectralGain) * 0.0015f;
      if (granularGate) {
        granularEnvelope = min(1.0f, granularEnvelope + attackStep);
        if (granularEnvelope >= 0.999f) {
          granularEnvelope += (sustain - granularEnvelope) / decayDivisor;
        }
      } else {
        granularEnvelope = max(0.0f, granularEnvelope - releaseStep);
      }
      float granularSample = outputInterleaved[sample] * granularEnvelope *
                             velocityGain * granularGain;
      float &filterState = (sample & 1U) ? granularFilterR : granularFilterL;
      filterState += filterCoefficient * (granularSample - filterState);
      granularSample = filterState + (granularSample - filterState) * resonance;
      const float driven = granularSample / 32768.0f * drive;
      granularSample = (driven / (1.0f + fabsf(driven))) * (32768.0f / drive) * level;
      const int32_t granularPart = static_cast<int32_t>(granularSample);
      const int32_t spectralPart = static_cast<int32_t>((spectral / 2) * spectralGain);
      const int32_t mixed = granularPart + spectralPart;
#ifdef AZ2_TEENSY_CLOCK_SLAVE
      const int16_t sample16 = static_cast<int16_t>(
          constrain(static_cast<int32_t>(mixed * rackGain), -32768, 32767));
      rackOutput32[sample] = static_cast<int32_t>(sample16) << 16;
#else
      outputInterleaved[sample] = static_cast<int16_t>(
          constrain(static_cast<int32_t>(mixed * rackGain), -32768, 32767));
#endif
    }
    const uint32_t renderUs = micros() - renderStart;
    if (renderUs > maxRenderUs) maxRenderUs = renderUs;
#ifdef AZ2_TEENSY_CLOCK_SLAVE
    if (i2s.write(rackOutput32, sizeof(rackOutput32)) != sizeof(rackOutput32))
#else
    if (i2s.write(outputInterleaved, sizeof(outputInterleaved)) != sizeof(outputInterleaved))
#endif
      ++shortWrites;
    ++blocks;
    const uint32_t now = millis();
    if (now - lastReport >= 1000) {
      Serial.printf("GMAX:RACK:STREAM:blocks=%lu:short_rx=%lu:short_tx=%lu:spectral_energy=%lld:max_render_us=%lu:heap=%u\n",
                    static_cast<unsigned long>(blocks), static_cast<unsigned long>(shortReads),
                    static_cast<unsigned long>(shortWrites), spectralEnergy,
                    static_cast<unsigned long>(maxRenderUs), static_cast<unsigned>(ESP.getFreeHeap()));
      spectralEnergy = 0;
      maxRenderUs = 0;
      lastReport = now;
    }
  }
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("GMAX:BOOT:chip=%s:cores=%u:cpu_mhz=%u:flash=%u:psram=%u\n",
                ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz(),
                ESP.getFlashChipSize(), ESP.getPsramSize());
  if (!psramFound() || ESP.getPsramSize() < kSourceCapacityBytes * 2U) {
    Serial.println("GMAX:FAIL:PSRAM");
    return;
  }
  source = static_cast<int16_t *>(heap_caps_malloc(kSourceCapacityBytes, MALLOC_CAP_SPIRAM));
  stagingSource = static_cast<int16_t *>(heap_caps_malloc(kSourceCapacityBytes, MALLOC_CAP_SPIRAM));
  if (source == nullptr || stagingSource == nullptr) {
    Serial.println("GMAX:FAIL:ALLOC");
    return;
  }
  for (size_t index = 0; index < kWindowSize; ++index) {
    const float phase = static_cast<float>(index) / (kWindowSize - 1U);
    windowTable[index] = 0.5f - 0.5f * cosf(2.0f * PI * phase);
  }
  // Banque PCM embarquee : mosaique non periodique des samples Kick/Snare
  // deja livres et licences avec AZ-2. Les coupures entre fragments ne sont
  // pas audibles car chaque grain applique sa propre fenetre.
  uint32_t mosaicState = 0x7f4a7c15U;
  uint32_t sourceOffset = 0;
  uint16_t sourceStep = 1;
  bool useSnare = false;
  bool reverse = false;
  sourceSampleCount = kSourceCapacitySamples;
  for (size_t index = 0; index < sourceSampleCount; ++index) {
    constexpr uint32_t kMosaicBlock = 2048;
    if ((index & (kMosaicBlock - 1U)) == 0U) {
      mosaicState ^= mosaicState << 13;
      mosaicState ^= mosaicState >> 17;
      mosaicState ^= mosaicState << 5;
      useSnare = (mosaicState & 1U) != 0;
      reverse = (mosaicState & 2U) != 0;
      sourceStep = 1U + ((mosaicState >> 2) % 3U);
      const uint32_t length = useSnare ? kSampleSnareLen : kSampleKickLen;
      sourceOffset = (mosaicState >> 8) % length;
    }
    const uint32_t length = useSnare ? kSampleSnareLen : kSampleKickLen;
    const uint32_t local = static_cast<uint32_t>(index) & (kMosaicBlock - 1U);
    const uint32_t advance = (local * sourceStep) % length;
    const uint32_t sampleIndex = reverse ? (sourceOffset + length - advance) % length
                                         : (sourceOffset + advance) % length;
    source[index] = useSnare ? kSampleSnare[sampleIndex] : kSampleKick[sampleIndex];
    if ((index & 4095U) == 0) delay(1);  // nourrit le watchdog pendant les 30 s de source
  }
  Serial.printf("GMAX:SAMPLE:seconds=30:bytes=%u:memory=PSRAM:source=kick-snare-mosaic\n",
                static_cast<unsigned>(sourceSampleCount * sizeof(int16_t)));
  mainTask = xTaskGetCurrentTaskHandle();
  if (xTaskCreatePinnedToCore(workerLoop, "grain_core0", 4096, nullptr, 24, &workerTask, 0) != pdPASS) {
    Serial.println("GMAX:FAIL:WORKER_TASK");
    return;
  }
  Serial.println("GMAX:DUALCORE:READY:main=1:worker=0");
#ifdef AZ2_RACK_AGGREGATOR
  runRackAggregator();
#else
  runMatrix();
  runRealtimeI2S(30);
  Serial.println("GMAX:ALL_TESTS:READY");
#endif
}

void loop() {
#ifndef AZ2_RACK_AGGREGATOR
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runMatrix();
    runRealtimeI2S(30);
    Serial.println("GMAX:ALL_TESTS:READY");
  }
#endif
  delay(20);
}
