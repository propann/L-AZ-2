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

class AZ2RgbDirectOutput final : public Arduino_G {
 public:
  explicit AZ2RgbDirectOutput(AZ2RgbDirect *driver);
  bool begin(int32_t speed = GFX_NOT_DEFINED) override;
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h,
                  uint16_t color, uint16_t bg) override;
  void drawIndexedBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                         uint16_t *color_index, int16_t w, int16_t h,
                         int16_t x_skip = 0) override;
  void draw3bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                         int16_t w, int16_t h) override;
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap,
                          int16_t w, int16_t h) override;
  void draw24bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                          int16_t w, int16_t h) override;

 private:
  AZ2RgbDirect *_driver;
};
