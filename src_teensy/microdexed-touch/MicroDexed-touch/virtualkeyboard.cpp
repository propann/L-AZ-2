#include "virtualkeyboard.h"
#include "touchbutton.h"
#include "UI.h"
#include "sequencer.h"
#include "livesequencer.h"

#include "touch.h"
#include "LCDMenuLib2.h"
#include "ILI9341_t3n.h"
#include "drums.h"

static constexpr float KEY_WIDTH_WHITE = 30;
static constexpr float KEY_HEIGHT_WHITE = 74;
static constexpr float KEY_SPACING_WHITE = 2;
static constexpr float KEY_LABEL_OFFSET = 9;
static constexpr float KEY_OFFSET_BLACK = 18.7;
static constexpr float KEY_WIDTH_BLACK = 22;
static constexpr float KEY_HEIGHT_BLACK = 34;

extern LCDMenuLib2 LCDML;
extern ILI9341_t3n display;
extern ts_t ts;
extern sequencer_t seq;
extern uint8_t selected_instance_id; // dexed
extern uint8_t drum_midi_channel;
extern config_t configuration;
extern microsynth_t microsynth[NUM_MICROSYNTH];
extern braids_t braids_osc;
extern uint8_t microsynth_selected_instance;
extern drum_config_t drum_config[NUM_DRUMSET_CONFIG];
extern int numTouchPoints;
extern void set_sample_pitch(uint8_t sample, float playbackspeed);
extern float get_sample_p_offset(uint8_t sample);
extern void handleNoteOn_MIDI_DEVICE_DIN(byte inChannel, byte inNumber, byte inVelocity);
extern void handleNoteOff_MIDI_DEVICE_DIN(byte inChannel, byte inNumber, byte inVelocity);

void printDrumPad(uint8_t padIndex, TouchButton::Color color);
void virtual_keyboard_print_current_instrument();
void print_drumpads();
void touch_check_all_keyboard_buttons();

bool dark_keyboard;

