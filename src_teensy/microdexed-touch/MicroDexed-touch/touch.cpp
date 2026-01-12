#include "touch.h"
#include "sequencer.h"
#include "livesequencer.h"
#include <LCDMenuLib2.h>
#include "ILI9341_t3n.h"
#include <Audio.h>
#include "template_mixer.hpp"
#include "UI.h"
#include "touchbutton.h"
#include "virtualkeyboard.h"
#include "noisemaker.h"

extern ILI9341_t3n display;
extern config_t configuration;
extern uint8_t selected_instance_id;
extern LCDMenuLib2 LCDML;
extern bool remote_touched;
extern sequencer_t seq;
extern void handleStop(void);
extern void handleStart(void);

extern uint8_t activeSample;
extern bool generic_full_draw_required;
extern bool mb_solo_low;
extern bool mb_solo_mid;
extern bool mb_solo_upper_mid;
extern bool mb_solo_high;
extern bool multiband_active;
extern bool multiband_confirm;
extern uint8_t generic_active_function;
extern uint8_t generic_temp_select_menu;
extern uint8_t last_menu_depth;

dexed_live_mod_t dexed_live_mod; // dexed quick live modifiers for attack and release
extern int temp_int;

ts_t ts;                         // touch screen
fm_t fm;                         // file manager
bool loop_start_button = false;
bool loop_end_button = false;
bool isButtonTouched = false;
int numTouchPoints = 0;

extern bool wakeScreenFlag;

#if defined APC
extern void apc_mute_matrix();
#endif

TouchFn currentTouchHandler;
FLASHMEM void registerTouchHandler(TouchFn touchFn) {
  currentTouchHandler = touchFn;
}

FLASHMEM void unregisterTouchHandler(void) {
  currentTouchHandler = 0;
}

TouchFn getCurrentTouchHandler(void) {
  return currentTouchHandler;
}

FLASHMEM void updateTouchScreen() {
  if (remote_touched) {
    numTouchPoints = 1;
  }
  else {
    // no remote touch, so update to check for real touch
    numTouchPoints = touch.touched();
    if (numTouchPoints > 0) {

#if defined GENERIC_DISPLAY    
      // Scale from ~0->4000 to tft
      ts.p = touch.getPoint();
      ts.p.x = map(ts.p.x, 205, 3860, 0, TFT_HEIGHT);
      ts.p.y = map(ts.p.y, 310, 3720, 0, TFT_WIDTH);
#endif

#ifdef CAPACITIVE_TOUCH_DISPLAY
      // Retrieve a point
      TS_Point p = touch.getPoint();

      switch (configuration.sys.touch_rotation) {
      case 1: //damster capacitive touch rotation (1)
        ts.p.x = p.y;
        ts.p.y = DISPLAY_HEIGHT - p.x;
        break;

      case 0: //positionhigh capacitive touch rotation (0)
      default:// in case configuration.sys.touch_rotation in config-file has stored 2 or 3 from the old screen, better behave like new default for now
        ts.p.x = DISPLAY_WIDTH - p.y;
        ts.p.y = p.x;
        break;
      }
#endif
    }
    else {
      isButtonTouched = false;
    }
  }
  wakeScreenFlag |= (numTouchPoints > 0);
}

// Main function that handles both types
FLASHMEM void helptext_impl(const void* str, uint8_t position, bool is_flash)
{
  display.setTextSize(1);

  // Calculate string length based on type
  uint8_t l;
  if (is_flash) {
    // Flash string length calculation
    PGM_P p = reinterpret_cast<PGM_P>(str);
    l = 0;
    char c;
    while ((c = pgm_read_byte(p + l)) != 0) {
      l++;
    }
  }
  else {
    // RAM string length calculation
    l = strlen((const char*)str);
  }

  uint8_t old_len = ts.old_helptext_length[position];
  uint8_t y_pos = DISPLAY_HEIGHT - CHAR_height_small;

  // Clear previous text if new text is shorter (except center)
  if (l < old_len && position != 2) {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    uint16_t clear_x, clear_width;

    if (position == 0) {
      clear_x = CHAR_width_small * l;
      clear_width = CHAR_width_small * (old_len - l);
    }
    else {
      clear_x = DISPLAY_WIDTH - CHAR_width_small * old_len;
      clear_width = CHAR_width_small * (old_len - l);
    }
    display.fillRect(clear_x, y_pos, clear_width, CHAR_height_small, COLOR_BACKGROUND);
  }

  // Calculate x position
  uint16_t x_pos = 0;
  switch (position) {
  case 0: x_pos = 0; break;
  case 1: x_pos = DISPLAY_WIDTH - CHAR_width_small * l; break;
  case 2:
    if (l != old_len) {
      display.fillRect(0, y_pos, DISPLAY_WIDTH, CHAR_height_small, COLOR_BACKGROUND);
    }
    x_pos = (DISPLAY_WIDTH - CHAR_width_small * l) / 2;
    break;
  }

  // Draw the text
  display.setCursor(x_pos, y_pos);
  display.setTextColor(COLOR_SYSTEXT, position == 2 ? COLOR_BACKGROUND : DX_DARKCYAN);

  if (is_flash) {
    display.print((const __FlashStringHelper*)str);
  }
  else {
    display.print((const char*)str);
  }

  ts.old_helptext_length[position] = l;
}

// RAM string versions
FLASHMEM void helptext_l(const char* str) {
  helptext_impl(str, 0, false);
}

FLASHMEM void helptext_r(const char* str) {
  helptext_impl(str, 1, false);
}

FLASHMEM void helptext_c(const char* str) {
  helptext_impl(str, 2, false);
}

// Flash string versions  
FLASHMEM void helptext_l(const __FlashStringHelper* str) {
  helptext_impl(str, 0, true);
}

FLASHMEM void helptext_r(const __FlashStringHelper* str) {
  helptext_impl(str, 1, true);
}

FLASHMEM void helptext_c(const __FlashStringHelper* str) {
  helptext_impl(str, 2, true);
}


extern void clear_bottom_half_screen_without_backbutton();

#ifdef TOUCH_UI
FLASHMEM void back_touchbutton()
{
  if (ts.keyb_in_menu_activated == false)
  {
    // back text can be drawn as a touch button
    if (LCDML.MENU_getLayer() != 0 && LCDML.FUNC_getID() == 255) //not in root menu but in menu
    {
      draw_button_on_grid(2, 25, "GO", back_text, 1);
    }
    else
      if (LCDML.MENU_getLayer() == 0 && LCDML.FUNC_getID() == 255) // root menu, no where to go back
      {
        draw_button_on_grid(2, 25, "", "", 98);//clear
      }
  }
  if (legacy_touch_button_back_page())
  {
    //remove unusable touch buttons from screen
    display.console = true;
    display.fillRect(270, 87, CHAR_width_small * button_size_x, CHAR_height_small * button_size_y, COLOR_BACKGROUND);
    display.fillRect(12, 144, 300, CHAR_height_small * button_size_y, COLOR_BACKGROUND);
    display.fillRect(78, 200, 234, CHAR_height_small * button_size_y, COLOR_BACKGROUND);
    display.console = false;
    //draw_button_on_grid(2, 25, "GO", back_text, 0);  //the back button should already be there
  }
  else if (touch_button_back_page() && seq.cycle_touch_element != 1)
  {
    draw_button_on_grid(2, 25, "GO", back_text, 1);
  }
}
#endif

FLASHMEM void draw_back_touchbutton()
{
  if (ts.keyb_in_menu_activated == false)
  {
    // back text can be drawn as a touch button
    if (LCDML.MENU_getLayer() != 0 && LCDML.FUNC_getID() == 255) //not in root menu but in menu
    {
      TouchButton::drawButton(GRID.X[0], GRID.Y[5], "GO", back_text, TouchButton::BUTTON_NORMAL);
    }
    else
      if (LCDML.MENU_getLayer() == 0 && LCDML.FUNC_getID() == 255) // root menu, no where to go back
      {
        TouchButton::clearButton(GRID.X[0], GRID.Y[5], COLOR_BACKGROUND);

      }
  }
  if (legacy_touch_button_back_page() && (LCDML.FUNC_getID() > _LCDML_DISP_cnt))
  {
    //remove unusable touch buttons from screen
    clear_bottom_half_screen_without_backbutton();
    //draw_button_on_grid(2, 25, "GO", back_text, 0);  //the back button should already be there

  }
  //else if (legacy_touch_button_back_page()&& ts.keyb_in_menu_activated  )
  else if (legacy_touch_button_back_page())
  { // but when virtual keyboard was active when coming from main menu, it must be redrawn
    TouchButton::drawButton(GRID.X[0], GRID.Y[5], "GO", back_text, TouchButton::BUTTON_NORMAL);
  }
  else if (touch_button_back_page() && seq.cycle_touch_element != 1)
  {
    TouchButton::drawButton(GRID.X[0], GRID.Y[5], "GO", back_text, TouchButton::BUTTON_NORMAL);
  }
}

