#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <databus/Arduino_ESP32RGBPanel.h>
#include <esp_cache.h>
#include "AZ2_RGB_Direct.h"

namespace {
constexpr int W = 480, H = 480;
Arduino_DataBus *bus = new Arduino_SWSPI(GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);
const int kDataPins[16] = {15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};
AZ2RgbDirect *rgb = nullptr;

void fillFrame(void *ptr, uint16_t color) {
  auto *p = static_cast<uint16_t *>(ptr);
  for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) p[i] = color;
  esp_cache_msync(ptr, static_cast<size_t>(W) * H * sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}
}

void setup() {
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:RGB_DIRECT_PROBE:START");
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  rgb = new AZ2RgbDirect(bus, gc9503v_type1_init_operations,
                          sizeof(gc9503v_type1_init_operations),
                          18, 17, 16, 21, kDataPins, W, H, 12000000);
  if (!rgb->begin()) {
    Serial.println("AZ2:RGB_DIRECT_PROBE:INIT_FAIL");
    return;
  }
  fillFrame(rgb->frameBuffer(0), 0x001F);
  fillFrame(rgb->frameBuffer(1), 0xF800);
  digitalWrite(38, HIGH);
  Serial.printf("AZ2:RGB_DIRECT_PROBE:READY:fb0=%p:fb1=%p\n", rgb->frameBuffer(0), rgb->frameBuffer(1));
}

void loop() {
  static uint32_t last = 0;
  static bool phase = false;
  if (!rgb) { delay(1000); return; }
  fillFrame(rgb->writableFrameBuffer(), phase ? 0x07E0 : 0xFFE0);
  phase = !phase;
  const uint32_t now = millis();
  if (now - last >= 1000) {
    last = now;
    Serial.printf("AZ2:RGB_DIRECT_PROBE:STAT:frames=%lu:write=%u\n",
                  (unsigned long)rgb->completedFrames(), (unsigned)rgb->writableIndex());
  }
  delay(250);
}