FLASHMEM void drawVirtualKeyboard(uint8_t drawFlags)
{
  if (ts.virtual_keyboard_instrument == VK_LIVESEQUENCER) {
    if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_livesequencer)) {
      ts.virtual_keyboard_instrument = VK_DEXED0;
    }
    // do not draw instrument buttons for live sequencer
    drawFlags &= ~VK_DRAW_INSTRUMENT_BUTTONS;
  }
  else { // livesequencer assigns display mode itself
    ts.current_virtual_keyboard_display_mode = (ts.virtual_keyboard_instrument == VK_DRUMS) ? 1 : 0; // pad view for drums, key for other
  }
  const bool modeChanged = ts.current_virtual_keyboard_display_mode != ts.previous_virtual_keyboard_display_mode;
  if (modeChanged) {
    // clear keys area
    display.console = true;
    display.fillRect(0, GRID.Y[4], DISPLAY_WIDTH, DISPLAY_HEIGHT - GRID.Y[4], COLOR_BACKGROUND);

    drawFlags |= VK_DRAW_KEYS | VK_DRAW_OCTAVE;
  }
  ts.previous_virtual_keyboard_display_mode = ts.current_virtual_keyboard_display_mode;

  if (drawFlags & VK_DRAW_OCTAVE_BUTTONS) {
    // draw oct +- buttons
    TouchButton::drawButton(GRID.X[0], GRID.Y[3], "OCTAVE", "-", TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[5], GRID.Y[3], "OCTAVE", "+", TouchButton::BUTTON_NORMAL);
  }

  if (drawFlags & VK_DRAW_INSTRUMENT_BUTTONS) {
    // draw instrument buttons
    TouchButton::drawButton(GRID.X[1], GRID.Y[3], "INSTR.", "-", TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[4], GRID.Y[3], "INSTR.", "+", TouchButton::BUTTON_NORMAL);
  }

  if (drawFlags & VK_DRAW_INSTRUMENT_NAME) {
    virtual_keyboard_print_current_instrument();
  }

  display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  display.setTextSize(1);

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element == 1 && seq.step_recording) {
    virtual_keyboard_print_velocity_bar();
  }

  if (drawFlags & VK_DRAW_KEYS) {
    switch (ts.current_virtual_keyboard_display_mode) {
    case 0: //piano keys
      // draw white keys
      for (uint8_t x = 0; x < 10; x++) {
        display.console = true;
        display.fillRect(1 + x * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE), VIRT_KEYB_YPOS, KEY_WIDTH_WHITE, KEY_HEIGHT_WHITE, dark_keyboard ? GREY3 : COLOR_SYSTEXT); // WHITE key
        display.console = false;
      }

      // draw black keys
      for (uint8_t x = 0; x < 16; x++) {
        if (seq.piano[x] == 1) {
          display.console = true;
          display.fillRect(x * KEY_OFFSET_BLACK, VIRT_KEYB_YPOS, KEY_WIDTH_BLACK, KEY_HEIGHT_BLACK, COLOR_BACKGROUND); // BLACK key
          display.console = false;
        }
      }
      break;

    case 1: // drum pads
      print_drumpads();
      break;
    }
  }

  if (drawFlags & VK_DRAW_OCTAVE) {
    display.setCursor(32 * CHAR_width_small + 2, 17 * CHAR_height_small + 3);
    display.setTextColor(dark_keyboard ? GREY2:COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print("OCT");
    display.setTextSize(2);
    display.setCursor(33 * CHAR_width_small + 3, 18 * CHAR_height_small + 3);
    display.print(ts.virtual_keyboard_octave);
    display.setTextSize(1);

    if (ts.current_virtual_keyboard_display_mode == 0) { // draw octave labels for keys mode
      static constexpr uint8_t indexes[2] = { 0, 7 };
      for (int i = 0; i < 2; i++) {
        if (ts.virtual_keyboard_state_white & (1 << indexes[i])) {
          display.setTextColor(COLOR_SYSTEXT, RED);
        }
        else {
          display.setTextColor( COLOR_BACKGROUND, dark_keyboard ? GREY3 : COLOR_SYSTEXT);
        }
        display.setCursor(1 + indexes[i] * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE) + KEY_LABEL_OFFSET, VIRT_KEYB_YPOS + 57.75);
        display.print("C");
        display.print(ts.virtual_keyboard_octave + i);
      }
    }
  }
  display.console = false;
}

FLASHMEM void virtual_keyboard_key_off_white(uint8_t x)
{
  uint8_t halftones = 0;
  display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  display.setTextSize(1);

  // draw white keys
  for (uint8_t z = 0; z < x; z++) {
    if (seq.piano2[z] == 1) {
      halftones = halftones + 1;
    }
  }
  handleNoteOff_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x + halftones, 0);
  
  display.console = true;
  display.fillRect(1 + x * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE), VIRT_KEYB_YPOS + 34, KEY_WIDTH_WHITE, KEY_HEIGHT_WHITE, dark_keyboard ? GREY3 : COLOR_SYSTEXT); // white key
  display.console = false;

  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.console = false;
}

FLASHMEM void virtual_keyboard_key_off_black(uint8_t x)
{
  display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  display.setTextSize(1);

  handleNoteOff_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x, 0);
  display.console = true;
  display.fillRect(x * KEY_OFFSET_BLACK, VIRT_KEYB_YPOS, KEY_WIDTH_BLACK, KEY_HEIGHT_BLACK, COLOR_BACKGROUND); // BLACK key
  display.console = false;

  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.console = false;
}

FLASHMEM void touch_button_oct_up()
{
  if (++ts.virtual_keyboard_octave > 8) {
    ts.virtual_keyboard_octave = 8;
  }
  if (ts.current_virtual_keyboard_display_mode == 1) {
    drawVirtualKeyboard(VK_DRAW_OCTAVE | VK_DRAW_KEYS);
  }
  else {
    drawVirtualKeyboard(VK_DRAW_OCTAVE);
  }
}

FLASHMEM void touch_button_oct_down()
{
  if (--ts.virtual_keyboard_octave == 0) {
    ts.virtual_keyboard_octave = 1;
  }
  if (ts.current_virtual_keyboard_display_mode == 1) {
    drawVirtualKeyboard(VK_DRAW_OCTAVE | VK_DRAW_KEYS);
  }
  else {
    drawVirtualKeyboard(VK_DRAW_OCTAVE);
  }
}