FLASHMEM uint16_t RGB24toRGB565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
}

FLASHMEM uint16_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val)
{
  // hue: 0-359, sat: 0-255, val (lightness): 0-255
  int r = 0, g = 0, b = 0, base;

  base = ((255 - sat) * val) >> 8;
  switch (hue / 60)
  {
  case 0:
    r = val;
    g = (((val - base) * hue) / 60) + base;
    b = base;
    break;
  case 1:
    r = (((val - base) * (60 - (hue % 60))) / 60) + base;
    g = val;
    b = base;
    break;
  case 2:
    r = base;
    g = val;
    b = (((val - base) * (hue % 60)) / 60) + base;
    break;
  case 3:
    r = base;
    g = (((val - base) * (60 - (hue % 60))) / 60) + base;
    b = val;
    break;
  case 4:
    r = (((val - base) * (hue % 60)) / 60) + base;
    g = base;
    b = val;
    break;
  case 5:
    r = val;
    g = base;
    b = (((val - base) * (60 - (hue % 60))) / 60) + base;
    break;
  }
  return RGB24toRGB565(r, g, b);
}

FLASHMEM bool check_button_on_grid(uint8_t x, uint8_t y)
{
  bool result = false;
  if (ts.p.x > x * CHAR_width_small && ts.p.x < (x + button_size_x) * CHAR_width_small && ts.p.y > y * CHAR_height_small && ts.p.y < (y + button_size_y) * CHAR_height_small) {
    if (isButtonTouched == false) {
      isButtonTouched = true;
      result = true;
    }
  }
  return result;
}

FLASHMEM void print_current_chord()
{
  for (uint8_t x = 0; x < 7; x++)
  {
    display.print(seq.chord_names[seq.arp_chord][x]);
  }
}

extern void sub_step_recording(bool touchinput, uint8_t touchparam);

FLASHMEM bool handleLiveModButtons(void) {
  bool pressedOne = false;
  const int8_t buttonStep = dexed_live_mod.active_button - 1;
  for (uint8_t step = 0; step < 4; step++) {
    if (TouchButton::isPressed(GRID.X[step], GRID.Y[5] - 23)) {
      pressedOne = true;
      if (buttonStep != step) {
        dexed_live_mod.active_button = step + 1;
        printLiveModButton(step); // draw newly active
      }
      else {
        dexed_live_mod.active_button = 0;
      }
      if (buttonStep >= 0) {
        printLiveModButton(buttonStep); // draw previously active
      }
    }
  }
  if (pressedOne) {
    if (dexed_live_mod.active_button > 0) {
      helptext_r(F("< > CHANGE MODIFIER VALUE"));
      display.setCursor(0, DISPLAY_HEIGHT - (CHAR_height_small * 2) - 2);
      print_empty_spaces(38, 1);
      display.setCursor(9 * CHAR_width_small, DISPLAY_HEIGHT - CHAR_height_small * 1);
      print_empty_spaces(9, 1);
      display.setCursor(CHAR_width_small * 38 + 2, DISPLAY_HEIGHT - (CHAR_height_small * 2) - 2);
      display.print(F(" PUSH TO RETURN"));
    }
    else {
      print_voice_select_default_help();
    }
  }
  return pressedOne;
}

#include "virtualtypewriter.h"
extern char edit_string_global[FILENAME_LEN];
extern uint8_t edit_len_global;
extern uint8_t edit_pos_global;
extern bool edit_mode_global;
extern uint8_t vtw_x;
extern char g_voice_name[6][11]; // Assuming 6 instances, 10 chars + null terminator
extern void remove_glow(void);
void virtual_typewriter_init(int8_t pos);
bool virtual_typewriter_touchloop(char* out_key);
void virtual_typewriter_update_display();
// void drawVoiceNameButton();
// void UI_update_instance_icons();
// void draw_favorite_icon(uint8_t pool, uint8_t bank, uint8_t voice, uint8_t instance);
extern void save_voice_name_and_exit();

extern uint8_t edit_pos_global;
extern bool edit_mode_global;
extern uint8_t edit_len_global;
extern  void   save_braids_preset();
extern void update_multisample_name();
extern void update_performance_name();
extern void virtual_typewriter_waitforsysex();
bool virtual_typewriter_active = false;

FLASHMEM void handle_generic_virtual_typewriter_touch() {
  if (virtual_typewriter_active)
  {
    static bool touch_processed = false;
    updateTouchScreen();
    if (numTouchPoints == 0) {
      touch_processed = false;
      return;
    }
    if (touch_processed) {
      return;
    }
    char vt_key = 0;
    if (virtual_typewriter_touchloop(&vt_key)) {
      touch_processed = true;
      if (vt_key == VK_ENTER) {
        if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_braids_name))
        {
          edit_mode_global = false;
          save_braids_preset();
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_multisample_name))
        {
          edit_mode_global = false;
          update_multisample_name();
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_performance_name))
        {
          edit_mode_global = false;
          update_performance_name();
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_voice_name))
        {
          edit_mode_global = false;
          save_voice_name_and_exit();
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_sysex_receive_bank))
        {
          virtual_typewriter_waitforsysex();
          virtual_typewriter_active = false;
        }

        return;
      }
      else if (vt_key == VK_LEFT) {
        // Move cursor left
        if (edit_pos_global > 0) {
          edit_pos_global--;
        }
      }
      else if (vt_key == VK_RIGHT) {
        // Move cursor right
        if (edit_pos_global < edit_len_global - 1) {
          edit_pos_global++;
        }
      }
      else {
        // Handle regular character input
        if (edit_pos_global < edit_len_global) {
          edit_string_global[edit_pos_global] = vt_key;
          // Auto-advance cursor
          if (edit_pos_global < edit_len_global - 1) {
            edit_pos_global++;
          }
        }
      }
      // Update display
      if (virtual_typewriter_active || seq.cycle_touch_element == 2)
        virtual_typewriter_update_display();
    }
  }
}

FLASHMEM void handle_touchscreen_voice_select()
{
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[0])) {
    if (seq.cycle_touch_element == 1) {
      border3_large_clear();
      seq.cycle_touch_element = 0;
      TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[0]);
      print_voice_settings_in_dexed_voice_select(true, true);
      printLiveModButtons();
      drawVoiceNameButton();
      print_voice_select_default_help();
      draw_algo(true); // force redraw
    }
    else {
      border3_large_clear();
      seq.cycle_touch_element = 1;
      TouchButton::drawButton(GRID.X[5], GRID.Y[0], "DEXED", "DETAIL", TouchButton::BUTTON_NORMAL);
      drawVirtualKeyboard();
    }
  }
  else if (TouchButton::isPressed(GRID.X[4], GRID.Y[0])) {
    save_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
  }
  // VOICE NAME BUTTON HANDLER - Enter naming mode
  else if (TouchButton::isPressed(GRID.X[4], GRID.Y[5] - 23) && seq.cycle_touch_element == 0) {
    // Enter voice naming mode
    seq.cycle_touch_element = 2;
    remove_glow();
    unregisterTouchHandler();
    LCDML.OTHER_jumpToFunc(UI_func_set_voice_name);
  }
  else if (TouchButton::isPressed(GRID.X[5], GRID.Y[2]) && seq.cycle_touch_element == 0) {
    if (++configuration.sys.favorites > 3) {
      configuration.sys.favorites = 0;
    }
    print_voice_select_fav_search_button();
  }
  else {
    if (seq.cycle_touch_element == 1) {
      handleTouchVirtualKeyboard();
    }
    else if (seq.cycle_touch_element == 0) {
      if (handleLiveModButtons()) {
        // touch handled
      }
      else if (TouchButton::isPressed(GRID.X[5], GRID.Y[3])) {
        uint8_t value = get_algo() + 1;
        value %= 32; // 0-31

        panic_dexed_current_instance();
        set_algo(value);
        draw_algo();
      }
      else if (TouchButton::isPressed(GRID.X[5], GRID.Y[5] - 23)) {
        LCDML.FUNC_setGBAToLastFunc();
        LCDML.OTHER_jumpToFunc(UI_func_voice_editor);
      }
      else if (TouchButton::isAreaPressed(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
        const int8_t buttonOld = dexed_live_mod.active_button - 1;
        if (buttonOld >= 0) {
          dexed_live_mod.active_button = 0;
          printLiveModButton(buttonOld);
        }
      }
    }
  }
}

