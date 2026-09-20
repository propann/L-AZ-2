#include "AZ2_RGB_Direct.h"
#include <esp_cache.h>

AZ2RgbDirect::AZ2RgbDirect(Arduino_DataBus *bus, const uint8_t *init_ops,
                           size_t init_len, int de, int vsync, int hsync,
                           int pclk, const int *data_pins, int width,
                           int height, uint32_t pclk_hz)
    : _bus(bus), _initOps(init_ops), _initLen(init_len), _de(de),
      _vsync(vsync), _hsync(hsync), _pclk(pclk), _dataPins(data_pins),
      _width(width), _height(height), _pclkHz(pclk_hz) {}

bool AZ2RgbDirect::begin() {
  if (!_bus || !_bus->begin()) return false;
  if (_initOps && _initLen) _bus->batchOperation((uint8_t *)_initOps, _initLen);
  return initPanel();
}

bool AZ2RgbDirect::initPanel() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = _pclkHz;
  cfg.timings.h_res = _width;
  cfg.timings.v_res = _height;
  cfg.timings.hsync_pulse_width = 8;
  cfg.timings.hsync_back_porch = 50;
  cfg.timings.hsync_front_porch = 10;
  cfg.timings.vsync_pulse_width = 8;
  cfg.timings.vsync_back_porch = 20;
  cfg.timings.vsync_front_porch = 10;
  cfg.timings.flags.hsync_idle_low = 0;
  cfg.timings.flags.vsync_idle_low = 0;
  cfg.timings.flags.de_idle_high = 0;
  cfg.timings.flags.pclk_active_neg = 0;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 2;
  cfg.sram_trans_align = 8;
  cfg.psram_trans_align = 64;
  cfg.hsync_gpio_num = _hsync;
  cfg.vsync_gpio_num = _vsync;
  cfg.de_gpio_num = _de;
  cfg.pclk_gpio_num = _pclk;
  cfg.disp_gpio_num = GPIO_NUM_NC;
  for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = _dataPins[i];
  cfg.flags.fb_in_psram = 1;
  cfg.flags.double_fb = 1;
  cfg.flags.refresh_on_demand = 0;

  if (esp_lcd_new_rgb_panel(&cfg, &_panel) != ESP_OK) return false;
  if (esp_lcd_panel_reset(_panel) != ESP_OK) return false;
  if (esp_lcd_panel_init(_panel) != ESP_OK) return false;
  if (esp_lcd_rgb_panel_get_frame_buffer(_panel, 2, &_frames[0], &_frames[1]) != ESP_OK) return false;

  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_frame_buf_complete = onFrameComplete;
  return esp_lcd_rgb_panel_register_event_callbacks(_panel, &callbacks, this) == ESP_OK;
}

bool IRAM_ATTR AZ2RgbDirect::onFrameComplete(esp_lcd_panel_handle_t,
                                             const esp_lcd_rgb_panel_event_data_t *,
                                             void *ctx) {
  auto *self = static_cast<AZ2RgbDirect *>(ctx);
  self->_writeIndex ^= 1;
  ++self->_completedFrames;
  return false;
}

void *AZ2RgbDirect::writableFrameBuffer() const {
  return _frames[_writeIndex & 1];
}

void *AZ2RgbDirect::frameBuffer(uint8_t index) const {
  return _frames[index & 1];
}

AZ2RgbDirectOutput::AZ2RgbDirectOutput(AZ2RgbDirect *driver)
    : Arduino_G(480, 480), _driver(driver) {}

bool AZ2RgbDirectOutput::begin(int32_t) {
  return _driver && _driver->begin();
}

void AZ2RgbDirectOutput::drawBitmap(int16_t, int16_t, uint8_t *, int16_t,
                                    int16_t, uint16_t, uint16_t) {}
void AZ2RgbDirectOutput::drawIndexedBitmap(int16_t, int16_t, uint8_t *,
                                           uint16_t *, int16_t, int16_t,
                                           int16_t) {}
void AZ2RgbDirectOutput::draw3bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t,
                                           int16_t) {}
void AZ2RgbDirectOutput::draw24bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t,
                                            int16_t) {}

void AZ2RgbDirectOutput::draw16bitRGBBitmap(int16_t x, int16_t y,
                                            uint16_t *bitmap, int16_t w,
                                            int16_t h) {
  if (!_driver || !bitmap || x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT) return;
  auto *dst = static_cast<uint16_t *>(_driver->writableFrameBuffer());
  for (int16_t row = 0; row < h; ++row) {
    memcpy(dst + (y + row) * WIDTH + x, bitmap + row * w,
           static_cast<size_t>(w) * sizeof(uint16_t));
  }
  esp_cache_msync(dst + y * WIDTH + x,
                  static_cast<size_t>(h) * WIDTH * sizeof(uint16_t),
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}