FLASHMEM void touch_button_inst_up()
{
  ts.virtual_keyboard_instrument++;
  if (ts.virtual_keyboard_instrument == VK_DRUMS && ts.virtual_keyboard_octave > 6) {
    ts.virtual_keyboard_octave = 6;
  }
  if (ts.virtual_keyboard_instrument > 14) {
    ts.virtual_keyboard_instrument = 14;
  }

  drawVirtualKeyboard(VK_DRAW_INSTRUMENT_NAME);
}
FLASHMEM void touch_button_inst_down()
{
  if (ts.virtual_keyboard_instrument > 1) {
    ts.virtual_keyboard_instrument--;
  }

  if (ts.virtual_keyboard_instrument == VK_DRUMS && ts.virtual_keyboard_octave > 6) {
    ts.virtual_keyboard_octave = 6;
  }

  drawVirtualKeyboard(VK_DRAW_INSTRUMENT_NAME);
}

FLASHMEM void touch_check_all_keyboard_buttons()
{
  // octave buttons
  if (TouchButton::isPressed(GRID.X[0], GRID.Y[3])) {
    touch_button_oct_down();
  }
  if (TouchButton::isPressed(GRID.X[5], GRID.Y[3])) {
    touch_button_oct_up();
  }
  //darkmode
  if (TouchButton::isPressed(GRID.X[2], GRID.Y[3]) || TouchButton::isPressed(GRID.X[3], GRID.Y[3])) {
    dark_keyboard = !dark_keyboard;
    drawVirtualKeyboard();
  }
  // instrument buttons (disabled for livesequencer)
  if (ts.virtual_keyboard_instrument != 0) {
    if (TouchButton::isPressed(GRID.X[1], GRID.Y[3])) {
      touch_button_inst_down();
    }
    if (TouchButton::isPressed(GRID.X[4], GRID.Y[3])) {
      touch_button_inst_up();
    }
  }
}

#ifdef GRANULAR
#include "granular.h"
extern granular_params_t granular_params;
#endif

FLASHMEM void virtual_keyboard_print_current_instrument()
{
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  display.setTextSize(2);
  display.setCursor(18 * CHAR_width_small, 16 * CHAR_height_small);
  if (ts.virtual_keyboard_instrument < 11 || ts.virtual_keyboard_instrument == 14) {
    display.print(F("PLAYING"));
    display.console = 1;
    display.fillRect(33 * CHAR_width_small, 16 * CHAR_height_small, 3 * CHAR_width_small, CHAR_height_small, COLOR_BACKGROUND);
    display.console = 0;
  }
  else if (ts.virtual_keyboard_instrument < 14) {
    display.setTextSize(1);
    display.print(F("PITCHED SAMPLE #"));
    display.print(ts.virtual_keyboard_instrument - 10);
    display.print(" ");

    display.console = 1;
    display.fillRect(18 * CHAR_width_small, 17 * CHAR_height_small, 14 * CHAR_width_small, CHAR_height_small, COLOR_BACKGROUND);
    display.console = 0;
  }

  display.setTextSize(2);
  display.setCursor(18 * CHAR_width_small, 18 * CHAR_height_small + 3);
  display.setTextColor(dark_keyboard ? GREY2: COLOR_SYSTEXT, COLOR_BACKGROUND);

  switch (ts.virtual_keyboard_instrument) {
  case VK_LIVESEQUENCER:
    display.print(F("LIVESEQ"));
    ts.virtual_keyboard_midi_channel = 20;  //fake channel for livesequencer
    break;
     case VK_GRANULAR:
    display.print(F("GRANULE"));
    #ifdef GRANULAR
    ts.virtual_keyboard_midi_channel = granular_params.midi_channel; 
    #endif
    break;
  case VK_CV1:
    display.print(F("CV1    "));
    ts.virtual_keyboard_midi_channel = configuration.sys.cv_midi_channel; 
    break;
  case VK_DEXED0:
    display.print(F("DEXED1 "));
    ts.virtual_keyboard_midi_channel = configuration.dexed[0].midi_channel;
    break;
  case VK_DEXED1:
    display.print(F("DEXED2 "));
    ts.virtual_keyboard_midi_channel = configuration.dexed[1].midi_channel;
    break;
  case VK_DEXED2:
    display.print(F("DEXED3 "));
    if (NUM_DEXED > 2)
      ts.virtual_keyboard_midi_channel = configuration.dexed[2].midi_channel;
    break;
  case VK_DEXED3:
    display.print(F("DEXED4 "));
    if (NUM_DEXED > 2)
      ts.virtual_keyboard_midi_channel = configuration.dexed[3].midi_channel;
    break;
  case VK_MICROSYNTH0:
    display.print(F("MSYNTH1"));
    ts.virtual_keyboard_midi_channel = microsynth[0].midi_channel;
    break;
  case VK_MICROSYNTH1:
    display.print(F("MSYNTH2"));
    ts.virtual_keyboard_midi_channel = microsynth[1].midi_channel;
    break;
  case VK_EPIANO:
    display.print(F("EPIANO "));
    ts.virtual_keyboard_midi_channel = configuration.epiano.midi_channel;
    break;
  case VK_DRUMS:
    display.print(F("DRUMS  "));
    ts.virtual_keyboard_midi_channel = drum_midi_channel;
    break;
  case VK_BRAIDS:
    display.print(F("BRAIDS "));
    ts.virtual_keyboard_midi_channel = braids_osc.midi_channel;
    break;
  default: // >9
    show_no_grid(18 * CHAR_height_small + 3, 18 * CHAR_width_small, 7, get_drum_name_from_note(ts.virtual_keyboard_instrument - 8 + 210));
    ts.virtual_keyboard_midi_channel = drum_midi_channel;
    break;
  }
}