FLASHMEM void update_latch_button()
{
  if (seq.cycle_touch_element == 1)
  {
    if (seq.content_type[seq.active_pattern] == 0)
      draw_button_on_grid(36, 6, "LATCH", "NOTE", 98);
    else
      draw_button_on_grid(36, 6, "LATCH", "NOTE", 1);
  }
}

FLASHMEM void update_step_rec_buttons()
{
  if (seq.cycle_touch_element == 1)
  {
    update_latch_button();

    draw_button_on_grid(45, 6, "EMPTY", "STEP", 1);

    if (seq.auto_advance_step == 1)
      draw_button_on_grid(45, 11, "AUTO", "ADV.", 1); // print step recorder icon
    else if (seq.auto_advance_step == 2)
      draw_button_on_grid(45, 11, "A.ADV.", ">STOP", 1); // print step recorder icon
    else
      draw_button_on_grid(45, 11, "KEEP", "STEP", 1); // print step recorder icon
  }
  if (seq.step_recording)
  {
    draw_button_on_grid(36, 1, "RECORD", "ACTIVE", 2); // print step recorder icon
  }
  else
  {
    draw_button_on_grid(36, 1, "STEP", "RECORD", 1); // print step recorder icon
  }
}

FLASHMEM void handle_touchscreen_pattern_editor()
{
  if (numTouchPoints > 0)
  {
    if (seq.cycle_touch_element == 1)
    {
      if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) ||
        (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
      {
        if (ts.p.y > 7 * CHAR_height_small && ts.p.y < 12 * CHAR_height_small + 20 && ts.p.x < 230)
        {
          ts.virtual_keyboard_velocity = ts.p.x - 70;
          if (ts.p.x - 70 < 1)
            ts.virtual_keyboard_velocity = 0;
          else
            if (ts.virtual_keyboard_velocity > 127)
              ts.virtual_keyboard_velocity = 127;
          virtual_keyboard_print_velocity_bar();
        }
      }
      if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) ||
        (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
      {
        if (check_button_on_grid(45, 10) && seq.step_recording)
        {
          seq.auto_advance_step++;
          if (seq.auto_advance_step > 2)
            seq.auto_advance_step = 0;
          update_step_rec_buttons();
        }
      }
    }
    if (check_button_on_grid(36, 1) && seq.running == false)
    {
      seq.note_in = 0;
      seq.step_recording = !seq.step_recording;
      update_step_rec_buttons();
      virtual_keyboard_print_velocity_bar();
    }
    else if (check_button_on_grid(45, 1))
    {
      border3_large();
      border3_large_clear();
      if (seq.cycle_touch_element == 1)
      {
        seq.cycle_touch_element = 0;
        draw_button_on_grid(45, 1, "", "", 99); // print keyboard icon
        display.console = true;
        display.fillRect(0, CHAR_height_small * 12 + 1, DISPLAY_WIDTH, 1, COLOR_BACKGROUND);
        display.console = true;
        display.fillRect(215, 48, 97, 42, COLOR_BACKGROUND);
        display.console = false;
        seq_pattern_editor_update_dynamic_elements();
      }
      else
      {
        seq.cycle_touch_element = 1;
        draw_button_on_grid(45, 1, back_text, "TO SEQ", 0);
        display.console = true;
        display.fillRect(216, CHAR_height_small * 6, 95, CHAR_height_small * 6 + 1, COLOR_BACKGROUND);
        display.console = true;
        display.fillRect(0, CHAR_height_small * 10 + 1, 195, CHAR_height_small * 2 + 1, COLOR_BACKGROUND);
        display.console = true;
        display.fillRect(0, CHAR_height_small * 10 + 1, 2, CHAR_height_small * 12 + 1, COLOR_BACKGROUND);
        display.console = false;
        update_step_rec_buttons();
        virtual_keyboard_print_velocity_bar();
        drawVirtualKeyboard();
      }
    }
    else if (check_button_on_grid(45, 6) && seq.cycle_touch_element == 1)//rest button
    {
      sub_step_recording(true, 1);
    }
    else if (check_button_on_grid(36, 6) && seq.cycle_touch_element == 1)//latch button
    {
      sub_step_recording(true, 2);
    }

    if (seq.cycle_touch_element != 1)
    {
      if (check_button_on_grid(36, 16)) // toggle seq. playmode song/pattern only
      {
        seq.play_mode = !seq.play_mode;

        if (seq.play_mode == false) // is in full song more
        {
          draw_button_on_grid(36, 16, "PLAYNG", "SONG", 0);
          seq.hunt_pattern = false;
          draw_button_on_grid(45, 22, "HUNT", "PATT", 0);
        }
        else // play only current pattern
          draw_button_on_grid(36, 16, "LOOP", "PATT", 2);
      }
      else if (check_button_on_grid(45, 22)) // hunt pattern
      {
        seq.hunt_pattern = !seq.hunt_pattern;

        if (seq.hunt_pattern == false)
          draw_button_on_grid(45, 22, "HUNT", "PATT", 0);
        else // play only current pattern
          draw_button_on_grid(45, 22, "HUNT", "PATT", 2);
      }
      else if (check_button_on_grid(36, 22) && seq.cycle_touch_element != 1) // jump song editor
      {
        LCDML.OTHER_jumpToFunc(UI_func_song);
      }
      else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element != 1)
        if (check_button_on_grid(45, 16)) // jump pattern editor functions
        {
          if (seq.content_type[seq.active_pattern] == 0)
          {
            activeSample = NUM_DRUMSET_CONFIG;
          }
          else
          {
            temp_int = 111;
          }
          draw_button_on_grid(45, 16, "JUMP", "TOOLS", 2);
          seq.menu = 0;
          seq.active_function = 0;
          pattern_editor_menu_0();
        }
    }
  }

  if (seq.cycle_touch_element == 1) {
    handleTouchVirtualKeyboard();
  }
}

FLASHMEM void handle_touchscreen_microsynth()
{
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[0]))
  {
    display.console = true;
    display.fillRect(0, VIRT_KEYB_YPOS - 6 * CHAR_height_small, DISPLAY_WIDTH,
      DISPLAY_HEIGHT - VIRT_KEYB_YPOS + 6 * CHAR_height_small, COLOR_BACKGROUND);
    display.console = false;
    if (seq.cycle_touch_element == 1)
    {
      seq.cycle_touch_element = 0;
      TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[0]);
      generic_full_draw_required = true;
      microsynth_refresh_lower_screen_static_text();
      microsynth_refresh_lower_screen_dynamic_text();
      draw_back_touchbutton();
      generic_full_draw_required = false;
    }
    else
    {
      seq.cycle_touch_element = 1;
      TouchButton::drawButton(GRID.X[5], GRID.Y[0], "MORE", "PARAM.", TouchButton::BUTTON_ACTIVE);
      drawVirtualKeyboard();
    }
  }
  if (seq.cycle_touch_element == 1) {
    handleTouchVirtualKeyboard();
  }
}

#ifdef  SECOND_SD
extern fm_t fm;
extern void _print_file_and_folder_copy_info(uint8_t selected_file);
#endif
extern void  fill_up_with_spaces_right_window();

extern int menuhelper_previous_val;

#ifdef  SECOND_SD
extern SDClass SD_EXTERNAL;
#endif

uint8_t previous_delete_item_type = 99;

FLASHMEM bool isSelectedItemFolder() {

  String fullPath = fm.sd_new_name + "/" + fm.sd_temp_name;

  if (fm.active_window == 0) { // Internal SD
    File f = SD.open(fullPath.c_str());
    if (f) {
      bool isDir = f.isDirectory();
      f.close();
      return isDir;
    }
  }
#ifdef SECOND_SD
  else if (fm.active_window == 1) { // External SD
    File f = SD_EXTERNAL.open(fullPath.c_str());
    if (f) {
      bool isDir = f.isDirectory();
      f.close();
      return isDir;
    }
  }
#endif
  return false;
}

FLASHMEM void print_file_manager_delete_button() {
  bool isFolder = isSelectedItemFolder();

  if (previous_delete_item_type != isFolder || menuhelper_previous_val != fm.sd_mode) {
    TouchButton::drawButton(GRID.X[1], GRID.Y[5],
      "DELETE",
      isFolder ? "FOLDER" : "FILE",
      (fm.sd_mode == FM_DELETE_FILE_OR_FOLDER) ?
      TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);

    previous_delete_item_type = isFolder;
    menuhelper_previous_val = fm.sd_mode;
  }
}

