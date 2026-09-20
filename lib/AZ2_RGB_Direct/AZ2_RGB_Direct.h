#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

class AZ2RgbDirect {
 public:
  AZ2RgbDirect(Arduino_DataBus *bus, const uint8_t *init_ops, size_t init_len,
               int de, int vsync, int hsync, int pclk,
               const int *data_pins, int width, int height,
               uint32_t pclk_hz = 12000000);

  bool begin();
  void *writableFrameBuffer() const;
  void *frameBuffer(uint8_t index) const;
  uint32_t completedFrames() const { return _completedFrames; }
  uint8_t writableIndex() const { return _writeIndex; }

 private:
  static bool IRAM_ATTR onFrameComplete(esp_lcd_panel_handle_t panel,
                                        const esp_lcd_rgb_panel_event_data_t *edata,
                                        void *ctx);
  bool initPanel();

  Arduino_DataBus *_bus;
  const uint8_t *_initOps;
  size_t _initLen;
  int _de, _vsync, _hsync, _pclk;
  const int *_dataPins;
  int _width, _height;
  uint32_t _pclkHz;
  esp_lcd_panel_handle_t _panel = nullptr;
  void *_frames[2] = {nullptr, nullptr};
  volatile uint8_t _writeIndex = 1;
  volatile uint32_t _completedFrames = 0;
};