FLASHMEM void printDrumPad(uint8_t padIndex, TouchButton::Color color) {
  static constexpr uint8_t LINECHARS = 6;
  static constexpr uint8_t offset = 18;
  std::string line1;
  std::string line2;
  const int16_t sampleIndex = padIndex + ts.virtual_keyboard_octave * 12 - offset;
  if (sampleIndex >= 0 && sampleIndex < NUM_DRUMSET_CONFIG) {
    line1 = std::string(&drum_config[sampleIndex].name[0], LINECHARS);
    line2 = std::string(&drum_config[sampleIndex].name[LINECHARS], LINECHARS);
  }
  TouchButton::drawButton(GRID.X[padIndex % 6], GRID.Y[4 + (padIndex / 6)], line1, line2, color);
}

FLASHMEM void print_drumpads() {
  for (uint8_t x = 0; x < 12; x++) {
    printDrumPad(x, TouchButton::BUTTON_ACTIVE);
  }
}

FLASHMEM void handleTouchVirtualKeyboard()
{
  touch_check_all_keyboard_buttons();

  bool isPressed = numTouchPoints > 0;

  if (ts.current_virtual_keyboard_display_mode == 0)
  {
    uint8_t halftones = 0;
    display.setTextColor(COLOR_SYSTEXT);
    display.setTextSize(1);

    // draw white keys
    bool isWithinWhiteY = ts.p.y > VIRT_KEYB_YPOS + 36;
    for (uint8_t x = 0; x < 10; x++) {
      bool isWithinKey = isPressed && ts.p.x > x * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE) && ts.p.x < (x + 1) * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE) && isWithinWhiteY;
      bool isKeyPress = (ts.virtual_keyboard_state_white & (1 << x)) == 0 && isWithinKey;
      bool isKeyRelease = (ts.virtual_keyboard_state_white & (1 << x)) != 0 && !isWithinKey;

      if (isKeyPress) {
        ts.virtual_keyboard_state_white |= (1 << x);

        for (uint8_t z = 0; z < x; z++) {
          if (seq.piano2[z] == 1) {
            halftones = halftones + 1;
          }
        }
        // pitched samples
        if (ts.virtual_keyboard_instrument > VK_GRANULAR && ts.virtual_keyboard_instrument != VK_CV1) {
          set_sample_pitch(ts.virtual_keyboard_instrument - 8, (float)pow(2, (ts.virtual_keyboard_octave * 12 + x + halftones - 72) / 12.00) * get_sample_p_offset(ts.virtual_keyboard_instrument - 8));
          if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) && seq.cycle_touch_element == 1))
          {
            handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, 210 + ts.virtual_keyboard_instrument - 8, ts.virtual_keyboard_velocity);
          }
          else {
            handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, 210 + ts.virtual_keyboard_instrument - 8, 100);
          }
        }
        else if (ts.virtual_keyboard_instrument == VK_CV1)
        {
          handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x + halftones + 12, 120);
        }
        else
        {
          if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) && seq.cycle_touch_element == 1))
          {
            handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x + halftones, ts.virtual_keyboard_velocity);
          }
          else {
            handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x + halftones, 120);
          }
        }
        display.console = true;
        display.fillRect(1 + x * (KEY_WIDTH_WHITE + KEY_SPACING_WHITE), VIRT_KEYB_YPOS + 34, KEY_WIDTH_WHITE, KEY_HEIGHT_WHITE, RED); // white key
        display.console = false;
      }
      else if (isKeyRelease) {
        ts.virtual_keyboard_state_white &= ~(1 << x);
        virtual_keyboard_key_off_white(x);
      }
      if (isKeyPress || isKeyRelease) {
        drawVirtualKeyboard(VK_DRAW_OCTAVE);
      }
    }
    bool isWithinBlackY = ts.p.y > VIRT_KEYB_YPOS && ts.p.y < VIRT_KEYB_YPOS + 34;
    for (uint8_t x = 0; x < 16; x++) {
      if (seq.piano[x] == 1) {
        bool isWithinKey = isPressed && ts.p.x > x * 18.46 && ts.p.x < x * 18.46 + 24 && isWithinBlackY;
        bool isKeyPress = (ts.virtual_keyboard_state_black & (1 << x)) == 0 && isWithinKey;
        bool isKeyRelease = (ts.virtual_keyboard_state_black & (1 << x)) != 0 && !isWithinKey;

        if (isKeyPress) {
          ts.virtual_keyboard_state_black |= (1 << x);

          // pitched samples
          if (ts.virtual_keyboard_instrument > VK_GRANULAR && ts.virtual_keyboard_instrument != VK_CV1) {
            set_sample_pitch(ts.virtual_keyboard_instrument - 8, (float)pow(2, (ts.virtual_keyboard_octave * 12 + x - 72) / 12.00) * get_sample_p_offset(ts.virtual_keyboard_instrument - 8));
            if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) && seq.cycle_touch_element == 1)) {
              handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, 210 + ts.virtual_keyboard_instrument - 8, ts.virtual_keyboard_velocity);
            }
            else {
              handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, 210 + ts.virtual_keyboard_instrument - 8, 100);
            }
          }
          else if (ts.virtual_keyboard_instrument == VK_CV1)
          {
            handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x + 12, 120);
          }
          else {
            if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.cycle_touch_element == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) && seq.cycle_touch_element == 1)) {
              handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x, ts.virtual_keyboard_velocity);
            }
            else {
              handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, ts.virtual_keyboard_octave * 12 + x, 120);
            }
          }
          display.console = true;
          display.fillRect(x * KEY_OFFSET_BLACK, VIRT_KEYB_YPOS, KEY_WIDTH_BLACK, KEY_HEIGHT_BLACK, RED); // BLACK key
          display.console = false;
        }
        else if (isKeyRelease) {
          ts.virtual_keyboard_state_black &= ~(1 << x);
          virtual_keyboard_key_off_black(x);
        }
      }
    }
  }

  else if (ts.current_virtual_keyboard_display_mode == 1) {
    if (ts.virtual_keyboard_instrument == VK_DRUMS || ts.virtual_keyboard_instrument == 0) {
      for (uint8_t x = 0; x < 12; x++) {
        const bool isWithinPad = TouchButton::isInArea(GRID.X[x % 6], GRID.Y[4 + (x / 6)], TouchButton::BUTTON_SIZE_X, TouchButton::BUTTON_SIZE_Y);
        const bool isPadPress = (ts.virtual_keyboard_state_white & (1 << x)) == 0 && isWithinPad;
        const bool isPadRelease = (ts.virtual_keyboard_state_white & (1 << x)) != 0 && !isWithinPad;
        const uint8_t note = x + ts.virtual_keyboard_octave * 12 - 18;

        if (isPadPress) {
          printDrumPad(x, TouchButton::BUTTON_HIGHLIGHTED);
          ts.virtual_keyboard_state_white |= (1 << x);
          handleNoteOn_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, drum_config[note].midinote, ts.virtual_keyboard_velocity);
        }
        if (isPadRelease) {
          printDrumPad(x, TouchButton::BUTTON_ACTIVE);
          ts.virtual_keyboard_state_white &= ~(1 << x);
          handleNoteOff_MIDI_DEVICE_DIN(ts.virtual_keyboard_midi_channel, drum_config[note].midinote, ts.virtual_keyboard_velocity);
        }
      }
    }
  }

  // display.fillRect(ts.p.x-1,ts.p.y-1,3,3,YELLOW);
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.console = false;
}