extern void file_manager_clear_text_line();

FLASHMEM void print_file_manager_buttons()
{

  // Update only when mode changes
  if (menuhelper_previous_val != fm.sd_mode) {
    // Browse button
    TouchButton::drawButton(GRID.X[0], GRID.Y[5], "BROWSE", "FILES",
      (fm.sd_mode == FM_BROWSE_FILES) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);

    // Other buttons...
#ifdef COMPILE_FOR_PSRAM
    if (fm.SD_CARD_READER_EXT == false) {
      // Copy to PSRAM button
      TouchButton::drawButton(GRID.X[3], GRID.Y[5], "COPY >", "PSRAM",
        (fm.sd_mode == FM_COPY_TO_PSRAM) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);

      // Delete from PSRAM button
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "DELETE", "PSRAM",
        (fm.sd_mode == FM_DELETE_FROM_PSRAM) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    }
#endif

#ifdef SECOND_SD
    if (fm.SD_CARD_READER_EXT) {
      if (fm.active_window == 0) {
        // Copy to external SD button
        TouchButton::drawButton(GRID.X[3], GRID.Y[5], "COPY >", "EXTERN",
          (fm.sd_mode == FM_COPY_TO_EXTERNAL) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
      }
      else if (fm.active_window == 1) {
        // Copy to internal SD button
        TouchButton::drawButton(GRID.X[3], GRID.Y[5], "COPY <", "INTERN",
          (fm.sd_mode == FM_COPY_TO_INTERNAL) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
      }
      // // Empty button
      // TouchButton::drawButton(GRID.X[4], GRID.Y[5], "", "", TouchButton::BUTTON_NORMAL);
    }
#endif

#ifdef COMPILE_FOR_PSRAM
    if (fm.SD_CARD_READER_EXT == false && fm.active_window == 1) {
      // Show DELETE PSRAM only for PSRAM window
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "DELETE", "PSRAM",
        (fm.sd_mode == FM_DELETE_FROM_PSRAM) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    }
#endif


    // Play button
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_NORMAL);

    // SD/PSRAM toggle button
#ifdef SECOND_SD
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "PSRAM/", "EXT.SD.",
      (fm.SD_CARD_READER_EXT) ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
#endif

    // Update existing state tracker
    menuhelper_previous_val = fm.sd_mode;
  }

  // Always update delete button (it handles its own optimization)
  print_file_manager_delete_button();

}

extern void play_sample(uint8_t note);
extern void print_small_scaled_bar(uint8_t x, uint8_t y, int16_t input_value, int16_t limit_min, int16_t limit_max, int16_t selected_option, boolean show_bar, const char* zero_name = NULL);
#include "drums.h"
extern drum_config_t drum_config[NUM_DRUMSET_CONFIG];
extern uint8_t current_played_pitched_sample;

#ifdef GRANULAR
#include "granular.h"
extern granular_params_t granular_params;

FLASHMEM void handle_touchscreen_granular() {
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[5]))
  {
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_RED);
    play_sample(granular_params.sample_note);
    delay(60);
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_NORMAL);
  }
}
#endif

FLASHMEM void print_file_manager_active_border()
{
  // active_window   0 = left window (SDCARD) , 1 = PSRAM OR SDCARD2
  if (fm.active_window == 0)
  {
    display.console = true;
    display.drawRect(CHAR_width_small * 27, 0, CHAR_width_small * 26 + 2, CHAR_height_small * 23, GREY2);
    display.console = true;
    display.drawRect(0, 0, CHAR_width_small * 27, CHAR_height_small * 23, COLOR_SYSTEXT);
  }
  else
  {
    display.console = true;
    display.drawRect(0, 0, CHAR_width_small * 27, CHAR_height_small * 23, GREY2);
    display.console = true;
    display.drawRect(CHAR_width_small * 27, 0, CHAR_width_small * 26 + 2, CHAR_height_small * 23, COLOR_SYSTEXT);
  }
  fm.sd_mode = 0;
  print_file_manager_buttons();
}

#ifdef COMPILE_FOR_PSRAM
extern void  show_smallfont_noGrid(int pos_y, int pos_x, uint8_t field_size, const char* str);
#endif

#ifdef  SECOND_SD
extern void sd_printDirectory(bool forceReload);
extern void sd_go_parent_folder();
extern void   _print_filemanager_header();
extern void _print_filemanager_sd_extern_header();
extern void print_psram_stats_filemanager();
extern void  psram_printCustomSamplesList();
extern void load_sd_directory();
#endif
extern void  reset_file_manager_folder();

#ifdef  SECOND_SD
FLASHMEM void print_destination_card_info()
{
  if (fm.SD_CARD_READER_EXT)
  {
    uint xoffset = 27;  //Destination Window is reverse side
    if (fm.SD_CARD_READER_EXT && fm.active_window == 1)
    {
      xoffset = 0;
    }

    display.console = true;
    display.fillRect(CHAR_width_small + xoffset * CHAR_width_small, 3 * 11 - 1, CHAR_width_small * 26 - 6, 13 * 11, COLOR_BACKGROUND);
    display.setTextSize(1);

    display.setCursor(CHAR_width_small + xoffset * CHAR_width_small + 7 * CHAR_width_small, 8 * 11);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print("DESTINATION");
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small + xoffset * CHAR_width_small + 2 * CHAR_width_small, 10 * 11);
    display.print("TOUCH HERE TO TOGGLE");
  }
}
FLASHMEM void filemanager_toggle_window_2sdcards()
{
  if (fm.SD_CARD_READER_EXT)
  {
    if (fm.active_window == 0)
      fm.sd_mode = FM_COPY_TO_EXTERNAL;
    else
      if (fm.active_window == 1)
        fm.sd_mode = FM_COPY_TO_INTERNAL;

    menuhelper_previous_val = 99;
    print_file_manager_buttons();
    reset_file_manager_folder();
    load_sd_directory();
    sd_printDirectory(true);

    print_destination_card_info();
  }
}
extern void draw_directory_after_load();

FLASHMEM void filemanager_update_sdcards()
{
  reset_file_manager_folder();
  fm.sd_selected_file = 0;
  fm.sd_mode = FM_BROWSE_FILES;
  sd_go_parent_folder();
  display.console = true;
  display.fillRect(27 * CHAR_width_small, 7, CHAR_width_small * 26, 170, COLOR_BACKGROUND);

  if (fm.SD_CARD_READER_EXT) {
    fm.active_window = 1;
    print_file_manager_active_border();
    _print_filemanager_sd_extern_header();
    load_sd_directory();
    sd_printDirectory(true);
    filemanager_toggle_window_2sdcards();
  }
  else {
    print_psram_stats_filemanager();
    fm.active_window = 1;
    psram_printCustomSamplesList();
    fm.active_window = 0;
    load_sd_directory();
    sd_printDirectory(true);

    print_file_manager_active_border();
  }

  menuhelper_previous_val = 99;
  print_file_manager_buttons();
  print_destination_card_info();
}

#endif

extern void showError(const char* message);
extern void show_delete_info_psram();

