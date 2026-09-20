#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <databus/Arduino_ESP32RGBPanel.h>
#include <esp_cache.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

namespace {
constexpr int16_t W = 480;
constexpr int16_t H = 480;
constexpr int kDe = 18, kVsync = 17, kHsync = 16, kPclk = 21;
constexpr int kBacklight = 38;

Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);

esp_lcd_panel_handle_t panel = nullptr;
void *frame0 = nullptr;
void *frame1 = nullptr;
volatile uint8_t writeIndex = 1;
volatile uint32_t frameDone = 0;

bool IRAM_ATTR onFrameDone(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *) {
  writeIndex ^= 1;
  ++frameDone;
  return false;
}

void fillFrame(void *ptr, uint16_t color) {
  auto *pixels = static_cast<uint16_t *>(ptr);
  for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) pixels[i] = color;
  esp_cache_msync(ptr, static_cast<size_t>(W) * H * sizeof(uint16_t),
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

}  // namespace

void setup() {
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:RGB_DIRECT_PROBE:START");
  pinMode(kBacklight, OUTPUT);
  digitalWrite(kBacklight, LOW);

  if (!bus->begin()) {
    Serial.println("AZ2:RGB_DIRECT_PROBE:BUS_FAIL");
    return;
  }
  bus->batchOperation((uint8_t *)gc9503v_type1_init_operations,
                      sizeof(gc9503v_type1_init_operations));

  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = 12000000;
  cfg.timings.h_res = W;
  cfg.timings.v_res = H;
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
  cfg.hsync_gpio_num = kHsync;
  cfg.vsync_gpio_num = kVsync;
  cfg.de_gpio_num = kDe;
  cfg.pclk_gpio_num = kPclk;
  cfg.disp_gpio_num = GPIO_NUM_NC;
  cfg.data_gpio_nums[0] = 15; cfg.data_gpio_nums[1] = 14;
  cfg.data_gpio_nums[2] = 13; cfg.data_gpio_nums[3] = 12;
  cfg.data_gpio_nums[4] = 11; cfg.data_gpio_nums[5] = 10;
  cfg.data_gpio_nums[6] = 9;  cfg.data_gpio_nums[7] = 8;
  cfg.data_gpio_nums[8] = 7;  cfg.data_gpio_nums[9] = 6;
  cfg.data_gpio_nums[10] = 5; cfg.data_gpio_nums[11] = 4;
  cfg.data_gpio_nums[12] = 3; cfg.data_gpio_nums[13] = 2;
  cfg.data_gpio_nums[14] = 1; cfg.data_gpio_nums[15] = 0;
  cfg.flags.fb_in_psram = 1;
  cfg.flags.double_fb = 1;
  cfg.flags.refresh_on_demand = 0;

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &frame0, &frame1));
  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_frame_buf_complete = onFrameDone;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, nullptr));
  fillFrame(frame0, 0x001F);
  fillFrame(frame1, 0xF800);
  digitalWrite(kBacklight, HIGH);
  Serial.printf("AZ2:RGB_DIRECT_PROBE:READY:fb0=%p:fb1=%p\n", frame0, frame1);
}

void loop() {
  static uint32_t last = 0;
  static bool phase = false;
  if (!panel) { delay(1000); return; }
  void *target = writeIndex ? frame1 : frame0;
  fillFrame(target, phase ? 0x07E0 : 0xFFE0);
  phase = !phase;
  const uint32_t now = millis();
  if (now - last >= 1000) {
    last = now;
    Serial.printf("AZ2:RGB_DIRECT_PROBE:STAT:frames=%lu:write=%u\n",
                  (unsigned long)frameDone, (unsigned)writeIndex);
  }
  delay(250);
}