FLASHMEM void virtual_keyboard_print_velocity_bar()
{
  // velocity bar disabled

  // if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) && seq.step_recording == false && seq.cycle_touch_element == 1) ||
  //   (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) && seq.step_recording == false && seq.cycle_touch_element == 1))
  // {
  //   display.console = 1;
  //   display.fillRect(0, 10 * CHAR_height_small, DISPLAY_WIDTH, 40, COLOR_BACKGROUND);
  //   display.console = 0;
  // } else

  if (seq.cycle_touch_element == 1)
  {
    // velocity bar enabled
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(1);
    display.setCursor(0, 11 * CHAR_height_small + 1);
    display.print(F("VELOCITY"));
    display.setCursor(0, 12 * CHAR_height_small + 3);
    print_formatted_number(ts.virtual_keyboard_velocity, 3);
    display.console = 1;
    display.drawRect(CHAR_width_small * 9 - 1, 11 * CHAR_height_small, CHAR_width * 17 - CHAR_width_small * 10 + 4, 20, GREY1);
    display.console = 1;
    display.fillRect(CHAR_width_small * 9, 11 * CHAR_height_small + 2, ts.virtual_keyboard_velocity * 1.140 + 1, 16, COLOR_PITCHSMP);
    if (ts.virtual_keyboard_velocity < 127 && ts.virtual_keyboard_velocity != 0)
    {
      display.console = 1;
      display.fillRect(CHAR_width_small * 9 + ts.virtual_keyboard_velocity * 1.142 + 1, 11 * CHAR_height_small + 2,
        CHAR_width * 17 - CHAR_width_small * 10 + 2 - ts.virtual_keyboard_velocity * 1.142, 16, GREY3);
      display.console = 0;
    }
  }
}

FLASHMEM void virtual_keyboard_smart_preselect_mode()
{
  //tying to auto select mode 

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {
    if (seq.content_type[seq.active_pattern] == 0)
    {
      ts.current_virtual_keyboard_display_mode = 1;
      ts.virtual_keyboard_instrument = VK_DRUMS;
    }
    else
    {
      if (seq.content_type[seq.active_pattern] != 0)
      {
        ts.current_virtual_keyboard_display_mode = 0;
        ts.virtual_keyboard_instrument = 1;
      }
    }
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_voice_select))
  {
    ts.current_virtual_keyboard_display_mode = 0;
    if (seq.cycle_touch_element == 0)
      ts.virtual_keyboard_instrument = selected_instance_id + 1;
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_braids))
  {
    ts.current_virtual_keyboard_display_mode = 0;
    ts.virtual_keyboard_instrument = VK_BRAIDS;
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_microsynth))
  {
    ts.current_virtual_keyboard_display_mode = 0;
    ts.virtual_keyboard_instrument = microsynth_selected_instance + 5;
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_cv_page))
  {
    ts.current_virtual_keyboard_display_mode = 0;
    ts.virtual_keyboard_instrument = 14;
  }
}