FLASHMEM void handle_touchscreen_file_manager()
{
  if (TouchButton::isInArea(0, 0, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT - TouchButton::BUTTON_SIZE_Y)) {
    fm.active_window = 0;
    print_file_manager_active_border();
#ifdef  SECOND_SD
    fm.sd_mode = FM_BROWSE_FILES;
    filemanager_toggle_window_2sdcards();
#endif
  }
#if defined COMPILE_FOR_PSRAM
  else if (TouchButton::isInArea(DISPLAY_WIDTH / 2, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT - TouchButton::BUTTON_SIZE_Y)) {
    fm.active_window = 1;
    print_file_manager_active_border();
#ifdef  SECOND_SD
    fm.sd_mode = FM_BROWSE_FILES;
    filemanager_toggle_window_2sdcards();
#endif
  }
#endif
  else if (TouchButton::isPressed(GRID.X[0], GRID.Y[5])) {
    fm.sd_mode = FM_BROWSE_FILES;
    file_manager_clear_text_line();
    menuhelper_previous_val = 77;
    print_file_manager_delete_button();
    menuhelper_previous_val = 88;
    print_file_manager_buttons();

  }
  else if (TouchButton::isPressed(GRID.X[1], GRID.Y[5])) {
    // Toggle delete mode
    if (fm.sd_mode == FM_DELETE_FILE_OR_FOLDER) {
      fm.sd_mode = FM_BROWSE_FILES;

      file_manager_clear_text_line();
    }
    else {
      fm.sd_mode = FM_DELETE_FILE_OR_FOLDER;

      // Immediately show the current item
      String confirmMsg = "DELETE: ";
      confirmMsg += fm.sd_temp_name;
      confirmMsg += "? PUSH ENC[R]";
      showError(confirmMsg.c_str());
    }

    print_file_manager_delete_button();
    menuhelper_previous_val = 88;
    print_file_manager_buttons();


  }

#ifdef  SECOND_SD  //toggle SD Card2/PSRAM
  else if (TouchButton::isPressed(GRID.X[2], GRID.Y[5])) {
    fm.SD_CARD_READER_EXT = !fm.SD_CARD_READER_EXT;

    filemanager_update_sdcards();
  }
#endif

#if defined COMPILE_FOR_PSRAM
  else  if (TouchButton::isPressed(GRID.X[3], GRID.Y[5]))
  {
    if (fm.SD_CARD_READER_EXT == false) {
      // Toggle copy to PSRAM mode
      if (fm.sd_mode == FM_COPY_TO_PSRAM) {
        fm.sd_mode = FM_BROWSE_FILES;
      }
      else {
        fm.sd_mode = FM_COPY_TO_PSRAM;
        file_manager_clear_text_line();
      }
      print_file_manager_buttons();
    }
#ifdef SECOND_SD
    else {
      // Toggle copy to external SD mode
      if (fm.active_window == 0) {
        if (fm.sd_mode == FM_COPY_TO_EXTERNAL) {
          fm.sd_mode = FM_BROWSE_FILES;
        }
        else {
          fm.sd_mode = FM_COPY_TO_EXTERNAL;
          file_manager_clear_text_line();
        }
      }
      // Toggle copy to internal SD mode
      else if (fm.active_window == 1) {
        if (fm.sd_mode == FM_COPY_TO_INTERNAL) {
          fm.sd_mode = FM_BROWSE_FILES;
        }
        else {
          fm.sd_mode = FM_COPY_TO_INTERNAL;
          file_manager_clear_text_line();
        }
      }
      print_file_manager_buttons();
    }
#endif

  }
#if defined COMPILE_FOR_PSRAM
  else if (TouchButton::isPressed(GRID.X[4], GRID.Y[5]))
  {
    if (fm.SD_CARD_READER_EXT == false && fm.active_window == 1) {
      // Toggle delete PSRAM mode
      if (fm.sd_mode == FM_DELETE_FROM_PSRAM) {
        fm.sd_mode = FM_BROWSE_FILES;
      }
      else {
        fm.sd_mode = FM_DELETE_FROM_PSRAM;
        // The deletion happens on the encoder button press.
        // We just toggle the state here.
        show_delete_info_psram();
      }
      print_file_manager_buttons();

    }
  }
#endif

#endif

  if (TouchButton::isPressed(GRID.X[5], GRID.Y[5])) {
    if ((!fm.sd_is_folder && fm.active_window == 0) || (fm.active_window == 1 && fm.SD_CARD_READER_EXT == false))
    {
      fm.sd_mode = FM_PLAY_SAMPLE;

      print_file_manager_buttons();
      previewWavFilemanager();
      fm.sd_mode = FM_BROWSE_FILES;
      print_file_manager_delete_button();
      menuhelper_previous_val = 77;
      print_file_manager_buttons();
    }
  }
}

FLASHMEM void update_midi_learn_button()
{
  draw_button_on_grid(45, 1, "MIDI", "LEARN", seq.midi_learn_active ? 2 : 0); // RED or grey button
}

FLASHMEM void handle_touchscreen_custom_mappings()
{
  if (numTouchPoints > 0)
  {
    if (check_button_on_grid(45, 1))
    {
      seq.midi_learn_active = !seq.midi_learn_active;
      update_midi_learn_button();
    }
  }
}

#ifdef COMPILE_FOR_PSRAM
extern void PlayOnly();
extern FLASHMEM void trimSample();
extern FLASHMEM void fadeSample();
extern FLASHMEM void saveSample();
#endif

FLASHMEM void handle_touchscreen_drums()
{
  if (TouchButton::isPressed(GRID.X[1], GRID.Y[5]))
  {
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_RED);
    play_sample(activeSample);
    delay(60);
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_ACTIVE);
  }

#ifdef COMPILE_FOR_PSRAM

  if (isCustomSample(activeSample) && drum_config[activeSample].len != 0)
  {
    // Handles the 'TRIM SAMPLE' button
    if (TouchButton::isPressed(GRID.X[2], GRID.Y[5]))
    {
      trimSample();
    }

    // Handles the 'FADE OUT' button
    if (TouchButton::isPressed(GRID.X[3], GRID.Y[5]))
    {
      fadeSample();
    }

    // Handles the 'SAVE SAMPLE' button
    if (TouchButton::isPressed(GRID.X[4], GRID.Y[5]))
    {
      saveSample();
    }
  }
#endif

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_drums) && activeSample == current_played_pitched_sample && seq.running)
    print_small_scaled_bar(0, 8, drum_config[activeSample].pitch, 0, 200, 1, 1, NULL); // update pitched sample info when in drums page and played sample is on screen

}

#ifdef COMPILE_FOR_PSRAM
extern bool noisemaker_update_params_flag;
extern void  print_nm_style();
extern bool remote_active;

FLASHMEM void draw_nm_emulation_mode_button()
{
  if (nm_params.category != NM_SYNTH)
  {
    if (nm_params.randomizer == false)
    {
      if (nm_params.mode == NM_MODE_TR808)
        TouchButton::drawButton(GRID.X[5], GRID.Y[2], "MODE", "808", TouchButton::BUTTON_ACTIVE);
      else  if (nm_params.mode == NM_MODE_TR909)
        TouchButton::drawButton(GRID.X[5], GRID.Y[2], "MODE", "909", TouchButton::BUTTON_ACTIVE);
    }
    else if (nm_params.randomizer == true)
    {
      TouchButton::drawButton(GRID.X[5], GRID.Y[2], "MODE", "RND", TouchButton::BUTTON_ACTIVE);
    }

  }
  else
    TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[2]);

  print_nm_style();
}


FLASHMEM void draw_nm_monostereo()
{
  if (nm_params.stereo_mode == false)
    TouchButton::drawButton(GRID.X[5], GRID.Y[3], "MONO", "SAMPLE", TouchButton::BUTTON_ACTIVE);
  else
    TouchButton::drawButton(GRID.X[5], GRID.Y[3], "STEREO", "SAMPLE", TouchButton::BUTTON_ACTIVE);
}


FLASHMEM void draw_nm_reverb_OffOn()
{
  if (nm_params.reverb_enable == false)
    TouchButton::drawButton(GRID.X[5], GRID.Y[4], "REVERB", "OFF", TouchButton::BUTTON_ACTIVE);
  else
    TouchButton::drawButton(GRID.X[5], GRID.Y[4], "REVERB", "ON", TouchButton::BUTTON_ACTIVE);
}

FLASHMEM void handle_touchscreen_noisemaker() {


  // Generate button
  if (TouchButton::isPressed(GRID.X[1], GRID.Y[5])) {
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "RENDER", "SOUND", TouchButton::BUTTON_RED);
    if (remote_active) {
      display.flushSysEx();
    }
    generateNoisePreview();
    generateAndPlayNoise();
    drawNoiseMakerWaveform(true);
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "RENDER", "SOUND", TouchButton::BUTTON_ACTIVE);
  }

  // Play button
  if (TouchButton::isPressed(GRID.X[2], GRID.Y[5])) {
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "PLAY", "SOUND", TouchButton::BUTTON_RED);
    if (remote_active) {
      display.flushSysEx();
    }
    // generateNoisePreview();
    // generateAndPlayNoise();
    drawNoiseMakerWaveform(true);
    PlayOnly();
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "PLAY", "SOUND", TouchButton::BUTTON_ACTIVE);
  }


  // Randomizer - Generate new sound and play it immediately
  if (TouchButton::isPressed(GRID.X[3], GRID.Y[5])) {
    TouchButton::drawButton(GRID.X[3], GRID.Y[5], "RAND", "", TouchButton::BUTTON_RED);
    if (remote_active) {
      display.flushSysEx();
    }
    // Generate new randomized parameters and preview
    randomizeNoiseMakerParams();
    generateNoisePreview();
    // Play the new sound immediately
    generateAndPlayNoise();
    drawNoiseMakerWaveform(true);
    noisemaker_update_params_flag = true;

  }

  // Play slot button
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[5])) {
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SLOT", TouchButton::BUTTON_RED);
    if (remote_active) {
      display.flushSysEx();
    }
    drawNoiseMakerWaveform(false);
    PlaySlot();
    delay(60);
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SLOT", TouchButton::BUTTON_ACTIVE);
  }

  if (TouchButton::isPressed(GRID.X[5], GRID.Y[2])) {
    if (nm_params.randomizer == false)
    {
      if (nm_params.mode == NM_MODE_TR808)
        nm_params.mode = NM_MODE_TR909;
      else  if (nm_params.mode == NM_MODE_TR909)
        nm_params.randomizer = true;
    }
    else if (nm_params.randomizer)
    {
      nm_params.randomizer = false;
      nm_params.mode = NM_MODE_TR808;
    }
    draw_nm_emulation_mode_button();
  }

  if (TouchButton::isPressed(GRID.X[5], GRID.Y[3]))
  { // mono stereo
    nm_params.stereo_mode = !nm_params.stereo_mode;
    draw_nm_monostereo();
  }
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[4]))
  { //reverb off / on
    nm_params.reverb_enable = !nm_params.reverb_enable;
    draw_nm_reverb_OffOn();
  }
  // Save button
  if (TouchButton::isPressed(GRID.X[4], GRID.Y[5])) {
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SLOT", TouchButton::BUTTON_RED);
    if (remote_active) {
      display.flushSysEx();
    }
    saveNoiseToCustomSlot();
    delay(60);
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SLOT", TouchButton::BUTTON_ACTIVE);
  }
}
#endif

