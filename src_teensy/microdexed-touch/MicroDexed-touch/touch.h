#ifndef _TOUCH_H
#define _TOUCH_H

#include "config.h"

#if defined GENERIC_DISPLAY
#include "XPT2046_Touchscreen.h"
extern XPT2046_Touchscreen touch;
#endif

#ifdef CAPACITIVE_TOUCH_DISPLAY
#include "Adafruit_FT6206.h"
extern Adafruit_FT6206 touch;
#endif

enum Dexed_ADSR {
  DXD_ATTACK = 0,
  DXD_DECAY,
  DXD_SUSTAIN,
  DXD_RELEASE
};

typedef struct dexed_live_mod_s
{
  int8_t active_button = 0;
  int8_t orig_adsr_values[NUM_DEXED][4][6];
  int8_t live_adsr_values[NUM_DEXED][4]; // -50 to +50 ADSR live mod
} dexed_live_mod_t;

typedef struct ts_s
{
  uint8_t virtual_keyboard_octave = 3;
  uint8_t virtual_keyboard_instrument = VK_DEXED0;
  uint8_t virtual_keyboard_midi_channel = 1;
  uint8_t virtual_keyboard_velocity = 120;
  uint8_t msp_peak[NUM_MULTISAMPLES];
  uint8_t current_virtual_keyboard_display_mode = 0;   // 0=keys 1=pads 
  uint8_t previous_virtual_keyboard_display_mode = 99; // 0=keys 1=pads 

  TS_Point p;

  uint32_t virtual_keyboard_state_white = 0;
  uint32_t virtual_keyboard_state_black = 0;

  uint8_t displayed_peak[20]; // volmeter peak levels, currently displayed level
  uint8_t old_helptext_length[3];
  bool touch_ui_drawn_in_menu = false;
  bool keyb_in_menu_activated = false;
  uint8_t fav_buttton_state = 0;
} ts_t;

// (Touch)File Manager

typedef struct fm_s
{
  uint8_t wav_recorder_mode = 0;

  uint8_t sample_source = 0; // 0 = SD, 1 = UNUSED, 2 = MSP1, 3 = MSP2, 4 = PSRAM
  uint8_t active_window = 0; // 0 = left window (SDCARD) , 1 = PSRAM
  uint16_t sd_sum_files = 0;
  uint8_t sd_cap_rows;
  uint8_t sd_folder_depth = 0;
  uint16_t sd_selected_file = 0;
  uint16_t sd_skip_files = 0;
  uint8_t sd_mode = 0;
  bool sd_is_folder;
  bool sd_parent_folder = false;
  String sd_new_name;
  String sd_full_name;
  String sd_prev_dir;
  String sd_temp_name;

  uint16_t psram_sum_files = 0;
  uint16_t psram_cap_rows;
  uint16_t psram_selected_file = 0;
  uint16_t psram_skip_files = 0;
  bool SD_CARD_READER_EXT;
} fm_t;

static constexpr int TOUCH_MAX_REFRESH_RATE_MS = 10; // 100Hz
typedef void (*TouchFn)();

void registerTouchHandler(TouchFn touchFn);
void unregisterTouchHandler(void);
TouchFn getCurrentTouchHandler(void);
void updateTouchScreen();
uint16_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val);

// void helptext_l(const char* str);
// void helptext_r(const char* str);

// RAM string versions
FLASHMEM void helptext_l(const char* str);
FLASHMEM void helptext_r(const char* str);
FLASHMEM void helptext_c(const char* str);

// Flash string versions  
FLASHMEM void helptext_l(const __FlashStringHelper* str);
FLASHMEM void helptext_r(const __FlashStringHelper* str);
FLASHMEM void helptext_c(const __FlashStringHelper* str);

void update_latch_button();
void draw_back_touchbutton();
void draw_loop_buttons();
void print_file_manager_buttons();
void print_current_chord();
void handle_page_with_touch_back_button();
void print_file_manager_active_border();
uint16_t RGB24toRGB565(uint8_t r, uint8_t g, uint8_t b);
void update_step_rec_buttons();
void handle_touchscreen_chain_editor();
void handle_touchscreen_braids(void);
void handle_touchscreen_cv_screen(void);
void handle_touchscreen_chord_arranger(void);
void handle_touchscreen_noisemaker(void);
void handle_touchscreen_arpeggio();
void handle_touchscreen_midi_channel_page();
void handle_touchscreen_mixer();
void handle_touchscreen_microsynth();
void handle_touchscreen_pattern_editor();
void handle_touchscreen_voice_select();
void handle_touchscreen_multiband();
void handle_touchscreen_menu();
void handle_touchscreen_test_touchscreen();
void handle_touchscreen_slice_editor();
void handle_touchscreen_settings_button_test();
void handle_touchscreen_file_manager();
void handle_touchscreen_mute_matrix();
void handle_touchscreen_custom_mappings();
void handle_touchscreen_drums();
void handle_load_performance();
void handle_touchscreen_granular();

// Define a flag to track if a redraw is needed to prevent continuous flashing.
//extern bool redrawNeededChainEditor ;
#endif
