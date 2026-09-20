#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <databus/Arduino_ESP32RGBPanel.h>
#include "AZ2_RGB_Direct.h"

namespace {
constexpr int W = 480, H = 480;
Arduino_DataBus *bus = new Arduino_SWSPI(GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);
const int kDataPins[16] = {15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};
AZ2RgbDirect driver(bus, gc9503v_type1_init_operations,
                    sizeof(gc9503v_type1_init_operations),
                    18, 17, 16, 21, kDataPins, W, H, 12000000);
AZ2RgbDirectOutput output(&driver);
Arduino_Canvas canvas(W, H, &output);
}

void setup() {
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:RGB_DIRECT_PROBE:START");
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  if (!canvas.begin()) {
    Serial.println("AZ2:RGB_DIRECT_PROBE:INIT_FAIL");
    return;
  }
  canvas.fillScreen(RGB565(0, 0, 40));
  canvas.setTextSize(4);
  canvas.setTextColor(RGB565(255, 255, 255));
  canvas.setCursor(70, 200);
  canvas.print("RGB x2");
  canvas.flush();
  digitalWrite(38, HIGH);
  Serial.printf("AZ2:RGB_DIRECT_PROBE:READY:fb0=%p:fb1=%p\n",
                driver.frameBuffer(0), driver.frameBuffer(1));
}

void loop() {
  static uint32_t last = 0;
  static bool phase = false;
  if (!driver.frameBuffer(0)) { delay(1000); return; }
  canvas.fillScreen(phase ? RGB565(0, 100, 0) : RGB565(100, 70, 0));
  canvas.setTextSize(4);
  canvas.setTextColor(RGB565(255, 255, 255));
  canvas.setCursor(70, 200);
  canvas.print("RGB x2");
  const uint32_t copyStartUs = micros();
  canvas.flush();
  const uint32_t copyUs = micros() - copyStartUs;
  phase = !phase;
  const uint32_t now = millis();
  if (now - last >= 1000) {
    last = now;
    Serial.printf("AZ2:RGB_DIRECT_PROBE:STAT:frames=%lu:write=%u:flush_us=%lu\n",
                  (unsigned long)driver.completedFrames(),
                  (unsigned)driver.writableIndex(),
                  (unsigned long)copyUs);
  }
  delay(250);
}