FLASHMEM void handle_touchscreen_mute_matrix()
{
  uint8_t button_count = 0;
  char buf[4];
  uint8_t spacerx = 90;
  for (uint8_t y = 0; y < 3; y++)
  {
    for (uint8_t x = 0; x < 4; x++)
    {
      if (y < 2)
      {
        if (TouchButton::isPressed(x * spacerx, 95 + y * 55))
        {
          seq.track_mute[button_count] = !seq.track_mute[button_count];
          //const uint8_t color = seq.track_mute[button_count] ? 0 : 1; // grey or active color
          //draw_button_on_grid(2 + x * 14, 12 + y * 8, "TRACK:", itoa(button_count + 1, buf, 10), color);
          TouchButton::drawButton(x * spacerx, 95 + y * 55, "TRACK:", itoa(button_count + 1, buf, 10), seq.track_mute[button_count] ? TouchButton::BUTTON_NORMAL : TouchButton::BUTTON_ACTIVE);
        }
        button_count++;
      }
      else
      {
        if (TouchButton::isPressed(x * spacerx, 35))
        {
          if (x == 1)
            seq.mute_mode = 0;
          else if (x == 2)
            seq.mute_mode = 1;
          else if (x == 3)
            seq.mute_mode = 2;

          if (seq.mute_mode == 0)
            TouchButton::drawButton(1 * spacerx, 35, "REAL", "TIME", TouchButton::BUTTON_ACTIVE);
          else
            TouchButton::drawButton(1 * spacerx, 35, "REAL", "TIME", TouchButton::BUTTON_NORMAL);
          if (seq.mute_mode == 1)
            TouchButton::drawButton(2 * spacerx, 35, "NEXT", "PATTRN", TouchButton::BUTTON_ACTIVE);
          else
            TouchButton::drawButton(2 * spacerx, 35, "NEXT", "PATTRN", TouchButton::BUTTON_NORMAL);
          if (seq.mute_mode == 2)
            TouchButton::drawButton(3 * spacerx, 35, "SONG", "STEP", TouchButton::BUTTON_ACTIVE);
          else
            TouchButton::drawButton(3 * spacerx, 35, "SONG", "STEP", TouchButton::BUTTON_NORMAL);
        }
      }
    }
  }
#if defined APC
  apc_mute_matrix();
#endif
}

FLASHMEM void handle_touchscreen_arpeggio()
{
  if (numTouchPoints > 0)
  {
    if (check_button_on_grid(2, 23))
    {
      if (seq.running)
        handleStop();
      else
        handleStart();
    }
  }
}

FLASHMEM void handle_touchscreen_braids()
{
  if (numTouchPoints > 0)
  {
    seq.cycle_touch_element = 1;
  }
  if (seq.cycle_touch_element == 1) {
    handleTouchVirtualKeyboard();
  }
}

FLASHMEM void handle_touchscreen_cv_screen()
{
  if (numTouchPoints > 0)
  {
    seq.cycle_touch_element = 1;
  }
  if (seq.cycle_touch_element == 1) {
    handleTouchVirtualKeyboard();
  }
}

bool chord_autostart = false;
extern void preview_chord_suggestion(uint8_t s);
extern SuggestedChord suggestions[6];

FLASHMEM void handle_touchscreen_chord_arranger()
{
  if (numTouchPoints > 0)
  {
    for (int i = 0; i < 6; i++)
    {
      if (TouchButton::isPressed(GRID.X[i], GRID.Y[4]))
      {
        TouchButton::drawButton(GRID.X[i], GRID.Y[4], suggestions[i].name, suggestions[i].notenames, TouchButton::BUTTON_RED);
        display.flushSysEx();
        preview_chord_suggestion(i);

        TouchButton::drawButton(GRID.X[i], GRID.Y[4], suggestions[i].name, suggestions[i].notenames, TouchButton::BUTTON_NORMAL);
        display.flushSysEx();
      }
    }

    if (TouchButton::isPressed(GRID.X[5], GRID.Y[1] + 16))
      chord_autostart = !chord_autostart;

    TouchButton::drawButton(GRID.X[5], GRID.Y[1] + 16, "AUTO", "START", chord_autostart ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
  }
}

FLASHMEM void draw_menu_ui_icons()
{
#ifdef TOUCH_UI
  // do touch_ui stuff

  draw_button_on_grid(45, 1, "", "", 99); // print keyboard icon

  if (seq.running)
    draw_button_on_grid(45, 7, "SEQ.", "STOP", 2);
  else
    draw_button_on_grid(45, 7, "SEQ.", "START", 1);

  draw_button_on_grid(45, 19, "UP", "", 0);
  draw_button_on_grid(45, 25, "DOWN", "", 0);

#else

  TouchButton::drawButton(GRID.X[0], GRID.Y[3], "DEXED", "VOICE", TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[1], GRID.Y[3], "SONG", "EDITOR", TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[2], GRID.Y[3], "PATT", "EDITOR", TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[3], GRID.Y[3], "LIVE", "SEQ", TouchButton::BUTTON_NORMAL);

  if (seq.running)
    TouchButton::drawButton(GRID.X[5], GRID.Y[3], "SEQ.", "STOP", TouchButton::BUTTON_RED);
  else
    TouchButton::drawButton(GRID.X[5], GRID.Y[3], "SEQ.", "START", TouchButton::BUTTON_ACTIVE);

  TouchButton::drawButton(GRID.X[2], GRID.Y[5], "LOAD", "PERF", TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[3], GRID.Y[5], "SAVE", "PERF", TouchButton::BUTTON_NORMAL);
  if (ts.keyb_in_menu_activated == false)
  {
    if (multiband_active == false && multiband_confirm == true)
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "CONFIRM", "?", TouchButton::BUTTON_ACTIVE);
    else
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "MULTI", "BAND", multiband_active ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
  }

  TouchButton::drawButton(GRID.X[5], GRID.Y[5], "MAIN", "MIXER", TouchButton::BUTTON_NORMAL);

  TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[2]);
#endif
}
extern elapsedMillis save_sys;
extern bool  is_offline_mode;
extern void  toggleOutputMode();


FLASHMEM void _multiband_button(uint8_t x, uint8_t y, uint8_t offset_y)
{
  if (multiband_active == false && multiband_confirm == true)
  {
    multiband_active = true;
    TouchButton::drawButton(GRID.X[x], GRID.Y[y] + offset_y, "MULTI", "BAND", TouchButton::BUTTON_RED);
  }
  else if (multiband_active == true)
  {
    multiband_active = false;
    multiband_confirm = false;
    TouchButton::drawButton(GRID.X[x], GRID.Y[y] + offset_y, "MULTI", "BAND", TouchButton::BUTTON_NORMAL);
  }
  else if (multiband_active == false && multiband_confirm == false)
  {
    multiband_confirm = true;
    save_sys = 0;
    TouchButton::drawButton(GRID.X[x], GRID.Y[y] + offset_y, "CONFIRM", "?", TouchButton::BUTTON_ACTIVE);
  }
  mb_set_master();
}

FLASHMEM void handle_touchscreen_menu()
{
  if ((ts.keyb_in_menu_activated == false) && TouchButton::isPressed(GRID.X[4], GRID.Y[5]))
  {
    _multiband_button(4, 5, 0);
  }

  if (LCDML.FUNC_getID() > _LCDML_DISP_cnt) // only when not in UI_func_volume
  {
    if (ts.touch_ui_drawn_in_menu == false)
    {
      if (ts.keyb_in_menu_activated)
      {
        drawVirtualKeyboard();
        TouchButton::drawButton(GRID.X[5], GRID.Y[2], "V.KEYB.", "OFF", TouchButton::BUTTON_ACTIVE);
      }
      else
      {
        draw_menu_ui_icons();
      }
      ts.touch_ui_drawn_in_menu = true;
    }

    if (TouchButton::isPressed(GRID.X[5], GRID.Y[2]))
    {
      display.console = true;
      display.fillRect(0, GRID.Y[3], DISPLAY_WIDTH, DISPLAY_HEIGHT - GRID.Y[3], COLOR_BACKGROUND);
      display.console = false;
      ts.keyb_in_menu_activated = !ts.keyb_in_menu_activated;
      if (ts.keyb_in_menu_activated)
      {
        drawVirtualKeyboard();
        TouchButton::drawButton(GRID.X[5], GRID.Y[2], "V.KEYB.", "OFF", TouchButton::BUTTON_ACTIVE);
      }
      else
      {
        draw_menu_ui_icons();
        if (LCDML.MENU_getLayer() > last_menu_depth)
        {
          draw_back_touchbutton();
        }
      }
    }
    if (ts.keyb_in_menu_activated == false)
    {
      if (TouchButton::isPressed(GRID.X[0], GRID.Y[3]))
      {
        LCDML.OTHER_jumpToFunc(UI_func_voice_select);
      }
      else if (TouchButton::isPressed(GRID.X[1], GRID.Y[3]))
      {
        LCDML.OTHER_jumpToFunc(UI_func_song);
      }
      else if (TouchButton::isPressed(GRID.X[2], GRID.Y[3]))
      {
        LCDML.OTHER_jumpToFunc(UI_func_seq_pattern_editor);
      }
      else if (TouchButton::isPressed(GRID.X[3], GRID.Y[3]))
      {
        LCDML.OTHER_jumpToFunc(UI_func_livesequencer);
      }
      else if (TouchButton::isPressed(GRID.X[5], GRID.Y[3]))
      {
        if (seq.running)
        {
          handleStop();
        }
        else
        {
          handleStart();
        }
      }
      else if (TouchButton::isPressed(GRID.X[2], GRID.Y[5]))
      {
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        LCDML.OTHER_jumpToFunc(UI_func_load_performance);
      }
      else if (TouchButton::isPressed(GRID.X[3], GRID.Y[5]))
      {
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        LCDML.OTHER_jumpToFunc(UI_func_save_performance);
      }
      else if (TouchButton::isPressed(GRID.X[5], GRID.Y[5]))
      {
        LCDML.OTHER_jumpToFunc(UI_func_mixer);
      }
      else if (TouchButton::isPressed(GRID.X[0], GRID.Y[5]))// back button
      {
        LCDML.BT_quit();
      }

      // else if (TouchButton::isPressed(GRID.X[5], GRID.Y[4]))  // render audio instead of recording test
      // {
      //   toggleOutputMode();

      //   if (is_offline_mode == false)
      //     TouchButton::drawButton(GRID.X[5], GRID.Y[4], "MODE", "LIVE", TouchButton::BUTTON_NORMAL);
      //   else
      //     TouchButton::drawButton(GRID.X[5], GRID.Y[4], "MODE", "RENDER", TouchButton::BUTTON_ACTIVE);
      // }
    }
    ts.touch_ui_drawn_in_menu = true;
  }
  if (ts.keyb_in_menu_activated) {
    handleTouchVirtualKeyboard();
  }
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void toggle_generic_active_function()
{
  if (generic_active_function == 0)
    generic_active_function = 1;
  else
    generic_active_function = 0;
}


FLASHMEM void handle_touchscreen_mixer()
{
  draw_volmeters_mixer();
}

#include "MultiBandCompressor.h" // Needed for MultiBandCompressor_UI_t struct definition

extern MultiBandCompressor_UI_t MultiBandUIparams;
extern AudioMultiBandCompressor MultiBandCompressor;

extern void drawSoloButton(int x, int y, bool is_soloed);

FLASHMEM void handle_touchscreen_multiband()
{
  if (numTouchPoints > 0 && !isButtonTouched) {
    int x = ts.p.x;
    int y = ts.p.y;

    uint8_t solo_mask = MultiBandUIparams.solo_mask;

    // Define touch areas for solo buttons based on their drawing coordinates
    const int solo_button_width_px = 19 * CHAR_width_small - 4;
    const int solo_button_height_px = 8 * CHAR_height_small + 2;

    const int band1_x_start_px = 8 * CHAR_width_small - 4;
    const int band1_y_start_px = 10 * CHAR_height_small - 5;

    const int band2_x_start_px = 28 * CHAR_width_small - 4;
    const int band2_y_start_px = 10 * CHAR_height_small - 5;

    const int band3_x_start_px = 8 * CHAR_width_small - 4;
    const int band3_y_start_px = 17 * CHAR_height_small - 5;

    const int band4_x_start_px = 28 * CHAR_width_small - 4;
    const int band4_y_start_px = 17 * CHAR_height_small - 5;

    const int band_x_starts[] = { band1_x_start_px, band2_x_start_px, band3_x_start_px, band4_x_start_px };
    const int band_y_starts[] = { band1_y_start_px, band2_y_start_px, band3_y_start_px, band4_y_start_px };

    for (uint8_t i = 0; i < 4; i++) {
      if (x > band_x_starts[i] && x < (band_x_starts[i] + solo_button_width_px) &&
        y > band_y_starts[i] && y < (band_y_starts[i] + solo_button_height_px)) {
        solo_mask ^= (1 << i);
        isButtonTouched = true;
        break; // Found the button, no need to check others
      }
    }

    // Check if all bands are soloed (all 4 bits are 1)
    if (solo_mask == 0b1111) {
      MultiBandUIparams.solo_mask = 0; // Turn off soloing if all are selected
    }
    else {
      MultiBandUIparams.solo_mask = solo_mask; // Update the solo mask
    }

    MultiBandCompressor.setUI(MultiBandUIparams);
    // Redraw solo buttons to reflect the updated state.
    drawSoloButton(8, 10, MultiBandUIparams.solo_mask & (1 << 0));
    drawSoloButton(28, 10, MultiBandUIparams.solo_mask & (1 << 1));
    drawSoloButton(8, 17, MultiBandUIparams.solo_mask & (1 << 2));
    drawSoloButton(28, 17, MultiBandUIparams.solo_mask & (1 << 3));
  }

  if (TouchButton::isPressed(GRID.X[0], GRID.Y[1] + 7))
  {
    _multiband_button(0, 1, 7);
  }
  draw_volmeters_multiband_compressor();
}

extern int temp_int;
extern uint8_t num_slices[2];
extern void draw_waveform_slice_editor(uint8_t sliceno);
extern void fill_slices();
extern void clear_slices();
extern bool slices_autoalign;
extern  uint16_t slices_scrollspeed;
extern uint8_t selected_slice_sample[2];
extern void print_sliced_status();
extern void check_and_print_sliced_message();

FLASHMEM void assign_free_slices_or_increment()
{
  if (num_slices[0] == 0 && selected_slice_sample[0] == 99)
  {
    num_slices[0] = 1;
    selected_slice_sample[0] = temp_int; // set sliced sample
    draw_button_on_grid(45, 23, "PLAY", "SLC", 0);
  }
  else if (num_slices[1] == 0 && selected_slice_sample[1] == 99 && selected_slice_sample[0] != 99 && selected_slice_sample[0] != temp_int)
  {
    num_slices[1] = 1;
    selected_slice_sample[1] = temp_int; // set sliced sample
    draw_button_on_grid(45, 23, "PLAY", "SLC", 0);
  }
  else if (selected_slice_sample[0] == temp_int && selected_slice_sample[0] != 99)
  {
    num_slices[0] = num_slices[0] * 2;
    if (num_slices[0] > 16)
    {
      clear_slices();
      num_slices[0] = 0;
      selected_slice_sample[0] = 99; // clear selected sliced sample 1
      draw_button_on_grid(45, 23, "PLAY", "SAMPLE", 0);
      //generic_temp_select_menu = 6;
      display.fillRect(CHAR_width_small * 26, 4 * CHAR_height_small - 2, 160, 39, COLOR_BACKGROUND);  //slices + scrollbar
    }
  }
  else if (selected_slice_sample[1] == temp_int && selected_slice_sample[1] != 99)
  {
    num_slices[1] = num_slices[1] * 2;
    if (num_slices[1] > 16)
    {
      clear_slices();
      num_slices[1] = 0;
      selected_slice_sample[1] = 99; // clear selected sliced sample 2
      draw_button_on_grid(45, 23, "PLAY", "SAMPLE", 0);
      //generic_temp_select_menu = 6;
      display.fillRect(CHAR_width_small * 26, 4 * CHAR_height_small - 2, 160, 39, COLOR_BACKGROUND);  //slices + scrollbar
    }
  }
  fill_slices();
  print_sliced_status();
  draw_waveform_slice_editor(99);
  check_and_print_sliced_message();
}

FLASHMEM void draw_loop_buttons()
{
  if (fm.sample_source == 2 || fm.sample_source == 3)
  {
    draw_button_on_grid(1, 23, "LOOP", "START", 1 + loop_start_button);
    draw_button_on_grid(9, 23, "LOOP", "END", 1 + loop_end_button);
  }
  else  if (fm.sample_source == 4)
  {
    draw_button_on_grid(1, 23, "LOOP", "START", 0);
    draw_button_on_grid(9, 23, "LOOP", "END", 0);
  }
}

extern void print_loop_data();

FLASHMEM void handle_touchscreen_slice_editor()
{
  if (numTouchPoints > 0)
  {
    if (check_button_on_grid(45, 23)) {
      previewWavSliceEditor();
    }

    if (check_button_on_grid(1, 23) && fm.sample_source > 1 && fm.sample_source < 4) //loop start
    {
      // draw_button_on_grid(1, 23, "LOOP", "START", 0);
      if (loop_start_button == false) {
        loop_start_button = true;
        loop_end_button = false;
        seq.edit_state = 1;
        generic_temp_select_menu = 3;
      }
      else {
        loop_start_button = false;
        seq.edit_state = 0;
      }
      draw_loop_buttons();
      print_loop_data();

    }
    else  if (check_button_on_grid(9, 23) && fm.sample_source > 1 && fm.sample_source < 4)  //loop end
    {
      draw_button_on_grid(9, 23, "LOOP", "END", 0);
      if (loop_end_button == false) {
        loop_end_button = true;
        loop_start_button = false;
        seq.edit_state = 1;
        generic_temp_select_menu = 4;
      }
      else {
        loop_end_button = false;
        seq.edit_state = 0;
      }
      draw_loop_buttons();
      print_loop_data();
    }

    if (check_button_on_grid(33, 23) && fm.sample_source == 4) // num slices
    {
      assign_free_slices_or_increment();
    }

    if (check_button_on_grid(25, 23) && fm.sample_source == 4) // auto align (slices only)
    {
      slices_autoalign = !slices_autoalign;
      draw_button_on_grid(25, 23, "AUTO", "ALIGN", 1 + slices_autoalign);
    }

    if (check_button_on_grid(17, 23)) // scrolling speed
    {
      slices_scrollspeed = slices_scrollspeed * 2;
      if (slices_scrollspeed > 512)
        slices_scrollspeed = 1;

      char buf[4] = { 0 };
      draw_button_on_grid(17, 23, "SCRSPD", itoa(slices_scrollspeed, buf, 10), 1);
    }
  }
}

FLASHMEM void handle_touchscreen_settings_button_test()
{
  static bool button_state = false;

  if (numTouchPoints > 0)
  {
    if (check_button_on_grid(42, 1))
    {
      draw_button_on_grid(42, 1, "TOUCH", button_state ? "OK" : "TEST", button_state ? 2 : 0);
      button_state = !button_state;
    }
  }
}

FLASHMEM void handle_touchscreen_test_touchscreen()
{
  if (numTouchPoints > 0)
  {
    if (check_button_on_grid(42, 1))
      sub_touchscreen_test_page_init();
    display.console = true;
    display.fillRect(ts.p.x, ts.p.y, 2, 2, COLOR_SYSTEXT);
    display.console = false;
  }
}

FLASHMEM void handle_page_with_touch_back_button()
{
  if (TouchButton::isPressed(GRID.X[0], GRID.Y[5]) && seq.cycle_touch_element != 1) {// back button
    LCDML.BT_quit();
  }
}

uint8_t perf_load_options = 255;

FLASHMEM void draw_all_performance_load_buttons()
{
  TouchButton::drawButton(GRID.X[0], GRID.Y[2], "DEXED", "", bitRead(perf_load_options, perf_dexed) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[1], GRID.Y[2], "MSYNTH", "", bitRead(perf_load_options, perf_msynth) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[2], GRID.Y[2], "EPIANO", "", bitRead(perf_load_options, perf_epiano) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[3], GRID.Y[2], "BRAIDS", "", bitRead(perf_load_options, perf_braids) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[4], GRID.Y[2], "CUST.", "SAMPLES", bitRead(perf_load_options, perf_samples) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[0], GRID.Y[3], "EFFECT", "SET.", bitRead(perf_load_options, perf_effects) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[1], GRID.Y[3], "SEQ.", "DATA", bitRead(perf_load_options, perf_seq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  TouchButton::drawButton(GRID.X[2], GRID.Y[3], "LIVESEQ", "DATA", bitRead(perf_load_options, perf_liveseq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
}

FLASHMEM void handle_load_performance()
{
  if (TouchButton::isPressed(GRID.X[4], GRID.Y[4]))
  {
    perf_load_options = 0;
    TouchButton::drawButton(GRID.X[4], GRID.Y[4], "NONE", "", TouchButton::BUTTON_RED);
    draw_all_performance_load_buttons();
    delay(300);
    TouchButton::drawButton(GRID.X[4], GRID.Y[4], "NONE", "", TouchButton::BUTTON_NORMAL);
  }
  else if (TouchButton::isPressed(GRID.X[5], GRID.Y[4]))
  {
    perf_load_options = 255;

    TouchButton::drawButton(GRID.X[5], GRID.Y[4], "ALL", "", TouchButton::BUTTON_RED);
    draw_all_performance_load_buttons();
    delay(300);
    TouchButton::drawButton(GRID.X[5], GRID.Y[4], "ALL", "", TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[0], GRID.Y[2]))
  {
    bitWrite(perf_load_options, perf_dexed, !bitRead(perf_load_options, perf_dexed));
    TouchButton::drawButton(GRID.X[0], GRID.Y[2], "DEXED", "", bitRead(perf_load_options, perf_dexed) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[1], GRID.Y[2]))
  {
    bitWrite(perf_load_options, perf_msynth, !bitRead(perf_load_options, perf_msynth));
    TouchButton::drawButton(GRID.X[1], GRID.Y[2], "MSYNTH", "", bitRead(perf_load_options, perf_msynth) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[2], GRID.Y[2]))
  {
    bitWrite(perf_load_options, perf_epiano, !bitRead(perf_load_options, perf_epiano));
    TouchButton::drawButton(GRID.X[2], GRID.Y[2], "EPIANO", "", bitRead(perf_load_options, perf_epiano) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[3], GRID.Y[2]))
  {
    bitWrite(perf_load_options, perf_braids, !bitRead(perf_load_options, perf_braids));
    TouchButton::drawButton(GRID.X[3], GRID.Y[2], "BRAIDS", "", bitRead(perf_load_options, perf_braids) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[4], GRID.Y[2]))
  {
    bitWrite(perf_load_options, perf_samples, !bitRead(perf_load_options, perf_samples));
    TouchButton::drawButton(GRID.X[4], GRID.Y[2], "CUST.", "SAMPLES", bitRead(perf_load_options, perf_samples) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[0], GRID.Y[3]))
  {
    bitWrite(perf_load_options, perf_effects, !bitRead(perf_load_options, perf_effects));
    TouchButton::drawButton(GRID.X[0], GRID.Y[3], "EFFECT", "SET.", bitRead(perf_load_options, perf_effects) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[1], GRID.Y[3]))
  {
    bitWrite(perf_load_options, perf_seq, !bitRead(perf_load_options, perf_seq));
    TouchButton::drawButton(GRID.X[1], GRID.Y[3], "SEQ.", "DATA", bitRead(perf_load_options, perf_seq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
  if (TouchButton::isPressed(GRID.X[2], GRID.Y[3]))
  {
    bitWrite(perf_load_options, perf_liveseq, !bitRead(perf_load_options, perf_liveseq));
    TouchButton::drawButton(GRID.X[2], GRID.Y[3], "LIVESEQ", "DATA", bitRead(perf_load_options, perf_liveseq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
  }
}
