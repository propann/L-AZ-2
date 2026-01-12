/*
   MicroDexed-touch

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6/4.x with audio shield.
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-android

   (c)2018-2021 H. Wirtz <wirtz@parasitstudio.de>
   (c)2021      H. Wirtz <wirtz@parasitstudio.de>, M. Koslowski <positionhigh@gmx.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _UI_HPP_
#define _UI_HPP_

#include "UI.h"
#include <LCDMenuLib2.h>
#include <Regexp.h>
#include "disp_plus.h"
#include "effect_modulated_delay.h"
#include "effect_stereo_mono.h"
#include "effect_platervbstereo.h"
#include "effect_stereo_panorama.h"
#include "effect_delay_ext8.h"
#include "effect_midside.h"
#include "effect_mono_stereo.h"
#include "effect_gate.h"
#include "template_mixer.hpp"
#include "drumset.h"
#include "sequencer.h"
#include "touch.h"
#include "splash_image.h"
#include "dexed_sd.h"
#include "screensaver.h"
#include "scope.h"
#include "psramloader.h"
#include "PitchSamplePlayer.h"
#include "microsynth.h"
#include "noisemaker.h"
#include "MultiBandCompressor.h" 

#include "ui_liveseq_pianoroll.h"
#include "ui_livesequencer.h"
#include "livesequencer.h"
#include "virtualkeyboard.h"
#include <synth_braids.h>
#include "braids.h"
#include "synth_mda_epiano.h"
#include "midi_devices.h"
#include "effect_dynamics.h"
#include "TeensyTimerTool.h"
#ifdef MIDI_DEVICE_USB_HOST
#include <USBHost_t36.h>
#include "config.h"
#include "virtualtypewriter.h"
#endif

using namespace TeensyTimerTool;
extern bool virtual_typewriter_active;

// File Manager: highlights the next PSRAM CUSTOM slot target during COPY > PSRAM
static int16_t fm_psram_copy_target = -1; // -1 = none, otherwise 0..(customSlots-1)

// SMF Player - to avoid duplicated notes
#if defined SMFPLAYER
static uint8_t smf_last_noteon_note[16] = { 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255 };
#endif

//globals for combined encoder text input and touch typewriter

#include "virtualtypewriter.h"

#ifdef GRANULAR
#include "granular.h"
// Global granular parameters
extern granular_params_t granular_params;
extern AudioFilterBiquad granular_filter_l;
extern AudioFilterBiquad granular_filter_r;
#endif

// Editable string and length
char edit_string_global[FILENAME_LEN];
uint8_t edit_len_global;

// Cursor position in the string
uint8_t edit_pos_global;

// Flag for edit mode
bool edit_mode_global;

#if defined APC
#include "apc.h"
extern uint8_t APC_MODE;
extern bool apc_scroll_message;
#endif

extern LiveSequencer liveSeq;
extern UI_LiveSequencer* ui_liveSeq;
extern bool touch_ic_found;
extern AudioSynthEPiano ep;
extern JoystickController joysticks[];
extern bool remote_active;
extern bool back_button_touch_page_check_and_init_done;
extern void set_dexed_delay_level(uint8_t delay_no, uint8_t instance, uint8_t level);
extern void handleStart(void);
extern void handleStop(void);
extern void handleNoteOn(byte inChannel, byte inNumber, byte inVelocity, byte device);
extern void handleNoteOff(byte inChannel, byte inNumber, byte inVelocity, byte device);

// sidechains
extern uint8_t sidechain_sample_number;
extern int sidechain_speed;
extern bool sidechain_active;
extern uint8_t sidechain_steps;

struct DxPoolSettings {
  uint8_t pool;
  uint8_t bank;
  uint8_t voice;
  uint8_t transpose;
};

File frec;
elapsedMillis gamepad_millis;
int gamepad_accelerate;

uint8_t gamepad_last_dir;

uint8_t GAMEPAD_UP_0 = 127;
uint8_t GAMEPAD_UP_1 = 0;
uint8_t GAMEPAD_DOWN_0 = 127;
uint8_t GAMEPAD_DOWN_1 = 255;
uint8_t GAMEPAD_RIGHT_0 = 255;
uint8_t GAMEPAD_RIGHT_1 = 127;
uint8_t GAMEPAD_LEFT_0 = 0;
uint8_t GAMEPAD_LEFT_1 = 127;
uint32_t GAMEPAD_SELECT = 256;
uint32_t GAMEPAD_START = 512;
uint32_t GAMEPAD_BUTTON_A = 2;
uint32_t GAMEPAD_BUTTON_B = 1;

uint32_t gamepad_buttons_neutral;
uint8_t gamepad_0_neutral;
uint8_t gamepad_1_neutral;

uint8_t previous_scroll_position = 255;
uint8_t fade_text = 255;

bool lastWasCustomSample = false;
bool drumScreenActive = false;

#ifdef MIDI_ACTIVITY_LIGHTS
uint8_t LED_MIDI_IN = 40;
uint8_t LED_MIDI_OUT = 36;
#endif 

extern uint8_t external_psram_size;
uint32_t psram_free_bytes = 0;
PsramLoader sampleLoader(psram_free_bytes);

#if defined GENERIC_DISPLAY
ILI9341_t3n display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO);
#else
ILI9341_t3n display(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK);
#endif

MD_REncoder ENCODER[NUM_ENCODER]{ MD_REncoder(ENC_R_PIN_B, ENC_R_PIN_A), MD_REncoder(ENC_L_PIN_B, ENC_L_PIN_A) };

#if defined SMFPLAYER
MD_MIDIFile SMF;
#endif

uint8_t activeSample;
uint8_t generic_temp_select_menu;
uint8_t generic_active_function = 99;
uint8_t generic_menu;
bool generic_full_draw_required = false;
uint8_t num_slices[2] = { 0,0 };
uint8_t selected_slice_sample[2] = { 99,99 };
uint32_t slice_start[2][16];
uint32_t slice_end[2][16];
uint16_t slices_scrollspeed = 1;
bool slices_autoalign = true;

bool remote_touched;

extern int menuhelper_previous_val;
bool check_and_confirm_midi_channels;

extern AudioFilterStateVariable global_delay_filter[NUM_DEXED];
extern AudioMixer<4> global_delay_filter_mixer[NUM_DEXED];

bool multiband_confirm = false;
bool multiband_active = false;

extern AudioMixer<4> mb_mixer_l;
extern AudioMixer<4> mb_mixer_r;
extern AudioMixer<3> finalized_mixer_r;
extern AudioMixer<3> finalized_mixer_l;

#ifdef AUDIO_DEVICE_USB
AudioOutputUSB usb1;
AudioConnection patchCordUsbL(finalized_mixer_l, 0, usb1, 1);
AudioConnection patchCordUsbR(finalized_mixer_r, 0, usb1, 0);
#endif

#ifndef MCP_CV
AudioOutputSPDIF3 spdif3; // enable S/PDIF by default
AudioConnection patchCord_spdif_L(finalized_mixer_l, 0, spdif3, 1); //enable S/PDIF OUTPUT BY DEFAULT
AudioConnection patchCord_spdif_R(finalized_mixer_r, 0, spdif3, 0);
#endif

extern AudioAnalyzePeak mb_before_l;
extern AudioAnalyzePeak mb_before_r;
extern AudioAnalyzePeak mb_after_l;
extern AudioAnalyzePeak mb_after_r;

#define _LCDML_DISP_cols display_cols
#define _LCDML_DISP_rows display_rows

#define _LCDML_DISP_cfg_cursor 0xda // cursor Symbol
#define _LCDML_DISP_cfg_scrollbar 1 // enable a scrollbar

extern PeriodicTimer sequencer_timer;

#if defined GENERIC_DISPLAY 
XPT2046_Touchscreen touch(TFT_TOUCH_CS);
#endif

#ifdef CAPACITIVE_TOUCH_DISPLAY
Adafruit_FT6206 touch = Adafruit_FT6206();
#endif

extern SdVolume volume;
elapsedMillis record_timer;
uint16_t record_x_pos;

extern config_t configuration;
extern void set_volume(uint8_t v, uint8_t m);
extern void generate_version_string(char* buffer, uint8_t len);
extern float midi_volume_transform(uint8_t midi_amp);
extern float volume_transform(float amp);
extern uint8_t selected_instance_id;
extern char receive_bank_filename[FILENAME_LEN];
extern uint8_t get_song_length(void);
extern uint8_t microsynth_selected_instance;
extern AudioMixer<2> microsynth_mixer_reverb;
extern void stop_all_drum_slots();
extern void dac_mute(void);
extern void dac_unmute(void);
extern fm_t fm;
extern custom_midi_map_t custom_midi_map[NUM_CUSTOM_MIDI_MAPPINGS];
extern dexed_live_mod_t dexed_live_mod;
extern AudioRecordQueue record_queue_l;
extern AudioRecordQueue record_queue_r;
extern char filename[CONFIG_FILENAME_LEN];
extern uint8_t remote_MIDI_CC;
extern uint8_t remote_MIDI_CC_value;

#if NUM_DRUMS > 0
#include "drums.h"
extern uint8_t drum_midi_channel;
extern uint8_t slices_midi_channel;
extern drum_config_t drum_config[NUM_DRUMSET_CONFIG];
#endif

extern sequencer_t seq;
extern multisample_t msp[NUM_MULTISAMPLES];
extern multisample_zone_t msz[NUM_MULTISAMPLES][NUM_MULTISAMPLE_ZONES];

// FX
extern AudioSynthWaveform* chorus_modulator[NUM_DEXED];
extern AudioMixer<2 * NUM_DRUMS + 4>* global_delay_in_mixer[2];
extern AudioMixer<2>* delay_fb_mixer[NUM_DEXED];

#ifdef PSRAM
extern AudioEffectDelayExternal8* delay_fx[2];
#else
extern AudioEffectDelay* delay_fx[2];
#endif

extern AudioMixer<2>* delay_mixer[NUM_DEXED];

extern AudioMixer<2> dexed_chorus_mixer_r[NUM_DEXED];
extern AudioMixer<2> dexed_chorus_mixer_l[NUM_DEXED];

extern AudioEffectMonoStereo* dexed_mono2stereo[NUM_DEXED];
extern AudioEffectMonoStereo* delay_mono2stereo[NUM_DEXED];

extern AudioAnalyzePeak microdexed_peak_0;
extern AudioAnalyzePeak microdexed_peak_1;
#if (NUM_DEXED>2)
extern AudioAnalyzePeak microdexed_peak_2;
extern AudioAnalyzePeak microdexed_peak_3;
#endif

extern AudioAnalyzePeak ep_peak_l;
extern AudioAnalyzePeak ep_peak_r;
extern AudioAnalyzePeak microsynth_peak_osc_0;
extern AudioAnalyzePeak microsynth_peak_osc_1;
extern AudioAnalyzePeak braids_peak_l;
extern AudioAnalyzePeak braids_peak_r;
extern AudioAnalyzePeak drum_mixer_peak_l;
extern AudioAnalyzePeak drum_mixer_peak_r;
extern AudioAnalyzePeak reverb_return_peak_l;
extern AudioAnalyzePeak reverb_return_peak_r;

// #ifdef GRANULAR
// extern AudioMixer<11> reverb_mixer_r;
// extern AudioMixer<11> reverb_mixer_l;
// #else
// extern AudioMixer<10> reverb_mixer_r;
// extern AudioMixer<10> reverb_mixer_l;
// #endif

extern AudioMixer<REVERB_MIX_CH_MAX> reverb_mixer_r;
extern AudioMixer<REVERB_MIX_CH_MAX> reverb_mixer_l;

extern microsynth_t microsynth[NUM_MICROSYNTH];

extern braids_t braids_osc;
extern AudioSynthBraids* synthBraids[NUM_BRAIDS];
extern AudioMixer<NUM_BRAIDS> braids_mixer;
extern AudioMixer<2> braids_mixer_reverb;

extern AudioEffectPlateReverb reverb;

extern AudioEffectStereoPanorama ep_stereo_panorama;
extern AudioSynthWaveform ep_chorus_modulator;
extern AudioMixer<2> ep_chorus_mixer_r;
extern AudioMixer<2> ep_chorus_mixer_l;
extern AudioMixer<MASTER_MIX_CH_MAX> master_mixer_r;
extern AudioMixer<MASTER_MIX_CH_MAX> master_mixer_l;

extern AudioEffectStereoMono stereo2mono;
extern AudioAnalyzePeak master_peak_r;
extern AudioAnalyzePeak master_peak_l;

extern char sd_string[display_cols + 1];
extern char g_voice_name[NUM_DEXED][VOICE_NAME_LEN];
extern char g_bank_name[NUM_DEXED][BANK_NAME_LEN];
extern char tmp_voice_name[VOICE_NAME_LEN];
extern char tmp_bank_name[BANK_NAME_LEN];
extern const float midi_ticks_factor[10];
extern uint8_t midi_bpm;
extern elapsedMillis save_sys;
extern bool save_sys_flag;
extern sdcard_t sdcard_infos;
extern ts_t ts;

//void UI_func_map_gamepad(uint8_t param);
void UI_func_test_mute(uint8_t param);
void UI_func_test_psram(uint8_t param);
void UI_func_favorites(uint8_t param);
void UI_func_startup_page(uint8_t param);
void UI_func_sysex_send_bank(uint8_t param);
void UI_func_startup_performance(uint8_t param);
void UI_func_sysex_send_voice(uint8_t param);
void UI_func_save_voice(uint8_t param);
void UI_func_set_voice_name(uint8_t param);
void UI_func_midi_soft_thru(uint8_t param);
void UI_func_smart_filter(uint8_t param);
void UI_func_seq_probabilities(uint8_t param);
void UI_func_set_multisample_name(uint8_t param);
void UI_func_clear_song(uint8_t param);
void UI_func_clear_song_chains(uint8_t param);
void UI_func_clear_patterns(uint8_t param);
void UI_func_clear_all(uint8_t param);
void UI_func_stereo_mono(uint8_t param);
void UI_handle_OP(uint8_t param);
void UI_func_set_braids_name(uint8_t param);
void tracker_print_pattern(int xpos, int ypos, uint8_t track_number);
void print_sample_type(bool dir);
void draw_euclidean_circle();

//#include "D50Synth.h"

// extern struct d50_config_s d50;
// extern D50Synth d50Synth;

#ifdef SECOND_SD
extern SDClass SD_EXTERNAL;
#endif

FLASHMEM void flush_sysex()
{
  if (remote_active)
    display.flushSysEx();
}

/***********************************************************************
   GLOBAL
************************************************************************/
elapsedMillis back_from_volume;
extern PROGMEM const char accepted_chars[] = " _ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-abcdefghijklmnopqrstuvwxyz";

int temp_int;

boolean COND_hide() // hide a menu element
{
  return false;
}

const bool isCustomSample(const uint8_t index) {
  return ((index >= NUM_STATIC_PITCHED_SAMPLES) && (index < NUM_STATIC_PITCHED_SAMPLES + NUM_CUSTOM_SAMPLES));
}

PROGMEM const char cc_names[MAX_CC_DEST][13] = {
    "Volume      ",
    "Panorama    ",
    "Cursor RIGHT",
    "Cursor LEFT ",
    "Cursor UP   ",
    "Cursor DOWN ",
    "SELECT      ",
    //  "START",
    "BUTTON B    ",
    "BUTTON A    ",
    "Bank Select ",
    "Reverb Send ",
    "Seq. Start  ",
    "Seq. Stop   ",
    "Seq. RECORD ",
    "Panic Dexed ",
};

PROGMEM const char cc_names_UI_mapping[8][13] = {

    "Cursor RIGHT",
    "Cursor LEFT ",
    "Cursor UP   ",
    "Cursor DOWN ",
    "SELECT      ",
    "START",
    "BUTTON B    ",
    "BUTTON A    ",
};


PROGMEM const uint8_t special_chars[24][8] = {
    {B11111111, B11110111, B11100111, B11110111, B11110111, B11110111, B11110111, B11111111}, //  [0] 1 small invers
    {B11111111, B11110111, B11101011, B11111011, B11110111, B11101111, B11100011, B11111111}, //  [1] 2 small invers
    {B11111, B11011, B10011, B11011, B11011, B11011, B11011, B11111},                         //  [2] 1 OP invers
    {B11111, B11011, B10101, B11101, B11011, B10111, B10001, B11111},                         //  [3] 2 OP invers
    {B11111, B10001, B11101, B11011, B11101, B10101, B11011, B11111},                         //  [4] 3 OP invers
    {B11111, B10111, B10111, B10101, B10001, B11101, B11101, B11111},                         //  [5] 4 OP invers
    {B11111, B10001, B10111, B10011, B11101, B11101, B10011, B11111},                         //  [6] 5 OP invers
    {B11111, B11001, B10111, B10011, B10101, B10101, B11011, B11111},                         //  [7] 6 OP invers
    {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B11111},                         //  [8] Level 1
    {B00000, B00000, B00000, B00000, B00000, B00000, B11111, B11111},                         //  [9] Level 2
    {B00000, B00000, B00000, B00000, B00000, B11111, B11111, B11111},                         // [10] Level 3
    {B00000, B00000, B00000, B00000, B11111, B11111, B11111, B11111},                         // [11] Level 4
    {B00000, B00000, B00000, B11111, B11111, B11111, B11111, B11111},                         // [12] Level 5
    {B00000, B00000, B11111, B11111, B11111, B11111, B11111, B11111},                         // [13] Level 6
    {B00000, B11111, B11111, B11111, B11111, B11111, B11111, B11111},                         // [14] Level 7
    {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111},                         // [15] Level 8
    {B00100, B00110, B00101, B00101, B01101, B11101, B11100, B11000},                         // [16] Note
    {B01110, B10001, B10001, B01110, B00100, B00100, B00110, B00110},                         // [17] Disabled 2nd instance symbol
    {B11111111, B11000011, B11011111, B11000011, B11011111, B11011111, B11011111, B11111111}, // [18] Favorites Icon
    {B01000000, B01100000, B01110000, B01111000, B01110000, B01100000, B01000000, B00000000}, // [19] Play Symbol
    {B00000000, B01111000, B11111100, B11111100, B11111100, B11111100, B01111000, B00000000}, // [20] Record Symbol
    {B00000000, B00000000, B01111100, B01111100, B01111100, B01111100, B01111100, B00000000}, // [21] Stop Symbol
    {B11111111, B11000011, B11011011, B11000011, B11011111, B11011111, B11011111, B11111111}, // [22] Pitched Sample
    {B00000000, B00000000, B00000000, B01110000, B11111111, B11111111, B11111111, B11111111}  // [23] File Folder
};

enum {
  MENU_VOICE_BANK = 2,
  MENU_VOICE_SOUND = 3
};

void selectSample(uint8_t sample);
uint8_t findSmartFilteredSample(uint8_t sample, int8_t direction);
void drawSampleWaveform(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t* data, uint32_t length, uint8_t numChannels, uint16_t fromX, uint16_t toX, uint16_t waveColor = COLOR_PITCHSMP, uint16_t bgColor = GREY3);

void lcdml_menu_display(void);
void lcdml_menu_clear(void);
void lcdml_menu_control(void);
void mFunc_screensaver(uint8_t param);
void set_delay_sync(uint8_t sync, uint8_t instance);
void master_effects_set_delay_time(uint8_t instance, int8_t change);
void master_effects_set_delay_feedback(uint8_t instance, int8_t change);
void master_effects_set_delay_panorama(uint8_t instance, int8_t change);
void master_effects_set_reverb_send(uint8_t instance, int8_t change);

void UI_update_instance_icons();
bool UI_select_name(uint8_t y, uint8_t x, uint8_t len, bool init);
uint8_t search_accepted_char(uint8_t c);
void display_int(int16_t var, uint8_t size, bool zeros, bool brackets, bool sign);
void display_float(float var, uint8_t size_number, uint8_t size_fraction, bool zeros, bool brackets, bool sign);
void display_bar_int(const char* title, uint32_t value, float factor, int32_t min_value, int32_t max_value, uint8_t size, bool zeros, bool sign, bool init);
void display_bar_float(const char* title, float value, float factor, int32_t min_value, int32_t max_value, uint8_t size_number, uint8_t size_fraction, bool zeros, bool sign, bool init);
void display_meter_int(const char* title, uint32_t value, float factor, float offset, int32_t min_value, int32_t max_value, uint8_t size, bool zeros, bool sign, bool init);
void display_meter_float(const char* title, float value, float factor, float offset, int32_t min_value, int32_t max_value, uint8_t size_number, uint8_t size_fraction, bool zeros, bool sign, bool init);
void string_trim(char* s);
void draw_favorite_icon(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id);
bool check_favorite(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id);
bool quick_check_favorites_in_bank(uint8_t p, uint8_t b, uint8_t instance_id);
void locate_previous_non_favorite();
void locate_previous_favorite();
void locate_next_favorite();
void locate_next_non_favorite();
void locate_random_non_favorite();
void splash_screen1();
void splash_screen2();
void UI_draw_FM_algorithm(uint8_t algo, uint8_t x, uint8_t y);
void displayOp(char id, int x, int y, char link, char fb);
void fill_msz(char filename[], const uint8_t preset_number, const uint8_t zone_number, const uint8_t psram_entry);
void _setup_rotation_and_encoders(bool init);
void sd_go_parent_folder();
void sd_update_display();
void sd_card_count_files_from_directory(const char* path);
char* basename(const char* filename);
bool load_performance_and_check_midi(uint8_t perf);
void clear_volmeter(int x, int y);

uint8_t x_pos_menu_header_layer[8];
uint8_t last_menu_depth = 99;
uint8_t prev_menu_item = 0;    // avoid screen flicker at start / end of menu items

ScopeSettings currentScopeSettings;

bool remote_console_keystate_select;
bool remote_console_keystate_a;
bool remote_console_keystate_b;

// normal menu
LCDMenuLib2_menu LCDML_0(255, 0, 0, NULL, NULL); // normal root menu element (do not change)
LCDMenuLib2 LCDML(LCDML_0, _LCDML_DISP_rows, _LCDML_DISP_cols, lcdml_menu_display, lcdml_menu_clear, lcdml_menu_control);

// the id numbers MUST be kept in incrementing sequence.
// when menu options are added or removed, they must be renumbered!
// To do this automatically, paste the code below beginning with the second line
//  into the following command:
// cat -n | sed -E 's/([0-9]+)([^0-9]+)([0-9]+)(.*+)/\2\1\4/'

LCDML_add(0, LCDML_0, 1, "Dexed", NULL);
LCDML_add(1, LCDML_0_1, 1, "Select Voice", UI_func_voice_select);
LCDML_add(2, LCDML_0_1, 2, "Edit Voice", UI_func_voice_editor);
LCDML_add(3, LCDML_0_1, 3, "Audio", UI_func_dexed_audio);
LCDML_add(4, LCDML_0_1, 4, "Controller", UI_func_dexed_controllers);
LCDML_add(5, LCDML_0_1, 5, "Setup", UI_func_dexed_setup);
LCDML_add(6, LCDML_0_1, 6, "Operator", UI_handle_OP);
LCDML_add(7, LCDML_0_1, 7, "Save Voice", UI_func_save_voice);
LCDML_add(8, LCDML_0_1, 8, "Name Voice", UI_func_set_voice_name);
LCDML_add(9, LCDML_0, 2, "E-Piano", UI_func_epiano);
LCDML_add(10, LCDML_0, 3, "MicroSynth", UI_func_microsynth);
LCDML_add(11, LCDML_0, 4, "Braids", UI_func_braids);
LCDML_add(12, LCDML_0, 5, "Granular Synth", UI_func_granular);
LCDML_add(13, LCDML_0, 6, "MultiSamplePlayer", UI_func_MultiSamplePlay);
LCDML_add(14, LCDML_0, 7, "Drums", UI_func_drums);
LCDML_add(15, LCDML_0, 8, "NoiseMaker", UI_func_noisemaker);
LCDML_add(16, LCDML_0, 9, "Master Effects", NULL);
LCDML_add(17, LCDML_0_9, 1, "Delay+Reverb", UI_func_master_effects);
LCDML_add(18, LCDML_0_9, 2, "SideChain Comp.", UI_func_sidechain);
LCDML_add(19, LCDML_0_9, 3, "Multiband Comp.", UI_func_multiband_dynamics);
LCDML_add(20, LCDML_0, 10, "Sequencer", NULL);
LCDML_add(21, LCDML_0_10, 1, "Song", UI_func_song);
LCDML_add(22, LCDML_0_10, 2, "Pattern Editor", UI_func_seq_pattern_editor);
LCDML_add(23, LCDML_0_10, 3, "Vel./Chrd Edit", UI_func_seq_vel_editor);
LCDML_add(24, LCDML_0_10, 4, "Live Sequencer", UI_func_livesequencer);
LCDML_add(25, LCDML_0_10, 5, "Live Seq. Editor", UI_func_liveseq_graphic);
LCDML_add(26, LCDML_0_10, 6, "Arpeggio", UI_func_arpeggio);
LCDML_add(27, LCDML_0_10, 7, "Mute Matrix", UI_func_seq_mute_matrix);
LCDML_add(28, LCDML_0_10, 8, "Tracker", UI_func_seq_tracker);
LCDML_add(29, LCDML_0_10, 9, "Probabilities", UI_func_seq_probabilities);
LCDML_add(30, LCDML_0_10, 10, "Smart Filter", UI_func_smart_filter);
LCDML_add(31, LCDML_0_10, 11, "Clear ALL", UI_func_clear_all);
LCDML_add(32, LCDML_0_10, 12, "Clear Song", UI_func_clear_song);
LCDML_add(33, LCDML_0_10, 13, "Clear Chains", UI_func_clear_song_chains);
LCDML_add(34, LCDML_0_10, 14, "Clear Patterns", UI_func_clear_patterns);
LCDML_add(35, LCDML_0_10, 15, "Settings", UI_func_seq_settings);
LCDML_add(36, LCDML_0, 11, "Mixer", UI_func_mixer);
LCDML_add(37, LCDML_0, 12, "Slice Editor", UI_func_slice_editor);
LCDML_add(38, LCDML_0, 13, "Audio/SMP Record", UI_func_recorder);
LCDML_add(39, LCDML_0, 14, "SMF/MIDI Player", UI_func_midiplayer);
LCDML_add(40, LCDML_0, 15, "Load/Save", NULL);
LCDML_add(41, LCDML_0_15, 1, "Load Perf.", UI_func_load_performance);
LCDML_add(42, LCDML_0_15, 2, "Save Perf.", UI_func_save_performance);
LCDML_add(43, LCDML_0_15, 3, "Name Perf.", UI_func_set_performance_name);
LCDML_add(44, LCDML_0_15, 4, "Name Voice", UI_func_set_voice_name);
LCDML_add(45, LCDML_0_15, 5, "Name Multisample", UI_func_set_multisample_name);
LCDML_add(46, LCDML_0_15, 6, "Name Braids", UI_func_set_braids_name);
LCDML_add(47, LCDML_0_15, 7, "MIDI", NULL);
LCDML_add(48, LCDML_0_15_7, 1, "MIDI Recv Bank", UI_func_sysex_receive_bank);
LCDML_add(49, LCDML_0_15_7, 2, "MIDI Send Bank", UI_func_sysex_send_bank);
LCDML_add(50, LCDML_0_15_7, 3, "MIDI Send Voice", UI_func_sysex_send_voice);
LCDML_add(51, LCDML_0, 16, "File Manager", UI_func_file_manager);
LCDML_add(52, LCDML_0, 17, "System", NULL);
LCDML_add(53, LCDML_0_17, 1, "Stereo/Mono", UI_func_stereo_mono);
LCDML_add(54, LCDML_0_17, 2, "MIDI channels", UI_func_midi_channels);
LCDML_add(55, LCDML_0_17, 3, "MIDI Soft THRU", UI_func_midi_soft_thru);
LCDML_add(56, LCDML_0_17, 4, "MIDI Mapping", UI_func_custom_mappings);
LCDML_add(57, LCDML_0_17, 5, "Favorites", UI_func_favorites);
LCDML_add(58, LCDML_0_17, 6, "Startup Perform.", UI_func_startup_performance);
LCDML_add(59, LCDML_0_17, 7, "Startup Page", UI_func_startup_page);
LCDML_add(60, LCDML_0_17, 8, "System Settings", UI_func_system_settings);
LCDML_add(61, LCDML_0, 18, "Extras/Test", NULL);
LCDML_add(62, LCDML_0_18, 1, "MIDI/CV Translate", UI_func_cv_page);
LCDML_add(63, LCDML_0_18, 2, "Chord Arranger", UI_func_chord_arranger);
LCDML_add(64, LCDML_0_18, 3, "Test Touchscreen", UI_func_test_touchscreen);
LCDML_add(65, LCDML_0_18, 4, "Test Audio Mute", UI_func_test_mute);
LCDML_add(66, LCDML_0_18, 5, "Test PSRAM", UI_func_test_psram);


//LCDML_add(67, LCDML_0, 19, "D50", UI_func_d50_synth);

#if defined BITCRUSH
LCDML_add(67, LCDML_0, 19, "Bitcrusher Test", UI_func_bitcrush);
LCDML_add(68, LCDML_0, 20, "Info", UI_func_information);
LCDML_addAdvanced(69, LCDML_0, 21, COND_hide, "Volume", UI_func_volume, 0, _LCDML_TYPE_default);
LCDML_addAdvanced(70, LCDML_0, 22, COND_hide, "Screensaver", mFunc_screensaver, 0, _LCDML_TYPE_default);
LCDML_addAdvanced(71, LCDML_0, 23, COND_hide, "SD_NOT_FOUND", UI_func_sd_content_not_found, 0, _LCDML_TYPE_default);
#else
LCDML_add(67, LCDML_0, 19, "Info", UI_func_information);
LCDML_addAdvanced(68, LCDML_0, 20, COND_hide, "Volume", UI_func_volume, 0, _LCDML_TYPE_default);
LCDML_addAdvanced(69, LCDML_0, 21, COND_hide, "Screensaver", mFunc_screensaver, 0, _LCDML_TYPE_default);
LCDML_addAdvanced(70, LCDML_0, 22, COND_hide, "SD_NOT_FOUND", UI_func_sd_content_not_found, 0, _LCDML_TYPE_default);
#endif



//old/depreciated menus
//LCDML_add(64, LCDML_0_18, 3, "Map Gamepad", UI_func_map_gamepad);


FLASHMEM void registerScope(uint16_t x, uint16_t y, uint16_t w, uint16_t h = 65, bool onlyDrawWhenRunning = false) {
  currentScopeSettings = {
    .enabled = true,
    .x = x,
    .y = y,
    .w = w,
    .h = h,
    .onlyDrawWhenRunning = onlyDrawWhenRunning
  };
}

extern const char* midiToNote(uint8_t midiNote);

FLASHMEM ScopeSettings& getCurrentScopeSettings(void) {
  return currentScopeSettings;
}

FLASHMEM void unregisterScope(void) {
  currentScopeSettings = {
    .enabled = false
  };
}


#if defined (RGB_ENCODERS) && defined (MCP_23008) || defined (RGB_ENCODERS) && defined (MCP_23017)
//#include "MCP23008.h"
//#include "MCP23017.h"
extern void setled(uint8_t pin, bool state);
#endif

FLASHMEM uint16_t getContentTypeColor(uint8_t contentType) {
  switch (contentType) {
  case 0:
    return COLOR_DRUMS;
  case 1:
    return COLOR_INSTR;
  case 2:
    return COLOR_CHORDS;
  case 3:
    return COLOR_ARP;
  case 4:
    return COLOR_PITCHSMP;
  default:
    return COLOR_SYSTEXT; // fallback
  }
}

FLASHMEM void clear_bottom_half_screen_without_backbutton()
{
  display.console = true;
  display.fillRect(270, 88, 50, 35, COLOR_BACKGROUND);
  display.fillRect(0, 127, 320, 35 + 43, COLOR_BACKGROUND);
  display.fillRect(50, 205, 320 - 50, 35, COLOR_BACKGROUND);
  display.console = false;
}

int favsearcher = 0;

FLASHMEM void update_seq_speed() {
  seq.tempo_ms = 60000000 / seq.bpm / 4;

  liveSeq.checkBpmChanged();

  if (seq.clock == 0) // INTERNAL TIMING
    seq.ticks_max = 7; //(0-7 = 8)
  else
    if (seq.clock == 2) // MIDI MASTER
      seq.ticks_max = 5; //(0-5 = 6)

  if (seq.clock == 0 || seq.clock == 2)
  {
    if (seq.running)
      sequencer_timer.begin(sequencer, seq.tempo_ms / (seq.ticks_max + 1));
    else
      sequencer_timer.begin(sequencer, seq.tempo_ms / (seq.ticks_max + 1), false);
  }

  for (uint8_t i = 0; i < 2; i++)
  {
    if (configuration.fx.delay_sync[i] > 0)
    {
      uint16_t midi_sync_delay_time = uint16_t(60000.0 * midi_ticks_factor[configuration.fx.delay_sync[i]] / seq.bpm);
      delay_fx[i]->delay(0, constrain(midi_sync_delay_time, DELAY_TIME_MIN, DELAY_TIME_MAX * 10));
    }
  }
}

FLASHMEM bool menu_item_check(uint8_t in)
{
  const bool redrawRequired = (generic_temp_select_menu == in) || generic_full_draw_required || ((seq.edit_state == false) && ((generic_temp_select_menu == in - 1) || (generic_temp_select_menu == in + 1)));
  return redrawRequired;
}

FLASHMEM void print_empty_spaces(uint8_t spaces, bool clear_background)
{
  if (clear_background)
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  for (uint8_t x = 0; x < spaces; x++)
  {
    display.print(" ");
  }
}

FLASHMEM void print_shortcut_navigator()
{
  if (seq.cycle_touch_element != 1)
  {
    display.setTextSize(1);
    display.setCursor(CHAR_width_small * 36, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.cycle_touch_element < 6)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("S");
    display.setCursor(CHAR_width_small * 37, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.cycle_touch_element > 5 && seq.cycle_touch_element < 7)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("C");
    display.setCursor(CHAR_width_small * 38, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.cycle_touch_element > 7)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("T");

    display.setCursor(CHAR_width_small * 39, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("P");
    display.setCursor(CHAR_width_small * 40, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("V");
    display.setCursor(CHAR_width_small * 41, 28 * (CHAR_height_small));
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_voice_select) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_epiano) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_microsynth) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_braids) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay))
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("I");
  }
}

FLASHMEM void print_song_playhead()
{
  if (seq.running)
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND); // play indicator song view

    if (CHAR_height_small * 8 + 10 * (seq.current_song_step - 1 - seq.scrollpos) > CHAR_height_small * 6)
    {
      display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (seq.current_song_step - 1 - seq.scrollpos));
      display.print(" ");
    }
    if (CHAR_height_small * 8 + 10 * (seq.current_song_step - seq.scrollpos) > CHAR_height_small * 7)
    {
      display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (seq.current_song_step - seq.scrollpos));
      display.print(">");
    }
    if (CHAR_height_small * 8 + 10 * (seq.current_song_step + 1 - seq.scrollpos) > CHAR_height_small * 6)
    {
      display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (seq.current_song_step + 1 - seq.scrollpos));
      display.print(" ");
    }

    if (seq.loop_start == 99) // no loop start set, start at step 0
    {
      if (seq.current_song_step == 0 && get_song_length() > 1) // clear last cursor step after first cycle when song longer than 1 Chainstep
      {
        display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (get_song_length() - seq.scrollpos - 1));
        display.print(" ");
      }
    }
    else
    {
      if (seq.current_song_step == seq.loop_start && seq.loop_start != seq.loop_end) // clear last cursor step after first cycle when song is in loop mode
      {
        display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (seq.loop_end - seq.scrollpos));
        display.print(" ");
      }
      // special case, loop start is higher than loop end
      if (seq.loop_start > seq.loop_end && seq.current_song_step == 0) // clear cursor on loop start since we are cycling to step 0
      {
        display.setCursor(CHAR_width_small * 5, CHAR_height_small * 8 + 10 * (seq.loop_start - seq.scrollpos));
        display.print(" ");
      }
    }
  }
}

FLASHMEM void _print_midi_channel(uint8_t midi_channel)
{
  if (midi_channel == MIDI_CHANNEL_OMNI)
  {
    display.print(F("OMNI"));
  }
  else
    if (midi_channel == MIDI_CHANNEL_OFF)
    {
      display.print(F("OFF "));
    }
    else
    {
      print_formatted_number(midi_channel, 2);
      display.print(F("  "));
    }
}

char midi_channel_name[5];

FLASHMEM const char* get_midi_channel_name(uint8_t midi_channel)
{
  if (midi_channel == MIDI_CHANNEL_OMNI)
    snprintf_P(midi_channel_name, sizeof(midi_channel_name), PSTR("%s"), "OMNI");
  else if (midi_channel == MIDI_CHANNEL_OFF)
    snprintf_P(midi_channel_name, sizeof(midi_channel_name), PSTR("%s"), "OFF ");
  else
    snprintf_P(midi_channel_name, sizeof(midi_channel_name), PSTR("%02d"), midi_channel);
  return midi_channel_name;
}


FLASHMEM void clear_song_playhead()
{
  display.console = true;
  display.fillRect(CHAR_width_small * 5, CHAR_height_small * 8 - 2, 5, 10 * 16, COLOR_BACKGROUND);
}

FLASHMEM void print_song_mode_help()
{
  print_shortcut_navigator();

  if (seq.tracktype_or_instrument_assign == 8) // clear loop
  {
    helptext_l(back_text);
    helptext_r(F("CLEAR LOOP POINTS (NOT DATA) ?"));
  }
  else if (seq.tracktype_or_instrument_assign == 10) // copy loop
  {
    helptext_l(back_text);
    helptext_r(F("COPY LOOP CONTENT TO NEW LOCATION ?"));
  }
  else if (seq.tracktype_or_instrument_assign == 11) // copy loop
  {
    helptext_l(back_text);
    helptext_r(F("SELECT DESTINATION < >"));
  }

  else if (seq.tracktype_or_instrument_assign == 6) // is in tracktype select mode
  {
    helptext_l(back_text);
    helptext_r(F("SELECT WITH < > THEN PUSH TO CONFIRM TRACKTYPE"));
  }
  else if (seq.tracktype_or_instrument_assign == 5) // is in tracktype select mode
  {
    helptext_l(F("TRK TYPE CHANGE"));
    helptext_r(F("CONFIRM TRK"));
  }
  else if (seq.tracktype_or_instrument_assign == 2) // is in inst. select mode
  {
    helptext_l(back_text);
    helptext_r(F("SEL. INSTR. < > PUSH=CONFIRM"));
  }
  else if (seq.tracktype_or_instrument_assign == 1)
  {
    display.fillRect(CHAR_width_small * 26, DISPLAY_HEIGHT - CHAR_height_small, 6 * CHAR_width_small + 2, CHAR_height_small, COLOR_BACKGROUND);
    helptext_l(F("SELECT TRACK< >FOR INSTR"));
    helptext_r(F("CONFIRM TRK"));
  }
  else if (seq.loop_edit_step == 1)
  {
    helptext_r(F("SEL LOOP START"));
    //display.fillRect(CHAR_width_small * 7, DISPLAY_HEIGHT - CHAR_height_small, 30 * CHAR_width_small, CHAR_height_small, COLOR_BACKGROUND);
  }
  else if (seq.tracktype_or_instrument_assign == 0)
  { // all messages below in standard song mode
    if (seq.loop_edit_step == 2)
      helptext_r(F("SET LOOP END"));
    else if (seq.loop_edit_step == 0 && seq.edit_state == false && seq.cycle_touch_element != 0)
      helptext_r(F("MOVE Y"));
    else if (seq.edit_state == false && seq.cycle_touch_element == 0)
    {
      helptext_l(F("MOVE X"));
      helptext_r(F("MOVE Y"));
    }
    else if (seq.edit_state) {
      switch (seq.cycle_touch_element) {
      case 5:
        display.setCursor(7 * CHAR_width_small, DISPLAY_HEIGHT - CHAR_height_small * 1);
        print_empty_spaces(31, 1);
        helptext_l(F("> CHAIN"));
        helptext_r(F("< > SELECT CHAIN"));
        break;

      case 6:
        helptext_l(F("SONG < > TRANSPOSE"));
        display.setCursor(CHAR_width_small * 18, DISPLAY_HEIGHT - CHAR_height_small);
        print_empty_spaces(10, 1);
        helptext_r(F("< > SEL. CH. STEP"));
        break;

      case 7:
        helptext_r(F("< > SEL. PATTERN"));
        break;

      case 8:
        helptext_l(F("CHAIN < > PATTERN EDITOR"));
        helptext_r(F("< > SEL. STEP"));
        break;

      case 9:
        helptext_l("");
        helptext_r(F("< > EDIT STEP"));
        break;
      }
    }
  }
  seq.help_text_needs_refresh = false;
}

FLASHMEM void print_fav_search_text(bool dir)
{
  display.setCursor(11 * CHAR_width_small, 2 * CHAR_height_small + 3);
  display.setTextSize(2);
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  if (dir == LEFT)
    display.print(F("<<SEARCH"));
  else    if (dir == RIGHT)
    display.print("SEARCH>>");

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

}

FLASHMEM void sub_song_print_instruments(uint16_t front, uint16_t back)
{
  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    if (seq.tracktype_or_instrument_assign == 2 && seq.selected_track == x && seq.track_type[x] != 0 && seq.track_type[x] != 4)
      display.setTextColor(COLOR_SYSTEXT, DARKGREEN);
    else if (seq.tracktype_or_instrument_assign == 1 && seq.selected_track == x && seq.track_type[x] != 0 && seq.track_type[x] != 4)
      display.setTextColor(COLOR_BACKGROUND, GREEN);
    else if (seq.tracktype_or_instrument_assign == 1 && seq.selected_track != x && seq.track_type[x] != 0 && seq.track_type[x] != 4)
      display.setTextColor(MIDDLEGREEN, COLOR_BACKGROUND);
    else if ((seq.tracktype_or_instrument_assign == 1 && seq.selected_track == x && seq.track_type[x] == 0) ||
      (seq.tracktype_or_instrument_assign == 1 && seq.selected_track == x && seq.track_type[x] == 4))
      display.setTextColor(COLOR_BACKGROUND, RED);
    else if (seq.tracktype_or_instrument_assign == 0 || seq.tracktype_or_instrument_assign == 5)
      display.setTextColor(front, back);
    else
      display.setTextColor(0x6000, COLOR_BACKGROUND);
    display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
    if (seq.track_type[x] != 0 && seq.track_type[x] != 4)
    {
      if (seq.instrument[x] == 0)
        display.print(F("DX1"));
      else if (seq.instrument[x] == 1)
        display.print(F("DX2"));
      else if (seq.instrument[x] == 70)
        display.print(F("DX3"));
      else if (seq.instrument[x] == 71)
        display.print(F("DX4"));
      else if (seq.instrument[x] == 72)
        display.print(F("GRA"));
      else if (seq.instrument[x] == 2)
        display.print(F("EP "));
      else if (seq.instrument[x] == 3)
        display.print(F("MS1"));
      else if (seq.instrument[x] == 4)
        display.print(F("MS2"));
      else if (seq.instrument[x] == 5)
        display.print(F("BRD"));
      else if (seq.instrument[x] > 5 && seq.instrument[x] < 16)
      {
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
        display.print(F("MSP"));
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
        display.print(F("#"));
        print_formatted_number(seq.instrument[x] - 6, 2);
      }
      else if (seq.instrument[x] > 15 && seq.instrument[x] < 32)
      {
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
        display.print(F("USB"));
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
        display.print(F("#"));
        print_formatted_number(seq.instrument[x] - 15, 2);
      }
      else if (seq.instrument[x] > 31 && seq.instrument[x] < 48)
      {
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
        display.print(F("DIN"));
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
        display.print(F("#"));
        print_formatted_number(seq.instrument[x] - 31, 2);
      }
      else if (seq.instrument[x] > 47 && seq.instrument[x] < 64)
      {
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
        display.print(F("INT"));
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
        display.print(F("#"));
        print_formatted_number(seq.instrument[x] - 47, 2);
      }
      else if (seq.instrument[x] == 64)
      {
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
        display.print(F("EXT"));
        display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 6);
        display.print(F("CV1"));
      }
      else
        display.print(F("???"));
    }
    else
    {
      if (seq.tracktype_or_instrument_assign == 0)
        display.print(F("   "));
      else  if (seq.track_type[x] == 0)
        display.print(F("DRM"));
      else if (seq.track_type[x] == 4)
        display.print(F("SLC"));
    }
  }
}

FLASHMEM void check_remote()
{
  if (remote_active)
    display.console = true;
}

FLASHMEM void draw_button_on_grid(uint8_t x, uint8_t y, const char* t1, const char* t2, uint8_t color)
{
  check_remote();
  if (color == 99) // special case, draw virtual keyboard button (icon)
  {
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y / 2 - 2, GREY2);
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small + CHAR_height_small * button_size_y / 2 - 2, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y / 2 + 2, COLOR_BACKGROUND);
    uint8_t offset[5] = { 1, 2, 2, 4, 6 }; //+ is the offset to left
    int offcount = 0;
    display.setTextSize(1);
    display.setTextColor(GREY1, GREY2);
    display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 4);
    display.print(F("V.KEYB"));
    // draw white keys
    for (uint8_t i = 0; i < 7; i++)
    {
      display.console = true;
      display.fillRect(x * CHAR_width_small + 6 * i, y * CHAR_height_small + 16, 5, 16, COLOR_SYSTEXT); // pianoroll white key
    }
    for (uint8_t i = 0; i < 11; i++)
    {
      if (seq.piano[i] == 1)
      {
        display.fillRect(x * CHAR_width_small + 4 * i - offset[offcount], y * CHAR_height_small + 16, 4, 8, COLOR_BACKGROUND); // BLACK key
        offcount++;
        if (offcount == 5)
          offcount = 0;
      }
    }
  }
  else if (color == 98) // special case, clear button to background color
  {
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, COLOR_BACKGROUND);
  }
  else if (color == 97) // special case, no touch button but text only, aligned / matching the button grid (used in live sequencer)
  {
    display.setTextColor(COLOR_SYSTEXT, GREY3);
    display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 6);
    display.print(t1);
    display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 10 + CHAR_height_small);
    display.print(t2);
    display.setTextSize(1);
  }
  else
  {
    display.setTextSize(1);
    if (color == 0) // standard grey
    {
      display.setTextColor(GREY1, GREY2);
      display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, GREY2);
    }
    else if (color == 1) // button has active color
    {
      display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
      display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, DX_DARKCYAN);
    }
    else if (color == 2) // button has RED color
    {
      display.setTextColor(COLOR_SYSTEXT, RED);
      display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, RED);
    }
    else if (color == 3) // button has highlighted color
    {
      display.setTextColor(COLOR_SYSTEXT, MIDDLEGREEN);
      display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, MIDDLEGREEN);
    }
    display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 6);
    display.print(t1);
    if (t2[1] == '\0' && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_mute_matrix))// big numbers for mute matrix
    {
      display.setCursor((x + 2) * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 6 + CHAR_height_small);
      display.setTextSize(2);
    }
    else if (t2[0] >= '1' && t2[0] < '9' && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_mute_matrix))  // big numbers for mute matrix
    {
      display.setCursor((x + 2) * CHAR_width_small - 4, y * CHAR_height_small + 6 + CHAR_height_small);
      display.setTextSize(2);
    }
    else
    {
      display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 10 + CHAR_height_small);
    }
    display.print(t2);
    display.setTextSize(1);
    // display.setTextColor(COLOR_SYSTEXT,COLOR_BACKGROUND);
  }
}

#ifdef TOUCH_UI
FLASHMEM void draw_button_on_grid_smarttext(uint8_t x, uint8_t y, const char* t1, uint8_t color)
{

  uint8_t c = 0;
  uint8_t len = strlen(t1);
  char out1[8] = { 0,0,0,0,0,0,0 };
  char out2[8] = { 0,0,0,0,0,0,0 };
  uint8_t c_out1 = 0;
  uint8_t c_out2 = 0;
  uint8_t line = 1;

  if (color == 98) // special case, clear button to background color
  {
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, GREY4);
  }
  else do {

    if (t1[c] == 32 && line == 1) // space triggers new line
    {
      line = 2;
      c++;
    }
    else
      if (c_out1 > 5) // if line 1 full, continue on line 2
      {
        line = 2;
      }

    if (line == 1)
    {
      out1[c_out1] = t1[c];
      c_out1++;
    }

    else if (line == 2)
    {
      out2[c_out2] = t1[c];
      c_out2++;
    }

    c++;

  } while (c < len && c_out2 < 6);

  if (remote_active)
    display.console = true;

  display.setTextSize(1);
  if (color == 0) // standard grey
  {
    display.setTextColor(GREY1, GREY2);
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, GREY2);
  }
  else if (color == 1) // button has active color
  {
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, DX_DARKCYAN);
  }
  else if (color == 2) // button has RED color
  {
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.fillRect(x * CHAR_width_small, y * CHAR_height_small, button_size_x * CHAR_width_small, CHAR_height_small * button_size_y, RED);
  }
  display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 6);
  display.print(out1);
  display.setCursor(x * CHAR_width_small + CHAR_width_small / 2, y * CHAR_height_small + 10 + CHAR_height_small);
  display.print(out2);
  display.setTextSize(1);
}
#endif


FLASHMEM void show_smallfont_noGrid(int pos_y, int pos_x, uint8_t field_size, const char* str)
{
  display.setTextSize(1);
  char tmp[STRING_BUFFER_SIZE];
  char* s = tmp;
  uint8_t l = strlen(str);
  memset(tmp, 0, sizeof(tmp));
  memset(tmp, 0x20, field_size); // blank
  if (l > field_size)
    l = field_size;
  strncpy(s, str, l);
  display.setCursor(pos_x, pos_y);
  display.print(tmp);
}

FLASHMEM void setCursor_textGrid(uint8_t pos_x, uint8_t pos_y)
{
  display.setCursor(pos_x * CHAR_width, pos_y * CHAR_height);
}

FLASHMEM void setCursor_textGrid_small(uint8_t pos_x, uint8_t pos_y)
{
  display.setCursor(pos_x * 6, pos_y * 10);
}

FLASHMEM void setCursor_textGrid_large(uint8_t pos_x, uint8_t pos_y)
{
  display.setCursor(pos_x * CHAR_width, pos_y * (CHAR_height + 1));
}

FLASHMEM void show(uint8_t pos_y, uint8_t pos_x, uint8_t field_size, const char* str)
{
  char tmp[STRING_BUFFER_SIZE];
  char* s = tmp;
  uint8_t l = strlen(str);
  memset(tmp, 0, sizeof(tmp));
  memset(tmp, 0x20, field_size); // blank
  if (l > field_size)
    l = field_size;
  strncpy(s, str, l);
  display.setCursor(pos_x * CHAR_width, pos_y * CHAR_height);
  display.print(tmp);
}

FLASHMEM void show_uppercase_no_grid(uint8_t pos_y, uint8_t pos_x, uint8_t field_size, const char* str)
{
  char tmp[STRING_BUFFER_SIZE];
  char* s = tmp;
  uint8_t l = strlen(str);
  memset(tmp, 0, sizeof(tmp));
  memset(tmp, 0x20, field_size); // blank
  if (l > field_size)
    l = field_size;
  strncpy(s, str, l);
  for (uint8_t i = 0; i < l; ++i)
    tmp[i] = toupper(tmp[i]);
  display.setCursor(pos_x, pos_y);
  display.print(tmp);
}

FLASHMEM void show_no_grid(int pos_y, int pos_x, uint8_t field_size, const char* str)
{
  char tmp[STRING_BUFFER_SIZE];
  char* s = tmp;
  uint8_t l = strlen(str);
  memset(tmp, 0, sizeof(tmp));
  memset(tmp, 0x20, field_size); // blank
  if (l > field_size)
    l = field_size;
  strncpy(s, str, l);
  display.setCursor(pos_x, pos_y);
  display.print(tmp);
}

#if defined GLOW
extern void set_glow_show_text_no_grid(uint16_t x, uint8_t y, uint8_t field_size, const char* str, uint8_t menuitem, uint8_t textsize);
extern void remove_glow(void);
#endif

FLASHMEM void show_no_grid(int pos_y, int pos_x, uint8_t field_size, const char* str, uint8_t menuitem, uint8_t textsize)
{
#if defined GLOW
  if (seq.edit_state == 0)
  {
    set_glow_show_text_no_grid(pos_x, pos_y, field_size, str, menuitem, textsize);
  }
#endif
  char tmp[STRING_BUFFER_SIZE];
  char* s = tmp;
  uint8_t l = strlen(str);
  memset(tmp, 0, sizeof(tmp));
  memset(tmp, 0x20, field_size); // blank
  if (l > field_size)
    l = field_size;
  strncpy(s, str, l);
  display.setCursor(pos_x, pos_y);
  display.print(tmp);
}


FLASHMEM void show(uint8_t pos_y, uint8_t pos_x, uint8_t field_size, long num)
{
  char _buf10[STRING_BUFFER_SIZE];
  show(pos_y, pos_x, field_size, itoa(num, _buf10, 10));
}
FLASHMEM void show_no_grid(uint8_t pos_y, uint8_t pos_x, uint8_t field_size, long num)
{
  char _buf10[STRING_BUFFER_SIZE];
  show_no_grid(pos_y, pos_x, field_size, itoa(num, _buf10, 10));
}

FLASHMEM void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
  int16_t w, int16_t h, uint16_t color)
{

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  display.console = true;
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (byte & 0x80)
        display.drawPixel(x + i, y, color);
    }
  }
}

FLASHMEM void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
  int16_t w, int16_t h, uint16_t color,
  uint16_t bg)
{
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  display.console = true;
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      display.drawPixel(x + i, y, (byte & 0x80) ? color : bg);
    }
  }
}

FLASHMEM void drawBitmap(int16_t x, int16_t y, uint8_t* bitmap, int16_t w,
  int16_t h, uint16_t color)
{

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  display.console = true;
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];
      if (byte & 0x80)
        display.drawPixel(x + i, y, color);
    }
  }
}

FLASHMEM void drawBitmap(int16_t x, int16_t y, uint8_t* bitmap, int16_t w,
  int16_t h, uint16_t color, uint16_t bg)
{

  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  display.console = true;
  for (int16_t j = 0; j < h; j++, y++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (i & 7)
        byte <<= 1;
      else
        byte = bitmap[j * byteWidth + i / 8];
      display.drawPixel(x + i, y, (byte & 0x80) ? color : bg);
    }
  }
}

FLASHMEM void drawScrollbar(uint16_t x, uint16_t y, uint8_t sbNbLines, uint8_t nbTotalItems, uint8_t currentItem, uint8_t sbLineHeight)
{
  uint8_t n_max = (nbTotalItems >= sbNbLines) ? sbNbLines : nbTotalItems;
  float sbHeight = sbLineHeight * n_max;
  float sbItemSize = sbHeight / nbTotalItems;
  display.console = true;
  if (sbLineHeight > 10)
  {
    // big font
    display.fillRect(x, y, CHAR_width_small, sbHeight - 1, GREY2);
    display.fillRect(x, y + currentItem * sbItemSize, CHAR_width_small, sbItemSize, COLOR_SYSTEXT);
  }
  else
  {
    display.fillRect(x, y, CHAR_width_small, sbHeight - 2, GREY2);
    int8_t posOffset = 0;
    if (currentItem == nbTotalItems - 1)
      posOffset = -1;
    //    if (currentItem > 0 && currentItem < nbTotalItems -1)
    //      posOffset = -1;
    display.fillRect(x, y + currentItem * sbItemSize + posOffset, CHAR_width_small, sbItemSize, COLOR_SYSTEXT);
  }
}

// create menu
LCDML_createMenu(_LCDML_DISP_cnt);

/***********************************************************************
   CONTROL
 ***********************************************************************/
class EncoderDirection
{
public:
  EncoderDirection(void)
  {
    reset();
  }

  void reset(void)
  {
    button_short = false;
    button_long = false;
    button_pressed = false;
    left = false;
    right = false;
    up = false;
    down = false;
  }

  void ButtonShort(bool state)
  {
    button_short = state;
  }

  bool ButtonShort(void)
  {
    if (button_short == true)
    {
      button_short = false;
      return (true);
    }
    return (false);
  }

  void ButtonLong(bool state)
  {
    button_long = state;
  }

  bool ButtonLong(void)
  {
    if (button_long == true)
    {
      button_long = false;
      return (true);
    }
    return (false);
  }

  void ButtonPressed(bool state)
  {
    button_pressed = state;
  }

  bool ButtonPressed(void)
  {
    return (button_pressed);
  }

  void Left(bool state)
  {
    left = state;
  }

  bool Left(void)
  {
    if (left == true)
    {
      left = false;
      return (true);
    }
    return (false);
  }

  void Right(bool state)
  {
    right = state;
  }

  bool Right(void)
  {
    if (right == true)
    {
      right = false;
      return (true);
    }
    return (false);
  }

  void Up(bool state)
  {
    up = state;
  }

  bool Up(void)
  {
    if (up == true)
    {
      up = false;
      return (true);
    }
    return (false);
  }

  void Down(bool state)
  {
    down = state;
  }

  bool Down(void)
  {
    if (down == true)
    {
      down = false;
      return (true);
    }
    return (false);
  }

private:
  bool button_short;
  bool button_long;
  bool button_pressed;
  bool left;
  bool right;
  bool up;
  bool down;
};

EncoderDirection encoderDir[NUM_ENCODER];
bool button[NUM_ENCODER];

FLASHMEM const EncoderEvents getEncoderEvents(uint8_t id) {
  EncoderEvents result = { 0 };
  if (LCDML.BT_checkDown() && encoderDir[id].Down()) {
    result.down = true;
    result.dir = TurnDir::TURN_DN;
  }
  if (LCDML.BT_checkUp() && encoderDir[id].Up()) {
    result.up = true;
    result.dir = TurnDir::TURN_UP;
  }
  if (result.up || result.down) {
    result.turned = true;
    result.speed = ENCODER[id].speed();
  }
  if (LCDML.BT_checkEnter()) {
    if (encoderDir[id].ButtonShort()) {
      result.pressed = true;
    }
    if (encoderDir[id].ButtonLong()) {
      result.longPressed = true;
    }
  }
  return result;
}

uint32_t g_LCDML_CONTROL_button_press_time[NUM_ENCODER] = { 0, 0 };
bool g_LCDML_CONTROL_button_prev[NUM_ENCODER] = { HIGH, HIGH };

FLASHMEM void fill_up_with_spaces_right_window()
{
  do
  {
    display.print(" ");
  } while (display.getCursorX() < DISPLAY_WIDTH - 8);
}

FLASHMEM void fill_up_with_spaces_left_window()
{
  do
  {
    display.print(" ");
  } while (display.getCursorX() < 36 * CHAR_width_small);
}

FLASHMEM void fill_up_with_spaces_left_window_filemanager()
{
  do
  {
    display.print(" ");
  } while (display.getCursorX() < 27 * CHAR_width_small);
}

#if defined APC
extern void apc_mute_matrix();
extern void check_and_clear_row3();
#endif

FLASHMEM bool legacy_touch_button_back_page() //legacy, 2 line text pages with touch back button
{
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_handle_OP) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_sysex_send_voice) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_save_voice) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_smart_filter) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_load_performance) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_save_performance) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_performance_name) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_multisample_name) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_set_braids_name) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_sysex_receive_bank) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_sysex_send_bank) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_stereo_mono) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_midi_soft_thru) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_favorites) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_startup_performance) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_startup_page))
    return true;
  else
    return false;
}

FLASHMEM bool touch_button_back_page() //modern pages with touch back button
{
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_voice_editor) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_dexed_controllers) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_drums) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_system_settings) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_microsynth) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_chord_arranger) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_noisemaker) ||
    LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_dexed_setup)) {
    return true;
  }
  else {
    return false;
  }
}

FLASHMEM void border1_clear() // upper left
{
  display.console = true;
  display.fillRect(0, CHAR_height, CHAR_width * 19, CHAR_height * display_rows, COLOR_BACKGROUND);
}

FLASHMEM void border3_clear() // lower left
{
  display.console = true;
  display.fillRect(0, CHAR_height * 5 - 4, CHAR_width * 17, DISPLAY_HEIGHT - CHAR_height * 5 + 3, COLOR_BACKGROUND);
}

FLASHMEM void border3_large() // lower left+right as one window
{
  display.console = true;
  display.drawRect(0, CHAR_height * 6 - 4, DISPLAY_WIDTH, DISPLAY_HEIGHT - CHAR_height * 6 + 4, GREY4);
}

FLASHMEM void border3_large_clear() // lower left+right as one window
{
  display.console = true;
  display.fillRect(0, CHAR_height * 5, DISPLAY_WIDTH, DISPLAY_HEIGHT - CHAR_height * 5, COLOR_BACKGROUND);
}

FLASHMEM const char* seq_find_shortname(uint8_t sstep)
{
  const char* shortname;
  bool found = false;
  if (seq.content_type[seq.active_pattern] == 0 && seq.vel[seq.active_pattern][sstep] < 210) // is Drumtrack and not a pitched sample
  {
    for (uint8_t d = 0; d < NUM_DRUMSET_CONFIG - 1; d++)
    {
      if (seq.note_data[seq.active_pattern][sstep] == drum_config[d].midinote && drum_config[d].midinote != 0)
      {
        shortname = drum_config[d].shortname;
        found = true;
        break;
      }
    }
    if (found == false)
      shortname = "-";
  }
  else
  {
    if (seq.note_data[seq.active_pattern][sstep] > 0 && seq.note_data[seq.active_pattern][sstep] != 130)
    {
      shortname = noteNames[seq.note_data[seq.active_pattern][sstep] % 12];
    }
    else if (seq.note_data[seq.active_pattern][sstep] == 130)
      shortname = "~"; // note has tie/latch
    else
      shortname = "-";
  }
  return shortname;
}

FLASHMEM void set_track_type_color(uint8_t track)
{
  display.setTextColor(getContentTypeColor(seq.track_type[track]), COLOR_BACKGROUND);
}

FLASHMEM void sub_song_print_tracktypes()
{
  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 4);
    if (seq.tracktype_or_instrument_assign == 0)
      set_track_type_color(x);
    else if (seq.tracktype_or_instrument_assign == 5 && seq.selected_track == x)
      display.setTextColor(COLOR_BACKGROUND, GREEN);
    else if (seq.tracktype_or_instrument_assign == 5 && seq.selected_track != x)
      display.setTextColor(MIDDLEGREEN, COLOR_BACKGROUND);
    else if (seq.tracktype_or_instrument_assign == 6 && seq.selected_track == x)
      display.setTextColor(COLOR_SYSTEXT, MIDDLEGREEN);
    else
      display.setTextColor(GREY3, COLOR_BACKGROUND);
    display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 5);
    if (seq.track_type[x] == 0)
      display.print(F("DRM"));
    else if (seq.track_type[x] == 1)
      display.print(F("INS"));
    else if (seq.track_type[x] == 2)
      display.print(F("CHD"));
    else if (seq.track_type[x] == 3)
      display.print(F("ARP"));
    else if (seq.track_type[x] == 4)
      display.print(F("SLC"));
    // else if (seq.track_type[x] == 1 && seq.instrument[seq.selected_track] < 16) display.print(F("INS"));
    // else if (seq.track_type[x] == 2 && seq.instrument[seq.selected_track] < 16) display.print(F("CHD"));
    // else if (seq.track_type[x] == 3 && seq.instrument[seq.selected_track] < 16) display.print(F("ARP"));
    // else if (seq.instrument[x] > 15 && seq.instrument[x] < 32) display.print("USB");
    // else if (seq.instrument[x] > 31 && seq.instrument[x] < 48) display.print("DIN");
    else
    {
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("???");
    }
  }
}

FLASHMEM void set_track_type_color_inverted(uint8_t track)
{
  display.setTextColor(COLOR_BACKGROUND, getContentTypeColor(seq.track_type[track]));
}

bool song_page_full_draw_pattern_complete = false;
bool song_page_full_draw_chain_complete = false;
bool song_page_full_draw_transpose_complete = false;
uint8_t temp_int_song_page;

FLASHMEM void print_song_loop_arrows()
{
  uint8_t lineheight = 10;

  display.console = true;
  display.fillRect(0, CHAR_height_small * 8 - 2, 5, lineheight * 16, COLOR_BACKGROUND);
  display.fillRect(3 * CHAR_width_small, CHAR_height_small * 8 - 2, 5, lineheight * 16, COLOR_BACKGROUND);
  for (uint8_t y = 0; y < 16; y++) // print loop arrows
  {
    // if (seq.loop_edit_step != 3)
    // {

    if (seq.tracktype_or_instrument_assign == 8 && y + seq.scrollpos >= seq.loop_start && y + seq.scrollpos <= seq.loop_end)//loop clear mode
      display.setTextColor(COLOR_BACKGROUND, GREEN);  //loop clear mode
    else if (seq.tracktype_or_instrument_assign == 10 && y + seq.scrollpos >= seq.loop_start && y + seq.scrollpos <= seq.loop_end)//loop copy mode
      display.setTextColor(COLOR_SYSTEXT, DARKGREEN);  //loop copy mode 
    else
      display.setTextColor(GREEN, COLOR_BACKGROUND);

    display.setCursor(CHAR_width_small, y * lineheight + CHAR_height_small * 8);
    print_formatted_number(y + 1 + seq.scrollpos, 2);
    // }

    display.setCursor(0, y * lineheight + CHAR_height_small * 8);

    if (seq.tracktype_or_instrument_assign == 8)
      display.setTextColor(RED, COLOR_BACKGROUND); //loop clear mode
    else if (seq.tracktype_or_instrument_assign == 10)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND); //loop copy mode
    else
      display.setTextColor(GREEN, COLOR_BACKGROUND);

    if (y + seq.scrollpos == seq.loop_start)
      display.print(">");
    display.setCursor(3 * CHAR_width_small, y * lineheight + CHAR_height_small * 8);

    if (y + seq.scrollpos == seq.loop_end)
      display.print("<");
  }
}

FLASHMEM void edit_song_loop()
{
  uint8_t lineheight = 10;

  for (uint8_t y = 0; y < 16; y++) // print song step number for song loop edit mode
  {
    if ((seq.loop_edit_step == 1 && y == seq.cursor_scroll) || (seq.loop_edit_step == 2 && y == seq.cursor_scroll))
      display.setTextColor(COLOR_BACKGROUND, GREEN); // select loop start/end step
    else
      display.setTextColor(GREEN, COLOR_BACKGROUND); // green step number

    if (seq.cycle_touch_element == 0)
    {
      display.setCursor(CHAR_width_small, y * lineheight + CHAR_height_small * 8);
      print_formatted_number(y + 1 + seq.scrollpos, 2);
    }

    if ((seq.loop_edit_step == 1 && y == seq.cursor_scroll) || (y + seq.scrollpos == seq.loop_start))
    {
      if (y < 15 && y - 1 >= 0 && y + seq.scrollpos - 1 != seq.loop_start && y - 1 == seq.cursor_scroll - 1)
      {
        display.console = true;
        display.fillRect(0, y * lineheight + CHAR_height_small * 8 - lineheight, 5, 7, COLOR_BACKGROUND);
      }
      if (y < 15 && y + seq.scrollpos + 1 != seq.loop_start && y + 1 == seq.cursor_scroll + 1)
      {
        display.console = true;
        display.fillRect(0, (y + 1) * lineheight + CHAR_height_small * 8, 5, 7, COLOR_BACKGROUND);
      }

      display.setCursor(0, y * lineheight + CHAR_height_small * 8);
      if (y + seq.scrollpos == seq.loop_start)
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      if (seq.loop_edit_step == 1 && y == seq.cursor_scroll)
        display.setTextColor(RED, COLOR_BACKGROUND);
      display.print(">");
    }

    if ((seq.loop_edit_step == 2 && y == seq.cursor_scroll) || (y + seq.scrollpos == seq.loop_end))
    {

      if (y < 15 && y - 1 >= 0 && y + seq.scrollpos - 1 != seq.loop_end && y - 1 == seq.cursor_scroll - 1)
      {
        display.console = true;
        display.fillRect(3 * CHAR_width_small, y * lineheight + CHAR_height_small * 8 - lineheight, 5, 7, COLOR_BACKGROUND);
      }
      if (y < 15 && y + seq.scrollpos + 1 != seq.loop_end && y + 1 == seq.cursor_scroll + 1)
      {
        display.console = true;
        display.fillRect(3 * CHAR_width_small, (y + 1) * lineheight + CHAR_height_small * 8, 5, 7, COLOR_BACKGROUND);
      }

      display.setCursor(3 * CHAR_width_small, y * lineheight + CHAR_height_small * 8);
      if (y + seq.scrollpos == seq.loop_end)
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      if (seq.loop_edit_step == 2 && y == seq.cursor_scroll)
        display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("<");
    }
  }
}

FLASHMEM void print_chain_matrix_in_song_page()
{
  if (seq.tracktype_or_instrument_assign > 0 && seq.tracktype_or_instrument_assign < 3) // select track for instrument assignment
  {
    sub_song_print_instruments(DARKGREEN, COLOR_BACKGROUND);
  }

  else if (seq.tracktype_or_instrument_assign > 4 && seq.tracktype_or_instrument_assign < 7) // select track for tracktype assignment
  {
    sub_song_print_tracktypes();
  }

  if (seq.tracktype_or_instrument_assign < 2 || seq.tracktype_or_instrument_assign == 6 || seq.tracktype_or_instrument_assign == 10 ||
    seq.tracktype_or_instrument_assign == 11 || seq.tracktype_or_instrument_assign == 8) // normal mode: show song grid, chain..
  {
    bool drawtrack[NUM_SEQ_TRACKS] = { false, false, false, false, false, false, false, false };
    if (seq.cursor_scroll == 0 || seq.cursor_scroll == 15 || seq.tracktype_or_instrument_assign == 6 ||
      seq.tracktype_or_instrument_assign == 11)
      song_page_full_draw_pattern_complete = false;

    if (seq.cursor_scroll < 16 && song_page_full_draw_pattern_complete && seq.tracktype_or_instrument_assign != 1)
    {
      if (seq.selected_track == 0)
      {
        drawtrack[7] = true;
        drawtrack[0] = true;
        drawtrack[1] = true;
      }
      else if (seq.selected_track == 7)
      {
        drawtrack[6] = true;
        drawtrack[7] = true;
        drawtrack[0] = true;
      }
      else
      {
        drawtrack[seq.selected_track] = true;
        drawtrack[seq.selected_track + 1] = true;
        drawtrack[seq.selected_track - 1] = true;
      }
    }
    else
    { // draw everything
      drawtrack[0] = true;
      drawtrack[1] = true;
      drawtrack[2] = true;
      drawtrack[3] = true;
      drawtrack[4] = true;
      drawtrack[5] = true;
      drawtrack[6] = true;
      drawtrack[7] = true;

    }

    for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
    {
      for (uint8_t y = 0; y < 16; y++) // visible song steps
      {
        uint8_t lineheight = 10;

        if (seq.cycle_touch_element < 7 || seq.tracktype_or_instrument_assign == 11)
        {
          if (drawtrack[x])
          {
            if ((y == seq.cursor_scroll && x == seq.selected_track) || seq.tracktype_or_instrument_assign == 11 ||
              (y == seq.cursor_scroll && x == 0 && seq.loop_edit_step == 1) || //is in loop edit, change trk0 to default state
              (y == seq.cursor_scroll && x == seq.previous_track) ||
              (y == seq.cursor_scroll_previous && x == seq.selected_track) ||
              seq.cursor_scroll == 15 || // not optimized when scrolling out of initial visible range (first 16 steps)
              (seq.cursor_scroll == 0 && song_page_full_draw_pattern_complete == false) || //not optimized when scrolling screen back
              seq.quicknav_pattern_to_song_jump) //returned from pattern editor to song view
            {
              display.setCursor(6 * CHAR_width_small + (2 * CHAR_width) * x, y * lineheight + CHAR_height_small * 8);

              if (seq.tracktype_or_instrument_assign == 11 && y >= seq.cursor_scroll && y <= seq.cursor_scroll + (seq.loop_end - seq.loop_start))
                display.setTextColor(COLOR_SYSTEXT, RED); //copy mode
              else if ((seq.tracktype_or_instrument_assign == 10 && y + seq.scrollpos >= seq.loop_start && y + seq.scrollpos <= seq.loop_end) ||
                (seq.tracktype_or_instrument_assign == 11 && y + seq.scrollpos >= seq.loop_start && y + seq.scrollpos <= seq.loop_end))

                display.setTextColor(COLOR_SYSTEXT, DARKGREEN); //copy mode

              else if (seq.tracktype_or_instrument_assign == 10 || seq.tracktype_or_instrument_assign == 11) // dim down non copied lines to make more clear what is happening
                display.setTextColor(GREY3, COLOR_BACKGROUND);
              else if (y == seq.cursor_scroll && x == seq.selected_track && seq.edit_state == false && seq.loop_edit_step == 0 && seq.tracktype_or_instrument_assign == 0)
                set_track_type_color_inverted(x);
              else if (y == seq.cursor_scroll && x == seq.selected_track && seq.edit_state && seq.cycle_touch_element == 5 && seq.loop_edit_step == 0)
                display.setTextColor(COLOR_SYSTEXT, RED);
              else if (y == seq.cursor_scroll && x == seq.selected_track && seq.edit_state && seq.cycle_touch_element != 5 && seq.loop_edit_step == 0)
                display.setTextColor(RED, COLOR_BACKGROUND);
              else
                set_track_type_color(x);

              if (seq.edit_state && seq.cycle_touch_element == 5 && x == seq.selected_track && y == seq.cursor_scroll) // is in song edit mode
              {
                if (temp_int_song_page == NUM_CHAINS)
                {
                  temp_int_song_page = 99; // Select empty step/chain
                  seq.song[x][y + seq.scrollpos] = temp_int_song_page;
                  temp_int_song_page = NUM_CHAINS;
                }
                else
                  seq.song[x][y + seq.scrollpos] = temp_int_song_page;
              }

              if (seq.song[x][y + seq.scrollpos] < 99)
              {
                print_formatted_number(seq.song[x][y + seq.scrollpos], 2);
                if (y == seq.cursor_scroll && x == seq.selected_track)
                {
                  display.setCursor(6 * CHAR_width_small + (2 * CHAR_width) * x, y * lineheight + CHAR_height_small * 8);
                  print_formatted_number(seq.song[x][y + seq.scrollpos], 2, x, seq.scrollpos, 1, true);
                }

#if defined APC
                if (APC_MODE == APC_SONG)
                {
                  if (y == seq.cursor_scroll && x == seq.selected_track && seq.edit_state == false && seq.loop_edit_step == 0 && seq.tracktype_or_instrument_assign == 0)
                    handleNoteOn(11, 64 - 8 + x - (y * 8), seq.song[x][y + seq.scrollpos] * 3 + 1, 1);
                  else
                    handleNoteOn(6, 64 - 8 + x - (y * 8), seq.song[x][y + seq.scrollpos] * 3 + 1, 1);
                }
#endif
              }
              else
              {
                display.print(F("--"));
                if (y == seq.cursor_scroll && x == seq.selected_track)
                {
                  display.setCursor(6 * CHAR_width_small + (2 * CHAR_width) * x, y * lineheight + CHAR_height_small * 8);
                  print_formatted_number(0, 2, x, seq.scrollpos, 1, false);
                }
#if defined APC
                if (APC_MODE == APC_SONG)
                {
                  if (y == seq.cursor_scroll && x == seq.selected_track && seq.edit_state == false && seq.loop_edit_step == 0 && seq.tracktype_or_instrument_assign == 0)
                    handleNoteOn(11, 64 - 8 + x - (y * 8), 5, 1);
                  else
                    handleNoteOn(6, 64 - 8 + x - (y * 8), 0, 1);
                }
#endif
              }
            }
          }
        }
      }
    }
  }
  song_page_full_draw_pattern_complete = true;
}

FLASHMEM void pattern_preview_in_song(uint8_t patternno)
{
  display.setTextSize(1);
  seq.active_pattern = patternno;
  display.setCursor(0, DISPLAY_HEIGHT - CHAR_height_small);
  display.setTextColor(COLOR_SYSTEXT, COLOR_CHORDS);
  if (patternno == 99)
  {
    display.print(F("EMPTY "));
  }
  else
  {
    display.print("PAT:");
    print_formatted_number(patternno, 2);
  }
  display.setTextColor(GREY1, GREY4);
  display.print("[");
  for (uint8_t i = 0; i < 16; i++)
  {
    if (seq.vel[seq.active_pattern][i] > 209)
      display.setTextColor(COLOR_PITCHSMP, GREY4);
    else
    {
      if (seq.content_type[patternno] == 0) // Drumpattern
        display.setTextColor(COLOR_DRUMS, GREY4);
      else if (seq.content_type[patternno] == 1) // Instrument Pattern
        display.setTextColor(COLOR_INSTR, GREY4);
      else if (seq.content_type[patternno] == 2 || seq.content_type[patternno] == 3) //  chord or arp pattern
        display.setTextColor(COLOR_CHORDS, GREY4);
    }
    if (patternno == 99)
      display.print(F(" "));
    else
      display.print(seq_find_shortname(i)[0]);
  }
  display.setTextColor(GREY1, GREY4);
  display.print("]");
}

FLASHMEM void print_transpose_in_song_page()
{
  uint8_t y_start;
  uint8_t y_end;

  if (song_page_full_draw_transpose_complete == false)
  {
    y_start = 0;
    y_end = 16;
    song_page_full_draw_transpose_complete = true;
  }
  else if (seq.song_menu == 0)
  {
    y_start = 0;
    y_end = 2;
  }
  else if (seq.song_menu == 15)
  {
    y_start = 14;
    y_end = 15;
  }
  else
  {
    y_start = seq.song_menu - 1;
    y_end = seq.song_menu + 2;
  }
  display.setTextSize(1);
  if (seq.cycle_touch_element < 10)
  {
    for (uint8_t y = y_start; y < y_end; y++) // transpose chain steps
    {
      display.setCursor(CHAR_width_small * 50, CHAR_height_small * 8 + y * 10); // chain transpose
      if (seq.edit_state && seq.cycle_touch_element == 9 && seq.song_menu == y)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (seq.edit_state && seq.cycle_touch_element == 8 && seq.song_menu == y)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);

      else if (seq.cycle_touch_element == 8 && seq.edit_state == false)
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      else
        display.setTextColor(GREY1, COLOR_BACKGROUND);
      if (seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos] != 99 && seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] < 99)
      {
        if (seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] < NUM_CHAINS)
          print_formatted_number(seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y], 2);
        else
          print_formatted_number((seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y]) - NUM_CHAINS, 2);
        display.setCursor(CHAR_width_small * 49, CHAR_height_small * 8 + y * 10);
        if (seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] > NUM_CHAINS && seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] < NUM_CHAINS * 2)
          display.print(F("-"));
        else if (seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] != 0 && seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] != 99)
          display.print(F("+"));
        else if (seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] == 0)
          display.print(F(" "));
      }
      else
      {
        display.print(F("--"));
        display.setCursor(CHAR_width_small * 49, CHAR_height_small * 8 + y * 10);
        display.setTextColor(GREY1, COLOR_BACKGROUND);
        display.print(F(" "));
      }
    }
  }
}

uint8_t song_previous_displayed_chain = 99;
uint8_t chain_endline = 99;

FLASHMEM void calc_chain_endline()
{
  for (uint8_t y = 0; y < 16; y++) // chain
  {
    if (seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] == 99)
    {
      chain_endline = y;
      break;
    }
  }
}

uint8_t chain_warning_number_buffer;

FLASHMEM void empty_chain_warning_text_in_song_page()
{
  if (chain_warning_number_buffer != seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos])
  {
    if (seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][0] == 99)
    {
      display.setCursor(13 * CHAR_width_small, 29 * CHAR_height_small);
      display.setTextSize(1);
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print(F("CHAIN "));
      display.printf("[%02d]", seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]);
      display.print(F(" IS EMPTY"));
    }
    else
    {
      display.console = true;
      display.fillRect(13 * CHAR_width_small, 29 * CHAR_height_small, 113, 7, COLOR_BACKGROUND);
    }
    chain_warning_number_buffer = seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos];
  }
}

FLASHMEM void print_chain_steps_in_song_page()
{
  uint8_t y_start;
  uint8_t y_end;
  if (song_page_full_draw_chain_complete == false)
  {
    y_start = 0;
    y_end = 16;
  }
  else if (seq.song_menu == 0)
  {
    y_start = 0;
    y_end = 2;
  }
  else if (seq.song_menu == 15)
  {
    y_start = 14;
    y_end = 15;
  }
  else
  {
    y_start = seq.song_menu - 1;
    y_end = seq.song_menu + 2;
  }
  display.setTextSize(1);
  if (seq.edit_state && seq.cycle_touch_element == 9)
  {
    seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] = seq.sub_menu;
  }
  if (seq.cycle_touch_element < 10)
  {
    calc_chain_endline();
    for (uint8_t y = y_start; y < y_end; y++) // chain
    {
      if (seq.cycle_touch_element != 7)
      {
        display.setCursor(CHAR_width_small * 44, CHAR_height_small * 8 + y * 10);
        if (y == chain_endline)
        {
          display.setTextColor(RED, COLOR_BACKGROUND);
          display.print(F("END"));
          display.setTextColor(GREY1, COLOR_BACKGROUND);
        }
        if (seq.edit_state && seq.cycle_touch_element == 6 && seq.song_menu == y)
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        else
          display.setTextColor(GREY1, COLOR_BACKGROUND);
        display.setCursor(CHAR_width_small * 44, CHAR_height_small * 8 + y * 10);

        if (seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos] < 99 && seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y] < 99)
        {
          print_formatted_number(seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][y], 2);
          display.print(F(" ")); //clear previous END marking after END value changed
        }
        else if (chain_endline != y)
        {
          display.setTextColor(GREY1, COLOR_BACKGROUND);
          display.print(F("-- "));
        }
      }
    }
    song_page_full_draw_chain_complete = true;
  }
  if (seq.edit_state && seq.cycle_touch_element == 6)
  {
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);

    display.setCursor(CHAR_width_small * 44, CHAR_height_small * 8 + seq.song_menu * 10);
    if (chain_endline == seq.song_menu)
      // if (seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] == 99 && chain_endline == seq.song_menu)
      display.print(F("END"));
    else if (seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] == 99)
      display.print(F("--"));
    else
      print_formatted_number(seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu], 2);
  }
  else if (seq.edit_state && seq.cycle_touch_element == 7)
  {
    if (seq.sub_menu == NUM_CHAINS)
    {
      seq.sub_menu = 99; // Select empty step/chain
      seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] = seq.sub_menu;
      seq.sub_menu = NUM_CHAINS;
    }
    else
      seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] = seq.sub_menu;
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.setCursor(CHAR_width_small * 44, CHAR_height_small * 8 + seq.song_menu * 10);
    if (seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] == 99)
      display.print(F("--"));
    else
      print_formatted_number(seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu], 2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F(" "));
    pattern_preview_in_song(seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu]);
  }
  // print current CHAIN Number
  display.setTextSize(1);
  display.setCursor(47 * CHAR_width_small, CHAR_height_small * 4);
  if (seq.edit_state && seq.cycle_touch_element > 5 && seq.cycle_touch_element < 10)
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  else
    display.setTextColor(GREY1, COLOR_BACKGROUND);

  if (seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos] < 99)
    print_formatted_number(seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos], 2);
  else
    display.print(F("--"));

  display.setCursor(51 * CHAR_width_small, CHAR_height_small * 4); // print chain length of current track step
  print_formatted_number(get_chain_length_from_current_track(seq.selected_track), 2);
}

uint8_t midiNoteToSampleNote(uint8_t note) {
  // TODO: eliminate magic numbers
  const uint8_t offset = (note > 209) ? 210 : 18;
  return note - offset;
}

FLASHMEM const char* get_drum_name_from_note(uint8_t note)
{
  const uint8_t sampleNote = midiNoteToSampleNote(note);
  const char* name;
  if (sampleNote < NUM_DRUMSET_CONFIG) {
    name = basename(drum_config[sampleNote].name);
  }
  else {
    name = " ";
  }
  return name;
}

FLASHMEM const char* get_drum_shortname_from_note(uint8_t note)
{
  const uint8_t sampleNote = midiNoteToSampleNote(note);
  const char* name;
  if (sampleNote < NUM_DRUMSET_CONFIG) {
    name = drum_config[sampleNote].shortname;
  }
  else {
    name = "-";
  }
  return name;
}

FLASHMEM const char* tracker_find_shortname_from_pattern_step(uint8_t track, uint8_t pattern, uint8_t sstep)
{
  const char* shortname;
  const uint8_t note = seq.note_data[seq.current_pattern[track]][sstep];
  if (seq.content_type[seq.current_pattern[track]] == 0 && seq.vel[seq.current_pattern[track]][sstep] < 210) // is Drumtrack and not a pitched sample
  {
    shortname = get_drum_shortname_from_note(note);
  }
  else
  {
    if (seq.vel[seq.current_pattern[track]][sstep] > 209)
      shortname = "P"; // pitched sample
    else if (note > 0 && note != 130)
      shortname = noteNames[note % 12];
    else if (note == 130)
      shortname = "~"; // note is a tie/latch
    else
      shortname = "-";
  }
  return shortname;
}

FLASHMEM const char* seq_find_shortname_in_track(uint8_t sstep, uint8_t track)
{
  const char* shortname;
  const uint8_t note = seq.note_data[track][sstep];
  if (seq.content_type[track] == 0 && seq.vel[track][sstep] < 210) // is Drumtrack and not a pitched sample
  {
    shortname = get_drum_shortname_from_note(note);
  }
  else
  {
    if (seq.vel[track][sstep] > 209)
      shortname = "P"; // pitched sample
    else if (note > 0 && note != 130)
      shortname = noteNames[note % 12];
    else if (note == 130)
      shortname = "~"; // note is a tie/latch
    else
      shortname = "-";
  }
  return shortname;
}

FLASHMEM void print_voice_settings_in_pattern_editor(int x, int y)
{
  display.setTextSize(1);
  display.setCursor(x, y);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("INST"));
  display.setTextColor(GREEN, COLOR_BACKGROUND);
  display.setCursor(x + 101, y);
  display.print(selected_instance_id + 1);
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  display.setCursor(x + 118, y - 1);
  print_formatted_number(configuration.dexed[selected_instance_id].bank, 2);
  display.setCursor(x + 118, y + 7);
  print_formatted_number(configuration.dexed[selected_instance_id].voice + 1, 2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(x + 120 + 16, y - 1);
  display.print(g_bank_name[selected_instance_id]);
  display.setCursor(x + 120 + 16, y + 7);
  display.print(g_voice_name[selected_instance_id]);
  display.setTextSize(2);
}

FLASHMEM void update_pattern_number_in_tracker(uint8_t tracknumber)
{
  setCursor_textGrid_small(9 + 6 * tracknumber, 3);

  if (seq.current_pattern[tracknumber] < 99 && seq.current_chain[tracknumber] != 99)
  {
    set_pattern_content_type_color(seq.current_pattern[tracknumber]);
    print_formatted_number(seq.current_pattern[tracknumber], 2);
  }
  else
  {
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("--"));
  }
}

FLASHMEM void print_live_probability_pattern_info()
{
  for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++) // print track numbers, patterns and currently playing notes/chords/drums
  {
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    setCursor_textGrid_small(22 + (4 * d), 0);
    print_formatted_number(d + 1, 1);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    if (seq.current_chain[d] != 99)
    {
      setCursor_textGrid_small(22 + (4 * d), 1);
      print_formatted_number(seq.current_pattern[d], 2);
      setCursor_textGrid_small(22 + (4 * d), 2);
      set_pattern_content_type_color(seq.current_pattern[d]);
      if (seq.content_type[seq.current_pattern[d]] > 0) // it is a Inst. pattern
      {
        if (seq.note_data[seq.current_pattern[d]][seq.step] > 12 && seq.note_data[seq.current_pattern[d]][seq.step] != 130 && seq.note_data[seq.current_pattern[d]][seq.step] != 99)
        {
          display.print(noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][0]);
          if (noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][1] != '\0')
          {
            display.print(noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][1]);
          }
          display.print((seq.note_data[seq.current_pattern[d]][seq.step] / 12) - 1);
        }
        else if (seq.note_data[seq.current_pattern[d]][seq.step] == 130) // latch
          display.print(F("LAT"));
        else
          display.print(F("   "));
      }
      else // it is a drum pattern

        if (seq.vel[seq.current_pattern[d]][seq.step] < 210) // is Drumtrack and not a pitched sample
        {
          bool found = false;
          for (uint8_t n = 0; n < NUM_DRUMSET_CONFIG - 1; n++)
          {
            if (seq.note_data[seq.current_pattern[d]][seq.step] == drum_config[n].midinote)
            {
              display.print(drum_config[n].shortname);
              found = true;
              break;
            }
          }
          if (found == false)
            display.print(F("- "));
        }
        else if (seq.vel[seq.current_pattern[d]][seq.step] > 209) // pitched sample
          display.print(F("PS"));
    }
  }
}
FLASHMEM void print_track_steps_detailed_only_current_playing_note(int xpos, int ypos, uint8_t currentstep)
{
  if (seq.cycle_touch_element == 0) // touch keyboard is off
  {
    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
    { // Drum Track
      uint8_t i = 0;
      int y = 0;
      uint8_t z = 0;
      uint8_t array[2] = { currentstep, 99 };
      display.setTextSize(1);
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.setCursor(xpos, ypos);
      if (currentstep == 0)
        array[1] = 15 - seq.pattern_len_dec;
      else if (currentstep == 15 - seq.pattern_len_dec)
        array[1] = 14 - seq.pattern_len_dec;
      else
        array[1] = currentstep - 1;
      while (z < 2)
      {
        i = array[z];
        y = ypos + 10 + i * (CHAR_height_small + 2);
        // Short Name
        if ((array[1] == seq.menu - 3 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) || (array[1] == seq.menu - 1 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
          display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        else if (i == currentstep)
          display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
        else
          display.setTextColor(GREY2, COLOR_BACKGROUND);
        display.setCursor(CHAR_width_small * 4, y);
        if (seq.vel[seq.active_pattern][i] > 209) // it is a pitched Drum Sample
        {
          seq_print_current_note_from_step(i);
        }
        else
        {
          display.print(seq_find_shortname_in_track(i, seq.active_pattern)[0]);
        }
        // Data values
        if ((array[1] == seq.menu - 3 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) || (array[1] == seq.menu - 1 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
          display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        else if (i == currentstep)
          display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
        else
          display.setTextColor(GREY2, COLOR_BACKGROUND);
        display.setCursor(CHAR_width_small * 7, y);
        print_formatted_number(seq.note_data[seq.active_pattern][i], 3);
        // Velocity values
        if ((array[1] == seq.menu - 3 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) || (array[1] == seq.menu - 1 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
          display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        else if (i == currentstep)
          display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
        else
          display.setTextColor(GREY1, COLOR_BACKGROUND);
        display.setCursor(CHAR_width_small * 12, y);
        print_formatted_number(seq.vel[seq.active_pattern][i], 3);
        // Long Name / Note
        if ((array[1] == seq.menu - 3 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) ||
          (array[1] == seq.menu - 1 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
          display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        else if (i == currentstep)
          display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
        else
          set_pattern_content_type_color(seq.active_pattern);

        if (seq.content_type[seq.active_pattern] == 0)
        {
          if (seq.vel[seq.active_pattern][i] > 209) // it is a pitched Drum Sample
          {
            show_smallfont_noGrid(y, CHAR_width_small * 17, 10, basename(drum_config[seq.vel[seq.active_pattern][i] - 210].name));
          }
          else // else it is a regular Drum Sample
            show_smallfont_noGrid(y, CHAR_width_small * 17, 10, get_drum_name_from_note(seq.note_data[seq.active_pattern][i]));
        }
        else  if ((seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) ||
          (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99)) // it is a Slice
        {
          if (seq.note_data[seq.active_pattern][i] - 48 >= 0 && seq.note_data[seq.active_pattern][i] - 48 <= num_slices[0] + num_slices[1])
          {
            if (seq.note_data[seq.active_pattern][i] - 48 < num_slices[0])
              show_smallfont_noGrid(y, CHAR_width_small * 17, 10, drum_config[selected_slice_sample[0]].name);
            else if (seq.note_data[seq.active_pattern][i] - 48 <= num_slices[0] + num_slices[1])
              show_smallfont_noGrid(y, CHAR_width_small * 17, 10, drum_config[selected_slice_sample[1]].name);
            display.print(" #");
            if (seq.note_data[seq.active_pattern][i] - 48 < num_slices[0])
              display.print(seq.note_data[seq.active_pattern][i] - 48 + 1);
            else
              display.print(seq.note_data[seq.active_pattern][i] - 48 - num_slices[0] + 1);
          }
        }

        z++;
        while (display.getCursorX() < CHAR_width_small * 32)
        {
          if (i == currentstep)
            display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
          else
            display.setTextColor(GREY2, COLOR_BACKGROUND);
          display.print(" ");
        }
      }
    }
    else if (seq.content_type[seq.active_pattern] > 0)
    { // Inst Track or Chord or Arp
      if (seq.cycle_touch_element != 1)
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, currentstep, false);
    }
  }
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void print_playing_chains()
{

  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.setCursor(30 * CHAR_width_small + (x * 3 * CHAR_width_small), 1);
    if (seq.current_chain[x] < 99)
      print_formatted_number(seq.current_chain[x], 2);
    else
    {
      display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
      display.print(F("--"));
    }
    display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
    display.setCursor(30 * CHAR_width_small + (x * 3 * CHAR_width_small), 10);
    if (get_chain_length_from_current_track(x) > 0)
      print_formatted_number(get_chain_length_from_current_track(x), 2);
    else
    {
      display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
      display.print(F("--"));
    }
    // debug, show chain counter for all tracks
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.setCursor(30 * CHAR_width_small + (x * 3 * CHAR_width_small), 20);
    print_formatted_number(seq.chain_counter[x], 2);
  }
  // show longest current chain
  display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
  display.setCursor(26 * CHAR_width_small, 10);
  print_formatted_number(find_longest_chain(), 2);
}

FLASHMEM void update_display_functions_while_seq_running()
{
  seq.UI_last_seq_step = seq.step;
  // is in UI_func_seq_pattern_editor or is in UI_func_seq_vel_editor
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {
    if (seq.menu != 33 && seq.menu != 32 && seq.active_function != 95 && seq.active_function != 97)
    {
      display.setTextSize(2);
      if (seq.step == 0)
      {
        set_pattern_content_type_color(seq.active_pattern);
        setCursor_textGrid(14 - seq.pattern_len_dec, 1);
        display.print(seq_find_shortname(14 - seq.pattern_len_dec)[0]);
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        setCursor_textGrid(15 - seq.pattern_len_dec, 1);
        display.print(seq_find_shortname(15 - seq.pattern_len_dec)[0]);
        if (seq.note_editor_view == 0 && seq.cycle_touch_element == 0)
          print_track_steps_detailed_only_current_playing_note(0, CHAR_height * 4 + 3, 15 - seq.pattern_len_dec);
      }
      else if (seq.step == 1)
      {
        set_pattern_content_type_color(seq.active_pattern);
        // setCursor_textGrid(15, 1);
        setCursor_textGrid(15 - seq.pattern_len_dec, 1);
        // display.print(seq_find_shortname(15)[0]);
        display.print(seq_find_shortname(15 - seq.pattern_len_dec)[0]);
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        setCursor_textGrid(0, 1);
        display.print(seq_find_shortname(0)[0]);
        if (seq.note_editor_view == 0 && seq.cycle_touch_element == 0)
          print_track_steps_detailed_only_current_playing_note(0, CHAR_height * 4 + 3, 0);
      }
      else
      {
        set_pattern_content_type_color(seq.active_pattern);
        setCursor_textGrid(seq.step - 2, 1);
        display.print(seq_find_shortname(seq.step - 2)[0]);
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        setCursor_textGrid(seq.step - 1, 1);
        display.print(seq_find_shortname(seq.step - 1)[0]);
        if (seq.note_editor_view == 0 && seq.cycle_touch_element == 0)
          print_track_steps_detailed_only_current_playing_note(0, CHAR_height * 4 + 3, seq.step - 1);
      }
      if (seq.menu > 2 && seq.menu < 19 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
      { // update static cursor while moving cursor running in pattern editor
        setCursor_textGrid(seq.menu - 3, 1);
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, RED);
        display.print(seq_find_shortname(seq.menu - 3)[0]);
      }
      else if (seq.menu < 16 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
      { // update static cursor while moving cursor running in vel editor
        setCursor_textGrid(seq.menu - 1, 1);
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, RED);
        display.print(seq_find_shortname(seq.menu - 1)[0]);
      }

      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_arpeggio)) // is in UI of Arpeggiator
  {
    display.setTextSize(1);
    setCursor_textGrid_small(12, 17);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    print_current_chord();
    draw_euclidean_circle();
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker)) // is in UI of Tracker
  {
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.setCursor(5 * CHAR_width_small, (5 + seq.step) * (CHAR_height_small + 3) - 7);
    display.print(F(">"));
    if (seq.step == 1)
    {
      display.setCursor(CHAR_width_small * 46, 2); // print song step at each 1. pattern step
      display.setTextColor(COLOR_DRUMS, DX_DARKCYAN);
      print_formatted_number(seq.current_song_step, 2);
      display.setCursor(CHAR_width_small * 51, 2);
      print_formatted_number(get_song_length(), 2);

      for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
      {
        display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
        display.setCursor(CHAR_width_small * 16 + (CHAR_width_small * 3) * d, 2);
        print_formatted_number(seq.chain_counter[d], 2);
        tracker_print_pattern((6 + 6 * d) * CHAR_width_small, 48, d);
        update_pattern_number_in_tracker(d);
      }
    }

    if (seq.step > 0)
    {
      display.setCursor(5 * CHAR_width_small, (5 + seq.step - 1) * (CHAR_height_small + 3) - 7);
      display.print(F(" "));
    }
    else if (seq.step == 0)
    {
      display.setCursor(5 * CHAR_width_small, (5 + 15) * (CHAR_height_small + 3) - 7);
      display.print(F(" "));
    }
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song))
  {
    print_song_playhead();

    for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++) // print currently playing notes/chords/drums
    {
      if (seq.current_chain[d] != 99)
      {
        display.setCursor(CHAR_width_small * 6 + (4 * d) * CHAR_width_small, CHAR_height_small * 4);
        set_pattern_content_type_color(seq.current_pattern[d]);
        if (seq.content_type[seq.current_pattern[d]] > 0) // it is a Inst. pattern
        {
          if (seq.note_data[seq.current_pattern[d]][seq.step] > 12 && seq.note_data[seq.current_pattern[d]][seq.step] != 130 && seq.note_data[seq.current_pattern[d]][seq.step] != 99)
          {
            display.print(noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][0]);
            if (noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][1] != '\0')
            {
              display.print(noteNames[seq.note_data[seq.current_pattern[d]][seq.step] % 12][1]);
            }
            display.print((seq.note_data[seq.current_pattern[d]][seq.step] / 12) - 1);
          }
          else if (seq.note_data[seq.current_pattern[d]][seq.step] == 130) // latch
            display.print(F("LAT"));
          else
            display.print(F("   "));
        }
        else // it is a drum pattern

          if (seq.vel[seq.current_pattern[d]][seq.step] < 210) // is Drumtrack and not a pitched sample
          {
            bool found = false;
            for (uint8_t n = 0; n < NUM_DRUMSET_CONFIG - 1; n++)
            {
              if (seq.note_data[seq.current_pattern[d]][seq.step] == drum_config[n].midinote)
              {
                display.print(drum_config[n].shortname);
                found = true;
                break;
              }
            }
            if (found == false)
              display.print(F("- "));
          }
          else if (seq.vel[seq.current_pattern[d]][seq.step] > 209) // pitched sample
            display.print(F("PS"));
      }
    }
    // print currently playing chain steps
    print_playing_chains();
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_probabilities))
  {
    print_live_probability_pattern_info();
  }

#if defined APC
  apc_mute_matrix();
#endif

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_mute_matrix))
  {
    uint8_t track_count = 0;
    display.setTextSize(1);
    for (uint8_t y = 0; y < 2; y++)
    {
      for (uint8_t x = 0; x < 4; x++)
      {
        if (seq.current_chain[track_count] != 99)
        {
          if (y == 0)
            display.setCursor(x * (CHAR_width_small * 15), 82);
          else
            display.setCursor(x * (CHAR_width_small * 15), 138);
          set_pattern_content_type_color(seq.current_pattern[track_count]);
          if (seq.content_type[seq.current_pattern[track_count]] > 0) // it is a Inst. pattern
          {
            if (seq.note_data[seq.current_pattern[track_count]][seq.step] > 12 && seq.note_data[seq.current_pattern[track_count]][seq.step] != 130 && seq.note_data[seq.current_pattern[track_count]][seq.step] != 99)
            {
              display.print(noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][0]);
              if (noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][1] != '\0')
              {
                display.print(noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][1]);
              }
              display.print((seq.note_data[seq.current_pattern[track_count]][seq.step] / 12) - 1);
            }
            else if (seq.note_data[seq.current_pattern[track_count]][seq.step] == 130) // latch
              display.print(F("LAT"));
            else
            {
              display.print(F("   "));
            }
          }
          else   // it is a drum pattern
            if (seq.vel[seq.current_pattern[track_count]][seq.step] < 210) // is Drumtrack and not a pitched sample
            {
              bool found = false;
              for (uint8_t n = 0; n < NUM_DRUMSET_CONFIG - 1; n++)
              {
                if (seq.note_data[seq.current_pattern[track_count]][seq.step] == drum_config[n].midinote)
                {
                  display.print(drum_config[n].shortname);
                  found = true;
                  break;
                }
              }
              if (found == false)
              {
                display.print(F("- "));
              }
            }
            else if (seq.vel[seq.current_pattern[track_count]][seq.step] > 209) // pitched sample
              display.print(F("PS"));
        }
        track_count++;
      }
    }
  }
  else  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_chord_arranger))
  {
    uint8_t track_count = 0;
    display.setTextSize(1);
    for (uint8_t x = 0; x < 8; x++)
    {
      if (seq.current_chain[track_count] != 99)
      {
        display.setCursor(x * (CHAR_width_small * 5) + CHAR_width_small, CHAR_height_small * 6);
        set_pattern_content_type_color(seq.current_pattern[track_count]);
        if (seq.content_type[seq.current_pattern[track_count]] > 0) // it is a Inst. pattern
        {
          if (seq.note_data[seq.current_pattern[track_count]][seq.step] > 12 && seq.note_data[seq.current_pattern[track_count]][seq.step] != 130 && seq.note_data[seq.current_pattern[track_count]][seq.step] != 99)
          {
            display.print(noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][0]);
            if (noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][1] != '\0')
            {
              display.print(noteNames[seq.note_data[seq.current_pattern[track_count]][seq.step] % 12][1]);
            }
            display.print((seq.note_data[seq.current_pattern[track_count]][seq.step] / 12) - 1);
          }
          else if (seq.note_data[seq.current_pattern[track_count]][seq.step] == 130) // latch
            display.print(F("LAT"));
          else
          {
            display.print(F("   "));
          }
        }
        else   // it is a drum pattern
          if (seq.vel[seq.current_pattern[track_count]][seq.step] < 210) // is Drumtrack and not a pitched sample
          {
            bool found = false;
            for (uint8_t n = 0; n < NUM_DRUMSET_CONFIG - 1; n++)
            {
              if (seq.note_data[seq.current_pattern[track_count]][seq.step] == drum_config[n].midinote)
              {
                display.print(drum_config[n].shortname);
                found = true;
                break;
              }
            }
            if (found == false)
            {
              display.print(F("- "));
            }
          }
          else if (seq.vel[seq.current_pattern[track_count]][seq.step] > 209) // pitched sample
            display.print(F("PS"));
      }
      track_count++;
    }
  }
}

uint8_t screensaver_mode_active;
int screensaver_switcher_timer;
extern uint8_t screensaver_brightness;
extern uint16_t screensaver_counthue;
bool flock_running = false;
extern void terrain_init();
extern void terrain_frame();
extern AsteroidsScreensaver asteroidsSaver;
extern void flock_init();
extern void flock_frame();
extern void CM5_init();
extern void CM5_frame_I();
extern void CM5_frame_II();
extern void gol_setup();
extern void gol_loop();
extern bool wakeScreenFlag;

FLASHMEM void resetScreenTimer() {
  LCDML.SCREEN_resetTimer();
  // exit screensaver if active
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(mFunc_screensaver)) {
    int id = LCDML.MENU_getLastActiveFunctionID();
    if (id != _LCDML_NO_FUNC) {
      LCDML.OTHER_jumpToID(id);
    }
    else {
      LCDML.FUNC_goBackToMenu();
    }
  }
}

FLASHMEM void print_screensaver_mode()
{
  if (configuration.sys.screen_saver_mode == ScreenSaver::RANDOM)
    display.print(F("RANDOM "));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::QIX)
    display.print(F("QIX    "));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::CUBE)
    display.print(F("CUBE   "));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::SWARM)
    display.print(F("SWARM  "));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::TERRAIN)
    display.print(F("TERRAIN"));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::GAMEOFLIFE)
    display.print(F("GO LIVE"));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::CM5_I)
    display.print(F("CM5 I "));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::CM5_II)
    display.print(F("CM5 II"));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::ASTERO)
    display.print(F("ASTERO"));
  else if (configuration.sys.screen_saver_mode == ScreenSaver::DISABLED)
    display.print(F("OFF    "));

  else
    display.print(F("ERROR  "));
}



FLASHMEM void print_screensaver_mode_active()
{
  if (fade_text > 1)
  {
    if (screensaver_mode_active == ScreenSaver::QIX)
      display.print(F("QIX"));
    else if (screensaver_mode_active == ScreenSaver::CUBE)
      display.print(F("CUBE"));
    else if (screensaver_mode_active == ScreenSaver::SWARM)
      display.print(F("SWARM"));
    else if (screensaver_mode_active == ScreenSaver::TERRAIN)
      display.print(F("TERRAIN"));
    else if (screensaver_mode_active == ScreenSaver::GAMEOFLIFE)
      display.print(F("GO LIVE"));
    else if (screensaver_mode_active == ScreenSaver::CM5_I)
      display.print(F("CM5 I"));
    else if (screensaver_mode_active == ScreenSaver::CM5_II)
      display.print(F("CM5 II"));
    else if (screensaver_mode_active == ScreenSaver::ASTERO)
      display.print(F("ASTERO"));
    else if (screensaver_mode_active == ScreenSaver::DISABLED)
      display.print(F("OFF"));

    else
      display.print(F("ERROR"));
  }
  else
    display.print(F("       "));
}

FLASHMEM void sc_name_fade()
{
  if (fade_text > 1)
    {
      display.setTextSize(0);
      display.setCursor(10, DISPLAY_HEIGHT - CHAR_height_small * 2);
      if (screensaver_mode_active == ScreenSaver::CM5_I)
        display.setCursor(81, DISPLAY_HEIGHT - CHAR_height_small * 2);
      int col = ColorHSV(0, 0, fade_text);
      display.setTextColor(col, COLOR_BACKGROUND);
      print_screensaver_mode_active();
      if (fade_text > 1)
        fade_text = fade_text / 1.02;
    }
}

// *********************************************************************
FLASHMEM void mFunc_screensaver(uint8_t param) // screensaver
// *********************************************************************
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    fade_text = 255;
    encoderDir[ENC_R].reset();
    // remove compiler warnings when the param variable is not used:
    LCDML_UNUSED(param);
    display.fillScreen(0);

    screensaver_counthue = random(SCREENSAVER_MAX_COUNTHUE);
    InitializeCube();

    if (configuration.sys.screen_saver_mode == ScreenSaver::RANDOM)
    {
      randomSeed(analogRead(0));
      screensaver_mode_active = random(ScreenSaver::NUM_SCREENSAVERS - 2) + 1; // skip 0, since that is for random
    }
    else {
      screensaver_mode_active = configuration.sys.screen_saver_mode;
    }

    if (screensaver_mode_active == ScreenSaver::TERRAIN) {
      terrain_init();
    }

    if (screensaver_mode_active == ScreenSaver::GAMEOFLIFE) {

      gol_setup();
    }

    if (screensaver_mode_active == ScreenSaver::CM5_I || screensaver_mode_active == ScreenSaver::CM5_II) {
      CM5_init();
    }
    if (screensaver_mode_active == ScreenSaver::ASTERO) {
  asteroidsSaver.begin();
    }

    // setup function
    LCDML.FUNC_setLoopInterval(SCREENSAVER_INTERVAL_MS); // screensaver FUNC_loop() call interval
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (remote_active) {
      display.console = true;
    }
    if (wakeScreenFlag || LCDML.BT_checkAny()) {
      // fast wakeup from MIDI noteOn event and touch presses
      resetScreenTimer();
    }

    if (++screensaver_counthue > SCREENSAVER_MAX_COUNTHUE) {
      screensaver_counthue = 0;
    }

    if (screensaver_mode_active == ScreenSaver::QIX)
      qix_screensaver();
    else if (screensaver_mode_active == ScreenSaver::CUBE)
      cube_screensaver();
    else if (screensaver_mode_active == ScreenSaver::SWARM)
    {
      if (flock_running == false)
      {
        flock_init();
        flock_running = true;
      }
      else
        flock_frame();
    }
    else if (screensaver_mode_active == ScreenSaver::TERRAIN)
    {
      terrain_frame();
    }
    else if (screensaver_mode_active == ScreenSaver::GAMEOFLIFE)
    {
      gol_loop();
      if (screensaver_counthue > 200)
      {
        screensaver_counthue = 0;
        gol_setup();
      }
    }

    else if (screensaver_mode_active == ScreenSaver::CM5_I)
    {
      CM5_frame_I();
      screensaver_switcher_timer = screensaver_switcher_timer + 5; //since the framerate is slower on purpose
    }
    else if (screensaver_mode_active == ScreenSaver::CM5_II)
    {
      CM5_frame_II();
      screensaver_switcher_timer = screensaver_switcher_timer + 5; //since the framerate is slower on purpose
    }
    else if (screensaver_mode_active == ScreenSaver::ASTERO)
    {
      asteroidsSaver.update();
       if (configuration.sys.screen_saver_mode == ScreenSaver::RANDOM)
        screensaver_switcher_timer = screensaver_switcher_timer + 5; //since the framerate is slower on purpose
    }

    if (configuration.sys.screen_saver_mode == ScreenSaver::RANDOM)
    {
      screensaver_switcher_timer++;
      if (screensaver_switcher_timer > SCREENSAVER_STAY_TIME)
      {
        fade_text = 255;
        screensaver_switcher_timer = 0;
        display.fillScreen(COLOR_BACKGROUND);

        randomSeed(analogRead(0));
        uint8_t oldScreenSaver = screensaver_mode_active;
        while (oldScreenSaver == screensaver_mode_active) {
          screensaver_mode_active = random(ScreenSaver::NUM_SCREENSAVERS - 2) + 1; // skip 0, since that is for random
        }
        if (screensaver_mode_active == ScreenSaver::CUBE) { // reinit because 3dterrain messes up some of it's vars in same functions
          InitializeCube();
        }
        if (screensaver_mode_active == ScreenSaver::TERRAIN) { // reinit because 3dterrain messes up some of it's vars in same functions
          terrain_init();
        }
      }

      if (screensaver_switcher_timer > (SCREENSAVER_STAY_TIME - SCREENSAVER_FADE_TIME)) {
        if (screensaver_brightness > SCREENSAVER_BRIGHTNESS_STEP) {
          screensaver_brightness -= SCREENSAVER_BRIGHTNESS_STEP;
        }
      }
      else if (screensaver_switcher_timer < SCREENSAVER_FADE_TIME) {
        if (screensaver_brightness < 255 - SCREENSAVER_BRIGHTNESS_STEP) {
          screensaver_brightness += SCREENSAVER_BRIGHTNESS_STEP;
        }
      }
      else {
        screensaver_brightness = 255;
      }
    }

#if defined APC
    if (screensaver_counthue % 8 == 0)
      apc(random(64), 0, 0);
    else  if (screensaver_counthue % 9 == 0)
      apc(random(64), 36, random(5) + 1);
    else if (screensaver_counthue % 183 == 0)
      apc(random(64), 5, 6);
#endif

    sc_name_fade();

  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    if (configuration.sys.screen_saver_mode != ScreenSaver::DISABLED)
    {
      encoderDir[ENC_L].reset();
      encoderDir[ENC_R].reset();
      display.fillScreen(COLOR_BACKGROUND);
#if defined APC
      apc_clear_grid();
#endif
    }
  }
}

FLASHMEM void setup_screensaver(void)
{
  configuration.sys.screen_saver_start = constrain(configuration.sys.screen_saver_start, 1, 59);
  if (configuration.sys.screen_saver_mode == ScreenSaver::DISABLED) // off
  {
    LCDML.SCREEN_disable();
  }
  else
  {
    // Enable Screensaver (screensaver menu function, time to activate in ms)
    LCDML.SCREEN_enable(mFunc_screensaver, configuration.sys.screen_saver_start * 60000); // from parameter in minutes
   // LCDML.SCREEN_enable(mFunc_screensaver, 3000); // quick screensaver test time
  }
}

FLASHMEM void setup_ui(void)
{
  _setup_rotation_and_encoders(true);

  if (LCDML.BT_setup())
  {
    ENCODER[ENC_R].begin();
    ENCODER[ENC_L].begin();

#ifdef PCM5102_MUTE_PIN
    pinMode(PCM5102_MUTE_PIN, OUTPUT);
    digitalWrite(PCM5102_MUTE_PIN, HIGH); // ENABLE/UNMUTE DAC
#endif
  }

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(2);
  display.fillScreen(COLOR_BACKGROUND);

  // LCDMenuLib Setup
  LCDML_setup(_LCDML_DISP_cnt);

  // Enable Menu Rollover
  // LCDML.MENU_enRollover();
  setup_screensaver();
}

FLASHMEM void toggle_sequencer_play_status()
{
  if (seq.running == false && seq.recording == false)
  {
    handleStart();
  }
  else if (seq.running == true && seq.recording == false)
  {
    handleStop();
  }
}

FLASHMEM boolean key_right()
{

  if (remote_MIDI_CC == 20)
  {
    remote_MIDI_CC = 0;
    return true;
  }

#ifdef ONBOARD_BUTTON_INTERFACE
  if (digitalRead(BI_RIGHT) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_RIGHT, HIGH);
#endif
    return true;
  }
#endif

  if (joysticks[0].getAxis(0) == GAMEPAD_RIGHT_0 && joysticks[0].getAxis(1) == GAMEPAD_RIGHT_1)
    return true;

  return false;
}

FLASHMEM boolean key_left()
{
  if (remote_MIDI_CC == 21)
  {
    remote_MIDI_CC = 0;
    return true;
  }

#ifdef ONBOARD_BUTTON_INTERFACE
  if (digitalRead(BI_LEFT) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_LEFT, HIGH);
#endif
    return true;
  }
#endif

  if (joysticks[0].getAxis(0) == GAMEPAD_LEFT_0 && joysticks[0].getAxis(1) == GAMEPAD_LEFT_1)
    return true;

  return false;
}

FLASHMEM boolean key_up()
{

  if (remote_MIDI_CC == 22)
  {
    remote_MIDI_CC = 0;
    return true;
  }

#ifdef ONBOARD_BUTTON_INTERFACE
  if (digitalRead(BI_UP) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_UP, HIGH);
#endif
    return true;
  }
#endif

  if (joysticks[0].getAxis(0) == GAMEPAD_UP_0 && joysticks[0].getAxis(1) == GAMEPAD_UP_1)
    return true;

  return false;
}

FLASHMEM boolean key_down()
{

  if (remote_MIDI_CC == 23)
  {
    remote_MIDI_CC = 0;
    return true;
  }

#ifdef ONBOARD_BUTTON_INTERFACE
  if (digitalRead(BI_DOWN) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_DOWN, HIGH);
#endif
    return true;
  }
#endif

  if (joysticks[0].getAxis(0) == GAMEPAD_DOWN_0 && joysticks[0].getAxis(1) == GAMEPAD_DOWN_1)
    return true;

  return false;
}

FLASHMEM void gamepad_seq_navigation_func(uint32_t buttons)
{
  if (gamepad_millis > configuration.sys.gamepad_speed && seq.cycle_touch_element < 6 && buttons == GAMEPAD_SELECT && key_right())
  {
    seq.cycle_touch_element = 6; // goto chain edit
    seq.help_text_needs_refresh = true;
    seq.edit_state = true;
    print_chain_steps_in_song_page();
    print_transpose_in_song_page();
    print_chain_matrix_in_song_page();
    print_shortcut_navigator();
    print_song_mode_help();
    gamepad_millis = 0;
  }
  else if ((seq.cycle_touch_element == 6 && buttons == GAMEPAD_SELECT && key_left()) || (seq.cycle_touch_element == 7 && buttons == GAMEPAD_SELECT && key_left()))
  {
    seq.cycle_touch_element = 0; // goto main song mode
    seq.help_text_needs_refresh = true;
    seq.edit_state = false;
    print_chain_steps_in_song_page();
    print_transpose_in_song_page();
    print_chain_matrix_in_song_page();
    print_shortcut_navigator();
    print_song_mode_help();
    gamepad_millis = 0;
  }
  else if ((seq.cycle_touch_element == 8 && buttons == GAMEPAD_SELECT && key_left()) || (seq.cycle_touch_element == 9 && buttons == GAMEPAD_SELECT && key_left()))
  {
    seq.cycle_touch_element = 6; // go back from transpose to chain
    seq.help_text_needs_refresh = true;
    seq.edit_state = true;
    print_chain_steps_in_song_page();
    print_transpose_in_song_page();
    print_chain_matrix_in_song_page();
    print_shortcut_navigator();
    print_song_mode_help();
    gamepad_millis = 0;
  }
  else if ((seq.cycle_touch_element == 6 && buttons == GAMEPAD_SELECT && key_right()) || (seq.cycle_touch_element == 7 && buttons == GAMEPAD_SELECT && key_right()))
  {
    seq.cycle_touch_element = 8; // goto transpose from chain
    seq.help_text_needs_refresh = true;
    seq.edit_state = true;
    print_chain_steps_in_song_page();
    print_transpose_in_song_page();
    print_chain_matrix_in_song_page();
    print_shortcut_navigator();
    print_song_mode_help();
    gamepad_millis = 0;
  }
  else if (buttons == GAMEPAD_SELECT && key_up())
  {
    generic_temp_select_menu = 6; // preselect BPM
    LCDML.OTHER_jumpToFunc(UI_func_seq_settings);
    gamepad_millis = 0;
  }
  else if (seq.cycle_touch_element > 7 && buttons == GAMEPAD_SELECT && key_right())
  { // go to pattern editor
    gamepad_millis = 0;
    seq.quicknav_song_to_pattern_jump = true;
    LCDML.OTHER_jumpToFunc(UI_func_seq_pattern_editor);
  }
}

// FLASHMEM void gamepad_learn_func(uint32_t buttons)
// {
//   if (gamepad_millis > 300)
//   {
//     if (temp_int > 7)
//     {
//       setCursor_textGrid_small(35, 3);
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//       display.print(F("READY ! "));
//       setCursor_textGrid_small(1, 17);
//       display.setTextColor(RED, COLOR_BACKGROUND);
//       display.print(F("GO BACK WITH ENC[L]"));
//       setCursor_textGrid_small(21, 17);
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//       display.print(F("AND TEST OUT YOUR GAMEPAD."));
//       setCursor_textGrid_small(1, 18);
//       display.print(F("ANY CHANGE TO GLOBALS, LIKE "));
//       display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//       display.print(F("MASTER VOLUME "));
//       setCursor_textGrid_small(1, 19);
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//       display.print(F("WILL STORE YOUR CURRENT GAMEPAD SETTINGS."));
//     }
//     else
//     {
//       display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//       setCursor_textGrid_small(35, 3);
//       display.print(F("STEP "));
//       display.print(temp_int + 1);
//       display.print(F("/8  "));
//     }
//     if (temp_int == 0)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 0)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 6);
//     print_formatted_number(GAMEPAD_UP_0, 3);
//     setCursor_textGrid_small(20, 6);
//     print_formatted_number(GAMEPAD_UP_1, 3);

//     if (temp_int == 1)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 1)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 7);
//     print_formatted_number(GAMEPAD_DOWN_0, 3);
//     setCursor_textGrid_small(20, 7);
//     print_formatted_number(GAMEPAD_DOWN_1, 3);

//     if (temp_int == 2)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 2)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 8);
//     print_formatted_number(GAMEPAD_LEFT_0, 3);
//     setCursor_textGrid_small(20, 8);
//     print_formatted_number(GAMEPAD_LEFT_1, 3);

//     if (temp_int == 3)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 3)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 9);
//     print_formatted_number(GAMEPAD_RIGHT_0, 3);
//     setCursor_textGrid_small(20, 9);
//     print_formatted_number(GAMEPAD_RIGHT_1, 3);

//     if (temp_int == 4)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 4)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 11);
//     print_formatted_number(GAMEPAD_BUTTON_A, 3);
//     if (temp_int == 5)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 5)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 12);
//     print_formatted_number(GAMEPAD_BUTTON_B, 3);
//     if (temp_int == 6)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 6)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 14);
//     print_formatted_number(GAMEPAD_SELECT, 3);
//     if (temp_int == 7)
//       display.setTextColor(RED, COLOR_BACKGROUND);
//     else if (temp_int > 7)
//       display.setTextColor(GREEN, COLOR_BACKGROUND);
//     else
//       display.setTextColor(GREY2, COLOR_BACKGROUND);
//     setCursor_textGrid_small(16, 15);
//     print_formatted_number(GAMEPAD_START, 3);
//     if (buttons != gamepad_buttons_neutral || joysticks[0].getAxis(0) != gamepad_0_neutral || joysticks[0].getAxis(1) != gamepad_1_neutral)
//     {
//       if (temp_int == 0)
//       {
//         GAMEPAD_UP_0 = joysticks[0].getAxis(0);
//         GAMEPAD_UP_1 = joysticks[0].getAxis(1);
//       }
//       else if (temp_int == 1)
//       {
//         GAMEPAD_DOWN_0 = joysticks[0].getAxis(0);
//         GAMEPAD_DOWN_1 = joysticks[0].getAxis(1);
//       }
//       else if (temp_int == 2)
//       {
//         GAMEPAD_LEFT_0 = joysticks[0].getAxis(0);
//         GAMEPAD_LEFT_1 = joysticks[0].getAxis(1);
//       }
//       else if (temp_int == 3)
//       {
//         GAMEPAD_RIGHT_0 = joysticks[0].getAxis(0);
//         GAMEPAD_RIGHT_1 = joysticks[0].getAxis(1);
//       }
//       else if (temp_int == 4)
//       {
//         GAMEPAD_BUTTON_A = buttons;
//       }
//       else if (temp_int == 5)
//       {
//         GAMEPAD_BUTTON_B = buttons;
//       }
//       else if (temp_int == 6)
//       {
//         GAMEPAD_SELECT = buttons;
//       }
//       else if (temp_int == 7)
//       {
//         GAMEPAD_START = buttons;
//       }
//       temp_int++;
//       gamepad_millis = 0;
//     }
//   }
// }

/***********************************************************************
   GENERAL UI ELEMENTS
 ***********************************************************************/

FLASHMEM bool increment_dexed_instance()
{
  const bool change = selected_instance_id < (NUM_DEXED - 1);
  if (change) {
    selected_instance_id++;
  }
  return change;
}

FLASHMEM void increment_dexed_instance_round_robin()
{
  selected_instance_id++;
  if (selected_instance_id == NUM_DEXED)
    selected_instance_id = 0;
}

FLASHMEM bool decrement_dexed_instance()
{
  const bool change = selected_instance_id > 0;
  if (change) {
    selected_instance_id--;
  }
  return change;
}

FLASHMEM void setModeColor(uint8_t selected_option)
{
  if (generic_temp_select_menu == selected_option)
  {
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  }
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  if ((generic_temp_select_menu == selected_option && generic_active_function == 1) || (generic_temp_select_menu == selected_option && seq.edit_state == 1))
  {
    display.setTextColor(COLOR_SYSTEXT, RED);
  }
}

// only used for displaying value as a bar-graph display without numeric value, borders or other extras
FLASHMEM void print_fast_level_indicator(uint8_t x, uint8_t y, int16_t input_value, int16_t limit_min, int16_t limit_max)
{
  if (limit_min >= 0)
  { // filled bar
    uint8_t split = (5 * CHAR_width_small - 2) * (input_value - limit_min) / (limit_max - limit_min);
    display.console = true;
    if (split < 5 * CHAR_width_small - 2)
      display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + split, 10 * y + 1, 5 * CHAR_width_small - 2 - split, 5, COLOR_BACKGROUND);
    display.console = true;
    if (split > 0)
      display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y + 1, split, 5, RED);
  }
}

#if defined GLOW
extern void set_glow_bargraph(uint8_t x, uint8_t y, int glow_input_value, int menuitem, int min, int max, const char* zero_name);
extern void set_glow_panbar(uint8_t x, uint8_t y, int glow_input_value, int menuitem);
#endif

// a small scaled bar, that can show an abitrary range in always the same space, given the expected min and max limit values.
// also automagically shows a pan bar (marker line instead of filled bar), if min is smaller than zero.
FLASHMEM void print_small_scaled_bar(uint8_t x, uint8_t y, int16_t input_value, int16_t limit_min, int16_t limit_max, int16_t selected_option, boolean show_bar, const char* zero_name = NULL)
{
  setCursor_textGrid_small(x, y);
#if defined GLOW
  if (seq.edit_state == 0)
    set_glow_bargraph(x, y, input_value, selected_option, limit_min, limit_max, zero_name);
#endif
  setModeColor(selected_option);

  if (limit_min == 0 && limit_max == 1)
    display.print(input_value ? F("ON ") : F("OFF"));
  else if (zero_name != NULL && input_value == 0)
    display.print(zero_name);
  else if (zero_name != NULL && input_value == 17)
    display.print(zero_name);
  else if (limit_min < 0)
    print_formatted_number_signed(input_value, 2);
  else if (limit_max < 100) {
    setCursor_textGrid_small(x + 1, y);
    print_formatted_number(input_value, 2);
  }
  else if (limit_max < 1000) {
    print_formatted_number(input_value, 3);
  }
  else {
    print_formatted_number(input_value, 4);
  }

  if (show_bar)
  {
    display.console = true;

    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 5 * CHAR_width_small, 7, input_value == 0 ? GREY2 : COLOR_SYSTEXT);

    if (limit_min >= 0)
    { // filled bar
      uint8_t split = (5 * CHAR_width_small - 2) * (input_value - limit_min) / (limit_max - limit_min);
      display.console = true;
      if (split < 5 * CHAR_width_small - 2)
        display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + split, 10 * y + 1, 5 * CHAR_width_small - 2 - split, 5, COLOR_BACKGROUND);
      display.console = true;
      if (split > 0)
        display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1, 10 * y + 1, split, 5, COLOR_PITCHSMP);
    }
    else
    { // pan bar
      uint8_t split = (5 * CHAR_width_small - 2 - 3) * (input_value - limit_min) / (limit_max - limit_min);
      display.console = true;
      display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1, 10 * y + 1, 5 * CHAR_width_small - 2, 7 - 2, COLOR_BACKGROUND);
      display.console = true;
      display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + split, 10 * y + 1, 3, 5, COLOR_PITCHSMP);
    }
  }
}

FLASHMEM void _print_panbar_value(uint8_t input_value)
{
  if (input_value < 20)
  {
    display.print(F("L"));
    print_formatted_number(20 - input_value, 2);
  }
  else if (input_value > 20)
  {
    display.print(F("R"));
    print_formatted_number(input_value - 20, 2);
  }
  else
  {
    display.print(F("C"));
    print_formatted_number(input_value - 20, 2);
  }
}
FLASHMEM void print_small_intbar(uint8_t x, uint8_t y, uint8_t input_value, uint8_t selected_option, boolean show_bar, boolean show_zero)
{
  print_small_scaled_bar(x, y, input_value, 0, 100, selected_option, show_bar, show_zero ? NULL : (const char*)F("OFF"));
  //set_glow(x,y,input_value,selected_option,0,100,show_zero ? NULL : (const char*)F("OFF"));
}

FLASHMEM void print_small_panbar(uint8_t x, uint8_t y, uint8_t input_value, uint8_t selected_option)
{
  setCursor_textGrid_small(x, y);
#if defined GLOW
  set_glow_panbar(x, y, input_value, selected_option);
#endif
  setModeColor(selected_option);

  _print_panbar_value(input_value);
  display.console = true;
  display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 5 * CHAR_width_small, 7, COLOR_SYSTEXT);
  display.console = true;
  display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1, 10 * y + 1, 5 * CHAR_width_small - 2, 7 - 2, COLOR_BACKGROUND);
  display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + input_value / 1.60, 10 * y + 1, 3, 5, COLOR_PITCHSMP);

}


// Store a generic parameter editor.
//
// The editor has
// - a name, as shown on screen
// - a position on screen (x,y),
// - a select_id defining its position in the selection order (usually assigned automatically)
// - limits for its value
// - a fast flag, to enable accelerated changes of large values (also set automatically for large ranges)
// - a pointer to the value to adjust, can be NULL if only custom setters / getters are used.
// - a pointer to a getter function (can be set automatically for simple editors, using the value pointer)
// - a pointer to a setter function (can be set automatically for simple editors, using the value pointer)
// - a pointer to a renderer (can be null to provide the default small-scaled-bar-with-number editor)
class Editor
{
  friend class UI;

private:
  // Helper function to determine the acceleration level based on range  // too many side effects, have to come up with something better
  int get_accel_level(int16_t value_range) {
    // if (value_range > 1000) {
    //   return 3; // Super-fast (for very large ranges)
    // }
    // if (value_range > 500) {
    //   return 2; // Fast (for large ranges)
    // }
    // if (value_range > 260) {
    //   return 1; // Normal fast (for medium ranges)
    // }
    return 0; // Default step of 1 (for small ranges)
  }

  // Changed from a bool 'fast' to an int 'accel_level'
  const int accel_level;
  int16_t(* const getter)(Editor* param);              // pointer to a function to read the value from somewhere
  void (* const setter)(Editor* param, int16_t value);  // pointer to a function to save the altered value somewhere
  void (* const renderer)(Editor* param, bool refresh); // pointer to a function to draw the editor on screen.
  // if refresh is true, it should redraw value-dependent parts.

  // constructor for creating empty array
  Editor()
    : accel_level(0), getter(NULL), setter(NULL), renderer(NULL), name(NULL), x(0), y(0), select_id(-1),
    limit_min(0), limit_max(0), value(NULL) {
  }

  // only valid constructor used by friend class UI
  Editor(const char* const _name, int16_t _limit_min, int16_t _limit_max,
    uint8_t _x, uint8_t _y, uint8_t _select_id,
    void* const _valuePtr,
    int16_t(* const _getter)(Editor* param),
    void (* const _setter)(Editor* param, int16_t value),
    void (* const _renderer)(Editor* param, bool refresh))
    : accel_level(get_accel_level(_limit_max - _limit_min)), // Set acceleration level in constructor
    getter(_getter), setter(_setter), renderer(_renderer),
    name(_name), x(_x), y(_y), select_id(_select_id), limit_min(_limit_min), limit_max(_limit_max),
    value(_valuePtr) {
  }

  // draw this editor on screen. If refresh is set,
  // only parts that depend on changed value or selection are drawn.
  // otherwise everything is drawn (label, borders, etc.)
  //
  // by default if renderer==NULL, print_small_scaled_bar is used.
  // otherwise the definded renderer is used.
  //
  void draw_editor(bool refresh)
  {
    if (renderer != NULL)
    {
      // a custom renderer is defined. Draw using it and exit.
      renderer(this, refresh);
      return;
    }
    // No custom rederer was defined, just draw label and small_scaled_bar.
    display.setTextSize(1);
    if (!refresh)
    {
      setCursor_textGrid_small(this->x + 10, this->y);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(this->name);
    }
    print_small_scaled_bar(x, y, get(), limit_min, limit_max, select_id, 1, NULL);
  };

  // read encoders / other input sources and adjust the assigned value
  // by using the getter and setter functions. And redraw afterwards.
  void handle_parameter_editor(EncoderEvents e)
  {
    int16_t change = e.dir;
    if (accel_level > 0) {
      change *= e.speed;
    }
    const int16_t value = get();
    const int16_t newValue = constrain(value + change, limit_min, limit_max);
    if (newValue != value) {
      set(newValue);
      //DBG_LOG(printf("value changed from %i to %i, draw!\n", value, newValue));
      draw_editor(true);
    }
  };

public:
  // these members are never set, but still public, as they are
  // used by custom getter, setter and render functions.
  const char* const name;             // short name of parameter.
  const uint8_t x, y;                 // position on screen in small-font grid
  const uint8_t select_id;            // position in input selection order - automatically set on definiton of UI elements.
  const int16_t limit_min, limit_max; // int limits of the parameter, used to limit input and scale the bar
  void* const value;                  // a pointer to the value to adjust. The value can be of any type (eg. uint8_t, int16_t, float32_t...)
  // The getter and setter functions must correctly handle the type.
  // get & set this editors value, propagating to the value storage or setter.
  int16_t get()
  {
    if (getter != NULL)
      return getter(this);
    return 0; // should never happen
  };
  void set(int16_t _value)
  {
    // should always be !=NULL, just prevent crashes.
    if (setter != NULL)
      setter(this, _value);
  };
};

// Global UI helper
//
// This helps conveniently build a single UI page,
// and repeatedly update all UI elements with a calls to handle_input() later.
//
// a lot of addEditor and addCustom editor functions are provided to define UI elements
// for different value types, adding custom getters or setters to update the engines under control.
// And provide custom renderers for some elements (eg. the instance selector, name fields etc.)
//
// The usual UI definition goes this way:
//
// on LCDML.func_setup():
// -Remove the old UI by calling ui.reset(). This also clears the screen.
// -Add one or more editors by addEditor and addCustom editor. Create sections by setCursor() and printLn().
// -all coding work goes here, especially by providing custom getter, setter or renderer code to handle
//  the different parameter types and how engine parameters are modified.
// on LCDML.func_loop():
// -just call ui.handle_input().
//
#define UI_MAX_EDITORS 64
class UI
{
private:
  uint8_t x, y;                      // current cursor position, can be set and updates automatically
  uint8_t num_editors;               // the count of editors defined so far
  Editor editors[UI_MAX_EDITORS];    // the editors defined for the current page
  Editor* encoderLeftHandler = NULL; // a special editor assigned to the left encoder
  Editor* buttonLongHandler = NULL;  // a special editor assigned to a long button press

public:

  bool instance_select_flag;

  // clear, but dont reset this UI.
  // this can be called for massive refreshs without loosing the current selection.
  // All UI elements must be defined again afterwards.
  void clear()
  {
    display.fillScreen(COLOR_BACKGROUND);

    if (touch_button_back_page() || legacy_touch_button_back_page())
    {
      draw_back_touchbutton();
      // current_page_has_touch_back_button = true;
    }
    else
      helptext_l(back_text);

    num_editors = 0;
    instance_select_flag = false;
    buttonLongHandler = NULL;
    encoderLeftHandler = NULL;
  };

  // reset. this must be called to start a new UI page.
  // like clear(), but the selection is reset too.
  void reset()
  {
    clear();
    seq.edit_state = 0;
    generic_temp_select_menu = 0;
  };

  void clear_noisemaker()  //only reset the editors in noisemaker
  {
    num_editors = 0;
    instance_select_flag = false;
    buttonLongHandler = NULL;
    encoderLeftHandler = NULL;
  };

  // set the cursor before drawing the next UI element.
  // if omitted, the next element is drawn just below the last one.
  void setCursor(uint8_t _x, uint8_t _y)
  {
    x = _x;
    y = _y;
  };

  // print a static label that can't be selected.
  void printLn(const char* text, uint32_t color = RED)
  {
    print(text, color);
    y += 1;
  }

  // print some text on cursor location for use in renderers
  void print(const char* text, uint32_t color = RED)
  {
    display.setTextSize(1);
    setCursor_textGrid_small(x, y);
    display.setTextColor(color);
    display.print(text);
  }


  // add a custom editor providing its own getter, setter and renderer function.
  void addCustomEditor(const char* const name, int16_t limit_min, int16_t limit_max,
    void* const valuePtr,
    int16_t(* const getter)(Editor* param),
    void (* const setter)(Editor* param, int16_t value),
    void (* const renderer)(Editor* param, bool refresh))
  {

    // construct editor in-place in editors array
    // there is no corresponding delete, as editors are just overwritten after ui.reset() !
    new (&editors[num_editors]) Editor(
      name, limit_min, limit_max, x, y, num_editors, valuePtr,
      getter, setter, renderer);

    editors[num_editors].draw_editor(false);
    y++;
    num_editors++;
  };

  // editor providing default float32_t getter + setters if missed out
  void addEditor(const char* const name, int16_t limit_min, int16_t limit_max, float32_t* valuePtr,
    int16_t(* const getter)(Editor* param) = NULL,
    void (* const setter)(Editor* param, int16_t value) = NULL,
    void (* const renderer)(Editor* param, bool refresh) = NULL)
  {
    addCustomEditor(
      name, limit_min, limit_max, valuePtr,
      getter != NULL ? getter : [](Editor* editor) -> int16_t
      {
        return *((float32_t*)editor->value) * 100;
      },
      setter != NULL ? setter : [](Editor* editor, int16_t value) -> void
      {
        *((float32_t*)editor->value) = value / 100.f;
      },
      renderer);
  };

  // editor providing default uint8_t getter + setters if missed out
  void addEditor(const char* const name, int16_t limit_min, int16_t limit_max, uint8_t* const valuePtr,
    int16_t(* const getter)(Editor* param) = NULL,
    void (* const setter)(Editor* param, int16_t value) = NULL,
    void (* const renderer)(Editor* param, bool refresh) = NULL)
  {
    addCustomEditor(
      name, limit_min, limit_max, valuePtr,
      getter != NULL ? getter : [](Editor* editor) -> int16_t
      {
        return *((uint8_t*)editor->value);
      },
      setter != NULL ? setter : [](Editor* editor, int16_t value) -> void
      {
        *((uint8_t*)editor->value) = value;
      },
      renderer);
  };

  // editor providing custom getter + setters not using the valuePtr feature
  void addEditor(const char* const name, int16_t limit_min, int16_t limit_max,
    int16_t(* const getter)(Editor* param),
    void (* const setter)(Editor* param, int16_t value),
    void (* const renderer)(Editor* param, bool refresh) = NULL)
  {
    addCustomEditor(
      name, limit_min, limit_max, NULL,
      getter != NULL ? getter : [](Editor* editor) -> int16_t
      {
        return *((uint8_t*)editor->value);
      },
      setter != NULL ? setter : [](Editor* editor, int16_t value) -> void
      {
        *((uint8_t*)editor->value) = value;
      },
      renderer);
  };

  // assign the editor just added to the long button press too (just a toggle action)
  void enableButtonLongEditor()
  {
    buttonLongHandler = &editors[num_editors - 1];
  }

  // check if the current UI snags on long button presses
  bool handlesButtonLong()
  {
    return buttonLongHandler != NULL;
  }

  // assign the editor just added to the left encoder (always, no selection needed)
  void enableLeftEncoderEditor()
  {
    encoderLeftHandler = &editors[num_editors - 1];
  }

  // check if the current UI snags on the left encoder
  bool handlesLeftEncoder()
  {
    return encoderLeftHandler != NULL;
  }

  // (re-)draw all editors, omit static parts if refresh is set.
  void draw_editors(bool refresh)
  {
    for (uint8_t i = 0; i < num_editors; i++)
      editors[i].draw_editor(refresh);
  };

  // Handle all input, updating editors and engine parameters, if changed.
  //
  // This need to be repeatedly called in the menu loop, usually by LCDML.func_loop() sections
  //
  void handle_input()
  {
    // toggle between navigate and value editing
    if (LCDML.BT_checkEnter() && encoderDir[ENC_R].ButtonShort())
    {
      seq.edit_state = 1 - seq.edit_state;
      editors[generic_temp_select_menu].draw_editor(true);
      flush_sysex();
    }

    EncoderEvents e = getEncoderEvents(ENC_R);

    // handle right encoder
    if (e.turned) {
      if (seq.edit_state == 0) {
        // handle parameter selection
        uint8_t last = generic_temp_select_menu;
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, num_editors - 1);
        editors[last].draw_editor(true);
        editors[generic_temp_select_menu].draw_editor(true);
      }
      else {
        // Use the accel_level to determine the speed multiplier
        int16_t speed_multiplier = 1;
        Editor& current_editor = editors[generic_temp_select_menu];

        // switch (current_editor.accel_level) {  //too much sideeffects. needs a better solution
        // case 3:
        //   speed_multiplier = 50;
        //   break;
        // case 2:
        //   speed_multiplier = 5;
        //   break;
        // case 1:
        //   speed_multiplier = 2;
        //   break;
        // default:
        //   speed_multiplier = 1;
        //   break;
        // }

        e.speed = speed_multiplier;

        // change currently selected editor's value by right encoder
        current_editor.handle_parameter_editor(e);
      }
    }

    // optionally toggle a specific editor by long button press
    if (buttonLongHandler && e.longPressed) {
      if (instance_select_flag) {
        increment_dexed_instance_round_robin();
        buttonLongHandler->set(selected_instance_id);
      }
      else {
        buttonLongHandler->set(1 - buttonLongHandler->get()); // toggle value between 0 and 1
      }

      buttonLongHandler->draw_editor(true);
    }

    // optionally set a specific editor's value by left encoder
    if (encoderLeftHandler) {
      EncoderEvents e_left = getEncoderEvents(ENC_L);
      if (e_left.turned) {
        // Use acceleration logic for the left encoder as well
        int16_t speed_multiplier = 1;

        switch (encoderLeftHandler->accel_level) {
        case 3:
          speed_multiplier = 100;
          break;
        case 2:
          speed_multiplier = 25;
          break;
        case 1:
          speed_multiplier = 5;
          break;
        default:
          speed_multiplier = 1;
          break;
        }
        e_left.speed = speed_multiplier;

        encoderLeftHandler->handle_parameter_editor(e_left);
      }
    }
  };
} ui;


#ifdef MUSIKANDMORE
extern int analog_volume_value;
extern int analog_volume_value_previous;
#endif

/***********************************************************************
   MENU CONTROL
 ***********************************************************************/

#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
extern void All_Button_LEDS(bool state);
#endif

uint8_t touchX = 0;
uint8_t touchY = 0;

FLASHMEM void lcdml_menu_control(void)
{
  // If something must init, put in in the setup condition

  if (back_from_volume > BACK_FROM_VOLUME_MS && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_volume))
  {
    encoderDir[ENC_L].reset();
    encoderDir[ENC_R].reset();

    if (LCDML.MENU_getLastActiveFunctionID() < 0xff)
      LCDML.OTHER_jumpToID(LCDML.MENU_getLastActiveFunctionID());
    else
      LCDML.OTHER_setCursorToID(LCDML.MENU_getLastCursorPositionID());
    // LCDML.FUNC_goBackToMenu();
  }

  // Volatile Variables
  int8_t g_LCDML_CONTROL_Encoder_position[NUM_ENCODER] = { ENCODER[ENC_R].read(), ENCODER[ENC_L].read() };

#if defined (RGB_ENCODERS) && defined (MCP_23008) || defined (RGB_ENCODERS) && defined (MCP_23017)
  button[0] = !digitalRead(BUT_R_PIN);
  button[1] = !digitalRead(BUT_L_PIN);
#else
  button[0] = digitalRead(BUT_R_PIN);
  button[1] = digitalRead(BUT_L_PIN);
#endif


#ifdef USB_KEYPAD // USB KEYPAD CONTROL TEST
  if (USB_KEY != 0)
  {
    switch (USB_KEY)
    {
    case 211:
      g_LCDML_CONTROL_Encoder_position[ENC_L] = -4;
      break;
    case 218:
      g_LCDML_CONTROL_Encoder_position[ENC_L] = 4;
      break;
    case 43:
      button[ENC_L] = LOW;
      break;

    case 214:
      g_LCDML_CONTROL_Encoder_position[ENC_R] = -4;
      break;
    case 217:
      g_LCDML_CONTROL_Encoder_position[ENC_R] = 4;
      break;
    case 10:
      button[ENC_R] = LOW;
      break;
    }
}
#endif

#ifdef MUSIKANDMORE
  if (configuration.sys.vol_control)
  {
    if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_song) && LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) &&
      LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker))
    {
      // analog_volume_value = 1024 - analogRead(ANALOG_VOLUME_IN);
      analog_volume_value = analogRead(ANALOG_VOLUME_IN);
      if (analog_volume_value > analog_volume_value_previous + 10 || analog_volume_value < analog_volume_value_previous - 10)
      {
        g_LCDML_CONTROL_Encoder_position[ENC_L] = +4;
        configuration.sys.vol = analog_volume_value / 8 + 1;
        if (configuration.sys.vol > 100)
          configuration.sys.vol = 100;
        analog_volume_value_previous = analog_volume_value;
      }
    }
  }
#endif

  uint32_t buttons = joysticks[0].getButtons();

  // MIDI remote
  if (remote_MIDI_CC > 0)
  {
    switch (remote_MIDI_CC)
    {
    case 24: // SELECT
      remote_MIDI_CC = 0;
      buttons = GAMEPAD_SELECT;
      remote_console_keystate_select = (remote_MIDI_CC_value == 127 ? true : false);
      break;
      // case 25: // START
      // buttons = buttons + GAMEPAD_START;
      // remote_MIDI_CC = 0;
      //   break;
    case 26: // BUTTON B
      remote_MIDI_CC = 0;
      buttons = buttons + GAMEPAD_BUTTON_B;
      remote_console_keystate_b = (remote_MIDI_CC_value == 127 ? true : false);
      break;
    case 27: // BUTTON A
      remote_MIDI_CC = 0;
      buttons = buttons + GAMEPAD_BUTTON_A;
      remote_console_keystate_a = (remote_MIDI_CC_value == 127 ? true : false);
      break;
    case 28: // init display at remote connection
      remote_MIDI_CC = 0;
      buttons = 0;
      remote_console_keystate_select = false;
      remote_console_keystate_a = false;
      remote_console_keystate_b = false;
      ts.touch_ui_drawn_in_menu = false;
      ts.keyb_in_menu_activated = false;

      // enable/disable remote display
      remote_active = (remote_MIDI_CC_value == 1 ? true : false);
      display.console = remote_active;

      // if sequencer is running, alert remote
      if (seq.running)
      {
        usbMIDI.sendRealTime(midi::Start);
      }

      // if remote active, dynamic patching for usb audio
#ifdef AUDIO_DEVICE_USB
      if (remote_active)
      {
        patchCordUsbL.connect();
        patchCordUsbR.connect();
      }
      else
      {
        patchCordUsbL.disconnect();
        patchCordUsbR.disconnect();
      }
#endif

      // LCDML.FUNC_goBackToMenu();
      LCDML.MENU_goRoot();
      break;
    case 29: // remote touch, receive x
      remote_MIDI_CC = 0;
      touchX = remote_MIDI_CC_value;
      break;
    case 30: // remote touch, receive y
      remote_MIDI_CC = 0;
      touchY = remote_MIDI_CC_value;

      // remote touch pressed
      ts.p.x = touchX * 2.5 + 1; // incoming x is divided by 2.5 minus 1
      ts.p.y = touchY * 2.5 + 1; // incoming y is divided by 2.5 minus 1
      buttons = 0;
      remote_touched = true;
      break;
    case 31:
      // remote touch released
      remote_MIDI_CC = 0;
      remote_touched = false;
      touchX = touchY = 0;
      break;
    default:
      break;
    }
  }

#ifdef ONBOARD_BUTTON_INTERFACE
  // if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_map_gamepad))
  // {

  if (digitalRead(BI_SELECT) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_SEL, HIGH);
#endif
    buttons = GAMEPAD_SELECT;
  }
  if (digitalRead(BI_START) == false)
  {
    if (buttons == GAMEPAD_SELECT)
      buttons = buttons + GAMEPAD_START;
    else
      buttons = GAMEPAD_START;
  }
  if (digitalRead(BI_BUTTON_A) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_BUT_A, HIGH);
#endif
    // buttons = buttons + GAMEPAD_BUTTON_A;
    buttons = GAMEPAD_BUTTON_A;
  }
  if (digitalRead(BI_BUTTON_B) == false)
  {
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    setled(LED_BUT_B, HIGH);
#endif
    // buttons = buttons + GAMEPAD_BUTTON_B;
    buttons = GAMEPAD_BUTTON_B;
  }
  //}
#endif

  if (remote_console_keystate_select)
  {
    buttons = GAMEPAD_SELECT;
    remote_console_keystate_select = true;
  }
  if (remote_console_keystate_a)
  {
    buttons = buttons + GAMEPAD_BUTTON_A;
    remote_console_keystate_a = true;
  }
  if (remote_console_keystate_b)
  {
    buttons = buttons + GAMEPAD_BUTTON_B;
    remote_console_keystate_b = true;
  }

  if (gamepad_millis + (gamepad_accelerate) >= configuration.sys.gamepad_speed)
  {

    // key-learn function
    // if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_map_gamepad) && temp_int < 9)
    // {
    //   gamepad_learn_func(buttons);
    // }

    // LSDJ Style Navigation:
    if (buttons == GAMEPAD_SELECT && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song))
    {
      gamepad_seq_navigation_func(buttons);
    }
    else if ((buttons != 0 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor)) || (buttons != 0 && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor)))
    {
      if (buttons == GAMEPAD_SELECT && key_left())
      { // go back to song-transpose
        seq.help_text_needs_refresh = true;
        seq.edit_state = true;
        seq.quicknav_pattern_to_song_jump = true;
        seq.quicknav_song_to_pattern_jump = false;
        gamepad_millis = 0;
        LCDML.OTHER_jumpToFunc(UI_func_song);
      }
    }
    else if (buttons == GAMEPAD_SELECT && key_down() && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_settings))
    {
      LCDML.OTHER_jumpToFunc(UI_func_song); // go back from seq.settings to song
    }

    //else if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_map_gamepad))
    //{

    bool reverse_y = false;
    bool xy_navigation = false;

    if (seq.edit_state || generic_active_function != 0) // is in edit state
      if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_song))
        reverse_y = true;

    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
    {
      if (seq.active_function != 99 || seq.quicknav_song_to_pattern_jump)
        reverse_y = true;
      if (seq.active_function == 99)
        reverse_y = false;
    }

    // if ((LCDML.FUNC_getID() > 5 && LCDML.FUNC_getID() < 9) || (LCDML.FUNC_getID() > 17 && LCDML.FUNC_getID() < 25) || (LCDML.FUNC_getID() > 33 && LCDML.FUNC_getID() < 41))  //"2-line menus", reverse y
    //   reverse_y = true;

    // some pages do x/y navigation using ENC[R]for y and ENC[L] for x movement - depending on that and the edit state of the field, this needs special handling for gamepad usage
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker))
    {

      if (seq.edit_state)
      {
        ;
      }
      else
      {
        xy_navigation = true;
      }
    }

    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song))
    {
      if (seq.edit_state == false && seq.cycle_touch_element == 0)
      {
        reverse_y = false;
      }
      else if (seq.edit_state && seq.cycle_touch_element == 5)
      {
        reverse_y = true;
      }
      else if (seq.edit_state && seq.cycle_touch_element == 6)
      {
        reverse_y = false;
      }
      else if (seq.edit_state && seq.cycle_touch_element == 7)
      {
        reverse_y = true;
      }
      else if (seq.edit_state && seq.cycle_touch_element == 8)
      {
        reverse_y = false;
      }
      else if (seq.edit_state && seq.cycle_touch_element == 9)
      {
        reverse_y = true;
      }
    }

    //Volume control by button interface or USB Gamepad

    else if ((buttons == GAMEPAD_SELECT && key_down()) || (buttons == GAMEPAD_SELECT && key_up()))
    {
      if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_volume))
      {
        if (key_up())
        {
          g_LCDML_CONTROL_Encoder_position[ENC_L] = -4;
          if (gamepad_last_dir == 1)
            gamepad_accelerate = 1 + gamepad_accelerate * 1.4;
          else
            gamepad_accelerate = 0;
          gamepad_millis = 0;
          gamepad_last_dir = 1;
        }
        else if (key_down())
        {
          g_LCDML_CONTROL_Encoder_position[ENC_L] = 4;
          if (gamepad_last_dir == 2)
            gamepad_accelerate = 1 + gamepad_accelerate * 1.4;
          else
            gamepad_accelerate = 0;
          gamepad_millis = 0;
          gamepad_last_dir = 2;
        }
      }
      else if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) &&
        //  LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_map_gamepad) &&
        LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_seq_settings) &&
        LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker) &&
        LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) &&
        LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor) &&
        LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_song))
      {
        LCDML.OTHER_jumpToFunc(UI_func_volume);
      }
    }
    // start gamepad cases

    if (buttons == gamepad_buttons_neutral)
    {
      if (key_up())
      {
        if (reverse_y)
          g_LCDML_CONTROL_Encoder_position[ENC_R] = -4;
        else
          g_LCDML_CONTROL_Encoder_position[ENC_R] = 4;
        if (gamepad_last_dir == 1)
          gamepad_accelerate = 1 + gamepad_accelerate * 1.4;
        else
          gamepad_accelerate = 0;
        gamepad_millis = 0;
        gamepad_last_dir = 1;
      }
      else if (key_down())
      {
        if (reverse_y)
          g_LCDML_CONTROL_Encoder_position[ENC_R] = 4;
        else
          g_LCDML_CONTROL_Encoder_position[ENC_R] = -4;
        if (gamepad_last_dir == 2)
          gamepad_accelerate = 1 + gamepad_accelerate * 1.4;
        else
          gamepad_accelerate = 0;
        gamepad_millis = 0;
        gamepad_last_dir = 2;
      }
      else if (key_right())
      {
        if (xy_navigation)
          g_LCDML_CONTROL_Encoder_position[ENC_L] = -4;
        else
          g_LCDML_CONTROL_Encoder_position[ENC_R] = -4;
        gamepad_accelerate = 0;
        gamepad_millis = 0;
      }
      else if (key_left())
      {
        if (xy_navigation)
          g_LCDML_CONTROL_Encoder_position[ENC_L] = 4;
        else
          g_LCDML_CONTROL_Encoder_position[ENC_R] = 4;
        gamepad_millis = 0;
        gamepad_accelerate = 0;
      }
      else
        gamepad_accelerate = 0;
    }

    // end gamepad cases
 // }
    if (gamepad_accelerate > configuration.sys.gamepad_speed / 1.1)
      gamepad_accelerate = configuration.sys.gamepad_speed / 1.1;

    // GAMEPAD BUTTON HANDLING

   // if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_map_gamepad))
   // {
    if (buttons == GAMEPAD_BUTTON_B)
    {
      button[ENC_L] = 0;
      gamepad_accelerate = 0;
    }
    else if (buttons == GAMEPAD_BUTTON_A)
    {
      button[ENC_R] = 0;
      gamepad_accelerate = 0;
    }
    else if (buttons == GAMEPAD_START && gamepad_millis >= configuration.sys.gamepad_speed * 3)
    {
      gamepad_millis = 0;
      gamepad_accelerate = 0;

      if (!seq.running)
        handleStart();
      else
        handleStop();
    }
    //  }
#if (defined BUTTON_LEDS && defined MCP_23008) || (defined BUTTON_LEDS && defined MCP_23017)
    All_Button_LEDS(false);
#endif
  }

  /************************************************************************************
    Basic encoder handling (from LCDMenuLib2)
   ************************************************************************************/

   // RIGHT
  if (g_LCDML_CONTROL_Encoder_position[ENC_R] <= -3)
  {
    if (!button[ENC_R])
    {
      LCDML.BT_left();
#ifdef DEBUG
      LOG.println(F("ENC-R left"));
#endif
      encoderDir[ENC_R].Left(true);
      g_LCDML_CONTROL_button_prev[ENC_R] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_R] = -1;
    }
    else
    {
#ifdef DEBUG
      LOG.println(F("ENC-R down"));
#endif
      encoderDir[ENC_R].Down(true);
      LCDML.BT_down();
    }
    ENCODER[ENC_R].write(g_LCDML_CONTROL_Encoder_position[ENC_R] + 4);
  }
  else if (g_LCDML_CONTROL_Encoder_position[ENC_R] >= 3)
  {
    if (!button[ENC_R])
    {
#ifdef DEBUG
      LOG.println(F("ENC-R right"));
#endif
      encoderDir[ENC_R].Right(true);
      LCDML.BT_right();
      g_LCDML_CONTROL_button_prev[ENC_R] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_R] = -1;
    }
    else
    {
#ifdef DEBUG
      LOG.println(F("ENC-R up"));
#endif
      encoderDir[ENC_R].Up(true);
      LCDML.BT_up();
    }
    ENCODER[ENC_R].write(g_LCDML_CONTROL_Encoder_position[ENC_R] - 4);
  }
  else
  {
    if (!button[ENC_R] && g_LCDML_CONTROL_button_prev[ENC_R]) // falling edge, button pressed
    {
      encoderDir[ENC_R].ButtonPressed(true);
      g_LCDML_CONTROL_button_prev[ENC_R] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_R] = millis();
    }
    else if (button[ENC_R] && !g_LCDML_CONTROL_button_prev[ENC_R]) // rising edge, button not active
    {
      encoderDir[ENC_R].ButtonPressed(false);
      g_LCDML_CONTROL_button_prev[ENC_R] = HIGH;

      if (g_LCDML_CONTROL_button_press_time[ENC_R] < 0)
      {
        g_LCDML_CONTROL_button_press_time[ENC_R] = millis();
        // Reset for left right action
      }
      else if ((millis() - g_LCDML_CONTROL_button_press_time[ENC_R]) >= LONG_BUTTON_PRESS)
      {
#ifdef DEBUG
        LOG.println(F("ENC-R long released"));
#endif
        // LCDML.BT_quit();
        encoderDir[ENC_R].ButtonLong(false);
      }
      else if ((millis() - g_LCDML_CONTROL_button_press_time[ENC_R]) >= BUT_DEBOUNCE_MS)
      {
#ifdef DEBUG
        LOG.println(F("ENC-R short"));
#endif
        encoderDir[ENC_R].ButtonShort(true);

        LCDML.BT_enter();
      }
    }
  }
  if (encoderDir[ENC_R].ButtonPressed() == true && (millis() - g_LCDML_CONTROL_button_press_time[ENC_R]) >= LONG_BUTTON_PRESS)
  {
#ifdef DEBUG
    LOG.println(F("ENC-R long recognized"));
#endif 
    encoderDir[ENC_R].ButtonLong(true);
    if (ui.handlesButtonLong() || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_voice_select)
      || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_handle_OP) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_microsynth) ||
      (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_custom_mappings) && generic_temp_select_menu == 1))
    { // handle long press ENC_R
      LCDML.BT_enter();
      LCDML.OTHER_updateFunc();
      LCDML.loop_menu();
      encoderDir[ENC_R].ButtonPressed(false);
      encoderDir[ENC_R].ButtonLong(false);
    }
    else
    {
      if (LCDML.FUNC_getID() < 0xff)
        LCDML.FUNC_setGBAToLastFunc();
      else
        LCDML.FUNC_setGBAToLastCursorPos();

      LCDML.OTHER_jumpToFunc(UI_func_voice_select);
      encoderDir[ENC_R].reset();
    }
  }
  // LEFT
  if (g_LCDML_CONTROL_Encoder_position[ENC_L] <= -3)
  {
    if (!button[ENC_L])
    {
#ifdef DEBUG
      LOG.println(F("ENC-L left"));
#endif
      encoderDir[ENC_L].Left(true);
      LCDML.BT_left();
      g_LCDML_CONTROL_button_prev[ENC_L] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_L] = -1;
    }
    else
    {
#ifdef DEBUG
      LOG.println(F("ENC-L down"));
#endif
      encoderDir[ENC_L].Down(true);
      LCDML.BT_down();
      if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_volume))
      {
        // special case : if is in tracker/song edit, left ENC controls x axis, right ENC controls y axis
        if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 0)
        {

          if (seq.cycle_touch_element == 8) // is in song transpose column - junp to pattern editor
          {
            seq.quicknav_song_to_pattern_jump = true;
            LCDML.OTHER_jumpToFunc(UI_func_seq_pattern_editor);
          }
          else
            if (seq.cycle_touch_element < 5 || seq.cycle_touch_element > 9) // is not in song/chain edit
            {
              if (seq.loop_edit_step < 2) // not in loop edit mode
              {
                seq.previous_track = seq.selected_track;
                seq.cursor_scroll_previous = seq.cursor_scroll;
                seq.selected_track++;
                if (seq.loop_edit_step == 1)
                {
                  seq.loop_edit_step = 0;
                  seq.selected_track = 0;
                  print_song_loop_arrows();
                }
                if (seq.selected_track > NUM_SEQ_TRACKS - 1)
                {
                  seq.loop_edit_step = 1;
                  seq.selected_track = 0;
                }
              }
            }
            else if (seq.cycle_touch_element == 5) // is in song edit
            {
              seq.cycle_touch_element = 6; // switch to chain edit
            }
            else if (seq.cycle_touch_element == 6) // is in chain edit
            {
              seq.cycle_touch_element = 8; // switch to transpose edit
            }
          seq.help_text_needs_refresh = true;


        }
        else if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 5) ||
          // select track for instrument or select track for tracktype change
          (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker) && seq.edit_state == false) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) && seq.edit_state == false))

        {

          if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) && seq.edit_state == false)
          {
            if (seq.selected_track == 8)
              seq.selected_track = 0;
            else
            {

              seq.selected_track++;
            }
          }
          else
          {
            if (seq.selected_track == NUM_SEQ_TRACKS - 1)
            {

              seq.selected_track = 0;
            }
            else
            {

              seq.selected_track++;
            }
          }
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker) && seq.edit_state)
        {
          if (seq.vel[seq.current_pattern[seq.selected_track]][seq.scrollpos] < 253)
            seq.vel[seq.current_pattern[seq.selected_track]][seq.scrollpos]++;
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign != 0)
        { // do nothing
          ;
        }
        else if (ui.handlesLeftEncoder())
        { // do nothing
          ;
        }

        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
        {
          if (seq.menu == 18)
          {
            seq.menu_status = 1;
            LCDML.OTHER_jumpToFunc(UI_func_seq_vel_editor);
          }
          else
          {
            seq.menu_status = 0;
            seq.menu = constrain(seq.menu + 1, 0, 18);
          }

        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
        {
          prev_menu_item = seq.menu;
          seq.menu = constrain(seq.menu + 1, 0, 17);
        }
        else if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
          LCDML.OTHER_jumpToFunc(UI_func_volume);
      }
    }
    ENCODER[ENC_L].write(g_LCDML_CONTROL_Encoder_position[ENC_L] + 4);
  }
  else if (g_LCDML_CONTROL_Encoder_position[ENC_L] >= 3)
  {
    if (!button[ENC_L])
    {
#ifdef DEBUG
      LOG.println(F("ENC-L right"));
#endif
      encoderDir[ENC_L].Right(true);
      LCDML.BT_right();
      g_LCDML_CONTROL_button_prev[ENC_L] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_L] = -1;
    }
    else
    {
#ifdef DEBUG
      LOG.println(F("ENC-L up"));
#endif
      encoderDir[ENC_L].Up(true);
      LCDML.BT_up();
      if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_volume))
      {
        // special case : if is in tracker edit, left ENC controls x axis, right ENC controls y axis
        if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 0)
        {
          if (seq.cycle_touch_element < 5 || seq.cycle_touch_element > 9) // is not in song/chain edit
          {
            if (seq.loop_edit_step < 2) // not in loop edit mode
            {
              if (seq.selected_track == 0)
              {
                seq.loop_edit_step = 1;
                seq.selected_track = NUM_SEQ_TRACKS - 1;
              }
              else if (seq.loop_edit_step == 1)
              {
                seq.loop_edit_step = 0;
                seq.selected_track = NUM_SEQ_TRACKS - 1;
                print_song_loop_arrows();
              }
              else
              {
                seq.previous_track = seq.selected_track;
                seq.cursor_scroll_previous = seq.cursor_scroll;
                seq.selected_track--;
              }
            }
          }
          else if (seq.cycle_touch_element == 8) // is in transpose edit
          {
            seq.cycle_touch_element = 6; // switch to chain edit
          }
          else if (seq.cycle_touch_element == 6) // is in chain edit
          {
            seq.cycle_touch_element = 5; // switch to song edit
          }
          seq.help_text_needs_refresh = true;
        }
        else if ((LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 1) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign == 5) ||
          // select track for instrument or select track for tracktype change
          (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker) && seq.edit_state == false) || (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) && seq.edit_state == false))
        {

          if (seq.selected_track == 0)
          {
            if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_MultiSamplePlay) && seq.edit_state == false)
              seq.selected_track = 8;
            else
              seq.selected_track = NUM_SEQ_TRACKS - 1;
          }
          else
            seq.selected_track--;
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_tracker) && seq.edit_state)
        {
          if (seq.vel[seq.current_pattern[seq.selected_track]][seq.scrollpos] > 0)
            seq.vel[seq.current_pattern[seq.selected_track]][seq.scrollpos]--;
        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_song) && seq.tracktype_or_instrument_assign != 0)
        { // do nothing
          ;
        }
        else if (ui.handlesLeftEncoder())
        { // do nothing
          ;
        }

        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
        {
          if (seq.menu == 0 && seq.quicknav_song_to_pattern_jump == true)
          {
            // go back to song-transpose when previously navigated in from song edit
            seq.help_text_needs_refresh = true;
            seq.edit_state = true;
            seq.quicknav_pattern_to_song_jump = true;
            seq.quicknav_song_to_pattern_jump = false;
            LCDML.OTHER_jumpToFunc(UI_func_song);
          }
          else
            seq.menu = constrain(seq.menu - 1, 0, 18);

        }
        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
        {
          if (seq.menu == 0)
          {
            seq.menu_status = 2;
            LCDML.OTHER_jumpToFunc(UI_func_seq_pattern_editor);
          }
          else
          {
            seq.menu_status = 0;
            seq.menu = constrain(seq.menu - 1, 0, 17);
          }

        }
        else if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
          LCDML.OTHER_jumpToFunc(UI_func_volume);
      }
    }
    ENCODER[ENC_L].write(g_LCDML_CONTROL_Encoder_position[ENC_L] - 4);
  }
  else
  {
    if (!button[ENC_L] && g_LCDML_CONTROL_button_prev[ENC_L]) // falling edge, button pressed
    {
      encoderDir[ENC_L].ButtonPressed(true);
      g_LCDML_CONTROL_button_prev[ENC_L] = LOW;
      g_LCDML_CONTROL_button_press_time[ENC_L] = millis();
    }
    else if (button[ENC_L] && !g_LCDML_CONTROL_button_prev[ENC_L]) // rising edge, button not active
    {
      encoderDir[ENC_L].ButtonPressed(false);
      g_LCDML_CONTROL_button_prev[ENC_L] = HIGH;

      if (g_LCDML_CONTROL_button_press_time[ENC_L] < 0)
      {
        g_LCDML_CONTROL_button_press_time[ENC_L] = millis();
        // Reset for left right action
      }
      else if ((millis() - g_LCDML_CONTROL_button_press_time[ENC_L]) >= LONG_BUTTON_PRESS)
      {
#ifdef DEBUG
        LOG.println(F("ENC-L long released"));
#endif
        // encoderDir[ENC_L].ButtonLong(true);
        // LCDML.BT_quit();
      }
      else if ((millis() - g_LCDML_CONTROL_button_press_time[ENC_L]) >= BUT_DEBOUNCE_MS)
      {
        // LCDML.BT_enter();
#ifdef DEBUG
        LOG.println(F("ENC-L short"));
#endif
        encoderDir[ENC_L].ButtonShort(true);

        // damster@09.03.2024: commented out as this bugs initial page back behavior 
        //        if ((LCDML.MENU_getLastActiveFunctionID() == 0xff && LCDML.MENU_getLastCursorPositionID() == 0) || menu_init == true)
        //        {
        //          LCDML.MENU_goRoot();
        //          menu_init = false;
        //        }
        //        else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_volume))
        if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_volume))
        {
          encoderDir[ENC_L].reset();
          encoderDir[ENC_R].reset();

          if (LCDML.MENU_getLastActiveFunctionID() < 0xff)
            LCDML.OTHER_jumpToID(LCDML.MENU_getLastActiveFunctionID());
          else
            LCDML.OTHER_setCursorToID(LCDML.MENU_getLastCursorPositionID());
        }
        else {
          const bool liveSeqGridConsumesBack =
            (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_livesequencer)) &&
            (ui_liveSeq != nullptr) &&
            ui_liveSeq->isGridEditorActive();

          if (liveSeqGridConsumesBack) {
            ui_liveSeq->notifyLeftEncoderShort();
          }
          else {
            LCDML.BT_quit();
            LCDML.SCREEN_resetTimer(); // reset timer on exiting screensaver through back key
          }
        }
      }
    }
  }

  if (encoderDir[ENC_L].ButtonPressed() == true && (millis() - g_LCDML_CONTROL_button_press_time[ENC_L]) >= LONG_BUTTON_PRESS)
  {
#ifdef DEBUG
    LOG.println(F("ENC-L long recognized"));
#endif

    // long left-press starts/stops sequencer

    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_file_manager))
    { // when in filemanager, long push ENC-L goes up one directory
      sd_go_parent_folder();
      sd_update_display();
    }
    else
    {
      toggle_sequencer_play_status();
    }

    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_voice_select))
      print_voice_select_default_help();

    // for (uint8_t i = 0; i < NUM_DEXED; i++)
    //   MicroDexed[i]->panic();

    encoderDir[ENC_L].reset();
    encoderDir[ENC_R].reset();
  }
}

#if defined GLOW
extern int glow_menu_item;
#endif



#ifdef TOUCH_UI
extern void back_touchbutton();
#endif

extern bool disableDirectMIDIinput;

/***********************************************************************
   MENU DISPLAY
 ***********************************************************************/
FLASHMEM void lcdml_menu_clear(void)
{

#ifdef TOUCH_UI
  //
#else
  if (seq.menu_status == 0)
    border1_clear();
#endif

  ts.touch_ui_drawn_in_menu = false;
  back_button_touch_page_check_and_init_done = false;

#if defined GLOW
  glow_menu_item = 9999;
#endif
}

#ifndef TOUCH_UI  //begin regular menu
FLASHMEM void lcdml_menu_display(void)
{
  disableDirectMIDIinput=false;
  
  // update content
  // ***************
  if (LCDML.DISP_checkMenuUpdate())
  {
    // declaration of some variables
    // ***************

    // content variable
    char content_text[_LCDML_DISP_cols]; // save the content text of current menu element
    // menu element object
    LCDMenuLib2_menu* tmp;
    // some limit values
    uint8_t i = LCDML.MENU_getScroll();
    uint8_t maxi = _LCDML_DISP_rows + i;
    uint8_t n = 0;

    // check if this element has children
    if ((tmp = LCDML.MENU_getDisplayedObj()) != NULL)
    {
      // Display a header with the parent element name
      display.setTextSize(1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);

      if (LCDML.MENU_getLayer() == 0)
      {
        // this is displayed when no header is available
       // x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = CHAR_width;
        x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = 0;
        last_menu_depth = LCDML.MENU_getLayer();
        display.console = true;
        display.fillRect(x_pos_menu_header_layer[LCDML.MENU_getLayer()], 7, 90, 7, COLOR_BACKGROUND); //clears longest text of submenus when going back
        display.console = false;
      }
      else if (LCDML.MENU_getLayer() > last_menu_depth)
      {
        display.setCursor(x_pos_menu_header_layer[LCDML.MENU_getLayer()], 7);
        display.setTextColor(RED, COLOR_BACKGROUND);
        display.print(">");
        display.setTextColor(GREY1, COLOR_BACKGROUND);
        LCDML_getContent(content_text, LCDML.MENU_getParentID());
        show_uppercase_no_grid(7, display.getCursorX(), strlen(content_text), content_text);
        // x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = display.getCursorX();
       // if (ts.keyb_in_menu_activated == false)
        //  helptext_l(back_text);
      }
      draw_back_touchbutton();
      display.setTextSize(2);
      seq.edit_state = false;
      generic_active_function = 0;

      // loop to display lines
      do
      {
        // check if a menu element has a condition and if the condition be true
        if (tmp->checkCondition())
        {
          // check the type off a menu element
          if (tmp->checkType_menu() == true)
          {
            // display normal content
            LCDML_getContent(content_text, tmp->getID());

            if (n == LCDML.MENU_getCursorPos())
              display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
            else
              display.setTextColor(GREY2, COLOR_BACKGROUND);
            show(n + 1, 0, display_cols - 2, content_text);
          }
          else
          {
            if (tmp->checkType_dynParam())
            {
              tmp->callback(n);
            }
          }
          // increment some values
          i++;
          n++;
        }
        // try to go to the next sibling and check the number of displayed rows
      } while (((tmp = tmp->getSibling(1)) != NULL) && (i < maxi));

      // clear menu lines if menu content < display lines
      if (i < _LCDML_DISP_rows)
      {
        display.console = true;
        display.fillRect(0, CHAR_height * (i + 1) - 1, CHAR_width * (_LCDML_DISP_cols - 1), CHAR_height * (_LCDML_DISP_rows - i), COLOR_BACKGROUND);
      }
    }
  }
  if (LCDML.DISP_checkMenuCursorUpdate())
  {
    drawScrollbar((_LCDML_DISP_cols - 2) * CHAR_width + CHAR_width_small, CHAR_height, 6, LCDML.MENU_getChilds(), LCDML.MENU_getCursorPosAbs(), CHAR_height);
  }
}

#else  // begin touch_ui 

FLASHMEM void lcdml_menu_display(void)
{

  if (seq.menu_status == 0)
  {
    display.fillScreen(COLOR_BACKGROUND);
    seq.menu_status = 1;
  }

  // update content
  // ***************
  if (LCDML.DISP_checkMenuUpdate())
  {
    // declaration of some variables
    // ***************
    // content variable
    char content_text[_LCDML_DISP_cols]; // save the content text of current menu element
    // menu element object
    LCDMenuLib2_menu* tmp;
    // some limit values
    uint8_t i = LCDML.MENU_getScroll();
    //uint8_t maxi = _LCDML_DISP_rows + i;
    uint8_t maxi = 16;
    uint8_t n = 0;
    uint8_t xpos = 0, ypos = 0;
    uint8_t count_num_buttons = 0;


    // check if this element has children
    if ((tmp = LCDML.MENU_getDisplayedObj()) != NULL)
    {
      // Display a header with the parent element name
      display.setTextSize(1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);

      if (LCDML.MENU_getLayer() == 0)
      {
        // this is displayed when no header is available
       // x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = CHAR_width + CHAR_width_small*4;
        x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = CHAR_width;
        last_menu_depth = LCDML.MENU_getLayer();
        display.console = true;
        //display.fillRect(x_pos_menu_header_layer[LCDML.MENU_getLayer()] + CHAR_width, 7, 90, 7, COLOR_BACKGROUND); //clears longest text of submenus when going back
        display.console = false;
        //if (ts.keyb_in_menu_activated == false)
        //{
          //display.setCursor(0, DISPLAY_HEIGHT - CHAR_height_small);
          //display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          //display.print(back_clear);
        //}
      }
      else if (LCDML.MENU_getLayer() > last_menu_depth)
      {
        // display.setCursor(x_pos_menu_header_layer[LCDML.MENU_getLayer()], 7);
        // display.setTextColor(RED, COLOR_BACKGROUND);
        // display.print(">");
        // display.setTextColor(GREY1, COLOR_BACKGROUND);
        // LCDML_getContent(content_text, LCDML.MENU_getParentID());
        // show_uppercase_no_grid(7, display.getCursorX(), strlen(content_text), content_text);
        // x_pos_menu_header_layer[LCDML.MENU_getLayer() + 1] = display.getCursorX();
       // if (ts.keyb_in_menu_activated == false)
        //  helptext_l(back_text);//xxxyyy
      }
      back_touchbutton();
      display.setTextSize(2);
      seq.edit_state = false;
      generic_active_function = 0;
      // loop to display lines
      do
      {
        // check if a menu element has a condition and if the condition be true
        if (tmp->checkCondition())
        {
          // check the type off a menu element
          if (tmp->checkType_menu() == true)
          {
            // display normal content
            LCDML_getContent(content_text, tmp->getID());
            if (n == LCDML.MENU_getCursorPos())
              draw_button_on_grid_smarttext(xpos * 11 + 2, ypos * 6 + 1, content_text, 1);
            else
              draw_button_on_grid_smarttext(xpos * 11 + 2, ypos * 6 + 1, content_text, 0);
            xpos++;
            if (xpos > 3)
            {
              ypos++;
              xpos = 0;
            }
            count_num_buttons++;
          }
          else
          {
            if (tmp->checkType_dynParam())
            {
              tmp->callback(n);
            }
          }
          i++;
          n++;
        }
        // try to go to the next sibling and check the number of displayed rows
      } while (((tmp = tmp->getSibling(1)) != NULL) && (i < maxi - 1));

      // clear menu buttons if menu content < number touch buttons
      do
      {
        draw_button_on_grid_smarttext(xpos * 11 + 2, ypos * 6 + 1, "", 98); //empty touch button slot
        xpos++;
        if (xpos > 3)
        {
          ypos++;
          xpos = 0;
        }
        count_num_buttons++;
      } while (count_num_buttons < maxi);
    }
  }
}
#endif

FLASHMEM void draw_instance_editor(Editor* editor, bool refresh)
{
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(1);

  if (!refresh)
  {
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 22, 6);
    display.print(F("DEXED"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 25 + 2, DISPLAY_HEIGHT - (CHAR_height_small * 2) - 2);
    display.print(F("SHORT:"));
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
    if (generic_active_function == 1)
      display.print(F("APPLY"));
    else if (generic_active_function == 0)
      display.print(F(" EDIT"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F(" LONG:"));
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
    display.print(F("TOGGLE INST"));
  }
  display.setCursor(CHAR_width_small * 10, 6);
  if (generic_temp_select_menu == editor->select_id)
  {
    setModeColor(editor->select_id);
    helptext_r(F("< > SELECT INSTANCE"));
    display.setTextSize(1);
    if (seq.edit_state == 0)
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    else
      display.setTextColor(COLOR_SYSTEXT, RED);
    display.setCursor(CHAR_width_small * 22, 6);
    display.print(F("DEXED"));
  }
  else
  {
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 22, 6);
    display.print(F("DEXED"));
    helptext_r(F("< > EDIT PARAM."));
  }
  UI_update_instance_icons();
}

FLASHMEM void addInstanceEditor(
  void (*renderer)(Editor* param, bool refresh) = &draw_instance_editor)
{
  ui.addEditor(
    "INSTANCE", 0, 3, &selected_instance_id, NULL,
    [](Editor* editor, int16_t value) -> void
    {
      selected_instance_id = value;
      ui.draw_editors(true);
    },
    renderer);
  ui.instance_select_flag = true;
  ui.enableButtonLongEditor();
}

// FLASHMEM void UI_func_map_gamepad(uint8_t param)
// {
//   if (LCDML.FUNC_setup()) // ****** SETUP *********
//   {
//     encoderDir[ENC_R].reset();
//     seq.temp_active_menu = 0;
//     temp_int = 0;
//     display.fillScreen(COLOR_BACKGROUND);
//     helptext_l(back_text);
//     helptext_r(F("RESTART"));
//     display.setTextSize(1);
//     setCursor_textGrid_small(1, 1);
//     display.setTextColor(RED);
//     display.print(F("AUTO-MAP GAMEPAD KEYS"));
//     display.setTextColor(GREY2);
//     setCursor_textGrid_small(1, 3);
//     display.print(F("PLEASE PUSH EACH BUTTON ONCE"));
//     setCursor_textGrid_small(1, 4);
//     display.print(F("IN THE LISTED ORDER"));

//     setCursor_textGrid_small(1, 6);
//     display.print(F("UP"));
//     setCursor_textGrid_small(1, 7);
//     display.print(F("DOWN"));
//     setCursor_textGrid_small(1, 8);
//     display.print(F("LEFT"));
//     setCursor_textGrid_small(1, 9);
//     display.print(F("RIGHT"));
//     setCursor_textGrid_small(1, 11);
//     display.print(F("BUTTON A"));
//     setCursor_textGrid_small(1, 12);
//     display.print(F("BUTTON B"));
//     setCursor_textGrid_small(1, 14);
//     display.print(F("SELECT"));
//     setCursor_textGrid_small(1, 15);
//     display.print(F("START"));
//     setCursor_textGrid_small(35, 5);
//     display.print(F("NEUTRAL STATE:"));
//     setCursor_textGrid_small(35, 6);
//     display.print(F("BUTTONS"));
//     setCursor_textGrid_small(35, 7);
//     display.print(F("Value 0"));
//     setCursor_textGrid_small(35, 8);
//     display.print(F("Value 1"));
//     setCursor_textGrid_small(46, 6);
//     print_formatted_number(gamepad_buttons_neutral, 3);
//     setCursor_textGrid_small(46, 7);
//     print_formatted_number(gamepad_0_neutral, 3);
//     setCursor_textGrid_small(46, 8);
//     print_formatted_number(gamepad_1_neutral, 3);
//     setCursor_textGrid_small(1, 21);
//     display.print(F("YOU CAN RESTART KEY-MAPPING WITH "));
//     display.setTextColor(RED);
//     display.print(F("ENC[R] "));
//     display.setTextColor(GREY2);
//     display.print(F("AT ANY TIME."));
//   }
//   if (LCDML.FUNC_loop()) // ****** LOOP *********
//   {
//     if (LCDML.BT_checkEnter())
//     {
//       temp_int = 0;
//       display.fillRect(0, 170, DISPLAY_WIDTH, 40, COLOR_BACKGROUND);
//     }
//   }
//   if (LCDML.FUNC_close()) // ****** STABLE END *********
//   {
//     encoderDir[ENC_R].reset();
//     display.fillScreen(COLOR_BACKGROUND);
//   }
// }

FLASHMEM void reverb_roomsize(int8_t change)
{
  configuration.fx.reverb_roomsize = constrain(configuration.fx.reverb_roomsize + change, REVERB_ROOMSIZE_MIN, REVERB_ROOMSIZE_MAX);
  // display_bar_int("Reverb Room", configuration.fx.reverb_roomsize, 1.0, REVERB_ROOMSIZE_MIN, REVERB_ROOMSIZE_MAX, 3, false, false, false);
  reverb.size(mapfloat(configuration.fx.reverb_roomsize, REVERB_ROOMSIZE_MIN, REVERB_ROOMSIZE_MAX, 0.0, 1.0));
}

FLASHMEM void reverb_lowpass(int8_t change)
{
  configuration.fx.reverb_lowpass = constrain(configuration.fx.reverb_lowpass + change, REVERB_LOWPASS_MIN, REVERB_LOWPASS_MAX);
  // display_bar_int("Reverb Lowpass", configuration.fx.reverb_lowpass, 1.0, REVERB_LOWPASS_MIN, REVERB_LOWPASS_MAX, 3, false, false, false);
  reverb.lowpass(mapfloat(configuration.fx.reverb_lowpass, REVERB_LOWPASS_MIN, REVERB_LOWPASS_MAX, 0.0, 1.0));
}

FLASHMEM void reverb_lodamp(int8_t change)
{
  configuration.fx.reverb_lodamp = constrain(configuration.fx.reverb_lodamp + change, REVERB_LODAMP_MIN, REVERB_LODAMP_MAX);
  // display_bar_int("Reverb Lodamp.", configuration.fx.reverb_lodamp, 1.0, REVERB_LODAMP_MIN, REVERB_LODAMP_MAX, 3, false, false, false);
  reverb.lodamp(mapfloat(configuration.fx.reverb_lodamp, REVERB_LODAMP_MIN, REVERB_LODAMP_MAX, 0.0, 1.0));
}

FLASHMEM void reverb_hidamp(int8_t change)
{
  configuration.fx.reverb_hidamp = constrain(configuration.fx.reverb_hidamp + change, REVERB_HIDAMP_MIN, REVERB_HIDAMP_MAX);
  // display_bar_int("Reverb Hidamp.", configuration.fx.reverb_hidamp, 1.0, REVERB_HIDAMP_MIN, REVERB_HIDAMP_MAX, 3, false, false, false);
  reverb.hidamp(mapfloat(configuration.fx.reverb_hidamp, REVERB_HIDAMP_MIN, REVERB_HIDAMP_MAX, 0.0, 1.0));
}

FLASHMEM void reverb_diffusion(int8_t change)
{
  configuration.fx.reverb_diffusion = constrain(configuration.fx.reverb_diffusion + change, REVERB_DIFFUSION_MIN, REVERB_DIFFUSION_MAX);
  // display_bar_int("Reverb Diff.", configuration.fx.reverb_diffusion, 1.0, REVERB_DIFFUSION_MIN, REVERB_DIFFUSION_MAX, 3, false, false, false);
  reverb.diffusion(mapfloat(configuration.fx.reverb_diffusion, REVERB_DIFFUSION_MIN, REVERB_DIFFUSION_MAX, 0.0, 1.0));
}

FLASHMEM void reverb_level(int8_t change)
{
  configuration.fx.reverb_level = constrain(configuration.fx.reverb_level + change, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX);
  // display_bar_int("Reverb Level", configuration.fx.reverb_level, 1.0, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 3, false, false, true);
  master_mixer_r.gain(MASTER_MIX_CH_REVERB, volume_transform(mapfloat(configuration.fx.reverb_level, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, VOL_MAX_FLOAT)));
  master_mixer_l.gain(MASTER_MIX_CH_REVERB, volume_transform(mapfloat(configuration.fx.reverb_level, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, VOL_MAX_FLOAT)));
}

FLASHMEM void master_effects_set_delay_time(uint8_t instance, int8_t change)
{
  if (instance == 0 || instance == 1) {

    if (configuration.fx.delay_sync[instance] > 0)
    {
      set_delay_sync(configuration.fx.delay_sync[instance], instance);
    }

    if (LCDML.BT_checkDown())
    {
      if (configuration.fx.delay_time[instance] == DELAY_TIME_MIN && configuration.fx.delay_sync[instance] > DELAY_SYNC_MIN)
      {
        // MIDI-sync delay
        configuration.fx.delay_sync[instance] = constrain(configuration.fx.delay_sync[instance] - 1, DELAY_SYNC_MIN, DELAY_SYNC_MAX);
      }
      else
      {
        configuration.fx.delay_time[instance] = constrain(configuration.fx.delay_time[instance] + ENCODER[ENC_R].speed(), DELAY_TIME_MIN, DELAY_TIME_MAX);
        MD_sendControlChange(configuration.dexed[instance].midi_channel, 105, configuration.fx.delay_time[instance]);
      }
    }
    else if (LCDML.BT_checkUp())
    {
      if (configuration.fx.delay_time[instance] == DELAY_TIME_MIN && configuration.fx.delay_sync[instance] > DELAY_SYNC_MIN)
      {
        // MIDI-sync delay
        configuration.fx.delay_sync[instance] = constrain(configuration.fx.delay_sync[instance] + 1, DELAY_SYNC_MIN, DELAY_SYNC_MAX);
      }
      else
      {
        if (configuration.fx.delay_time[instance] == DELAY_TIME_MIN)
          configuration.fx.delay_sync[instance] = DELAY_SYNC_MIN + 1;
        else
        {
          configuration.fx.delay_time[instance] = constrain(configuration.fx.delay_time[instance] - ENCODER[ENC_R].speed(), DELAY_TIME_MIN, DELAY_TIME_MAX);
          MD_sendControlChange(configuration.dexed[instance].midi_channel, 105, configuration.fx.delay_time[instance]);
        }
      }
    }
    if (configuration.fx.delay_sync[instance] > 0)
    {
      set_delay_sync(configuration.fx.delay_sync[instance], instance); // MIDI Sync Delay
    }
    else
    {
      if (configuration.fx.delay_time[instance] <= DELAY_TIME_MIN)
        delay_fx[instance]->disable(0);
      else
        delay_fx[instance]->delay(0, constrain(configuration.fx.delay_time[instance], DELAY_TIME_MIN, DELAY_TIME_MAX) * 10);
    }
  }
}

FLASHMEM void master_effects_set_delay_feedback(uint8_t instance, int8_t change)
{
  configuration.fx.delay_feedback[instance] = constrain(configuration.fx.delay_feedback[instance] + change, DELAY_FEEDBACK_MIN, DELAY_FEEDBACK_MAX);
  MD_sendControlChange(configuration.dexed[instance].midi_channel, 106, configuration.fx.delay_feedback[instance]);

  delay_fb_mixer[instance]->gain(1, mapfloat(configuration.fx.delay_feedback[instance], DELAY_FEEDBACK_MIN, DELAY_FEEDBACK_MAX, 0.0, 0.8));
}

FLASHMEM void master_effects_delay_level_global(uint8_t instance, int8_t change)
{
  configuration.fx.delay_level_global[instance] = constrain(configuration.fx.delay_level_global[instance] + change, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
  delay_mixer[instance]->gain(1, mapfloat(configuration.fx.delay_level_global[instance], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.9));
}

FLASHMEM void master_effects_set_delay_panorama(uint8_t instance, int8_t change)
{
  configuration.fx.delay_pan[instance] = constrain(configuration.fx.delay_pan[instance] + change, PANORAMA_MIN, PANORAMA_MAX);
  delay_mono2stereo[instance]->panorama(mapfloat(configuration.fx.delay_pan[instance], PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
}

FLASHMEM void master_effects_set_reverb_send(uint8_t instance, int8_t change)
{
  configuration.fx.reverb_send[instance] = constrain(configuration.fx.reverb_send[instance] + change, REVERB_SEND_MIN, REVERB_SEND_MAX);
  MD_sendControlChange(configuration.dexed[instance].midi_channel, 91, configuration.fx.reverb_send[instance]);

  // display_bar_int("Reverb Send", configuration.fx.reverb_send[instance], 1.0, REVERB_SEND_MIN, REVERB_SEND_MAX, 3, false, false, false);
  reverb_mixer_r.gain(instance, volume_transform(mapfloat(configuration.fx.reverb_send[instance], REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
  reverb_mixer_l.gain(instance, volume_transform(mapfloat(configuration.fx.reverb_send[instance], REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
}

FLASHMEM void UI_func_filter_cutoff(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();

    display_bar_int("Filter Cutoff", configuration.fx.filter_cutoff[selected_instance_id], 1.0, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX, 3, false, false, true);

    UI_update_instance_icons();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.fx.filter_cutoff[selected_instance_id] = constrain(configuration.fx.filter_cutoff[selected_instance_id] + e.dir * e.speed, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX);
      MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 104, configuration.fx.filter_cutoff[selected_instance_id]);
    }
    if (e.pressed) {
      selected_instance_id = !selected_instance_id;
      UI_update_instance_icons();
    }

    display_bar_int("Filter Cutoff", configuration.fx.filter_cutoff[selected_instance_id], 1.0, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX, 3, false, false, false);
    MicroDexed[selected_instance_id]->setFilterCutoff(mapfloat(configuration.fx.filter_cutoff[selected_instance_id], FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX, 1.0, 0.0));
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void UI_func_filter_resonance(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();

    display_bar_int("Filter Reso.", configuration.fx.filter_resonance[selected_instance_id], 1.0, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX, 3, false, false, true);

    UI_update_instance_icons();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.fx.filter_resonance[selected_instance_id] = constrain(configuration.fx.filter_resonance[selected_instance_id] + e.dir * e.speed, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX);
      MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 103, configuration.fx.filter_resonance[selected_instance_id]);
    }
    if (e.pressed) {
      selected_instance_id = !selected_instance_id;
      UI_update_instance_icons();
    }

    display_bar_int("Filter Reso.", configuration.fx.filter_resonance[selected_instance_id], 1.0, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX, 3, false, false, false);

    MicroDexed[selected_instance_id]->setFilterResonance(mapfloat(configuration.fx.filter_resonance[selected_instance_id], FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX, 1.0, 0.0));
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void getNoteName(char* noteName, uint8_t noteNumber)
{
  const char notes[12][3] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
  uint8_t oct_index = noteNumber - 12;
  if (noteNumber == 130) // it is a latched note
  {
    noteName[0] = 'L';
    noteName[1] = '\0';
  }
  else
  {
    noteNumber -= 21;
    if (notes[noteNumber % 12][1] == '\0')
      sprintf(noteName, "%1s-%1d", notes[noteNumber % 12], oct_index / 12);
    else
      sprintf(noteName, "%2s%1d", notes[noteNumber % 12], oct_index / 12);
  }
}

FLASHMEM void UI_func_sound_intensity(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();

    display_bar_int("Voice Level", configuration.dexed[selected_instance_id].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, true);

    UI_update_instance_icons();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.dexed[selected_instance_id].sound_intensity = constrain(configuration.dexed[selected_instance_id].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
      MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 7, configuration.dexed[selected_instance_id].sound_intensity);
    }
    if (e.pressed) {
      selected_instance_id = !selected_instance_id;
      UI_update_instance_icons();
    }

    display_bar_int("Voice Level", configuration.dexed[selected_instance_id].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
    MicroDexed[selected_instance_id]->setGain(midi_volume_transform(map(configuration.dexed[selected_instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127)));
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void UI_func_favorites(uint8_t param)
{
  static uint8_t old_favorites;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    encoderDir[ENC_L].reset();
    old_favorites = configuration.sys.favorites;
    setCursor_textGrid(1, 1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    display.print(F("Favorites"));
    setCursor_textGrid(1, 2);
    switch (configuration.sys.favorites)
    {
    case 0:
      display.print(F("[ All  presets ]"));
      break;
    case 1:
      display.print(F("[  FAVs. only  ]"));
      break;
    case 2:
      display.print(F("[non-FAVs. only]"));
      break;
    case 3:
      display.print(F("[random non-FAV]"));
      break;
    }
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.sys.favorites = constrain(configuration.sys.favorites + e.dir, 0, 3);
    }

    setCursor_textGrid(1, 2);
    switch (configuration.sys.favorites)
    {
    case 0:
      display.print(F("[ All  presets ]"));
      break;
    case 1:
      display.print(F("[  FAVs. only  ]"));
      break;
    case 2:
      display.print(F("[non-FAVs. only]"));
      break;
    case 3:
      display.print(F("[random non-FAV]"));
      break;
    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_L].reset();
    if (old_favorites != configuration.sys.favorites)
    {
      save_sys_flag = true;
      save_sys = 0;
    }
  }
}

FLASHMEM void drawDrumsSampleWave(bool fullRedraw) {
#if defined(COMPILE_FOR_PSRAM) || defined(COMPILE_FOR_PROGMEM)
  static uint16_t lastFromX = 0;
  static uint16_t lastToX = 0;
  const drum_config_t& slot = drum_config[activeSample];
  static constexpr uint16_t plotX = 120;
  static constexpr uint16_t plotY = 19 + 8;
  static constexpr uint16_t plotW = 200;
  static constexpr uint16_t plotH = 63 + 8;

  if (isCustomSample(activeSample)) {
    const CustomSample& s = customSamples[activeSample - NUM_STATIC_PITCHED_SAMPLES];
    const float from = s.start / float(1000);
    const float to = s.end / float(1000);
    const uint16_t fromX = plotX + floor(from * plotW);
    const uint16_t toX = plotX + ceil(to * plotW);

    if (fullRedraw) {
      drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, plotX, fromX, GREY2, GREY4);
      drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, fromX, toX);
      drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, toX, plotX + plotW, GREY2, GREY4);
      lastFromX = fromX;
      lastToX = toX;
    }
    else {
      // from changed
      if (lastFromX != fromX) {
        if (lastFromX < fromX) {
          DBG_LOG(printf("FROM >>: draw from %i to %i\n", lastFromX, fromX));
          drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, lastFromX, fromX, GREY2, GREY4);
        }
        else {
          DBG_LOG(printf("FROM <<: draw from %i to %i\n", fromX, lastFromX));
          drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, fromX, lastFromX);
        }
        lastFromX = fromX;
      }

      // to changed
      if (lastToX != toX) {
        display.console = true;
        if (lastToX < toX) {
          DBG_LOG(printf("UPTO >>: draw from %i to %i\n", lastToX, toX));
          drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, lastToX, toX);
        }
        else {
          DBG_LOG(printf("UPTO <<: draw from %i to %i\n", toX, lastToX));
          drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, toX, lastToX, GREY2, GREY4);
        }
        lastToX = toX;
      }
    }
  }
  else {
    drawSampleWaveform(plotX, plotY, plotW, plotH, (int16_t*)slot.drum_data, slot.len, slot.numChannels, plotX, plotX + plotW);
  }
#endif
}

FLASHMEM void handleCustomSliceEditor(bool isCustom, bool refreshTitle) {
  if (refreshTitle) {
    setCursor_textGrid_small(20, 10);

    if (isCustom) {
      display.setTextColor(RED);
      display.print(F("CUSTOM SAMPLE"));
    }
    else {
      print_empty_spaces(13, 1);
    }
  }
  if (isCustom == false) {
    switch (generic_temp_select_menu) {
    case 16: // FROM
      generic_temp_select_menu = 19;
      ui.draw_editors(false);
      break;
    case 17: // UPTO
    case 18: // TYPE
      generic_temp_select_menu = 15;
      ui.draw_editors(false);
      break;
    }
  }
}

FLASHMEM void sampleStartEndRenderer(Editor* param, bool refresh) {
  const bool isCustom = isCustomSample(activeSample);

  DBG_LOG(printf("sampleStartEndRenderer, refresh %i\n", refresh));
  const bool sampleTypeChanged = lastWasCustomSample != isCustom;
  handleCustomSliceEditor(isCustom, sampleTypeChanged);

  if (isCustom) {
    setCursor_textGrid_small(param->x + 10, param->y);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(param->name);
    print_small_scaled_bar(param->x, param->y, param->get(), param->limit_min, param->limit_max, param->select_id, 1, NULL);
  }
  else if (sampleTypeChanged) {
    setCursor_textGrid_small(param->x, param->y);
    print_empty_spaces(10 + strlen(param->name), true);
  }
}

FLASHMEM void UI_func_stereo_mono(uint8_t param)
{
  static uint8_t old_mono;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    encoderDir[ENC_R].reset();
    old_mono = configuration.sys.mono;

    setCursor_textGrid(1, 1);
    display.print(F("Stereo/Mono"));
    setCursor_textGrid(1, 2);
    switch (configuration.sys.mono)
    {
    case 0:
      display.print(F("[STEREO]"));
      stereo2mono.stereo(true);
      break;
    case 1:
      display.print(F("[MONO  ]"));
      stereo2mono.stereo(false);
      break;
    case 2:
      display.print(F("[MONO-R]"));
      stereo2mono.stereo(false);
      break;
    case 3:
      display.print(F("[MONO-L]"));
      stereo2mono.stereo(false);
      break;
    }
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.sys.mono = constrain(configuration.sys.mono + e.dir, MONO_MIN, MONO_MAX);
    }

    setCursor_textGrid(1, 2);
    switch (configuration.sys.mono)
    {
    case 0:
      display.print(F("[STEREO]"));
      stereo2mono.stereo(true);
      break;
    case 1:
      display.print(F("[MONO  ]"));
      stereo2mono.stereo(false);
      break;
    case 2:
      display.print(F("[MONO-R]"));
      stereo2mono.stereo(false);
      break;
    case 3:
      display.print(F("[MONO-L]"));
      stereo2mono.stereo(false);
      break;
    }
    set_volume(configuration.sys.vol, configuration.sys.mono);
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_L].reset();

    if (old_mono != configuration.sys.mono)
    {
      save_sys_flag = true;
      save_sys = 0;
    }
  }
}

// get and set dexed configuration to our dexed configuration structure
FLASHMEM int16_t dexed_current_instance_getter(Editor* editor)
{
  // the controller parameter may be from either instance, which may be
  // switched at any time. So recompute the value pointer in respect of the instance!
  uint8_t* ptr = (uint8_t*)((char*)editor->value - (char*)&configuration.dexed[0] + (char*)&configuration.dexed[selected_instance_id]);
  return *ptr;
}
FLASHMEM void dexed_current_instance_setter(Editor* editor, int16_t value)
{
  // the controller parameter may be from either instance, which may be
  // switched at any time. So recompute the value pointer in respect of the instance!
  uint8_t* ptr = (uint8_t*)((char*)editor->value - (char*)&configuration.dexed[0] + (char*)&configuration.dexed[selected_instance_id]);
  *ptr = (uint8_t)value;
}

// prepare rendering for an editor field providing multiple options to select between
FLASHMEM void prepare_multi_options(Editor* editor, bool refresh)
{
  display.setTextSize(1);
  if (!refresh)
  {
    setCursor_textGrid_small(editor->x + 10, editor->y);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(editor->name);
  }

  setModeColor(editor->select_id);
  setCursor_textGrid_small(editor->x, editor->y);
  ui.setCursor(editor->x, editor->y);
}

// set portamento setup to dexed engine and send SysEx for it.
FLASHMEM void dexed_portamento_setter(Editor* editor, int16_t value)
{
  dexed_current_instance_setter(editor, value);
  dexed_t& dexed = configuration.dexed[selected_instance_id];
  MicroDexed[selected_instance_id]->setPortamento(dexed.portamento_mode, dexed.portamento_glissando, dexed.portamento_time);
  send_sysex_param(dexed.midi_channel, 67, dexed.portamento_mode, 2);
  send_sysex_param(dexed.midi_channel, 68, dexed.portamento_glissando, 2);
  send_sysex_param(dexed.midi_channel, 69, dexed.portamento_time, 2);
};

// display the voice name for the currently selected instance
// used by mutliple UI pages.
FLASHMEM void dexed_voice_name_renderer(Editor* param, bool refresh)
{
  draw_instance_editor(param, refresh);
  display.setTextSize(2);
  show(1, 1, 10, g_voice_name[selected_instance_id]);
}

FLASHMEM int16_t fx_current_instance_getter(struct Editor* editor)
{
  // the parameter may be from either instance, which may be
  // switched at any time. So recompute the value pointer in respect of the instance!
  uint8_t* ptr = &((uint8_t*)editor->value)[selected_instance_id];
  return *ptr;
}

FLASHMEM void fx_current_instance_setter(struct Editor* editor, int16_t value)
{
  // the parameter may be from either instance, which may be
  // switched at any time. So recompute the value pointer in respect of the instance!
  uint8_t* ptr = &((uint8_t*)editor->value)[selected_instance_id];
  *ptr = (uint8_t)value;
}

FLASHMEM void UI_func_dexed_audio(uint8_t param)
{

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    ui.reset();
    ui.setCursor(1, 1);
    addInstanceEditor(&dexed_voice_name_renderer);

    ui.setCursor(1, 4);
    ui.printLn("DEXED AUDIO SETUP");
    ui.printLn("");
    ui.addEditor("VOLUME", SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, &configuration.dexed[0].sound_intensity,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);
        MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 7, value);
        MicroDexed[selected_instance_id]->setGain(midi_volume_transform(map(value, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127))); });

    // pan: custom getter and setter to center pan around 0 for a nice pan bar
    const int16_t pan_center = (PANORAMA_MAX + PANORAMA_MIN) / 2;
    ui.addEditor(
      "PAN", (int16_t)(PANORAMA_MIN - pan_center), (int16_t)(PANORAMA_MAX - pan_center), &configuration.dexed[0].pan,
      [](Editor* editor) -> int16_t
      {
        return dexed_current_instance_getter(editor) - pan_center; // center around 0
      },
      [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value + pan_center);
        MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 10, map(value + pan_center, PANORAMA_MIN, PANORAMA_MAX, 0, 127));
        if (configuration.sys.mono == 0)
        {
          dexed_mono2stereo[selected_instance_id]->panorama(mapfloat(value + pan_center, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
        }
      });

    ui.printLn("");
    ui.printLn("CHORUS", RED);
    ui.printLn("");
    ui.addEditor("FREQUENCY", CHORUS_FREQUENCY_MIN, CHORUS_FREQUENCY_MAX, &configuration.fx.chorus_frequency[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        chorus_modulator[selected_instance_id]->frequency(value / 20.0); });
    ui.addEditor("WAVEFORM", CHORUS_WAVEFORM_MIN, CHORUS_WAVEFORM_MAX, &configuration.fx.chorus_waveform[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        chorus_modulator[selected_instance_id]->begin(value == 1 ? WAVEFORM_SINE : WAVEFORM_TRIANGLE); },
      [](Editor* editor, bool refresh)
      {
        prepare_multi_options(editor, refresh);
        if (!editor->get())
          display.print(F("[TRIANGLE]"));
        else
          display.print(F("[SINE    ]"));
        //         ui.print(editor->get() ? "[SINE    ]" : "[TRIANGLE]");  //not working correctly
      });
    ui.addEditor("DEPTH", CHORUS_DEPTH_MIN, CHORUS_DEPTH_MAX, &configuration.fx.chorus_depth[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        chorus_modulator[selected_instance_id]->amplitude(value / 100.0); });
    ui.addEditor("LEVEL", CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX, &configuration.fx.chorus_level[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 93, value);
        dexed_chorus_mixer_r[selected_instance_id].gain(1, mapfloat(configuration.fx.chorus_level[selected_instance_id], CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX, 0.0, 0.5));
        dexed_chorus_mixer_l[selected_instance_id].gain(1, mapfloat(configuration.fx.chorus_level[selected_instance_id], CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX, 0.0, 0.5));
      });
    ui.printLn("");
    ui.printLn("EFFECTS", RED);
    ui.printLn("");
    ui.addEditor("DELAY SEND", DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, &configuration.fx.delay_level1[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        set_dexed_delay_level(0, selected_instance_id, value);
      });
    ui.addEditor("REVERB SEND", REVERB_SEND_MIN, REVERB_SEND_MAX, &configuration.fx.reverb_send[0],
      &fx_current_instance_getter, [](Editor* editor, int16_t value)
      {
        fx_current_instance_setter(editor, value);
        MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 91, value);
        reverb_mixer_l.gain(selected_instance_id, volume_transform(mapfloat(value, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
        reverb_mixer_r.gain(selected_instance_id, volume_transform(mapfloat(value, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT))); });
    ui.addEditor("DELAY TO REVERB", REVERB_SEND_MIN, REVERB_SEND_MAX, &configuration.fx.delay_to_reverb[0],
      &fx_current_instance_getter, &fx_current_instance_setter);

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    ui.clear();
  }
}

FLASHMEM void note_name_renderer(struct Editor* editor, bool refresh)
{
  prepare_multi_options(editor, refresh);
  char note_name[4];
  getNoteName(note_name, editor->get());
  display.print("[");
  display.print(note_name);
  display.print("]");
}

// UI page to allow editing of all global dexed parameters
// this somehow resebles the "Function" edit plane on an DX7 instrument.
//
FLASHMEM void UI_func_dexed_setup(uint8_t param)
{

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    ui.reset();
    ui.setCursor(1, 1);
    // allow switching the currently displayed instance
    addInstanceEditor(&dexed_voice_name_renderer);

    ui.setCursor(1, 4);
    ui.printLn("DEXED INSTANCE SETUP");
    ui.printLn("");

    ui.printLn("MIDI");
    ui.printLn("");
    ui.addEditor("MIDI CHANNEL", MIDI_CHANNEL_MIN, MIDI_CHANNEL_MAX + 1, &configuration.dexed[0].midi_channel, &dexed_current_instance_getter, dexed_current_instance_setter);
    ui.addEditor("LOWEST NOTE", INSTANCE_LOWEST_NOTE_MIN, INSTANCE_LOWEST_NOTE_MAX, &configuration.dexed[0].lowest_note,
      &dexed_current_instance_getter, &dexed_current_instance_setter, &note_name_renderer);
    ui.addEditor("HIGHEST NOTE", INSTANCE_HIGHEST_NOTE_MIN, INSTANCE_HIGHEST_NOTE_MAX, &configuration.dexed[0].highest_note,
      &dexed_current_instance_getter, &dexed_current_instance_setter, &note_name_renderer);
    ui.printLn("");

    ui.printLn("POLYPHONY");
    ui.printLn("");
    ui.addEditor(
      "POLY/MONO", MONOPOLY_MIN, MONOPOLY_MAX, &configuration.dexed[0].monopoly,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);

        MicroDexed[selected_instance_id]->setMonoMode(value);
      },
      [](struct Editor* editor, bool refresh)
      {
        prepare_multi_options(editor, refresh);
        if (editor->get())
          display.print(F("[MONO]"));
        else
          display.print(F("[POLY]"));

      });
    ui.addEditor("POLYPHONY", POLYPHONY_MIN, POLYPHONY_MAX, &configuration.dexed[0].polyphony,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);
        MicroDexed[selected_instance_id]->setMaxNotes(value); });
    //ui.printLn("");
    ui.setCursor(27, 4);
    ui.printLn("TUNING");
    ui.printLn("");
    ui.addEditor("TRANSPOSE", TRANSPOSE_MIN, TRANSPOSE_MAX, &configuration.dexed[0].transpose,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);
        MicroDexed[selected_instance_id]->setTranspose(value);
        MicroDexed[selected_instance_id]->notesOff();
        send_sysex_param(configuration.dexed[selected_instance_id].midi_channel, 144, value, 0); });
    ui.addEditor("FINE TUNE", TUNE_MIN, TUNE_MAX, &configuration.dexed[0].tune,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);
        MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 94, value); });
    ui.printLn("");

    ui.printLn("PORTAMENTO", RED);
    ui.printLn("");
    ui.addEditor("MODE", PORTAMENTO_MODE_MIN, PORTAMENTO_MODE_MAX, &configuration.dexed[0].portamento_mode,
      &dexed_current_instance_getter, &dexed_portamento_setter, [](struct Editor* editor, bool refresh)
      {
        prepare_multi_options(editor, refresh);
        uint8_t mode = editor->get();
        uint8_t monopoly = configuration.dexed[selected_instance_id].monopoly;
        if (!mode && monopoly) display.print("[RETAIN]");
        if (!mode && !monopoly) display.print("[FINGER]");
        if (mode && monopoly) display.print("[FOLLOW]");
        if (mode && !monopoly) display.print("[FULL  ]"); });
    ui.addEditor("GLISSANDO", PORTAMENTO_GLISSANDO_MIN, PORTAMENTO_GLISSANDO_MAX, &configuration.dexed[0].portamento_glissando,
      &dexed_current_instance_getter, &dexed_portamento_setter);
    ui.addEditor("TIME", PORTAMENTO_TIME_MIN, PORTAMENTO_TIME_MAX, &configuration.dexed[0].portamento_time,
      &dexed_current_instance_getter, &dexed_portamento_setter);
    ui.printLn("");

    ui.printLn("INTERNALS", RED);
    ui.printLn("");
    ui.addEditor(
      "NOTE REFRESH", NOTE_REFRESH_MIN, NOTE_REFRESH_MAX, &configuration.dexed[0].note_refresh,
      &dexed_current_instance_getter, [](Editor* editor, int16_t value)
      {
        dexed_current_instance_setter(editor, value);
        MicroDexed[selected_instance_id]->setNoteRefreshMode(value); },
      [](struct Editor* editor, bool refresh)
      {
        prepare_multi_options(editor, refresh);
        if (!editor->get())
          display.print(F("[NORMAL ]"));
        else
          display.print(F("[RETRIG.]"));
      });
    ui.addEditor("VELOCITY LEVEL", VELOCITY_LEVEL_MIN, VELOCITY_LEVEL_MAX, &configuration.dexed[0].velocity_level,
      &dexed_current_instance_getter, &dexed_current_instance_setter);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    ui.clear();
  }
}

FLASHMEM void UI_handle_OP(uint8_t param)
{
  static uint8_t op_selected;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    encoderDir[ENC_R].reset();
    helptext_r(F("LONG PUSH - CHANGE INSTANCE"));
    display.setCursor(CHAR_width_small * 22, 6);
    display.setTextSize(1);
    display.print(F("DEXED"));
    UI_update_instance_icons();
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("OP Enable"));
    for (uint8_t i = 0; i < 6; i++)
    {
      display.setTextColor(GREY2);
      setCursor_textGrid(i * 2 + 1, 3);
      display.print(i + 1);
    }
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      op_selected = constrain(op_selected + e.dir, 0, 5);
    }

    if (e.longPressed) {
      if (selected_instance_id < NUM_DEXED - 1) {
        selected_instance_id++;
      }
      else {
        selected_instance_id = 0;
      }
      UI_update_instance_icons();
    }

    if (e.pressed) {
      if (bitRead(configuration.dexed[selected_instance_id].op_enabled, op_selected)) {
        bitClear(configuration.dexed[selected_instance_id].op_enabled, op_selected);
      }
      else {
        bitSet(configuration.dexed[selected_instance_id].op_enabled, op_selected);
      }
    }
    for (uint8_t i = 0; i < 6; i++)
    {
      if (i == op_selected) {
        display.setTextColor(COLOR_SYSTEXT, DARKGREEN);
      }
      else {
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      }

      setCursor_textGrid(i * 2 + 1, 4);

      if (bitRead(configuration.dexed[selected_instance_id].op_enabled, i)) {
        display.print("1");
      }
      else {
        display.print("0");
      }
    }

    UI_update_instance_icons();
    MicroDexed[selected_instance_id]->setOPAll(configuration.dexed[selected_instance_id].op_enabled);
    MicroDexed[selected_instance_id]->doRefreshVoice();
    send_sysex_param(configuration.dexed[selected_instance_id].midi_channel, 155, configuration.dexed[selected_instance_id].op_enabled, 0);
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void pattern_preview_in_probability_editor(uint8_t line, uint8_t patternno)
{
  display.setTextSize(1);
  seq.active_pattern = patternno;
  display.setTextColor(COLOR_SYSTEXT, COLOR_CHORDS);
  if (patternno == 99)
  {
    display.print(F("EMPTY "));
  }
  display.setTextColor(GREY1, GREY4);
  display.print("[");
  for (uint8_t i = 0; i < 16; i++)
  {
    if (seq.vel[patternno][i] > 209)
      display.setTextColor(COLOR_PITCHSMP, GREY4);
    else
    {
      if (seq.content_type[patternno] == 0) // Drumpattern
        display.setTextColor(COLOR_DRUMS, GREY4);
      else if (seq.content_type[patternno] == 1) // Instrument Pattern
        display.setTextColor(COLOR_INSTR, GREY4);
      else if (seq.content_type[patternno] == 2 || seq.content_type[patternno] == 3) //  chord or arp pattern
        display.setTextColor(COLOR_CHORDS, GREY4);
    }
    if (patternno == 99)
      display.print(F(" "));
    else
      display.print(seq_find_shortname(i)[0]);
  }
  display.setTextColor(GREY1, GREY4);
  display.print("]");
}

FLASHMEM void print_probabilities()
{
  for (uint8_t y = 0; y < 14; y++)
  {
    display.setTextSize(1);
    if (y == temp_int - generic_temp_select_menu && generic_menu == 0)
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    else
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(1, y + 6);
    print_formatted_number(y + generic_temp_select_menu, 2);
    display.print(F("        "));
    setCursor_textGrid_small(11, y + 6);
    if (y == temp_int - generic_temp_select_menu && generic_menu == 1)
      display.setTextColor(RED, GREY4);
    print_formatted_number(seq.pat_chance[y + generic_temp_select_menu], 3);
    display.print(F(" %"));
    if (y == temp_int - generic_temp_select_menu && generic_menu == 1)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("    "));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(20, y + 6);
    if (y == temp_int - generic_temp_select_menu && generic_menu == 2)
      display.setTextColor(RED, GREY4);
    else if (y == temp_int - generic_temp_select_menu && generic_menu == 0)
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    else
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    print_formatted_number(seq.pat_vel_variation[y + generic_temp_select_menu], 3);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    if (y == temp_int - generic_temp_select_menu && generic_menu == 2)
      display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F(" STP"));
    setCursor_textGrid_small(29, y + 6);
    pattern_preview_in_probability_editor(y + 6, y + generic_temp_select_menu);
  }

  // scrollbar
  drawScrollbar(DISPLAY_WIDTH - 6 - CHAR_width_small, 6 * (CHAR_height_small + 2), 14, NUM_SEQ_PATTERN, temp_int, CHAR_height_small + 2);
}

FLASHMEM void UI_func_seq_probabilities(uint8_t param)
{
  display.setTextSize(1);

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    temp_int = 0;
    generic_temp_select_menu = 0;
    generic_menu = 0;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    helptext_l(back_text);
    helptext_r(F("SELECT PATTERN"));
    display.setCursor(1 * CHAR_width_small, DISPLAY_HEIGHT - CHAR_height_small * 3);
    display.setTextColor(COLOR_INSTR);
    display.print(F("INSTR  "));
    display.setTextColor(COLOR_DRUMS);
    display.print(F("DRUM/SMP  "));
    display.setTextColor(COLOR_PITCHSMP);
    display.print(F("PITCHED SAMPLE  "));
    display.setTextColor(COLOR_CHORDS);
    display.print(F("CHORD  "));
    display.setTextColor(COLOR_ARP);
    display.print(F("ARP"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(1, 1);
    display.print(F("SEQUENCER TRIGGER"));
    setCursor_textGrid_small(1, 2);
    display.print(F("PROBABILITIES"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    setCursor_textGrid_small(20, 0);
    display.print(F("T"));
    setCursor_textGrid_small(20, 1);
    display.print(F("P"));
    setCursor_textGrid_small(20, 2);
    display.print(F("N"));
    print_live_probability_pattern_info();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(1, 4);
    display.print(F("PATTERN   CHANCE   VEL.+-   PREVIEW"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (generic_menu == 0) {
        if (e.down) {
          temp_int = constrain(temp_int + 1, 0, NUM_SEQ_PATTERN - 1);
          if (generic_temp_select_menu < NUM_SEQ_PATTERN - 1 - 13 && temp_int > 13) {
            generic_temp_select_menu++;
          }
        }
        else { // up
          temp_int = constrain(temp_int - 1, 0, NUM_SEQ_PATTERN - 1);
          if (generic_temp_select_menu > 0) {
            generic_temp_select_menu--;
          }
        }
      }
      else if (generic_menu == 1) {
        seq.pat_chance[temp_int] = constrain(seq.pat_chance[temp_int] + e.dir, 0, 100);
      }
      else if (generic_menu == 2) {
        seq.pat_vel_variation[temp_int] = constrain(seq.pat_vel_variation[temp_int] + e.dir, 0, 100);
      }
    }

    if (e.pressed) {
      generic_menu++;
      if (generic_menu > 2) {
        generic_menu = 0;
      }

      if (generic_menu == 0)
        helptext_r(F("SELECT PATTERN"));
      else if (generic_menu == 1)
        helptext_r(F("SET PROBABILITY"));
      else if (generic_menu == 2)
        helptext_r(F("SET VEL. VARIATION"));
    }

    print_probabilities();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void print_custom_mappings()
{
  display.setTextSize(1);
  uint8_t line = 9;
  int offset = generic_temp_select_menu - 3;
  if (offset < 10 || offset >= 99)
    offset = 0;
  if (offset > 9)
    offset = generic_temp_select_menu - 3 - 9;
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  for (uint8_t y = 0 + offset; y < 10 + offset; y++)
  {
    display.setCursor(1 * CHAR_width_small, line * 12);

    if (generic_temp_select_menu - 3 == y)
      display.setTextColor(COLOR_SYSTEXT, RED);
    else
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    print_formatted_number((y + 1), 2); // entry no.
    if (custom_midi_map[y].type == 0)
    {
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      show_no_grid(line * 12, 5 * CHAR_width_small, 7, "NONE");
    }
    else if (custom_midi_map[y].type == 1)
    {
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      show_no_grid(line * 12, 5 * CHAR_width_small, 7, "KEY/PAD");
    }
    else if (custom_midi_map[y].type == 2)
    {
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      show_no_grid(line * 12, 5 * CHAR_width_small, 7, "MIDI CC");
    }
    else if (custom_midi_map[y].type == 3)
    {
      display.setTextColor(PINK, COLOR_BACKGROUND);
      show_no_grid(line * 12, 5 * CHAR_width_small, 7, "UI KEY ");
    }
    display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
    show_no_grid(line * 12, 14 * CHAR_width_small, 3, custom_midi_map[y].in);
    display.setTextColor(COLOR_CHORDS, COLOR_BACKGROUND);
    show_no_grid(line * 12, 19 * CHAR_width_small, 3, custom_midi_map[y].out);
    display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
    show_no_grid(line * 12, 24 * CHAR_width_small, 3, custom_midi_map[y].channel);

    if (generic_temp_select_menu - 3 == y)
      display.setTextColor(COLOR_SYSTEXT, RED);
    // else
    //   display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else if (custom_midi_map[y].in == 0)
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    else if (custom_midi_map[y].type == 1)
      display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
    else if (custom_midi_map[y].type == 2)
      display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
    else if (custom_midi_map[y].type == 3)
      display.setTextColor(PINK, COLOR_BACKGROUND);

    if (custom_midi_map[y].in == 0)
      show_no_grid(line * 12, 34 * CHAR_width_small, 14, "EMPTY SLOT");
    else if (custom_midi_map[y].type == 1)
    {

      show_no_grid(line * 12, 34 * CHAR_width_small, 14, get_drum_name_from_note(custom_midi_map[y].out));
    }
    else if (custom_midi_map[y].type == 2)
    {

      for (uint8_t i = 0; i < sizeof(cc_dest_values); i++)
      {
        if (custom_midi_map[y].out == cc_dest_values[i])
        {
          show_no_grid(line * 12, 34 * CHAR_width_small, 14, cc_names[i]);
        }
      }
    }
    else if (custom_midi_map[y].type == 3)
    {
      for (uint8_t i = 0; i < sizeof(cc_dest_values_UI_mapping); i++)
      {
        if (custom_midi_map[y].out == cc_dest_values_UI_mapping[i])
        {
          show_no_grid(line * 12, 34 * CHAR_width_small, 14, cc_names_UI_mapping[i]);
        }
      }
    }
    line++;
  }
}

FLASHMEM void print_custom_mapping_drums()
{
  display.fillRect(14 * CHAR_width + 10, 5, 20, 9, COLOR_BACKGROUND);
  display.setTextSize(1);
  setCursor_textGrid_small(1, 6);
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("SET YOUR MIDI DEVICE TO DRUM CH. "));
  display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
  display.print(F("["));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(drum_midi_channel);
  display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
  display.print(F("]        "));
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 7);
  display.print(F("TOUCH "));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("LEARN "));
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("TO REMAP YOUR FAVORITE DRUMS "));
}

FLASHMEM void print_custom_mapping_cc()
{
  UI_update_instance_icons();
  display.setTextSize(1);
  setCursor_textGrid_small(1, 6);
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("MAP MIDI CC TO DEXED PARAMETERS, INSTANCE "));
  display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
  display.print(F("["));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(selected_instance_id + 1);
  display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
  display.print(F("] "));
  setCursor_textGrid_small(1, 7);
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("SWITCH INSTANCES WITH "));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("LONG PUSH "));
  display.setTextColor(RED, COLOR_BACKGROUND);
  display.print(F("ENCODER_R   "));
}

FLASHMEM void print_custom_mapping_ui()
{
  display.fillRect(14 * CHAR_width + 10, 5, 20, 9, COLOR_BACKGROUND);
  display.setTextSize(1);
  setCursor_textGrid_small(1, 6);
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("MAP/ADD CUSTOM MIDI KEYS FOR MIDI UI CONTROL "));
  setCursor_textGrid_small(1, 7);
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  display.print(F("USEFUL WHEN YOU CAN'T SEND DEFAULT CC VALUES"));
}

FLASHMEM void print_mapping_help_text()
{
  if (seq.edit_state == false && generic_temp_select_menu == 0)
    print_custom_mapping_drums();
  else if (seq.edit_state == false && generic_temp_select_menu == 1)
    print_custom_mapping_cc();
  else if (seq.edit_state == false && generic_temp_select_menu == 2)
    print_custom_mapping_ui();
}

FLASHMEM void draw_scrollbar_custom_mappings()
{
  if (generic_temp_select_menu > 2)
    drawScrollbar(DISPLAY_WIDTH - CHAR_width_small * 2, 9 * 12, 10, NUM_CUSTOM_MIDI_MAPPINGS, generic_temp_select_menu - 3, 12);
  else
    drawScrollbar(DISPLAY_WIDTH - CHAR_width_small * 2, 9 * 12, 10, NUM_CUSTOM_MIDI_MAPPINGS, 0, 12);
  print_custom_mappings();
}

FLASHMEM void UI_func_custom_mappings(uint8_t param)
{
  char displayname[8] = { 0, 0, 0, 0, 0, 0, 0 };

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_custom_mappings);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    generic_menu = 0;
    generic_temp_select_menu = 0;
    generic_active_function = 0;

    seq.edit_state = false;
    helptext_l(back_text);
    helptext_r(F("SELECT MAPPING TYPE"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(1);
    draw_button_on_grid(36, 1, "PREV.", "", 0);
    drawBitmap(CHAR_width_small * 38 + 4, CHAR_height * 1 + 8, special_chars[19], 8, 8, GREEN);
    draw_button_on_grid(45, 1, "MIDI", "LEARN", 0);
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(1, 9);
    display.print(F("NO  TYPE     IN   OUT  CH.  NAME"));
    print_custom_mapping_drums();
    print_custom_mappings();
    draw_scrollbar_custom_mappings();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state) {
        switch (generic_temp_select_menu) {
        case 0:
          selectSample(findSmartFilteredSample(activeSample, e.dir));
          break;

        case 1:
          generic_menu = constrain(generic_menu + e.dir, 0, MAX_CC_DEST - 1);
          break;

        case 2:
          generic_menu = constrain(generic_menu + e.dir, 0, 7);
          break;
        }
      }
      else {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 2 + NUM_CUSTOM_MIDI_MAPPINGS);
        print_mapping_help_text();
        draw_scrollbar_custom_mappings();
      }
    }
    if (e.longPressed) {
      selected_instance_id = !selected_instance_id;
      print_custom_mapping_cc();
    }
    else if (e.pressed)
    {
      if (generic_temp_select_menu < 3)
      {
        seq.edit_state = !seq.edit_state;

        if (seq.edit_state == false)
          helptext_r(F("SELECT MAPPING TYPE"));
        else
        {
          generic_menu = 0;
          helptext_r(F("SELECT DESTINATION"));
        }
        display.setTextSize(2);
      }
      if (generic_temp_select_menu > 2)
      { // is in data line

        custom_midi_map[generic_temp_select_menu - 3].type = 0;
        custom_midi_map[generic_temp_select_menu - 3].in = 0;
        custom_midi_map[generic_temp_select_menu - 3].out = 0;
        custom_midi_map[generic_temp_select_menu - 3].channel = 0;
        print_custom_mappings();
      }
    }

    display.setTextSize(2);

    setCursor_textGrid_small(3, 1);
    if (generic_temp_select_menu == 0)
    {
      setModeColor(0);
      display.print(F("NOTES/DRUMS"));
    }
    else if (generic_temp_select_menu == 1)
    {
      setModeColor(1);
      display.print(F("MIDI CC    "));
    }
    else if (generic_temp_select_menu == 2)
    {
      setModeColor(2);
      display.print(F("NOTES TO UI"));
      if (seq.edit_state == false)
        helptext_r(F("SELECT MAPPING TYPE"));
      else
        helptext_r(F("SELECT DESTINATION"));
    }
    display.setTextSize(2);
    if (generic_temp_select_menu < 3)
    {
      if (seq.edit_state)
      {
        display.setTextColor(GREY2, COLOR_BACKGROUND);
        setCursor_textGrid_small(1, 1);
        display.print(" ");
        setCursor_textGrid_small(25, 1);
        display.print(" ");

        display.setTextColor(COLOR_SYSTEXT, RED);
        setCursor_textGrid_small(1, 3);
        display.print("[");
        setCursor_textGrid_small(9, 3);
        display.print("]");
      }
      else
      {
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
        setCursor_textGrid_small(1, 1);
        display.print("[");
        setCursor_textGrid_small(25, 1);
        display.print("]");

        display.setTextColor(GREY2, COLOR_BACKGROUND);
        setCursor_textGrid_small(1, 3);
        display.print(" ");
        setCursor_textGrid_small(9, 3);
        display.print(" ");
      }
    }
    if (generic_temp_select_menu == 3)
    { // gone from menu to data lines

      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid_small(1, 1);
      display.print(" ");
      setCursor_textGrid_small(25, 1);
      display.print(" ");
      helptext_r(F("DELETE MAPPING"));
    }

    display.setTextSize(2);
    if (generic_temp_select_menu == 0)
    {
      setCursor_textGrid_small(3, 3);
      snprintf_P(displayname, sizeof(displayname), PSTR("%03d"), activeSample);
      display.print(displayname);

      // display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      show_no_grid(4 * CHAR_height_small - 2, 11 * CHAR_width_small, 12, basename(drum_config[activeSample].name));
    }
    else if (generic_temp_select_menu == 1)
    {

      setCursor_textGrid_small(3, 3);
      snprintf_P(displayname, sizeof(displayname), PSTR("%03d"), cc_dest_values[generic_menu]);
      display.print(displayname);
      show_no_grid(4 * CHAR_height_small - 2, 11 * CHAR_width_small, 12, cc_names[generic_menu]);
    }
    else if (generic_temp_select_menu == 2)
    {

      setCursor_textGrid_small(3, 3);
      snprintf_P(displayname, sizeof(displayname), PSTR("%03d"), cc_dest_values_UI_mapping[generic_menu]);
      display.print(displayname);
      show_no_grid(4 * CHAR_height_small - 2, 11 * CHAR_width_small, 12, cc_names_UI_mapping[generic_menu]);
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    seq.midi_learn_active = false;
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
  }
}

// MIDI channel renderer
// just a "small bar" but showing "OMN" for zero value and "OFF" for (virtual) channel 17
FLASHMEM void midi_channel_renderer(Editor* editor, bool refresh)
{
  display.setTextSize(1);
  if (!refresh)
  {
    setCursor_textGrid_small(editor->x + 10, editor->y);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(editor->name);
  }
  if (editor->get() == MIDI_CHANNEL_OFF)
    print_small_scaled_bar(editor->x, editor->y, editor->get(), 0, 17, editor->select_id, 1, (const char*)F("OFF"));
  else
    print_small_scaled_bar(editor->x, editor->y, editor->get(), 0, 17, editor->select_id, 1, (const char*)F("OMN"));
}

// the drum parameter editors depend on the currently selected sample activesample.
// So we need custom getters and setters respecting activesample.
FLASHMEM void addDrumParameterEditor(const char* name, int16_t limit_min, int16_t limit_max, float32_t* valuePtr)
{
  ui.addCustomEditor(
    name, limit_min, limit_max, valuePtr,
    [](Editor* editor) -> int16_t
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      float32_t* ptr = (float32_t*)((char*)editor->value - (char*)&drum_config[0] + (char*)&drum_config[activeSample]);
      return round(*ptr * 100.0f);
    },
    [](Editor* editor, int16_t value)
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      float32_t* ptr = (float32_t*)((char*)editor->value - (char*)&drum_config[0] + (char*)&drum_config[activeSample]);
      *ptr = value / 100.0f;
    },
    NULL);
}

FLASHMEM void addSampleParameterEditor(const char* name, int16_t limit_min, int16_t limit_max, int16_t* valuePtr, void (* const renderer)(Editor* param, bool refresh))
{
  ui.addCustomEditor(
    name, limit_min, limit_max, valuePtr,
    [](Editor* editor) -> int16_t
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      if (isCustomSample(activeSample)) {
        int16_t* ptr = (int16_t*)((char*)editor->value - (char*)&customSamples[0] + (char*)&customSamples[activeSample - NUM_STATIC_PITCHED_SAMPLES]);
        return *ptr;
      }
      else {
        return 0;
      }
    },
    [](Editor* editor, int16_t value)
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      if (isCustomSample(activeSample)) {
        const CustomSample* s = &customSamples[activeSample - NUM_STATIC_PITCHED_SAMPLES];
        int16_t* ptr = (int16_t*)((char*)editor->value - (char*)&customSamples[0] + (char*)s);

        // limit to start point must be smaller than end point 
        if (ptr == &s->start) { // start point
          *ptr = std::min(value, int16_t(s->end - 1));
        }
        else { // end point
          *ptr = std::max(value, int16_t(s->start + 1));
        }
        sampleStartEndRenderer(editor, true);
        drawDrumsSampleWave(false); // partial redraw
      }
    },
    renderer);
}

FLASHMEM void addSampleParameterEditor_uint8_t(const char* name, int16_t limit_min, int16_t limit_max, uint8_t* valuePtr, void (* const renderer)(Editor* param, bool refresh))
{
  ui.addCustomEditor(
    name, limit_min, limit_max, valuePtr,
    [](Editor* editor) -> int16_t
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      if (isCustomSample(activeSample)) {
        uint8_t* ptr = (uint8_t*)((char*)editor->value - (char*)&customSamples[0] + (char*)&customSamples[activeSample - NUM_STATIC_PITCHED_SAMPLES]);
        return *ptr;
      }
      else {
        return 0;
      }
    },
    [](Editor* editor, int16_t value)
    {
      // the parameter may be from either sample, which may be
      // switched at any time. So recompute the value pointer in respect of to activesample !
      if (isCustomSample(activeSample)) {
        uint8_t* ptr = (uint8_t*)((char*)editor->value - (char*)&customSamples[0] + (char*)&customSamples[activeSample - NUM_STATIC_PITCHED_SAMPLES]);
        *ptr = value;
      }
    },
    renderer);
}

FLASHMEM void addDrumParameterEditor_uint8_t(const char* name, int16_t limit_min, int16_t limit_max, uint8_t* valuePtr, void (* const renderer)(Editor* param, bool refresh) = NULL)
{
  ui.addCustomEditor(
    name, limit_min, limit_max, valuePtr,
    [](Editor* editor) -> int16_t
    {
      uint8_t* ptr = (uint8_t*)((uint8_t*)editor->value - (uint8_t*)&drum_config[0] + (uint8_t*)&drum_config[activeSample]);
      return *ptr;
    },
    [](Editor* editor, int16_t value)
    {
      uint8_t* ptr = (uint8_t*)((uint8_t*)editor->value - (uint8_t*)&drum_config[0] + (uint8_t*)&drum_config[activeSample]);
      *ptr = static_cast<uint8_t>(value);
    },
    renderer);
}

FLASHMEM void addDrumParameterEditor_int8_t(const char* name, int16_t limit_min, int16_t limit_max, int8_t* valuePtr)
{
  ui.addCustomEditor(
    name, limit_min, limit_max, valuePtr,
    [](Editor* editor) -> int16_t
    {
      int8_t* ptr = (int8_t*)((uint8_t*)editor->value - (uint8_t*)&drum_config[0] + (uint8_t*)&drum_config[activeSample]);
      return *ptr;
    },
    [](Editor* editor, int16_t value)
    {
      int8_t* ptr = (int8_t*)((uint8_t*)editor->value - (uint8_t*)&drum_config[0] + (uint8_t*)&drum_config[activeSample]);
      *ptr = static_cast<int8_t>(value);
    },
    NULL);
}

void drawSampleWaveform(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
  int16_t* data, uint32_t length, uint8_t numChannels,
  uint16_t fromX, uint16_t toX,
  uint16_t waveColor, uint16_t bgColor) {
  if (toX > fromX) {
    DBG_LOG(printf("DRAW from %i to %i\n", fromX, toX));

    if ((toX - fromX) > 1) {
      display.console = true;
      display.fillRect(fromX, y, toX - fromX, h, bgColor);
    }
    else {
      // workaround fillRect not working correctly for width = 1
      for (uint16_t xLine = fromX; xLine < toX; xLine++) {
        display.console = true;
        display.drawLine(xLine, y, xLine, y + h, bgColor);
      }
    }

    if (w > 0 && data != nullptr) {
      const uint8_t spacing = 4; // stereo looks better with spacing for loud waves
      const float scaleY = (h - spacing) / float(UINT16_MAX * numChannels);
      const uint16_t maxDitherIgnore = 1 / scaleY;
      uint16_t binSize = length / w / numChannels;
      const float blockSize = length / float(w);

      int16_t binMin[numChannels];
      int16_t binMax[numChannels];

      // loop thru pixels
      for (uint16_t xi = fromX; xi <= toX; xi++) {

        uint32_t dataIndex = (xi - x) * blockSize;
        dataIndex &= ~(numChannels - 1); // round down to numCh!

        for (uint8_t ch = 0; ch < numChannels; ch++) {
          binMin[ch] = INT16_MAX;
          binMax[ch] = INT16_MIN;
        }

        // loop thru pixel samples
        for (uint16_t ps = 0; ps < binSize; ps++) {

          // loop thru channels
          for (uint8_t ch = 0; ch < numChannels; ch++) {
            binMin[ch] = std::min(binMin[ch], data[dataIndex + ch]);
            binMax[ch] = std::max(binMax[ch], data[dataIndex + ch]);
          }
          dataIndex += numChannels;
        }

        for (uint8_t ch = 0; ch < numChannels; ch++) {
          const uint16_t yZero = y + (h / (2 * numChannels)) + ch * (h / numChannels); // find zero for upper and lower plot

          if ((binMax[ch] - binMin[ch]) < maxDitherIgnore) {
            binMax[ch] = binMin[ch]; // avoid drawing 2px height for dithering noise (eg. silence)
          }

          const uint16_t yMax = yZero + binMax[ch] * scaleY;
          const uint16_t yMin = yZero + binMin[ch] * scaleY;

          display.console = true;
          display.drawLine(xi, yMin, xi, yMax, waveColor); // drawLine() is off a bit to the left          
        }
      }
    }
    else {
      const uint16_t yZero = y + (h / 2);
      display.drawLine(x, yZero, x + w, yZero, waveColor); // horizontal zero line
    }
  }
  flush_sysex();
}

FLASHMEM void selectSample(uint8_t sample) {
  const uint8_t newSample = constrain(sample, 0, NUM_DRUMSET_CONFIG - 2);
  if (newSample != activeSample) {
    activeSample = newSample;

    if (drumScreenActive) {
      ui.draw_editors(true);  // force redraw all parameters as sample changed
      drawDrumsSampleWave(true);
    }
    lastWasCustomSample = isCustomSample(activeSample);
  }
}

FLASHMEM uint8_t findSmartFilteredSample(uint8_t sample, int8_t direction) {
  uint8_t result = sample;

  if (direction != 0) {
    direction = constrain(direction, -1, +1);

    if (seq.smartfilter && direction != 0) {
      bool found = false;

      for (int16_t note = sample + direction; found == false; note += direction)
      {
        const bool outOfBounds = (note < 0) || (note > NUM_DRUMSET_CONFIG - 2);
        if (outOfBounds) {
          break;
        }
        for (uint8_t pattern = 0; (found == false) && (pattern < NUM_SEQ_PATTERN); pattern++)
        {
          if (seq.content_type[pattern] == 0)
          {
            for (uint8_t step = 0; step < 16; step++)
            {
              found = (drum_config[note].midinote == seq.note_data[pattern][step] && seq.vel[pattern][step] < 210)
                || (drum_config[note].midinote == seq.vel[pattern][step] && seq.vel[pattern][step] > 209);
              if (found) {
                result = note;
                break;
              }
            }
          }
        }
      }
    }
    else {
      const uint8_t boundary = (direction > 0) ? NUM_DRUMSET_CONFIG - 2 : 0;
      if (sample != boundary) {
        result = sample + direction;
      }
    }
  }
  return result;
}

FLASHMEM void checkDrawFilterText(bool forceDraw = false) {
  const bool isFilterSelected = (generic_temp_select_menu == 13); // adapt number when drums page layout changes
  if (forceDraw || isFilterSelected) {
    display.setTextSize(1);
    setCursor_textGrid_small(10, 17);
    display.setTextColor(GREY1, COLOR_BACKGROUND);

    switch (drum_config[activeSample].filter_mode) {
    case 0:
      display.print(F("OFF     "));
      break;
    case 1:
      display.print(F("LOWPASS "));
      break;
    case 2:
      display.print(F("BANDPASS"));
      break;
    case 3:
      display.print(F("HIGHPASS"));
      break;
    }
  }
}

FLASHMEM void drum_page_show_sample_size() {

  uint32_t sample_size_bytes = drum_config[activeSample].len * sizeof(int16_t);
  setCursor_textGrid_small(46, 10);
  display.setTextSize(1);

  if (sample_size_bytes >= 1024 * 1024) {
    display.setTextColor(RED, COLOR_BACKGROUND);
    uint32_t size_mb_main = sample_size_bytes / (1024 * 1024);
    uint32_t size_mb_rem = (sample_size_bytes % (1024 * 1024)) / (1024 * 1024 / 100);
    display.printf("%2lu.%02lu MB", size_mb_main, size_mb_rem);
  }
  else {
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    uint32_t size_kb = sample_size_bytes / 1024;
    display.printf("%4lu KB", size_kb);
  }
}

#ifdef COMPILE_FOR_PSRAM

FLASHMEM void trimSample() {
  TouchButton::drawButton(GRID.X[2], GRID.Y[5], "TRIM", "SAMPLE", TouchButton::BUTTON_RED);
  uint8_t targetSlot = activeSample;

  if (!isCustomSample(targetSlot) || drum_config[targetSlot].drum_data == nullptr) {
    return;
  }

  // Get the current sample length from drum_config
  uint32_t currentSampleLen = drum_config[targetSlot].len;

  // Correctly map the start and end points from the 0-1000 range to the actual sample length
  uint32_t startSampleIndex = map(customSamples[targetSlot - NUM_STATIC_PITCHED_SAMPLES].start, 0, 1000, 0, currentSampleLen);
  uint32_t endSampleIndex = map(customSamples[targetSlot - NUM_STATIC_PITCHED_SAMPLES].end, 0, 1000, 0, currentSampleLen);

  // Ensure the end index is valid and not before the start
  if (endSampleIndex <= startSampleIndex || endSampleIndex > currentSampleLen) {
    return;
  }

  uint32_t newLen = endSampleIndex - startSampleIndex;
  size_t newBytes = newLen * sizeof(int16_t);

  uint8_t* newTrimmedData = (uint8_t*)extmem_malloc(newBytes);

  if (newTrimmedData) {
    uint8_t* sourcePtr = (uint8_t*)drum_config[targetSlot].drum_data + (startSampleIndex * sizeof(int16_t));

    memcpy(newTrimmedData, sourcePtr, newBytes);

    extmem_free(const_cast<uint8_t*>(drum_config[targetSlot].drum_data));
    drum_config[targetSlot].drum_data = nullptr;

    drum_config[targetSlot].drum_data = newTrimmedData;
    drum_config[targetSlot].len = newLen;

    customSamples[targetSlot - NUM_STATIC_PITCHED_SAMPLES].start = 0;
    customSamples[targetSlot - NUM_STATIC_PITCHED_SAMPLES].end = 1000;

    drawDrumsSampleWave(true);
    drum_page_show_sample_size();
  }
  delay(80);
  TouchButton::drawButton(GRID.X[2], GRID.Y[5], "TRIM", "SAMPLE", TouchButton::BUTTON_ACTIVE);
}

extern FLASHMEM void saveCustomSampleToSdCard(uint8_t sample_id);

FLASHMEM void saveSample() {
  TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SAMPLE", TouchButton::BUTTON_RED);

  uint8_t targetSlot = activeSample;
  if (isCustomSample(targetSlot) && drum_config[targetSlot].drum_data != nullptr) {
    saveCustomSampleToSdCard(targetSlot);
  }
  delay(80);
  TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SAMPLE", TouchButton::BUTTON_ACTIVE);

}

FLASHMEM void fadeSample() {
  TouchButton::drawButton(GRID.X[3], GRID.Y[5], "FADE", "OUT", TouchButton::BUTTON_RED);
  uint8_t targetSlot = activeSample;

  if (!isCustomSample(targetSlot) || drum_config[targetSlot].drum_data == nullptr) {
    return;
  }

  uint32_t numSamples = drum_config[targetSlot].len;
  uint32_t numChannels = drum_config[targetSlot].numChannels;

  // The total number of sample frames (pairs for stereo)
  uint32_t numFrames = numSamples / numChannels;

  // Calculate the start point for the fade-out based on frames
  uint32_t fadeStartFrame = numFrames * 0.50;

  if (fadeStartFrame >= numFrames) {
    return;
  }

  int16_t* data = (int16_t*)drum_config[targetSlot].drum_data;
  uint32_t fadeLengthFrames = numFrames - fadeStartFrame;

  for (uint32_t i = fadeStartFrame; i < numFrames; i++) {
    // Calculate the position in the fade (0.0 at the start to 1.0 at the end)
    float fadePos = (float)(i - fadeStartFrame) / fadeLengthFrames;

    // Calculate the linear gain, which goes from 1.0 to 0.0
    float gain = 1.0f - fadePos;

    // Apply the gain to each channel in the current frame
    for (uint32_t c = 0; c < numChannels; c++) {
      data[i * numChannels + c] = (int16_t)((float)data[i * numChannels + c] * gain);
    }
  }

  drawDrumsSampleWave(true);
  delay(80);
  TouchButton::drawButton(GRID.X[3], GRID.Y[5], "FADE", "OUT", TouchButton::BUTTON_ACTIVE);
}

void _draw_drum_page_dynamic_touch_buttons()
{
  if (isCustomSample(activeSample) && drum_config[activeSample].len != 0)
  {
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "TRIM", "SAMPLE", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[3], GRID.Y[5], "FADE", "OUT", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SAMPLE", TouchButton::BUTTON_ACTIVE);
  }
  else
  {
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "TRIM", "SAMPLE", TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[3], GRID.Y[5], "FADE", "OUT", TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SAMPLE", TouchButton::BUTTON_NORMAL);
  }
}

FLASHMEM void drum_page_drawButtonsOnStateChange() {
  // A static variable to remember the previous state.
  // It's initialized only once.
  static bool lastIsCustomSample = false;

  // Get the current state
  bool currentIsCustomSample = isCustomSample(activeSample) && drum_config[activeSample].len != 0;

  // Only redraw if the state has changed
  if (currentIsCustomSample != lastIsCustomSample) {
    // Redraw all buttons to reflect the new state
    _draw_drum_page_dynamic_touch_buttons();

    // Update the static variable for the next loop iteration
    lastIsCustomSample = currentIsCustomSample;
  }
}
#endif

// Create drum UI.
// The drum UI needs to be redrawn if activesample changes.
FLASHMEM void UI_func_drums(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_drums);
    drumScreenActive = true;
    ui.reset();
    ui.clear(); // just recreate UI without resetting selection / edit mode

    display.setTextColor(GREY2, COLOR_BACKGROUND);
    for (uint8_t d = 0; d < NUM_DRUMS; d++)
    {
      display.setCursor(122 + (d * 2 * CHAR_width_small), 18);
      display.print("-");
    }

    ui.setCursor(0, 0);
    drawDrumsSampleWave(true); //otherwise it does not draw a custom sample when entering the page
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_ACTIVE);
#ifdef COMPILE_FOR_PSRAM
    _draw_drum_page_dynamic_touch_buttons(); //only force draw on init
#endif
    drum_page_show_sample_size();

    // the drum sample slot selector.
    // changes activesample and redraws all other editors as they depend on.
    ui.addEditor((const char*)F(""), 0, NUM_DRUMSET_CONFIG - 2,
      [](Editor* editor) -> int16_t {
        return activeSample;
      },
      [](Editor* editor, int16_t value) {
        selectSample(findSmartFilteredSample(activeSample, (value - activeSample)));
      },
      [](Editor* editor, bool refresh) { // TODO: still called twice on editor change
        // render the current selected drum sample slot number and name
        char header[27] = { " " };
        sprintf(header, "%02i:  %s", activeSample, basename(drum_config[activeSample].name));

        display.setTextSize(2);
        setModeColor(editor->select_id);
        show(editor->y, editor->x, 27, header);
#if defined GLOW
        //set_glow_show_text_no_grid(5 * CHAR_width, 0, 18, basename(drum_config[activeSample].name), 0, 2);
#endif
        display.setTextSize(1);
        display.setCursor(276, 6);
        display.print((drum_config[activeSample].numChannels == 1) ? "  MONO" : "STEREO");
        display.setCursor(234, 18);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        print_sample_type(1);
        checkDrawFilterText(true); // force redraw filter text on active sample change
#ifdef COMPILE_FOR_PSRAM
        drum_page_drawButtonsOnStateChange();
#endif
        drum_page_show_sample_size();
        flush_sysex();
      });

    ui.setCursor(0, 3);

    // the parameter editors depend on activesample. We define them by sample index 0,
    // but they will act on current slot activesample later.

    addDrumParameterEditor_uint8_t((const char*)F("VOLUME"), 0, 200, &drum_config[0].vol_max);
    addDrumParameterEditor_int8_t((const char*)F("PAN"), -99, 99, &drum_config[0].pan);
    addDrumParameterEditor_uint8_t((const char*)F("REVERB"), 0, 100, &drum_config[0].reverb_send);
    addDrumParameterEditor_uint8_t((const char*)F("DELAY1"), 0, 100, &drum_config[0].delay1);
    addDrumParameterEditor_uint8_t((const char*)F("DELAY2"), 0, 100, &drum_config[0].delay2);
    addDrumParameterEditor_uint8_t((const char*)F("PITCH"), 1, 200, &drum_config[0].pitch);
    addDrumParameterEditor_uint8_t((const char*)F("TUNE"), 0, 200, &drum_config[0].p_offset);

    flush_sysex();

    setCursor_textGrid_small(0, 10);
    display.setTextColor(RED);
    display.print("ENVELOPE");

    ui.setCursor(0, 11);
#if (NUM_DEXED>2)
    addDrumParameterEditor_uint8_t((const char*)F("ATTACK"), 0, 100, &drum_config[0].env_attack);
    addDrumParameterEditor_uint8_t((const char*)F("HOLD"), 0, 100, &drum_config[0].env_hold);
    addDrumParameterEditor_uint8_t((const char*)F("DECAY"), 0, 100, &drum_config[0].env_decay);
    addDrumParameterEditor_uint8_t((const char*)F("SUSTAIN"), 0, 100, &drum_config[0].env_sustain);
    addDrumParameterEditor_uint8_t((const char*)F("RELEASE"), 0, 100, &drum_config[0].env_release);
#else
    addDrumParameterEditor_uint8_t((const char*)F("ATTACK"), 0, 0, &drum_config[0].env_attack);
    addDrumParameterEditor_uint8_t((const char*)F("HOLD"), 0, 0, &drum_config[0].env_hold);
    addDrumParameterEditor_uint8_t((const char*)F("DECAY"), 0, 0, &drum_config[0].env_decay);
    addDrumParameterEditor_uint8_t((const char*)F("SUSTAIN"), 0, 0, &drum_config[0].env_sustain);
    addDrumParameterEditor_uint8_t((const char*)F("RELEASE"), 0, 0, &drum_config[0].env_release);
#endif

    flush_sysex();

    setCursor_textGrid_small(0, 16);
    display.setTextColor(RED);
    display.print("FILTER");
    ui.setCursor(0, 17);
#if (NUM_DEXED>2)
    addDrumParameterEditor_uint8_t("", 0, 3, &drum_config[0].filter_mode);
    addDrumParameterEditor_uint8_t((const char*)F("FRQ"), 0, 100, &drum_config[0].filter_freq);
    ui.setCursor(0, 19);
    addDrumParameterEditor_uint8_t((const char*)F("Q"), 0, 100, &drum_config[0].filter_q);
#else
    addDrumParameterEditor_uint8_t("FILTER", 0, 0, &drum_config[0].filter_mode);
    addDrumParameterEditor_uint8_t((const char*)F("FRQ"), 0, 0, &drum_config[0].filter_freq);
    ui.setCursor(0, 19);
    addDrumParameterEditor_uint8_t((const char*)F("Q"), 0, 0, &drum_config[0].filter_q);
#endif

    flush_sysex();

    ui.setCursor(20, 11);
    addSampleParameterEditor((const char*)F("STARTPOINT"), 0, 1000, &customSamples[0].start, sampleStartEndRenderer);
    addSampleParameterEditor((const char*)F("ENDPOINT"), 0, 1000, &customSamples[0].end, sampleStartEndRenderer);

    handleCustomSliceEditor(isCustomSample(activeSample), true); // force update custom sample title

    addDrumParameterEditor_uint8_t("TYPE", 1, 11, &drum_config[0].drum_class, [](Editor* param, bool refresh) {
      if (isCustomSample(activeSample)) {
        print_small_scaled_bar(param->x, param->y, param->get(), param->limit_min, param->limit_max, param->select_id, 1, NULL);
        setCursor_textGrid_small(param->x + 10, param->y);
        display.setTextColor(GREY1, COLOR_BACKGROUND);
        print_sample_type(0);

        if (refresh) {
          display.setCursor(234, 18);
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          print_sample_type(1);
        }
      }
      else {
        setCursor_textGrid_small(param->x, param->y);
        print_empty_spaces(22, 1);
      }
      });

    flush_sysex();

    // TODO: add support for loop types. sample should loop until noteOff event
    /*addSampleParameterEditor_uint8_t((const char*)F("LOOP"), 0, 2, &customSamples[0].loopType, [](Editor *param, bool refresh) {
      const std::string names[] = { "NONE", "REPEAT", "PINGPONG" };
      if(isCustomSample(activeSample)) {

        setCursor_textGrid_small(param->x, param->y);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        display.printf("%s", param->name);

        setModeColor(param->select_id);
        setCursor_textGrid_small(param->x + 5, param->y);
        const std::string s = names[param->get()];
        display.print(s.c_str());
        print_empty_spaces(8 - s.length(), 1);
      }
      else {
        setCursor_textGrid_small(param->x, param->y);
        print_empty_spaces(10, 1);
      }
    });*/

    display.setTextColor(RED);
    setCursor_textGrid_small(20, 16);
    display.print("GLOBAL");

    ui.setCursor(20, 17);
    ui.addEditor((const char*)F("DRM MAIN VOLUME"), 0, 100,
      [](Editor* editor) -> int16_t
      {
        return round(mapfloat(seq.drums_volume, 0.0, VOL_MAX_FLOAT, 0., 100.));
      },
      [](Editor* editor, int16_t value)
      {
        seq.drums_volume = mapfloat(value, 0., 100., 0.0, VOL_MAX_FLOAT);
        master_mixer_r.gain(MASTER_MIX_CH_DRUMS, volume_transform(seq.drums_volume));
        master_mixer_l.gain(MASTER_MIX_CH_DRUMS, volume_transform(seq.drums_volume));
      });
    ui.addEditor((const char*)F("DRM MIDI CHANNEL"), 0, 18, &drum_midi_channel, NULL, NULL, midi_channel_renderer);

    ui.setCursor(20, 19);
    ui.addEditor((const char*)F("SMART FILTER"), 0, 1,
      [](Editor* editor) -> int16_t
      {
        return seq.smartfilter;
      },
      [](Editor* editor, int16_t value)
      {
        seq.smartfilter = !seq.smartfilter;
      });

    flush_sysex();

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();
    handleCustomSliceEditor(isCustomSample(activeSample), false); // jump over custom sample fields if not visible
    checkDrawFilterText();
    flush_sysex();

  }
  if (LCDML.FUNC_close()) // ****** CLOSE *********
  {
    unregisterTouchHandler();
    drumScreenActive = false;
    ui.clear();
  }
}

extern float get_sample_p_offset(uint8_t sample);
extern void set_sample_pitch(uint8_t sample, float playbackspeed);

FLASHMEM void pattern_editor_play_current_step(uint8_t step)
{
  if (seq.running == false && seq.note_data[seq.active_pattern][step] > 0 && step < 16)
  {
    if (seq.content_type[seq.active_pattern] == 0)
    { // Drumtrack

      if (seq.vel[seq.active_pattern][step] > 209) // it is a pitched sample
      {
        // Drum[slot]->setPlaybackRate( pow (2, (inNote - 72) / 12.00) * drum_config[sample].pitch ); get_sample_vol_max(sample)
        set_sample_pitch(seq.vel[seq.active_pattern][step] - 210, (float)pow(2, (seq.note_data[seq.active_pattern][step] - 72) / 12.00) * get_sample_p_offset(seq.vel[seq.active_pattern][step] - 210));
        handleNoteOn(drum_midi_channel, seq.vel[seq.active_pattern][step], 90, 0);
      }
      else // else play normal drum sample
        handleNoteOn(drum_midi_channel, seq.note_data[seq.active_pattern][step], seq.vel[seq.active_pattern][step], 0);
    }
    else if (seq.content_type[seq.active_pattern] == 3)
    { // Slices
      handleNoteOn(slices_midi_channel, seq.note_data[seq.active_pattern][step], seq.vel[seq.active_pattern][step], 0);
    }
    else  // is not a sample, preview note with epiano by default
    {
      handleNoteOn(configuration.epiano.midi_channel, seq.note_data[seq.active_pattern][step], seq.vel[seq.active_pattern][step], 0);

      handleNoteOff(configuration.epiano.midi_channel, seq.note_data[seq.active_pattern][step], 0, 0);
    }
  }
}


FLASHMEM void addAutoUpdateParameterEditor_uint8_t(const char* name, uint8_t minv, uint8_t maxv, uint8_t* value) {
  ui.addEditor(name, minv, maxv, value,
    nullptr, // Use default getter
    [](Editor* editor, int16_t value) {
      *((uint8_t*)editor->value) = value;
    }
  );
}


#ifdef COMPILE_FOR_PSRAM

FLASHMEM void print_filter_ringing_warning()
{
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, GREY3);
  setCursor_textGrid_small(25, 2);
  display.print(F("WARNING!"));
  setCursor_textGrid_small(25, 4);
  display.print(F("FILTER RINGING DETECTED:"));
  setCursor_textGrid_small(25, 5);
  display.print(F("REDUCE FILTER FREQ. OR Q."));
}

extern bool isSampleDangerous();

// ===================== Waveform drawing ==================================
FLASHMEM void drawNoiseMakerWaveform(bool slot_or_temp) {

  static constexpr uint16_t plotX = 138;
  static constexpr uint16_t plotY = 5;
  static constexpr uint16_t plotW = 182;
  static constexpr uint16_t plotH = 60;

  if (slot_or_temp == false)//draw slot
  {
    uint8_t targetSlot = NUM_STATIC_PITCHED_SAMPLES + noisemaker_custom_slot;
    if (drum_config[targetSlot].drum_data == nullptr && drum_config[targetSlot].len == 0) {
      // Draw empty waveform area
      display.fillRect(plotX, plotY, plotW, plotH, COLOR_BACKGROUND);
      display.drawRect(plotX, plotY, plotW, plotH, GREY3);

      // Display "No data" message
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.setTextSize(1);
      display.setCursor(plotX + (plotW - 40) / 2, plotY + (plotH - 8) / 2);
      display.print("NO DATA");
      return;
    }

    // Check if we're viewing a custom sample or a generated preview
    else if (noisemaker_custom_slot >= 0 && noisemaker_custom_slot < NUM_CUSTOM_SAMPLES) {

      ////NUM_STATIC_PITCHED_SAMPLES +noisemaker_custom_slot
      // Draw the custom sample waveform if it exists
      if (drum_config[targetSlot].drum_data != nullptr && drum_config[targetSlot].len > 0) {
        drawSampleWaveform(plotX, plotY, plotW, plotH,
          (int16_t*)drum_config[targetSlot].drum_data,
          drum_config[targetSlot].len,
          drum_config[targetSlot].numChannels,
          plotX, plotX + plotW);

      }

    }
  }
  else //use the generated preview (not saved yet)
  {
    // if ((const uint8_t*)nm_preview_data != NULL && nm_preview_len > 0) {

  // Check if we have valid preview data
    if (!nm_preview_data || nm_preview_len == 0) {
      // Draw empty waveform area
      display.fillRect(plotX, plotY, plotW, plotH, COLOR_BACKGROUND);
      display.drawRect(plotX, plotY, plotW, plotH, GREY3);

      // Display "No data" message
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.setTextSize(1);
      display.setCursor(plotX + (plotW - 40) / 2, plotY + (plotH - 8) / 2);
      display.print("NO DATA");
      return;
    }

    // Check if we're viewing a custom sample or a generated preview
    else if (noisemaker_custom_slot >= 0 && noisemaker_custom_slot < NUM_CUSTOM_SAMPLES) {
      // uint8_t targetSlot = NUM_STATIC_PITCHED_SAMPLES + noisemaker_custom_slot;

       // Draw the custom sample waveform if it exists
      // if (drum_config[targetSlot].drum_data != nullptr && drum_config[targetSlot].len > 0) {
      drawSampleWaveform(plotX, plotY, plotW, plotH,
        (int16_t*)(const uint8_t*)nm_preview_data,
        nm_preview_len,
        (nm_params.stereo_mode == 0) ? 1 : 2,
        plotX, plotX + plotW);

    }
  }
  if (isSampleDangerous())
    print_filter_ringing_warning();

}

FLASHMEM void print_noisemaker_synth_note()
{
  if (nm_params.category == NM_SYNTH)
  {
    setCursor_textGrid_small(10, 15);
    char note_name[4];
    getNoteName(note_name, nm_params.synth_midi_note);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(note_name);
  }
}

FLASHMEM void print_noisemaker_synth_wave()
{
  if (nm_params.category == NM_SYNTH)
  {
    setCursor_textGrid_small(36, 9);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    if (nm_params.synth_waveform == 0)
      display.print("MDT  ");
    else  if (nm_params.synth_waveform == 1)
      display.print("SINE ");
    else  if (nm_params.synth_waveform == 2)
      display.print("SAW  ");
    else  if (nm_params.synth_waveform == 3)
      display.print("PULSE");

  }
}


FLASHMEM void nm_print_note_text() {
  if (nm_params.category == NM_SYNTH)
  {
    setCursor_textGrid_small(13, 15);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(" NOTE");
  }
}

FLASHMEM void addAutoUpdateParameterEditor_uint8_t_note(const char* name, uint8_t minv, uint8_t maxv, uint8_t* value) {
  ui.addEditor(name, minv, maxv, value,
    nullptr, // Use default getter
    [](Editor* editor, int16_t value) {
      *((uint8_t*)editor->value) = value;
      print_noisemaker_synth_note();
    }
  );
}

FLASHMEM void addAutoUpdateParameterEditor_uint8_t_wave(const char* name, uint8_t minv, uint8_t maxv, uint8_t* value) {
  ui.addEditor(name, minv, maxv, value,
    nullptr, // Use default getter
    [](Editor* editor, int16_t value) {
      *((uint8_t*)editor->value) = value;
      print_noisemaker_synth_wave();
    }
  );
}


FLASHMEM void addAutoUpdateParameterEditor_uint16_t(const char* name, uint16_t minv, uint16_t maxv, uint16_t* value) {
  ui.addCustomEditor(
    name,
    minv,
    maxv,
    value,
    [](Editor* editor) -> int16_t {
      return *((uint16_t*)editor->value);
    },
    [](Editor* editor, int16_t value) {
      *((uint16_t*)editor->value) = (uint16_t)value;
      // generateNoisePreview(); // Auto-update preview
     //  drawNoiseMakerWaveform(true);
    },
    nullptr // No custom renderer
  );
}

FLASHMEM void print_nm_style()
{

  display.setTextSize(1);
  setCursor_textGrid_small(9, 3);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.printf(" (");

  if (nm_params.category != NM_SYNTH)
  {
    if (nm_params.mode == NM_MODE_TR808)
      display.printf("808");
    else
      if (nm_params.mode == NM_MODE_TR909)
        display.printf("909");
  }
  else
    display.printf("SYN");

  display.printf(")");

}

// ===================== UI parameter builders =============================
FLASHMEM void addNoiseMakerParameterEditors(NoiseMakerParams& p) {
  // Clear parameter area but preserve header and controls
  display.console = true;
  display.fillRect(0, 70, 270, 130, COLOR_BACKGROUND);

  auto header = [](int x, int y, const char* t) {
    setCursor_textGrid_small(x, y);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print(t);
    };

  header(0, 5, "GLOBAL");
  ui.setCursor(0, 6);
  addAutoUpdateParameterEditor_uint8_t("LEVEL", 1, 100, &p.accent);

  header(0, 8, "GLOBAL REVERB");
  ui.setCursor(0, 9);


  addAutoUpdateParameterEditor_uint8_t("ROOM SIZE", 0, 100, &p.reverb_room_size);
  addAutoUpdateParameterEditor_uint8_t("DAMPING", 0, 100, &p.reverb_damping);
  addAutoUpdateParameterEditor_uint8_t("DRY", 0, 100, &p.reverb_dry);
  addAutoUpdateParameterEditor_uint8_t("WET", 0, 100, &p.reverb_wet);
  addAutoUpdateParameterEditor_uint8_t("WIDTH", 0, 100, &p.reverb_width);

  switch (p.category) {

    // -------------------- Bass Drum --------------------
  case NM_BD: {

    header(0, 15, "BASIC");
    ui.setCursor(0, 16);
    addAutoUpdateParameterEditor_uint8_t("PITCH", 1, 200, &p.bd_pitch);
    addAutoUpdateParameterEditor_uint8_t("P.ENV", 1, 100, &p.bd_pitch_env);
    addAutoUpdateParameterEditor_uint16_t("P.DECAY", 1, 100, &p.bd_pitch_decay);

    header(23, 8, "AMP ENV");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 30, &p.bd_attack);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.bd_decay);
    addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.bd_release);
    header(23, 13, "OTHER");
    ui.setCursor(23, 14);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.bd_tone);
    addAutoUpdateParameterEditor_uint8_t("CLICK", 0, 100, &p.bd_click);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 100, &p.bd_noise);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.bd_drive);
    addAutoUpdateParameterEditor_uint8_t("COMP", 0, 100, &p.bd_comp);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.bd_spread);

  } break;

    // -------------------- Snare --------------------
  case NM_SD: {
    header(0, 15, "BASIC");
    ui.setCursor(0, 16);
    addAutoUpdateParameterEditor_uint16_t("TONE1", 100, 1000, &p.sn_tone1);
    addAutoUpdateParameterEditor_uint16_t("TONE2", 100, 1000, &p.sn_tone2);
    addAutoUpdateParameterEditor_uint8_t("MIX", 0, 100, &p.sn_tone_mix);

    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("P.ENV", 0, 100, &p.sn_pitch_env);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.sn_decay);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 100, &p.sn_noise);

    header(23, 13, "FILTER");
    ui.setCursor(23, 14);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 0, 6000, &p.sn_bp_freq);
    addAutoUpdateParameterEditor_uint8_t("BP Q", 0, 25, &p.sn_bp_q);
    addAutoUpdateParameterEditor_uint8_t("SNAP", 0, 100, &p.sn_snap);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.sn_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.sn_spread);
  } break;

    // -------------------- Toms --------------------
  case NM_TOM: {
    header(23, 8, "BASIC");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("PITCH", 50, 500, &p.tom_pitch);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.tom_decay);
    addAutoUpdateParameterEditor_uint8_t("P.ENV", 0, 100, &p.tom_pitch_env);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.tom_tone);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 100, &p.tom_noise);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.tom_spread);

  } break;

  case NM_CONGA: {
    header(23, 8, "CONGA");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("PITCH", 50, 500, &p.conga_pitch);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.conga_decay);
    addAutoUpdateParameterEditor_uint8_t("TONE", 1, 100, &p.conga_tone);
    addAutoUpdateParameterEditor_uint8_t("P.ENV", 1, 100, &p.conga_pitch_env);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 100, &p.conga_noise);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.conga_spread);
  } break;


  case NM_RIM: {
    header(23, 8, "RIMSHOT");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("PITCH", 800, 2500, &p.rim_pitch);
    addAutoUpdateParameterEditor_uint16_t("MOD FREQ", 800, 5000, &p.rim_mod_freq);
    addAutoUpdateParameterEditor_uint16_t("NOISE BP", 2000, 8000, &p.rim_noise_bp_freq);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.rim_decay);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.rim_tone);
    addAutoUpdateParameterEditor_uint8_t("CLICK LVL", 0, 100, &p.rim_click_level);
    addAutoUpdateParameterEditor_uint8_t("NOISE LVL", 0, 100, &p.rim_noise_level);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.rim_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.rim_spread);

  } break;

    // -------------------- Clap --------------------
  case NM_CLAP: {
    header(23, 8, "CLAP OSC");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("REPEATS", 2, 10, &p.clap_repeats);
    addAutoUpdateParameterEditor_uint8_t("TAIL", 0, 100, &p.clap_tail);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 0, 1000, &p.clap_bp_hz);
    addAutoUpdateParameterEditor_uint8_t("BP Q", 0, 80, &p.clap_bp_q);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.clap_decay);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.clap_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.clap_spread);
  } break;

    // -------------------- Hi-Hats --------------------
  case NM_HH: {
    header(23, 8, "HIHAT");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("OPEN", 0, 1, &p.hh_mode);   // Closed/Open
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.hh_tone);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 100, &p.hh_noise);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 500, 10000, &p.hh_bp_hz);
    addAutoUpdateParameterEditor_uint8_t("BP Q", 1, 20, &p.hh_bp_q);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.hh_decay);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.hh_drive);
    addAutoUpdateParameterEditor_uint8_t("DETUNE", 0, 10, &p.hh_detune);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.hh_spread);
  } break;

    // -------------------- Cymbals --------------------
  case NM_CRASH: {
    header(23, 8, "CRASH");

    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.crash_tone);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 2000, 12000, &p.crash_bp_hz);
    addAutoUpdateParameterEditor_uint8_t("BP Q", 0, 100, &p.crash_bp_q);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.crash_decay);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.crash_spread);
  } break;

  case NM_RIDE: {
    header(23, 8, "RIDE");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.ride_tone);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 2000, 12000, &p.ride_bp_hz);
    addAutoUpdateParameterEditor_uint8_t("BP Q", 0, 100, &p.ride_bp_q);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.ride_decay);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.ride_spread);
  } break;

    // -------------------- Cowbell --------------------
  case NM_COWBELL: {
    header(23, 8, "COWBELL");
    ui.setCursor(23, 9);

    addAutoUpdateParameterEditor_uint16_t("FREQ 1", 200, 1000, &p.cb_freq1);
    addAutoUpdateParameterEditor_uint16_t("FREQ 2", 200, 1000, &p.cb_freq2);
    addAutoUpdateParameterEditor_uint8_t("BALANCE", 0, 100, &p.cb_tone_balance);

    // Column 2 - Envelope & Effects
    header(23, 13, "ENVELOPE");
    ui.setCursor(23, 14);
    addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 5, &p.cb_amp_attack);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 5, 250, &p.cb_amp_decay);
    addAutoUpdateParameterEditor_uint8_t("NOISE", 0, 20, &p.cb_noise);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 60, &p.cb_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.cb_spread);

  } break;

    // -------------------- Claves --------------------
  case NM_CLAVES: {
    header(23, 8, "CLAVES");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("HZ", 50, 2000, &p.claves_hz);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.claves_decay);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.claves_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.claves_spread);
  } break;

    // -------------------- Maracas --------------------
  case NM_MARACAS: {
    header(23, 8, "MARACAS");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("BP FREQ", 1000, 6000, &p.mar_bp_hz); // center freq
    addAutoUpdateParameterEditor_uint8_t("RESO", 50, 100, &p.mar_bp_q);        // resonance
    addAutoUpdateParameterEditor_uint8_t("DECAY", 1, 100, &p.mar_decay);       // AR envelope
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.mar_spread);
  } break;

    // -------------------- Tambourine --------------------
  case NM_ZAP: {
    header(23, 8, "ZAP");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("START PITCH", 1000, 10000, &p.zap_pitch_start);
    addAutoUpdateParameterEditor_uint16_t("END PITCH", 100, 1000, &p.zap_pitch_end);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.zap_decay);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.zap_spread);
  } break;

    // -------------------- Rimshot --------------------
  case NM_SNAP: {
    header(23, 8, "SNAP/WOODBLOCK");
    ui.setCursor(23, 9);
    addAutoUpdateParameterEditor_uint16_t("PITCH", 300, 4000, &p.snap_pitch);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.snap_decay);
    addAutoUpdateParameterEditor_uint8_t("TONE", 0, 100, &p.snap_tone);
    addAutoUpdateParameterEditor_uint8_t("DRIVE", 0, 100, &p.snap_drive);
    addAutoUpdateParameterEditor_uint8_t("SPREAD", 0, 100, &p.snap_spread);
  } break;

  case NM_SYNTH: {

    header(0, 14, "SYNTH");
    ui.setCursor(0, 15);
    addAutoUpdateParameterEditor_uint8_t_note("", 12, 126, &p.synth_midi_note);
    addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 100, &p.synth_attack);
    addAutoUpdateParameterEditor_uint8_t("DECAY", 0, 100, &p.synth_decay);
    addAutoUpdateParameterEditor_uint8_t("SUST.LVL", 0, 100, &p.synth_sustain_level);
    addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.synth_release);
    header(20, 8, "SYNTH");
    ui.setCursor(20, 9);

    addAutoUpdateParameterEditor_uint8_t_wave("WAVE:", 0, 3, &p.synth_waveform);
    addAutoUpdateParameterEditor_uint8_t("PULSE WIDTH", 0, 100, &p.synth_pulse_width);
    addAutoUpdateParameterEditor_uint8_t("FILT CUTOFF", 0, 100, &p.synth_filter_cutoff);
    addAutoUpdateParameterEditor_uint8_t("FILT RESO", 0, 100, &p.synth_filter_resonance);
    addAutoUpdateParameterEditor_uint8_t("FILT ENV AMT", 0, 100, &p.synth_filter_env_amount);
    addAutoUpdateParameterEditor_uint8_t("FILT ENV ATTK", 0, 100, &p.synth_filter_env_attack);
    addAutoUpdateParameterEditor_uint8_t("FILT ENV DECAY", 0, 100, &p.synth_filter_env_decay);
    //  addAutoUpdateParameterEditor_uint8_t("FILT ENV REL",0, 100, &p.synth_filter_env_release);
    addAutoUpdateParameterEditor_uint8_t("WAVE MIX", 0, 100, &p.synth_waveform_mix);
    addAutoUpdateParameterEditor_uint8_t("OCTAVE MIX", 0, 100, &p.synth_octave_mix);
    // addAutoUpdateParameterEditor_uint8_t("DETUNE",       0, 100, &p.synth_detune);
    addAutoUpdateParameterEditor_uint8_t("NOISE LEVEL", 0, 100, &p.synth_noise_level);
    addAutoUpdateParameterEditor_uint8_t("NOISE DECAY", 0, 100, &p.synth_noise_decay);

  } break;

  default: break;
  }
}
#endif

// ===================== UI page ===========================================

#if defined COMPILE_FOR_PSRAM
bool noisemaker_update_params_flag = false;
bool noisemaker_needs_refresh = false;
extern bool check_sd_noisemaker_config_exists();
bool nm_firstSetup = true;
extern void draw_nm_emulation_mode_button();
extern void draw_nm_monostereo();
extern void draw_nm_reverb_OffOn();
extern void save_sd_noisemaker_json(void);

FLASHMEM void UI_func_noisemaker(uint8_t param)
{
  if (LCDML.FUNC_setup())
  {
    // Register touch handler
    registerTouchHandler(handle_touchscreen_noisemaker);

    // Initialize only once
    if (nm_firstSetup) {
      ui.reset();
      if (!check_sd_noisemaker_config_exists())
        initNoiseMakerParams(nm_params);
      // generateNoisePreview();
      nm_firstSetup = false;
    }
    drawNoiseMakerWaveform(true);
    // --- PAGE HEADER ---
    display.setTextSize(1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    setCursor_textGrid_small(0, 0);
    display.print("NOISEMAKER");
    display.setTextSize(1);

    // --- CATEGORY SELECTOR ---
    ui.setCursor(0, 2);
    ui.addEditor("CATEGORY", 0, NM_CATEGORY_COUNT - 1,
      [](Editor* editor) -> int16_t {
        return nm_params.category;
      },
      [](Editor* editor, int16_t value) {
        // Update category and request full refresh
        nm_params.category = value;
        generateNoisePreview();          // <-- regenerate preview first
        // drawNoiseMakerWaveform(true);    // <-- then draw it
            // initNoiseMakerParams(nm_params);
        noisemaker_needs_refresh = true;

      },
      [](Editor* editor, bool refresh) {
        setCursor_textGrid_small(editor->x, editor->y);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        display.setTextSize(1);
        display.printf("TYPE: ");
        setModeColor(editor->select_id);
        display.print(noisemaker_category_names[nm_params.category]);
        print_empty_spaces(5, 1);
        //  noisemaker_needs_refresh=true;
      }
    );

    // --- SLOT SELECTOR ---
    ui.setCursor(0, 3);
    ui.addEditor("SLOT", 0, NUM_CUSTOM_SAMPLES - 1,
      [](Editor* editor) -> int16_t {
        return noisemaker_custom_slot;
      },
      [](Editor* editor, int16_t value) {
        noisemaker_custom_slot = value;
        drawNoiseMakerWaveform(false);
      },
      [](Editor* editor, bool refresh) {
        setCursor_textGrid_small(editor->x, editor->y);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        display.setTextSize(1);
        display.printf("SLOT: ");
        setModeColor(editor->select_id);
        display.printf("C%02d", noisemaker_custom_slot + 1);
        print_nm_style();
        print_empty_spaces(2, 1);
      }
    );

    // --- Touch buttons ---
    TouchButton::drawButton(GRID.X[1], GRID.Y[5], "RENDER", "SOUND", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[2], GRID.Y[5], "PLAY", "SOUND", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[3], GRID.Y[5], "RAND", "", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SLOT", TouchButton::BUTTON_ACTIVE);
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SLOT", TouchButton::BUTTON_ACTIVE);

    draw_nm_emulation_mode_button();
    draw_nm_monostereo();
    draw_nm_reverb_OffOn();

    addNoiseMakerParameterEditors(nm_params);
    print_noisemaker_synth_note();
    nm_print_note_text();

    if (nm_params.category == NM_SYNTH)
      print_noisemaker_synth_wave();
  }

  if (LCDML.FUNC_loop())
  {
    ui.handle_input();

    // If category changed → refresh UI
    if (noisemaker_needs_refresh) {

      ui.clear_noisemaker();
      // --- PAGE HEADER ---
      display.setTextSize(1);
      display.setTextColor(RED, COLOR_BACKGROUND);
      setCursor_textGrid_small(0, 0);
      display.print("NOISEMAKER");

      // --- CATEGORY SELECTOR ---
      ui.setCursor(0, 2);
      ui.addEditor("CATEGORY", 0, NM_CATEGORY_COUNT - 1,
        [](Editor* editor) -> int16_t {
          return nm_params.category;
        },
        [](Editor* editor, int16_t value) {
          nm_params.category = value;

          noisemaker_needs_refresh = true;
        },
        [](Editor* editor, bool refresh) {
          setCursor_textGrid_small(editor->x, editor->y);
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.setTextSize(1);
          display.printf("TYPE: ");
          setModeColor(editor->select_id);
          display.print(noisemaker_category_names[nm_params.category]);
          print_empty_spaces(5, 1);
        }
      );

      // --- SLOT SELECTOR ---
      ui.setCursor(0, 3);
      ui.addEditor("SLOT", 0, NUM_CUSTOM_SAMPLES - 1,
        [](Editor* editor) -> int16_t {
          return noisemaker_custom_slot;
        },
        [](Editor* editor, int16_t value) {
          noisemaker_custom_slot = value;
          drawNoiseMakerWaveform(false);
        },
        [](Editor* editor, bool refresh) {
          setCursor_textGrid_small(editor->x, editor->y);
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.setTextSize(1);
          display.printf("SLOT: ");
          setModeColor(editor->select_id);
          display.printf("C%02d", noisemaker_custom_slot + 1);
          print_nm_style();
          print_empty_spaces(2, 1);
        }
      );

      // --- Parameter editors for current category ---
      addNoiseMakerParameterEditors(nm_params);
      print_noisemaker_synth_note();
      nm_print_note_text();

      // --- Touch buttons ---
      TouchButton::drawButton(GRID.X[1], GRID.Y[5], "RENDER", "SOUND", TouchButton::BUTTON_ACTIVE);
      TouchButton::drawButton(GRID.X[2], GRID.Y[5], "PLAY", "SOUND", TouchButton::BUTTON_ACTIVE);

      TouchButton::drawButton(GRID.X[3], GRID.Y[5], "RAND", "", TouchButton::BUTTON_ACTIVE);
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SAVE", "SLOT", TouchButton::BUTTON_ACTIVE);
      TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SLOT", TouchButton::BUTTON_ACTIVE);

      draw_nm_emulation_mode_button();
      draw_nm_monostereo();
      draw_nm_reverb_OffOn();
      noisemaker_needs_refresh = false;
    }
  }
  if (LCDML.FUNC_close())
  {
    display.setTextSize(1);
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    setCursor_textGrid_small(0, 0);
    display.print("SETTINGS SAVED");

    save_sd_noisemaker_json();
    delay(200);
    unregisterTouchHandler();
    // nm_firstSetup = true;
    ui.clear();
    //nm_preview_free();
  }
}

FLASHMEM void refreshNoisemakerParameterEditors() {
  // Only refresh if we're currently on the noisemaker page
  if (noisemaker_update_params_flag && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_noisemaker)) {
    ui.draw_editors(true); // refresh all editors to show new values
    noisemaker_update_params_flag = false;
    TouchButton::drawButton(GRID.X[3], GRID.Y[5], "RAND", "", TouchButton::BUTTON_ACTIVE);
  }
}
#else
FLASHMEM void UI_func_noisemaker(uint8_t param)
{
  if (LCDML.FUNC_setup())
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_l(back_text);
    setCursor_textGrid(1, 1);
    display.setTextSize(1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("ONLY IN PSRAM FIRMWARE"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ;
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}
#endif

FLASHMEM void set_seq_timing()
{
  if (seq.clock == 0) // INTERNAL TIMING
    seq.ticks_max = 7; //(0-7 = 8)
  else // MIDI SLAVE or MIDI MASTER
    seq.ticks_max = 5; //(0-5 = 6)
}



FLASHMEM void print_arr_split_range(uint8_t x, uint8_t y)
{  //23, 7 on seq. settings page
  setCursor_textGrid_small(x, y);
  if (seq.arr_split_note != 0)
  {
    display.print("C-0");
    setCursor_textGrid_small(x + 6, y);
    display.print(midiToNote(seq.arr_split_note));
    display.print(F("-"));
    display.print(seq.arr_split_note / 12);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F(" "));
  }
  else
  {
    display.print(F("OFF"));
    setCursor_textGrid_small(x + 6, y);
    display.print(F("OFF"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F(" "));

  }
}


FLASHMEM void UI_func_seq_settings(uint8_t param)
{
  //char displayname[5] = { 0, 0, 0, 0, 0 };
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    if (generic_temp_select_menu != 6) // preselect BPM when coming from song edit by quick navigation
      generic_temp_select_menu = 0;
    seq.edit_state = false;
    encoderDir[ENC_R].reset();
    helptext_l(back_text);
    display.setTextSize(1);
    display.setTextColor(RED);

    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 0);
    display.setTextColor(RED);
    display.print("ARP/CHORD TRANSPOSE");

    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 2);
    display.print("OCTAVE SHIFT -/+");
    setCursor_textGrid_small(1, 3);
    display.print("NOTE (INTERVAL) SHIFT");
    setCursor_textGrid_small(1, 4);
    display.print("MAX. CHORD NOTES");
    setCursor_textGrid_small(26, 4);
    display.setTextColor(GREY2);
    display.print(F("KEYS"));
    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 5);
    display.print("MAX. ARP NOTES/STEP");
    setCursor_textGrid_small(26, 5);
    display.setTextColor(GREY2);
    display.print(F("STEPS"));
    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 6);
    display.print(F("CHRD/ARP VELOCITY"));
    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 7);
    display.print(F("ARRANGER SPLIT NOTE"));
    setCursor_textGrid_small(27, 7);
    display.print("-");

    display.setTextColor(RED);
    setCursor_textGrid_small(1, 9);
    display.print(F("SAMPLES"));
    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 11);
    display.print(F("HIHAT RANDOMIZER"));
    display.setTextColor(GREY2);
    setCursor_textGrid_small(28, 11);
    display.print(F("SAMPLES"));

    display.setTextColor(RED);
    setCursor_textGrid_small(1, 13);
    display.print(F("SONG"));

    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1, 15);
    display.print(F("SONG TEMPO"));
    setCursor_textGrid_small(27, 15);
    display.print("BPM");
    display.setTextColor(GREY2);
    setCursor_textGrid_small(12, 15);
    display.print("(");
    setCursor_textGrid_small(16, 15);
    display.print("ms)");
    setCursor_textGrid_small(1, 16);
    display.print(F("PATTERN LENGTH"));
    setCursor_textGrid_small(1, 17);
    display.print(F("STEP RECORDING:"));
    setCursor_textGrid_small(1, 18);
    display.print(F("TIMING CLOCK:"));
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        // navigate through menu
        generic_temp_select_menu = constrain(generic_temp_select_menu + (e.dir * 1), 0, 10);
      }
      else {
        // handle setting change
        switch (generic_temp_select_menu) {
        case 0:
          seq.oct_shift = constrain(seq.oct_shift + (e.dir * 1), -2, 2);
          break;

        case 1:
          seq.element_shift = constrain(seq.element_shift + (e.dir * 1), 0, 6);
          break;

        case 2:
          seq.chord_key_ammount = constrain(seq.chord_key_ammount + (e.dir * 1), 1, 7);
          break;

        case 3:
          seq.arp_num_notes_max = constrain(seq.arp_num_notes_max + (e.dir * 1), 1, 64);
          break;

        case 4:
          seq.chord_vel = constrain(seq.chord_vel + (e.dir * 1), 10, 120);
          break;

        case 5:
          seq.arr_split_note = constrain(seq.arr_split_note + (e.dir * 1), 0, 72);
          break;

        case 6:
          seq.hihat_randomizer = constrain(seq.hihat_randomizer + (e.dir * 1), 0, 100);
          break;

        case 7:
          seq.bpm = constrain(seq.bpm + (e.dir * 1), 50, 240);
          update_seq_speed();
          break;

        case 8:
          seq.pattern_len_dec = constrain(seq.pattern_len_dec - (e.dir * 1), 0, 8);  // dir reversed as this is decreased from 16
          break;

        case 9:
          seq.auto_advance_step = constrain(seq.auto_advance_step + (e.dir * 1), 0, 2);
          break;

        case 10:
          seq.clock = constrain(seq.clock + (e.dir * 1), 0, 2);

          if (seq.clock == 1 && seq.running == true) {// stop when switching to MIDI Slave
            handleStop();
          }
          else {
            update_seq_speed();
          }
          break;
        }
      }
    }
    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
    }

    setModeColor(0);
    setCursor_textGrid_small(23, 2);
    display.printf("%02d", seq.oct_shift);
    //print_formatted_number(seq.oct_shift, 2,0);
    setModeColor(1);
    setCursor_textGrid_small(23, 3);
    print_formatted_number(seq.element_shift, 2, 1, 1);
    setModeColor(2);
    setCursor_textGrid_small(23, 4);
    print_formatted_number(seq.chord_key_ammount, 2, 2, 1);
    setModeColor(3);
    setCursor_textGrid_small(23, 5);
    print_formatted_number(seq.arp_num_notes_max, 2, 3, 1);
    setModeColor(4);
    setCursor_textGrid_small(23, 6);
    print_formatted_number(seq.chord_vel, 3, 4, 1);
    setModeColor(5);
    print_arr_split_range(23, 7);
    setModeColor(6);
    setCursor_textGrid_small(23, 11);
    print_formatted_number(seq.hihat_randomizer * 50, 4, 6, 1);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    setCursor_textGrid_small(13, 15);
    display.printf("%03d", seq.tempo_ms / 1000);

    setModeColor(7);
    setCursor_textGrid_small(23, 15);
    //display.printf("%03d", seq.bpm);
    print_formatted_number(seq.bpm, 3, 7, 1);

    setModeColor(8);
    setCursor_textGrid_small(23, 16);
    print_formatted_number(16 - seq.pattern_len_dec, 2, 8, 1);

    setModeColor(9);
    setCursor_textGrid_small(23, 17);
    if (seq.auto_advance_step == 1)
      display.print(F("AUTO ADVANCE STEP"));
    else if (seq.auto_advance_step == 2)
      display.print(F("AUTO ADVANCE+STOP"));
    else
      display.print(F("KEEP CURRENT STEP"));

    setModeColor(10);
    setCursor_textGrid_small(23, 18);
    if (seq.clock == 0)
      display.print(F("INTERNAL (NO MIDI SYNC)   "));
    else if (seq.clock == 1)
      display.print(F("MIDI SLAVE (RECEIVE CLOCK)"));
    else if (seq.clock == 2)
      display.print(F("MIDI MASTER (SEND CLOCK)  "));

    set_seq_timing();

    //warning message
    if (seq.clock == 0) {
      display.setTextSize(1);
      display.setTextColor(DARKGREEN, COLOR_BACKGROUND);
      setCursor_textGrid_small(1, 20);
      display.print(F("TIMING IS SET TO INTERNAL FOR BEST PERFORMANCE"));
      setCursor_textGrid_small(1, 21);
      display.print(F("YOU CAN USE EXTERNAL MIDI GEAR BUT NO MIDI CLOCK"));
    }
    else if (seq.clock == 1) {
      display.setTextSize(1);
      display.setTextColor(RED, COLOR_BACKGROUND);
      setCursor_textGrid_small(1, 20);
      display.print(F("WARNING: CLOCK IS SET TO EXTERNAL (MIDI CLOCK)"));
      setCursor_textGrid_small(1, 21);
      display.print(F("SEQ. WILL DEPEND ON THE EXTERNAL DEVICE TO WORK "));
    }
    else {
      display.console = true;
      display.fillRect(4, 12 * CHAR_height - 4, 300, 19, COLOR_BACKGROUND);
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void print_performance_name(int x, int y)
{
  display.setTextSize(1);
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  // display.setCursor(CHAR_width_small * 36,   11 * (CHAR_height_small + 2) + 10);
  display.setCursor(x, y);
  print_formatted_number(configuration.sys.performance_number, 2);
  display.print(":");
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  display.print("[");
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(seq.name);
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  display.print("]");
  display.setTextSize(2);
}

FLASHMEM void seq_printAllSeqSteps()
{
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {
    display.setTextSize(2);
    setCursor_textGrid(0, 1);
  }

  for (uint8_t i = 0; i < 16; i++)
  {
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
    {
      if (i > 15 - seq.pattern_len_dec) // pattern is shorter than default 16 steps
        display.setTextColor(GREY3, COLOR_BACKGROUND);
      else if (seq.vel[seq.active_pattern][i] > 209)
        display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      else
        set_pattern_content_type_color(seq.active_pattern);
      display.print(seq_find_shortname(i)[0]);
    }

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
    {
      if (apc_scroll_message == false)
      {
        if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)  //drum or sliced
        {
          if (i < 8)
          {
            if (seq.vel[seq.active_pattern][i] < 210) //normal sample
              apc(64 - 8 + i, seq.note_data[seq.active_pattern][i], 6);
            else
              apc(64 - 8 + i, seq.vel[seq.active_pattern][i] - 64, 6);//pitched sample
          }
          else
          {
            if (seq.vel[seq.active_pattern][i] < 210) //normal sample
              apc(64 - 16 - 8 + i, seq.note_data[seq.active_pattern][i], 6);
            else
              apc(64 - 16 - 8 + i, seq.vel[seq.active_pattern][i] - 64, 6); //pitched sample
          }
        }
        else  //instrument track
        {
          if (i < 8)
          {
            apc(64 - 8 + i, seq.note_data[seq.active_pattern][i], 6);
          }
          else
          {
            apc(64 - 16 - 8 + i, seq.note_data[seq.active_pattern][i], 6);
          }
        }
      }
  }
#endif
}
}

FLASHMEM void sub_print_volume_pad(uint8_t in, uint8_t pos, uint8_t col);

FLASHMEM void seq_printAllVelocitySteps()
{
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {
    if (remote_active)
      display.console = true;
    display.fillRect(0, 2 * CHAR_height, 17 * CHAR_width, 17, COLOR_BACKGROUND);
  }

  for (uint8_t i = 0; i < 16; i++)
  {

    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
    {
      if (seq.vel[seq.active_pattern][i] < 130)
      {
        display.drawLine(0 + i * CHAR_width + 3, CHAR_height * 3 - 3, 0 + i * CHAR_width + 3, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][i] / 10), GREY1);
        display.drawLine(0 + i * CHAR_width + 4, CHAR_height * 3 - 3, 0 + i * CHAR_width + 4, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][i] / 10), GREY1);
        display.drawLine(0 + i * CHAR_width + 5, CHAR_height * 3 - 3, 0 + i * CHAR_width + 5, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][i] / 10), GREY1);
      }
      else
      {
        if (seq.vel[seq.active_pattern][i] > 209)
        {
          drawBitmap((i - 1) * CHAR_width + 13, 2 * CHAR_height + 7, special_chars[22], 8, 8, COLOR_PITCHSMP);
        }
      }
    }

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
    {
      if (apc_scroll_message == false)
      {
        if (i < 8)
        {
          if (seq.vel[seq.active_pattern][i] < 210) //normal sample
            sub_print_volume_pad(seq.vel[seq.active_pattern][i], 64 - 32 + i, 45);
          else
            sub_print_volume_pad(seq.vel[seq.active_pattern][i], 64 - 32 + i, 3); //white, pitched sample
        }
        else
        {
          if (seq.vel[seq.active_pattern][i] < 210) //normal sample
            sub_print_volume_pad(seq.vel[seq.active_pattern][i], 64 - 32 - 8 - 8 + i, 45);
          else
            sub_print_volume_pad(seq.vel[seq.active_pattern][i], 64 - 32 - 8 - 8 + i, 3); //white, pitched sample
        }
      }
  }
#endif
}
}

FLASHMEM void seq_printAllVelocitySteps_single_step(uint8_t step, int color)
{
  check_remote();

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor) || LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {

    display.fillRect(0 + step * CHAR_width + 1, CHAR_height * 2 + 1, 8, 14, COLOR_BACKGROUND);
    if (seq.vel[seq.active_pattern][step] < 130)
    {
      display.drawLine(0 + step * CHAR_width + 3, CHAR_height * 3 - 3, 0 + step * CHAR_width + 3, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][step] / 10), color);
      display.drawLine(0 + step * CHAR_width + 4, CHAR_height * 3 - 3, 0 + step * CHAR_width + 4, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][step] / 10), color);
      display.drawLine(0 + step * CHAR_width + 5, CHAR_height * 3 - 3, 0 + step * CHAR_width + 5, CHAR_height * 3 - 3 - (seq.vel[seq.active_pattern][step] / 10), color);
    }
    else
    {
      if (seq.vel[seq.active_pattern][step] > 209)
      {
        drawBitmap((step - 1) * CHAR_width + 13, 2 * CHAR_height + 7, special_chars[22], 8, 8, COLOR_PITCHSMP);
      }
    }
  }

#if defined APC
  if (APC_MODE == APC_PATTERN_EDITOR)
  {
    if (step < 8)
    {
      sub_print_volume_pad(seq.vel[seq.active_pattern][step], 64 - 32 + step, 45);
    }
    else
    {
      sub_print_volume_pad(seq.vel[seq.active_pattern][step], 64 - 32 - 8 - 8 + step, 45);
    }
}
#endif
}

bool wave_editor_skip_redraw_overview = false;

FLASHMEM void slice_editor_update_file_counts()
{
  if (fm.sample_source == 0) // source = SD CARD
  {
    sd_card_count_files_from_directory("/DRUMS");
  }
}

FLASHMEM void print_available_loop_options()
{
  if (fm.sample_source == 2 || fm.sample_source == 3 || fm.sample_source == 4)
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  else
    display.setTextColor(GREY2, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 4);
  display.print(F("LOOP START:"));
  setCursor_textGrid_small(1, 5);
  display.print(F("LOOP END:"));
  setCursor_textGrid_small(1, 6);
  display.print(F("LOOP TYPE:"));
}

FLASHMEM uint8_t get_current_sliced_sample_in_ui()
{
  uint8_t sel = 0;
  if (selected_slice_sample[0] == temp_int && selected_slice_sample[0] != 99)
    sel = 0;
  else if (selected_slice_sample[1] == temp_int && selected_slice_sample[1] != 99)
    sel = 1;

  return sel;
}

FLASHMEM void clear_slices()
{
  if (selected_slice_sample[get_current_sliced_sample_in_ui()] != 99)
  {
    for (uint8_t i = 0; i < 16; i++)
    {
      slice_start[get_current_sliced_sample_in_ui()][i] = 0;
      slice_end[get_current_sliced_sample_in_ui()][i] = 0;
    }
  }
}

FLASHMEM void fill_slices()
{
  if (selected_slice_sample[get_current_sliced_sample_in_ui()] != 99 && selected_slice_sample[get_current_sliced_sample_in_ui()] == temp_int)
  {
    for (uint8_t y = 0; y < num_slices[get_current_sliced_sample_in_ui()]; y++)
    {
      slice_start[get_current_sliced_sample_in_ui()][y] = (drum_config[temp_int].len / num_slices[get_current_sliced_sample_in_ui()]) * y;
      slice_end[get_current_sliced_sample_in_ui()][y] = (drum_config[temp_int].len / num_slices[get_current_sliced_sample_in_ui()]) * (y + 1);
    }
  }
}

FLASHMEM void print_slices_static_text()
{
  if (selected_slice_sample[get_current_sliced_sample_in_ui()] != 99 && selected_slice_sample[get_current_sliced_sample_in_ui()] == temp_int)
  {
    for (uint8_t y = 0; y < 4; y++)
    {
      setCursor_textGrid_small(26, y + 3);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print("SLICE");

      setCursor_textGrid_small(42, y + 3);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print("-");

    }
  }
}

FLASHMEM void print_updated_slices_text()
{
  if (selected_slice_sample[get_current_sliced_sample_in_ui()] != 99 && selected_slice_sample[get_current_sliced_sample_in_ui()] == temp_int)
  {
    char tmp[10];

    uint8_t i = 0;
    uint8_t field_offset = 0;

    if (generic_temp_select_menu > 13 + 8 + 8)  //this is not beautiful. I intended to make a full scrolling list, not paged, gave up after several hours due to high complexity
      field_offset = 4 + 4 + 4;
    else
      if (generic_temp_select_menu > 13 + 8)
        field_offset = 4 + 4;
      else
        if (generic_temp_select_menu > 13)
          field_offset = 4;

    for (uint8_t y = 0; y < 4; y++)
    {
      int slice_col;
      if ((y + field_offset) % 2 == 0)
        slice_col = ColorHSV((y + field_offset) * 22, 180, 254);
      else
        slice_col = ColorHSV((y + field_offset) * 22, 250, 180);

      setCursor_textGrid_small(32, y + 3);

      display.setTextColor(slice_col, COLOR_BACKGROUND);
      snprintf_P(tmp, sizeof(tmp), PSTR("%02d"), y + 1 + (field_offset));
      display.print(tmp);

      if (generic_temp_select_menu - 5 == i + 1 + (field_offset * 2) && seq.edit_state)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (generic_temp_select_menu - 5 == i + 1 + (field_offset * 2))
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      setCursor_textGrid_small(35, y + 3);
      snprintf_P(tmp, sizeof(tmp), PSTR("%06d"), slice_start[get_current_sliced_sample_in_ui()][y + field_offset]);
      display.print(tmp);

      i++;

      if (generic_temp_select_menu - 5 == i + 1 + (field_offset * 2) && seq.edit_state)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (generic_temp_select_menu - 5 == i + 1 + (field_offset * 2))
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      setCursor_textGrid_small(44, y + 3);
      snprintf_P(tmp, sizeof(tmp), PSTR("%06d"), slice_end[get_current_sliced_sample_in_ui()][y + field_offset]);
      display.print(tmp);
      i++;
    }
  }
  else  //is unsliced 
  {
    display.console = true;
    display.fillRect(155, 30, 156, 38, COLOR_BACKGROUND);
    display.console = false;
  }
}

FLASHMEM void draw_header_slice_editor()
{
  if (fm.sample_source == 2 || fm.sample_source == 3) //MSP
  {
    setCursor_textGrid_small(14, 2);
    display.setTextColor(RED, COLOR_BACKGROUND);
    if (drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number].len == 0 && strlen(msz[fm.sample_source - 2][temp_int].filename) > 2)
    {
      display.print(F("PLEASE RELOAD THIS SAMPLE IN MSP"));
    }
    else
      display.print(F("                                "));

    if (strlen(msz[fm.sample_source - 2][temp_int].filename) < 2)
      display.printf("%04d", 0);
    else
      display.printf("%04d", drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number].len / 1024);
  }
  else //slice/normal sample
  {
    display.printf("%04d", drum_config[temp_int].len / 1024);
  }
  setCursor_textGrid_small(32, 1);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.print(" KB  ");
}

FLASHMEM void draw_waveform_slice_editor(uint8_t slice)
{
  const drum_config_t* sample = &drum_config[temp_int];
  uint8_t from = 0;
  uint8_t to = 0;

  if (slice == 99) //all
  {
    from = 0;
    to = num_slices[get_current_sliced_sample_in_ui()];
  }
  else
  {
    if (slice == 0)
      from = 0;
    else
      from = slice - 1;

    if (slice == num_slices[get_current_sliced_sample_in_ui()])
      to = num_slices[get_current_sliced_sample_in_ui()];
    else
      to = slice + 2;
  }

  print_slices_static_text();
  print_updated_slices_text();
  if (temp_int != selected_slice_sample[get_current_sliced_sample_in_ui()])
    drawSampleWaveform(0, 72, DISPLAY_WIDTH, 100, (int16_t*)sample->drum_data, sample->len, sample->numChannels, 0, DISPLAY_WIDTH, COLOR_SYSTEXT, GREY4);
  else
  {
    if (sample->len != 0)
    {
      const uint16_t xCoord = 0;
      const uint16_t yCoord = 72;
      const uint16_t w = DISPLAY_WIDTH;
      const uint16_t h = 100;

      for (uint8_t y = from; y < to; y++)
      {
        int slice_col;
        if (y % 2 == 0)
          slice_col = ColorHSV(y * 22, 180, 254);
        else
          slice_col = ColorHSV(y * 22, 250, 180);

        const uint16_t from = w * slice_start[get_current_sliced_sample_in_ui()][y] / sample->len;
        const uint16_t to = w * slice_end[get_current_sliced_sample_in_ui()][y] / sample->len;

        drawSampleWaveform(xCoord, yCoord, w, h, (int16_t*)sample->drum_data, sample->len, sample->numChannels, from, to, slice_col, GREY3);

        display.console = true;
        display.fillRect(xCoord - 2, 164, 2, 7, GREY3);
        display.console = false;
        display.setCursor(xCoord + 1, 164);
        display.setTextColor(slice_col, GREY3);

        if (get_current_sliced_sample_in_ui() == 0)
        {
          display.print(noteNames[(48 + y) % 12][0]);
          if (noteNames[(48 + y) % 12][1] != '\0')
          {
            display.print(noteNames[(48 + y) % 12][1]);
          }
          display.print(((48 + y) / 12) - 1);
        }
        else if (get_current_sliced_sample_in_ui() == 1)
        {
          display.print(noteNames[(48 + num_slices[0] + y) % 12][0]);
          if (noteNames[(48 + num_slices[0] + y) % 12][1] != '\0')
          {
            display.print(noteNames[(48 + num_slices[0] + y) % 12][1]);
          }
          display.print(((48 + num_slices[0] + y) / 12) - 1);
        }

      }
    }
    else // empty sample
    {
      display.console = true;
      display.fillRect(0, 72, DISPLAY_WIDTH, 100, GREY3);
      display.console = false;
      display.setCursor(118, 120);
      display.setTextColor(RED, GREY3);
      display.print(F("EMPTY SAMPLE SLOT"));
    }
  }
}

FLASHMEM void draw_waveform_loop_editor()
{  // LOOP for samples in MSP (MultiSamplePlayer)

  const drum_config_t* sample = &drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number];

  int slice_col = GREY1;
  if (sample->len != 0)
  {
    if (fm.sample_source == 2 || fm.sample_source == 3)
    {
      if (drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number].len != 0 && strlen(msz[fm.sample_source - 2][temp_int].filename) > 2)
      {
        const uint16_t xCoord = 0;
        const uint16_t yCoord = 72;
        const uint16_t w = DISPLAY_WIDTH;
        const uint16_t h = 100;
        const uint16_t sampleStartX = w * msz[fm.sample_source - 2][temp_int].loop_start / sample->len;
        const uint16_t sampleEndX = w * msz[fm.sample_source - 2][temp_int].loop_end / sample->len;
        //START
        drawSampleWaveform(xCoord, yCoord, w, h, (int16_t*)sample->drum_data, sample->len, sample->numChannels, xCoord, sampleStartX, slice_col, GREY2);

        //LOOP
        drawSampleWaveform(xCoord, yCoord, w, h, (int16_t*)sample->drum_data, sample->len, sample->numChannels, sampleStartX, sampleEndX, COLOR_SYSTEXT, RED);

        //END
        drawSampleWaveform(xCoord, yCoord, w, h, (int16_t*)sample->drum_data, sample->len, sample->numChannels, sampleEndX, w, slice_col, GREY2);
      }
    }
  }
}

FLASHMEM void print_sliced_status()
{
  char buf[4];
  setModeColor(1);
  setCursor_textGrid_small(44, 1);
  if (selected_slice_sample[0] != 99 && temp_int == selected_slice_sample[0])
  {
    display.print(F("SLICED1/2"));
    draw_button_on_grid(33, 23, "SLICES", itoa(num_slices[0], buf, 10), 1);
  }
  else if (selected_slice_sample[0] != 99 && temp_int == selected_slice_sample[1])
  {
    display.print(F("SLICED2/2"));
    draw_button_on_grid(33, 23, "SLICES", itoa(num_slices[1], buf, 10), 1);
  }
  else if (selected_slice_sample[0] != 99 && temp_int != selected_slice_sample[0])
  {
    display.print(F("NORMAL   "));
    draw_button_on_grid(33, 23, "SLICES", itoa(0, buf, 10), 0);
  }
  else if (selected_slice_sample[0] == 99)
  {
    display.print(F("NORMAL   "));
    draw_button_on_grid(33, 23, "SLICES", itoa(0, buf, 10), 1);
  }
}

FLASHMEM void check_and_print_sliced_message()
{
  if (num_slices[0] != 0 && selected_slice_sample[0] != temp_int && selected_slice_sample[0] != 99 &&
    num_slices[1] != 0 && selected_slice_sample[1] != temp_int && selected_slice_sample[1] != 99)
  {
    setCursor_textGrid_small(1, 16);
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("SLICED SAMPLES ARE CURRENTLY ASSIGNED TO: "));
    display.print(selected_slice_sample[0] + 1);
    display.print(F(" AND "));
    display.print(selected_slice_sample[1] + 1);
  }
}

FLASHMEM void draw_scrollbar_slice_editor()
{
  if (selected_slice_sample[get_current_sliced_sample_in_ui()] != 99)
    drawScrollbar(DISPLAY_WIDTH - CHAR_width_small * 2 - 3, CHAR_height_small * 3 + 6, 4, num_slices[0] * 2, generic_temp_select_menu - 6, 10);
}

extern void  assign_free_slices_or_increment();

FLASHMEM void print_empty_loop()
{
  setModeColor(3);
  show_no_grid(4 * 11 - 5, CHAR_width_small * 13, 7, "0");
  setModeColor(4);
  show_no_grid(5 * 11 - 5, CHAR_width_small * 13, 7, "0");
  setModeColor(5);
  setCursor_textGrid_small(13, 6);
  display.print(F("NONE    "));
}

FLASHMEM void print_loop_data()
{
  if (fm.sample_source == 2 || fm.sample_source == 3)
  { //MSP 1 or 2
    if (strlen(msz[fm.sample_source - 2][temp_int].filename) > 0)
    {
      setModeColor(3);
      show_no_grid(4 * 11 - 5, CHAR_width_small * 13, 7, msz[fm.sample_source - 2][temp_int].loop_start);
      setModeColor(4);
      show_no_grid(5 * 11 - 5, CHAR_width_small * 13, 7, msz[fm.sample_source - 2][temp_int].loop_end);
      setModeColor(5);
      setCursor_textGrid_small(13, 6);
      if (msz[fm.sample_source - 2][temp_int].loop_type == 0)
        display.print(F("NO LOOP "));
      else    if (msz[fm.sample_source - 2][temp_int].loop_type == 1)
        display.print(F("REPEAT  "));
      else    if (msz[fm.sample_source - 2][temp_int].loop_type == 2)
        display.print(F("PINGPONG"));
      else
        display.print(F("NONE    "));
    }
    else
    { //MSP but empty slot
      print_empty_loop();
    }
  }
  else
  { //psram - no loops
    print_empty_loop();
  }
}

FLASHMEM void UI_func_slice_editor(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    registerTouchHandler(handle_touchscreen_slice_editor);

#ifdef COMPILE_FOR_PROGMEM
    fm.sample_source = 0;
#endif

#ifdef COMPILE_FOR_SDCARD
    fm.sample_source = 0;
#endif

#ifdef COMPILE_FOR_PSRAM
    fm.sample_source = 4;
#endif

    if (selected_slice_sample[0] != 99) {
      temp_int = selected_slice_sample[0];
    }
    else {
      temp_int = 0;
    }

    fm.psram_sum_files = 0;
    fm.sd_sum_files = 0;
    seq.edit_state = 0;
    generic_temp_select_menu = 0;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    helptext_l(back_text);
    helptext_r(F("SELECT SAMPLE"));
    display.setTextSize(1);
    setCursor_textGrid_small(1, 1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("SAMPLE"));

    setCursor_textGrid_small(1, 3);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("SOURCE:"));

    char buf[3] = { 0 };

    draw_loop_buttons();

    draw_button_on_grid(17, 23, "SCRLSP", itoa(slices_scrollspeed, buf, 10), 1);
    draw_button_on_grid(25, 23, "AUTO", "ALIGN", 0);

    //draw_button_on_grid(33, 23, "SLICES", itoa(num_slices, buf, 10), 1);
    draw_button_on_grid(45, 23, "PLAY", "SAMPLE", 0);

    slice_editor_update_file_counts();
    print_available_loop_options();
    draw_header_slice_editor();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0)
      {
        if (fm.sample_source == 2 || fm.sample_source == 3) {
          generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 5);
        }
        else if (fm.sample_source == 4) {
          generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 5 + (num_slices[0] * 2));
        }
        else {
          generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 2);
        }
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu == 0)
      {
        stop_all_drum_slots();
        if (fm.sample_source == 0) {
          temp_int = constrain(temp_int + e.dir, 0, fm.sd_sum_files - 1);
        }
        else if (fm.sample_source == 1) {
          temp_int = constrain(temp_int + e.dir, 0, fm.psram_sum_files - 1);
        }
        else if (fm.sample_source == 2 || fm.sample_source == 3) {//MSP
          temp_int = constrain(temp_int + e.dir, 0, 7);
        }
        else if (fm.sample_source == 4) {
          temp_int = constrain(temp_int + e.dir, 0, NUM_DRUMSET_CONFIG - 2);
          print_sliced_status();
        }
        draw_header_slice_editor();

      }
      else if (seq.edit_state == 1 && generic_temp_select_menu == 1)
      {
        stop_all_drum_slots();
        if (fm.sample_source == 4)
        {
          assign_free_slices_or_increment();
        }
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu == 2)
      {
        stop_all_drum_slots();

#ifdef COMPILE_FOR_PROGMEM
        fm.sample_source = 0;
#endif

#ifdef COMPILE_FOR_SDCARD
        fm.sample_source = 0;
#endif

#ifdef COMPILE_FOR_PSRAM
        fm.sample_source = constrain(fm.sample_source + e.dir, 2, 4);
        if (fm.sample_source == 2 || fm.sample_source == 3)
        {
          if (temp_int > 7) {
            temp_int = 7;
          }
          display.console = true;
          display.fillRect(155, 30, 156, 38, COLOR_BACKGROUND);
          display.console = false;
        }
        // fm.sample_source = 4;
        draw_header_slice_editor();
#endif

        slice_editor_update_file_counts();
        print_available_loop_options();
        if (fm.sample_source != menuhelper_previous_val)
        {
          menuhelper_previous_val = fm.sample_source;
        }
      }

      else if (seq.edit_state == 1 && generic_temp_select_menu == 3 && fm.sample_source > 1 && fm.sample_source < 4)  // loop start
      {
        stop_all_drum_slots();
        if (LCDML.BT_checkDown())
        {
          if (msz[fm.sample_source - 2][temp_int].loop_start < drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number].len)
            msz[fm.sample_source - 2][temp_int].loop_start = msz[fm.sample_source - 2][temp_int].loop_start + slices_scrollspeed;
        }
        else if (LCDML.BT_checkUp())
        {
          if (msz[fm.sample_source - 2][temp_int].loop_start > 0)
            msz[fm.sample_source - 2][temp_int].loop_start = msz[fm.sample_source - 2][temp_int].loop_start - slices_scrollspeed;
        }
        if (msz[fm.sample_source - 2][temp_int].loop_end <= msz[fm.sample_source - 2][temp_int].loop_start)
          msz[fm.sample_source - 2][temp_int].loop_end = msz[fm.sample_source - 2][temp_int].loop_start + 1;
        if (msz[fm.sample_source - 2][temp_int].loop_start < 0)
          msz[fm.sample_source - 2][temp_int].loop_start = 0;
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu == 4 && fm.sample_source > 1 && fm.sample_source < 4) //loop end
      {
        stop_all_drum_slots();
        if (LCDML.BT_checkDown())
        {
          if (msz[fm.sample_source - 2][temp_int].loop_end < drum_config[msz[fm.sample_source - 2][temp_int].psram_entry_number].len)
            msz[fm.sample_source - 2][temp_int].loop_end = msz[fm.sample_source - 2][temp_int].loop_end + slices_scrollspeed;
        }
        else if (LCDML.BT_checkUp())
        {
          if (msz[fm.sample_source - 2][temp_int].loop_end > 1)
            msz[fm.sample_source - 2][temp_int].loop_end = msz[fm.sample_source - 2][temp_int].loop_end - slices_scrollspeed;
        }
        if (msz[fm.sample_source - 2][temp_int].loop_end <= msz[fm.sample_source - 2][temp_int].loop_start)
          msz[fm.sample_source - 2][temp_int].loop_start = msz[fm.sample_source - 2][temp_int].loop_end - 1;
        if (msz[fm.sample_source - 2][temp_int].loop_start < 0 || msz[fm.sample_source - 2][temp_int].loop_start>msz[fm.sample_source - 2][temp_int].loop_end)
          msz[fm.sample_source - 2][temp_int].loop_start = 0;
        if (msz[fm.sample_source - 2][temp_int].loop_end <= 1)
          msz[fm.sample_source - 2][temp_int].loop_end = 1;
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu == 5 && fm.sample_source > 1)
      {
        stop_all_drum_slots();
        msz[fm.sample_source - 2][temp_int].loop_type = constrain(msz[fm.sample_source - 2][temp_int].loop_type + e.dir, 0, 2);
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu > 5)
      {
        if ((generic_temp_select_menu - 6) % 2 == 0)
        {
          // slice start
          slice_start[0][(generic_temp_select_menu - 6) / 2] = slice_start[0][(generic_temp_select_menu - 6) / 2] + e.dir * slices_scrollspeed;
          if (slices_autoalign && (generic_temp_select_menu - 7) / 2 >= 0) {
            slice_end[0][(generic_temp_select_menu - 7) / 2] = slice_start[0][(generic_temp_select_menu - 6) / 2];
          }
        }
        else {
          // slice end
          slice_end[0][(generic_temp_select_menu - 6) / 2] = slice_end[0][(generic_temp_select_menu - 6) / 2] + e.dir * slices_scrollspeed;
          if (slices_autoalign && (generic_temp_select_menu - 5) / 2 >= 0 && (generic_temp_select_menu - 5) / 2 < num_slices[0]) {
            slice_start[0][(generic_temp_select_menu - 5) / 2] = slice_end[0][(generic_temp_select_menu - 6) / 2];
          }
        }

        draw_waveform_slice_editor((generic_temp_select_menu - 6) / 2);
      }
    }
    if (e.pressed)// handle button presses during menu >>>>>>>>>>>>>>>>
    {
      stop_all_drum_slots();
      seq.edit_state = !seq.edit_state;
      draw_loop_buttons();
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    display.setTextSize(1);
    setCursor_textGrid_small(9, 1);
    setModeColor(0);
    print_formatted_number(temp_int + 1, 3);

    if (fm.sample_source == 2 || fm.sample_source == 3)
    {
      if (strlen(msz[fm.sample_source - 2][temp_int].filename) < 2)
      {
        show_smallfont_noGrid(CHAR_height_small + 2, CHAR_width_small * 14, 16, "EMPTY  ");
        display.console = true;
        display.fillRect(0, 72, DISPLAY_WIDTH, 100, GREY3);
        display.console = false;
      }
      else {
        show_smallfont_noGrid(CHAR_height_small + 2, CHAR_width_small * 14, 16, msz[fm.sample_source - 2][temp_int].filename);
      }
    }

    else {//psram
      show_smallfont_noGrid(CHAR_height_small + 2, CHAR_width_small * 14, 16, drum_config[temp_int].name);
    }

    print_sliced_status();

    setModeColor(2);
    setCursor_textGrid_small(10, 3);
    if (fm.sample_source == 0)
      display.print(F("SD - CARD"));
    else if (fm.sample_source == 1)
      display.print(F("UNUSED    "));
    else if (fm.sample_source == 2)
      display.print(F("MSP #1   "));
    else if (fm.sample_source == 3)
      display.print(F("MSP #2   "));
    else if (fm.sample_source == 4)
      display.print(F("PSRAM    "));

    print_loop_data();

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
#if defined(COMPILE_FOR_PROGMEM) || defined(COMPILE_FOR_PSRAM)

    if (fm.sample_source == 4) {// PSRAM    
      draw_waveform_slice_editor(99);
    }
    else if (fm.sample_source == 2 || fm.sample_source == 3) {//MSP
      draw_waveform_loop_editor();
    }

    draw_loop_buttons();

    if (fm.sample_source == 4)
    {
      draw_scrollbar_slice_editor();
      print_updated_slices_text();
    }
#endif
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void print_content_type()
{
  set_pattern_content_type_color(seq.active_pattern);
  display.setTextSize(1);
  if (seq.content_type[seq.active_pattern] == 0)
    display.print(F("Drum/Smp."));
  else if (seq.content_type[seq.active_pattern] == 1)
    display.print(F("Instr.   "));
  else if (seq.content_type[seq.active_pattern] == 2)
    display.print(F("Chord/Arp"));
  else if (seq.content_type[seq.active_pattern] == 3)
    display.print(F("smpSlices"));
  display.setTextColor(GREY1, COLOR_BACKGROUND);
}

FLASHMEM void print_edit_mode()
{
  display.setTextSize(1);
  display.setCursor(136 - 12, 71 - 17);

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_vel_editor))
  {

    if (seq.menu < 19)
    {
      display.setTextColor(COLOR_SYSTEXT, PINK);
      display.print(F("VEL./CHORDS"));
    }
  }
  else if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
  {
    display.setTextColor(COLOR_BACKGROUND, MIDDLEGREEN);
    display.print(F("NOTE EDITOR"));
  }
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(2);
}

FLASHMEM void sub_set_color_print_track_step_detailed(uint8_t i, uint8_t currentstep)
{
  if (i > 15 - seq.pattern_len_dec && i != currentstep)  //above current pattern length setting
    display.setTextColor(GREY3, COLOR_BACKGROUND);
  else if (i == currentstep && i > 15 - seq.pattern_len_dec)
    display.setTextColor(COLOR_BACKGROUND, GREY3);
  else if (i == currentstep)
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  else
    display.setTextColor(GREY1, COLOR_BACKGROUND);

}
FLASHMEM void print_track_steps_detailed(int xpos, int ypos, uint8_t currentstep, bool init, bool allsteps)
{
  if (seq.cycle_touch_element == 0) // touch keyboard is off
  {
    // bool init = only print static content one time. if true, print the static content
    // allsteps ==  print all lines , allsteps == false print just the current step +-1 steps
    uint8_t i = 0;
    uint8_t laststep = 16;
    int y = 0;
    int x = 0;

    display.setTextSize(1);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(xpos, ypos);

    if (allsteps == false)
    {
      if (currentstep == 0)
        laststep = 2;
      else if (currentstep == 15)
      {
        i = 14;
        laststep = 16;
      }
      else
      {
        i = currentstep - 1;
        laststep = currentstep + 2;
      }
    }

    while (i < laststep)
    {
      x = xpos;
      y = ypos + 10 + i * (CHAR_height_small + 2);

      if (init)
      { // only needs to be drawn at first run

        if (i % 4 == 0)
          display.setTextColor(GREY1, COLOR_BACKGROUND);
        else
          display.setTextColor(MIDDLEGREEN, COLOR_BACKGROUND);
        display.setCursor(x, y);
        print_formatted_number(i + 1, 2);
      }
      // Short Name
      sub_set_color_print_track_step_detailed(i, currentstep);

      display.setCursor(CHAR_width_small * 4, y);

      if (seq.vel[seq.active_pattern][i] > 209) // it is a pitched Drum Sample
      {
        seq_print_current_note_from_step(i);
      }
      else
      {
        display.print(seq_find_shortname_in_track(i, seq.active_pattern)[0]);
      }
      // Data values
      sub_set_color_print_track_step_detailed(i, currentstep);
      display.setCursor(CHAR_width_small * 7, y);
      print_formatted_number(seq.note_data[seq.active_pattern][i], 3);

      // Velocity values
      sub_set_color_print_track_step_detailed(i, currentstep);
      display.setCursor(CHAR_width_small * 12, y);
      print_formatted_number(seq.vel[seq.active_pattern][i], 3);

      // Long Name / Note
      if (i > 15 - seq.pattern_len_dec && i != currentstep)  //above current pattern length setting
        display.setTextColor(GREY3, COLOR_BACKGROUND);
      else if (i == currentstep && i > 15 - seq.pattern_len_dec)
        display.setTextColor(COLOR_BACKGROUND, GREY3);
      else if (i == currentstep)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        set_pattern_content_type_color(seq.active_pattern);

      display.setCursor(CHAR_width_small * 17, y);

      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Track or Slice Track
      {
        if (seq.vel[seq.active_pattern][i] > 209 && seq.content_type[seq.active_pattern] == 0) // it is a pitched Drum Sample
        {
          show_smallfont_noGrid(y, CHAR_width_small * 17, 10, basename(drum_config[seq.vel[seq.active_pattern][i] - 210].name));
        }
        else  if (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) // it is a Slice
        {
          if (seq.note_data[seq.active_pattern][i] - 48 >= 0 && seq.note_data[seq.active_pattern][i] - 48 <= num_slices[0] + num_slices[1])
          {
            if (seq.note_data[seq.active_pattern][i] - 48 < num_slices[0])
              show_smallfont_noGrid(y, CHAR_width_small * 17, 10, drum_config[selected_slice_sample[0]].name);
            else if (seq.note_data[seq.active_pattern][i] - 48 <= num_slices[0] + num_slices[1])
              show_smallfont_noGrid(y, CHAR_width_small * 17, 10, drum_config[selected_slice_sample[1]].name);
            display.print(" #");
            if (seq.note_data[seq.active_pattern][i] - 48 < num_slices[0])
              display.print(seq.note_data[seq.active_pattern][i] - 48 + 1);
            else
              display.print(seq.note_data[seq.active_pattern][i] - 48 - num_slices[0] + 1);
          }
        }
        else // else it is a regular Drum Sample
          show_smallfont_noGrid(y, CHAR_width_small * 17, 10, get_drum_name_from_note(seq.note_data[seq.active_pattern][i]));
      }
      else if (seq.content_type[seq.active_pattern] > 0 && seq.content_type[seq.active_pattern] != 3) // Inst Track or Chord or Arp
      {
        display.setCursor(x + CHAR_width_small * 17, y);
        if (seq.note_data[seq.active_pattern][i] != 0)
        {
          if (seq.note_data[seq.active_pattern][i] == 130) // it is a latched note
          {
            if (i == currentstep) {
              display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
            }
            else {
              display.setTextColor(GREEN, COLOR_BACKGROUND);
            }
            display.write(0x7E);
            display.print("LATCH"); // Tilde Symbol for latched note
          }
          else
          {
            display.print(noteNames[seq.note_data[seq.active_pattern][i] % 12][0]);
            if (noteNames[seq.note_data[seq.active_pattern][i] % 12][1] != '\0')
            {
              display.print(noteNames[seq.note_data[seq.active_pattern][i] % 12][1]);
            }
            if (seq.vel[seq.active_pattern][i] < 200) // print octave is not a chord
            {
              display.print((seq.note_data[seq.active_pattern][i] / 12) - 1);
            }
            if (seq.vel[seq.active_pattern][i] > 199) // is a chord
            {
              display.print(" ");
              print_chord_name(i);
            }
          }
        }
      }
      while (display.getCursorX() < CHAR_width_small * 32)
      {
        sub_set_color_print_track_step_detailed(i, currentstep);
        display.print(" ");
      }
      i++;
    }
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
  }
}

FLASHMEM void seq_sub_pattern_editor_print_slicename_toprow()
{
  uint8_t sel = 0;

  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  if (activeSample - 30 < num_slices[0])
    sel = 0;
  else
  {
    sel = 1;
  }

  if (seq.menu == 0)
  {
    show(0, 1, 6, basename(drum_config[selected_slice_sample[sel]].name));
    display.print("#");

    if (activeSample - 30 >= 0 && activeSample - 30 < num_slices[0] + num_slices[1])
    {
      if (activeSample - 30 >= num_slices[0]) // sliced sample2
        print_formatted_number(activeSample - 30 - num_slices[0] + 1, 2);//display slice numbers from 1-xx not from 0-xx
      else  // sliced sample1 
        print_formatted_number(activeSample - 30 + 1, 2);//display slice numbers from 1-xx not from 0-xx
    }
  }
  else
  {
    if (seq.note_data[seq.active_pattern][seq.menu - 3] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 3] - 48 < num_slices[0])
    {
      show(0, 1, 6, basename(drum_config[selected_slice_sample[0]].name));
      display.print("#");
      print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 3] - 48 + 1, 2);
    }
    else  if (seq.note_data[seq.active_pattern][seq.menu - 3] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 3] - 48 - num_slices[0] < num_slices[0] + num_slices[1])
    {
      show(0, 1, 6, basename(drum_config[selected_slice_sample[1]].name));
      display.print("#");
      print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 3] - 48 - num_slices[0] + 1, 2);
    }
  }
}

FLASHMEM void seq_sub_velocity_editor_print_slicename_toprow()
{
  display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  if (seq.note_data[seq.active_pattern][seq.menu - 1] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 1] - 48 < num_slices[0])
  {
    show(0, 6, 5, basename(drum_config[selected_slice_sample[0]].name));
    display.print("#");
    print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 1] - 48 + 1, 2);
  }
  else  if (seq.note_data[seq.active_pattern][seq.menu - 1] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 1] - 48 - num_slices[0] < num_slices[0] + num_slices[1])
  {
    show(0, 6, 5, basename(drum_config[selected_slice_sample[1]].name));
    display.print("#");
    print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 1] - 48 - num_slices[0] + 1, 2);
  }
}

FLASHMEM void print_seq_step_slicename_textonly()
{
  char name[10];
  if (seq.note_data[seq.active_pattern][seq.menu - 3] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 3] - 48 < num_slices[0])
  {
    snprintf_P(name, 6, PSTR("%s"), basename(drum_config[selected_slice_sample[0]].name));
    display.print(name);
    display.print("#");
    print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 3] - 48 + 1, 2);
  }
  else  if (seq.note_data[seq.active_pattern][seq.menu - 3] - 48 >= 0 && seq.note_data[seq.active_pattern][seq.menu - 3] - 48 - num_slices[0] < num_slices[0] + num_slices[1])
  {
    snprintf_P(name, 6, PSTR("%s"), basename(drum_config[selected_slice_sample[1]].name));
    display.print(name);
    display.print("#");
    print_formatted_number(seq.note_data[seq.active_pattern][seq.menu - 3] - 48 - num_slices[0] + 1, 2);
  }
}

FLASHMEM void print_active_slice_textonly()
{
  char name[10];
  uint8_t sel = 0;

  if (activeSample - 30 < num_slices[0])
    sel = 0;
  else
  {
    sel = 1;
  }

  snprintf_P(name, 6, PSTR("%s"), basename(drum_config[selected_slice_sample[sel]].name));
  display.print(name);
  display.print("#");

  if (activeSample - 30 >= 0 && activeSample - 30 < num_slices[0] + num_slices[1])
  {
    if (activeSample - 30 >= num_slices[0]) // sliced sample2
      print_formatted_number(activeSample - 30 - num_slices[0] + 1, 2);//display slice numbers from 1-xx not from 0-xx
    else  // sliced sample1 
      print_formatted_number(activeSample - 30 + 1, 2);//display slice numbers from 1-xx not from 0-xx
  }
}

FLASHMEM void _velocity_editor_change_pattern()
{
  display.setTextSize(2);
  if (seq.active_function != 99)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid(14, 0);
  display.print("[");
  print_formatted_number(seq.active_pattern, 2);
  display.print("]");
  setCursor_textGrid(0, 1);
  seq_printAllSeqSteps();
  seq_printAllVelocitySteps();
  if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Mode or Slices
  {
    if (seq.note_editor_view != 0)
    {
      seq.note_editor_view = 0;
      if (seq.cycle_touch_element == 0) // touch keyboard is off
        border3_clear();

      print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
    }
    else
      print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, false, true);
  }
  else
  {
    if (seq.note_editor_view != 1)
    {
      seq.note_editor_view = 1;
      if (seq.cycle_touch_element == 0) // touch keyboard is off
        border3_clear();
      if (seq.cycle_touch_element != 1)
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, true);
    }
    else if (seq.active_function == 99)
    {
      if (seq.cycle_touch_element != 1)
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, false);
    }
  }
}

FLASHMEM void UI_func_seq_vel_editor(uint8_t param)
{
  char tmp[5];
  uint8_t prev_option = 99; // avoid screen flicker at start / end of menu options
  if (LCDML.FUNC_setup())   // ****** SETUP *********
  {
    // setup function
    registerTouchHandler(handle_touchscreen_pattern_editor);
    registerScope(216, 0, button_size_x * CHAR_width_small, 50, true); // only draw when seq running
    virtual_keyboard_smart_preselect_mode();
    if (seq.cycle_touch_element != 1)
      draw_button_on_grid(45, 16, "", "", 0); // clear button
    if (seq.menu_status != 1)
    {
      display.fillScreen(COLOR_BACKGROUND);
      seq_pattern_editor_update_dynamic_elements();
    }
    encoderDir[ENC_R].reset();
    print_edit_mode();
    seq.menu = 1;
    print_shortcut_navigator();
    display.setTextSize(2);
    display.fillRect(0, 3 * CHAR_height + 17, 212, 8, COLOR_BACKGROUND);
    setCursor_textGrid(14, 0);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("[");
    setCursor_textGrid(15, 0);
    print_formatted_number(seq.active_pattern, 2);
    setCursor_textGrid(17, 0);
    display.print("]");
    if (seq.menu_status != 1)
    {
      display.setCursor(0, CHAR_height * 3 + 3);
      display.setTextSize(1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(F("CONT.TYPE:"));
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
      seq_printAllVelocitySteps();
    }
    display.setTextSize(2);
    seq_printAllSeqSteps();

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
      apc_fader_control(0, 0);
#endif

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.active_function == 99)
      {
        if (e.down) {
          prev_menu_item = seq.menu;
          seq.menu = constrain(seq.menu + 1, 0, 17);
          pattern_editor_play_current_step(seq.menu - 1);
        }
        else { // up
          if (seq.menu == 0)
          {
            seq.menu_status = 2;
            LCDML.OTHER_jumpToFunc(UI_func_seq_pattern_editor);
          }
          else
          {
            seq.menu_status = 0;
            seq.menu = constrain(seq.menu - 1, 0, 17);
            pattern_editor_play_current_step(seq.menu - 1);
          }
        }
      }
      else if (seq.active_function == 0 && seq.menu == 0)
      {
        seq.active_pattern = constrain(seq.active_pattern + e.dir * 1, 0, NUM_SEQ_PATTERN - 1);
        display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
        print_content_type();
        virtual_keyboard_smart_preselect_mode();
        update_latch_button();
      }
      else if (seq.active_function == 0 && seq.menu > 0 && seq.menu < 17)
      {
        if (seq.note_data[seq.active_pattern][seq.menu - 1] > 0)
        {
          if (seq.vel[seq.active_pattern][seq.menu - 1] < 210) // it is a normal sample
          {
            if ((seq.active_function == 0 && seq.content_type[seq.active_pattern] < 2) || (seq.active_function == 0 && seq.content_type[seq.active_pattern] == 3))
            { // if is Drum or normal Instrument Track
              seq.vel[seq.active_pattern][seq.menu - 1] = constrain(seq.vel[seq.active_pattern][seq.menu - 1] + e.dir * ENCODER[ENC_R].speed(), 0, 127);
            }
            else if (seq.active_function == 0 && seq.content_type[seq.active_pattern] > 1)
            { // is in Chord or Arp Mode
              seq.vel[seq.active_pattern][seq.menu - 1] = constrain(seq.vel[seq.active_pattern][seq.menu - 1] + e.dir * 1, 200, 205);
            }
          }
          else
          {
            seq.vel[seq.active_pattern][seq.menu - 1] = constrain(seq.vel[seq.active_pattern][seq.menu - 1] + e.dir * 1, 210, 217);
          }
        }
      }
      else if (seq.active_function == 0 && seq.menu == 17) // edit content type of current pattern
      {
        prev_option = seq.note_editor_view;
        seq.content_type[seq.active_pattern] = constrain(seq.content_type[seq.active_pattern] + e.dir * 1, 0, 3);

        seq_printAllSeqSteps();
        border3_clear();
        if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
        {
          seq.note_editor_view = 0;
          print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
        }
        else
        {
          seq.note_editor_view = 1;
          print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, true);
        }
      }
    }

    if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    {
      if (seq.active_function == 99)
      {
        seq.active_function = 0;
      }
      else if (seq.active_function == 0)
      {
        seq.active_function = 99;
      }
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    // if ( seq.content_type[seq.active_pattern] > 1 && seq.vel[seq.active_pattern][seq.menu - 1] < 200)
    //   seq.vel[seq.active_pattern][seq.menu - 1] = 200;

    if (seq.active_function == 0)
    {
      display.setTextSize(1);
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
      display.setTextSize(2);
    }
    if (seq.menu == 0 && seq.active_function == 99)
    {
      display.setTextSize(1);
      display.setCursor(0, 3 * CHAR_height + 17);
      display.setTextColor(GREEN, COLOR_BACKGROUND);
      display.print(F("SELECT PATTERN TO EDIT"));
      print_empty_spaces(8, 1);
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      print_empty_spaces(14, 1);
    }
    if (seq.menu == 0)
    {
      _velocity_editor_change_pattern();
    }
    else if (seq.menu == 1)
    {
      display.setTextSize(2);
      setCursor_textGrid(14, 0);
      display.print(" ");
      setCursor_textGrid(17, 0);
      display.print(" ");

      setCursor_textGrid(0, 1);
      if (seq.active_function == 99)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_SYSTEXT, RED);
      setCursor_textGrid(0, 1);
      display.print(seq_find_shortname(0)[0]);

      set_pattern_content_type_color(seq.active_pattern);
      setCursor_textGrid(1, 1);
      display.print(seq_find_shortname(1)[0]);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
    else if (seq.menu > 1 && seq.menu < 17)
    {
      display.setTextSize(2);
      //set_pattern_content_type_color(seq.active_pattern);
      setCursor_textGrid(seq.menu - 2, 1);

      if (seq.menu - 2 > 15 - seq.pattern_len_dec)
        display.setTextColor(GREY3, COLOR_BACKGROUND);

      else if (seq.vel[seq.active_pattern][seq.menu - 2] > 209) // if pitched sample, change color
        display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      else
        set_pattern_content_type_color(seq.active_pattern);
      display.print(seq_find_shortname(seq.menu - 2)[0]);

      setCursor_textGrid(seq.menu - 1, 1);

      if (seq.menu - 1 > 15 - seq.pattern_len_dec && seq.active_function == 99) ////////
        display.setTextColor(COLOR_BACKGROUND, GREY2);

      else   if (seq.active_function == 99)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else if (seq.menu - 1 > 15 - seq.pattern_len_dec)
        display.setTextColor(COLOR_BACKGROUND, RED);
      else
        display.setTextColor(COLOR_SYSTEXT, RED);

      display.print(seq_find_shortname(seq.menu - 1)[0]);
      set_pattern_content_type_color(seq.active_pattern);

      if (seq.menu < 16)
      {
        setCursor_textGrid(seq.menu + 0, 1);

        if (seq.menu > 15 - seq.pattern_len_dec)
          display.setTextColor(GREY3, COLOR_BACKGROUND);

        else  if (seq.vel[seq.active_pattern][seq.menu] > 209) // if pitched sample, change color
          display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
        else
          set_pattern_content_type_color(seq.active_pattern);
        display.print(seq_find_shortname(seq.menu)[0]);
      }
    }
    if (seq.menu > 0 && seq.menu < 17)
    {
      display.setTextSize(2);
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Mode or Slices
      {
        if (seq.menu - 1 == 0)
          print_track_steps_detailed(0, CHAR_height * 4 + 3, seq.menu - 1, false, true);
        else
          print_track_steps_detailed(0, CHAR_height * 4 + 3, seq.menu - 1, false, false);
      }
      else if (seq.active_function == 99)
      {
        if (seq.cycle_touch_element != 1)
          print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 1, false);
      }
      setCursor_textGrid(3, 0);
      if (seq.note_data[seq.active_pattern][seq.menu - 1] > 0)
      {
        if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // is Drumtrack or Slices
        {
          setCursor_textGrid(0, 0);
          if (seq.vel[seq.active_pattern][seq.menu - 1] < 210) // it is a normal sample
          {
            display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            display.print("V:");
            snprintf_P(tmp, sizeof(tmp), PSTR("%03d"), seq.vel[seq.active_pattern][seq.menu - 1]);
            display.print(tmp);
            display.print(" ");
            display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
            if (seq.content_type[seq.active_pattern] == 0)
              show(0, 6, 8, get_drum_name_from_note(seq.note_data[seq.active_pattern][seq.menu - 1]));
            else
              seq_sub_velocity_editor_print_slicename_toprow();
            if (seq.active_function == 99)
            {
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("EDIT VELOCITY OF STEP "));
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.print(seq.menu);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" ?    "));
              display.setTextSize(2);
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
          }
          else
          { // else it is a live-pitched sample
            display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            setCursor_textGrid(0, 0);
            display.print(F("Smp:["));
            setCursor_textGrid(13, 0);
            display.print("]");
            display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
            show(0, 5, 8, basename(drum_config[seq.vel[seq.active_pattern][seq.menu - 1] - 210].name));

            if (seq.active_function == 99)
            {
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("REPLACE "));
              display.setTextColor(COLOR_BACKGROUND, GREEN);
              display.print(basename(drum_config[seq.vel[seq.active_pattern][seq.menu - 1] - 210].name));
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" IN STEP "));
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.print(seq.menu);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" ? "));
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.setTextSize(2);
            }
          }
        }
        else
        {
          if (seq.note_data[seq.active_pattern][seq.menu - 1] != 130) // note not latched
          {
            if (seq.content_type[seq.active_pattern] < 2 || seq.content_type[seq.active_pattern] == 3)
            {
              setCursor_textGrid(0, 0);
              display.print(F("Vel:"));
              snprintf_P(tmp, sizeof(tmp), PSTR("%03d"), seq.vel[seq.active_pattern][seq.menu - 1]);
              setCursor_textGrid(4, 0);
              display.print(tmp);
              setCursor_textGrid(7, 0);
              print_empty_spaces(3, 1);
            }
            set_pattern_content_type_color(seq.active_pattern);

            setCursor_textGrid(10, 0);
            display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 1] % 12][0]);
            if (noteNames[seq.note_data[seq.active_pattern][seq.menu - 1] % 12][1] != '\0')
            {
              display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 1] % 12][1]);
            }
            display.print((seq.note_data[seq.active_pattern][seq.menu - 1] / 12) - 1);
            display.print("  ");
          }
          else
          { // note is latched
            setCursor_textGrid(0, 0);
            display.print(F("latched note "));
          }
          if (seq.content_type[seq.active_pattern] > 1 && seq.content_type[seq.active_pattern] != 3) // is not drum or inst, print chord
          {
            setCursor_textGrid(0, 0);
            display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            display.print("[");
            set_pattern_content_type_color(seq.active_pattern);
            print_chord_name(seq.menu - 1);
            display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            display.print("]");
          }
        }
        seq_printAllVelocitySteps_single_step(seq.menu - 1, GREEN);
        if (seq.menu - 1 > 0)
          seq_printAllVelocitySteps_single_step(seq.menu - 2, GREY1); // previous
        if (seq.menu - 1 < 15)
          seq_printAllVelocitySteps_single_step(seq.menu, GREY1); // next
      }
      else
      {
        setCursor_textGrid(0, 0);
        display.print(F("              "));
        if (seq.menu - 1 > 0)
          seq_printAllVelocitySteps_single_step(seq.menu - 2, GREY1); // previous
        if (seq.menu - 1 < 15)
          seq_printAllVelocitySteps_single_step(seq.menu, GREY1); // next

        // clear "Insert xyz ?"  message
        display.fillRect(0, 3 * CHAR_height + 17, 212, 8, COLOR_BACKGROUND);
      }
    }

    if (seq.menu == 17 && prev_menu_item != 17 && prev_option != seq.note_editor_view) // edit content type of pattern
    {
      if (seq.active_function != 1)
      {
        display.setCursor(0, 0);
        display.setTextSize(2);
        display.setTextColor(GREY1, COLOR_BACKGROUND);
        print_empty_spaces(15, 1);
        display.setTextSize(1);
        display.setCursor(0, 3 * CHAR_height + 17);
        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(F("EDIT CONTENT TYPE OF PAT. "));
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        display.print(seq.active_pattern);
        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(" ? ");
      }
      print_edit_mode();
      display.setTextSize(1);
      if (seq.active_function == 99)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_SYSTEXT, RED);
      display.setCursor(0, CHAR_height * 3 + 3);
      display.print("CONT.TYPE:");
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
      display.setTextSize(2);
      seq_printAllSeqSteps();
      seq_printAllVelocitySteps();
      if (seq.cycle_touch_element == 0) // touch keyboard is off
        border3_clear();
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Mode or Slices
      {
        print_track_steps_detailed(0, CHAR_height * 4 + 3, seq.menu - 1, true, true);
      }
      else
      {
        if (seq.cycle_touch_element != 1)
          print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 1, true);
      }
      prev_option = 99;
    }

    else if (seq.menu == 16)
    {
      display.setTextSize(1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.setCursor(0, CHAR_height * 3 + 3);
      display.print(F("CONT.TYPE:"));
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
      display.setTextSize(2);
      print_edit_mode();
    }
    virtual_keyboard_smart_preselect_mode();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_R].reset();

    if (seq.menu_status != 2) // don't clear screen when jumping (back) to pattern editor
    {
      display.fillScreen(COLOR_BACKGROUND);
      seq.menu_status = 0;
    }
  }
}

FLASHMEM void seq_clear_active_pattern()
{
  memset(seq.note_data[seq.active_pattern], 0, sizeof(seq.note_data[seq.active_pattern]));
  memset(seq.vel[seq.active_pattern], 0, sizeof(seq.vel[seq.active_pattern]));
}

FLASHMEM void seq_clear_all_patterns()
{
  for (uint8_t i = 0; i < NUM_SEQ_PATTERN - 1; i++)
  {
    memset(seq.note_data[i], 0, sizeof(seq.note_data[i]));
    memset(seq.vel[i], 0, sizeof(seq.vel[i]));
    seq.content_type[i] = 0;
  }
  for (uint8_t i = 0; i < NUM_SEQ_TRACKS - 1; i++)
  {
    seq.track_type[i] = 0;
  }
}

FLASHMEM void seq_clear_song_data()
{
  for (uint8_t i = 0; i < NUM_SEQ_TRACKS; i++)
  {
    memset(seq.song[i], 99, sizeof(seq.song[i]));
  }
}

FLASHMEM void seq_clear_chain_data()
{
  for (uint8_t i = 0; i < NUM_CHAINS; i++)
  {
    memset(seq.chain[i], 99, sizeof(seq.chain[i]));
    memset(seq.chain_transpose[i], 99, sizeof(seq.chain_transpose[i]));
  }
}

void seq_refresh_display_play_status()
{
  display.fillRect(12 * CHAR_width, 0, 13, 16, COLOR_BACKGROUND);
  if (seq.running == false && seq.recording == false)
  {
    drawBitmap(12 * CHAR_width + 3, 0 + 3, special_chars[19], 8, 8, GREEN);
  }
  else if (seq.running == true && seq.recording == false)
  {
    seq.note_in = 0;
    drawBitmap(12 * CHAR_width + 3, 0 + 3, special_chars[20], 8, 8, RED);
  }
  else if (seq.running == true && seq.recording == true)
  {
    seq.note_in = 0;
    drawBitmap(12 * CHAR_width + 3, 0 + 3, special_chars[21], 8, 8, COLOR_PITCHSMP);
  }
}
FLASHMEM void check_variable_samples_basespeed()
{
  for (uint8_t i = 0; i < 6; i++)
  {
    if (drum_config[i].p_offset == 0)
      drum_config[i].p_offset = 100; // %
  }
}

FLASHMEM void print_sample_type(bool dir)
{
  char text[13];
  uint8_t len = 0;

  if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.content_type[seq.active_pattern] != 3)
  {
    if (drum_config[activeSample].drum_class == 0)
      strcpy(text, "NONE");
    else if (drum_config[activeSample].drum_class == 1)
      strcpy(text, "BASSDRUM");
    else if (drum_config[activeSample].drum_class == 2)
      strcpy(text, "SNARE DRUM");
    else if (drum_config[activeSample].drum_class == 3)
      strcpy(text, "HIGH HAT");
    else if (drum_config[activeSample].drum_class == 4)
      strcpy(text, "HANDCLAP");
    else if (drum_config[activeSample].drum_class == 5)
      strcpy(text, "RIDE CYMBAL");
    else if (drum_config[activeSample].drum_class == 6)
      strcpy(text, "CRASH CYMBAL");
    else if (drum_config[activeSample].drum_class == 7)
      strcpy(text, "LOW TOM");
    else if (drum_config[activeSample].drum_class == 8)
      strcpy(text, "MID TOM");
    else if (drum_config[activeSample].drum_class == 9)
      strcpy(text, "HIGH TOM");
    else if (drum_config[activeSample].drum_class == 10)
      strcpy(text, "PERCUSSION");
    else if (drum_config[activeSample].drum_class == 11)
      strcpy(text, "POLY/SYNTH");
    else
      strcpy(text, "other");
  }
  else if (seq.content_type[seq.active_pattern] == 3) // sliced
  {
    strcpy(text, "SLICES");
  }
  else
    strcpy(text, "NONE");

  len = strlen(text);
  if (dir == 0) //left adjusted
  {
    display.print(text);
    print_empty_spaces(13 - len, 0);
  }
  else if (dir == 1) //right adjusted
  {
    print_empty_spaces(13 - len, 0);
    display.print(text);
  }
}

FLASHMEM void seq_sub_copy_swap(EncoderEvents e)
{
  if (seq.menu == 30 || seq.menu == 31)
  { // is in sub-function - swap pattern or copy pattern
    seq.active_function = 98;

    if (e.turned)
    {
      temp_int = constrain(temp_int + e.dir, 0, NUM_SEQ_PATTERN - 1);
      if (temp_int == seq.active_pattern) {
        temp_int += e.dir;
      }
      if (temp_int > NUM_SEQ_PATTERN - 1) {
        temp_int = 0;
      }
      else if (temp_int < 0) {
        temp_int = NUM_SEQ_PATTERN - 1;
      }
    }
  }
  if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {
    if (seq.menu == 31 && seq.active_function != 40) // copy patterns
    {
      memcpy(seq.note_data[temp_int], seq.note_data[seq.active_pattern], sizeof(seq.note_data[0]));
      memcpy(seq.vel[temp_int], seq.vel[seq.active_pattern], sizeof(seq.vel[0]));
      seq.content_type[temp_int] = seq.content_type[seq.active_pattern];
      seq.menu = 0;
      seq.active_function = 0;
      activeSample = 0;
      temp_int = seq.note_data[seq.active_pattern][0];
      display.setTextSize(2);
      setCursor_textGrid(14, 0);
      display.print(" ");
      setCursor_textGrid(17, 0);
      display.print(" ");
      setCursor_textGrid(1, 0);
      display.print(F("         "));
      setCursor_textGrid(16, 1);
      display.print(F("  "));
      seq_refresh_display_play_status();
      seq_printAllSeqSteps();
    }
    else if (seq.menu == 30 && seq.active_function != 40) // swap patterns
    {
      uint8_t data_temp[1][16];
      uint8_t vel_temp[1][16];
      uint8_t content_type_temp;
      memcpy(data_temp[0], seq.note_data[seq.active_pattern], sizeof(data_temp[0]));
      memcpy(vel_temp[0], seq.vel[seq.active_pattern], sizeof(vel_temp[0]));
      content_type_temp = seq.content_type[seq.active_pattern];
      memcpy(seq.note_data[seq.active_pattern], seq.note_data[temp_int], sizeof(data_temp[0]));
      memcpy(seq.vel[seq.active_pattern], seq.vel[temp_int], sizeof(vel_temp[0]));
      seq.content_type[seq.active_pattern] = seq.content_type[temp_int];
      memcpy(seq.note_data[temp_int], data_temp[0], sizeof(data_temp[0]));
      memcpy(seq.vel[temp_int], vel_temp[0], sizeof(vel_temp[0]));
      seq.content_type[temp_int] = content_type_temp;
      seq.menu = 0;
      seq.active_function = 0;
      activeSample = 0;
      border3_clear();
      display.setTextSize(2);
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
      display.setTextSize(2);
      setCursor_textGrid(14, 0);
      display.print(" ");
      setCursor_textGrid(17, 0);
      display.print(" ");
      setCursor_textGrid(1, 0);
      display.print(F("         "));
      setCursor_textGrid(16, 1);
      display.print("  ");
      temp_int = seq.note_data[seq.active_pattern][0];
      seq_refresh_display_play_status();
      seq_printAllSeqSteps();
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
        print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
      else
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 1, true);
    }

    if (seq.menu == 0 && seq.active_function == 0)
    {
      if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG + 3) || (seq.content_type[seq.active_pattern] == 3 && activeSample == NUM_DRUMSET_CONFIG + 3) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 114))
      { // swap patterns: Active pattern <-> destination pattern
        setCursor_textGrid(0, 0);
        display.setTextSize(2);
        display.print(F("SwapPattern: "));
        temp_int = seq.active_pattern + 1;
        if (temp_int > NUM_SEQ_PATTERN - 1)
          temp_int = 0;
        seq.menu = 30;
      }
      else if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG + 2) || (seq.content_type[seq.active_pattern] == 3 && activeSample == NUM_DRUMSET_CONFIG + 2) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 113))
      { // copy pattern
        setCursor_textGrid(0, 0);
        display.setTextSize(2);
        display.print(F("Copy Pattern:"));
        temp_int = seq.active_pattern + 1;
        if (temp_int > NUM_SEQ_PATTERN - 1)
          temp_int = 0;
        seq.menu = 31;
      }
    }
  }
  // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  if (seq.menu == 31)
  { // copy pattern
    display.setTextSize(2);
    setCursor_textGrid(14, 0);
    display.print("[");
    setCursor_textGrid(17, 0);
    display.print("]");
    setCursor_textGrid(0, 1);
    display.print(F("          to: [  ]"));
    setCursor_textGrid(15, 1);
    print_formatted_number(temp_int, 2);
  }
  else if (seq.menu == 30)
  { // swap pattern
    display.setTextSize(2);
    setCursor_textGrid(14, 0);
    display.print("[");
    setCursor_textGrid(17, 0);
    display.print("]");
    setCursor_textGrid(0, 1);
    display.print(F("       with:  [  ]"));
    setCursor_textGrid(15, 1);
    print_formatted_number(temp_int, 2);
  }
}

FLASHMEM void seq_sub_pattern_fill(EncoderEvents e)
{
  if (e.turned) {
    if (seq.menu == 33)
    { // is in sub-function - fill pattern
      seq.active_function = 95;
      seq.temp_active_menu = constrain(seq.temp_active_menu + e.dir, 0, 2); // fill step 1/4 , 1/8, 1/16
    }
    else if (seq.menu == 32)
    { // is in sub-function - fill pattern
      seq.active_function = 97;
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
      {
        seq.temp_select_menu = constrain(seq.temp_select_menu + e.dir, 0, NUM_DRUMSET_CONFIG - 1);
      }
      else
      {
        seq.temp_select_menu = constrain(seq.temp_select_menu + e.dir, 0, 108);
      }
    }
  }

  if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {
    if (seq.menu == 32 && seq.active_function == 97) // fill pattern every 1/4, 1/8, 1/16 step with active sample/note  step 1
    {
      seq.active_function = 96;
      seq.menu = 33;
    }
    else if (seq.menu == 33 && seq.active_function == 95) // fill pattern every 1/4, 1/8, 1/16 step with active sample/note  step 2
    {
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
      { // Drumtrack
        for (uint8_t i = 0; i < 16; i++)
        {
          seq.note_data[seq.active_pattern][i] = drum_config[seq.temp_select_menu].midinote;
          seq.vel[seq.active_pattern][i] = 120;
          if (seq.temp_active_menu == 0)
            i = i + 3;
          else if (seq.temp_active_menu == 1)
            i = i + 1;
        }
      }
      else
      { // Inst. Track
        for (uint8_t i = 0; i < 16; i++)
        {
          seq.note_data[seq.active_pattern][i] = seq.temp_select_menu;
          seq.vel[seq.active_pattern][i] = 120;
          if (seq.temp_active_menu == 0)
            i = i + 3;
          else if (seq.temp_active_menu == 1)
            i = i + 1;
        }
      }
      seq.menu = 0;
      seq.active_function = 0;
      activeSample = 0;
      temp_int = seq.note_data[seq.active_pattern][0];
      setCursor_textGrid(1, 0);
      display.setTextSize(2);
      display.print("         ");
      setCursor_textGrid(16, 1);
      display.print("  ");
      seq_refresh_display_play_status();
      seq_printAllSeqSteps();
      seq_printAllVelocitySteps();
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
        print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
      else
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 1, true);
    }
    else if (seq.menu == 0 && seq.active_function == 0)
    {
      if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG + 4) || (seq.content_type[seq.active_pattern] == 3 && activeSample == NUM_DRUMSET_CONFIG + 4) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 115))
      { // fill patterns
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        setCursor_textGrid(0, 0);
        display.setTextSize(2);
        display.print(F("Fill Pattern:"));
        seq.menu = 32;
        seq.temp_select_menu = 0;
        seq.temp_active_menu = 0;
      }
    }
  }
  // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  if (seq.menu == 33) // editor step 2
  {                   // fill pattern 2nd parameter
    setCursor_textGrid(5, 1);
    display.setTextSize(2);
    display.print(" ");
    setCursor_textGrid(12, 1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print("[");
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    if (seq.temp_active_menu == 0)
      display.print(F(" 1/4"));
    else if (seq.temp_active_menu == 1)
      display.print(F(" 1/8"));
    else if (seq.temp_active_menu == 2)
      display.print(F("1/16"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print("]");
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
  else if (seq.menu == 32) // editor step 1
  {                        // fill pattern
    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
    { // drum
      setCursor_textGrid(0, 1);
      display.setTextSize(2);

      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print("with ");
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("[");
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      show(1, 6, 6, basename(drum_config[seq.temp_select_menu].name));
      setCursor_textGrid(12, 1);
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("]");
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(13, 1);
      if (seq.temp_active_menu == 0)
        display.print(F(" 1/4"));
      else if (seq.temp_active_menu == 1)
        display.print(F(" 1/8"));
      else if (seq.temp_active_menu == 2)
        display.print(F("1/16"));
    }
    else
    { // inst
      setCursor_textGrid(0, 1);
      display.setTextSize(2);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print("with ");
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("[");
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(noteNames[seq.temp_select_menu % 12]);
      display.print((seq.temp_select_menu / 12) - 1);
      display.print(F("   "));
      setCursor_textGrid(12, 1);
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("]");
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(13, 1);
      if (seq.temp_active_menu == 0)
        display.print(F(" 1/4"));
      else if (seq.temp_active_menu == 1)
        display.print(F(" 1/8"));
      else if (seq.temp_active_menu == 2)
        display.print(F("1/16"));
    }
  }
}

FLASHMEM void seq_sub_pattern_transpose(EncoderEvents e)
{
  if (seq.menu == 34)
  { // is in transpose edit
    seq.active_function = 94;
    if (e.turned) {
      temp_int = constrain(temp_int + e.dir, -36, 36);
    }
  }
  if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {
    if (seq.menu == 34) // transpose pattern
    {
      seq.menu = 0;
      seq.active_function = 0;
      activeSample = 0;
      temp_int = seq.note_data[seq.active_pattern][0];
      setCursor_textGrid(14, 1);
      print_formatted_number(seq.active_pattern, 2);
      seq_refresh_display_play_status();
      seq_printAllSeqSteps();
      display.setTextSize(2);
      setCursor_textGrid(2, 0);
      display.print("         ");
      setCursor_textGrid(13, 0);
      display.print(" ");
      setCursor_textGrid(16, 0);
      display.print(" ");
    }
    if (seq.menu == 0 && seq.active_function == 0)
    {
      if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG + 5) || (seq.content_type[seq.active_pattern] == 3 && activeSample == NUM_DRUMSET_CONFIG + 5) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 116))
      { // transpose pattern
        setCursor_textGrid(1, 0);
        display.setTextSize(2);
        display.print(F("Transpose: [ 00] "));
        for (uint8_t i = 0; i < 16; i++)
        {
          seq.data_buffer[i] = seq.note_data[seq.active_pattern][i];
        }
        seq.menu = 34;
        temp_int = 0;
        seq.temp_select_menu = 0;
        seq.temp_active_menu = 0;
      }
    }
  }
  // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  if (seq.menu == 34 && seq.active_function == 94)
  { // transpose
    setCursor_textGrid(12, 0);
    display.setTextSize(2);
    display.print("[");
    if (temp_int > 0)
    {
      display.print("+");
    }
    else if (temp_int < 0)
    {
      display.print("-");
    }
    else
    {
      display.print(" ");
    }

    display.printf("%02d", abs(temp_int));
    display.print("]");
    for (uint8_t i = 0; i < 16; i++)
    {
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
      {                                                                                                                                                    // drums
        if (seq.data_buffer[i] != 0 && seq.data_buffer[i] + temp_int >= 0 && seq.data_buffer[i] + temp_int < 254 && seq.vel[seq.active_pattern][i] >= 210) // pitched drums only
          seq.note_data[seq.active_pattern][i] = seq.data_buffer[i] + temp_int;
      }
      else
        // instruments
        if (seq.content_type[seq.active_pattern] > 0)
        {
          if (seq.data_buffer[i] != 0 && seq.data_buffer[i] != 130 && seq.data_buffer[i] + temp_int > 0 && seq.data_buffer[i] + temp_int < 254)
            seq.note_data[seq.active_pattern][i] = seq.data_buffer[i] + temp_int;
        }
    }
    seq_printAllSeqSteps();
    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
      print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
    else
      print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 1, true);
  }
}

FLASHMEM void seq_sub_clear_pattern_or_clear_all()
{
  if (LCDML.BT_checkEnter()) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {
    if (seq.menu == 0 && seq.active_function == 0)
    {
      if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG + 1) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 112))
      { // clear all patterns
        seq_clear_all_patterns();
        seq_printAllSeqSteps();
        print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
      }
      else if ((seq.content_type[seq.active_pattern] == 0 && activeSample == NUM_DRUMSET_CONFIG) || (seq.content_type[seq.active_pattern] == 3 && activeSample == NUM_DRUMSET_CONFIG) || (seq.content_type[seq.active_pattern] > 0 && temp_int == 111))
      { // clear pattern
        seq_clear_active_pattern();
        seq_printAllSeqSteps();
        print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
      }
      seq.active_function = 99;
    }
  }
  // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
}

FLASHMEM void seq_sub_pitch_edit_pitched_sample(EncoderEvents e)
{
  if (seq.active_function == 40)
  { // pitch edit sample
    if (e.turned) {
      seq.note_data[seq.active_pattern][seq.menu - 3] = constrain(seq.note_data[seq.active_pattern][seq.menu - 3] + e.dir, 12, 108);
    }
  }
  if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  {
    if (seq.menu > 2 && seq.menu < 19 && seq.vel[seq.active_pattern][seq.menu - 3] > 209 && seq.active_function != 40 && activeSample != NUM_DRUMSET_CONFIG - 1) // enter edit pitch of sample mode
    {
      seq.active_function = 40;
    }
    else if (seq.active_function == 40) // get out of pitch edit for samples
    {
      if (seq.note_data[seq.active_pattern][seq.menu - 3] == 12)
      {
        seq.note_data[seq.active_pattern][seq.menu - 3] = 0;
        seq.vel[seq.active_pattern][seq.menu - 3] = 0;
      }
      setCursor_textGrid(11, 1);
      display.print(" ");
      seq.active_function = 0;
      activeSample = 0;
      seq_refresh_display_play_status();
      seq_printAllSeqSteps();
    }
  }
  // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  if (seq.vel[seq.active_pattern][seq.menu - 3] > 209 && activeSample != NUM_DRUMSET_CONFIG - 1 && seq.active_function == 40) // is pitched sample and selected item is not set to EMPTY
  {
    display.setTextSize(2);
    setCursor_textGrid(1, 0);
    if (seq.note_data[seq.active_pattern][seq.menu - 3] != 12)
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(F("[EDIT "));
      // seq_print_current_note_from_step(seq.menu - 3);
      display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][0]);
      if (noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1] != '\0')
      {
        display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1]);
      }
      display.print((seq.note_data[seq.active_pattern][seq.menu - 3] / 12) - 1);
      display.print(" ");
      setCursor_textGrid(10, 0);
      display.print("?]");
    }
    else
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print("[  ");
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print(F("CLEAR"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print("  ]");
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
    display.setTextSize(1);
    display.setCursor(0, 4 * CHAR_height);
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("SELECT [ "));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print("C0");
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F(" ] TO CLEAR THIS STEP"));
    fill_up_with_spaces_left_window();
    display.setTextSize(2);
  }
}

FLASHMEM void print_current_sample_and_pitch_buffer()
{
  if (seq.cycle_touch_element == 0) // touch keyboard is off
  {
    display.setTextSize(1);
    display.setCursor(36 * CHAR_width_small, 4 * (CHAR_height_small + 2) + 10);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("SAMPLE BUFFER: "));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(36 * CHAR_width_small, 5 * (CHAR_height_small + 2) + 10);

    if (seq.content_type[seq.active_pattern] == 0)
    {

      if (activeSample == NUM_DRUMSET_CONFIG - 1) // empty
      {
        display.print(F("EMPTY"));
        fill_up_with_spaces_right_window();
      }
      else
        if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.vel[seq.active_pattern][seq.menu - 3] < 210) // normal sample
        {
          display.print(basename(drum_config[activeSample].name));  //phtodo
          fill_up_with_spaces_right_window();
        }
        else if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.vel[seq.active_pattern][seq.menu - 3] > 209) // pitched sample
        {
          display.print(basename(drum_config[seq.vel[seq.active_pattern][seq.menu - 3] - 210].name));
          fill_up_with_spaces_right_window();
        }
    }
    else  if ((seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) || (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99))
    {
      if (activeSample - 30 >= 0 && activeSample - 30 < num_slices[0])
        display.print(basename(drum_config[selected_slice_sample[0]].name));
      else
        display.print(basename(drum_config[selected_slice_sample[1]].name));
      display.print("#");
      if (activeSample - 30 >= 0 && activeSample - 30 < num_slices[0])
        print_formatted_number(activeSample - 30 + 1, 2);
      else if (activeSample - 30 - num_slices[0] >= 0 && activeSample - 30 - num_slices[0] < num_slices[0] + num_slices[1])
        print_formatted_number(activeSample - 30 - num_slices[0] + 1, 2);

      fill_up_with_spaces_right_window();
    }
    display.setCursor(36 * CHAR_width_small, 6 * (CHAR_height_small + 2) + 10);
    display.setTextColor(GREY2);
    display.print(F("SAMPLE TYPE: "));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(36 * CHAR_width_small, 7 * (CHAR_height_small + 2) + 10);
    print_sample_type(0);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(36 * CHAR_width_small, 8 * (CHAR_height_small + 2) + 10);
    display.print(F("NOTE BUFFER: "));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    if (temp_int <= MIDI_C8)
    {
      display.print(noteNames[temp_int % 12]);
      display.print((temp_int / 12) - 1);
      display.print(" ");
    }
    else if (temp_int == MIDI_C8 + 1)
      display.print(F("---"));
    else
      display.print(F("   "));

    display.setTextSize(1);
    print_performance_name(CHAR_width_small * 36, 10 * (CHAR_height_small + 2) + 10);
    display.setTextSize(2);
  }
}

FLASHMEM void set_sample_type_color()
{
  if (drum_config[activeSample].midinote > 209)
    display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
  else
    display.setTextColor(COLOR_BACKGROUND, COLOR_DRUMS);
}

FLASHMEM void set_sample_type_color_of(uint8_t samplekey)
{
  if (samplekey > 209)
    display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
  else
    display.setTextColor(COLOR_BACKGROUND, COLOR_DRUMS);
}

FLASHMEM void seq_pattern_editor_update_dynamic_elements()
{
  if (seq.running == false)
  {
    if (seq.step_recording)
      draw_button_on_grid(36, 1, "RECORD", "ACTIVE", 2); // print step recorder icon
    else
      draw_button_on_grid(36, 1, "STEP", "RECORD", 1); // print step recorder icon
  }
  if (seq.cycle_touch_element == 0)
  {
    draw_button_on_grid(45, 1, "", "", 99); // print keyboard icon

    if (seq.play_mode == false) // is in full song more
      draw_button_on_grid(36, 16, "PLAYNG", "SONG", 0);
    else // play only current pattern
      draw_button_on_grid(36, 16, "LOOP", "PATT", 2);
    if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
      draw_button_on_grid(45, 16, "JUMP", "TOOLS", 0);
    else
      draw_button_on_grid(45, 16, "-", "-", 0);

    draw_button_on_grid(36, 22, "SONG", "EDITOR", 0);
    draw_button_on_grid(45, 22, "HUNT", "PATT", 0);

    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Mode or Slices
    {
      print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
      seq.note_editor_view = 0;
    }
    else
    {
      print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, true);
      seq.note_editor_view = 1;
    }
    // seq_sub_print_track_assignments(CHAR_width * 12, CHAR_height * 2, true);
    print_current_sample_and_pitch_buffer();
  }
  else if (seq.cycle_touch_element == 1)
  {
    draw_button_on_grid(45, 1, "BACK", "TO SEQ", 0);
    drawVirtualKeyboard();
    update_step_rec_buttons();
    virtual_keyboard_print_velocity_bar();
  }
  display.setTextSize(2);
}

FLASHMEM uint8_t find_track_in_song_where_pattern_is_used(uint8_t pattern)
{
  uint8_t result = 99;
  for (uint8_t s = 0; s < SONG_LENGTH; s++)
  {
    for (uint8_t t = 0; t < NUM_SEQ_TRACKS; t++)
    {
      for (uint8_t c = 0; c < 16; c++)
      {
        if (seq.chain[seq.song[t][s]][c] == pattern)
        {
          result = t;
          seq.current_track_type_of_active_pattern = seq.track_type[t];
          return result;
          break;
        }
      }
    }
  }
  return result;
}

FLASHMEM uint8_t find_first_song_step_with_pattern(uint8_t pattern)
{
  uint8_t result = 99;
  for (uint8_t s = 0; s < SONG_LENGTH; s++)
  {
    for (uint8_t t = 0; t < NUM_SEQ_TRACKS; t++)
    {
      for (uint8_t c = 0; c < 16; c++)
      {
        if (seq.chain[seq.song[t][s]][c] == pattern)
        {
          result = s;
          return result;
          break;
        }
      }
    }
  }
  return result;
}

FLASHMEM uint8_t find_first_chain_step_with_pattern(uint8_t pattern)
{
  uint8_t result = 99;
  for (uint8_t s = 0; s < SONG_LENGTH; s++)
  {
    for (uint8_t t = 0; t < NUM_SEQ_TRACKS; t++)
    {
      for (uint8_t c = 0; c < 16; c++)
      {
        if (seq.chain[seq.song[t][s]][c] == pattern)
        {
          result = c;
          return result;
          break;
        }
      }
    }
  }
  return result;
}

FLASHMEM void pattern_editor_menu_0()
{
  display.setTextSize(2);
  if (seq.menu == 0) // sound select menu
  {
    print_current_sample_and_pitch_buffer();
    if (seq.active_function != 99)
      display.setTextColor(RED, COLOR_BACKGROUND);
    else
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(11, 0);
    display.print(" ");
    setCursor_textGrid(13, 0);
    display.print(" ");
    setCursor_textGrid(0, 0);
    display.print("[");
    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum Mode or Slices
    {
      if (activeSample < NUM_DRUMSET_CONFIG - 1)
      {
        if (drum_config[activeSample].midinote < 210)
          display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
        else
          display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
        if (seq.content_type[seq.active_pattern] == 0)
          show(0, 1, 9, basename(drum_config[activeSample].name));
        else  if ((seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) || (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[1] != 99))
        {
          seq_sub_pattern_editor_print_slicename_toprow();
        }
        else
        {
          setCursor_textGrid(1, 0);
          display.print(F("NO SLICES"));
        }
        if (seq.active_function != 99)
          display.setTextColor(RED, COLOR_BACKGROUND);
        else
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      }
      else if (activeSample == NUM_DRUMSET_CONFIG - 1)
      {
        setCursor_textGrid(1, 0);
        display.print(F("EMPTY    "));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Clear Pat"));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG + 1)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Clear All"));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG + 2)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Copy Pat."));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG + 3)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Swap Pat"));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG + 4)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Fill Pat."));
      }
      else if (activeSample == NUM_DRUMSET_CONFIG + 5)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Transpose"));
      }
      setCursor_textGrid(10, 0);
      display.print("]");
    }
    else // Inst. Mode
    {
      if (temp_int < 109 && seq.content_type[seq.active_pattern] != 3)
      {
        setCursor_textGrid(1, 0);
        if (seq.content_type[seq.active_pattern] == 1) // Inst
        {
          display.print(noteNames[temp_int % 12]);
          display.print((temp_int / 12) - 1);
          display.print(F("  "));
        }
        else if (seq.content_type[seq.active_pattern] == 2) // Chord
        {
          // print_chord_name(seq.menu - 3);
          setCursor_textGrid(1, 0);
          display.print(noteNames[temp_int % 12]);
          display.print((temp_int / 12) - 1);
          display.print(F("  "));
        }
      }
      else if (temp_int == 109)
      {
        setCursor_textGrid(1, 0);
        display.print(F("EMPTY    "));
      }
      else if (temp_int == 110)
      {
        setCursor_textGrid(1, 0);
        display.print(F("LATCH    "));
      }
      else if (temp_int == 111)
      {
        setCursor_textGrid(1, 0);
        display.print(F("ClearPat."));
      }
      else if (temp_int == 112)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Clear All"));
      }
      else if (temp_int == 113)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Copy Pat."));
      }
      else if (temp_int == 114)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Swap Pat."));
      }
      else if (temp_int == 115)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Fill Pat."));
      }
      else if (temp_int == 116)
      {
        setCursor_textGrid(1, 0);
        display.print(F("Transpose"));
      }
      setCursor_textGrid(10, 0);
      display.print("]");
      if (temp_int == 108)
      {
        setCursor_textGrid(5, 0);
        display.print(F("  "));
      }
    }
  }
}

FLASHMEM void play_sample(uint8_t note) {
  handleNoteOn(drum_midi_channel, drum_config[note].midinote, 90, 0);
}

FLASHMEM void _seq_pattern_change()
{
  if (seq.cycle_touch_element != 1)
    display.fillRect(0, 3 * CHAR_height + 17, 212, 8, COLOR_BACKGROUND);
  if (seq.active_function != 99)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(2);
  setCursor_textGrid(11, 0);
  display.print(" ");
  setCursor_textGrid(13, 0);
  display.print(" ");
  setCursor_textGrid(14, 0);
  display.print("[");
  setCursor_textGrid(15, 0);
  print_formatted_number(seq.active_pattern, 2);
  setCursor_textGrid(17, 0);
  display.print("]");
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum or Slice Mode
  {
    setCursor_textGrid(0, 0); // Print current sample name when switching track and track is drum track
    display.print(" ");
    if (seq.content_type[seq.active_pattern] == 0)
    {
      if (drum_config[activeSample].midinote < 210)
        display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
      else
        display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      show(0, 1, 9, basename(drum_config[activeSample].name));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print("  ");
    }
    // else  if ((seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) ||
    // (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[1] != 99 ))
    // {
    //   //display.setTextSize(2);
    //  // seq_sub_pattern_editor_print_slicename_toprow();
    //   ;
    // }

    if (seq.note_editor_view != 0 && seq.cycle_touch_element != 1)
    {
      seq.note_editor_view = 0;
      border3_clear();
      print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, true, true);
    }
    else
    {
      if (seq.cycle_touch_element != 1)
        print_track_steps_detailed(0, CHAR_height * 4 + 3, 254, false, true);
    }
  }
  else
  {
    // Print note buffer when switching track and track is an instrument track
    setCursor_textGrid(0, 0);
    display.print(" ");
    display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
    display.print(noteNames[temp_int % 12][0]);
    if (noteNames[temp_int % 12][1] != '\0')
    {
      display.print(noteNames[temp_int % 12][1]);
    }
    display.print((temp_int / 12) - 1);
    display.print(F("       "));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    if (seq.note_editor_view != 1 && seq.cycle_touch_element != 1)
    {
      seq.note_editor_view = 1;
      border3_clear();
      print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, true);

    }
    else
    {
      if (seq.cycle_touch_element != 1)
        print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, false);
    }
  }
  display.setTextSize(2);
  seq_printAllSeqSteps();
  seq_printAllVelocitySteps();
#if defined APC
  if (APC_MODE == APC_PATTERN_EDITOR)
  {
    apc_print_volume_pads();
    check_and_clear_row3();
}
#endif

  if (seq.cycle_touch_element != 1)
    print_current_sample_and_pitch_buffer();
  // seq_sub_print_track_assignments(CHAR_width * 12, CHAR_height * 2, false);

}

extern bool apc_shift_key;

FLASHMEM void insert_pattern_step(uint8_t step)
{
#if defined APC
  if (apc_shift_key)
  {
    seq.note_data[seq.active_pattern][step] = 0;
    seq.vel[seq.active_pattern][step] = 0;
  }
  else
#endif
    if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)
    { // Drumtrack

      if (drum_config[activeSample].midinote > 209) // it selected sample is a pitched sample
      {
        // Step is empty - put in selected pitched sample with selected note value
        if (seq.note_data[seq.active_pattern][step] == 0)
        {
          seq.vel[seq.active_pattern][step] = drum_config[activeSample].midinote;
          if (temp_int > 24)
            seq.note_data[seq.active_pattern][step] = temp_int;
          else
            seq.note_data[seq.active_pattern][step] = 24; // else insert C2
          pattern_editor_play_current_step(step);
        }
        else
        { // step is an other sample, replace with selected pitched sample
          seq.vel[seq.active_pattern][step] = drum_config[activeSample].midinote;
        }
      }
      else

        if (drum_config[activeSample].midinote < 210)
        {
          // normal sample: check if note is already there, if not -> insert it, else remove it from grid.
          if (seq.note_data[seq.active_pattern][step] == drum_config[activeSample].midinote)
          {
            seq.note_data[seq.active_pattern][step] = 0;
            seq.vel[seq.active_pattern][step] = 0;
          }
          else
          {
            seq.note_data[seq.active_pattern][step] = drum_config[activeSample].midinote;
            seq.vel[seq.active_pattern][step] = 120;
            pattern_editor_play_current_step(step);
          }
        }
    }
    else
    { // Inst. Track
      if (temp_int == 109 || seq.note_data[seq.active_pattern][step] == temp_int)
      { // clear note
        seq.note_data[seq.active_pattern][step] = 0;
        seq.vel[seq.active_pattern][step] = 0;
      }
      else if (temp_int == 110)
      { // latch note
        seq.note_data[seq.active_pattern][step] = 130;
        // seq.vel[seq.active_pattern][step] = 0;
      }
      else
      {
        if (seq.note_data[seq.active_pattern][step] != temp_int)
        {

          seq.note_data[seq.active_pattern][step] = temp_int;
          if (seq.content_type[seq.active_pattern] == 2) // Chords
            seq.vel[seq.active_pattern][step] = 200;
          else // normal note
            seq.vel[seq.active_pattern][step] = 120;
          pattern_editor_play_current_step(step);
        }
      }
    }
}


FLASHMEM void UI_func_seq_pattern_editor(uint8_t param)
{

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
#if defined APC
    APC_MODE = APC_PATTERN_EDITOR;
    if (apc_scroll_message == false)
      apc_clear_grid();
    apc_print_right_buttons();

#endif
    registerTouchHandler(handle_touchscreen_pattern_editor);
    registerScope(216, 0, button_size_x * CHAR_width_small, 50, true); // only draw when seq running

    if (seq.quicknav_song_to_pattern_jump && seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] < NUM_SEQ_PATTERN)
      seq.active_pattern = seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu];

    seq.menu = 3;
    virtual_keyboard_smart_preselect_mode();
    if (seq.cycle_touch_element != 1)
      draw_button_on_grid(45, 16, "JUMP", "TOOLS", 0);

    if (seq.menu_status != 2)
    {
      display.fillScreen(COLOR_BACKGROUND);
      seq_pattern_editor_update_dynamic_elements();
      display.setCursor(0, CHAR_height * 3 + 3);
      display.setTextSize(1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(F("CONT.TYPE:"));
      display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
      print_content_type();
    }
    print_edit_mode();
    check_variable_samples_basespeed();
    temp_int = seq.note_data[seq.active_pattern][0];
    encoderDir[ENC_R].reset();
    seq.note_in = 0;
    display.setTextSize(2);
    seq_refresh_display_play_status();
    setCursor_textGrid(15, 0);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    print_formatted_number(seq.active_pattern, 2);

    if (seq.menu_status != 2)
    {
      seq_printAllSeqSteps();
      seq_printAllVelocitySteps();
#if defined APC
      apc_print_volume_pads();
#endif
    }

    display.setTextSize(2);

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
      apc_fader_control(0, 0);
#endif

  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);

    seq_sub_pitch_edit_pitched_sample(e);
    seq_sub_pattern_fill(e);
    seq_sub_copy_swap(e);
    seq_sub_pattern_transpose(e);

    if (seq.active_function == 99)
    {
      if (e.turned)
      {
        if (e.down)
        {
          if (seq.menu == 18)
          {
            seq.menu_status = 1;
            LCDML.OTHER_jumpToFunc(UI_func_seq_vel_editor);
          }
          else
          {
            seq.menu_status = 0;
            seq.menu = constrain(seq.menu + 1, 0, 18);
            pattern_editor_play_current_step(seq.menu - 3);
          }
        }
        else if (e.up)
        {
          if (seq.menu == 0 && seq.quicknav_song_to_pattern_jump == true)
          {
            // go back to song-transpose when previously navigated in from song edit
            seq.help_text_needs_refresh = true;
            seq.edit_state = true;
            seq.quicknav_pattern_to_song_jump = true;
            seq.quicknav_song_to_pattern_jump = false;
            LCDML.OTHER_jumpToFunc(UI_func_song);
          }
          else
            seq.menu = constrain(seq.menu - 1, 0, 18);
          pattern_editor_play_current_step(seq.menu - 3);
        }
      }
    }
    else if (seq.active_function == 0)
    {
      if (seq.content_type[seq.active_pattern] == 0) // is in Drumedit  mode 
      {
        if (e.turned)
        {
          activeSample = constrain(activeSample + e.dir, 0, NUM_DRUMSET_CONFIG + 5);

          if (seq.running == false && activeSample < NUM_DRUMSET_CONFIG - 1) {
            handleNoteOn(drum_midi_channel, drum_config[activeSample].midinote, 90, 0);
          }
        }
      }
      else  if (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) // is in Slice mode 
      {
        if (e.turned)
        {
          activeSample = constrain(activeSample + e.dir, 30, num_slices[0] + num_slices[1] - 1 + 30);

          if (seq.running == false)
          {
            handleNoteOn(slices_midi_channel, activeSample + 48 - 30, 100, 0);
          }
        }
      }
      else if (seq.content_type[seq.active_pattern] != 3)// is in Instrument Mode
      {
        if (e.turned)
        {
          temp_int = constrain(temp_int + e.dir, 0, 116);
        }
      }
    }
    else if (seq.active_function == 2)
    {
      if (e.turned)
      {
        seq.active_pattern = constrain(seq.active_pattern + e.dir, 0, NUM_SEQ_PATTERN - 1);

        display.setCursor(11 * CHAR_width_small, CHAR_height * 3 + 3);
        print_content_type();
        virtual_keyboard_smart_preselect_mode();
        update_latch_button();
#if defined APC
        if (APC_MODE == APC_PATTERN_EDITOR)
        {
          APC_BUTTONS_RIGHT[5] = false;
          APC_BUTTONS_RIGHT[6] = false;
          APC_BUTTONS_RIGHT[7] = false;
          apc_print_right_buttons();
      }
#endif
    }
  }
    if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    {
      if (seq.menu == 0 && seq.active_function == 0 && seq.cycle_touch_element != 1)
      {
        if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
          draw_button_on_grid(45, 16, "JUMP", "TOOLS", 0);
        display.setTextSize(2);
      }
      if (seq.menu == 0 && seq.active_function == 99)
      {
        seq.active_function = 0;
      }
      else if (seq.menu == 0 && seq.active_function == 0)
      {
        seq_sub_clear_pattern_or_clear_all();
      }
      if (seq.menu == 1 && seq.active_function != 40)
      {
        if (seq.running == false && seq.recording == false)
        {
          handleStart();
        }
        else if (seq.running == true && seq.recording == false)
        {
          seq.running = true;
          seq.recording = true;
          seq.note_in = 0;
        }
        else if (seq.running == true && seq.recording == true)
        {
          handleStop();
        }
      }
      else if (seq.menu == 2 && seq.active_function != 40)
      {
        if (seq.active_function != 2)
          seq.active_function = 2;
        else
          seq.active_function = 99;
      }
      else if (seq.menu > 2 && seq.menu < 30 && seq.active_function != 40)
      {
        if (seq.active_function == 99)
          insert_pattern_step(seq.menu - 3);
        else
          seq.active_function = 99;
        display.setTextSize(2);
        seq_printAllSeqSteps();
        seq_printAllVelocitySteps();
#if defined APC
        if (APC_MODE == APC_PATTERN_EDITOR)
          apc_print_volume_pads();

#endif
      }
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
      print_apc_source_selection_pads();
#endif
    print_shortcut_navigator();
    display.setTextSize(2);
    if (seq.menu == 0) // sound select menu
      pattern_editor_menu_0();

    else if (seq.menu == 1)
    {
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid(15, 0);
      print_formatted_number(seq.active_pattern, 2);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(0, 0);
      display.print(" ");
      setCursor_textGrid(10, 0);
      display.print(" ");
      setCursor_textGrid(14, 0);
      display.print(" ");
      setCursor_textGrid(17, 0);
      display.print(" ");
      setCursor_textGrid(11, 0);
      display.print("[");
      setCursor_textGrid(13, 0);
      display.print("]");
      seq_refresh_display_play_status();
      if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.content_type[seq.active_pattern] == 0)
      {
        if (drum_config[activeSample].midinote < 210)
          display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
        else
          display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);

        show(0, 1, 9, basename(drum_config[activeSample].name));
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      }
      else
        if ((activeSample < NUM_DRUMSET_CONFIG - 1 && seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) ||
          (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[1] != 99)) //Slices
        {
          seq_sub_pattern_editor_print_slicename_toprow();
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        }
    }

    if (seq.menu == 2)
    {
      _seq_pattern_change();
    }

    if (seq.menu == 3)
    {
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid(15, 0);
      print_formatted_number(seq.active_pattern, 2);

      setCursor_textGrid(14, 0);
      display.print(" ");
      setCursor_textGrid(17, 0);
      display.print(" ");
    }

    if (seq.menu > 2 && seq.menu < 19 && seq.active_function != 40)
    {
      if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // Drum or slice Mode
      {
        if (seq.cycle_touch_element != 1)
          print_track_steps_detailed(0, CHAR_height * 4 + 3, seq.menu - 3, false, false);
      }
      else
      {
        if (seq.cycle_touch_element != 1)
          print_single_pattern_pianoroll_in_pattern_editor(0, DISPLAY_HEIGHT, seq.active_pattern, seq.menu - 3, false);
      }
      if (seq.menu == 3)
        setCursor_textGrid(0, 1);
      else
        setCursor_textGrid(seq.menu - 3, 1);

      if (seq.menu - 3 > 15 - seq.pattern_len_dec)
        display.setTextColor(COLOR_BACKGROUND, RED);
      else
        display.setTextColor(COLOR_SYSTEXT, RED);
      display.print(seq_find_shortname(seq.menu - 3)[0]);
      if (seq.menu > 3)
      {
        setCursor_textGrid(seq.menu - 4, 1);

        if (seq.menu - 4 > 15 - seq.pattern_len_dec)
          display.setTextColor(GREY3, COLOR_BACKGROUND);

        else   if (seq.vel[seq.active_pattern][seq.menu - 4] > 209) // if pitched sample, change color
          display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
        else
          set_pattern_content_type_color(seq.active_pattern);
        display.print(seq_find_shortname(seq.menu - 4)[0]);
      }
      if (seq.menu < 18)
      {
        setCursor_textGrid(seq.menu - 2, 1);

        if (seq.menu - 2 > 15 - seq.pattern_len_dec)
          display.setTextColor(GREY3, COLOR_BACKGROUND);
        else  if (seq.vel[seq.active_pattern][seq.menu - 2] > 209) // if pitched sample, change color
          display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
        else
          set_pattern_content_type_color(seq.active_pattern);


        display.print(seq_find_shortname(seq.menu - 2)[0]);
      }
      if (seq.content_type[seq.active_pattern] > 0 && seq.vel[seq.active_pattern][seq.menu - 3] < 210 &&
        seq.note_data[seq.active_pattern][seq.menu - 3] != 0 && seq.content_type[seq.active_pattern] != 3) // is not Drum Mode and not empty, print note
      {
        setCursor_textGrid(0, 0);
        display.print(" ");
        seq_print_current_note_from_step(seq.menu - 3);
      }
      else if (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3) // is drum or slice pattern
      {
        // print current sample name on active step if not empty
        if (seq.note_data[seq.active_pattern][seq.menu - 3] > 0)
        {
          setCursor_textGrid(0, 0);
          display.print(" ");

          if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.vel[seq.active_pattern][seq.menu - 3] < 210) // normal sample
          {
            if (seq.content_type[seq.active_pattern] == 0)
              show(0, 1, 9, get_drum_name_from_note(seq.note_data[seq.active_pattern][seq.menu - 3]));
            else if ((seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) ||
              (seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[1] != 99))
              seq_sub_pattern_editor_print_slicename_toprow();  //phtodo

            // check if the same note is already there, if so ask to clear it
            if (seq.note_data[seq.active_pattern][seq.menu - 3] == drum_config[activeSample].midinote)
            {
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print("CLEAR ");
              set_sample_type_color();
              if (seq.content_type[seq.active_pattern] == 0)
                display.print(basename(drum_config[activeSample].name));
              else
                print_seq_step_slicename_textonly();
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(" FROM STEP ");
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.print(seq.menu - 2);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(" ?");
              fill_up_with_spaces_left_window();
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
            // else if there is an other note, ask to replace it, normal sample
            else if (seq.note_data[seq.active_pattern][seq.menu - 3] != drum_config[activeSample].midinote &&
              seq.note_data[seq.active_pattern][seq.menu - 3] > 0 && seq.content_type[seq.active_pattern] == 0)
            {
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("REPLACE "));
              set_sample_type_color_of(seq.note_data[seq.active_pattern][seq.menu - 3]);
              display.print(get_drum_name_from_note(seq.note_data[seq.active_pattern][seq.menu - 3]));
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" WITH "));
              set_sample_type_color();
              display.print(basename(drum_config[activeSample].name));
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" ?"));
              fill_up_with_spaces_left_window();
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
            else if (seq.note_data[seq.active_pattern][seq.menu - 3] != drum_config[activeSample].midinote &&
              seq.note_data[seq.active_pattern][seq.menu - 3] > 0 &&
              seq.content_type[seq.active_pattern] == 3 && selected_slice_sample[0] != 99) //sliced sample
            {
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("REPLACE "));
              set_sample_type_color_of(seq.note_data[seq.active_pattern][seq.menu - 3]);
              print_seq_step_slicename_textonly();
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" WITH "));
              set_sample_type_color();
              print_active_slice_textonly();
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" ?"));
              fill_up_with_spaces_left_window();
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
          }
          else
          {
            if (activeSample < NUM_DRUMSET_CONFIG - 1 && seq.vel[seq.active_pattern][seq.menu - 3] > 209 && seq.content_type[seq.active_pattern] != 3) // pitched sample
            {
              display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
              show(0, 1, 9, basename(drum_config[seq.vel[seq.active_pattern][seq.menu - 3] - 210].name));

              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.setCursor(0, 3 * CHAR_height + 17);
              display.setTextSize(1);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("EDIT/"));
              display.setTextColor(RED, COLOR_BACKGROUND);
              display.print(F("DELETE "));
              set_sample_type_color_of(seq.vel[seq.active_pattern][seq.menu - 3]);
              display.print(basename(drum_config[seq.vel[seq.active_pattern][seq.menu - 3] - 210].name));
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F(" IN "));
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
              display.print(seq.menu - 2);
              display.setTextColor(GREEN, COLOR_BACKGROUND);
              display.print(F("/16 ?"));
              fill_up_with_spaces_left_window();
              display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
          }
        }
        else if (seq.note_data[seq.active_pattern][seq.menu - 3] == 0 && seq.vel[seq.active_pattern][seq.menu - 3] < 210) // if step empty, print selected, active sample and ask if should be inserted
        {
          setCursor_textGrid(0, 0);
          display.print(" ");
          display.setTextColor(GREY2, COLOR_BACKGROUND);
          show(0, 1, 11, "--EMPTY--");
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.setCursor(0, 3 * CHAR_height + 17);
          display.setTextSize(1);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F("INSERT "));
          set_sample_type_color();
          if (seq.content_type[seq.active_pattern] == 0)
            display.print(basename(drum_config[activeSample].name));
          else
            display.print(F("SLICE"));
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F(" IN "));
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.print(seq.menu - 2);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F("/16 ?"));
          fill_up_with_spaces_left_window();
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        }
        else if (seq.active_function == 40 && activeSample != NUM_DRUMSET_CONFIG - 1)
        { // is in pitch edit function 40
          setCursor_textGrid(0, 0);
          if (seq.content_type[seq.active_pattern] == 0)
            show(0, 1, 4, basename(drum_config[activeSample].name));
          else
            seq_sub_pattern_editor_print_slicename_toprow();
          setCursor_textGrid(6, 0);
          seq_print_current_note_from_step(seq.menu - 3);
        }
      }
    }
    // instr. pitch edit/delete/exchange
    if (seq.menu > 2 && seq.menu - 3 < 16 && seq.content_type[seq.active_pattern] > 0 && seq.content_type[seq.active_pattern] != 3)
    {
      if (seq.note_data[seq.active_pattern][seq.menu - 3] == 0) // insert note buffer if step is empty
      {
        display.setCursor(0, 3 * CHAR_height + 17);
        display.setTextSize(1);

        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(F("INSERT NOTE "));
        display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
        display.print(noteNames[temp_int % 12][0]);
        if (noteNames[temp_int % 12][1] != '\0')
        {
          display.print(noteNames[temp_int % 12][1]);
        }
        display.print(temp_int / 12 - 1);
        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(F(" IN STEP "));
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        display.print(seq.menu - 2);
        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(F(" ?   "));
        display.setTextSize(2);
      }
      else // change note if different
        if (seq.note_data[seq.active_pattern][seq.menu - 3] > 0 && seq.note_data[seq.active_pattern][seq.menu - 3] != temp_int)
        {
          display.setCursor(0, 3 * CHAR_height + 17);
          display.setTextSize(1);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F("CHANGE "));
          display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
          display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][0]);
          if (noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1] != '\0')
          {
            display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1]);
          }
          display.print((seq.note_data[seq.active_pattern][seq.menu - 3] / 12) - 1);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(" > ");
          display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
          display.print(noteNames[temp_int % 12][0]);
          if (noteNames[temp_int % 12][1] != '\0')
          {
            display.print(noteNames[temp_int % 12][1]);
          }
          display.print(temp_int / 12 - 1);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F(" IN STEP "));
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.print(seq.menu - 2);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(" ? ");
          display.setTextSize(2);
        }
        else if (seq.note_data[seq.active_pattern][seq.menu - 3] == temp_int) // ask to delete note if it is the same
        {
          display.setCursor(0, 3 * CHAR_height + 17);
          display.setTextSize(1);
          display.setTextColor(RED, COLOR_BACKGROUND);
          display.print(F("DELETE"));
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(F(" NOTE "));
          display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
          display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][0]);
          if (noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1] != '\0')
          {
            display.print(noteNames[seq.note_data[seq.active_pattern][seq.menu - 3] % 12][1]);
          }
          display.print((seq.note_data[seq.active_pattern][seq.menu - 3] / 12) - 1);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(" IN STEP ");
          display.print(seq.menu - 2);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          display.print(" ?   ");
          display.setTextSize(2);
        }
    }
    virtual_keyboard_smart_preselect_mode();
}
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_R].reset();
    seq.menu = 0;
    seq.active_function = 99;
    display.setTextSize(2);
    if (seq.menu_status != 1) // do not clear screen, jumping to velocity edit
    {
      display.fillScreen(COLOR_BACKGROUND);
      seq.menu_status = 0;
    }
  }
}

FLASHMEM void UI_toplineInfoText(uint8_t s)
{
  // s=size 1/2 lines
  check_remote();
  if (s == 2)
    display.fillRect(0, 0, DISPLAY_WIDTH, CHAR_height_small * 2 + 3, DX_DARKCYAN);
  else if (s == 1)
    display.fillRect(0, 0, DISPLAY_WIDTH, CHAR_height_small * 1 + 3, DX_DARKCYAN);
  if (remote_active)
    display.console = false;
}

FLASHMEM void update_microsynth_instance_icons()
{
  display.setTextSize(1);
  if (microsynth_selected_instance == 0)
  {
    setCursor_textGrid_small(13, 0);
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    display.print(F("1"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    setCursor_textGrid_small(15, 0);
    display.print(F("2"));
  }
  else
  {
    setCursor_textGrid_small(13, 0);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("1"));

    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    setCursor_textGrid_small(15, 0);
    display.print(F("2"));
  }
  display.setTextColor(GREY1);
}

FLASHMEM void update_pwm_text()
{
  if (seq.cycle_touch_element != 1)
  {
    if (microsynth[microsynth_selected_instance].wave == 4 || microsynth[microsynth_selected_instance].wave == 7)
      display.setTextColor(GREY1);
    else
      display.setTextColor(GREY2);

    setCursor_textGrid_small(0, 16);
    display.print(F("PWM"));
    setCursor_textGrid_small(0, 17);
    display.print(F("SPEED"));
    display.setTextColor(GREY1);
  }
}

FLASHMEM void microsynth_refresh_lower_screen_static_text()
{
  helptext_r(F("LONG PUSH:INST.SEL.  <>SEL.PARA."));

  display.setTextColor(GREY1, COLOR_BACKGROUND);
  setCursor_textGrid_small(0, 12);
  display.print(F("FILTER"));
  setCursor_textGrid_small(0, 13);
  display.print(F("FREQ"));
  setCursor_textGrid_small(0, 14);
  display.print(F("RES"));
  setCursor_textGrid_small(9, 14);
  display.print(F("SPEED"));
  setCursor_textGrid_small(22, 12);
  display.print(F("REV. SEND"));
  setCursor_textGrid_small(22, 13);
  display.print(F("CHR. SEND"));
  setCursor_textGrid_small(22, 14);
  display.print(F("DLY. SENDS"));
  setCursor_textGrid_small(22, 16);
  display.print(F("PANORAMA"));
  setCursor_textGrid_small(22, 17);
  display.print(F("MIDI CHN."));
  setCursor_textGrid_small(22, 19);
  display.print(F("MAP VELOCITY TO FILTER FREQ"));
  setCursor_textGrid_small(22, 20);
  display.print(F("OSC"));
  setCursor_textGrid_small(22, 21);
  display.print(F("NOISE"));
  setCursor_textGrid_small(13, 16);
  display.print(F(">"));
  setCursor_textGrid_small(13, 13);
  display.print(F(">"));
}

FLASHMEM void microsynth_refresh_lower_screen_dynamic_text()
{
  if (menu_item_check(8))
  {
    setModeColor(8);
    setCursor_textGrid_small(9, 12);
    if (microsynth[microsynth_selected_instance].filter_osc_mode == 0)
      display.print(F("OFF   "));
    else if (microsynth[microsynth_selected_instance].filter_osc_mode == 1)
      display.print(F("LP12dB"));
    else if (microsynth[microsynth_selected_instance].filter_osc_mode == 2)
      display.print(F("BP12dB"));
    else if (microsynth[microsynth_selected_instance].filter_osc_mode == 3)
      display.print(F("HI12dB"));
  }
  if (menu_item_check(9))
    print_small_intbar(9, 13, microsynth[microsynth_selected_instance].filter_osc_freq_from / 100, 9, 0, 1);
  if (menu_item_check(10))
    print_small_intbar(15, 13, microsynth[microsynth_selected_instance].filter_osc_freq_to / 100, 10, 0, 1);
  if (menu_item_check(11))
    print_small_intbar(5, 14, microsynth[microsynth_selected_instance].filter_osc_resonance, 11, 0, 1);
  if (menu_item_check(12))
    print_small_intbar(15, 14, microsynth[microsynth_selected_instance].filter_osc_speed / 10, 12, 0, 1);
  if (menu_item_check(13))
    print_small_intbar(9, 16, microsynth[microsynth_selected_instance].pwm_from / 10, 13, 0, 1);
  if (menu_item_check(14))
    print_small_intbar(15, 16, microsynth[microsynth_selected_instance].pwm_to / 10, 14, 0, 1);

  if (menu_item_check(15))
  {
    setCursor_textGrid_small(10, 17);
    setModeColor(15);
    print_formatted_number(microsynth[microsynth_selected_instance].pwm_speed, 2, 15, 1);
  }
  if (menu_item_check(28))
  {
    setCursor_textGrid_small(33, 12);
    setModeColor(28);
    print_formatted_number(microsynth[microsynth_selected_instance].rev_send, 3, 28, 1);
  }
  if (menu_item_check(29))
  {
    setCursor_textGrid_small(33, 13);
    setModeColor(29);
    print_formatted_number(microsynth[microsynth_selected_instance].chorus_send, 3, 29, 1);
  }
  if (menu_item_check(30))
  {
    setCursor_textGrid_small(33, 14);
    setModeColor(30);
    print_formatted_number(microsynth[microsynth_selected_instance].delay_send[0], 3, 30, 1);
  }
  if (menu_item_check(31))
  {
    setModeColor(31);
    setCursor_textGrid_small(37, 14);
    print_formatted_number(microsynth[microsynth_selected_instance].delay_send[1], 3, 31, 1);
  }
  if (seq.cycle_touch_element != 1)
    if (menu_item_check(32))
      print_small_panbar(33, 16, microsynth[microsynth_selected_instance].pan, 32);
  if (menu_item_check(33))
  {
    setModeColor(33);
    setCursor_textGrid_small(33, 17);
    _print_midi_channel(microsynth[microsynth_selected_instance].midi_channel);
#if defined GLOW
    set_glow_show_text_no_grid(33 * CHAR_width_small, 17 * (CHAR_height_small + 2), 4, get_midi_channel_name(microsynth[microsynth_selected_instance].midi_channel), 33, 1);
#endif
  }
  if (menu_item_check(34))
  {
    setModeColor(34);
    setCursor_textGrid_small(33, 20);

    print_formatted_number(microsynth[microsynth_selected_instance].vel_mod_filter_osc, 3, 34, 1);
  }
  if (menu_item_check(35))
  {
    setModeColor(35);
    setCursor_textGrid_small(33, 21);
    print_formatted_number(microsynth[microsynth_selected_instance].vel_mod_filter_noise, 3, 35, 1);
  }
  update_pwm_text();
}

FLASHMEM void print_epiano_static_texts()
{
  display.setTextSize(1);
  helptext_l(back_text);
  helptext_r(F("< > SELECT PARAM"));
  display.setTextSize(1);
  generic_active_function = 0;
  setCursor_textGrid_small(1, 1);
  display.setTextColor(RED);
  display.print(F("ELECTRIC PIANO"));
  display.setTextColor(COLOR_SYSTEXT);
  display.setTextColor(GREY1);
  setCursor_textGrid_small(1, 3);
  display.print(F("VOICE LEVEL"));
  setCursor_textGrid_small(1, 4);
  display.print(F("PANORAMA"));
  setCursor_textGrid_small(1, 5);
  display.print(F("TRANSPOSE"));
  display.setTextColor(GREY2);
  setCursor_textGrid_small(1, 7);
  display.print(F("AUDIO"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(1, 9);
  display.print(F("DECAY"));
  setCursor_textGrid_small(1, 10);
  display.print(F("RELEASE"));
  setCursor_textGrid_small(1, 11);
  display.print(F("HARDNESS"));
  setCursor_textGrid_small(1, 12);
  display.print(F("TREBLE"));
  setCursor_textGrid_small(1, 13);
  display.print(F("STEREO"));
  setCursor_textGrid_small(1, 15);
  display.print(F("TUNE"));
  setCursor_textGrid_small(1, 16);
  display.print(F("DETUNE"));

  display.setTextColor(GREY2);
  setCursor_textGrid_small(1, 18);
  display.print(F("SYSTEM"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(1, 20);
  display.print(F("POLYPHONY"));
  setCursor_textGrid_small(1, 21);
  display.print(F("VELOCITY SENSE"));

  display.setTextColor(GREY2);
  setCursor_textGrid_small(24, 1);
  display.print(F("EFFECTS"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(24, 3);
  display.print(F("OVERDRIVE"));
  setCursor_textGrid_small(24, 5);
  display.setTextColor(GREY2);
  display.print(F("TREMOLO"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(24, 6);
  display.print(F("WIDTH"));
  setCursor_textGrid_small(24, 7);
  display.print(F("LFO"));
  display.setTextColor(GREY2);
  setCursor_textGrid_small(24, 9);
  display.print(F("CHORUS"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(24, 10);
  display.print(F("FREQUENCY"));
  setCursor_textGrid_small(24, 11);
  display.print(F("WAVEFORM"));
  setCursor_textGrid_small(24, 12);
  display.print(F("DEPTH"));
  setCursor_textGrid_small(24, 13);
  display.print(F("LEVEL"));
  setCursor_textGrid_small(24, 15);
  display.print(F("DELAYS"));
  setCursor_textGrid_small(24, 17);
  display.print(F("REVERB S."));

  display.setTextColor(GREY1);
  setCursor_textGrid_small(24, 19);
  display.print(F("LOWEST NOTE"));
  setCursor_textGrid_small(24, 20);
  display.print(F("HIGHEST NOTE"));
  setCursor_textGrid_small(24, 21);
  display.print(F("MIDI CHANNEL"));

  display.setTextColor(GREY2, COLOR_BACKGROUND);
  setCursor_textGrid_small(41, 19);
  display.print(F("["));
  setCursor_textGrid_small(45, 19);
  display.print(F("]"));
  setCursor_textGrid_small(41, 20);
  display.print(F("["));
  setCursor_textGrid_small(45, 20);
  display.print(F("]"));
}
FLASHMEM void update_selective_values_epiano()
{
  char note_name[4];
  if (menu_item_check(0))
    print_small_intbar(13, 3, configuration.epiano.sound_intensity, 0, 1, 0);
  if (generic_temp_select_menu == 0 && seq.edit_state)
    ep.setVolume(mapfloat(configuration.epiano.sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 1.0));
  if (menu_item_check(1))
    print_small_panbar(13, 4, configuration.epiano.pan, 1);
  if (generic_temp_select_menu == 1 && seq.edit_state)
    ep_stereo_panorama.panorama(mapfloat(configuration.epiano.pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
  if (menu_item_check(2))
  {
    setModeColor(2);
    setCursor_textGrid_small(13, 5);
    print_formatted_number_signed(configuration.epiano.transpose - 24, 2, 2, 1);
  }
  if (menu_item_check(3))
    print_small_intbar(13, 9, configuration.epiano.decay, 3, 1, 1);
  if (generic_temp_select_menu == 3 && seq.edit_state)
    ep.setDecay(mapfloat(configuration.epiano.decay, EP_DECAY_MIN, EP_DECAY_MAX, 0, 1.0));
  if (menu_item_check(4))
    print_small_intbar(13, 10, configuration.epiano.release, 4, 1, 1);
  if (generic_temp_select_menu == 4 && seq.edit_state)
    ep.setRelease(mapfloat(configuration.epiano.release, EP_RELEASE_MIN, EP_RELEASE_MAX, 0, 1.0));
  if (menu_item_check(5))
    print_small_intbar(13, 11, configuration.epiano.hardness, 5, 1, 1);
  if (generic_temp_select_menu == 5 && seq.edit_state)
    ep.setHardness(mapfloat(configuration.epiano.hardness, EP_HARDNESS_MIN, EP_HARDNESS_MAX, 0, 1.0));
  if (menu_item_check(6))
    print_small_intbar(13, 12, configuration.epiano.treble, 6, 1, 1);
  if (generic_temp_select_menu == 6 && seq.edit_state)
    ep.setTreble(mapfloat(configuration.epiano.treble, EP_TREBLE_MIN, EP_TREBLE_MAX, 0, 1.0));
  if (menu_item_check(7))
    print_small_intbar(13, 13, configuration.epiano.stereo, 7, 1, 1);
  if (generic_temp_select_menu == 7 && seq.edit_state)
    ep.setStereo(mapfloat(configuration.epiano.stereo, EP_STEREO_MIN, EP_STEREO_MAX, 0, 1.0));
  if (menu_item_check(8))
  {
    setModeColor(8);
    setCursor_textGrid_small(13, 15);
    print_formatted_number_signed(configuration.epiano.tune - 100, 3, 8, 1);
  }
  if (generic_temp_select_menu == 8 && seq.edit_state)
    ep.setTune(mapfloat(configuration.epiano.tune, EP_TUNE_MIN, EP_TUNE_MAX, 0.0, 1.0));
  if (menu_item_check(9))
    print_small_intbar(13, 16, configuration.epiano.detune, 9, 1, 1);
  if (generic_temp_select_menu == 9 && seq.edit_state)
    ep.setDetune(mapfloat(configuration.epiano.detune, EP_DETUNE_MIN, EP_DETUNE_MAX, 0, 1.0));
  if (menu_item_check(10))
  {
    setModeColor(10);
    setCursor_textGrid_small(17, 20);
    print_formatted_number(configuration.epiano.polyphony, 2, 10, 1);
  }
  if (generic_temp_select_menu == 10 && seq.edit_state)
    ep.setPolyphony(configuration.epiano.polyphony);
  if (menu_item_check(11))
    print_small_intbar(17, 21, configuration.epiano.velocity_sense, 11, 0, 1);
  if (generic_temp_select_menu == 11 && seq.edit_state)
    ep.setVelocitySense(mapfloat(configuration.epiano.velocity_sense, EP_VELOCITY_SENSE_MIN, EP_VELOCITY_SENSE_MAX, 0, 1.0));
  if (menu_item_check(12))
    print_small_intbar(34, 3, configuration.epiano.overdrive, 12, 1, 0);
  if (generic_temp_select_menu == 12 && seq.edit_state)
    ep.setOverdrive(mapfloat(configuration.epiano.overdrive, EP_OVERDRIVE_MIN, EP_OVERDRIVE_MAX, 0, 1.0));
  if (menu_item_check(13))
    print_small_intbar(34, 6, configuration.epiano.pan_tremolo, 13, 1, 0);
  if (generic_temp_select_menu == 13 && seq.edit_state)
  {
    if (configuration.epiano.pan_tremolo == 0)
      ep.setPanTremolo(0.0);
    else
      ep.setPanTremolo(mapfloat(configuration.epiano.pan_tremolo, EP_PAN_TREMOLO_MIN, EP_PAN_TREMOLO_MAX, 0.0, 1.0));
  }
  if (menu_item_check(14))
    print_small_intbar(34, 7, configuration.epiano.pan_lfo, 14, 1, 0);
  if (generic_temp_select_menu == 14 && seq.edit_state)
  {
    if (configuration.epiano.pan_lfo == 0)
      ep.setPanLFO(0.0);
    else
      ep.setPanLFO(mapfloat(configuration.epiano.pan_lfo, EP_PAN_LFO_MIN, EP_PAN_LFO_MAX, 0.0, 1.0));
  }
  if (menu_item_check(15))
    print_small_intbar(34, 10, configuration.fx.ep_chorus_frequency, 15, 1, 0);
  if (generic_temp_select_menu == 15 && seq.edit_state)
    ep_chorus_modulator.frequency(configuration.fx.ep_chorus_frequency / 10.0);
  if (menu_item_check(16))
  {
    setModeColor(16);
    setCursor_textGrid_small(34, 11);
    switch (configuration.fx.ep_chorus_waveform)
    {
    case 0:
      if (generic_temp_select_menu == 16 && seq.edit_state)
        ep_chorus_modulator.begin(WAVEFORM_TRIANGLE);
      display.print(F("TRIANGLE"));
      break;
    case 1:
      if (generic_temp_select_menu == 16 && seq.edit_state)
        ep_chorus_modulator.begin(WAVEFORM_SINE);
      display.print(F("SINE    "));
      break;
    default:
      if (generic_temp_select_menu == 16 && seq.edit_state)
        ep_chorus_modulator.begin(WAVEFORM_TRIANGLE);
      display.print(F("TRIANGLE"));
      break;
    }
  }
  if (menu_item_check(17))
    print_small_intbar(34, 12, configuration.fx.ep_chorus_depth, 17, 1, 0);
  if (generic_temp_select_menu == 17 && seq.edit_state)
    ep_chorus_modulator.amplitude(mapfloat(configuration.fx.ep_chorus_depth, EP_CHORUS_DEPTH_MIN, EP_CHORUS_DEPTH_MAX, 0.0, 1.0));
  if (menu_item_check(18))
    print_small_intbar(34, 13, configuration.fx.ep_chorus_level, 18, 1, 0);
  if (generic_temp_select_menu == 18 && seq.edit_state)
  {
    ep_chorus_mixer_r.gain(1, mapfloat(configuration.fx.ep_chorus_level, EP_CHORUS_LEVEL_MIN, EP_CHORUS_LEVEL_MAX, 0.0, 0.5));
    ep_chorus_mixer_l.gain(1, mapfloat(configuration.fx.ep_chorus_level, EP_CHORUS_LEVEL_MIN, EP_CHORUS_LEVEL_MAX, 0.0, 0.5));
  }
  if (menu_item_check(19))
  {
    setModeColor(19);
    setCursor_textGrid_small(34, 15);
    print_formatted_number(configuration.fx.ep_delay_send_1, 3, 19, 1);
  }
  if (generic_temp_select_menu == 19 && seq.edit_state)
  {
    global_delay_in_mixer[0]->gain(7, mapfloat(configuration.fx.ep_delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//prev 5
    global_delay_in_mixer[0]->gain(8, mapfloat(configuration.fx.ep_delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//prev 6 
  }
  if (menu_item_check(20))
  {
    setCursor_textGrid_small(38, 15);
    setModeColor(20);
    print_formatted_number(configuration.fx.ep_delay_send_2, 3, 20, 1);
  }
  if (generic_temp_select_menu == 20 && seq.edit_state)
  {
    global_delay_in_mixer[1]->gain(7, mapfloat(configuration.fx.ep_delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//prev 5
    global_delay_in_mixer[1]->gain(8, mapfloat(configuration.fx.ep_delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//prev 6
  }
  if (menu_item_check(21))
    print_small_intbar(34, 17, configuration.fx.ep_reverb_send, 21, 1, 0);
  if (generic_temp_select_menu == 21 && seq.edit_state)
  {
    reverb_mixer_r.gain(REVERB_MIX_CH_EPIANO, volume_transform(mapfloat(configuration.fx.ep_reverb_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
    reverb_mixer_l.gain(REVERB_MIX_CH_EPIANO, volume_transform(mapfloat(configuration.fx.ep_reverb_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
  }
  if (menu_item_check(22))
  {
    setModeColor(22);
    setCursor_textGrid_small(37, 19);
    getNoteName(note_name, configuration.epiano.lowest_note);
    display.print(note_name);
    setCursor_textGrid_small(42, 19);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    print_formatted_number(configuration.epiano.lowest_note, 3);
  }
  if (menu_item_check(23))
  {
    setModeColor(23);
    setCursor_textGrid_small(37, 20);
    getNoteName(note_name, configuration.epiano.highest_note);
    display.print(note_name);
    setCursor_textGrid_small(42, 20);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    print_formatted_number(configuration.epiano.highest_note, 3);
  }
  if (menu_item_check(24))
  {
    setModeColor(24);
    setCursor_textGrid_small(37, 21);
    _print_midi_channel(configuration.epiano.midi_channel);
#if defined GLOW
    set_glow_show_text_no_grid(37 * CHAR_width_small, 21 * (CHAR_height_small + 2), 4, get_midi_channel_name(configuration.epiano.midi_channel), 24, 1);
#endif
  }
}

FLASHMEM void UI_func_epiano(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    print_epiano_static_texts();
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
      generic_temp_select_menu = 0;

    generic_full_draw_required = true;
    update_selective_values_epiano(); // print all at init
    generic_full_draw_required = false;

    display.setTextSize(1);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 24);
      }
      else {
        if (generic_temp_select_menu == 0)
        {
          configuration.epiano.sound_intensity = constrain(configuration.epiano.sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 7, configuration.epiano.sound_intensity);
        }
        else if (generic_temp_select_menu == 1)
        {
          configuration.epiano.pan = constrain(configuration.epiano.pan + e.dir * e.speed, PANORAMA_MIN, PANORAMA_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 10, map(configuration.epiano.pan, PANORAMA_MIN, PANORAMA_MAX, 0, 127));
        }
        else if (generic_temp_select_menu == 2)
          configuration.epiano.transpose = constrain(configuration.epiano.transpose + e.dir * e.speed, EP_TRANSPOSE_MIN, EP_TRANSPOSE_MAX);
        else if (generic_temp_select_menu == 3)
          configuration.epiano.decay = constrain(configuration.epiano.decay + e.dir * e.speed, EP_DECAY_MIN, EP_DECAY_MAX);
        else if (generic_temp_select_menu == 4)
          configuration.epiano.release = constrain(configuration.epiano.release + e.dir * e.speed, EP_RELEASE_MIN, EP_RELEASE_MAX);
        else if (generic_temp_select_menu == 5)
          configuration.epiano.hardness = constrain(configuration.epiano.hardness + e.dir * e.speed, EP_HARDNESS_MIN, EP_HARDNESS_MAX);
        else if (generic_temp_select_menu == 6)
          configuration.epiano.treble = constrain(configuration.epiano.treble + e.dir * e.speed, EP_TREBLE_MIN, EP_TREBLE_MAX);
        else if (generic_temp_select_menu == 7)
          configuration.epiano.stereo = constrain(configuration.epiano.stereo + e.dir * e.speed, EP_STEREO_MIN, EP_STEREO_MAX);
        else if (generic_temp_select_menu == 8)
        {
          configuration.epiano.tune = constrain(configuration.epiano.tune + e.dir * e.speed, EP_TUNE_MIN, EP_TUNE_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 94, configuration.epiano.tune);
        }
        else if (generic_temp_select_menu == 9)
          configuration.epiano.detune = constrain(configuration.epiano.detune + e.dir * e.speed, EP_DETUNE_MIN, EP_DETUNE_MAX);
        else if (generic_temp_select_menu == 10)
          configuration.epiano.polyphony = constrain(configuration.epiano.polyphony + e.dir, EP_POLYPHONY_MIN, EP_POLYPHONY_MAX);
        else if (generic_temp_select_menu == 11)
          configuration.epiano.velocity_sense = constrain(configuration.epiano.velocity_sense + e.dir * e.speed, EP_VELOCITY_SENSE_MIN, EP_VELOCITY_SENSE_MAX);
        else if (generic_temp_select_menu == 12)
          configuration.epiano.overdrive = constrain(configuration.epiano.overdrive + e.dir * e.speed, EP_OVERDRIVE_MIN, EP_OVERDRIVE_MAX);
        else if (generic_temp_select_menu == 13)
          configuration.epiano.pan_tremolo = constrain(configuration.epiano.pan_tremolo + e.dir * e.speed, EP_PAN_TREMOLO_MIN, EP_PAN_TREMOLO_MAX);
        else if (generic_temp_select_menu == 14)
          configuration.epiano.pan_lfo = constrain(configuration.epiano.pan_lfo + e.dir * e.speed, EP_PAN_LFO_MIN, EP_PAN_LFO_MAX);
        else if (generic_temp_select_menu == 15)
          configuration.fx.ep_chorus_frequency = constrain(configuration.fx.ep_chorus_frequency + e.dir * e.speed, EP_CHORUS_FREQUENCY_MIN, EP_CHORUS_FREQUENCY_MAX);
        else if (generic_temp_select_menu == 16)
          configuration.fx.ep_chorus_waveform = constrain(configuration.fx.ep_chorus_waveform + e.dir, EP_CHORUS_WAVEFORM_MIN, EP_CHORUS_WAVEFORM_MAX);
        else if (generic_temp_select_menu == 17)
          configuration.fx.ep_chorus_depth = constrain(configuration.fx.ep_chorus_depth + e.dir * e.speed, EP_CHORUS_DEPTH_MIN, EP_CHORUS_DEPTH_MAX);
        else if (generic_temp_select_menu == 18)
        {
          configuration.fx.ep_chorus_level = constrain(configuration.fx.ep_chorus_level + e.dir * e.speed, EP_CHORUS_LEVEL_MIN, EP_CHORUS_LEVEL_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 93, configuration.fx.ep_chorus_level);
        }
        else if (generic_temp_select_menu == 19)
        {
          configuration.fx.ep_delay_send_1 = constrain(configuration.fx.ep_delay_send_1 + e.dir * e.speed, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
        }
        else if (generic_temp_select_menu == 20)
        {
          configuration.fx.ep_delay_send_2 = constrain(configuration.fx.ep_delay_send_2 + e.dir * e.speed, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
        }
        else if (generic_temp_select_menu == 21)
        {
          configuration.fx.ep_reverb_send = constrain(configuration.fx.ep_reverb_send + e.dir * e.speed, REVERB_SEND_MIN, REVERB_SEND_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 91, configuration.fx.ep_reverb_send);
        }
        else if (generic_temp_select_menu == 22)
          configuration.epiano.lowest_note = constrain(configuration.epiano.lowest_note + e.dir * e.speed, EP_LOWEST_NOTE_MIN, EP_LOWEST_NOTE_MAX);
        else if (generic_temp_select_menu == 23)
          configuration.epiano.highest_note = constrain(configuration.epiano.highest_note + e.dir * e.speed, EP_HIGHEST_NOTE_MIN, EP_HIGHEST_NOTE_MAX);
        else if (generic_temp_select_menu == 24)
          configuration.epiano.midi_channel = constrain(configuration.epiano.midi_channel + e.dir, EP_MIDI_CHANNEL_MIN, EP_MIDI_CHANNEL_MAX + 1);
      }
    }
    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    update_selective_values_epiano();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    generic_menu = 0;
    // generic_active_function = 99;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void print_static_texts_microsynth()
{
  display.setTextSize(1);
  generic_active_function = 0;
  setCursor_textGrid_small(0, 0);
  display.setTextColor(RED);
  display.print(F("MICROSYNTH"));
  update_microsynth_instance_icons();

  setCursor_textGrid_small(0, 2);
  display.print(F("VOLUME"));
  setCursor_textGrid_small(0, 3);
  display.print(F("WAVE"));
  setCursor_textGrid_small(0, 4);
  display.print(F("COARSE"));
  setCursor_textGrid_small(0, 5);
  display.print(F("DETUNE"));

  setCursor_textGrid_small(22, 0);
  display.print(F("NOISE VOL"));
  setCursor_textGrid_small(22, 1);
  display.print(F("NOISE DCY"));
  setCursor_textGrid_small(22, 2);
  display.print(F("TRIG.WITH"));

  setCursor_textGrid_small(22, 3);
  display.print(F("FILTER"));
  setCursor_textGrid_small(22, 4);
  display.print(F("FREQ"));
  setCursor_textGrid_small(22, 5);
  display.print(F("RES"));
  setCursor_textGrid_small(32, 5);
  display.print(F("SPEED"));

  setCursor_textGrid_small(0, 7);
  display.print(F("ATTACK"));
  setCursor_textGrid_small(0, 8);
  display.print(F("DECAY"));
  setCursor_textGrid_small(0, 9);
  display.print(F("SUSTAIN"));
  setCursor_textGrid_small(0, 10);
  display.print(F("RELEASE"));

  setCursor_textGrid_small(22, 7);
  display.print(F("OSC LFO"));
  setCursor_textGrid_small(22, 8);
  display.print(F("MODE"));
  setCursor_textGrid_small(22, 9);
  display.print(F("DELAY"));
  setCursor_textGrid_small(32, 9);
  display.print(F("SPEED"));

  update_pwm_text();

  setCursor_textGrid_small(13, 4);
  display.print(F("STEPS"));
  setCursor_textGrid_small(13, 5);
  display.print(F("CENTS"));
  setCursor_textGrid_small(16, 7);
  display.print(F("MS"));
  setCursor_textGrid_small(16, 8);
  display.print(F("MS"));
  setCursor_textGrid_small(16, 9);
  display.print(F("LEVL"));
  setCursor_textGrid_small(16, 10);
  display.print(F("MS"));

  // arrows
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  setCursor_textGrid_small(36, 4);
  display.print(F(">"));
}

FLASHMEM void update_selective_values_microsynth()
{
  display.setTextSize(1);
  if (seq.edit_state == 1)
    microsynth_update_single_setting(microsynth_selected_instance);
  if (generic_temp_select_menu == 0)
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(1);
  if (menu_item_check(0))
    print_small_intbar(9, 2, microsynth[microsynth_selected_instance].sound_intensity, 0, 1, 0);
  if (menu_item_check(1))
  {
    setModeColor(1);
    setCursor_textGrid_small(9, 3);
    if (microsynth[microsynth_selected_instance].wave == 0)
      display.print(F("SINE    "));
    else if (microsynth[microsynth_selected_instance].wave == 1)
      display.print(F("TRIANGLE"));
    else if (microsynth[microsynth_selected_instance].wave == 2)
      display.print(F("SAWTOOTH"));
    else if (microsynth[microsynth_selected_instance].wave == 3)
      display.print(F("SQUARE  "));
    else if (microsynth[microsynth_selected_instance].wave == 4)
      display.print(F("PULSE   "));
    else if (microsynth[microsynth_selected_instance].wave == 5)
      display.print(F("LM_SAW  "));
    else if (microsynth[microsynth_selected_instance].wave == 6)
      display.print(F("LM_SQR  "));
    else if (microsynth[microsynth_selected_instance].wave == 7)
      display.print(F("LM_PULSE"));
    else if (microsynth[microsynth_selected_instance].wave == 8)
      display.print(F("SMP&HOLD"));
  }
  if (menu_item_check(2))
  {
    setModeColor(2);
    setCursor_textGrid_small(9, 4);
    print_formatted_number_signed(microsynth[microsynth_selected_instance].coarse, 2, 2, 1);
  }
  if (menu_item_check(3))
  {
    setModeColor(3);
    setCursor_textGrid_small(9, 5);
    print_formatted_number_signed(microsynth[microsynth_selected_instance].detune, 2, 3, 1);
  }
  if (menu_item_check(4))
  {
    setModeColor(4);
    setCursor_textGrid_small(10, 7);
    print_formatted_number(microsynth[microsynth_selected_instance].env_attack * 4, 4, 4, 1);
  }
  if (menu_item_check(5))
  {
    setModeColor(5);
    setCursor_textGrid_small(10, 8);
    print_formatted_number(microsynth[microsynth_selected_instance].env_decay * 4, 4, 5, 1);
  }
  if (menu_item_check(6))
  {
    setModeColor(6);
    setCursor_textGrid_small(11, 9);
    print_formatted_number(microsynth[microsynth_selected_instance].env_sustain * 2, 3, 6, 1);
  }
  if (menu_item_check(7))
  {
    setModeColor(7);
    setCursor_textGrid_small(10, 10);
    print_formatted_number(microsynth[microsynth_selected_instance].env_release * microsynth[microsynth_selected_instance].env_release, 4, 7, 1);
  }
  if (seq.cycle_touch_element != 1)
    microsynth_refresh_lower_screen_dynamic_text();
  if (menu_item_check(16))
    print_small_intbar(32, 0, microsynth[microsynth_selected_instance].noise_vol, 16, 1, 0);
  if (menu_item_check(17))
    print_small_intbar(32, 1, microsynth[microsynth_selected_instance].noise_decay, 17, 1, 1);
  if (menu_item_check(18))
  {
    setModeColor(18);
    setCursor_textGrid_small(32, 2);
    if (microsynth[microsynth_selected_instance].trigger_noise_with_osc)
      display.print(F("OSC."));
    else
      display.print(F("C-8 "));
  }
  if (menu_item_check(19))
  {
    setModeColor(19);
    setCursor_textGrid_small(32, 3);
    if (microsynth[microsynth_selected_instance].filter_noise_mode == 0)
      display.print(F("OFF   "));
    else if (microsynth[microsynth_selected_instance].filter_noise_mode == 1)
      display.print(F("LP12dB"));
    else if (microsynth[microsynth_selected_instance].filter_noise_mode == 2)
      display.print(F("BP12dB"));
    else if (microsynth[microsynth_selected_instance].filter_noise_mode == 3)
      display.print(F("HI12dB"));
  }
  if (menu_item_check(20))
    print_small_intbar(32, 4, microsynth[microsynth_selected_instance].filter_noise_freq_from / 100, 20, 0, 1);
  if (menu_item_check(21))
    print_small_intbar(38, 4, microsynth[microsynth_selected_instance].filter_noise_freq_to / 100, 21, 0, 1);
  if (menu_item_check(22))
    print_small_intbar(27, 5, microsynth[microsynth_selected_instance].filter_noise_resonance, 22, 0, 1);
  if (menu_item_check(23))
    print_small_intbar(38, 5, microsynth[microsynth_selected_instance].filter_noise_speed / 10, 23, 0, 1);
  if (menu_item_check(24))
    print_small_intbar(32, 7, microsynth[microsynth_selected_instance].lfo_intensity, 24, 0, 1);
  if (menu_item_check(25))
  {
    setModeColor(25);
    setCursor_textGrid_small(29, 8);
    print_formatted_number(microsynth[microsynth_selected_instance].lfo_mode, 2, 25, 1);

    if (microsynth[microsynth_selected_instance].lfo_mode == 0)
    {
      display.setTextColor(COLOR_SYSTEXT, RED);
    }
    else
      display.setTextColor(GREY1, GREY3);

    setCursor_textGrid_small(32, 8);
    display.print(F("U&D"));
    if (microsynth[microsynth_selected_instance].lfo_mode == 1)
    {
      display.setTextColor(COLOR_SYSTEXT, RED);
    }
    else
      display.setTextColor(GREY1, GREY3);
    setCursor_textGrid_small(36, 8);
    display.print(F("1U"));

    if (microsynth[microsynth_selected_instance].lfo_mode == 2)
    {
      display.setTextColor(COLOR_SYSTEXT, RED);
    }
    else
      display.setTextColor(GREY1, GREY3);
    setCursor_textGrid_small(39, 8);
    display.print(F("1D"));
  }
  if (menu_item_check(26))
    print_small_intbar(28, 9, microsynth[microsynth_selected_instance].lfo_delay, 26, 0, 1);
  if (menu_item_check(27))
    print_small_intbar(38, 9, microsynth[microsynth_selected_instance].lfo_speed, 27, 0, 1);
}

FLASHMEM void UI_func_microsynth(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    registerTouchHandler(handle_touchscreen_microsynth);
    registerScope(269, 40, 51, 40);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    virtual_keyboard_smart_preselect_mode();
    if (seq.cycle_touch_element != 1)
    {
      TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[0]);
      microsynth_refresh_lower_screen_static_text();
    }
    else
    {
      TouchButton::drawButton(GRID.X[5], GRID.Y[0], "MORE", "PARAM.", TouchButton::BUTTON_ACTIVE);
      drawVirtualKeyboard();
    }
    print_static_texts_microsynth();
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
      generic_temp_select_menu = 0;

    generic_full_draw_required = true;
    update_selective_values_microsynth();
    generic_full_draw_required = false;
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 35);
      }
      else {
        microsynth_t& ms = microsynth[microsynth_selected_instance];

        if (generic_temp_select_menu == 0)
          ms.sound_intensity = constrain(ms.sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
        else if (generic_temp_select_menu == 1)
        {
          ms.wave = constrain(ms.wave + e.dir, 0, 8);
          update_pwm_text();
        }
        else if (generic_temp_select_menu == 2)
          ms.coarse = constrain(ms.coarse + e.dir, -36, 36);
        else if (generic_temp_select_menu == 3)
          ms.detune = constrain(ms.detune + e.dir * e.speed, -99, 99);
        else if (generic_temp_select_menu == 4)
          ms.env_attack = constrain(ms.env_attack + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 5)
          ms.env_decay = constrain(ms.env_decay + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 6)
          ms.env_sustain = constrain(ms.env_sustain + e.dir * e.speed, 0, 50);
        else if (generic_temp_select_menu == 7)
          ms.env_release = constrain(ms.env_release + e.dir * e.speed, 0, 99);
        else if (generic_temp_select_menu == 8)
          ms.filter_osc_mode = constrain(ms.filter_osc_mode + e.dir, 0, 3);
        else if (generic_temp_select_menu == 9)
          ms.filter_osc_freq_from = constrain(ms.filter_osc_freq_from + e.dir * e.speed * 40, 0, 15000);
        else if (generic_temp_select_menu == 10)
          ms.filter_osc_freq_to = constrain(ms.filter_osc_freq_to + e.dir * e.speed * 40, 0, 15000);
        else if (generic_temp_select_menu == 11)
          ms.filter_osc_resonance = constrain(ms.filter_osc_resonance + e.dir * e.speed, 0, 99);
        else if (generic_temp_select_menu == 12)
          ms.filter_osc_speed = constrain(ms.filter_osc_speed + e.dir * e.speed * 50, 0, 999);
        else if (generic_temp_select_menu == 13)
          ms.pwm_from = constrain(ms.pwm_from + e.dir * e.speed * 4, 0, 999);
        else if (generic_temp_select_menu == 14)
          ms.pwm_to = constrain(ms.pwm_to + e.dir * e.speed * 4, 0, 999);
        else if (generic_temp_select_menu == 15)
          ms.pwm_speed = constrain(ms.pwm_speed + e.dir * e.speed, 0, 99);
        else if (generic_temp_select_menu == 16)
          ms.noise_vol = constrain(ms.noise_vol + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 17)
          ms.noise_decay = constrain(ms.noise_decay + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 18)
          ms.trigger_noise_with_osc = !ms.trigger_noise_with_osc;
        else if (generic_temp_select_menu == 19)
          ms.filter_noise_mode = constrain(ms.filter_noise_mode + e.dir, 0, 3);
        else if (generic_temp_select_menu == 20)
          ms.filter_noise_freq_from = constrain(ms.filter_noise_freq_from + e.dir * e.speed * 40, 0, 15000);
        else if (generic_temp_select_menu == 21)
          ms.filter_noise_freq_to = constrain(ms.filter_noise_freq_to + e.dir * e.speed * 40, 0, 15000);
        else if (generic_temp_select_menu == 22)
          ms.filter_noise_resonance = constrain(ms.filter_noise_resonance + e.dir * e.speed, 0, 99);
        else if (generic_temp_select_menu == 23)
          ms.filter_noise_speed = constrain(ms.filter_noise_speed + e.dir * e.speed * 2, 0, 999);
        else if (generic_temp_select_menu == 24)
          ms.lfo_intensity = constrain(ms.lfo_intensity + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 25)
          ms.lfo_mode = constrain(ms.lfo_mode + e.dir, 0, 2);
        else if (generic_temp_select_menu == 26)
          ms.lfo_delay = constrain(ms.lfo_delay + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 27)
          ms.lfo_speed = constrain(ms.lfo_speed + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 28)
          ms.rev_send = constrain(ms.rev_send + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 29)
          ms.chorus_send = constrain(ms.chorus_send + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 30)
          ms.delay_send[0] = constrain(ms.delay_send[0] + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 31)
          ms.delay_send[1] = constrain(ms.delay_send[1] + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 32)
          ms.pan = constrain(ms.pan + e.dir, PANORAMA_MIN, PANORAMA_MAX);
        else if (generic_temp_select_menu == 33)
          ms.midi_channel = constrain(ms.midi_channel + e.dir, 0, 17);
        else if (generic_temp_select_menu == 34)
          ms.vel_mod_filter_osc = constrain(ms.vel_mod_filter_osc + e.dir * e.speed, 0, 254);
        else if (generic_temp_select_menu == 35)
          ms.vel_mod_filter_noise = constrain(ms.vel_mod_filter_noise + e.dir * e.speed, 0, 254);
      }
    }

    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
      generic_full_draw_required = true;
    }
    if (e.longPressed) {
      if (microsynth_selected_instance == 0) {
        microsynth_selected_instance = 1;
      }
      else {
        microsynth_selected_instance = 0;
      }
      update_microsynth_instance_icons();
      virtual_keyboard_smart_preselect_mode();
      generic_full_draw_required = true;
    }

    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    update_selective_values_microsynth();
    generic_full_draw_required = false;
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    generic_active_function = 99;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void tracker_print_pattern(int xpos, int ypos, uint8_t track_number)
{
  uint8_t yspacer = CHAR_height_small + 3;
  uint8_t ycount = 0;
  display.setTextSize(1);

  for (uint8_t y = 0; y < 16; y++)
  {
    // print data byte of current step
    if (track_number == seq.selected_track && y == seq.scrollpos && seq.current_chain[track_number] != 99) // print velocity of active pattern-step
    {
      if (seq.edit_state)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      display.setCursor(22 * CHAR_width_small, DISPLAY_HEIGHT - CHAR_height_small);
      print_formatted_number(seq.vel[seq.current_pattern[track_number]][y], 3);
      display.setCursor(34 * CHAR_width_small, DISPLAY_HEIGHT - CHAR_height_small);
      if (seq.vel[seq.current_pattern[track_number]][y] < 128)
        display.print("VELOCITY");
      else if (seq.vel[seq.current_pattern[track_number]][y] > 209)
        display.print("PITCHSMP");
      else if (seq.vel[seq.current_pattern[track_number]][y] > 199)
        display.print("CHORD   ");
      else
        display.print("OTHER   ");
    }

    /// -- update all other columns --

    display.setCursor(xpos, ypos + ycount * yspacer);
    if (seq.note_data[seq.current_pattern[track_number]][y] != 99 && seq.note_data[seq.current_pattern[track_number]][y] != 0 && seq.current_chain[track_number] != 99)
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        set_pattern_content_type_color(seq.current_pattern[track_number]);
      display.print(tracker_find_shortname_from_pattern_step(track_number, seq.current_pattern[track_number], y)[0]);
    }
    else
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos && seq.current_chain[track_number] != 99)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos && seq.current_chain[track_number] != 99)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(GREY3, COLOR_BACKGROUND);
      display.print("-");
    }
    display.setCursor(xpos + 2 * CHAR_width_small, ypos + ycount * yspacer);
    if (seq.note_data[seq.current_pattern[track_number]][y] != 99 && seq.note_data[seq.current_pattern[track_number]][y] != 0 && seq.note_data[seq.current_pattern[track_number]][y] != 130 && seq.current_chain[track_number] != 99)
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        set_pattern_content_type_color(seq.current_pattern[track_number]);
      print_formatted_number(seq.note_data[seq.current_pattern[track_number]][y], 3);
    }
    else if (seq.note_data[seq.current_pattern[track_number]][y] == 0 && seq.current_chain[track_number] != 99) // empty
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      print_formatted_number(seq.note_data[seq.current_pattern[track_number]][y], 3);
    }
    else if (seq.note_data[seq.current_pattern[track_number]][y] == 130 && seq.current_chain[track_number] != 99) // Latch
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos)
        display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
      else
        display.setTextColor(DX_DARKCYAN, COLOR_BACKGROUND);
      display.print("LAT");
    }
    else
    {
      if (seq.edit_state && track_number == seq.selected_track && y == seq.scrollpos && seq.current_chain[track_number] != 99)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else if (track_number == seq.selected_track && y == seq.scrollpos && seq.current_chain[track_number] != 99)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.print("---");
    }
    ycount++;
  }
}

FLASHMEM void UI_func_seq_tracker(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    seq.menu = 0;
    seq.edit_state = false;
    display.setTextSize(1);
    display.fillScreen(COLOR_BACKGROUND);
    UI_toplineInfoText(1);
    display.setCursor(1, 2);
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("TRACKER"));
    display.setCursor(CHAR_width_small * 10, 2);
    display.print(F("CHAIN"));
    display.setCursor(CHAR_width_small * 41, 2);
    display.print(F("SONG"));
    display.setCursor(CHAR_width_small * 49, 2);
    display.print(F("/"));
    display.setCursor(CHAR_width_small * 46, 2); // print song step at init
    display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
    print_formatted_number(seq.current_song_step, 2);
    display.setCursor(CHAR_width_small * 51, 2);
    print_formatted_number(get_song_length(), 2);
    display.setCursor(CHAR_width_small * 22, 2);

    for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++) // print chain steps
    {
      if (seq.current_chain[d] != 99)
      {
        display.setCursor(CHAR_width_small * 16 + (CHAR_width_small * 3) * d, 2);
        print_formatted_number(seq.chain_counter[d], 2);
      }
      //  display.setCursor(CHAR_width_small * 16+ (CHAR_width_small*3)*d , 2);
      //  print_formatted_number( get_chain_length_from_current_track(d)  , 2);
    }

    for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
    {
      seq.current_chain[x] = seq.song[x][seq.current_song_step];
      seq.current_pattern[x] = seq.chain[seq.current_chain[x]][seq.chain_counter[x]];

      setCursor_textGrid_small(6 + 6 * x, 2);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(F("TRK:"));
      display.print(x + 1);
      setCursor_textGrid_small(6 + 6 * x, 3);
      display.print("PAT");
      if (seq.current_chain[x] != 99)
        update_pattern_number_in_tracker(x);
    }
    display.setTextColor(DARKGREEN, COLOR_BACKGROUND);
    for (uint8_t y = 0; y < 16; y++)
    {
      display.setCursor(0, 45 + y * (CHAR_height_small + 3));
      print_formatted_number(y, 2);
    }
    display.setCursor(CHAR_width_small * 11, DISPLAY_HEIGHT - CHAR_height_small);
    display.print("DATA BYTE: ");
    display.setCursor(CHAR_width_small * 26, DISPLAY_HEIGHT - CHAR_height_small);
    display.print("USED AS");

    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.setCursor(5 * CHAR_width_small, (5 + seq.step) * (CHAR_height_small + 3) - 7);
    display.print(F(">"));
    encoderDir[ENC_R].reset();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == false) {// NOT in editor mode
        seq.scrollpos = constrain(seq.scrollpos + e.dir, 0, 15);
      }
      else // IS in editor mode
      {
        seq.note_data[seq.current_pattern[seq.selected_track]][seq.scrollpos] = constrain(seq.note_data[seq.current_pattern[seq.selected_track]][seq.scrollpos] + e.dir * e.speed, 0, 254);
      }
    }
    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
    }
    for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
    {
      seq.current_chain[x] = seq.song[x][seq.current_song_step];
      seq.current_pattern[x] = seq.chain[seq.current_chain[x]][seq.chain_counter[x]];
      // update_pattern_number_in_tracker(x);
    }

    display.setTextSize(1);
    setCursor_textGrid_small(1, 2);
    if (seq.edit_state)
    {
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print("EDIT");
      helptext_l(F("< > DATA"));
      helptext_r(F("< > NOTE"));
    }
    else
    {
      display.setTextColor(GREEN, COLOR_BACKGROUND);
      display.print("PLAY");
      helptext_l(F("< > MOVE X"));
      helptext_r(F("< > MOVE Y"));
    }
    for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
    {
      tracker_print_pattern((6 + 6 * d) * CHAR_width_small, 48, d);
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void sub_song_print_tracknumbers()
{
  char str[3];

  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    display.setCursor(6 * CHAR_width_small + (4 * CHAR_width_small) * x, CHAR_height_small * 4);
    if (seq.tracktype_or_instrument_assign == 0)
      set_track_type_color(x);
    else
      display.setTextColor(GREY3, COLOR_BACKGROUND);

    sprintf(str, "T%d", x + 1);
    display.print(str);
  }
}

FLASHMEM void print_song_loop_text()
{
  // print loop
  if (seq.tracktype_or_instrument_assign < 8) // not in loop clear
  {
    display.setCursor(CHAR_width_small * 11, 10);
    display.setTextColor(COLOR_BACKGROUND, DX_DARKCYAN);
    if (seq.loop_start != 99)
      print_formatted_number(seq.loop_start + 1, 2);
    else
      display.print(F("--"));

    display.setCursor(CHAR_width_small * 13, 10);
    display.print(F(" - "));

    display.setCursor(CHAR_width_small * 16, 10);
    if (seq.loop_end != 99)
      print_formatted_number(seq.loop_end + 1, 2);
    else
      display.print(F("--"));
  }
  else if (seq.tracktype_or_instrument_assign == 8) //clear loop mode
  {
    display.setCursor(CHAR_width_small * 11, 10);
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("CLEAR ?"));
    print_song_loop_arrows();
    print_chain_matrix_in_song_page();
  }
  else if (seq.tracktype_or_instrument_assign == 10) //copy loop mode
  {
    display.setCursor(CHAR_width_small * 11, 10);
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("COPY  ?"));
    print_song_loop_arrows();
    print_chain_matrix_in_song_page();
  }
  else
  {
    display.setCursor(CHAR_width_small * 11, 10);
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("???????")); //error
  }

}

FLASHMEM void print_song_length()
{
  // print song length
  display.setCursor(CHAR_width_small * 21, 10);
  display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
  print_formatted_number(get_song_length(), 2);
}

FLASHMEM void print_chain_header()
{
  if (seq.cycle_touch_element < 6)
  {
    display.setTextColor(GREY2, COLOR_BACKGROUND);
  }
  else
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
  display.setCursor(40 * CHAR_width_small, CHAR_height_small * 4);
  display.print(F("CHAIN:"));
  display.setCursor(50 * CHAR_width_small, CHAR_height_small * 4);
  display.print(F("L"));
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  display.setCursor(40 * CHAR_width_small, CHAR_height_small * 6);
  display.print(F("ST"));
  display.setCursor(43 * CHAR_width_small, CHAR_height_small * 6);
  if (seq.cycle_touch_element == 6 || seq.cycle_touch_element == 7)
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("PAT"));
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  if (seq.cycle_touch_element == 8 || seq.cycle_touch_element == 9)
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(48 * CHAR_width_small, CHAR_height_small * 6);
  display.print(F("TRANS"));
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  for (uint8_t y = 0; y < 16; y++) // chain
  {
    display.setCursor(CHAR_width_small * 40, CHAR_height_small * 8 + y * 10);
    print_formatted_number(y + 1, 2);
    display.setCursor(CHAR_width_small * 43, CHAR_height_small * 8 + y * 10);
    display.print("P");
    display.setCursor(CHAR_width_small * 48, CHAR_height_small * 8 + y * 10);
    display.print("T");
  }
}

FLASHMEM void adjust_scroll_pos_for_loop_visible_on_screen()
{
  if (seq.loop_start > 7) // loop start is below half of screen
  {
    if (seq.loop_start > seq.scrollpos)
      seq.scrollpos = seq.loop_start;
  }
}

FLASHMEM void UI_func_song(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
#if defined APC
    APC_MODE = APC_SONG;
    apc_clear_right_buttons();
#endif

    encoderDir[ENC_R].reset();
    song_page_full_draw_pattern_complete = false;
    seq.help_text_needs_refresh = true;
    seq.loop_edit_step = 0;
    seq.cycle_touch_element = 0;
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextSize(1);
    if (seq.quicknav_pattern_to_song_jump == false)
    {
      seq.edit_state = false;
      seq.song_menu = 0;
      temp_int_song_page = 0;
      seq.cursor_scroll = 0;
      seq.scrollpos = 0;
    }
    else
    {
      print_chain_matrix_in_song_page();
      seq.quicknav_pattern_to_song_jump = false;
      seq.cycle_touch_element = 8;
    }

    UI_toplineInfoText(2);
    display.setCursor(1, 1);
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("SONG"));
    display.setCursor(CHAR_width_small * 11, 1);
    display.print(F("LOOP"));
    display.setCursor(CHAR_width_small * 21, 1);
    display.print(F("SLEN"));
    display.setCursor(CHAR_width_small * 26, 1);
    display.print(F("LC"));
    print_chain_header();

    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    show_no_grid(10, 1, 10, seq.name);
    // print loop text
    print_song_loop_text();
    print_song_loop_arrows();
    // print song length
    print_song_length();
    // print currently playing chain steps
    print_playing_chains();
    sub_song_print_tracknumbers();
    sub_song_print_tracktypes();
    sub_song_print_instruments(GREY2, COLOR_BACKGROUND);

    display.setTextSize(1);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);

    if (e.down) {
      if (seq.edit_state == false)
      {
        if (seq.cursor_scroll == 15)
        {
          seq.scrollpos++;
          if (seq.scrollpos > SONG_LENGTH - 16)
            seq.scrollpos = SONG_LENGTH - 16;
          print_song_loop_arrows();
        }

        else if (seq.tracktype_or_instrument_assign == 10) // go back from loop copy to tracktype
        {
          seq.tracktype_or_instrument_assign = 8;
          print_song_loop_text();
          seq.help_text_needs_refresh = true;
        }

        else if (seq.tracktype_or_instrument_assign == 8 && seq.loop_edit_step != 3) // go back from loop clear to tracktype
        {
          seq.tracktype_or_instrument_assign = 5;
          print_song_loop_text();
          seq.help_text_needs_refresh = true;
        }
        else if (seq.tracktype_or_instrument_assign == 8 && seq.loop_edit_step == 3) // exit clear song loop
        {
          seq.tracktype_or_instrument_assign = 0;
          seq.loop_edit_step = 0;
          seq.selected_track = 0;
          print_song_loop_text();
          print_song_loop_arrows();
          seq.help_text_needs_refresh = true;
        }
        else if (seq.tracktype_or_instrument_assign == 1 && seq.loop_edit_step == 0) // disable edit instruments for tracks
        {
          seq.tracktype_or_instrument_assign = 0;
          sub_song_print_tracknumbers();
          sub_song_print_tracktypes();
          sub_song_print_instruments(GREY2, COLOR_BACKGROUND);
          seq.help_text_needs_refresh = true;
        }
        else if (seq.tracktype_or_instrument_assign == 2 && seq.loop_edit_step == 0) // select instruments for track
        {  // 26/01/2025 new dexed instance will start at 70
          if (seq.instrument[seq.selected_track] < 64 && seq.instrument[seq.selected_track] != 1)
            seq.instrument[seq.selected_track]++;
          else  if (seq.instrument[seq.selected_track] == 1) //second dexed instance
            seq.instrument[seq.selected_track] = 70;
          else  if (seq.instrument[seq.selected_track] == 70) //third dexed instance
            seq.instrument[seq.selected_track] = 71;
          else  if (seq.instrument[seq.selected_track] == 71)
            seq.instrument[seq.selected_track] = 72; // granular synth
          else  if (seq.instrument[seq.selected_track] == 72)
            seq.instrument[seq.selected_track] = 2;

          if (seq.instrument[seq.selected_track] > 5 && seq.instrument[seq.selected_track] < 16) // skip currently unused msp slots
          {
            if (seq.instrument[seq.selected_track] > 5 + NUM_MULTISAMPLES)
              seq.instrument[seq.selected_track] = 16;
          }
        }
        else if (seq.tracktype_or_instrument_assign == 6 && seq.loop_edit_step == 0) // tracktype change
        {
          if (seq.track_type[seq.selected_track] < 4)
            seq.track_type[seq.selected_track]++;
        }
        else if (seq.tracktype_or_instrument_assign == 5 && seq.loop_edit_step == 0) // disable edit tracktype select
        {
          seq.help_text_needs_refresh = true;
          seq.tracktype_or_instrument_assign = 1;
          sub_song_print_instruments(GREY2, COLOR_BACKGROUND);
          sub_song_print_tracktypes();
        }
        else if (seq.tracktype_or_instrument_assign == 0 || seq.tracktype_or_instrument_assign == 11)
        {
          seq.cursor_scroll_previous = seq.cursor_scroll;
          seq.previous_track = seq.selected_track;
          seq.cursor_scroll = constrain(seq.cursor_scroll + 1, 0, 15);
        }
      }
      else if (seq.edit_state == true && seq.cycle_touch_element == 5)
      {
        if (temp_int_song_page == NUM_CHAINS)
          temp_int_song_page = 0;
        else
          temp_int_song_page = constrain(temp_int_song_page + 1, 0, NUM_CHAINS); // not -1: last element is for empty step
      }
      else if ((seq.edit_state == true && seq.cycle_touch_element == 6) || (seq.edit_state == true && seq.cycle_touch_element == 8))
      {
        // if ( seq.loop_edit_step == 0)
        seq.song_menu = constrain(seq.song_menu + 1, 0, 15);
      }
      else if (seq.edit_state == true && seq.cycle_touch_element == 7)
      {
        if (seq.sub_menu == 99)
          seq.sub_menu = 0;
        else
          seq.sub_menu = constrain(seq.sub_menu + 1, 0, NUM_CHAINS);
        song_page_full_draw_chain_complete = false;

      }
      else if (seq.edit_state == true && seq.cycle_touch_element == 9)
      {
        if (seq.sub_menu > NUM_CHAINS)
          seq.sub_menu = seq.sub_menu - 1;
        else if (seq.sub_menu >= 0 && seq.sub_menu < NUM_CHAINS - 1)
          seq.sub_menu = seq.sub_menu + 1;
        if (seq.sub_menu == NUM_CHAINS)
          seq.sub_menu = 0;

      }
    }
    else if (e.up) {
      if (seq.edit_state == false)
      {
        if ((seq.cursor_scroll == 0 && seq.scrollpos > 0 && seq.tracktype_or_instrument_assign == 0)
          || (seq.cursor_scroll == 0 && seq.scrollpos > 0 && seq.tracktype_or_instrument_assign == 11))
        {
          seq.scrollpos--;
          print_song_loop_arrows();
        }

        else if (seq.cursor_scroll == 0 && seq.scrollpos == 0 && seq.tracktype_or_instrument_assign == 0 && seq.loop_edit_step == 0) // edit instruments for tracks
        {
          seq.tracktype_or_instrument_assign = 1;
          seq.help_text_needs_refresh = true;
          sub_song_print_tracknumbers();
          sub_song_print_tracktypes();
          sub_song_print_instruments(GREY1, COLOR_BACKGROUND);
        }

        else if (seq.cursor_scroll == 0 && seq.scrollpos == 0 && seq.tracktype_or_instrument_assign == 0 && seq.loop_edit_step != 0) // go to clear loop 
        {
          if (seq.loop_start != 99 || seq.loop_end != 99) //do not go in if no loop is set
          {
            seq.tracktype_or_instrument_assign = 8;
            seq.loop_edit_step = 3;
            print_song_loop_arrows();
            sub_song_print_tracktypes();
            print_song_loop_text();
            seq.help_text_needs_refresh = true;
            clear_song_playhead();
          }
        }
        else if (seq.tracktype_or_instrument_assign == 2 && seq.loop_edit_step == 0) // select instruments for track
        {//26/01/2025 new dexed instance will start at 70  //27/09/2025 added granular synth at 72
          if (seq.instrument[seq.selected_track] > 0 && seq.instrument[seq.selected_track] < 65 && seq.instrument[seq.selected_track] != 2)
          {
            seq.instrument[seq.selected_track]--;
            if (seq.instrument[seq.selected_track] == 5)
              sub_song_print_tracktypes();

          }
          else if (seq.instrument[seq.selected_track] == 2)
            seq.instrument[seq.selected_track] = 72; //granular
          else if (seq.instrument[seq.selected_track] == 70) // third dexed instance
            seq.instrument[seq.selected_track] = 1;
          else if (seq.instrument[seq.selected_track] == 71) // fourth dexed instance
            seq.instrument[seq.selected_track] = 70;
          else if (seq.instrument[seq.selected_track] == 72) // granular synth
            seq.instrument[seq.selected_track] = 71;

          if (seq.instrument[seq.selected_track] > 5 && seq.instrument[seq.selected_track] < 16) // skip currently unused msp slots
          {
            if (seq.instrument[seq.selected_track] > 5 + NUM_MULTISAMPLES)
              seq.instrument[seq.selected_track] = 5 + NUM_MULTISAMPLES;
          }
        }
        else if (seq.tracktype_or_instrument_assign == 1 && seq.loop_edit_step == 0) // goto for tracktype change
        {
          seq.tracktype_or_instrument_assign = 5;
          seq.help_text_needs_refresh = true;
          sub_song_print_instruments(GREY3, COLOR_BACKGROUND);
        }

        else if (seq.tracktype_or_instrument_assign == 6) // tracktype change
        {
          if (seq.track_type[seq.selected_track] > 0)
            seq.track_type[seq.selected_track]--;
        }

        else if (seq.tracktype_or_instrument_assign == 5) // goto loop clear
        {
          if (seq.loop_start != 99 || seq.loop_end != 99) //do not go if no loop is set
          {
            seq.tracktype_or_instrument_assign = 8;
            seq.loop_edit_step = 3;
            sub_song_print_tracktypes();
            adjust_scroll_pos_for_loop_visible_on_screen();
            print_song_loop_text();
            seq.help_text_needs_refresh = true;
            clear_song_playhead();
          }
        }

        else if (seq.tracktype_or_instrument_assign == 8) // goto loop copy mode
        {
          if (seq.loop_start != 99 || seq.loop_end != 99) //do not go if no loop is set
          {

            seq.tracktype_or_instrument_assign = 10;

            //scroll display if selected loop is out of screen bounds

            adjust_scroll_pos_for_loop_visible_on_screen();

            print_song_loop_text();
            seq.help_text_needs_refresh = true;
            clear_song_playhead();
          }
        }

        if (seq.tracktype_or_instrument_assign == 0 || seq.tracktype_or_instrument_assign == 11)
        {
          seq.cursor_scroll_previous = seq.cursor_scroll;
          seq.previous_track = seq.selected_track;
          seq.cursor_scroll = constrain(seq.cursor_scroll - 1, 0, 15);
        }
      }

      else if (seq.edit_state == true && seq.cycle_touch_element == 5)
      {
        if (temp_int_song_page == 0)
          temp_int_song_page = NUM_CHAINS;
        else
          temp_int_song_page = constrain(temp_int_song_page - 1, 0, NUM_CHAINS); // not -1:last element is for empty step
      }
      else if ((seq.edit_state == true && seq.cycle_touch_element == 6) || (seq.edit_state == true && seq.cycle_touch_element == 8))
      {
        seq.song_menu = constrain(seq.song_menu - 1, 0, 15);

      }
      else if ((seq.edit_state == true && seq.cycle_touch_element == 7))
      {
        if (seq.sub_menu == 0)
          seq.sub_menu = 99;
        else if (seq.sub_menu == 99)
          ;
        else
          seq.sub_menu = constrain(seq.sub_menu - 1, 0, NUM_CHAINS);
        song_page_full_draw_chain_complete = false;
      }

      else if (seq.edit_state == true && seq.cycle_touch_element == 9)
      {
        if (seq.sub_menu > 0 && seq.sub_menu < NUM_CHAINS)
          seq.sub_menu--;
        else if (seq.sub_menu > NUM_CHAINS)
          seq.sub_menu = seq.sub_menu + 1;
        else if (seq.sub_menu == 0)
          seq.sub_menu = NUM_CHAINS + 1;
        if (seq.sub_menu > NUM_CHAINS * 2 - 1)
          seq.sub_menu = NUM_CHAINS * 2 - 1;

      }
    }
    else if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    {
      if (seq.tracktype_or_instrument_assign == 5) // tracktype change
      {
        seq.tracktype_or_instrument_assign = 6;
        seq.help_text_needs_refresh = true;
      }
      else if (seq.tracktype_or_instrument_assign == 6) // exit track assign menu
      {
        seq.tracktype_or_instrument_assign = 5;
        seq.help_text_needs_refresh = true;
      }
      else if (seq.tracktype_or_instrument_assign == 1) // go into instr. assign menu
      {
        if (seq.track_type[seq.selected_track] != 0) // not set to a drum track
        {
          seq.tracktype_or_instrument_assign = 2;
          seq.help_text_needs_refresh = true;
        }
        else
        {
          helptext_l(back_text);
          helptext_r(F("TRACKTYPE [DRUMS] NOT VALID FOR INSTR"));
        }
      }
      else if (seq.tracktype_or_instrument_assign == 2) // exit instr. assign menu
      {
        seq.tracktype_or_instrument_assign = 1;
        sub_song_print_tracktypes();
        seq.help_text_needs_refresh = true;
      }

      else if (seq.tracktype_or_instrument_assign == 8) // clear song loop
      {
        seq.tracktype_or_instrument_assign = 0;
        seq.loop_start = 99;
        seq.loop_end = 99;
        seq.scrollpos = 0;
        seq.loop_edit_step = 0;
        seq.selected_track = 0;
        print_song_loop_arrows();
        print_song_loop_text();
        clear_song_playhead();
        seq.help_text_needs_refresh = true;
      }

      else if (seq.tracktype_or_instrument_assign == 10) // copy mode - not enabled
      {
        seq.tracktype_or_instrument_assign = 11;
        if (seq.scrollpos == 0)
          seq.cursor_scroll = seq.loop_end + 1;

        if (seq.loop_end > 15) {
          seq.cursor_scroll = seq.loop_end - seq.loop_start + 1;
          seq.scrollpos = seq.loop_start;
        }

        seq.help_text_needs_refresh = true;
      }
      else if (seq.tracktype_or_instrument_assign == 11) // confirmed copy mode 
      {
        ////copy loop to destination
        for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
        {
          for (uint8_t y = 0; y < seq.loop_end - seq.loop_start + 1; y++)
          {
            if (seq.cursor_scroll + seq.scrollpos + y < SONG_LENGTH - 1) // do not copy out of song bounds
              seq.song[x][seq.cursor_scroll + seq.scrollpos + y] = seq.song[x][seq.loop_start + y];
          }
        }

        seq.tracktype_or_instrument_assign = 0;
        seq.scrollpos = 0;
        seq.selected_track = 0;
        seq.loop_edit_step = 0;
        print_song_loop_arrows();
        print_song_loop_text();
        clear_song_playhead();
        seq.cursor_scroll = 0;
        seq.help_text_needs_refresh = true;
      }

      else  if (seq.tracktype_or_instrument_assign == 0)
      {
        seq.help_text_needs_refresh = true;
        if (seq.loop_edit_step == 1) // edit loop step 1, set start
        {
          seq.loop_edit_step = 2;
          seq.loop_start = seq.cursor_scroll + seq.scrollpos;
        }
        else if (seq.loop_edit_step == 2) // edit loop step, set end
        {
          seq.loop_edit_step = 0;
          seq.selected_track = 0;
          seq.loop_end = seq.cursor_scroll + seq.scrollpos;
          print_song_loop_arrows();
          clear_song_playhead();
        }
        else if (seq.cycle_touch_element == 5 && seq.edit_state)
        {
          seq.edit_state = !seq.edit_state;
          seq.cycle_touch_element = 0;
        }
        else if (seq.cycle_touch_element == 0)
        {
          seq.edit_state = !seq.edit_state;
          seq.cycle_touch_element = 5;
          temp_int_song_page = seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos];
        }
        else if (seq.edit_state && seq.cycle_touch_element == 6)
        {
          seq.cycle_touch_element = 7; // edit chain
          seq.help_text_needs_refresh = true;
          seq.sub_menu = seq.chain[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu];
        }
        else if (seq.edit_state && seq.cycle_touch_element == 7)
        {
          seq.cycle_touch_element = 6; // go back from chain
        }
        else if (seq.edit_state && seq.cycle_touch_element == 8)
        {
          seq.cycle_touch_element = 9; // edit transpose
          if (seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] != 99)
            seq.sub_menu = seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu];
          else
            seq.sub_menu = 0;
          song_page_full_draw_transpose_complete = false;
        }
        else if (seq.edit_state && seq.cycle_touch_element == 9)
        {
          if (seq.sub_menu == 0)
            seq.chain_transpose[seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]][seq.song_menu] = 99;
          seq.cycle_touch_element = 8; // go back from transpose 
        }
        else if (seq.edit_state == false)
        {
          seq.cycle_touch_element = 0;
        }

        if (seq.cycle_touch_element < 7)
        {
          print_song_loop_text();
          print_song_length();
        }
        seq.help_text_needs_refresh = true;
      }
    } // Button END


    if (seq.help_text_needs_refresh)
    {
      print_chain_header();
      print_song_mode_help();
      seq.help_text_needs_refresh = false;
    }

    if (seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos] != song_previous_displayed_chain)
    {
      //current displayed chain has changed
#ifdef DEBUG
      // LOG.println(F(" "));
      // LOG.println(F("chain changed from "));
      // LOG.print(song_previous_displayed_chain);
      // LOG.print(F(" to "));
      // LOG.print(seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos]);
#endif

      song_page_full_draw_chain_complete = false;
      song_page_full_draw_transpose_complete = false;
      song_previous_displayed_chain = seq.song[seq.selected_track][seq.cursor_scroll + seq.scrollpos];
      chain_endline = 99;
    }

    if (seq.loop_edit_step == 1 || seq.loop_edit_step == 2)
      edit_song_loop();

    print_song_playhead();
    print_chain_matrix_in_song_page();
    print_chain_steps_in_song_page();
    print_transpose_in_song_page();
    empty_chain_warning_text_in_song_page();
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    seq.cycle_touch_element = 0;
    seq.tracktype_or_instrument_assign = 0;
    song_page_full_draw_pattern_complete = false;
    song_page_full_draw_chain_complete = false;
    song_page_full_draw_transpose_complete = false;

    chain_endline = 99;
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);

    // #if defined APC
    //     apc_clear_grid();
    // #endif
  }
}

FLASHMEM void arp_refresh_display_play_status()
{
  if (seq.running == false)
  {
    // play symbol
    drawBitmap(4 * CHAR_width - 4, CHAR_height * 11 + 3, special_chars[19], 8, 8, GREEN);
  }
  else if (seq.running == true)
  {
    // stop symbol
    drawBitmap(4 * CHAR_width - 4, CHAR_height * 11 + 3, special_chars[21], 8, 8, COLOR_SYSTEXT);
  }
}

FLASHMEM void print_arp_start_stop_button()
{
  if (seq.running)
    draw_button_on_grid(2, 23, "SEQ.", "STOP", 1);
  else
    draw_button_on_grid(2, 23, "SEQ.", "START", 0);
  arp_refresh_display_play_status();
}

FLASHMEM void draw_euclidean_circle()
{
  uint8_t r = 61;
  int a = 300;
  int b = 210;
  for (int i = 0; i < 16; i++)
  {
    double t = 2 * PI * i / 16;
    int x = (int)a + r * cos(t);
    int y = (int)b + r * sin(t);
    if (i > 3)
    {
      if (seq.euclidean_state[i - 4])
        display.fillCircle(x - r, y - r, 10, RED);
      else
        display.fillCircle(x - r, y - r, 10, GREY3);
    }
    else
    {
      if (seq.euclidean_state[i + 12])
        display.fillCircle(x - r, y - r, 10, RED);
      else
        display.fillCircle(x - r, y - r, 10, GREY3);
    }
  }
  r = 41;
  a = 300 - 20;
  b = 210 - 20;
  for (int i = 0; i < 16; i++)
  {
    double t = 2 * PI * i / 16;
    int x = (int)a + r * cos(t);
    int y = (int)b + r * sin(t);
    if (seq.step > 4)
    {
      if (i == seq.step - 5)
        display.fillCircle(x - r, y - r, 6, RED);
      else
        display.fillCircle(x - r, y - r, 6, GREY3);
    }
    else
    {
      if (i == seq.step + 11)
        display.fillCircle(x - r, y - r, 6, RED);
      else
        display.fillCircle(x - r, y - r, 6, GREY3);
    }
  }
}

//----------------------------- Euclid calculation functions ---------------------------------------//
//--- the three functions below are taken directly from http://clsound.com/euclideansequenc.html ---//
//--- acknowledgment to Craig Lee ------------------------------------------------------------------//

//------------Function to right rotate n by d bits---------------------------------//
uint16_t rightRotate(int shift, uint16_t value, uint8_t pattern_length)
{
  uint16_t mask = ((1 << pattern_length) - 1);
  value &= mask;
  return ((value >> shift) | (value << (pattern_length - shift))) & mask;
}

//----1---------Function to find the binary length of a number by counting bitwise-------//
int findlength(unsigned int bnry)
{
  boolean lengthfound = false;
  int length = 1; // no number can have a length of zero - single 0 has a length of one, but no 1s for the sytem to count
  for (int q = 32; q >= 0; q--)
  {
    int r = bitRead(bnry, q);
    if (r == 1 && lengthfound == false)
    {
      length = q + 1;
      lengthfound = true;
    }
  }
  return length;
}

//-----2--------Function to concatenate two binary numbers bitwise----------------------//
FLASHMEM unsigned int ConcatBin(unsigned int bina, unsigned int binb)
{
  int binb_len = findlength(binb);
  unsigned int sum = (bina << binb_len);
  sum = sum | binb;
  return sum;
}

//------3-------------------Euclidean bit sorting funciton-------------------------------//
FLASHMEM unsigned int euclid(int n, int k, int o)
{ // inputs: n=total, k=beats, o = offset
  int pauses = n - k;
  int pulses = k;
  int offset = o;
  int steps = n;
  int per_pulse = pauses / k;
  int remainder = pauses % pulses;
  unsigned int workbeat[n];
  unsigned int outbeat;
  uint16_t outbeat2;
  int workbeat_count = n;
  int a;
  int b;
  int trim_count;

  for (a = 0; a < n; a++)
  { // Populate workbeat with unsorted pulses and pauses
    if (a < pulses)
    {
      workbeat[a] = 1;
    }
    else
    {
      workbeat[a] = 0;
    }
  }

  if (per_pulse > 0 && remainder < 2)
  { // Handle easy cases where there is no or only one remainer
    for (a = 0; a < pulses; a++)
    {
      for (b = workbeat_count - 1; b > workbeat_count - per_pulse - 1; b--)
      {
        workbeat[a] = ConcatBin(workbeat[a], workbeat[b]);
      }
      workbeat_count = workbeat_count - per_pulse;
    }

    outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count
    for (a = 0; a < workbeat_count; a++)
    {
      outbeat = ConcatBin(outbeat, workbeat[a]);
    }

    if (offset > 0)
    {
      outbeat2 = rightRotate(offset, outbeat, steps); // Add offset to the step pattern
    }
    else
    {
      outbeat2 = outbeat;
    }

    return outbeat2;
  }

  else
  {
    if (pulses == 0)
    {
      pulses = 1; // Prevent crashes when k=0 and n goes from 0 to 1
    }
    int groupa = pulses;
    int groupb = pauses;
    int iteration = 0;
    if (groupb <= 1)
    {
    }

    while (groupb > 1)
    { // main recursive loop

      if (groupa > groupb)
      {                                    // more Group A than Group B
        int a_remainder = groupa - groupb; // what will be left of groupa once groupB is interleaved
        trim_count = 0;
        for (a = 0; a < groupa - a_remainder; a++)
        { // count through the matching sets of A, ignoring remaindered
          workbeat[a] = ConcatBin(workbeat[a], workbeat[workbeat_count - 1 - a]);
          trim_count++;
        }
        workbeat_count = workbeat_count - trim_count;

        groupa = groupb;
        groupb = a_remainder;
      }

      else if (groupb > groupa)
      {                                    // More Group B than Group A
        int b_remainder = groupb - groupa; // what will be left of group once group A is interleaved
        trim_count = 0;
        for (a = workbeat_count - 1; a >= groupa + b_remainder; a--)
        { // count from right back through the Bs
          workbeat[workbeat_count - a - 1] = ConcatBin(workbeat[workbeat_count - a - 1], workbeat[a]);

          trim_count++;
        }
        workbeat_count = workbeat_count - trim_count;
        groupb = b_remainder;
      }

      else if (groupa == groupb)
      { // groupa = groupb
        trim_count = 0;
        for (a = 0; a < groupa; a++)
        {
          workbeat[a] = ConcatBin(workbeat[a], workbeat[workbeat_count - 1 - a]);
          trim_count++;
        }
        workbeat_count = workbeat_count - trim_count;
        groupb = 0;
      }

      else
      {
        // LOG.println(F("ERROR"));
      }
      iteration++;
    }

    outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count
    for (a = 0; a < workbeat_count; a++)
    {
      outbeat = ConcatBin(outbeat, workbeat[a]);
    }

    if (offset > 0)
    {
      outbeat2 = rightRotate(offset, outbeat, steps); // Add offset to the step pattern
    }
    else
    {
      outbeat2 = outbeat;
    }

    return outbeat2;
  }
}
//------------------end euclidian math-------------------------------------//

FLASHMEM void update_euclidean()
{
  for (uint8_t i = 0; i < 16; i++)
  {
    if (bitRead(euclid(16, seq.arp_length, 16 - seq.euclidean_offset), i))
      seq.euclidean_state[i] = true;
    else
      seq.euclidean_state[i] = false;
  }
}

FLASHMEM void show_euclidean()
{
  update_euclidean();
  draw_euclidean_circle();
}

FLASHMEM void UI_func_arpeggio(uint8_t param)
{
  char displayname[8] = { 0, 0, 0, 0, 0, 0, 0 };
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_arpeggio);
    registerScope(180, 0, 128, 50);
    encoderDir[ENC_R].reset();
    generic_temp_select_menu = 0;
    seq.temp_active_menu = 0;
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextSize(1);
    seq.edit_state = false;
    setCursor_textGrid_large(1, 1);
    display.setTextColor(RED);
    display.print(F("ARPEGGIO SETTINGS"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 4);
    display.print(F("LENGTH"));
    setCursor_textGrid_small(15, 4);
    display.print(F("STEPS"));
    setCursor_textGrid_small(2, 5);
    display.print(F("STYLE"));
    setCursor_textGrid_small(2, 6);
    display.print(F("SPEED"));
    setCursor_textGrid_small(17, 6);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("32/64 MICROSYNTH ONLY [SID STYLE]"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 7);
    display.print(F("OFFSET"));
    setCursor_textGrid_small(2, 8);
    display.print(F("MODE"));
    setCursor_textGrid_small(2, 9);
    display.print(F("MX NOTES"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);

    setCursor_textGrid_small(2, 11);
    display.print(F("FROM ADV. SETTINGS:"));

    setCursor_textGrid_small(2, 13);
    display.print(F("VELOCITY"));

    setCursor_textGrid_small(17, 13);
    print_formatted_number(seq.chord_vel, 3);

    setCursor_textGrid_small(2, 14);
    display.print(F("OCTAVE SHIFT"));

    setCursor_textGrid_small(17, 14);
    snprintf_P(displayname, sizeof(displayname), PSTR("%02d"), seq.oct_shift);
    display.print(displayname);

    setCursor_textGrid_small(2, 15);
    display.print(F("NOTE SHIFT"));

    setCursor_textGrid_small(17, 15);
    display.print(seq.element_shift);

    setCursor_textGrid_small(2, 17);
    display.print(F("PLAYING:"));
    setCursor_textGrid_small(11, 17);
    display.print(F("["));
    setCursor_textGrid_small(19, 17);
    display.print(F("]"));
    print_arp_start_stop_button();

    helptext_l(back_text);
    display.setTextSize(2);
    show_euclidean();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == false)
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 5);
      else if (generic_temp_select_menu == 0)
        seq.arp_length = constrain(seq.arp_length + e.dir * e.speed, 0, 16);
      else if (generic_temp_select_menu == 1)
        seq.arp_style = constrain(seq.arp_style + e.dir * e.speed, 0, 3);
      else if (generic_temp_select_menu == 2)
        seq.arp_speed = constrain(seq.arp_speed + e.dir * e.speed, 0, 3);
      else if (generic_temp_select_menu == 3)
        seq.euclidean_offset = constrain(seq.euclidean_offset + e.dir * e.speed, 0, 15);
      else if (generic_temp_select_menu == 4)
        seq.euclidean_active = !seq.euclidean_active;
      else if (generic_temp_select_menu == 5)
        seq.arp_num_notes_max = constrain(seq.arp_num_notes_max + e.dir, 1, 64);
    }
    if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    {
      seq.edit_state = !seq.edit_state;
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    show_euclidean();

    if (seq.edit_state == false)
      helptext_r(F("< > SELECT OPTION TO EDIT"));
    else
      helptext_r(F("< > EDIT VALUE"));
    display.setTextSize(1);

    setModeColor(0);
    setCursor_textGrid_small(11, 4);
    if (seq.arp_length == 0)
      display.print("ALL");
    else
      print_formatted_number(seq.arp_length, 3); // play all elements or from 1-xx elements

    setModeColor(1);
    setCursor_textGrid_small(11, 5);
    for (uint8_t i = 0; i < 4; i++)
    {
      if (i == seq.arp_style && generic_temp_select_menu == 1)
        setModeColor(1);
      else if (i == seq.arp_style)
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      else
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.print(seq.arp_style_names[i][0]);
      display.print(seq.arp_style_names[i][1]);
      display.print(seq.arp_style_names[i][2]);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(" ");
    }
    setModeColor(2);
    setCursor_textGrid_small(11, 6);
    if (seq.arp_speed == 0)
      display.print(F("1/16 "));
    else if (seq.arp_speed == 1)
      display.print(F("1/8  "));
    else if (seq.arp_speed == 2)
      display.print(F("1/32 "));
    else if (seq.arp_speed == 3)
      display.print(F("1/64 "));

    setModeColor(3);
    setCursor_textGrid_small(11, 7);
    print_formatted_number(seq.euclidean_offset, 2, 3, 1);

    setCursor_textGrid_small(11, 8);
    if (!seq.euclidean_active && generic_temp_select_menu == 4)
      setModeColor(4);
    else if (!seq.euclidean_active)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("LINEAR"));
    setCursor_textGrid_small(18, 8);
    if (seq.euclidean_active && generic_temp_select_menu == 4)
      setModeColor(4);
    else if (seq.euclidean_active)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("EUCLIDEAN"));

    setModeColor(5);

    setCursor_textGrid_small(11, 9);
    //snprintf_P(displayname, sizeof(displayname), PSTR("%02d"), seq.arp_num_notes_max);
    //display.print(displayname);
    print_formatted_number(seq.arp_num_notes_max, 2, 5, 1);


  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    seq.menu = 0;
    seq.edit_state = false;
    encoderDir[ENC_R].reset();

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_seq_mute_matrix(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
#if defined APC
    APC_MODE = APC_MUTE_MATRIX;
    apc_clear_grid();
    apc_clear_right_buttons();
#endif

    // setup function
    registerTouchHandler(handle_touchscreen_mute_matrix);
    display.fillScreen(COLOR_BACKGROUND);
    UI_toplineInfoText(1);
    display.setTextSize(1);
    display.setCursor(1, 2);
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("MUTE"));
    display.setCursor(1 + 5 * CHAR_width_small, 2);
    display.print(F("MATRIX"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(0, 4);
    display.print(F("MUTE/UNMUTE"));
    setCursor_textGrid_small(0, 5);
    display.print(F("AT/IN:"));
    helptext_l(back_text);
    helptext_r(F("TOUCH SCREEN TO MUTE/UNMUTE"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
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
          if (!seq.track_mute[button_count])
          {
            TouchButton::drawButton(x * spacerx, 95 + y * 55, "TRACK:", itoa(button_count + 1, buf, 10), TouchButton::BUTTON_ACTIVE);
#if defined APC
            apc(64 - 16 + x * 2 - 8 * y * 3, 78, 6);
#endif
          }
          else
          {
            TouchButton::drawButton(x * spacerx, 95 + y * 55, "TRACK:", itoa(button_count + 1, buf, 10), TouchButton::BUTTON_NORMAL);
#if defined APC
            apc(64 - 16 + x * 2 - 8 * y * 3, 1, 6);
#endif
          }
          button_count++;
        }
        else
        {
          if (x == 1)
          {
            if (seq.mute_mode == 0)
              TouchButton::drawButton(x * spacerx, 35, "REAL", "TIME", TouchButton::BUTTON_ACTIVE);
            else
              TouchButton::drawButton(x * spacerx, 35, "REAL", "TIME", TouchButton::BUTTON_NORMAL);
          }
          else if (x == 2)
          {
            if (seq.mute_mode == 1)
              TouchButton::drawButton(x * spacerx, 35, "NEXT", "PATTRN", TouchButton::BUTTON_ACTIVE);
            else
              TouchButton::drawButton(x * spacerx, 35, "NEXT", "PATTRN", TouchButton::BUTTON_NORMAL);
          }
          else if (x == 3)
          {
            if (seq.mute_mode == 2)
              TouchButton::drawButton(x * spacerx, 35, "SONG", "STEP", TouchButton::BUTTON_ACTIVE);
            else
              TouchButton::drawButton(x * spacerx, 35, "SONG", "STEP", TouchButton::BUTTON_NORMAL);
          }
        }
      }
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
  }
}

FLASHMEM uint8_t count_omni()
{
  uint8_t count = 0;

  if (configuration.dexed[0].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (configuration.dexed[1].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
#if (NUM_DEXED>2)
  if (configuration.dexed[2].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (configuration.dexed[3].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
#endif

#ifdef GRANULAR
  if (granular_params.midi_channel == MIDI_CHANNEL_OMNI)
    count++;
#endif

  if (configuration.epiano.midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (microsynth[0].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (microsynth[1].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (braids_osc.midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (msp[0].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (msp[1].midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (drum_midi_channel == MIDI_CHANNEL_OMNI)
    count++;
  if (slices_midi_channel == MIDI_CHANNEL_OMNI)
    count++;

  if (count != 0)
    check_and_confirm_midi_channels = true;
  return count;
}

FLASHMEM uint8_t count_midi_channel_duplicates(bool find_first)
{
  uint8_t count = 0;
  uint8_t midi_ch[17] = { 0 };

  // collect instrument MIDI channels
  uint8_t ins[] = {
    configuration.dexed[0].midi_channel,
    configuration.dexed[1].midi_channel,
#if (NUM_DEXED > 2)
    configuration.dexed[2].midi_channel,
    configuration.dexed[3].midi_channel,
#endif
#ifdef GRANULAR
    granular_params.midi_channel,
#endif
    configuration.epiano.midi_channel,
    microsynth[0].midi_channel,
    microsynth[1].midi_channel,
    braids_osc.midi_channel,
    msp[0].midi_channel,
    msp[1].midi_channel,
    drum_midi_channel,
    slices_midi_channel
  };

  // count usage of each MIDI channel
  for (uint8_t i = 1; i < 17; i++)
  {
    for (uint8_t j = 0; j < sizeof(ins) / sizeof(ins[0]); j++)
    {
      if (ins[j] == i)
        midi_ch[i]++;
    }
  }

  // find duplicates
  for (uint8_t i = 1; i < 17; i++)
  {
    if (midi_ch[i] > 1)
    {
      count++;
      if (find_first)
      {
        count = i;
        break;
      }
    }
  }

  if (count != 0)
    check_and_confirm_midi_channels = true;

  return count;
}


FLASHMEM bool load_performance_and_check_midi(uint8_t perf)
{
  bool ret = load_sd_performance_json(perf);

  // check MIDI channels
  if ((count_omni() != 0 || count_midi_channel_duplicates(false) != 0) && configuration.sys.skip_midi_channel_warning == false) // startup with midi channel setup page
    LCDML.OTHER_jumpToFunc(UI_func_midi_channels);

  return ret;
}

FLASHMEM void _cancel_message_old_menus()
{
  display.setTextSize(2);
  display.setTextColor(RED, COLOR_BACKGROUND);
  setCursor_textGrid(1, 2);
  display.print(F("Canceled.       "));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  delay(MESSAGE_WAIT_TIME);

}

extern uint8_t perf_load_options;

FLASHMEM void UI_func_load_performance(uint8_t param)
{
  static uint8_t mode;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_load_performance);
    registerScope(230, 18, 87, 64, true);
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    char tmp[10];

    if (configuration.sys.performance_number != 0)
      temp_int = configuration.sys.performance_number;
    else
      temp_int = param;

    mode = 0;
    encoderDir[ENC_R].reset();
    //display.fillScreen(COLOR_BACKGROUND);
    clear_bottom_half_screen_without_backbutton();
    border1_clear();
    setCursor_textGrid(1, 1);
    display.print(F("Load Performance"));
    setCursor_textGrid(1, 2);
    snprintf_P(tmp, sizeof(tmp), PSTR("[%02d]"), param);
    display.print(tmp);

    display.setTextSize(1);
    setCursor_textGrid(0, 4);
    display.print(F("SELECT PERFORMANCE PARTS:"));

    TouchButton::drawButton(GRID.X[0], GRID.Y[2], "DEXED", "", bitRead(perf_load_options, perf_dexed) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[1], GRID.Y[2], "MSYNTH", "", bitRead(perf_load_options, perf_msynth) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[2], GRID.Y[2], "EPIANO", "", bitRead(perf_load_options, perf_epiano) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[3], GRID.Y[2], "BRAIDS", "", bitRead(perf_load_options, perf_braids) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[4], GRID.Y[2], "CUST.", "SAMPLES", bitRead(perf_load_options, perf_samples) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);

    TouchButton::drawButton(GRID.X[0], GRID.Y[3], "EFFECT", "SET.", bitRead(perf_load_options, perf_effects) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[1], GRID.Y[3], "SEQ", "DATA", bitRead(perf_load_options, perf_seq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[2], GRID.Y[3], "LIVESEQ", "DATA", bitRead(perf_load_options, perf_liveseq) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);

    TouchButton::drawButton(GRID.X[4], GRID.Y[4], "NONE", "", TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[5], GRID.Y[4], "ALL", "", TouchButton::BUTTON_NORMAL);

    helptext_r(F("LOAD PERF."));
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, RED);
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0)
      {
        temp_int = constrain(temp_int + e.dir, 0, 99);
      }
    }

    if (e.pressed)
    {
      display.fillRect(0, CHAR_height * 3, CHAR_width * 19, CHAR_height * 1, COLOR_BACKGROUND);
      mode = 0xff;
      display.fillRect(230, 18, 90, 64, COLOR_BACKGROUND); // clear scope
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(1, 2);
      bool perfLoaded = load_performance_and_check_midi(temp_int);
      if (!perfLoaded)
      {
        display.setTextColor(RED, COLOR_BACKGROUND);
        display.print(F("Does not exist."));
      }
      else
      {
        setCursor_textGrid(1, 2);
        seq.play_mode = false;
        display.setTextColor(GREEN, COLOR_BACKGROUND);
        display.print(F("Done.           "));
      }
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }

    if (mode != 0xff)
    {
      setCursor_textGrid(1, 2);
      char tmp[10];
      snprintf_P(tmp, sizeof(tmp), PSTR("[%02d] "), temp_int);
      display.print(tmp);
      if (check_sd_performance_exists(temp_int))
      {
        get_sd_performance_name_json(temp_int);
        if (seq.name_temp[0] != 0)
          show(2, 6, 11, seq.name_temp);
        else
          display.print(F(" -- DATA --"));
      }
      else
        display.print(F("-- EMPTY --"));

      //      for (uint8_t nextslot = 1; nextslot < 5; nextslot++)
      //      {
      //        display.setTextColor(GREY3, COLOR_BACKGROUND);
      //        setCursor_textGrid(1, 2 + nextslot);
      //        if (temp_int + nextslot < 100)
      //        {
      //          snprintf_P(tmp, sizeof(tmp), PSTR("[%2d] "), temp_int + nextslot);
      //          display.print(tmp);
      //          if (check_sd_performance_exists(temp_int + nextslot))
      //          {
      //            get_sd_performance_name_json(temp_int + nextslot);
      //            if ( seq.name_temp[0] != 0 )
      //              show(2 + nextslot, 6, 11, seq.name_temp);
      //            else
      //              display.print(F(" -- DATA --"));
      //          }
      //          else print_empty_spaces(11);
      //        }
      //        else
      //        {
      //          setCursor_textGrid(1, 2 + nextslot);
      //          print_empty_spaces(11);
      //        }
      //      }
    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    if (mode < 0xff)
    {
      display.fillRect(0, CHAR_height * 3, CHAR_width * 19, CHAR_height * 1, COLOR_BACKGROUND);
      _cancel_message_old_menus();
    }
    else
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      configuration.sys.performance_number = temp_int;
      save_sd_sys_json();
      encoderDir[ENC_R].reset();
    }
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_save_performance(uint8_t param)
{
  static bool overwrite;
  static bool yesno;
  static uint8_t mode;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    char tmp[FILENAME_LEN];
    yesno = false;

    if (configuration.sys.performance_number != 0)
      temp_int = configuration.sys.performance_number;
    else
      temp_int = param;

    mode = 0;
    border1_clear();
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    setCursor_textGrid(1, 1);
    display.print(F("Save Performance"));
    setCursor_textGrid(1, 2);
    snprintf_P(tmp, sizeof(tmp), PSTR("[%2d] "), temp_int);
    display.print(tmp);

    if (check_sd_performance_exists(temp_int))
    {
      overwrite = true;
      get_sd_performance_name_json(temp_int);
      if (seq.name_temp[0] != 0)
        show(2, 6, 11, seq.name_temp);
      else
        display.print(F(" -- DATA --"));
    }
    else
      overwrite = false;
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, RED);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (mode == 0) {
        temp_int = constrain(temp_int + e.dir * e.speed, 0, 99);
      }
      else {
        yesno = e.down;
      }
    }
    if (e.pressed) {
      if (mode == 0 && overwrite == true) {
        mode = 1;
        setCursor_textGrid(1, 2);
        display.print(F("Overwrite: [   ]"));
      }
      else {
        mode = 0xff;
        if (overwrite == false || yesno == true) {
          save_sd_performance_json(temp_int);
          display.setTextColor(GREEN, COLOR_BACKGROUND);
          show(2, 1, 16, "Done.");
          delay(MESSAGE_WAIT_TIME);
          LCDML.FUNC_goBackToMenu();
        }
        else if (overwrite == true && yesno == false) {
          mode = 0;
          setCursor_textGrid(1, 2);
          display.printf("[%2d]   ", temp_int);
        }
      }
    }

    if (mode == 0) {
      overwrite = check_sd_performance_exists(temp_int);
      setCursor_textGrid(1, 2);
      display.printf("[%2d] ", temp_int);
      setCursor_textGrid(6, 2);
      if (overwrite) {
        get_sd_performance_name_json(temp_int);
        if (seq.name_temp[0] != 0) {
          show(2, 6, 11, seq.name_temp);
        }
        else {
          display.print(F("-- DATA --"));
        }
      }
      else {
        display.print(F("-- EMPTY --"));
      }
    }
    else if (mode == 1) {
      setCursor_textGrid(13, 2);
      display.print(yesno ? F("YES") : F("NO "));
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    if (mode < 0xff) {
      _cancel_message_old_menus();
    }
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void set_delay_sync(uint8_t sync, uint8_t instance)
{
  if (instance == 0 || instance == 1)
  {
    if (seq.running == false)
    {
      // uint16_t midi_sync_delay_time = uint16_t(60000.0 * midi_ticks_factor[sync] / midi_bpm + 0.5);
      uint16_t midi_sync_delay_time = uint16_t(60000.0 * midi_ticks_factor[sync] / seq.bpm);
      delay_fx[instance]->delay(0, constrain(midi_sync_delay_time * configuration.fx.delay_multiplier[instance], DELAY_TIME_MIN, DELAY_TIME_MAX * 10));
      if (midi_sync_delay_time > DELAY_MAX_TIME)
      {
#ifdef DEBUG
        LOG.println(F("Calculated MIDI-Sync delay: "));
        LOG.print(round(60000.0 * midi_ticks_factor[sync] / midi_bpm), DEC);
        LOG.println(F("ms"));
        LOG.println(F("MIDI-Sync delay: midi_sync_delay_time"));
        LOG.print(midi_sync_delay_time, DEC);
        LOG.println(F("ms"));
#endif
      }
    }
    else
    {
      uint16_t midi_sync_delay_time = uint16_t(60000.0 * midi_ticks_factor[sync] / seq.bpm);
      delay_fx[instance]->delay(0, constrain(midi_sync_delay_time * configuration.fx.delay_multiplier[instance], DELAY_TIME_MIN, DELAY_TIME_MAX * 10));
    }
  }
}

FLASHMEM void print_sync_timing(uint8_t sync)
{
  switch (sync)
  {
  case 1:
    display.print(F("1/16 "));
    break;
  case 2:
    display.print(F("1/16T"));
    break;
  case 3:
    display.print(F("1/8  "));
    break;
  case 4:
    display.print(F("1/8T "));
    break;
  case 5:
    display.print(F("1/4  "));
    break;
  case 6:
    display.print(F("1/4T "));
    break;
  case 7:
    display.print(F("1/2  "));
    break;
  case 8:
    display.print(F("1/2T "));
    break;
  case 9:
    display.print(F("1/1  "));
    break;
  }
}

FLASHMEM void print_delay_sync_status(uint8_t instance)
{

  if (configuration.fx.delay_sync[instance] > 0)
  {
    if (instance == 0)
    {
      setCursor_textGrid_small(6, 4);
    }
    else
    {
      setCursor_textGrid_small(22, 4);
    }
    display.setTextColor(GREY1, GREY2);
    display.print(F("ON "));
    if (instance == 0)
      setCursor_textGrid_small(10, 4);
    else
      setCursor_textGrid_small(26, 4);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    print_sync_timing(configuration.fx.delay_sync[instance]);
  }
  else
  {
    if (instance == 0)
    {
      setCursor_textGrid_small(6, 4);
    }
    else
    {
      setCursor_textGrid_small(22, 4);
    }
    display.setTextColor(GREY2, GREY3);
    display.print(F("OFF"));
    print_empty_spaces(6, 1);
  }
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void print_static_text_master_effects()
{
  if (configuration.fx.delay_multiplier[0] == 0)
    configuration.fx.delay_multiplier[0] = 1;
  if (configuration.fx.delay_multiplier[1] == 0)
    configuration.fx.delay_multiplier[1] = 1;

  display.setTextSize(1);
  display.setTextColor(RED, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 0);
  display.print(F("MASTER EFFECTS"));
  display.setTextColor(COLOR_SYSTEXT, GREY4);
  setCursor_textGrid_small(1, 2);
  display.print(F("DELAY A"));

  display.setTextColor(GREY1, GREY4);

  setCursor_textGrid_small(1, 3);
  display.print(F("TIME"));
  setCursor_textGrid_small(13, 3);
  display.print(F("MS"));
  setCursor_textGrid_small(1, 4);
  display.print(F("SYNC"));
  setCursor_textGrid_small(1, 5);
  display.print(F("GATE"));
  setCursor_textGrid_small(1, 6);
  display.print(F("THRD"));
  display.setTextColor(GREY1, GREY4);
  setCursor_textGrid_small(1, 7);
  display.print(F("FDBK"));
  setCursor_textGrid_small(1, 8);
  display.print(F("PAN"));
  setCursor_textGrid_small(1, 9);
  display.print(F("LEVEL"));
  setCursor_textGrid_small(1, 10);
  display.print(F("FILTER"));
  setCursor_textGrid_small(1, 11);
  display.print(F("FREQ."));

  display.setTextColor(COLOR_SYSTEXT, GREY3);
  setCursor_textGrid_small(17, 2);
  display.print(F("DELAY B"));
  display.setTextColor(GREY1, GREY3);

  setCursor_textGrid_small(17, 3);
  display.print(F("TIME"));
  setCursor_textGrid_small(29, 3);
  display.print(F("MS"));
  setCursor_textGrid_small(17, 4);
  display.print(F("SYNC"));
  setCursor_textGrid_small(17, 5);
  display.print(F("GATE"));
  setCursor_textGrid_small(17, 6);
  display.print(F("THRD"));
  display.setTextColor(GREY1, GREY3);
  setCursor_textGrid_small(17, 7);
  display.print(F("FDBK"));
  setCursor_textGrid_small(17, 8);
  display.print(F("PAN"));
  setCursor_textGrid_small(17, 9);
  display.print(F("LEVEL"));
  setCursor_textGrid_small(17, 10);
  display.print(F("FILTER"));
  setCursor_textGrid_small(17, 11);
  display.print(F("FREQ."));

  display.setTextColor(COLOR_SYSTEXT, GREY4);
  setCursor_textGrid_small(33, 2);
  display.print(F("REVERB"));
  display.setTextColor(GREY1, GREY4);
  setCursor_textGrid_small(33, 4);
  display.print(F("ROOMSIZE"));
  setCursor_textGrid_small(33, 5);
  display.print(F("LOWPASS"));
  setCursor_textGrid_small(33, 6);
  display.print(F("LODAMP"));
  setCursor_textGrid_small(33, 7);
  display.print(F("HIDAMP"));
  setCursor_textGrid_small(33, 8);
  display.print(F("DIFFUSION"));
  setCursor_textGrid_small(33, 9);
  display.print(F("LEVEL"));

  display.setTextColor(GREY2, GREY4);
  setCursor_textGrid_small(1, 12);
  display.print(F("SEND LEVELS A"));

  display.setTextColor(GREY2, GREY3);
  setCursor_textGrid_small(17, 12);
  display.print(F("SEND LEVELS B"));

  display.setTextColor(GREY1, GREY4);
  setCursor_textGrid_small(1, 13);
  display.print(F("DX1"));

  setCursor_textGrid_small(1, 14);
  display.print(F("DX2"));
  setCursor_textGrid_small(1, 15);
  display.print(F("DX3"));
  setCursor_textGrid_small(1, 16);
  display.print(F("DX4"));

  setCursor_textGrid_small(1, 17);
  display.print(F("EP"));
  setCursor_textGrid_small(1, 18);
  display.print(F("MS1"));
  setCursor_textGrid_small(1, 19);
  display.print(F("MS2"));
  setCursor_textGrid_small(1, 20);
  display.print(F("BRD"));
  setCursor_textGrid_small(1, 21);
  display.print(F("DL2"));
  setCursor_textGrid_small(1, 22);
  display.print(F("REV"));


  display.setTextColor(GREY1, GREY3);
  setCursor_textGrid_small(17, 13);
  display.print(F("DX1"));

  setCursor_textGrid_small(17, 14);
  display.print(F("DX2"));
  setCursor_textGrid_small(17, 15);
  display.print(F("DX3"));
  setCursor_textGrid_small(17, 16);
  display.print(F("DX4"));

  setCursor_textGrid_small(17, 17);
  display.print(F("EP"));
  setCursor_textGrid_small(17, 18);
  display.print(F("MS1"));
  setCursor_textGrid_small(17, 19);
  display.print(F("MS2"));
  setCursor_textGrid_small(17, 20);
  display.print(F("BRD"));
  setCursor_textGrid_small(17, 21);
  display.print(F("DL1"));
  setCursor_textGrid_small(17, 22);
  display.print(F("REV"));

  display.setTextColor(GREY1, GREY4);
  setCursor_textGrid_small(33, 13);
  display.print(F("DX1"));
  setCursor_textGrid_small(33, 14);
  display.print(F("DX2"));
  setCursor_textGrid_small(33, 15);
  display.print(F("DX3"));
  setCursor_textGrid_small(33, 16);
  display.print(F("DX4"));

  setCursor_textGrid_small(33, 17);
  display.print(F("EP"));
  setCursor_textGrid_small(33, 18);
  display.print(F("MS1"));
  setCursor_textGrid_small(33, 19);
  display.print(F("MS2"));
  setCursor_textGrid_small(33, 20);
  display.print(F("BRD"));

  display.setTextColor(GREEN, GREY4);

  display.setTextSize(1);
  setCursor_textGrid_small(1 + 5, 5);
  display.print(F("STATE"));
  display.setTextColor(GREEN, GREY3);
  setCursor_textGrid_small(17 + 5, 5);
  display.print(F("STATE"));


}

FLASHMEM void print_delay_time(uint8_t instance, uint8_t param)
{
  setModeColor(param);
  if (instance == 0)
    setCursor_textGrid_small(6 + 3, 3);
  else
    setCursor_textGrid_small(22 + 3, 3);

  if (configuration.fx.delay_sync[instance] > 0)
  {
    uint16_t synced_delay_time = uint16_t(60000.0 * midi_ticks_factor[configuration.fx.delay_sync[instance]] / seq.bpm) * configuration.fx.delay_multiplier[instance];
    if (synced_delay_time <= DELAY_MAX_TIME)
    {
      if (instance == 0)
        //setCursor_textGrid_small(6 + 3, 3);
        print_formatted_number(synced_delay_time, 4, 2, 1);
      else if (instance == 1)
        //setCursor_textGrid_small(22 + 3, 3);
        print_formatted_number(synced_delay_time, 4, 19, 1);
      if (seq.edit_state == 1)
        helptext_r(F("SCROLL > FOR TIME IN MS"));
    }
    else
    {
      display.print(F("---"));
      if (seq.edit_state == 1)
        helptext_r(F("NOT ENOUGH MEMORY"));
    }
  }
  else
  {
    if (instance == 0)
      print_formatted_number(configuration.fx.delay_time[instance] * 10, 4, 1, 1);
    else
      print_formatted_number(configuration.fx.delay_time[instance] * 10, 4, 19, 1);
    if (seq.edit_state == 1)
      helptext_r(F("SCROLL < FOR SYNCED TIME"));
  }
}

FLASHMEM void set_global_delay_filter(uint8_t instance)
{
  global_delay_filter[instance].resonance(3);
  if (configuration.fx.delay_filter_mode[instance] == 0)
  {

    global_delay_filter_mixer[instance].gain(0, 1.0);
    global_delay_filter_mixer[instance].gain(1, 0.0);
    global_delay_filter_mixer[instance].gain(2, 0.0);
    global_delay_filter_mixer[instance].gain(3, 0.0);
  }
  else if (configuration.fx.delay_filter_mode[instance] == 1)
  {
    global_delay_filter[instance].frequency(configuration.fx.delay_filter_freq[instance] / 1.7);
    global_delay_filter_mixer[instance].gain(0, 0.0);
    global_delay_filter_mixer[instance].gain(1, 1.0);
  }
  else if (configuration.fx.delay_filter_mode[instance] == 2)
  {
    global_delay_filter[instance].frequency(configuration.fx.delay_filter_freq[instance] / 1.6);
    global_delay_filter_mixer[instance].gain(0, 0.0);
    global_delay_filter_mixer[instance].gain(2, 1.0);
  }
  else if (configuration.fx.delay_filter_mode[instance] == 3)
  {
    global_delay_filter[instance].frequency(configuration.fx.delay_filter_freq[instance] * 2.6);
    global_delay_filter_mixer[instance].gain(0, 0.0);
    global_delay_filter_mixer[instance].gain(3, 1.0);
  }
}

FLASHMEM void print_delay_filter_mode(uint8_t instance, uint8_t param)
{
  uint8_t x = 0;
  if (instance == 0)
    x = 10;
  else if (instance == 1)
    x = 26;
  setModeColor(param);
  setCursor_textGrid_small(x, 10);
  if (configuration.fx.delay_filter_mode[instance] == 0)
    display.print(F("OFF "));

  else if (configuration.fx.delay_filter_mode[instance] == 1)
    display.print(F("LOW "));
  else if (configuration.fx.delay_filter_mode[instance] == 2)
    display.print(F("BAND"));
  else if (configuration.fx.delay_filter_mode[instance] == 3)
    display.print(F("HIGH"));
  set_global_delay_filter(instance);
}

FLASHMEM void print_delay_filter_freq(uint8_t instance, uint8_t param)
{
  uint8_t x = 0;
  if (instance == 0)
    x = 10;
  else if (instance == 1)
    x = 26;
  setModeColor(param);
  print_small_intbar(x - 4, 11, configuration.fx.delay_filter_freq[instance] / 100, param, 1, 1);
  set_global_delay_filter(instance);
}

FLASHMEM void print_delay_multiplier(uint8_t instance, uint8_t param)
{
  setModeColor(param);

  if (instance == 0)
  {
    setCursor_textGrid_small(6, 3);
    print_formatted_number(configuration.fx.delay_multiplier[instance], 1, 1, 1);
  }
  else
  {
    setCursor_textGrid_small(22, 3);
    print_formatted_number(configuration.fx.delay_multiplier[instance], 1, 18, 1);
  }
  // display.print(configuration.fx.delay_multiplier[instance]);

  display.print(F("x"));
  print_delay_time(instance, 99);
  set_global_delay_filter(instance);
}

extern AudioEffectGate* Gate[NUM_DEXED];

FLASHMEM void update_selective_values_master_effects()
{
  print_delay_sync_status(0);
  print_delay_sync_status(1);

  if (menu_item_check(1))
  {
    print_delay_multiplier(0, 1);
    print_small_intbar(43, 20, braids_osc.rev_send, 48, 0, 0);
  }

  if (menu_item_check(2))
    print_delay_time(0, 2);

  if (menu_item_check(3))
    print_small_intbar(6, 7, configuration.fx.delay_feedback[0], 3, 1, 0);

  if (menu_item_check(4))
    print_small_panbar(6, 8, configuration.fx.delay_pan[0], 4);

  if (menu_item_check(5))
    print_small_intbar(6, 9, configuration.fx.delay_level_global[0], 5, 1, 0);

  if (menu_item_check(6))
    print_delay_filter_mode(0, 6);

  if (menu_item_check(7))
    print_delay_filter_freq(0, 7);

  if (menu_item_check(8))
    print_small_intbar(6, 13, configuration.fx.delay_level1[0], 8, 1, 0);
  if (menu_item_check(9))
    print_small_intbar(6, 14, configuration.fx.delay_level1[1], 9, 1, 0);
#if (NUM_DEXED>2)
  if (menu_item_check(10))
    print_small_intbar(6, 15, configuration.fx.delay_level1[2], 10, 1, 0);
  if (menu_item_check(11))
    print_small_intbar(6, 16, configuration.fx.delay_level1[3], 11, 1, 0);
#else
  if (menu_item_check(10))
    print_small_intbar(6, 15, 0, 10, 1, 0);
  if (menu_item_check(11))
    print_small_intbar(6, 16, 0, 11, 1, 0);
#endif
  if (menu_item_check(12))
    print_small_intbar(6, 17, configuration.fx.ep_delay_send_1, 12, 1, 0);
  if (menu_item_check(13))
    print_small_intbar(6, 18, microsynth[0].delay_send[0], 13, 1, 0);
  if (menu_item_check(14))
    print_small_intbar(6, 19, microsynth[1].delay_send[0], 14, 1, 0);
  if (menu_item_check(15))
    print_small_intbar(6, 20, braids_osc.delay_send_1, 15, 1, 0);
  if (menu_item_check(16))
    print_small_intbar(6, 21, configuration.fx.delay1_to_delay2, 16, 1, 0);
  if (menu_item_check(17))
    print_small_intbar(6, 22, configuration.fx.delay_to_reverb[0], 17, 1, 0);


  if (menu_item_check(18))
    print_delay_multiplier(1, 18);

  if (menu_item_check(19))
    print_delay_time(1, 19);

  if (menu_item_check(20))
    print_small_intbar(22, 7, configuration.fx.delay_feedback[1], 20, 1, 0);

  if (menu_item_check(21))
    print_small_panbar(22, 8, configuration.fx.delay_pan[1], 21);

  if (menu_item_check(22))
    print_small_intbar(22, 9, configuration.fx.delay_level_global[1], 22, 1, 0);

  if (menu_item_check(23))
    print_delay_filter_mode(1, 23);

  if (menu_item_check(24))
    print_delay_filter_freq(1, 24);

  if (menu_item_check(25))
    print_small_intbar(22, 13, configuration.fx.delay_level2[0], 25, 1, 0);

  if (menu_item_check(26))
    print_small_intbar(22, 14, configuration.fx.delay_level2[1], 26, 1, 0);
#if (NUM_DEXED>2)
  if (menu_item_check(27))
    print_small_intbar(22, 15, configuration.fx.delay_level2[2], 27, 1, 0);
  if (menu_item_check(28))
    print_small_intbar(22, 16, configuration.fx.delay_level2[3], 28, 1, 0);
#else
  if (menu_item_check(27))
    print_small_intbar(22, 15, 0, 27, 1, 0);
  if (menu_item_check(28))
    print_small_intbar(22, 16, 0, 28, 1, 0);
#endif

  if (menu_item_check(29))
    print_small_intbar(22, 17, configuration.fx.ep_delay_send_2, 29, 1, 0);
  if (menu_item_check(30))
    print_small_intbar(22, 18, microsynth[0].delay_send[1], 30, 1, 0);
  if (menu_item_check(31))
    print_small_intbar(22, 19, microsynth[1].delay_send[1], 31, 1, 0);
  if (menu_item_check(32))
    print_small_intbar(22, 20, braids_osc.delay_send_2, 32, 1, 0);
  if (menu_item_check(33))
    print_small_intbar(22, 21, configuration.fx.delay2_to_delay1, 33, 1, 0);
  if (menu_item_check(34))
    print_small_intbar(22, 22, configuration.fx.delay_to_reverb[1], 34, 1, 0);

  if (menu_item_check(35))
    print_small_intbar(43, 4, configuration.fx.reverb_roomsize, 35, 1, 0);
  if (menu_item_check(36))
    print_small_intbar(43, 5, configuration.fx.reverb_lowpass, 36, 1, 0);
  if (menu_item_check(37))
    print_small_intbar(43, 6, configuration.fx.reverb_lodamp, 37, 1, 0);
  if (menu_item_check(38))
    print_small_intbar(43, 7, configuration.fx.reverb_hidamp, 38, 1, 0);
  if (menu_item_check(39))
    print_small_intbar(43, 8, configuration.fx.reverb_diffusion, 39, 1, 0);
  if (menu_item_check(40))
    print_small_intbar(43, 9, configuration.fx.reverb_level, 40, 1, 0);

  if (menu_item_check(41))
    print_small_intbar(43, 13, configuration.fx.reverb_send[0], 41, 1, 0);
  if (menu_item_check(42))
    print_small_intbar(43, 14, configuration.fx.reverb_send[1], 42, 1, 0);
#if (NUM_DEXED>2)
  if (menu_item_check(43))
    print_small_intbar(43, 15, configuration.fx.reverb_send[2], 43, 1, 0);
  if (menu_item_check(44))
    print_small_intbar(43, 16, configuration.fx.reverb_send[3], 44, 1, 0);
#else
  if (menu_item_check(43))
    print_small_intbar(43, 15, 0, 43, 1, 0);
  if (menu_item_check(44))
    print_small_intbar(43, 16, 0, 44, 1, 0);
#endif
  if (menu_item_check(45))
    print_small_intbar(43, 17, configuration.fx.ep_reverb_send, 45, 1, 0);
  if (menu_item_check(46))
    print_small_intbar(43, 18, microsynth[0].rev_send, 46, 1, 0);
  if (menu_item_check(47))
    print_small_intbar(43, 19, microsynth[1].rev_send, 47, 1, 0);
  if (menu_item_check(48))
  {
    print_small_intbar(43, 20, braids_osc.rev_send, 48, 1, 0);
    print_delay_multiplier(0, 1);
  }

  display.setTextColor(COLOR_SYSTEXT, GREY3);
  setCursor_textGrid_small(1 + 5, 6);
  display.print(Gate[0]->get_threshold());
  setCursor_textGrid_small(22, 6);
  display.setTextColor(COLOR_SYSTEXT, GREY2);
  display.print(Gate[1]->get_threshold());
}

FLASHMEM void UI_func_master_effects(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);

    //setCursor_textGrid_small(4, 23);
    display.setCursor(CHAR_width_small * 5, DISPLAY_HEIGHT - CHAR_height_small);
    display.setTextColor(GREY2);
    display.setTextSize(1);

#ifdef PSRAM
    uint8_t size = external_psram_size;
    if (size != 0) {
      display.printf("%2d MB PSRAM, MAX DELAY: 2x %dMS", size, DELAY_MAX_TIME);
    }
    else {
      display.print(F("NO VALID PSRAM FOUND"));
    }
#else
    display.print(F("FOR LONGER DELAYS ADD PSRAM"));
#endif
    display.fillRect(0, 15, 95, 214, GREY4);
    display.fillRect(98, 15, 92, 214, GREY3);
    display.fillRect(193, 15, 130, 214, GREY4);

    print_static_text_master_effects();
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver)) {
      generic_temp_select_menu = 1;
    }
    generic_full_draw_required = true;
    update_selective_values_master_effects();
    generic_full_draw_required = false;

    helptext_l(back_text);
    helptext_r(F("SELECT PARAM."));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter()) {
      seq.edit_state = !seq.edit_state;
      if (seq.edit_state == 0 && generic_temp_select_menu == 0) {
        helptext_r(F("SELECT PARAM"));
      }
      else if (seq.edit_state == 1 && generic_temp_select_menu != 0) {
        helptext_r(F("EDIT VALUE"));
      }
      update_selective_values_master_effects();
    }
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        if (e.down && generic_temp_select_menu < 48)
          generic_temp_select_menu++;
        else if (e.down)
          generic_temp_select_menu = 1;
        else if (e.up && generic_temp_select_menu > 1)
          generic_temp_select_menu--;
        else if (e.up)
          generic_temp_select_menu = 48;
        update_selective_values_master_effects();
      }
      else {
        switch (generic_temp_select_menu) {
        case 1:
          configuration.fx.delay_multiplier[0] = constrain(configuration.fx.delay_multiplier[0] + (e.dir * 1), 1, 4);
          // multiplier 1
          print_delay_multiplier(0, 1);
          if (configuration.fx.delay_sync[0] > 0) {
            set_delay_sync(configuration.fx.delay_sync[0], 0); // go to MIDI Sync
          }
          break;

        case 2:
          // delay time
          master_effects_set_delay_time(0, e.dir * e.speed);
          print_delay_time(0, 2);
          print_delay_sync_status(0);
          break;

        case 3:
          master_effects_set_delay_feedback(0, e.dir * e.speed); // feedback instance 0
          print_small_intbar(6, 7, configuration.fx.delay_feedback[0], 3, 1, 0);
          break;

        case 4:
          // pan
          master_effects_set_delay_panorama(0, e.dir * e.speed);
          print_small_panbar(6, 8, configuration.fx.delay_pan[0], 4);
          break;

        case 5:
          // level
          master_effects_delay_level_global(0, e.dir * e.speed);
          print_small_intbar(6, 9, configuration.fx.delay_level_global[0], 5, 1, 0);
          break;

        case 6:
          configuration.fx.delay_filter_mode[0] = constrain(configuration.fx.delay_filter_mode[0] + (e.dir * 1), 0, 3);
          print_delay_filter_mode(0, 6);
          break;

        case 7:
          configuration.fx.delay_filter_freq[0] = constrain(configuration.fx.delay_filter_freq[0] + (e.dir * 50 * ENCODER[ENC_R].speed()), 0, 9999);
          print_delay_filter_freq(0, 7);
          break;

        case 8:
          configuration.fx.delay_level1[0] = constrain(configuration.fx.delay_level1[0] + (e.dir * ENCODER[ENC_R].speed()), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(0, 0, configuration.fx.delay_level1[0]);
          print_small_intbar(6, 13, configuration.fx.delay_level1[0], 8, 1, 0);
          break;

        case 9:
          configuration.fx.delay_level1[1] = constrain(configuration.fx.delay_level1[1] + (e.dir * ENCODER[ENC_R].speed()), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(0, 1, configuration.fx.delay_level1[1]);
          print_small_intbar(6, 14, configuration.fx.delay_level1[1], 9, 1, 0);
          break;
#if (NUM_DEXED>2)
        case 10:
          configuration.fx.delay_level1[2] = constrain(configuration.fx.delay_level1[2] + (e.dir * ENCODER[ENC_R].speed()), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(0, 2, configuration.fx.delay_level1[2]);
          print_small_intbar(6, 15, configuration.fx.delay_level1[2], 10, 1, 0);
          break;

        case 11:
          configuration.fx.delay_level1[3] = constrain(configuration.fx.delay_level1[3] + (e.dir * ENCODER[ENC_R].speed()), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(0, 3, configuration.fx.delay_level1[3]);
          print_small_intbar(6, 16, configuration.fx.delay_level1[3], 11, 1, 0);
          break;
#endif

        case 12:
          // epiano delay level
          configuration.fx.ep_delay_send_1 = constrain(configuration.fx.ep_delay_send_1 + (e.dir * ENCODER[ENC_R].speed()), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          global_delay_in_mixer[0]->gain(7, mapfloat(configuration.fx.ep_delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 5
          global_delay_in_mixer[0]->gain(8, mapfloat(configuration.fx.ep_delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 6
          print_small_intbar(6, 17, configuration.fx.ep_delay_send_1, 9, 1, 0);
          break;

        case 13:
          // microsynth 1 delay1 level
          microsynth[0].delay_send[0] = constrain(microsynth[0].delay_send[0] + (e.dir * ENCODER[ENC_R].speed()), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[0]->gain(4, mapfloat(microsynth[0].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 2
          print_small_intbar(6, 18, microsynth[0].delay_send[0], 10, 1, 0);
          break;

        case 14:
          // microsynth 2 delay1 level
          microsynth[1].delay_send[0] = constrain(microsynth[1].delay_send[0] + (e.dir * ENCODER[ENC_R].speed()), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[0]->gain(5, mapfloat(microsynth[1].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 3
          print_small_intbar(6, 19, microsynth[1].delay_send[0], 11, 1, 0);
          break;

        case 15:
          // braids delay level
          braids_osc.delay_send_1 = constrain(braids_osc.delay_send_1 + (e.dir * ENCODER[ENC_R].speed()), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[0]->gain(6, mapfloat(braids_osc.delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 4
          print_small_intbar(6, 20, braids_osc.delay_send_1, 12, 1, 0);
          break;

        case 16:
          // delay1 to delay2
          configuration.fx.delay1_to_delay2 = constrain(configuration.fx.delay1_to_delay2 + (e.dir * ENCODER[ENC_R].speed()), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[1]->gain(9, mapfloat(configuration.fx.delay1_to_delay2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.9));//previously 7
          print_small_intbar(6, 21, configuration.fx.delay1_to_delay2, 13, 1, 0);
          break;

        case 17:
          // delay1 to reverb send
          configuration.fx.delay_to_reverb[0] = constrain(configuration.fx.delay_to_reverb[0] + (e.dir * ENCODER[ENC_R].speed()), REVERB_LEVEL_MIN, REVERB_LEVEL_MAX);
          reverb_mixer_l.gain(REVERB_MIX_CH_AUX_DELAY1, mapfloat(configuration.fx.delay_to_reverb[0], REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, 1.0));
          reverb_mixer_r.gain(REVERB_MIX_CH_AUX_DELAY1, mapfloat(configuration.fx.delay_to_reverb[0], REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, 1.0));
          print_small_intbar(6, 22, configuration.fx.delay_to_reverb[0], 14, 1, 0);
          break;

        case 18:
          configuration.fx.delay_multiplier[1] = constrain(configuration.fx.delay_multiplier[1] + (e.dir * 1), 1, 4);
          if (configuration.fx.delay_sync[1] > 0) {
            set_delay_sync(configuration.fx.delay_sync[1], 1); // go to MIDI Sync
          }
          print_delay_multiplier(1, 15);
          break;

        case 19:
          // delay time
          master_effects_set_delay_time(1, e.dir * e.speed);
          print_delay_time(1, 16);
          print_delay_sync_status(1);
          break;

        case 20:
          master_effects_set_delay_feedback(1, e.dir * e.speed); // feedback
          print_small_intbar(22, 7, configuration.fx.delay_feedback[1], 20, 1, 0);
          break;

        case 21:
          // pan
          master_effects_set_delay_panorama(1, e.dir * e.speed);
          print_small_panbar(22, 8, configuration.fx.delay_pan[1], 21);
          break;

        case 22:
          // level
          master_effects_delay_level_global(1, e.dir * e.speed);
          print_small_intbar(22, 9, configuration.fx.delay_level_global[1], 22, 1, 0);
          break;

        case 23:
          configuration.fx.delay_filter_mode[1] = constrain(configuration.fx.delay_filter_mode[1] + (e.dir * 1), 0, 3);
          print_delay_filter_mode(1, 23);
          break;

        case 24:
          configuration.fx.delay_filter_freq[1] = constrain(configuration.fx.delay_filter_freq[1] + (50 * e.dir * e.speed), 0, 9999);
          print_delay_filter_freq(1, 24);
          break;

        case 25:
          //  delay2 send
          configuration.fx.delay_level2[0] = constrain(configuration.fx.delay_level2[0] + (e.dir * e.speed), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(1, 0, configuration.fx.delay_level2[0]);
          print_small_intbar(22, 13, configuration.fx.delay_level2[0], 25, 1, 0);
          break;

        case 26:
          //  delay2 send
          configuration.fx.delay_level2[1] = constrain(configuration.fx.delay_level2[1] + (e.dir * e.speed), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(1, 1, configuration.fx.delay_level2[1]);
          print_small_intbar(22, 14, configuration.fx.delay_level2[1], 26, 1, 0);
          break;
#if (NUM_DEXED>2)
        case 27:
          //  delay2 send
          configuration.fx.delay_level2[2] = constrain(configuration.fx.delay_level2[2] + (e.dir * e.speed), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(1, 2, configuration.fx.delay_level2[2]);
          print_small_intbar(22, 15, configuration.fx.delay_level2[2], 27, 1, 0);
          break;

        case 28:
          //  delay2 send
          configuration.fx.delay_level2[3] = constrain(configuration.fx.delay_level2[3] + (e.dir * e.speed), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          set_dexed_delay_level(1, 3, configuration.fx.delay_level2[3]);
          print_small_intbar(22, 16, configuration.fx.delay_level2[3], 28, 1, 0);
          break;
#endif

        case 29:
          // ep send
          configuration.fx.ep_delay_send_2 = constrain(configuration.fx.ep_delay_send_2 + (e.dir * e.speed), DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
          global_delay_in_mixer[1]->gain(7, mapfloat(configuration.fx.ep_delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 5
          global_delay_in_mixer[1]->gain(8, mapfloat(configuration.fx.ep_delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 6
          print_small_intbar(22, 17, configuration.fx.ep_delay_send_2, 29, 1, 0);
          break;

        case 30:
          // microsynth 1 delay2 level
          microsynth[0].delay_send[1] = constrain(microsynth[0].delay_send[1] + (e.dir * e.speed), DELAY_TIME_MIN, DELAY_LEVEL_MAX);
          global_delay_in_mixer[1]->gain(4, mapfloat(microsynth[0].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 2
          print_small_intbar(22, 18, microsynth[0].delay_send[1], 30, 1, 0);
          break;

        case 31:
          // microsynth 2 delay2 level
          microsynth[1].delay_send[1] = constrain(microsynth[1].delay_send[1] + (e.dir * e.speed), DELAY_TIME_MIN, DELAY_LEVEL_MAX);
          global_delay_in_mixer[1]->gain(5, mapfloat(microsynth[1].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 3
          print_small_intbar(22, 19, microsynth[1].delay_send[1], 31, 1, 0);
          break;

        case 32:
          // braids delay2 level
          braids_osc.delay_send_2 = constrain(braids_osc.delay_send_2 + (e.dir * e.speed), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[1]->gain(6, mapfloat(braids_osc.delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.0));//previously 4
          print_small_intbar(22, 20, braids_osc.delay_send_2, 32, 1, 0);
          break;

        case 33:
          configuration.fx.delay2_to_delay1 = constrain(configuration.fx.delay2_to_delay1 + (e.dir * e.speed), 0, DELAY_LEVEL_MAX);
          global_delay_in_mixer[0]->gain(9, mapfloat(configuration.fx.delay2_to_delay1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.9));//previously 7
          print_small_intbar(22, 21, configuration.fx.delay2_to_delay1, 33, 1, 0);
          break;

        case 34:
          // delay2 to reverb
          configuration.fx.delay_to_reverb[1] = constrain(configuration.fx.delay_to_reverb[1] + (e.dir * e.speed), REVERB_LEVEL_MIN, REVERB_LEVEL_MAX);
          reverb_mixer_l.gain(REVERB_MIX_CH_AUX_DELAY2, mapfloat(configuration.fx.delay_to_reverb[1], REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, 1.0));
          reverb_mixer_r.gain(REVERB_MIX_CH_AUX_DELAY2, mapfloat(configuration.fx.delay_to_reverb[1], REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, 1.0));
          print_small_intbar(22, 22, configuration.fx.delay_to_reverb[1], 34, 1, 0);
          break;

        case 35:
          // reverb room size
          reverb_roomsize(e.dir * e.speed);
          print_small_intbar(43, 4, configuration.fx.reverb_roomsize, 35, 1, 0);
          break;

        case 36:
          reverb_lowpass(e.dir * e.speed);
          print_small_intbar(43, 5, configuration.fx.reverb_lowpass, 36, 1, 0);
          break;

        case 37:
          reverb_lodamp(e.dir * e.speed);
          print_small_intbar(43, 6, configuration.fx.reverb_lodamp, 37, 1, 0);
          break;

        case 38:
          reverb_hidamp(e.dir * e.speed);
          print_small_intbar(43, 7, configuration.fx.reverb_hidamp, 38, 1, 0);
          break;

        case 39:
          reverb_diffusion(e.dir * e.speed);
          print_small_intbar(43, 8, configuration.fx.reverb_diffusion, 39, 1, 0);
          break;

        case 40:
          reverb_level(e.dir * e.speed);
          print_small_intbar(43, 9, configuration.fx.reverb_level, 40, 1, 0);
          break;

        case 41:
          master_effects_set_reverb_send(0, e.dir * e.speed);
          print_small_intbar(43, 13, configuration.fx.reverb_send[0], 41, 1, 0);
          break;

        case 42:
          master_effects_set_reverb_send(1, e.dir * e.speed);
          print_small_intbar(43, 14, configuration.fx.reverb_send[1], 42, 1, 0);
          break;
#if (NUM_DEXED>2)
        case 43:
          master_effects_set_reverb_send(2, e.dir * e.speed);
          print_small_intbar(43, 15, configuration.fx.reverb_send[2], 43, 1, 0);
          break;

        case 44:
          master_effects_set_reverb_send(3, e.dir * e.speed);
          print_small_intbar(43, 16, configuration.fx.reverb_send[3], 44, 1, 0);
          break;
#endif

        case 45:
          // epiano reverb send
          configuration.fx.ep_reverb_send = constrain(configuration.fx.ep_reverb_send + (e.dir * e.speed), REVERB_SEND_MIN, REVERB_SEND_MAX);
          MD_sendControlChange(configuration.epiano.midi_channel, 91, configuration.fx.ep_reverb_send);
          reverb_mixer_r.gain(REVERB_MIX_CH_EPIANO, mapfloat(configuration.fx.ep_reverb_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.0)); // EPiano Reverb-Send
          reverb_mixer_l.gain(REVERB_MIX_CH_EPIANO, mapfloat(configuration.fx.ep_reverb_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.0)); // EPiano Reverb-Send
          print_small_intbar(43, 17, configuration.fx.ep_reverb_send, 45, 1, 0);
          break;

        case 46:
          microsynth[0].rev_send = constrain(microsynth[0].rev_send + (e.dir * e.speed), REVERB_SEND_MIN, REVERB_SEND_MAX);
          microsynth_mixer_reverb.gain(0, volume_transform(mapfloat(microsynth[0].rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
          print_small_intbar(43, 18, microsynth[0].rev_send, 46, 1, 0);
          break;

        case 47:
          microsynth[1].rev_send = constrain(microsynth[1].rev_send + (e.dir * e.speed), REVERB_SEND_MIN, REVERB_SEND_MAX);
          microsynth_mixer_reverb.gain(1, volume_transform(mapfloat(microsynth[1].rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
          print_small_intbar(43, 19, microsynth[1].rev_send, 47, 1, 0);
          break;

        case 48:
          braids_osc.rev_send = constrain(braids_osc.rev_send + (e.dir * e.speed), REVERB_SEND_MIN, REVERB_SEND_MAX);
          braids_mixer_reverb.gain(0, volume_transform(mapfloat(braids_osc.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
          braids_mixer_reverb.gain(1, volume_transform(mapfloat(braids_osc.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
          print_small_intbar(43, 20, braids_osc.rev_send, 48, 1, 0);
          break;
        }
      }
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void sysinfo_reload_prev_voice(DxPoolSettings settings)
{
  if (seq.running == false) {
    MicroDexed[0]->keyup(MIDI_E4);
    MicroDexed[0]->keyup(MIDI_G3);
    MicroDexed[0]->keyup(MIDI_AIS5);
    MicroDexed[0]->keyup(MIDI_D5);
    MicroDexed[0]->keyup(MIDI_D4);
    MicroDexed[0]->keyup(MIDI_F4);

    // reload current(previous active) dexed0 patch
    MicroDexed[0]->setGain(midi_volume_transform(map(configuration.dexed[0].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127)));
    load_sd_voice(settings.pool, settings.bank, settings.voice, 0);
    configuration.dexed[0].transpose = settings.transpose;
    MicroDexed[0]->setTranspose(configuration.dexed[0].transpose);
  }
}

FLASHMEM void UI_func_information(uint8_t param)
{
  static uint16_t loopMs = 0;
  static uint8_t sysinfo_logo_version = 0;
  static bool sysinfo_page_at_bootup_shown_once = false;
  static uint8_t sysinfo_chord_state = 0;
  static DxPoolSettings oldSettings;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerScope(180, 150, 128, 50);
    sysinfo_chord_state = 0;
    char version_string[display_cols + 10 + 1];
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    generate_version_string(version_string, sizeof(version_string));
    display.setCursor(CHAR_width_small * 4 - 1, CHAR_height_small * 19);
    display.setTextSize(1);
    display.print(version_string);
    display.setCursor(CHAR_width_small * 4 - 1, CHAR_height_small * 21);
    display.print(sd_string);
    display.setCursor(CHAR_width_small * 4 - 1, CHAR_height_small * 23);
    display.setTextColor(GREY2);
    display.print(F("COMPILED FOR "));
    display.setTextColor(RED);
#ifdef COMPILE_FOR_PROGMEM
    display.print(F("PROGMEM"));
#endif
#ifdef COMPILE_FOR_PSRAM
    display.print(F("PSRAM ;-)"));
#endif
#ifdef COMPILE_FOR_SDCARD
    display.print(F("SD CARD"));
#endif
    display.setTextColor(GREY1);
    display.setCursor(CHAR_width_small * 34 - 2, CHAR_height_small * 25);
    display.print(F("CPU"));
    display.setCursor(CHAR_width_small * 41 - 2, CHAR_height_small * 25);
    display.print(F("%"));
    display.setCursor(CHAR_width_small * 43 - 2, CHAR_height_small * 25);
    display.print(F("TEMP"));
    display.setCursor(CHAR_width_small * 51 - 2, CHAR_height_small * 25);
    display.print(F("C"));
    display.setCursor(CHAR_width_small * 4 - 2, CHAR_height_small * 25);
    display.setTextColor(COLOR_BACKGROUND, GREY2);
#ifdef DEBUG
    display.setTextColor(COLOR_SYSTEXT, RED);
#endif
    display.print(F("DEBUG"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);
    if (remote_active)
      display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("REMOTE_CON"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));

    display.setTextColor(COLOR_BACKGROUND, GREY2);
    if (external_psram_size != 0)
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    else
      display.setTextColor(COLOR_BACKGROUND, GREY2);
    display.print(F("PSRAM:"));
    display.print(F(" "));
    if (external_psram_size != 0)
    {
      display.print(external_psram_size);
      display.print(" MB");
    }
    else
    {
      display.print("NONE");
    }

    display.setCursor(CHAR_width_small * 4 - 2, CHAR_height_small * 27);
    display.setTextColor(COLOR_BACKGROUND, GREY2);
#ifdef I2S_AUDIO_ONLY
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("I2S"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

#ifdef TEENSY_AUDIO_BOARD
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("T_AUDIO"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

#ifdef MIDI_DEVICE_DIN
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("MIDI DIN"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

#ifdef MIDI_DEVICE_USB
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("MIDI USB"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

#ifdef MIDI_DEVICE_USB_HOST
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("USB HOST"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

#ifdef AUDIO_DEVICE_USB
    display.setTextColor(COLOR_SYSTEXT, GREY2);
#endif
    display.print(F("AUDIO USB"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F(" "));
    display.setTextColor(COLOR_BACKGROUND, GREY2);

    /////SPDIF
    display.setTextColor(GREY1);
    display.setCursor(CHAR_width_small * 8 - 2, CHAR_height_small * 29);
    display.print(F("SPDIF IN:"));
    display.setCursor(CHAR_width_small * 21 - 2, CHAR_height_small * 29);
    display.print(F("FREQ:"));

    /// CAPACITIVE TOUCH STATUS
#ifdef CAPACITIVE_TOUCH_DISPLAY
    display.setCursor(CHAR_width_small * 34 - 2, CHAR_height_small * 29);
    if (touch_ic_found)
      display.setTextColor(COLOR_BACKGROUND, GREEN);
    else
      display.setTextColor(COLOR_BACKGROUND, RED);
    display.print(F("TOUCH"));
    display.setTextColor(GREY1);
#endif

    ///////////////
    if (seq.running == false) {
      oldSettings.pool = configuration.dexed[0].pool;
      oldSettings.bank = configuration.dexed[0].bank;
      oldSettings.voice = configuration.dexed[0].voice;
      oldSettings.transpose = configuration.dexed[0].transpose;
      load_sd_voice(0, 1, 21, 0);
      MicroDexed[0]->setGain(0.9);
      MicroDexed[0]->keydown(MIDI_G3, 40);
      sysinfo_chord_state = 1;
    }
    randomSeed(analogRead(0));
    if (random(2) == 0) {
      sysinfo_logo_version = 2;
      splash_screen2();
    }
    else {
      sysinfo_logo_version = 1;
      splash_screen1();
    }

    LCDML.FUNC_setLoopInterval(50); // 20Hz main loop refresh
    loopMs = 0;
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (loopMs % 250 == 0) { // 4Hz
      display.setTextSize(1);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.setCursor(CHAR_width_small * 38 - 2, CHAR_height_small * 25);
      print_formatted_number(AudioProcessorUsage(), 3);
      display.setCursor(CHAR_width_small * 48 - 2, CHAR_height_small * 25);
      print_formatted_number(tempmonGetTemp(), 2);
    }

    if (sysinfo_logo_version == 1) {
      if (loopMs < 2000) {
        splash_draw_X((loopMs & 0b100) != 0);
      }
    }
    if (sysinfo_logo_version == 2) {
      draw_logo2(0, loopMs / 50);
    }

    if (seq.running == false) {
      switch (sysinfo_chord_state) {
      case 1:
        if (loopMs >= 200) {
          MicroDexed[0]->keydown(MIDI_D4, 55);
          sysinfo_chord_state++;
        }
        break;
      case 2:
        if (loopMs >= 400) {
          MicroDexed[0]->keydown(MIDI_F4, 60);
          MicroDexed[0]->keydown(MIDI_G4, 50);
          MicroDexed[0]->keydown(MIDI_AIS5, 50);
          MicroDexed[0]->keydown(MIDI_D5, 60);
          sysinfo_chord_state++;
        }
        break;
      case 3:
        if (loopMs >= 1100) {
          MicroDexed[0]->keyup(MIDI_G3);
          MicroDexed[0]->keyup(MIDI_D4);
          MicroDexed[0]->keyup(MIDI_F4);
          MicroDexed[0]->keyup(MIDI_G4);
          MicroDexed[0]->keyup(MIDI_AIS5);
          MicroDexed[0]->keyup(MIDI_D5);
          sysinfo_chord_state++;
        }
        break;
      case 4:
        if (loopMs >= 2800) {
          sysinfo_reload_prev_voice(oldSettings);
          sysinfo_chord_state = 0;
          if (configuration.sys.load_at_startup_page == 50 && sysinfo_page_at_bootup_shown_once == false) {
            sysinfo_page_at_bootup_shown_once = true;
            LCDML.MENU_goRoot();
          }
          else {
            helptext_l(back_text);
          }
        }
        break;
      }
    }

    loopMs += 50;
    if (loopMs == 20000) { // repeat animation every 20s
      loopMs = 0;
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterScope();
    sysinfo_reload_prev_voice(oldSettings);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

// FLASHMEM void print_mb_params()
// {
//   display.setTextSize(1);
//   setCursor_textGrid_small(12, 1);
//   if (generic_temp_select_menu == 0)
//   {
//     display.setTextColor(COLOR_BACKGROUND, GREEN);
//     if (multiband_active)
//       display.print(F("ACTIVE  "));
//     else
//     {
//       display.setTextColor(COLOR_BACKGROUND, RED);
//       display.print(F("INACTIVE"));
//     }
//   }
//   else
//   {
//     display.setTextColor(GREEN, COLOR_BACKGROUND);
//     if (multiband_active)
//       display.print(F("ACTIVE  "));
//     else
//     {
//       display.setTextColor(RED, COLOR_BACKGROUND);
//       display.print(F("INACTIVE"));
//     }
//   }
//   setCursor_textGrid_small(14, 2);
//   if (generic_temp_select_menu == 1)
//     display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
//   else
//     display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//   display.print(mb_global_gain);
//   display.print("dB");
//   display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//   display.print("  ");

//   setCursor_textGrid_small(20, 3);
//   if (generic_temp_select_menu == 2)
//     display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
//   else
//     display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//   display.print("1:");
//   display.print(mb_global_ratio);
//   display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
//   display.print(" ");
// }

// FLASHMEM void mb_set_mutes()
// {
//   if (mb_solo_low)
//   {
//     mb_mixer_l.gain(0, VOL_MAX_FLOAT + mb_global_gain + mb_gain_low);
//     mb_mixer_r.gain(0, VOL_MAX_FLOAT + mb_global_gain + mb_gain_low);
//   }
//   else
//   {
//     mb_mixer_l.gain(0, 0);
//     mb_mixer_r.gain(0, 0);
//   }
//   if (mb_solo_mid)
//   {
//     mb_mixer_l.gain(1, VOL_MAX_FLOAT + mb_global_gain + mb_gain_mid);
//     mb_mixer_r.gain(1, VOL_MAX_FLOAT + mb_global_gain + mb_gain_mid);
//   }
//   else
//   {
//     mb_mixer_l.gain(1, 0);
//     mb_mixer_r.gain(1, 0);
//   }
//   if (mb_solo_upper_mid)
//   {
//     mb_mixer_l.gain(2, VOL_MAX_FLOAT + mb_global_gain + mb_gain_upper_mid);
//     mb_mixer_r.gain(2, VOL_MAX_FLOAT + mb_global_gain + mb_gain_upper_mid);
//   }
//   else
//   {
//     mb_mixer_l.gain(2, 0);
//     mb_mixer_r.gain(2, 0);
//   }
//   if (mb_solo_high)
//   {
//     mb_mixer_l.gain(3, VOL_MAX_FLOAT + mb_global_gain + mb_gain_high);
//     mb_mixer_r.gain(3, VOL_MAX_FLOAT + mb_global_gain + mb_gain_high);
//   }
//   else
//   {
//     mb_mixer_l.gain(3, 0);
//     mb_mixer_r.gain(3, 0);
//   }
//   if (mb_solo_low == false && mb_solo_upper_mid == false && mb_solo_mid == false && mb_solo_high == false)
//   {
//     mb_mixer_l.gain(0, 1.0 + mb_global_gain + mb_gain_low);
//     mb_mixer_r.gain(0, 1.0 + mb_global_gain + mb_gain_low);
//     mb_mixer_l.gain(1, 1.0 + mb_global_gain + mb_gain_mid);
//     mb_mixer_r.gain(1, 1.0 + mb_global_gain + mb_gain_mid);
//     mb_mixer_l.gain(2, 1.0 + mb_global_gain + mb_gain_upper_mid);
//     mb_mixer_r.gain(2, 1.0 + mb_global_gain + mb_gain_upper_mid);
//     mb_mixer_l.gain(3, 1.0 + mb_global_gain + mb_gain_high);
//     mb_mixer_r.gain(3, 1.0 + mb_global_gain + mb_gain_high);
//   }
// }

FLASHMEM void mb_set_master()
{
  if (multiband_active)
  {
    finalized_mixer_l.gain(0, 0); // mute normal output
    finalized_mixer_r.gain(0, 0);
    finalized_mixer_l.gain(1, VOL_MAX_FLOAT);
    finalized_mixer_r.gain(1, VOL_MAX_FLOAT);
  }
  else // disable multiband output on finalized mixer
  {
    finalized_mixer_l.gain(0, VOL_MAX_FLOAT); // normal output, mute multiband
    finalized_mixer_r.gain(0, VOL_MAX_FLOAT);
    finalized_mixer_l.gain(1, 0);
    finalized_mixer_r.gain(1, 0);
  }
}

// FLASHMEM void mb_clear_caches()
// {
//   memset(ts.displayed_peak, 0, sizeof(ts.displayed_peak));
//   clear_volmeter(CHAR_width_small * 1, 228);
//   clear_volmeter(CHAR_width_small * 5, 228);
//   clear_volmeter(DISPLAY_WIDTH - CHAR_width_small * 8 + 2, 228);
//   clear_volmeter(DISPLAY_WIDTH - CHAR_width_small * 4 + 2, 228);
// }

extern AudioMultiBandCompressor MultiBandCompressor;
MultiBandCompressor_UI_t MultiBandUIparams;

// This is the main function that draws the entire crossover graphic.
// It uses a logarithmic scale for frequency mapping.
FLASHMEM void drawCrossoverFrequencies(const MultiBandCompressor_UI_t& p) {
  // Define the graphical area for the crossovers on the display.
  const int graph_y_pos = 2 * CHAR_height_small;
  const int graph_x_start = 0;
  const int graph_x_end = 320;

  // Clear the specific area where the graphic will be drawn.
  // This prevents drawing over old values.
  display.fillRect(graph_x_start, graph_y_pos - 5, graph_x_end, 30, COLOR_BACKGROUND);

  // Logarithmic scale mapping: log10(freq_hz) -> pixel_x.
  // Map the audible frequency range (20 Hz to 20000 Hz) to the screen width.
  const float min_freq_log = log10(20.0f);
  const float max_freq_log = log10(20000.0f);
  const float scale = (graph_x_end - graph_x_start) / (max_freq_log - min_freq_log);

  // Draw the main frequency axis line.
  display.drawLine(graph_x_start, graph_y_pos, graph_x_end, graph_y_pos, GREY2);

  // Helper lambda to convert a frequency in Hz to a pixel x-coordinate.
  auto freqToPixel = [&](float freq) {
    if (freq <= 0) return 0;
    return (int)((log10(freq) - min_freq_log) * scale) + graph_x_start;
    };

  // Helper lambda to draw a vertical line and label for a single crossover.
  auto drawCrossoverLine = [&](float freq, uint16_t color, const char* band_label) {
    int x_px = freqToPixel(freq);
    display.drawLine(x_px, graph_y_pos - 5, x_px, graph_y_pos + 5, color);

    // Format the frequency string for display (e.g., "1.5KHz" or "500Hz").
    char freq_str[20];
    display.setTextColor(color);

    // Convert the frequency to a string using a more reliable function
    if (freq >= 1000.0f) {
      float k_freq = freq / 1000.0f;
      dtostrf(k_freq, 0, 1, freq_str); // Convert float to string with 1 decimal place
      strcat(freq_str, "KHz");
    }
    else if (freq > 0) {
      dtostrf(freq, 0, 0, freq_str); // Convert float to string with 0 decimal places
      strcat(freq_str, "Hz");
    }
    else {
      strcpy(freq_str, "0Hz");
    }

    // Position and print the frequency label
    display.setCursor(x_px - (strlen(freq_str) * CHAR_width_small / 2), graph_y_pos + 8);
    display.print(freq_str);

    // Position and print the band label
    display.setCursor(x_px - (strlen(band_label) * CHAR_width_small / 2), graph_y_pos + 18);
    display.print(band_label);
    };

  // Retrieve the actual calculated frequencies from the global compressor instance.
  float crossover1_freq = MultiBandCompressor.getCrossover1Hz();
  float crossover2_freq = MultiBandCompressor.getCrossover2Hz();
  float crossover3_freq = MultiBandCompressor.getCrossover3Hz();

  // Draw the three crossover lines and their labels.
  drawCrossoverLine(crossover1_freq, COLOR_DRUMS, "BAND 1/2");
  drawCrossoverLine(crossover2_freq, COLOR_CHORDS, "BAND 2/3");
  drawCrossoverLine(crossover3_freq, COLOR_PITCHSMP, "BAND 3/4");
}

FLASHMEM void drawSoloButton(int x, int y, bool is_soloed) {
  int x_px = x * 6 - 4;
  int y_px = y * 10 - 5;
  int width_px = 19 * CHAR_width_small - 4;
  int height_px = 8 * CHAR_height_small + 2;

  // Draw the button border
  display.drawRect(x_px, y_px, width_px, height_px, is_soloed ? RED : GREY3);
  display.setTextColor(COLOR_SYSTEXT);
}

// ===================== UI parameter builders =============================
FLASHMEM void addMultiBandParameterEditors(MultiBandCompressor_UI_t& p) {
  // Clear parameter area but preserve header and controls
  display.console = true;
  display.fillRect(0, 70, 270, 130, COLOR_BACKGROUND);

  auto header = [](int x, int y, const char* t) {
    setCursor_textGrid_small(x, y);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print(t);
    };

  // Master Controls
  header(11, 5, "MASTER");
  ui.setCursor(11, 6);
  addAutoUpdateParameterEditor_uint8_t("GAIN", 0, 100, &p.master_gain);
  addAutoUpdateParameterEditor_uint8_t("MIX", 0, 100, &p.master_mix);

  // Crossover Frequencies
  header(28, 5, "CROSSOVERS");
  ui.setCursor(28, 6);
  addAutoUpdateParameterEditor_uint8_t("LOW-MID", 1, 100, &p.crossover1);
  addAutoUpdateParameterEditor_uint8_t("MID-HIGH", 1, 100, &p.crossover2);
  addAutoUpdateParameterEditor_uint8_t("HIGH-TOP", 1, 100, &p.crossover3);

  // Band 1 (Low)
  header(8, 10, "BAND 1 (LOW)");
  ui.setCursor(8, 11);
  addAutoUpdateParameterEditor_uint8_t("THRSH", 0, 100, &p.band1_thresh);
  addAutoUpdateParameterEditor_uint8_t("RATIO", 0, 100, &p.band1_ratio);
  addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 100, &p.band1_attack);
  addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.band1_release);
  addAutoUpdateParameterEditor_uint8_t("GAIN", 0, 100, &p.band1_gain);


  // Band 2 (Low-Mid)
  header(28, 10, "BAND 2 (LOW-MID)");
  ui.setCursor(28, 11);
  addAutoUpdateParameterEditor_uint8_t("THRSH", 0, 100, &p.band2_thresh);
  addAutoUpdateParameterEditor_uint8_t("RATIO", 0, 100, &p.band2_ratio);
  addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 100, &p.band2_attack);
  addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.band2_release);
  addAutoUpdateParameterEditor_uint8_t("GAIN", 0, 100, &p.band2_gain);


  // Band 3 (Mid-High)
  header(8, 17, "BAND 3 (MID-HIGH)");
  ui.setCursor(8, 18);
  addAutoUpdateParameterEditor_uint8_t("THRSH", 0, 100, &p.band3_thresh);
  addAutoUpdateParameterEditor_uint8_t("RATIO", 0, 100, &p.band3_ratio);
  addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 100, &p.band3_attack);
  addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.band3_release);
  addAutoUpdateParameterEditor_uint8_t("GAIN", 0, 100, &p.band3_gain);

  // Band 4 (High)
  header(28, 17, "BAND 4 (HIGH)");
  ui.setCursor(28, 18);
  addAutoUpdateParameterEditor_uint8_t("THRSH", 0, 100, &p.band4_thresh);
  addAutoUpdateParameterEditor_uint8_t("RATIO", 0, 100, &p.band4_ratio);
  addAutoUpdateParameterEditor_uint8_t("ATTACK", 0, 100, &p.band4_attack);
  addAutoUpdateParameterEditor_uint8_t("RELEASE", 0, 100, &p.band4_release);
  addAutoUpdateParameterEditor_uint8_t("GAIN", 0, 100, &p.band4_gain);

  // Add Solo button for Band 1, checking bit 0 of solo_mask
  drawSoloButton(8, 10, p.solo_mask & (1 << 0));
  // Add Solo button for Band 2, checking bit 1 of solo_mask
  drawSoloButton(28, 10, p.solo_mask & (1 << 1));
  // Add Solo button for Band 3, checking bit 2 of solo_mask
  drawSoloButton(8, 17, p.solo_mask & (1 << 2));
  // Add Solo button for Band 4, checking bit 3 of solo_mask
  drawSoloButton(28, 17, p.solo_mask & (1 << 3));
}

FLASHMEM void UI_func_multiband_dynamics(uint8_t param)
{
  // ****** SETUP: runs once when the UI page is entered *********
  if (LCDML.FUNC_setup())
  {
    registerTouchHandler(handle_touchscreen_multiband);
    //     registerScope(188, 0, 128, 64);
    ui.reset();
    ui.clear();

    setCursor_textGrid(0, 0);
    display.setTextSize(1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print("MULTIBAND COMPRESSOR");
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    addMultiBandParameterEditors(MultiBandUIparams);
    MultiBandCompressor.setUI(MultiBandUIparams);  // initial setup
    drawCrossoverFrequencies(MultiBandUIparams); // Draw the graphic once on startup
    TouchButton::drawButton(GRID.X[0], GRID.Y[1] + 7, "MULTI", "BAND", multiband_active ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
  }

  // ****** LOOP: runs continuously while the page is active *********
  if (LCDML.FUNC_loop())
  {
    ui.handle_input();
    // Update compressor parameters continuously
    MultiBandCompressor.setUI(MultiBandUIparams);
    if (MultiBandCompressor.getAndClearRedrawFlag())
      drawCrossoverFrequencies(MultiBandUIparams);
  }

  // ****** CLOSE: runs once when the page is exited *********
  if (LCDML.FUNC_close())
  {
    ui.clear();
    unregisterTouchHandler();
    //     unregisterScope();
  }
}

uint32_t totalBytesWritten;

FLASHMEM void encodeUint16(File& file, uint16_t value) {
  uint8_t bytes[] =
  {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>(value >> 8)
  };
  file.write(reinterpret_cast<const uint8_t*>(bytes), sizeof(bytes));
}

FLASHMEM void encodeUint32(File& file, uint32_t value) {
  uint8_t bytes[] =
  {
      static_cast<uint8_t> (value & 0x000000FF),
      static_cast<uint8_t>((value & 0x0000FF00) >> 8),
      static_cast<uint8_t>((value & 0x00FF0000) >> 16),
      static_cast<uint8_t>((value & 0xFF000000) >> 24),
  };
  file.write(reinterpret_cast<const uint8_t*>(bytes), sizeof(bytes));
}

FLASHMEM void writeWavHeader(File file, unsigned int sampleRate, unsigned int channelCount) {
  // Write the main chunk ID
  static constexpr uint8_t mainChunkId[4] = { 'R', 'I', 'F', 'F' };
  file.write(mainChunkId, sizeof(mainChunkId));

  // Write the main chunk header
  uint32_t mainChunkSize = 0; // placeholder, will be written on closing
  encodeUint32(file, mainChunkSize);
  static constexpr uint8_t mainChunkFormat[4] = { 'W', 'A', 'V', 'E' };
  file.write(mainChunkFormat, sizeof(mainChunkFormat));

  // Write the sub-chunk 1 ("format") id and size
  static constexpr uint8_t fmtChunkId[4] = { 'f', 'm', 't', ' ' };
  file.write(fmtChunkId, sizeof(fmtChunkId));
  uint32_t fmtChunkSize = 16;
  encodeUint32(file, fmtChunkSize);

  // Write the format (PCM)
  uint16_t format = 1;
  encodeUint16(file, format);

  // Write the sound attributes
  encodeUint16(file, static_cast<uint16_t>(channelCount));
  encodeUint32(file, static_cast<uint32_t>(sampleRate));
  uint32_t byteRate = sampleRate * channelCount * 2;
  encodeUint32(file, byteRate);
  uint16_t blockAlign = channelCount * 2;
  encodeUint16(file, blockAlign);
  uint16_t bitsPerSample = 16;
  encodeUint16(file, bitsPerSample);

  // Write the sub-chunk 2 ("data") id and size
  static constexpr uint8_t dataChunkId[4] = { 'd', 'a', 't', 'a' };
  file.write(dataChunkId, sizeof(dataChunkId));
  uint32_t dataChunkSize = 0; // placeholder, will be written on closing
  encodeUint32(file, dataChunkSize);

  totalBytesWritten += 44;
}

FLASHMEM void startWavRecording()
{
  display.setTextSize(2);

  char fullpath[CONFIG_FILENAME_LEN * 2];
  sprintf(fullpath, "/RECORDINGS/%s", filename);

  record_x_pos = 0;
  record_timer = 0;
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  setCursor_textGrid(11, 7);
  display.print(":");
  setCursor_textGrid(14, 7);
  display.print(":");
  setCursor_textGrid(17, 7);
  display.print("    ");

  if (SD.exists(fullpath))
  {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove(fullpath);
  }
  frec = SD.open(fullpath, FILE_WRITE);

  // Write WAV header, "CD quality" 44.1KHz stereo
  writeWavHeader(frec, 44100, 2);

  // start record
  helptext_r(F("PUSH TO STOP"));

  if (frec)
  {
    record_queue_l.begin();
    record_queue_r.begin();
    fm.wav_recorder_mode = 1;
  }
}

short writeWavStereoBlock() {
  byte buffer[512];
  byte bufferL[256];
  byte bufferR[256];
  memcpy(bufferL, record_queue_l.readBuffer(), 256);
  memcpy(bufferR, record_queue_r.readBuffer(), 256);
  record_queue_l.freeBuffer();
  record_queue_r.freeBuffer();
  int b = 0;
  for (int i = 0; i < 512; i += 4)
  {
    buffer[i] = bufferL[b];
    buffer[i + 1] = bufferL[b + 1];
    buffer[i + 2] = bufferR[b];
    buffer[i + 3] = bufferR[b + 1];
    b = b + 2;
  }
  frec.write(buffer, 512);
  totalBytesWritten += 512;

  return (bufferL[1] * 256) + bufferL[0];
}

// write all 512 bytes to the SD card
void continueWavRecording()
{
  static constexpr int PLOT_HALFHEIGHT = 40;
  static constexpr int PLOT_Y_CENTER = 185;

  static int16_t sampleValueMin = INT16_MAX;
  static int16_t sampleValueMax = INT16_MIN;

  static uint32_t lastPlotted = 0;

  if (record_queue_l.available() >= 2 && record_queue_r.available() >= 2)
  {
    const int16_t sampleValue = writeWavStereoBlock();
    sampleValueMin = std::min(sampleValueMin, sampleValue);
    sampleValueMax = std::max(sampleValueMax, sampleValue);

    if (record_timer - lastPlotted > 80)
    {
      lastPlotted = record_timer;
      const int16_t yFrom = constrain(sampleValueMin / 500, -PLOT_HALFHEIGHT, +PLOT_HALFHEIGHT);
      const int16_t yTo = constrain(sampleValueMax / 500, -PLOT_HALFHEIGHT, +PLOT_HALFHEIGHT);

      sampleValueMin = INT16_MAX;
      sampleValueMax = INT16_MIN;

      record_x_pos++;
      if (record_x_pos > DISPLAY_WIDTH - 1)
      {
        record_x_pos = 0;
      }

      display.drawLine(record_x_pos, PLOT_Y_CENTER - PLOT_HALFHEIGHT, record_x_pos, PLOT_Y_CENTER + PLOT_HALFHEIGHT, GREY4);
      display.drawLine(record_x_pos, PLOT_Y_CENTER + yFrom, record_x_pos, PLOT_Y_CENTER + yTo, GREEN);

      uint32_t seconds = record_timer / 1000, minutes, hours;
      minutes = seconds / 60;
      seconds %= 60;
      hours = minutes / 60;
      minutes %= 60;

      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid(9, 7);
      print_formatted_number(hours, 2);
      setCursor_textGrid(12, 7);
      print_formatted_number(minutes, 2);
      setCursor_textGrid(15, 7);
      print_formatted_number(seconds, 2);
    }
  }
}

FLASHMEM void stopWavRecording()
{
  char tmp[6];

  record_queue_l.end();
  record_queue_r.end();
  if (fm.wav_recorder_mode == 1)
  {
    // Flush all existing remaining data from the queues
    while (record_queue_l.available() > 0 && record_queue_r.available() > 0)
    {
      writeWavStereoBlock();
    }

    setCursor_textGrid(9, 7);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print("SAVED ");
    display.setTextColor(RED, COLOR_BACKGROUND);
    if (frec.size() / 1024 / 1024 > 0)
    {
      snprintf_P(tmp, sizeof(tmp), PSTR("%03d"), int(frec.size() / 1024 / 1024));
      display.print(tmp);
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.print(" MB  ");
    }
    else if (int(frec.size() / 1024) > 0)
    {
      snprintf_P(tmp, sizeof(tmp), PSTR("%03d"), int(frec.size() / 1024));
      display.print(tmp);
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.print(" KB  ");
    }

    // Update the WAV header main chunk size and data sub-chunk size
    uint32_t mainChunkSize = totalBytesWritten - 8;  // 8 bytes RIFF header
    uint32_t dataChunkSize = totalBytesWritten - 44; // 44 bytes RIFF + WAVE headers
    frec.seek(4);
    encodeUint32(frec, mainChunkSize);
    frec.seek(40);
    encodeUint32(frec, dataChunkSize);

    frec.close();
    fm.wav_recorder_mode = 0;
    // helptext_r ("PUSH TO RECORD");
    display.setTextColor(COLOR_SYSTEXT);
  }
}

FLASHMEM void UI_func_recorder(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    generic_active_function = 0;
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextColor(COLOR_SYSTEXT);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("AUDIO RECORDER"));
    setCursor_textGrid(1, 4);
    display.print(F("STATUS:"));
    setCursor_textGrid(1, 5);
    display.print(F("PATH:"));
    setCursor_textGrid(9, 5);
    display.print(F("/RECORDINGS"));
    setCursor_textGrid(1, 6);
    display.print(F("FILE:"));

    display.setTextColor(GREY2, COLOR_BACKGROUND);
    setCursor_textGrid(9, 7);
    display.print("00:00:00");
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      temp_int = constrain(temp_int + e.dir * e.speed, 0, 254);
    }
    if (e.pressed) {
      if (fm.wav_recorder_mode == 0) {
        startWavRecording();
      }
      else if (fm.wav_recorder_mode == 1) {
        stopWavRecording();
      }
    }

    helptext_r(F("+/- FILE, PUSH TO RECORD"));
    helptext_l(back_text);
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.setTextSize(2);

    setCursor_textGrid(9, 4);
    if (fm.wav_recorder_mode == 0)
      display.print(F("READY TO RECORD"));
    else
    {
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print(F("NOW RECORDING     "));
    }

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    snprintf_P(filename, sizeof(filename), PSTR("MDT_REC_%03d.wav"), temp_int);
    setCursor_textGrid(9, 6);
    display.print(filename);
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
    temp_int = 0;
  }
}

FLASHMEM void print_braids_static_texts()
{
  display.setTextSize(1);
  seq.edit_state = 0;

  setCursor_textGrid_small(1, 1);
  display.setTextColor(RED);
  display.print(F("BRAIDS"));

  setCursor_textGrid_small(30, 1);
  display.setTextColor(GREY1);
  display.print(F("SAVE"));
  setCursor_textGrid_small(37, 1);
  display.print(F("NAME"));

  display.setTextColor(GREY1);
  setCursor_textGrid_small(1, 3);
  display.print(F("VOLUME"));
  setCursor_textGrid_small(1, 4);
  display.print(F("SHAPE"));
  setCursor_textGrid_small(1, 5);
  display.print(F("COLOR"));
  setCursor_textGrid_small(1, 6);
  display.print(F("TIMBRE"));
  setCursor_textGrid_small(1, 7);
  display.print(F("COARSE"));
  setCursor_textGrid_small(1, 8);
  display.print(F("ATTACK"));
  setCursor_textGrid_small(1, 9);
  display.print(F("DECAY"));
  setCursor_textGrid_small(1, 10);
  display.print(F("SUSTAIN"));
  setCursor_textGrid_small(1, 11);
  display.print(F("RELEASE"));
  setCursor_textGrid_small(22, 3);
  display.print(F("FILTER"));
  setCursor_textGrid_small(22, 4);
  display.print(F("FREQ"));
  setCursor_textGrid_small(22, 5);
  display.print(F("RES"));
  setCursor_textGrid_small(32, 5);
  display.print(F("SPEED"));
  setCursor_textGrid_small(22, 6);
  display.print(F("LFO"));
  setCursor_textGrid_small(32, 6);
  display.print(F("L.SPD"));
  setCursor_textGrid_small(22, 7);
  display.print(F("REV. SEND"));
  setCursor_textGrid_small(22, 8);
  display.print(F("FLANGER"));
  setCursor_textGrid_small(39, 7);
  display.setTextColor(GREY2);
  display.print(F("<LR>"));
  display.setTextColor(GREY1);
  setCursor_textGrid_small(22, 9);
  display.print(F("DLY. SENDS"));
  setCursor_textGrid_small(22, 10);
  display.print(F("PANORAMA"));
  setCursor_textGrid_small(22, 11);
  display.print(F("MIDI CHN."));
  setCursor_textGrid_small(13, 7);
  display.print(F("STEPS"));
  setCursor_textGrid_small(16, 8);
  display.print(F("MS"));
  setCursor_textGrid_small(16, 9);
  display.print(F("MS"));
  setCursor_textGrid_small(16, 10);
  display.print(F("LEVL"));
  setCursor_textGrid_small(16, 11);
  display.print(F("MS"));
  setCursor_textGrid_small(45, 5);
  display.print(F("FILTERS"));

  // arrows
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  setCursor_textGrid_small(36, 4);
  display.print(F(">"));
}


uint8_t braids_presetno;
extern bool save_sd_braids_json(uint8_t number, uint8_t braidsmode);
extern bool load_sd_braids_json(uint8_t number, uint8_t braidsmode);
extern bool get_sd_braids_name(uint8_t number, uint8_t mode);

FLASHMEM void _print_braids_patchname(uint8_t number, uint8_t mode)
{
  if (check_sd_braids_patch_exists(number))
  {
    if (generic_temp_select_menu == 1 && seq.edit_state == 1)  //while in selection of destination patch 
    {
      get_sd_braids_name(number, 1);
      display.print(braids_osc.temp_name);
    }
    else
    {
      get_sd_braids_name(number, 0);
      display.print(braids_osc.name);
    }
  }
  else
  {
    if (number > 0)
      display.print(F("-----EMPTY-----"));
  }
}

FLASHMEM void update_selective_values_braids()
{
  display.setTextSize(1);

  if (menu_item_check(0))
  {
    setModeColor(0);
    setCursor_textGrid_small(9, 1);
    display.printf("P%02d", braids_presetno);

    setCursor_textGrid_small(13, 1);
    _print_braids_patchname(braids_presetno, 0);
  }

  if (menu_item_check(1))
  {
    setModeColor(1);
    setCursor_textGrid_small(30, 1);
    if (seq.edit_state == 0)
      display.print(F(" SAVE "));
    else
    {
      if (generic_temp_select_menu == 1)
      {
        display.print(F("SELECT"));
        display.setTextColor(COLOR_SYSTEXT, RED);
        setCursor_textGrid_small(9, 1);
        display.printf("P%02d", seq.cycle_touch_element);
        setCursor_textGrid_small(13, 1);
        _print_braids_patchname(seq.cycle_touch_element, 1);
      }
    }
  }
  if (menu_item_check(2))
  {
    setModeColor(2);
    setCursor_textGrid_small(37, 1);
    display.print(F("NAME"));
  }

  if (menu_item_check(3))
  {
    setModeColor(3);
    print_small_intbar(9, 3, braids_osc.sound_intensity, 3, 1, 0);
  }

  if (menu_item_check(4))
  {
    setModeColor(4);
    setCursor_textGrid_small(9, 4);
    print_formatted_number(braids_osc.algo, 2, 4, 1);
    setCursor_textGrid_small(13, 4);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(synthBraids[0]->get_name(braids_osc.algo));
    // braids_print (synthBraids[0]->get_name(braids_osc.algo)[i],i);
  }
  if (menu_item_check(5))
  {
    setModeColor(5);
    setCursor_textGrid_small(9, 5);
    print_formatted_number(braids_osc.color, 3, 5, 1);
  }
  if (menu_item_check(6))
  {
    setModeColor(6);
    setCursor_textGrid_small(9, 6);
    print_formatted_number(braids_osc.timbre, 3, 6, 1);
  }
  if (menu_item_check(7))
  {
    setModeColor(7);
    setCursor_textGrid_small(9, 7);
    print_formatted_number_signed(braids_osc.coarse, 2);
  }
  if (menu_item_check(8))
  {
    setModeColor(8);
    setCursor_textGrid_small(10, 8);
    print_formatted_number(braids_osc.env_attack * 4, 4, 8, 1);
  }
  if (menu_item_check(9))
  {
    setModeColor(9);
    setCursor_textGrid_small(10, 9);
    print_formatted_number(braids_osc.env_decay * 4, 4, 9, 1);
  }
  if (menu_item_check(10))
  {
    setModeColor(10);
    setCursor_textGrid_small(11, 10);
    print_formatted_number(braids_osc.env_sustain * 2, 3, 10, 1);
  }
  if (menu_item_check(11))
  {
    setModeColor(11);
    setCursor_textGrid_small(10, 11);
    print_formatted_number(braids_osc.env_release * braids_osc.env_release, 4, 11, 1);
  }
  if (menu_item_check(12))
  {
    setModeColor(12);
    setCursor_textGrid_small(32, 3);
    if (braids_osc.filter_mode == 0)
      display.print(F("OFF   "));
    else if (braids_osc.filter_mode == 1)
      display.print(F("LP12dB"));
    else if (braids_osc.filter_mode == 2)
      display.print(F("BP12dB"));
    else if (braids_osc.filter_mode == 3)
      display.print(F("HI12dB"));
  }
  if (menu_item_check(13))
    print_small_intbar(32, 4, braids_osc.filter_freq_from / 100, 13, 0, 1);
  if (menu_item_check(14))
    print_small_intbar(38, 4, braids_osc.filter_freq_to / 100, 14, 0, 1);
  if (menu_item_check(15))
    print_small_intbar(27, 5, braids_osc.filter_resonance, 15, 0, 1);
  if (menu_item_check(16))
    print_small_intbar(38, 5, braids_osc.filter_speed / 10, 16, 0, 1);
  if (menu_item_check(17))
    print_small_intbar(27, 6, braids_osc.filter_lfo_intensity / 100, 17, 0, 1);
  if (menu_item_check(18))
    print_small_intbar(38, 6, braids_osc.filter_lfo_speed, 18, 0, 1);
  if (menu_item_check(19))
  {
    setCursor_textGrid_small(33, 7);
    setModeColor(19);
    print_formatted_number(braids_osc.rev_send, 3, 19, 1);
  }
  if (menu_item_check(20))
  {
    setCursor_textGrid_small(33, 8);
    setModeColor(20);
    print_formatted_number(braids_osc.flanger, 3, 20, 1);
  }
  if (menu_item_check(21))
  {
    setModeColor(21);
    setCursor_textGrid_small(38, 8);
    print_formatted_number(braids_osc.flanger_spread, 3, 21, 1);
  }
  if (menu_item_check(22))
  {
    setModeColor(22);
    setCursor_textGrid_small(33, 9);
    print_formatted_number(braids_osc.delay_send_1, 3, 22, 1);
  }
  if (menu_item_check(23))
  {
    setCursor_textGrid_small(38, 9);
    setModeColor(23);
    print_formatted_number(braids_osc.delay_send_2, 3, 23, 1);
  }
  if (menu_item_check(24))
  {
    setModeColor(24);
    print_small_panbar(33, 10, braids_osc.pan, 24);
  }
  if (menu_item_check(25))
  {
    setModeColor(25);
    setCursor_textGrid_small(33, 11);
    _print_midi_channel(braids_osc.midi_channel);
#if defined GLOW
    set_glow_show_text_no_grid(33 * CHAR_width_small, 11 * (CHAR_height_small + 2), 4, get_midi_channel_name(braids_osc.midi_channel), 25, 1);
#endif
  }
}

extern void handle_generic_virtual_typewriter_touch();


FLASHMEM void print_OK()
{
  display.setTextSize(2);
  setCursor_textGrid(1, 2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("OK."));
  print_empty_spaces(24, true);
  display.console = true;
  display.fillRect(0, CHAR_height * 3, 310, 4, COLOR_BACKGROUND);
}

FLASHMEM void save_braids_preset()
{
  strcpy(braids_osc.name, edit_string_global);
  print_OK();
  save_sd_braids_json(braids_presetno, 1);
  delay(MESSAGE_WAIT_TIME);
  LCDML.FUNC_goBackToMenu();
}


uint8_t sysex_pool_number;
uint8_t sysex_bank_number;

FLASHMEM void virtual_typewriter_waitforsysex()
{
  display.setTextSize(2);
  setCursor_textGrid(1, 2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  char tmp[18];
  strcpy(tmp, edit_string_global);
  //sprintf(receive_bank_filename, "/%s/%d/%d/%s.syx", DEXED_CONFIG_PATH, sysex_pool_number, sysex_bank_number, tmp);
 // snprintf(receive_bank_filename, "%s.syx", tmp);
  snprintf(receive_bank_filename, sizeof(receive_bank_filename), "%s.syx", tmp);
  virtual_typewriter_active = false;
  setCursor_textGrid(1, 2);
  display.print(F("Waiting...         "));
  virtual_typewriter_active = false;
  /// Storing is done in SYSEX code
}

FLASHMEM void update_performance_name()
{
  strcpy(seq.name, edit_string_global);
  print_OK();
  delay(MESSAGE_WAIT_TIME);
  LCDML.FUNC_goBackToMenu();
}


FLASHMEM void update_multisample_name()
{
  strcpy(msp[seq.active_multisample].name, edit_string_global);
  print_OK();

  delay(MESSAGE_WAIT_TIME);
  LCDML.FUNC_goBackToMenu();
}

FLASHMEM void UI_func_set_braids_name(uint8_t param)
{
  static uint8_t mode;
  static uint8_t ui_select_name_state;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    registerTouchHandler(handle_generic_virtual_typewriter_touch);
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextSize(2);
    mode = 0;
    setCursor_textGrid(1, 1);
    display.print(F("Braids Preset Name"));
    helptext_r(F("SELECT PRESET SLOT WITH ENC[R]"));

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {

    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (mode == 0) {
        braids_presetno = constrain(braids_presetno + e.dir, 0, 99);
      }
      else if (mode == 2) {
        ui_select_name_state = UI_select_name(2, 4, BRAIDS_NAME_LEN, false);
      }
    }
    else if (e.pressed) {
      if (mode == 2)
      {
        ui_select_name_state = UI_select_name(2, 4, BRAIDS_NAME_LEN, false);
        if (ui_select_name_state)
        {
          mode = 0xff;
          save_braids_preset();
        }

      }
      if (mode == 0) {
        mode = 1;
      }
    }

    if (mode == 0)
    {
      setCursor_textGrid(1, 2);
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      print_formatted_number(braids_presetno, 2);
      setCursor_textGrid(3, 2);
      display.print(F("["));
      setCursor_textGrid(19, 2);
      display.print(F("]"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      if (check_sd_braids_patch_exists(braids_presetno))
      {
        load_sd_braids_json(braids_presetno, 1);
        show(2, 4, BRAIDS_NAME_LEN, braids_osc.name);
      }
      else
      {
        show(2, 4, BRAIDS_NAME_LEN, "-----EMPTY-----");
        strcpy(braids_osc.name, "MyNewPresetName");
      }
    }
    if (mode == 1)
    {
      mode = 2;
      strcpy(edit_string_global, braids_osc.name);

      setCursor_textGrid(3, 2);
      helptext_r(F("USE ENC[R] OR TOUCH TO EDIT"));
      ui_select_name_state = UI_select_name(2, 4, BRAIDS_NAME_LEN, true); //changed it to global input with combined touch
      setCursor_textGrid(24, 2);
      virtual_typewriter_init(0);
      virtual_typewriter_active = true;
    }

  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    unregisterTouchHandler();
    virtual_typewriter_active = false;
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_braids(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    static constexpr char BRAIDS[] = "BRAIDS";
    // Auto create missing folders if needed
    if (!SD.exists(BRAIDS)) {
      SD.mkdir(BRAIDS);
    }
    // setup function
    registerTouchHandler(handle_touchscreen_braids);
    registerScope(250, 0, 62, 50);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    virtual_keyboard_smart_preselect_mode();
    seq.cycle_touch_element = 1;
    drawVirtualKeyboard();

    print_braids_static_texts();
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver)) {
      generic_temp_select_menu = 0;
    }
    generic_full_draw_required = true;
    update_selective_values_braids();
    generic_full_draw_required = false;
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 25);
        if (generic_temp_select_menu == 1 && seq.edit_state == false)
          seq.cycle_touch_element = braids_presetno;
      }
      else {
        if (generic_temp_select_menu == 0) //select patch
        {
          braids_presetno = constrain(braids_presetno + e.dir * e.speed, 0, 99);

          if (seq.edit_state) // load preset 
          {
            if (check_sd_braids_patch_exists(braids_presetno))
            {
              load_sd_braids_json(braids_presetno, 1);
              generic_full_draw_required = true;
              update_selective_values_braids();
              generic_full_draw_required = false;
            }
          }
        }
        else if (generic_temp_select_menu == 1 && seq.edit_state) // save slot select
        {
          seq.cycle_touch_element = constrain(seq.cycle_touch_element + e.dir * e.speed, 1, 99);
          // load_sd_braids_json(seq.cycle_touch_element, 2);
          get_sd_braids_name(seq.cycle_touch_element, 1);
          generic_full_draw_required = true;
          update_selective_values_braids();
          generic_full_draw_required = false;
        }
        else  if (generic_temp_select_menu == 3)
          braids_osc.sound_intensity = constrain(braids_osc.sound_intensity + e.dir * e.speed, 0, 100);
        else if (generic_temp_select_menu == 4)
          braids_osc.algo = constrain(braids_osc.algo + e.dir, 0, 42);
        else if (generic_temp_select_menu == 5)
          braids_osc.color = constrain(braids_osc.color + e.dir * e.speed, 0, 255);
        else if (generic_temp_select_menu == 6)
          braids_osc.timbre = constrain(braids_osc.timbre + e.dir * e.speed, 0, 255);
        else if (generic_temp_select_menu == 7)
          braids_osc.coarse = constrain(braids_osc.coarse + e.dir, -36, 36);
        else if (generic_temp_select_menu == 8)
          braids_osc.env_attack = constrain(braids_osc.env_attack + e.dir, 0, 255);
        else if (generic_temp_select_menu == 9)
          braids_osc.env_decay = constrain(braids_osc.env_decay + e.dir, 0, 255);
        else if (generic_temp_select_menu == 10)
          braids_osc.env_sustain = constrain(braids_osc.env_sustain + e.dir, 0, 50);
        else if (generic_temp_select_menu == 11)
          braids_osc.env_release = constrain(braids_osc.env_release + e.dir, 0, 99);
        else if (generic_temp_select_menu == 12)
          braids_osc.filter_mode = constrain(braids_osc.filter_mode + e.dir, 0, 3);
        else if (generic_temp_select_menu == 13)
          braids_osc.filter_freq_from = constrain(braids_osc.filter_freq_from + e.dir * 80, 0, 15000);
        else if (generic_temp_select_menu == 14)
          braids_osc.filter_freq_to = constrain(braids_osc.filter_freq_to + e.dir * 80, 0, 15000);
        else if (generic_temp_select_menu == 15)
          braids_osc.filter_resonance = constrain(braids_osc.filter_resonance + e.dir, 0, 99);
        else if (generic_temp_select_menu == 16)
          braids_osc.filter_speed = constrain(braids_osc.filter_speed + e.dir * 5, 0, 999);
        else if (generic_temp_select_menu == 17)
          braids_osc.filter_lfo_intensity = constrain(braids_osc.filter_lfo_intensity + e.dir * 80, 0, 15000);
        else if (generic_temp_select_menu == 18)
          braids_osc.filter_lfo_speed = constrain(braids_osc.filter_lfo_speed + e.dir, 0, 255);
        else if (generic_temp_select_menu == 19)
          braids_osc.rev_send = constrain(braids_osc.rev_send + e.dir, 0, REVERB_SEND_MAX);
        else if (generic_temp_select_menu == 20)
          braids_osc.flanger = constrain(braids_osc.flanger + e.dir, 0, REVERB_SEND_MAX);
        else if (generic_temp_select_menu == 21)
          braids_osc.flanger_spread = constrain(braids_osc.flanger_spread + e.dir, 0, REVERB_SEND_MAX);
        else if (generic_temp_select_menu == 22)
          braids_osc.delay_send_1 = constrain(braids_osc.delay_send_1 + e.dir, 0, DELAY_LEVEL_MAX);
        else if (generic_temp_select_menu == 23)
          braids_osc.delay_send_2 = constrain(braids_osc.delay_send_2 + e.dir, 0, DELAY_LEVEL_MAX);
        else if (generic_temp_select_menu == 24)
          braids_osc.pan = constrain(braids_osc.pan + e.dir, PANORAMA_MIN, PANORAMA_MAX);
        else if (generic_temp_select_menu == 25)
          braids_osc.midi_channel = constrain(braids_osc.midi_channel + e.dir, 0, 17);
      }
    }

    if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    {
      if (generic_temp_select_menu == 1 && seq.edit_state) // save 
      {
        display.setTextSize(1);
        setCursor_textGrid_small(30, 1);
        if (check_sd_braids_patch_exists(seq.cycle_touch_element) == false && seq.cycle_touch_element == braids_presetno)
          strcpy(braids_osc.name, "MyNewPresetName");

        if (save_sd_braids_json(seq.cycle_touch_element, 1))
        {
          display.setTextColor(COLOR_SYSTEXT, DARKGREEN);
          display.print(F("SAVED!"));
          flush_sysex();
          delay(100);
          braids_presetno = seq.cycle_touch_element;
        }
        else
        {
          display.setTextColor(COLOR_SYSTEXT, RED);
          display.print(F("FAILED"));
          flush_sysex();
        }
        delay(500);
        seq.edit_state = 0;
        setCursor_textGrid_small(13, 1);
        _print_braids_patchname(braids_presetno, 1);
      }
      else if (generic_temp_select_menu == 2 && seq.edit_state == false) // edit name
      {
        LCDML.OTHER_jumpToFunc(UI_func_set_braids_name);
        seq.edit_state = 0;
      }
      else
        seq.edit_state = !seq.edit_state;
    }

    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    braids_update_single_setting();
    update_selective_values_braids();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    generic_active_function = 99;
    seq.cycle_touch_element = 0;
    encoderDir[ENC_R].reset();
    if (generic_temp_select_menu != 2)
      display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_cv_page(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    registerTouchHandler(handle_touchscreen_cv_screen);
    registerScope(250, 0, 62, 50);
    if (configuration.sys.cv_midi_channel == MIDI_CHANNEL_OMNI)
      configuration.sys.cv_midi_channel = 1;

    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    virtual_keyboard_smart_preselect_mode();
    seq.cycle_touch_element = 1;
    drawVirtualKeyboard();

    display.setTextSize(1);
    seq.edit_state = 0;
    generic_temp_select_menu = 0;

    setCursor_textGrid_small(1, 1);
    display.setTextColor(RED);
    display.print(F("MIDI/CV TRANSLATE PAGE"));

    setCursor_textGrid_small(30, 1);

    display.setTextColor(GREY1);
    setCursor_textGrid_small(1, 3);
    display.print(F("MIDI INPUT VALUE"));
    setCursor_textGrid_small(1, 4);
    display.print(F("MIDI INPUT NOTE"));
    setCursor_textGrid_small(1, 5);
    display.print(F("CV OUTPUT VALUE"));

    setCursor_textGrid_small(1, 6);
    display.print(F("VELOCITY"));
    setCursor_textGrid_small(1, 7);
    display.print(F("OUTPUT VELOCITY"));

    setCursor_textGrid_small(1, 8);
    display.print(F("MIDI CHANNEL"));
    setCursor_textGrid_small(1, 9);
    display.print(F("MIDI CC > CV2"));
    setCursor_textGrid_small(1, 10);
    display.print(F("MIDI CC > CV4"));
    setCursor_textGrid_small(1, 11);
    display.print(F("NOTE TRANSP.>CV1"));

    display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(19, 8);
    print_formatted_number(configuration.sys.cv_midi_channel, 2);
    setCursor_textGrid_small(19, 9);
    print_formatted_number(configuration.sys.dac_cv_2, 2);
    setCursor_textGrid_small(19, 10);
    print_formatted_number(configuration.sys.dac_cv_4, 2);
    setCursor_textGrid_small(19, 11);
    print_formatted_number(configuration.sys.dac_cv_transpose, 2);

    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) &&
      LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver)) {
      generic_temp_select_menu = 0;
    }
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == false)
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 3);
      else if (generic_temp_select_menu == 0)
      {
        configuration.sys.cv_midi_channel = constrain(configuration.sys.cv_midi_channel + (e.dir * e.speed), 1, 16);
        //if (seq.cycle_touch_element == 1 )
        ts.virtual_keyboard_midi_channel = configuration.sys.cv_midi_channel;
      }
      else if (generic_temp_select_menu == 1)
        configuration.sys.dac_cv_2 = constrain(configuration.sys.dac_cv_2 + (e.dir * e.speed), 0, 99);
      else if (generic_temp_select_menu == 2)
        configuration.sys.dac_cv_4 = constrain(configuration.sys.dac_cv_4 + (e.dir * e.speed), 0, 99);
      else if (generic_temp_select_menu == 3)
        configuration.sys.dac_cv_transpose = constrain(configuration.sys.dac_cv_transpose + (e.dir * e.speed), 0, 36);
    }
    if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    {
      seq.edit_state = !seq.edit_state;
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    display.setTextSize(1);
    setModeColor(0);
    setCursor_textGrid_small(19, 8);
    print_formatted_number(configuration.sys.cv_midi_channel, 2, 0, 1);
    setModeColor(1);
    setCursor_textGrid_small(19, 9);
    print_formatted_number(configuration.sys.dac_cv_2, 2, 1, 1);
    setModeColor(2);
    setCursor_textGrid_small(19, 10);
    print_formatted_number(configuration.sys.dac_cv_4, 2, 2, 1);
    setModeColor(3);
    setCursor_textGrid_small(19, 11);
    print_formatted_number(configuration.sys.dac_cv_transpose, 2, 3, 1);

  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_R].reset();
  }
}

extern bool chord_autostart;

FLASHMEM void  _print_arranger_labels()
{
  display.setTextSize(1);
  setCursor_textGrid_small(1, 1);
  display.setTextColor(RED);
  display.print(F("CHORD ARRANGER"));
  display.setTextColor(GREY2);
  display.print(F(" VERSION 0.2"));

  display.setTextSize(1);
  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    setCursor_textGrid_small(1 + (x * 5), 6);
    display.setTextColor(GREY1);

    display.print(x + 1);
    set_track_type_color(x);
    setCursor_textGrid_small(1 + (x * 5), 7);
    if (seq.track_type[x] == 0)
      display.print(F("DRM"));
    else if (seq.track_type[x] == 1)
      display.print(F("INS"));
    else if (seq.track_type[x] == 2)
      display.print(F("CHD"));
    else if (seq.track_type[x] == 3)
      display.print(F("ARP"));
    else if (seq.track_type[x] == 4)
      display.print(F("SLC"));
    setCursor_textGrid_small(1 + (x * 5), 8);

    if (seq.track_type[x] == 1)  // only instrument tracks
    {
      if (seq.arr_type[x] == 0)
      {
        display.setTextColor(GREY2);
        display.print(F("OFF "));
      }

      else if (seq.arr_type[x] == 1)
      {
        display.setTextColor(COLOR_SYSTEXT);
        display.print(F("ON  "));
      }
      else if (seq.arr_type[x] == 2)
      {
        display.setTextColor(COLOR_ARP);
        display.print(F("MELO"));
      }
      else if (seq.arr_type[x] == 3)
      {
        display.setTextColor(GREEN);
        display.print(F("BASS"));
      }
      else if (seq.arr_type[x] == 4)
      {
        display.setTextColor(COLOR_PITCHSMP);
        display.print(F("ROOT"));
      }
    }
  }
  display.setTextColor(COLOR_SYSTEXT);
  setCursor_textGrid_small(1, 10);
  display.print(F("ARRANGER SPLIT NOTE"));
  setCursor_textGrid_small(26, 10);
  display.print("-");

  display.setTextColor(GREY2);

  setCursor_textGrid_small(0, 15);
  display.print("DOMINANT");
  setCursor_textGrid_small(9, 15);
  display.print("SUBDOM");
  setCursor_textGrid_small(18, 15);
  display.print("RELATIVE");
  setCursor_textGrid_small(18 + 9, 15);
  display.print("PARALLEL");
  setCursor_textGrid_small(18 + 18, 15);
  display.print("DIATONIC");
  setCursor_textGrid_small(18 + 18 + 9, 15);
  display.print("CHROMA");

  for (int i = 0; i < 6; i++)
    TouchButton::drawButton(GRID.X[i], GRID.Y[4], "NONE", "", TouchButton::BUTTON_NORMAL);

  display.setTextSize(1);

  setCursor_textGrid_small(9, 21);
  display.setTextColor(COLOR_INSTR);
  display.print(F("INST"));

  display.setTextColor(COLOR_SYSTEXT);
  display.print(F(", "));

  display.setTextColor(COLOR_CHORDS);
  display.print(F("CHORD "));
  display.setTextColor(COLOR_SYSTEXT);
  display.print(F("AND "));
  display.setTextColor(COLOR_ARP);
  display.print(F("ARP "));

  display.setTextColor(GREY1);
  display.print(F("TRACKS ARE TRANSFORMED"));

  setCursor_textGrid_small(9, 22);
  display.setTextColor(COLOR_SYSTEXT);
  display.print(F("OFF:  "));
  display.setTextColor(GREY2);
  display.print(F("PLAY UNALTERED  "));
  display.setTextColor(COLOR_SYSTEXT);
  display.print(F("ON: "));
  display.setTextColor(GREY2);
  display.print(F("TRANSFORM TO CHORD"));


  setCursor_textGrid_small(9, 23);
  display.setTextColor(COLOR_SYSTEXT);
  display.print(F("BASS: "));
  display.setTextColor(GREY2);

  display.print(F("ADV.BASSLINE "));
  display.setTextColor(COLOR_SYSTEXT);
  setCursor_textGrid_small(29, 23);
  display.print(F("ROOT: "));
  display.setTextColor(GREY2);
  display.print(F("ROOT NOTES ONLY"));


  TouchButton::drawButton(GRID.X[5], GRID.Y[1] + 16, "AUTO", "START", chord_autostart ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);

}

FLASHMEM void  _print_arranger_values()
{
  display.setTextSize(1);
  for (uint8_t x = 0; x < NUM_SEQ_TRACKS; x++)
  {
    setCursor_textGrid_small(1 + (x * 5), 6);
    // display.setTextColor(COLOR_SYSTEXT);
    setCursor_textGrid_small(1 + (x * 5), 8);
    setModeColor(x);
    // if (seq.track_type[x] == 1 || seq.track_type[x] == 2 || seq.track_type[x] == 3 )  // only instrument and arp tracks
    // {
    if (seq.arr_type[x] == 0)
      display.print(F("OFF "));
    else if (seq.arr_type[x] == 1)
      display.print(F("ON  "));
    else if (seq.arr_type[x] == 2)
      display.print(F("MELO"));
    else if (seq.arr_type[x] == 3)
      display.print(F("BASS"));
    else if (seq.arr_type[x] == 4)
      display.print(F("ROOT"));
    //}
     // else
     // {
     //   // display.setTextColor(GREY2);
     //   display.print(F("   "));
     // }

  }
  setModeColor(8);
  print_arr_split_range(22, 10);
  display.console = true;
  if (seq.arr_split_note != 0)
  {
    display.fillRect(CHAR_width_small, 110, seq.arr_split_note * 3.55, 4, DARKGREEN);
    display.fillRect(CHAR_width_small + seq.arr_split_note * 3.55, 110, 320 - seq.arr_split_note * 2, 4, COLOR_BACKGROUND);
  }
  else
    display.fillRect(CHAR_width_small, 110, 320 - CHAR_width_small, 4, COLOR_BACKGROUND);

}

extern void print_sampler_keyboard(int x, int y);


extern uint8_t chord_input[];

FLASHMEM void UI_func_chord_arranger(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    registerTouchHandler(handle_touchscreen_chord_arranger);
    registerScope(250, 0, 62, 50);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);

    chord_input[0] = 0;
    chord_input[1] = 0;
    chord_input[2] = 0;
    chord_input[3] = 0;
    // virtual_keyboard_smart_preselect_mode();
    // seq.cycle_touch_element = 1;
    // drawVirtualKeyboard();

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(1);
    for (uint8_t oct = 0; oct < 8; oct++)
    {
      print_sampler_keyboard(oct * 7 + 1, 95);
      display.setCursor(CHAR_width_small + oct * 42, 137);
      display.print(oct);
    }

    seq.edit_state = 0;
    generic_menu = 0;


    _print_arranger_labels();

    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) &&
      LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
    {
      generic_temp_select_menu = 0;
    }

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0)
      {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 8);
      }
      else if (seq.edit_state == 1)
      {

        if (generic_temp_select_menu < 8)
        {
          if (e.down)
          {
            if (seq.arr_type[generic_temp_select_menu] < 4)
              seq.arr_type[generic_temp_select_menu]++;

          }
          else
          {
            if (seq.arr_type[generic_temp_select_menu] > 0)
              seq.arr_type[generic_temp_select_menu]--;
          }
        }
        else if (generic_temp_select_menu == 8)
        {
          if (e.down)
          {
            if (seq.arr_split_note < 72)
              seq.arr_split_note++;

          }

          else
          {
            if (seq.arr_split_note > 0)
              seq.arr_split_note--;
          }

        }
      }
    }
    _print_arranger_values();
    if (e.pressed) // handle button presses during menu
    {
      // if (seq.track_type[generic_temp_select_menu] == 1 || seq.track_type[generic_temp_select_menu] == 2 ||  seq.track_type[generic_temp_select_menu] == 3 ||  generic_temp_select_menu == 8)
      seq.edit_state = !seq.edit_state;

      _print_arranger_values();

    }

  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_R].reset();

  }
}


FLASHMEM void show_empty_folder_notice() {
  if (fm.sd_sum_files == 0)
  {
    uint8_t xoffset = (fm.active_window == 1) ? 27 : 0;

    display.setTextSize(1);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * (3 + xoffset), CHAR_height_small * 7);
    display.print("EMPTY FOLDER");
    fm.sd_selected_file = 0;      // Force highlight on ".."
    fm.sd_cap_rows = 0;           // No scrolling past ".."
  }

}


#ifdef  SECOND_SD
extern void  fill_up_with_spaces_right_window();

FLASHMEM void _print_file_and_folder_copy_info(uint8_t f)
{
  display.setTextSize(1);
  display.setCursor(CHAR_width_small, (CHAR_height_small * 23) + 5);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  if (fm.sd_mode == FM_COPY_TO_EXTERNAL && fm.SD_CARD_READER_EXT && f == fm.sd_selected_file)
  {
    display.print(F("COPY ["));
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    display.print(fm.sd_temp_name.c_str());
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("] TO EXTERNAL SD CARD"));
    // print_empty_spaces(9, 1);
    fill_up_with_spaces_right_window();
  }
  else if (fm.sd_mode == FM_COPY_TO_INTERNAL && fm.SD_CARD_READER_EXT && f == fm.sd_selected_file)
  {
    display.print(F("COPY ["));
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    display.print(fm.sd_temp_name.c_str());
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("] TO INTERNAL SD CARD"));
    //print_empty_spaces(9, 1);
    fill_up_with_spaces_right_window();
  }
  else if (fm.SD_CARD_READER_EXT && f == fm.sd_selected_file)
    // print_empty_spaces(40, 1);
    fill_up_with_spaces_right_window();
}
#endif


FLASHMEM void sd_printDirectory(bool forceReload)
{
  if (forceReload || fm.sd_new_name != fm.sd_prev_dir)
  {
#ifdef COMPILE_FOR_PSRAM
    if (fm.sd_mode == FM_COPY_TO_PSRAM)
    {
      fm.sd_prev_dir = "/CUSTOM/";
      fm.sd_new_name = "/CUSTOM/";
    }
#endif

    load_sd_directory();
  }

  uint8_t xoffset = 0;
#ifdef  SECOND_SD
  if (fm.SD_CARD_READER_EXT && fm.active_window == 1)
  {
    xoffset = 27;
  }
#endif

  fm.sd_is_folder = false;
  fm.sd_cap_rows = 10;

  // Draw the top line: ".." or "/"
  if (fm.sd_parent_folder && fm.sd_folder_depth > 0)
  {
    fm.sd_is_folder = true;
    fm.sd_temp_name = "..";
    display.setTextColor(COLOR_SYSTEXT, COLOR_PITCHSMP);
  }
  else
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);

  if (fm.sd_folder_depth > 0)
  {
    drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, 4 * 11 - 1, special_chars[23], 8, 8, YELLOW);
    display.setCursor(CHAR_width_small * 3 + xoffset * CHAR_width_small, 4 * 11);
    display.print("..");
  }
  else
  {
    drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, 4 * 11 - 1, special_chars[23], 8, 8, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 3 + xoffset * CHAR_width_small, 4 * 11);
    display.print("/ ");
  }

  // ======== Empty-folder handling (do this BEFORE the file-loop) ========
  // number of available entries after paging (defensive)
  uint16_t availableCount = 0;
  if (fm.sd_sum_files > fm.sd_skip_files) availableCount = fm.sd_sum_files - fm.sd_skip_files;
  else availableCount = 0;

  if (availableCount == 0)
  {
    // Force selection to top and disable scrolling/paging
    fm.sd_selected_file = 0;
    fm.sd_skip_files = 0;
    fm.sd_cap_rows = 0;

    // If we are in a subfolder, treat top as parent entry
    if (fm.sd_folder_depth > 0) {
      fm.sd_parent_folder = true;
      fm.sd_temp_name = "..";
      fm.sd_is_folder = true;
    }
    else {
      fm.sd_parent_folder = false; // at root there is no parent
      fm.sd_temp_name = "/";
      fm.sd_is_folder = false;
    }

    // clear the file-list area for this pane so no ghost entries remain
    display.console = true;
    display.fillRect(CHAR_width_small + xoffset * CHAR_width_small,
      5 * 11 - 1,
      CHAR_width_small * 26 - 6,
      11 * 11,
      COLOR_BACKGROUND);

    // Draw the top line with proper highlight if needed
    if (fm.sd_folder_depth > 0) {
      // show ".." highlighted so cursor is visibly on it
      drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, 4 * 11 - 1, special_chars[23], 8, 8, YELLOW);
      display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP); // inverted highlight
      display.setCursor(CHAR_width_small * 3 + xoffset * CHAR_width_small, 4 * 11);
      display.print("..");
    }
    else {
      drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, 4 * 11 - 1, special_chars[23], 8, 8, COLOR_BACKGROUND);
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      display.setCursor(CHAR_width_small * 3 + xoffset * CHAR_width_small, 4 * 11);
      display.print("/ ");
    }

    // Draw the "(empty folder)" notice (non-selectable)
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * (3 + xoffset), (5 + 1) * 11);
    display.print("EMPTY FOLDER");

    return; // skip the normal file list loop
  }
  // ======== end empty-folder handling ========


  // Normal file loop (safe indexing)
  uint8_t rows_drawn = 0;
  for (uint8_t f = 0; f < 11; f++)
  {
    uint16_t idx = fm.sd_skip_files + f;
    if (idx >= fm.sd_sum_files)
    {
      // no more files to draw, set cap_rows safely
      if (rows_drawn == 0) fm.sd_cap_rows = 0;
      else fm.sd_cap_rows = rows_drawn - 1;

      display.console = true;
      display.fillRect(CHAR_width_small + xoffset * CHAR_width_small, f * 11 + 5 * 11 - 1, CHAR_width_small * 26 - 6, (11 - f) * 11, COLOR_BACKGROUND);
      break;
    }

    storage_file_t sd_entry = sdcard_infos.files[idx];

    if (sd_entry.isDirectory)
    {
      drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, f * 11 - 1 + 5 * 11, special_chars[23], 8, 8, YELLOW);
      if (f == fm.sd_selected_file && fm.sd_parent_folder == false)
        display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
      else
        display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);

      show_smallfont_noGrid(f * 11 + 5 * 11, CHAR_width_small * 3 + xoffset * CHAR_width_small, 17, sd_entry.name.c_str());
      display.setCursor(CHAR_width_small * 20 + xoffset * CHAR_width_small, f * 11 + 5 * 11);
      display.setTextColor(DX_DARKCYAN, COLOR_BACKGROUND);
      display.print(" DIR  ");
    }
    else
    {
      drawBitmap(CHAR_width_small + xoffset * CHAR_width_small, f * 11 - 1 + 5 * 11, special_chars[23], 8, 8, COLOR_BACKGROUND);

      if (sd_entry.name.length()>FILENAME_MAX_LEN-1 && f == fm.sd_selected_file)
        display.setTextColor(COLOR_BACKGROUND,RED);
      else if (sd_entry.name.length()>FILENAME_MAX_LEN-1 )
        display.setTextColor(RED, COLOR_BACKGROUND);
      else if (f == fm.sd_selected_file && fm.sd_parent_folder == false)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      show_smallfont_noGrid(f * 11 + 5 * 11, CHAR_width_small * 3 + xoffset * CHAR_width_small, 16, sd_entry.name.c_str());
      display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
      display.setCursor(CHAR_width_small * 19 + xoffset * CHAR_width_small, f * 11 + 5 * 11);
      if (int(sd_entry.size / 1024) > 0)
      {
        display.printf("%4d", sd_entry.size / 1024);
        display.print(" KB");
      }
      else
      {
        display.printf("%4d", sd_entry.size);
        display.print(" B ");
      }
    }

    if (f == fm.sd_selected_file && fm.sd_parent_folder == false)
      fm.sd_temp_name = sd_entry.name;
    if (f == fm.sd_selected_file && sd_entry.isDirectory)
      fm.sd_is_folder = true;

    rows_drawn++;
  } // end for
}


#if defined COMPILE_FOR_PSRAM
FLASHMEM void psram_printCustomSamplesList()
{
  fm.psram_cap_rows = 9;

  for (uint8_t i = 6; i < 16; i++)
  {
    const int16_t slotIndex = (int16_t)(i - 6) + (int16_t)fm.psram_skip_files; // 0..N
    const bool isSelected   = (i == (fm.psram_selected_file + 6));
    const bool isFocused    = (fm.active_window == 1);
    const bool isCopyTarget = (slotIndex == fm_psram_copy_target);

    // --- Left index column ---
    if (isFocused && isSelected) {
      display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
    }
    else if (isCopyTarget) {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
    else {
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    }

    display.setCursor(CHAR_width_small * 29, i * 11);
    display.printf("%02d", i - 5 + fm.psram_skip_files);

    // --- Sample name ---
    if (isFocused && isSelected) {
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    }
    else if (isCopyTarget) {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
    else {
      if (drum_config[i + fm.psram_skip_files].len == 0 || drum_config[i + fm.psram_skip_files].len > 10000000)
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      else
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }

    show_smallfont_noGrid(i * 11, CHAR_width_small * 32, 12, drum_config[i + fm.psram_skip_files].name);

    // --- Size column (unchanged) ---
    display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 45, i * 11);

    if (int(drum_config[i + fm.psram_skip_files].len / 1024) > 0)
    {
      display.printf("%4d", ((drum_config[i + fm.psram_skip_files].len) / 1024) * 2);
      display.print(" KB");
    }
    else
    {
      display.printf("%4d", (drum_config[i + fm.psram_skip_files].len) * 2);
      display.print(" B ");
    }
  }
}
#endif

FLASHMEM void print_sampler_keyboard(int x, int y)
{
  uint8_t offset[5] = { 1, 2, 2, 4, 6 }; //+ is the offset to left
  int offcount = 0;
  display.console = true;
  // draw white keys
  for (uint8_t i = 0; i < 7; i++)
  {
    if (x * CHAR_width_small + 6 * i < DISPLAY_WIDTH - 8)
      display.fillRect(x * CHAR_width_small + 6 * i, y + 23, 5, 15, COLOR_SYSTEXT); // pianoroll white key
  }
  for (uint8_t i = 0; i < 12; i++)
  {
    if (seq.piano[i] == 1)
    {
      if (x * CHAR_width_small + 4 * i - offset[offcount] < DISPLAY_WIDTH - 10)
        display.fillRect(x * CHAR_width_small + 4 * i - offset[offcount], y + 23, 4, 8, COLOR_BACKGROUND); // BLACK key
      offcount++;
      if (offcount == 5)
        offcount = 0;
    }
  }
}

FLASHMEM void print_note_name_and_octave(uint8_t note)
{
  display.print(noteNames[note % 12][0]);
  if (noteNames[note % 12][1] != '\0')
  {
    display.print(noteNames[note % 12][1]);
  }
  display.print((note / 12) - 1);
  display.print(" ");
}

FLASHMEM uint8_t get_distance(uint8_t a, uint8_t b)
{
  if (a == b)
    return 0;
  else if (a > b)
    return a - b;
  else
    return b - a;
}

FLASHMEM void calc_low_high(uint8_t preset)
{
  uint8_t result = 200;
  uint8_t result_zone = 99;

  for (uint8_t zone = 0; zone < NUM_MULTISAMPLE_ZONES; zone++)
  {
    msz[preset][zone].low = 0;
    msz[preset][zone].high = 0;
  }
  for (uint8_t key = 24; key < 110; key++)
  {
    for (uint8_t zone = 0; zone < NUM_MULTISAMPLE_ZONES; zone++)
    {
      if (get_distance(msz[preset][zone].rootnote, key) < result)
      {
        result = get_distance(msz[preset][zone].rootnote, key);
        result_zone = zone;
      }
    }
    if (msz[preset][result_zone].rootnote > key && msz[preset][result_zone].rootnote != 0)
    {
      if (msz[preset][result_zone].low > msz[preset][result_zone].rootnote - result || msz[preset][result_zone].low == 0)
        msz[preset][result_zone].low = msz[preset][result_zone].rootnote - result;
    }
    else
    {
      if (msz[preset][result_zone].high < msz[preset][result_zone].rootnote + result && msz[preset][result_zone].rootnote != 0)
        msz[preset][result_zone].high = msz[preset][result_zone].rootnote + result;
    }

    result = 200;
    result_zone = 99;
  }
}

FLASHMEM uint16_t get_multisample_zone_color(uint8_t row)
{
  uint16_t temp_color = 0;
  if (row == 0)
    temp_color = COLOR_PITCHSMP;
  else if (row == 1)
    temp_color = DX_DARKCYAN;
  else if (row == 2)
    temp_color = COLOR_CHORDS;
  else if (row == 3)
    temp_color = COLOR_ARP;
  else if (row == 4)
    temp_color = COLOR_DRUMS;
  else if (row == 5)
    temp_color = GREEN;
  else if (row == 6)
    temp_color = MIDDLEGREEN;
  else if (row == 7)
    temp_color = YELLOW;
  return temp_color;
}

FLASHMEM void sub_MultiSample_setColor(uint8_t row, uint8_t column)
{
  uint16_t temp_color = 0;
  uint16_t temp_background = 0;

  if ((generic_temp_select_menu == row + 3 && seq.selected_track == column && seq.edit_state == false) ||
    (generic_temp_select_menu == row + 3 && column == 99 && seq.edit_state == false))
  {
    temp_color = COLOR_BACKGROUND;
    temp_background = COLOR_SYSTEXT;
  }
  else if ((generic_temp_select_menu == row + 3 && seq.selected_track == column && seq.edit_state))
  {
    temp_background = RED;
    temp_color = COLOR_SYSTEXT;
  }
  else
  {
    temp_background = COLOR_BACKGROUND;
    if (row == 0)
      temp_color = COLOR_PITCHSMP;
    else {
      temp_color = get_multisample_zone_color(row);
    }
  }
  display.setTextColor(temp_color, temp_background);
}

FLASHMEM void print_multisampler_panbar(uint8_t x, uint8_t y, uint8_t input_value, uint8_t selected_option)
{
  display.console = true;

  if (selected_option == generic_temp_select_menu - 3 && seq.selected_track == 5 && seq.edit_state)
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, RED);
  else if (selected_option == generic_temp_select_menu - 3 && seq.selected_track == 5)
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, COLOR_SYSTEXT);
  else
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, GREY2);

  display.console = true;
  display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1, 10 * y + 1, 3 * CHAR_width_small - 1, 7 - 2, COLOR_BACKGROUND);
  display.console = true;
  if (input_value == 20)
    display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + input_value / 2.83, 10 * y + 1, 3, 5, COLOR_SYSTEXT);
  else
    display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + input_value / 2.83, 10 * y + 1, 3, 5, COLOR_PITCHSMP);
  display.console = false;
}

FLASHMEM void print_multisampler_tunebar(uint8_t x, uint8_t y, uint8_t input_value, uint8_t selected_option)
{
  display.console = true;

  if (selected_option == generic_temp_select_menu - 3 && seq.selected_track == 7 && seq.edit_state)
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, RED);
  else if (selected_option == generic_temp_select_menu - 3 && seq.selected_track == 7)
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, COLOR_SYSTEXT);
  else
    display.drawRect(CHAR_width_small * x + 4 * CHAR_width_small, 10 * y, 3 * CHAR_width_small + 1, 7, GREY2);

  display.console = true;
  display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1, 10 * y + 1, 3 * CHAR_width_small - 1, 7 - 2, COLOR_BACKGROUND);
  display.console = true;
  if (input_value == 100)
    display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + input_value / 14, 10 * y + 1, 3, 5, COLOR_SYSTEXT);
  else
    display.fillRect(CHAR_width_small * x + 4 * CHAR_width_small + 1 + input_value / 14, 10 * y + 1, 3, 5, COLOR_PITCHSMP);
  display.console = false;
}

#if defined COMPILE_FOR_PSRAM

FLASHMEM void print_msp_zone(uint8_t zone, bool fullrefresh_values)
{
  uint8_t yoffset = 7;
  display.setTextSize(1);

  sub_MultiSample_setColor(zone, 0);
  setCursor_textGrid_small(2, zone + yoffset);
  print_note_name_and_octave(msz[seq.active_multisample][zone].rootnote);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(" ");

  setCursor_textGrid_small(7, zone + yoffset);
  sub_MultiSample_setColor(zone, 1);
  print_note_name_and_octave(msz[seq.active_multisample][zone].low);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(" ");

  setCursor_textGrid_small(11, zone + yoffset);
  sub_MultiSample_setColor(zone, 2);
  print_note_name_and_octave(msz[seq.active_multisample][zone].high);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(" ");
  char tmp[4];
  sub_MultiSample_setColor(zone, 3);

  setCursor_textGrid_small(16, zone + yoffset);
  if (msz[seq.active_multisample][zone].playmode != 0)
  {

    uint8_t offset_x = 0;
    if (msz[seq.active_multisample][zone].playmode == 1)
      offset_x = 1;
    else if (msz[seq.active_multisample][zone].playmode == 3)
      offset_x = 2;
    else if (msz[seq.active_multisample][zone].playmode == 5)
      offset_x = 3;
    else if (msz[seq.active_multisample][zone].playmode == 7)
      offset_x = 4;

    display.write(23);

    display.setCursor(display.getCursorX() - 5 + offset_x, display.getCursorY());
    display.write(24);
    if (offset_x < 4)
    {
      display.console = 1;
      if (seq.edit_state && seq.selected_track == 3)
        display.fillRect(display.getCursorX(), display.getCursorY(), 4 - offset_x, 8, RED);
      else
        display.fillRect(display.getCursorX(), display.getCursorY(), 4 - offset_x, 8, COLOR_BACKGROUND);
    }
  }
  else
  {
    display.write(21);
    display.setCursor(display.getCursorX() - 1, display.getCursorY());
    display.write(22);
  }
  sub_MultiSample_setColor(zone, 4);
  show_smallfont_noGrid((zone + yoffset) * (CHAR_height_small + 2), 19 * CHAR_width_small, 3, itoa(msz[seq.active_multisample][zone].vol, tmp, 10));
  sub_MultiSample_setColor(zone, 5);
  print_multisampler_panbar(19, yoffset + zone, msz[seq.active_multisample][zone].pan, zone);

  sub_MultiSample_setColor(zone, 6);
  show_smallfont_noGrid((zone + yoffset) * (CHAR_height_small + 2), 27 * CHAR_width_small, 3, itoa(msz[seq.active_multisample][zone].rev, tmp, 10));

  sub_MultiSample_setColor(zone, 7);
  print_multisampler_tunebar(27, yoffset + zone, msz[seq.active_multisample][zone].tune, zone);

  setCursor_textGrid_small(36, zone + yoffset);
  sub_MultiSample_setColor(zone, 8);
  display.print("[");
  if (msz[seq.active_multisample][generic_temp_select_menu - 3].entry_number == 0 && seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 8)
    show_smallfont_noGrid((zone + yoffset) * (CHAR_height_small + 2), 37 * CHAR_width_small, 15, "CLEAR ZONE ?");
  else
    show_smallfont_noGrid((zone + yoffset) * (CHAR_height_small + 2), 37 * CHAR_width_small, 15, msz[seq.active_multisample][zone].filename);
  setCursor_textGrid_small(51, zone + yoffset);
  display.print("]");
  display.console = true;
  if (msz[seq.active_multisample][zone].low == 0 && msz[seq.active_multisample][zone].high == 0)
    display.fillRect(1,
      185 + zone * 5,
      DISPLAY_WIDTH - 2, 5, COLOR_BACKGROUND);
  else
  {
    if (msz[seq.active_multisample][zone].low < 25)
      msz[seq.active_multisample][zone].low = 24;
    if (msz[seq.active_multisample][zone].high > 109)
      msz[seq.active_multisample][zone].high = 109;

    display.console = true;
    display.fillRect(1, 185 + zone * 5, 2 * CHAR_width_small + msz[seq.active_multisample][zone].low * 3.5 - (24 * 3.5) - 1, 5, COLOR_BACKGROUND);
    display.console = true;
    display.fillRect(2 * CHAR_width_small + msz[seq.active_multisample][zone].low * 3.5 - (24 * 3.5), 185 + zone * 5,
      (msz[seq.active_multisample][zone].high - msz[seq.active_multisample][zone].low) * 3.5 + 2.5 + 1, 5, get_multisample_zone_color(zone));
    display.console = true;
    display.fillRect(2 * CHAR_width_small + msz[seq.active_multisample][zone].high * 3.5 - (24 * 3.5) + 3.5 + 1, 185 + zone * 5,
      DISPLAY_WIDTH - (msz[seq.active_multisample][zone].high * 3.5) + (18 * 3.5), 5, COLOR_BACKGROUND);
    display.console = true;
    display.fillRect(2 * CHAR_width_small + msz[seq.active_multisample][zone].rootnote * 3.5 - (24 * 3.5) + 1, 185 + zone * 5 + 1,
      3.5 + 1, 5 - 2, COLOR_SYSTEXT);
  }
}
FLASHMEM void print_msp_all_zones()
{
  for (uint8_t zone = 0; zone < NUM_MULTISAMPLE_ZONES; zone++)
  {
    print_msp_zone(zone, true);
  }
}

FLASHMEM void UI_func_MultiSamplePlay(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    seq.edit_state = false;
    generic_temp_select_menu = 0;
    // calc_low_high(seq.active_multisample);
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    display.setTextSize(1);

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    for (uint8_t oct = 0; oct < 8; oct++)
    {
      print_sampler_keyboard(oct * 7 + 2, 135);
      display.setCursor(CHAR_width_small * 2 + oct * 42, 175);
      display.print(oct + 1);
    }
    display.setTextSize(1);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 4);
    display.print(F("Volume"));
    setCursor_textGrid_small(22, 4);
    display.print(F("MIDI Channel"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(2 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("ROOT"));
    display.setCursor(7 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("LOW"));
    display.setCursor(11 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("HIGH"));
    display.setCursor(16 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("PM"));
    display.setCursor(19 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("VOL"));
    display.setCursor(23 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("PAN"));
    display.setCursor(27 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("REV"));
    display.setCursor(31 * CHAR_width_small, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("TUNE"));
    display.setCursor(36 * CHAR_width_small + 2, (6) * (CHAR_height_small + 2) - 2);
    display.print(F("FILENAME"));

    print_msp_all_zones();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    multisample_zone_t& zoneEdited = msz[seq.active_multisample][generic_temp_select_menu - 3];
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state && generic_temp_select_menu == 0)
      {
        seq.active_multisample = constrain(seq.active_multisample + e.dir * e.speed, 0, NUM_MULTISAMPLES);
        if (seq.active_multisample < NUM_MULTISAMPLES)
        {
          calc_low_high(seq.active_multisample);
          print_msp_all_zones();
        }
        else
        {
          if (seq.active_multisample == NUM_MULTISAMPLES)
          {
            helptext_r(F("PUSH JUMPS TO RENAME MENU"));
            display.setTextSize(2);
            show(1, 1, 15, "JUMP TO RENAME?");
          }
        }
      }
      if (seq.edit_state && generic_temp_select_menu == 1)
      {
        msp[seq.active_multisample].sound_intensity = constrain(msp[seq.active_multisample].sound_intensity + e.dir * e.speed, 0, 100);
      }
      if (seq.edit_state && generic_temp_select_menu == 2)
      {
        msp[seq.active_multisample].midi_channel = constrain(msp[seq.active_multisample].midi_channel + e.dir, 0, 17);
      }

      if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 8) // file name selection
      {

#if  defined COMPILE_FOR_PSRAM
        zoneEdited.entry_number = constrain(zoneEdited.entry_number + e.dir, 0, NUM_DRUMSET_CONFIG - 1);
        if (seq.running == true)
          stop_all_drum_slots();

        if ((zoneEdited.entry_number) - 1 >= 0)
          fill_msz(drum_config[zoneEdited.entry_number - 1].filename, seq.active_multisample, generic_temp_select_menu - 3, zoneEdited.entry_number - 1);
#endif
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 7) // tune/pitch
      {
        zoneEdited.tune = constrain(zoneEdited.tune + e.dir * e.speed, 1, 200);
      }
      if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 6) // reverb send selection
      {
        zoneEdited.rev = constrain(zoneEdited.rev + e.dir * e.speed, 0, 100);
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 5) // pan selection
      {
        zoneEdited.pan = constrain(zoneEdited.pan + e.dir * e.speed, PANORAMA_MIN, PANORAMA_MAX);
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 4) // volume selection
      {
        zoneEdited.vol = constrain(zoneEdited.vol + e.dir * e.speed, 0, 100);
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 3) // playmode selection
      {
        // zoneEdited.playmode = !zoneEdited.playmode;
        zoneEdited.playmode = constrain(zoneEdited.playmode + e.dir, 0, 6);
        if (zoneEdited.playmode > 1 && zoneEdited.playmode % 2 == 0)
        {
          if (e.up)
            zoneEdited.playmode++;
          else if (e.down)
            zoneEdited.playmode--;
        }
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 2) // high selection
      {
        zoneEdited.high = constrain(zoneEdited.high + e.dir * e.speed, 24, 109);
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 1) // low selection
      {
        zoneEdited.low = constrain(zoneEdited.low + e.dir * e.speed, 24, 109);
      }
      else if (seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 0) // root note selection
      {
        zoneEdited.rootnote = constrain(zoneEdited.rootnote + e.dir * e.speed, 24, 109);
      }
      else if (seq.edit_state == false) // no option is selected, scroll parameter rows
      {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 11);
      }
    }

    if (LCDML.BT_checkEnter())
    {
      if (seq.edit_state && generic_temp_select_menu == 0 && seq.active_multisample == NUM_MULTISAMPLES)
      {
        seq.active_multisample = 0;
        LCDML.OTHER_jumpToFunc(UI_func_set_multisample_name);
      }
      else if (zoneEdited.entry_number == 0 && seq.edit_state && generic_temp_select_menu > 2 && seq.selected_track == 8)
      {
        // clear zone
        msz[seq.active_multisample][generic_temp_select_menu - 3].rootnote = 0;
        msz[seq.active_multisample][generic_temp_select_menu - 3].rev = 0;
        msz[seq.active_multisample][generic_temp_select_menu - 3].high = 0;
        msz[seq.active_multisample][generic_temp_select_menu - 3].low = 0;
        msz[seq.active_multisample][generic_temp_select_menu - 3].vol = 100;
        msz[seq.active_multisample][generic_temp_select_menu - 3].tune = 100;
        //msz[seq.active_multisample][generic_temp_select_menu - 3].psram_entry_number = NUM_DRUMSET_CONFIG-1;
        msz[seq.active_multisample][generic_temp_select_menu - 3].psram_entry_number = 0;
        memset(msz[seq.active_multisample][generic_temp_select_menu - 3].filename, 0, sizeof(msz[seq.active_multisample][generic_temp_select_menu - 3].filename));
      }
      if (seq.edit_state == false)
        seq.edit_state = true;
      else
        seq.edit_state = false;
    }
    if (generic_temp_select_menu > 2)
      helptext_l(F("MOVE X"));
    else
      helptext_l(back_text);

    if (generic_temp_select_menu > 2)
      helptext_r(F("MOVE Y"));
    else if (generic_temp_select_menu == 0 && seq.active_multisample != NUM_MULTISAMPLES)
      helptext_r(F("SEL. MULTISAMPLE"));
    else if (generic_temp_select_menu == 1)
      helptext_r(F("CHANGE VOLUME"));
    else if (generic_temp_select_menu == 2)
      helptext_r(F("SEL. MIDI CHANNEL"));
    display.setTextSize(2);

    if (seq.active_multisample < NUM_MULTISAMPLES)
    {
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid(1, 1);
      if (generic_temp_select_menu == 0)
        setModeColor(0);
      print_formatted_number(seq.active_multisample + 1, 2, 0, 2);
      setCursor_textGrid(3, 1);
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.print(F("["));
      setCursor_textGrid(15, 1);
      display.print(F("]"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      show(1, 4, 11, msp[seq.active_multisample].name);

      setModeColor(1);
      display.setTextSize(1);
      setCursor_textGrid_small(9, 4);
      print_small_intbar(9, 4, msp[seq.active_multisample].sound_intensity, 1, 1, 0);

      setModeColor(2);
      setCursor_textGrid_small(35, 4);
      _print_midi_channel(msp[seq.active_multisample].midi_channel);
#if defined GLOW
      set_glow_show_text_no_grid(35 * CHAR_width_small, 4 * (CHAR_height_small + 2), 4, get_midi_channel_name(msp[seq.active_multisample].midi_channel), 2, 1);
#endif

      if (seq.edit_state == false)
      {
        if (generic_temp_select_menu == 2)
        {
          print_msp_zone(0, false);
        }
        else if (generic_temp_select_menu == 3)
        {
          print_msp_zone(generic_temp_select_menu - 3, false);
          print_msp_zone(generic_temp_select_menu - 2, false);
        }
        else if (generic_temp_select_menu > 3 && generic_temp_select_menu < 10)
        {
          print_msp_zone(generic_temp_select_menu - 4, false);
          print_msp_zone(generic_temp_select_menu - 3, false);
          print_msp_zone(generic_temp_select_menu - 2, false);
        }
        else if (generic_temp_select_menu == 10)
        {
          print_msp_zone(generic_temp_select_menu - 4, false);
          print_msp_zone(generic_temp_select_menu - 3, false);
        }
      }
      else
      {
        if (generic_temp_select_menu > 2)
          print_msp_zone(generic_temp_select_menu - 3, false);
      }
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    // seq.scrollpos = 0;
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
}
#else
FLASHMEM void UI_func_MultiSamplePlay(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_l(back_text);
    setCursor_textGrid(1, 1);
    display.setTextSize(1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("NOT AVAILABLE IN THIS FIRMWARE"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ;
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}
#endif

#ifdef COMPILE_FOR_PSRAM
static drum_config_t previewSlot;
extern PitchSamplePlayer WAV_preview_PSRAM;
FLASHMEM void previewWavSlot(drum_config_t& slot) {
  if (slot.drum_data != NULL && slot.len > 0) {
    WAV_preview_PSRAM.playRaw((int16_t*)slot.drum_data, slot.len, slot.numChannels);
  }
}
#endif

FLASHMEM void previewWavFilemanager() {
#ifdef COMPILE_FOR_PSRAM
  bool isWav = (!fm.sd_is_folder && fm.sd_temp_name.endsWith(".wav"));

  // Internal SD, left window
  if (fm.active_window == 0 && isWav) {
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_ACTIVE);
    fm.sd_full_name = fm.sd_new_name + "/" + fm.sd_temp_name;
    sampleLoader.loadSampleToDrums(fm.sd_full_name, previewSlot, SD);
    if (previewSlot.drum_data != nullptr) previewWavSlot(previewSlot);
  }
  // PSRAM, right window (when using internal SD)
  else if (!fm.SD_CARD_READER_EXT && fm.active_window == 1) {
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_ACTIVE);
    previewWavSlot(drum_config[fm.psram_selected_file + 6 + fm.psram_skip_files]);
  }
#ifdef SECOND_SD
  // External SD, right window
  else if (fm.SD_CARD_READER_EXT && fm.active_window == 1 && isWav) {
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_ACTIVE);
    fm.sd_full_name = fm.sd_new_name + "/" + fm.sd_temp_name;
    sampleLoader.loadSampleToDrums(fm.sd_full_name, previewSlot, SD_EXTERNAL);
    if (previewSlot.drum_data != nullptr) previewWavSlot(previewSlot);
  }
#endif

  delay(100);
  TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_NORMAL);
#endif
}

#ifdef COMPILE_FOR_PSRAM

// Decide if a PSRAM custom slot is "free".
// We require: no audio loaded + filepath empty + name equals the default "CUSTOMxx".
static FLASHMEM bool fm_is_psram_slot_free(uint16_t customIdx)
{
  if (customIdx >= NUM_CUSTOM_SAMPLES) return false;

  const uint16_t si = customIdx + NUM_STATIC_PITCHED_SAMPLES;

  const bool noAudioLoaded = (drum_config[si].drum_data == nullptr) || (drum_config[si].len == 0);
  const bool noFilePath    = (customSamples[customIdx].filepath[0] == '\0');

  char expected[16];
  snprintf(expected, sizeof(expected), "CUSTOM%02d", (int)customIdx + 1);

  // Name must match default label OR be empty (some builds might leave it empty until first use)
  const bool nameIsDefault = (strncmp(drum_config[si].name, expected, sizeof(drum_config[si].name)) == 0)
                             || (drum_config[si].name[0] == '\0');

  return noAudioLoaded && noFilePath && nameIsDefault;
}

// Returns next free custom slot index [0..NUM_CUSTOM_SAMPLES-1] starting AFTER startIdx.
// Wraps around. Returns -1 if none is free.
static FLASHMEM int16_t fm_find_next_free_psram_slot(int16_t startIdx)
{
  if (NUM_CUSTOM_SAMPLES == 0) return -1;

  for (int16_t step = 1; step <= (int16_t)NUM_CUSTOM_SAMPLES; ++step)
  {
    const int16_t idx = (startIdx + step) % (int16_t)NUM_CUSTOM_SAMPLES;
    if (fm_is_psram_slot_free((uint16_t)idx))
      return idx;
  }
  return -1;
}

// Move selection (white bar) to an absolute custom slot index and keep it visible in the list.
static FLASHMEM void fm_select_psram_slot(uint16_t absCustomIndex)
{
  if (absCustomIndex >= NUM_CUSTOM_SAMPLES) return;

  if (absCustomIndex < (uint16_t)fm.psram_skip_files)
  {
    fm.psram_skip_files = absCustomIndex;
  }
  else if (absCustomIndex > (uint16_t)(fm.psram_skip_files + fm.psram_cap_rows))
  {
    fm.psram_skip_files = absCustomIndex - fm.psram_cap_rows;
  }

  fm.psram_selected_file = (int16_t)absCustomIndex - (int16_t)fm.psram_skip_files;

  // clamps
  const int16_t maxSkip = max(0, (int16_t)NUM_CUSTOM_SAMPLES - (int16_t)fm.psram_cap_rows - 1);
  fm.psram_skip_files = constrain(fm.psram_skip_files, 0, maxSkip);
  fm.psram_selected_file = constrain(fm.psram_selected_file, 0, fm.psram_cap_rows);
}

#endif



FLASHMEM void sd_card_count_files_from_directory(const char* dir_name)
{
  fm.sd_sum_files = 0;
  File dir = SD.open(dir_name);

  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      break;
    }
    //    if (entry.isDirectory()) {
    //      count++;
    //    } else {
    // }
    fm.sd_sum_files++;
    entry.close();
  }

  dir.close();
}

FLASHMEM void sd_go_parent_folder()
{
  if (fm.sd_folder_depth < 2)
  {
    fm.sd_folder_depth = 0;
    fm.sd_skip_files = 0;
    fm.sd_new_name = "/";
  }
  else
  {
    // path has at least one parent folder
    for (uint8_t count = fm.sd_new_name.length(); count > 0; count--)
    {
      if (fm.sd_new_name[count] == 0x2f)
      {
        fm.sd_new_name[count] = '\0';
        break;
      }
    }
    fm.sd_folder_depth--;
  }

  fm.sd_selected_file = 0;
}
FLASHMEM void sd_update_display()
{
  sd_printDirectory(true);

  display.setCursor(CHAR_width_small * 24, 1 * CHAR_height_small);
  display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
  print_formatted_number(fm.sd_sum_files, 3);
  show_smallfont_noGrid(3 * CHAR_height_small, CHAR_width_small * 7, 20, fm.sd_new_name.c_str());
  show_smallfont_noGrid(5 * CHAR_height_small, CHAR_width_small * 1, 20, fm.sd_temp_name.c_str());

}

FLASHMEM void _print_filemanager_header()
{
  display.setTextSize(1);

  display.setCursor(CHAR_width_small * 1, 1 * CHAR_height_small);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("SD CARD"));
  display.setCursor(CHAR_width_small * 9, 1 * CHAR_height_small);

  display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
  display.print(F("INTERN"));

  display.setTextColor(GREY1);

  display.setCursor(CHAR_width_small * 16, 1 * CHAR_height_small);
  display.print(F("FILES:"));
  display.setCursor(CHAR_width_small * 1, 2 * CHAR_height_small);
  display.print(F("TYPE:"));
  display.setTextColor(COLOR_PITCHSMP);
  display.print(sdcard_infos.type);
  display.setCursor(CHAR_width_small * 11, 2 * CHAR_height_small);
  uint32_t volumesize = volume.blocksPerCluster(); // clusters are collections of blocks
  volumesize *= volume.clusterCount();  // we'll have a lot of clusters
  if (volumesize < 0X800000)
  {
    volumesize *= 512;  // SD card blocks are always 512 bytes
    volumesize /= 1024;
  }
  else
  {
    volumesize /= 2;
  }
  display.setTextColor(GREY1);
  display.print(F("TOTAL: "));
  display.setTextColor(COLOR_PITCHSMP);
  volumesize /= 1024;
  display.print(volumesize);
  display.print(F(" MB"));
}



FLASHMEM void reset_file_manager_folder()
{
  fm.sd_new_name = "/";
  fm.sd_full_name = "/";
  fm.sd_prev_dir = "";
  fm.sd_temp_name = "/";
  fm.sd_skip_files = 0;
  fm.sd_selected_file = 0;
  fm.sd_is_folder = false;

}

#if defined COMPILE_FOR_PSRAM
FLASHMEM void print_psram_stats_filemanager()
{
  uint32_t total_data_size = 0;
  uint32_t psram_size = external_psram_size * 1048576;
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(CHAR_width_small * 29, 1 * CHAR_height_small);
  display.print(F("PSRAM"));
  display.setCursor(CHAR_width_small * 44, 1 * CHAR_height_small);
  if (psram_size != 0)
  {
    display.printf("%05d KB", (int)psram_size / 1024);
  }
  for (int i = 0; i < NUM_DRUMSET_CONFIG; i++) {
    total_data_size = total_data_size + drum_config[i].len;
  }
  total_data_size = psram_size - psram_free_bytes;
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.setCursor(CHAR_width_small * 29, 3 * CHAR_height_small);
  display.print(F("MEMORY USED/FREE:"));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(CHAR_width_small * 29, 5 * CHAR_height_small);
  display.printf("%05d KB/%05d KB  USED", (int)total_data_size / 1024, (int)(psram_size / 1024));
  display.setCursor(CHAR_width_small * 38, 6 * CHAR_height_small);
  display.printf("%05d KB  FREE", (int)(psram_size / 1024 - total_data_size / 1024));
  fm.psram_sum_files = NUM_CUSTOM_SAMPLES;
}




FLASHMEM void previewWavFile(String filename) {
  WAV_preview_PSRAM.stop();
  sampleLoader.loadSampleToDrums(filename, previewSlot, SD);
  if (previewSlot.drum_data != nullptr) {
    previewWavSlot(previewSlot);
  }
}
#endif

FLASHMEM void previewWavSliceEditor() {
#ifdef COMPILE_FOR_PSRAM
  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_slice_editor)) {
    if (fm.sample_source > 1 && fm.sample_source < 4) {// MSP
      previewWavFile(msz[fm.sample_source - 2][temp_int].filename);
    }
    else if (fm.sample_source == 4) {// PSRAM
      if (num_slices[0] == 0) {
        draw_button_on_grid(45, 23, "PLAY", "SAMPLE", 0);
        previewWavSlot(drum_config[temp_int]);
      }
      else {
        draw_button_on_grid(45, 23, "PLAY", "SLC", 0);
        if (generic_temp_select_menu < 6) {
          generic_temp_select_menu = 6;
        }
        seq.edit_state = 0;
        print_updated_slices_text();

        uint8_t sliceNumber = (generic_temp_select_menu - 6) / 2;
        const drum_config_s& sample = drum_config[selected_slice_sample[0]];

        WAV_preview_PSRAM.playRaw((int16_t*)sample.drum_data + slice_start[0][sliceNumber], slice_end[0][sliceNumber] - slice_start[0][sliceNumber], sample.numChannels);
      }
    }
  }
#endif
}


FLASHMEM void file_manager_clear_text_line()
{
  display.console = true;
  display.fillRect(CHAR_width_small * 1, CHAR_height_small * 23 + 5, (53 * CHAR_width_small) + 4, 8, COLOR_BACKGROUND);
}


// Consolidated error/info display function
FLASHMEM void showError(const char* message) {
  file_manager_clear_text_line();
  display.setCursor(CHAR_width_small * 1, 23 * CHAR_height_small + 5);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(message);
  //fill_up_with_spaces_left_window_filemanager();
}


#ifdef COMPILE_FOR_PSRAM 
#ifdef SECOND_SD
FLASHMEM void previewWavFileFromExternal(String filename) {
  WAV_preview_PSRAM.stop();
  sampleLoader.loadSampleToDrums(filename, previewSlot, SD_EXTERNAL); // Pass SD_EXTERNAL instance
  if (previewSlot.drum_data != nullptr) {
    previewWavSlot(previewSlot);
  }
}
#endif
#endif

FLASHMEM void _print_filemanager_sd_extern_header()
{
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(CHAR_width_small * 29, 1 * CHAR_height_small);
  display.print(F("SD CARD"));
  display.setCursor(CHAR_width_small * 37, 1 * CHAR_height_small);
  display.setTextColor(COLOR_SYSTEXT, RED);
  display.print(F("EXTERN"));

  // Add path label aligned with left side
  display.setCursor(CHAR_width_small * 29, 3 * CHAR_height_small);
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  display.print(F("PATH:"));
}

// Enhanced file progress display
FLASHMEM void showCopyProgress(const char* path, uint32_t copied, uint32_t total) {
  // file_manager_clear_text_line();

   // Display path (truncate if too long)
  display.setCursor(CHAR_width_small * 1, 23 * CHAR_height_small + 5);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  // Calculate available display width
  int maxWidth = (26 * CHAR_width_small) - CHAR_width_small;

  // Display truncated path if needed
  int pathLen = strlen(path);
  if (pathLen * CHAR_width_small > maxWidth) {
    int startPos = pathLen - (maxWidth / CHAR_width_small) + 3;
    display.print("...");
    display.print(path + startPos);

  }
  else {
    display.print(path);
  }
  fill_up_with_spaces_left_window_filemanager();
  // Progress bar
  if (total > 0) {
    int progressWidth = (26 * CHAR_width_small) * copied / total;
    display.fillRect(CHAR_width_small * 27, CHAR_height_small * 23 + 5,
      progressWidth, 8, RED);
  }
}


// File copy function with enhanced feedback
FLASHMEM bool copyFileWithProgress(File& source, File& dest, const char* path) {
  uint32_t length = source.size();
  uint32_t count = 0;
  char buf[256];

  while (count < length) {
    if (count % 8192 == 0 || count == length - 1) {
      showCopyProgress(path, count, length);
    }

    uint16_t toRead = min(sizeof(buf), length - count);
    uint16_t n = source.read(buf, toRead);
    if (n == 0) break;

    if (dest.write(buf, n) != n) {
      showError("Write failed");
      return false;
    }
    count += n;
  }

  display.fillRect(CHAR_width_small * 27, CHAR_height_small * 23 + 5,
    (26 * CHAR_width_small) + 4 + 1, 8, COLOR_BACKGROUND);
  return true;
}

#ifdef COMPILE_FOR_PSRAM
#ifdef SECOND_SD
FLASHMEM bool copyFolderRecursive(const char* sourcePath, const char* destPath, bool internalToExternal) {
  // Open source directory (correct SD card)
  File sourceDir = internalToExternal ? SD.open(sourcePath) : SD_EXTERNAL.open(sourcePath);
  if (!sourceDir || !sourceDir.isDirectory()) {
    showError("Invalid source dir");
    return false;
  }

  // Create destination directory (correct SD card)
  if (internalToExternal) {
    if (!SD_EXTERNAL.exists(destPath)) {
      if (!SD_EXTERNAL.mkdir(destPath)) {
        showError("Can't create dest dir");
        sourceDir.close();
        return false;
      }
    }
  }
  else {
    if (!SD.exists(destPath)) {
      if (!SD.mkdir(destPath)) {
        showError("Can't create dest dir");
        sourceDir.close();
        return false;
      }
    }
  }

  while (true) {
    File entry = sourceDir.openNextFile();
    if (!entry) break;

    const char* name = entry.name();
    // Skip hidden files and macOS metadata
    if (name[0] == '.' || strstr(name, "._") != nullptr) {
      entry.close();
      continue;
    }

    char sourceFull[CONFIG_FILENAME_LEN];
    char destFull[CONFIG_FILENAME_LEN];
    snprintf(sourceFull, sizeof(sourceFull), "%s/%s", sourcePath, name);
    snprintf(destFull, sizeof(destFull), "%s/%s", destPath, name);

    if (entry.isDirectory()) {
      if (!copyFolderRecursive(sourceFull, destFull, internalToExternal)) {
        entry.close();
        sourceDir.close();
        return false;
      }
    }
    else {
      // Remove existing file before copying
      if (internalToExternal) {
        if (SD_EXTERNAL.exists(destFull)) {
          if (!SD_EXTERNAL.remove(destFull)) {
            showError("Can't remove dest file");
            entry.close();
            sourceDir.close();
            return false;
          }
        }
      }
      else {
        if (SD.exists(destFull)) {
          if (!SD.remove(destFull)) {
            showError("Can't remove dest file");
            entry.close();
            sourceDir.close();
            return false;
          }
        }
      }

      // Open destination file (correct SD card)
      File destFile = internalToExternal ?
        SD_EXTERNAL.open(destFull, FILE_WRITE) :
        SD.open(destFull, FILE_WRITE);

      if (!destFile) {
        showError("Can't create file");
        entry.close();
        sourceDir.close();
        return false;
      }

      if (!copyFileWithProgress(entry, destFile, sourceFull)) {
        entry.close();
        destFile.close();
        sourceDir.close();
        return false;
      }
      destFile.close();
    }
    entry.close();
  }

  sourceDir.close();
  return true;
}
#endif
#endif


#ifdef COMPILE_FOR_PSRAM
#ifdef SECOND_SD
// Helper functions for explicit SD card access
FLASHMEM File openSourceFile(const char* path, bool internalToExternal) {
  return internalToExternal ? SD.open(path) : SD_EXTERNAL.open(path);
}

FLASHMEM File openDestFile(const char* path, bool internalToExternal, uint8_t mode = FILE_WRITE) {
  return internalToExternal ? SD_EXTERNAL.open(path, mode) : SD.open(path, mode);
}

FLASHMEM bool existsDest(const char* path, bool internalToExternal) {
  return internalToExternal ? SD_EXTERNAL.exists(path) : SD.exists(path);
}

FLASHMEM bool removeDest(const char* path, bool internalToExternal) {
  return internalToExternal ? SD_EXTERNAL.remove(path) : SD.remove(path);
}

FLASHMEM bool mkdirDest(const char* path, bool internalToExternal) {
  return internalToExternal ? SD_EXTERNAL.mkdir(path) : SD.mkdir(path);
}

#endif
#endif

#ifdef COMPILE_FOR_PSRAM
#ifdef SECOND_SD
// Unified copy function using explicit SD card accessors
FLASHMEM bool copyItem(const char* sourcePath, const char* destPath, bool internalToExternal) {
  File sourceItem = openSourceFile(sourcePath, internalToExternal);
  if (!sourceItem) {
    showError("Source not found");
    return false;
  }

  if (sourceItem.isDirectory()) {
    // Handle folder copy
    if (!copyFolderRecursive(sourcePath, destPath, internalToExternal)) {
      sourceItem.close();
      return false;
    }
  }
  else {
    // Handle file copy
    if (existsDest(destPath, !internalToExternal) && !removeDest(destPath, !internalToExternal)) {
      showError("Can't remove dest");
      sourceItem.close();
      return false;
    }

    File destFile = openDestFile(destPath, !internalToExternal);
    if (!destFile) {
      showError("Can't create dest");
      sourceItem.close();
      return false;
    }

    bool success = copyFileWithProgress(sourceItem, destFile, sourcePath);
    destFile.close();
    if (!success) {
      sourceItem.close();
      return false;
    }
  }
  sourceItem.close();
  return true;
}

// Main copy interface
FLASHMEM void file_manager_copy(bool internalToExternal) {
  // Build paths safely
  String sourcePath = String(fm.sd_new_name) + "/" + String(fm.sd_temp_name);
  String destPath = String(fm.sd_new_name) + "/" + String(fm.sd_temp_name);

  // Ensure parent directory exists on destination
  if (!existsDest(fm.sd_new_name.c_str(), !internalToExternal) &&
    !mkdirDest(fm.sd_new_name.c_str(), !internalToExternal)) {
    showError("Can't create dir");
    return;
  }

  if (!copyItem(sourcePath.c_str(), destPath.c_str(), internalToExternal)) {
    showError("Copy failed");
  }

  fm.sd_mode = 99; // Return to file manager
}
#endif
#endif

#endif

// Recursive folder deletion for internal SD
bool deleteFolderInternalSD(const String& path) {
  File dir = SD.open(path.c_str());  // Added .c_str() here
  if (!dir || !dir.isDirectory()) {
    showError("Not a directory");
    return false;
  }

  bool success = true;
  while (File entry = dir.openNextFile()) {
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      success &= deleteFolderInternalSD(entryPath);
    }
    else {
      if (!SD.remove(entryPath.c_str())) {
        showError("Failed to delete file");
        success = false;
      }
    }
    entry.close();
  }
  dir.close();
  if (!SD.rmdir(path.c_str())) {  // Added .c_str() here
    showError("Failed to remove dir");
    success = false;
  }
  return success;
}

#ifdef SECOND_SD
// Recursive folder deletion for external SD
bool deleteFolderExternalSD(const String& path) {
  File dir = SD_EXTERNAL.open(path.c_str());  // Added .c_str() here
  if (!dir || !dir.isDirectory()) {
    showError("Not a directory");
    return false;
  }

  bool success = true;
  while (File entry = dir.openNextFile()) {
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      success &= deleteFolderExternalSD(entryPath);
    }
    else {
      if (!SD_EXTERNAL.remove(entryPath.c_str())) {
        showError("Failed to delete file");
        success = false;
      }
    }
    entry.close();
  }
  dir.close();
  if (!SD_EXTERNAL.rmdir(path.c_str())) {  // Added .c_str() here
    showError("Failed to remove dir");
    success = false;
  }
  return success;
}
#endif

extern void print_file_manager_delete_button();
extern uint8_t previous_delete_item_type;
FLASHMEM void show_delete_info_psram()
{
  const uint8_t customSampleIndex = fm.psram_selected_file + fm.psram_skip_files;
  const uint8_t sampleIndex = customSampleIndex + NUM_STATIC_PITCHED_SAMPLES;
  if (drum_config[sampleIndex].drum_data != nullptr)
  {
    String confirmMsg = "DELETE ";
    confirmMsg += drum_config[sampleIndex].name;
    confirmMsg += "? PUSH ENC[R] TO CONFIRM";
    showError(confirmMsg.c_str());
    fill_up_with_spaces_left_window_filemanager();
  }
  else
  {
    showError("");
    fill_up_with_spaces_left_window_filemanager();
  }
}

// -----------------------------------------------------------------------------
// File Manager: force immediate refresh after directory changes.
// This avoids needing a "dummy" encoder rotation to redraw the file list.
// -----------------------------------------------------------------------------
static FLASHMEM void fm_refresh_sd_view_after_nav()
{
  // Reload directory model
  load_sd_directory();

  // Redraw directory list
  sd_printDirectory(false);

  // Refresh UI elements that depend on mode/selection
  print_file_manager_buttons();
  print_file_manager_delete_button();
  print_file_manager_active_border();
  file_manager_clear_text_line();
}

FLASHMEM void UI_func_file_manager(uint8_t param)
{
  static constexpr char CUSTOM_DIR[] = "CUSTOM";
  static constexpr char RECORDINGS_DIR[] = "RECORDINGS";
  static constexpr char MIDI_DIR[] = "MIDI";
  static constexpr char BRAIDS[] = "BRAIDS";

#ifdef  SECOND_SD
  static constexpr char DRUMS[] = "DRUMS";
#endif

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_file_manager);
    // handleStop(); 

    menuhelper_previous_val = 199;  //fake value to draw all touch buttons at start
    if (sd_card_internal > 0)
    {
      // Auto create missing folders if needed
      if (!SD.exists(CUSTOM_DIR)) {
        SD.mkdir(CUSTOM_DIR);
      }
      if (!SD.exists(RECORDINGS_DIR)) {
        SD.mkdir(RECORDINGS_DIR);
      }
      if (!SD.exists(MIDI_DIR)) {
        SD.mkdir(MIDI_DIR);
      }
      if (!SD.exists(BRAIDS)) {
        SD.mkdir(BRAIDS);
      }
    }
#ifdef SECOND_SD
    if (!SD_EXTERNAL.exists(CUSTOM_DIR)) {
      SD_EXTERNAL.mkdir(CUSTOM_DIR);
    }
    if (!SD_EXTERNAL.exists(RECORDINGS_DIR)) {
      SD_EXTERNAL.mkdir(RECORDINGS_DIR);
    }
    if (!SD_EXTERNAL.exists(MIDI_DIR)) {
      SD_EXTERNAL.mkdir(MIDI_DIR);
    }
    if (!SD_EXTERNAL.exists(BRAIDS)) {
      SD_EXTERNAL.mkdir(BRAIDS);
    }
    if (!SD_EXTERNAL.exists(DRUMS)) {
      SD_EXTERNAL.mkdir(DRUMS);
    }
#endif

    fm.sd_mode = FM_BROWSE_FILES;
    fm.active_window = 0;

    reset_file_manager_folder();
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    _print_filemanager_header();
    display.setTextSize(1);
    display.setCursor(CHAR_width_small * 1, 3 * CHAR_height_small);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("PATH:"));

#ifdef COMPILE_FOR_PSRAM
    if (fm.SD_CARD_READER_EXT == false)
    {
      print_psram_stats_filemanager();
      fm.active_window = 1;
      psram_printCustomSamplesList();
      fm.active_window = 0;
    }
#endif

#ifdef  SECOND_SD
    if (fm.SD_CARD_READER_EXT) {
      _print_filemanager_sd_extern_header();
      fm.active_window = 1;
      fm.sd_new_name[0] = 0x2f;
      load_sd_directory();
      sd_printDirectory(false);
      fm.active_window = 0;
    }
#endif

    print_file_manager_buttons();
    print_file_manager_delete_button();
    print_file_manager_active_border();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {

      if (fm.active_window == 0 || fm.SD_CARD_READER_EXT) { // left window, SDCARD OR External SD Card right window
        if (e.down) {
          if (fm.sd_selected_file == fm.sd_cap_rows && fm.sd_cap_rows > 8 && fm.sd_skip_files < fm.sd_sum_files - fm.sd_cap_rows - 1)
            fm.sd_skip_files++;
          if (fm.sd_selected_file == 0 && fm.sd_parent_folder == true) {
            fm.sd_is_folder = true;
            fm.sd_selected_file = 0;
            fm.sd_parent_folder = false;
          }
          else
            fm.sd_selected_file = constrain(fm.sd_selected_file + 1, 0, fm.sd_cap_rows);
        }
        else if (e.up) {
          if (fm.sd_selected_file == 0 && fm.sd_skip_files > 0)
            fm.sd_skip_files--;
          else if (fm.sd_selected_file == 0 && fm.sd_skip_files == 0) {
            if (fm.sd_folder_depth > 0)
              fm.sd_parent_folder = true;
          }
          fm.sd_selected_file = constrain(fm.sd_selected_file - 1, 0, fm.sd_cap_rows);
        }
      }
#ifdef COMPILE_FOR_PSRAM
      if (fm.active_window == 1 && fm.SD_CARD_READER_EXT == false) { // right window, PSRAM
        if (e.down) {
          if (fm.psram_selected_file == fm.psram_cap_rows && fm.psram_cap_rows > 8 && fm.psram_skip_files < fm.psram_sum_files - fm.psram_cap_rows - 1)
            fm.psram_skip_files++;

          fm.psram_selected_file = constrain(fm.psram_selected_file + 1, 0, fm.psram_cap_rows);
        }
        else if (e.up) {
          if (fm.psram_selected_file == 0 && fm.psram_skip_files > 0)
            fm.psram_skip_files--;

          fm.psram_selected_file = constrain(fm.psram_selected_file - 1, 0, fm.psram_cap_rows);
        }
      }
#endif

      if (fm.sd_selected_file > fm.sd_cap_rows) fm.sd_selected_file = fm.sd_cap_rows;
      if (fm.sd_selected_file < 0) fm.sd_selected_file = 0;

      print_file_manager_delete_button();
    }


    if (e.pressed) {

      // NEW: In File Manager browse mode, clicking a FILE goes to parent directory.
      // Clicking a DIRECTORY must keep working (enter directory), so we trigger this only when NOT a folder.
      if (fm.sd_mode == FM_BROWSE_FILES &&
          (fm.active_window == 0 || fm.SD_CARD_READER_EXT) &&
          !fm.sd_is_folder)
      {
        // If already at root, do nothing
        if (fm.sd_folder_depth == 0 || fm.sd_new_name == "/") {
          return; // consume click, avoids accidental file action
        }

        // Go to parent (same logic you already use for "..")
        if (fm.sd_folder_depth < 2) {
          fm.sd_folder_depth = 0;
          fm.sd_new_name = "/";
          fm.sd_parent_folder = false;
        } else {
          const int lastSlash = fm.sd_new_name.lastIndexOf("/");
          fm.sd_new_name = fm.sd_new_name.substring(0, lastSlash);
          fm.sd_folder_depth--;
        }

        // Reset selection and refresh listing
        fm.sd_selected_file = 0;
        fm.sd_skip_files = 0;
        load_sd_directory();

        // Optional UI refresh
        sd_printDirectory(false); 
        print_file_manager_buttons();
        print_file_manager_delete_button();
        file_manager_clear_text_line();
        return; // IMPORTANT: do not continue with normal press handling
      }


      if (fm.sd_mode == FM_DELETE_FROM_PSRAM) {
        const uint8_t customSampleIndex = fm.psram_selected_file + fm.psram_skip_files;
        const uint8_t sampleIndex = customSampleIndex + NUM_STATIC_PITCHED_SAMPLES;

        // Unload the sample from memory
        if (drum_config[sampleIndex].drum_data != nullptr) {
          sampleLoader.unloadSample(drum_config[sampleIndex].drum_data, drum_config[sampleIndex].len * 2);
          drum_config[sampleIndex].drum_data = nullptr;
          drum_config[sampleIndex].len = 0;
          drum_config[sampleIndex].filename[0] = '\0';
          // drum_config[sampleIndex].name[0] = 'EMPTY\0';
          sprintf(drum_config[sampleIndex].name, "CUSTOM%02d", customSampleIndex + 1);
          customSamples[customSampleIndex].filepath[0] = '\0';
        }
#if defined(COMPILE_FOR_PSRAM)
        print_psram_stats_filemanager();
        psram_printCustomSamplesList();
#endif
        // fm.sd_mode = FM_BROWSE_FILES;
        print_file_manager_buttons();

      }

      else if (fm.sd_mode == FM_DELETE_FILE_OR_FOLDER) {
        // Show deleting message
        String deletingMsg = "Deleting: ";
        deletingMsg += fm.sd_temp_name;
        showError(deletingMsg.c_str());

        // Declare and initialize success variable
        bool delete_success = false;
        fm.sd_full_name = fm.sd_new_name + "/" + fm.sd_temp_name;

        // Determine if item is a folder
        bool isFolder = false;
        if (fm.active_window == 0) { // Internal SD
          File f = SD.open(fm.sd_full_name.c_str());
          if (f) {
            isFolder = f.isDirectory();
            f.close();
          }
        }
#ifdef SECOND_SD
        else if (fm.active_window == 1) { // External SD
          File f = SD_EXTERNAL.open(fm.sd_full_name.c_str());
          if (f) {
            isFolder = f.isDirectory();
            f.close();
          }
        }
#endif

        // Perform deletion
        if (isFolder) {
          if (fm.active_window == 0) {
            delete_success = deleteFolderInternalSD(fm.sd_full_name);
          }
#ifdef SECOND_SD
          else if (fm.active_window == 1) {
            delete_success = deleteFolderExternalSD(fm.sd_full_name);
          }
#endif
        }
        else {
          if (fm.active_window == 0) {
            delete_success = SD.remove(fm.sd_full_name.c_str());
          }
#ifdef SECOND_SD
          else if (fm.active_window == 1) {
            delete_success = SD_EXTERNAL.remove(fm.sd_full_name.c_str());
          }
#endif
        }


        // Refresh directory listing
        if (fm.active_window == 0) {
          load_sd_directory();
        }
#ifdef SECOND_SD
        else if (fm.SD_CARD_READER_EXT && fm.active_window == 1) {
          load_sd_directory();
        }
#endif

        // Show result
        if (delete_success) {

          // Clear any delete messages
          file_manager_clear_text_line();

          // Update buttons to reflect browse mode
          print_file_manager_buttons();
          print_file_manager_delete_button();
        }
        else {

          // Stay in delete mode for retry
          print_file_manager_delete_button();
        }

        // Reset selection
        // fm.sd_selected_file = 0;
        // fm.sd_skip_files = 0;
        // fm.sd_temp_name = "";

        // Stay in delete mode
        print_file_manager_delete_button();
        // fm.sd_mode = FM_BROWSE_FILES;
        print_file_manager_buttons();
        print_file_manager_delete_button();

      }

      if (fm.sd_is_folder)
      {
        // ---------------------------------------------------------------------------
        // FOLDER CLICK HANDLING (single source of truth)
        // - ".." is NOT a real directory name and must NEVER be appended to the path.
        // - In BROWSE mode: enter directory (or go parent if "..")
        // - In COPY > PSRAM: allow directory navigation while keeping COPY > PSRAM active
        // ---------------------------------------------------------------------------

        // Cache current mode because COPY modes must survive directory navigation
        const uint8_t modeBeforeNav = fm.sd_mode;

        // --- Handle ".." first (works in ANY mode) ---
        if (fm.sd_temp_name == "..")
        {
          if (fm.sd_folder_depth < 2)
          {
            fm.sd_folder_depth = 0;
            fm.sd_new_name = "/";
            fm.sd_parent_folder = false;
          }
          else
          {
            const int lastSlash = fm.sd_new_name.lastIndexOf("/");
            fm.sd_new_name = fm.sd_new_name.substring(0, lastSlash);
            fm.sd_folder_depth--;
          }

          fm.sd_selected_file = 0;
          fm.sd_skip_files = 0;
          fm.sd_parent_folder = false;

          fm_refresh_sd_view_after_nav();


          // Restore mode (important for COPY > PSRAM and other copy modes)
          fm.sd_mode = modeBeforeNav;

          // UI refresh (optional but keeps UX responsive)
          print_file_manager_buttons();
          print_file_manager_delete_button();
          file_manager_clear_text_line();

          return; // consume click
        }

        // --- Copy actions that trigger on folder click (external/internal SD copy) ---
      #ifdef SECOND_SD
        if (fm.SD_CARD_READER_EXT && fm.active_window == 0 && fm.sd_mode == FM_COPY_TO_EXTERNAL)
        {
          file_manager_copy(true);   // internal -> external
          return;
        }
        else if (fm.SD_CARD_READER_EXT && fm.active_window == 1 && fm.sd_mode == FM_COPY_TO_INTERNAL)
        {
          file_manager_copy(false);  // external -> internal
          return;
        }
      #endif

        // --- Directory navigation ---
        // Enter a real directory when:
        // - browsing, or
        // - COPY > PSRAM is active (we allow navigation in this mode too)
        if (fm.sd_mode == FM_BROWSE_FILES || fm.sd_mode == FM_COPY_TO_PSRAM)
        {
          if (fm.sd_folder_depth > 0)
          {
            fm.sd_new_name.append("/");
          }

          fm.sd_new_name.append(fm.sd_temp_name);
          fm.sd_folder_depth++;

          fm.sd_selected_file = 0;
          fm.sd_skip_files = 0;
          fm.sd_parent_folder = false;

          fm_refresh_sd_view_after_nav();


          // Keep previous mode (important for COPY > PSRAM)
          fm.sd_mode = modeBeforeNav;

          print_file_manager_buttons();
          print_file_manager_delete_button();
          file_manager_clear_text_line();

          return; // consume click
        }

        // --- Optional UI button (kept from your original code) ---
        if (fm.SD_CARD_READER_EXT == false)
        {
          TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_NORMAL);
        }

        return; // folder click handled
      }

      else { // is a file
        switch (fm.sd_mode) {

#ifdef COMPILE_FOR_PSRAM
        case FM_COPY_TO_PSRAM:
          if (fm.sd_temp_name.endsWith(".wav") && fm.sd_temp_name.length()<=FILENAME_MAX_LEN-1 ) {
            fm.sd_full_name = fm.sd_new_name + "/" + fm.sd_temp_name;
            const uint8_t customSampleIndex = fm.psram_selected_file + fm.psram_skip_files;
            const uint8_t sampleIndex = customSampleIndex + NUM_STATIC_PITCHED_SAMPLES;
            sampleLoader.loadSampleToDrums(fm.sd_full_name, drum_config[sampleIndex], SD);
            if (drum_config[sampleIndex].drum_data != nullptr) {
              customSamples[customSampleIndex].start = 0;    // 0%
              customSamples[customSampleIndex].end = 1000;  // 100%
              customSamples[customSampleIndex].loopType = 0; // loop_type::looptype_none
              strcpy(customSamples[customSampleIndex].filepath, fm.sd_new_name.c_str());
              drum_config[sampleIndex].drum_class = DRUM_POLY; // default is poly
              strcpy(drum_config[sampleIndex].filename, fm.sd_temp_name.c_str());
              strip_extension(fm.sd_temp_name.c_str(), drum_config[sampleIndex].name, 14);
              // NEW (refined): move selection to next truly free CUSTOM slot
              const int16_t nextFree = fm_find_next_free_psram_slot((int16_t)customSampleIndex);
              if (nextFree >= 0) {
                fm_psram_copy_target = nextFree;  // set this to highlight next free slot
                fm_select_psram_slot((uint16_t)nextFree);
              } else {
                fm_psram_copy_target = -1;
                // NEW: PSRAM FULL
                  showError("PSRAM FULL");
                  delay(1000);
                // keep selection on the slot just used
              }
              print_psram_stats_filemanager();
              psram_printCustomSamplesList();
            }
          } else
          { showError("FILENAME TOO LONG OR INVALID");
          delay(1000);
          }
             
          break;

#endif

#ifdef  SECOND_SD
        case FM_COPY_TO_EXTERNAL:
          file_manager_copy(true); // To copy from internal to external SD:

          break;
        case FM_COPY_TO_INTERNAL:
          file_manager_copy(false);// To copy from external to internal SD:

          break;
#endif

          // default:
          // case FM_PLAY_SAMPLE:
          //   previewWavFilemanager();
          //   break;

        }
      }

      if (fm.sd_mode == 99) {
        fm.sd_mode = FM_BROWSE_FILES;

        // NEW: leaving COPY/temporary modes -> clear PSRAM COPY target highlight
        fm_psram_copy_target = -1;

        print_file_manager_buttons();
      }
    }


  } //loop end

  // show files
  if (fm.active_window == 0 && fm.SD_CARD_READER_EXT == false)
  {
    fm.sd_new_name[0] = 0x2f;
    load_sd_directory();
    sd_printDirectory(false);

    display.setCursor(CHAR_width_small * 22, 1 * CHAR_height_small);
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    print_formatted_number(fm.sd_sum_files, 4);
    show_smallfont_noGrid(3 * CHAR_height_small, CHAR_width_small * 7, 19, fm.sd_new_name.c_str());
  }
#ifdef  SECOND_SD
  if (fm.SD_CARD_READER_EXT)
  {
    fm.sd_new_name[0] = 0x2f;
    load_sd_directory();
    sd_printDirectory(false);
  }
#endif

#ifdef COMPILE_FOR_PSRAM
  else if (fm.active_window == 1 && fm.SD_CARD_READER_EXT == false)
  {
    psram_printCustomSamplesList();
  }
#endif

#ifdef  SECOND_SD
  _print_file_and_folder_copy_info(fm.sd_selected_file);
#endif

  // DRAW DELETE MESSAGE LAST - AFTER EVERYTHING ELSE
  if (fm.sd_mode == FM_DELETE_FILE_OR_FOLDER) {

    if (fm.sd_temp_name == "..")
    {
      fm.sd_mode = FM_BROWSE_FILES;
      menuhelper_previous_val = 98;
      print_file_manager_delete_button();
      menuhelper_previous_val = 89;
      print_file_manager_buttons();
      file_manager_clear_text_line();
    }
    else
    {
      String confirmMsg = "DELETE: ";
      confirmMsg += fm.sd_temp_name;
      confirmMsg += "? PUSH ENC[R] TO CONFIRM";

      // Clear only the message area
      file_manager_clear_text_line();
      display.setCursor(CHAR_width_small * 1, 23 * CHAR_height_small + 5);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(confirmMsg);

    }
  }

  else if (fm.sd_mode == FM_DELETE_FROM_PSRAM) {
    show_delete_info_psram();
  }

  else
  {
    file_manager_clear_text_line();
  }

 print_file_manager_delete_button();


  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {

#ifdef COMPILE_FOR_PSRAM
    if (previewSlot.drum_data != nullptr) {
      WAV_preview_PSRAM.stop();
      sampleLoader.unloadSample(previewSlot.drum_data, previewSlot.len * 2); // 16bit value
      previewSlot.len = 0;
    }
#endif
    unregisterTouchHandler();
    previous_delete_item_type = 98;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
}


FLASHMEM void UI_func_midi_soft_thru(uint8_t param)
{
  static uint8_t old_soft_midi_thru;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    old_soft_midi_thru = configuration.sys.soft_midi_thru;
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("MIDI Soft THRU"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      configuration.sys.soft_midi_thru = constrain(configuration.sys.soft_midi_thru + e.dir, SOFT_MIDI_THRU_MIN, SOFT_MIDI_THRU_MAX);
    }

    setCursor_textGrid(1, 2);
    switch (configuration.sys.soft_midi_thru)
    {
    case 0:
      display.print(F("[OFF]"));
      break;
    case 1:
      display.print(F("[ON ]"));
      break;
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    if (old_soft_midi_thru != configuration.sys.soft_midi_thru)
    {
      save_sys_flag = true;
      save_sys = 0;
    }
  }
}

FLASHMEM void _show_midi_channel(const __FlashStringHelper* _txt, uint8_t line, uint8_t midi_channel, uint8_t instance = 0)
{
  char text[21];
  const char* txt = (const char*)_txt;

  if (0 == instance)
    snprintf_P(text, sizeof(text), PSTR("%s"), txt);
  else
    snprintf_P(text, sizeof(text), PSTR("%s%d"), txt, instance);

  setCursor_textGrid_small(2, line);
  display.setTextColor(GREY1);
  display.print(text);
  display.setTextColor(COLOR_SYSTEXT);
  setCursor_textGrid_small(40, line);

  _print_midi_channel(midi_channel); // deals with OMNI and OFF itself
}

FLASHMEM void UI_func_sd_content_not_found(uint8_t param)
{
  if (LCDML.FUNC_setup())
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    generic_temp_select_menu = 0;
    configuration.sys.screen_saver_mode = 4; // no screensaver
    LCDML.SCREEN_disable();
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(2, 1);
    display.print(F("WELCOME TO MDT! "));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("UNFORTUNATELY THERE IS AN ISSUE:"));
    display.setTextSize(2);
    setCursor_textGrid_small(2, 3);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("*ERROR*"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(2, 5);
    display.print(F("DEFAULT DATA FROM THE SD CARD "));
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("NOT FOUND"));
    setCursor_textGrid_small(2, 6);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("MDT "));
    display.setTextColor(COLOR_SYSTEXT, RED);
    display.print(F("CAN NOT"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F(" LOAD ANY PRESET OR PERFORMANCE "));
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    display.print(F(":_("));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 9);
    display.print(F("HAVE YOU COPIED THE SD CONTENT TO YOUR SD CARD ? "));
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 11);
    display.print(F("Please copy all Files/Directories from:"));
    setCursor_textGrid_small(2, 12);
    display.print(F("/addon/SD/"));
    setCursor_textGrid_small(2, 13);
    display.print(F("to the root of your SD Card (FAT32) and"));
    setCursor_textGrid_small(2, 14);
    display.print(F("insert the card in SD Slot of the Teensy 4.1"));
    setCursor_textGrid_small(2, 16);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("PLEASE ALSO CHECK:"));
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 18);
    display.print(F("codeberg.org/positionhigh/MicroDexed-touch/releases"));
    setCursor_textGrid_small(2, 19);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("and the FAQ for further information"));
    setCursor_textGrid_small(2, 21);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("DISCORD CHAT: "));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("https://discord.gg/XCYk5P8GzF"));
    helptext_l(back_text);
  }
  if (LCDML.FUNC_loop())
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      // nothing yet
    }
    if (e.pressed) {
      // nothing yet
    }
  }
  // ****** STABLE END *********
  if (LCDML.FUNC_close())
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

#if defined GLOW
extern void set_glow_print_number(uint16_t x, uint8_t y, int input, uint8_t length, uint8_t menuitem, uint8_t textsize);
#endif

static FLASHMEM void check_midi_channel(uint8_t item, uint8_t pos_x, uint8_t pos_y, uint8_t midi_channel)
{
  if (menu_item_check(item))
  {
    setModeColor(item);
    setCursor_textGrid_small(pos_x, pos_y);
    if (MIDI_CHANNEL_ABSENT != midi_channel)
    {
      _print_midi_channel(midi_channel);
#if defined GLOW
      if (generic_temp_select_menu == item)
        set_glow_show_text_no_grid(pos_x * CHAR_width_small, pos_y * (CHAR_height_small + 2), 4, get_midi_channel_name(midi_channel), item, 1);
#endif
    }
    else
      _print_midi_channel(MIDI_CHANNEL_OFF);
  }
}

FLASHMEM void UI_func_midi_channels(uint8_t param)
{
  if (LCDML.FUNC_setup())
  {
    registerTouchHandler(handle_touchscreen_midi_channel_page);
    registerScope(180, 0, 128, 50);
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    generic_temp_select_menu = 0;
    setCursor_textGrid(1, 1);
    display.setTextSize(2);
    display.print(F("MIDI channels"));
    helptext_l(back_text);
    helptext_r(F("EDIT"));
    display.setTextSize(1);
    for (uint8_t i = 0; i < 4; i++)
    {
#if (NUM_DEXED>2)
      _show_midi_channel(F("DEXED INSTANCE #"), 5 + i, configuration.dexed[i].midi_channel, i + 1);
#else
      _show_midi_channel(F("DEXED INSTANCE #"), 5 + i, MIDI_CHANNEL_OFF, i + 1);
#endif
    }

#ifdef GRANULAR
    _show_midi_channel(F("GRANULAR SYNTH"), 9, granular_params.midi_channel);
#endif
    _show_midi_channel(F("MDA ePIANO"), 10, configuration.epiano.midi_channel);

    for (uint8_t i = 0; i < 2; i++)
    {
      _show_midi_channel(F("MICROSYNTH #"), 12 + i, microsynth[i].midi_channel, i + 1);
    }

    _show_midi_channel(F("BRAIDS"), 14, braids_osc.midi_channel);

    for (uint8_t i = 0; i < 2; i++)
    {
      _show_midi_channel(F("MULTISAMPLE (MSP) #"), 15 + i, msp[i].midi_channel, i + 1);
    }

    _show_midi_channel(F("DRUMS/SAMPLES"), 17, drum_midi_channel);
    _show_midi_channel(F("DRUM SLICES"), 18, slices_midi_channel);
  }

  if (LCDML.FUNC_loop())
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0)
      {
        uint32_t valid = 0x1FFF
#if (NUM_DEXED<=2)
          & ~(3 << 2) // eliminate two Dexed instances as valid selections
#endif // (NUM_DEXED<=2)
#if !defined(GRANULAR)
          & ~(1 << 4)// eliminate granular instance as valid selection
#endif // !defined(GRANULAR)
          ;

        uint8_t new_menu = generic_temp_select_menu;

        new_menu = constrain(generic_temp_select_menu + e.dir, 0, 12);
        while (0 == (valid & (1 << new_menu))) // invalid selection?
        {
          new_menu = constrain(new_menu + e.dir, 0, 12); // keep going: 0 and 12 better be valid!
        }

#if defined(GLOW)
        if (new_menu != generic_temp_select_menu)
          remove_glow();
#endif // defined(GLOW)

        generic_temp_select_menu = new_menu;
      }
      else if (generic_temp_select_menu == 0)
        configuration.dexed[0].midi_channel = constrain(configuration.dexed[0].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 1)
        configuration.dexed[1].midi_channel = constrain(configuration.dexed[1].midi_channel + e.dir, 0, 17);
#if (NUM_DEXED>2)
      else if (generic_temp_select_menu == 2)
        configuration.dexed[2].midi_channel = constrain(configuration.dexed[2].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 3)
        configuration.dexed[3].midi_channel = constrain(configuration.dexed[3].midi_channel + e.dir, 0, 17);
#endif

#ifdef GRANULAR
      else if (generic_temp_select_menu == 4)
        granular_params.midi_channel = constrain(granular_params.midi_channel + e.dir, 0, 17);
#endif
      else if (generic_temp_select_menu == 5)
        configuration.epiano.midi_channel = constrain(configuration.epiano.midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 6)
        microsynth[0].midi_channel = constrain(microsynth[0].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 7)
        microsynth[1].midi_channel = constrain(microsynth[1].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 8)
        braids_osc.midi_channel = constrain(braids_osc.midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 9)
        msp[0].midi_channel = constrain(msp[0].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 10)
        msp[1].midi_channel = constrain(msp[1].midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 11)
        drum_midi_channel = constrain(drum_midi_channel + e.dir, 0, 17);
      else if (generic_temp_select_menu == 12)
        slices_midi_channel = constrain(slices_midi_channel + e.dir, 0, 17);
    }
    // handle button presses during menu
    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
    }
    // button check end

    display.setTextSize(1);

    check_midi_channel(0, 40, 5, configuration.dexed[0].midi_channel);
    check_midi_channel(1, 40, 6, configuration.dexed[1].midi_channel);
#if (NUM_DEXED>2)
    check_midi_channel(2, 40, 7, configuration.dexed[2].midi_channel);
    check_midi_channel(3, 40, 8, configuration.dexed[3].midi_channel);
#else
    check_midi_channel(2, 40, 7, MIDI_CHANNEL_ABSENT);
    check_midi_channel(3, 40, 8, MIDI_CHANNEL_ABSENT);
#endif // (NUM_DEXED>2)

#if defined(GRANULAR)
    check_midi_channel(4, 40, 9, granular_params.midi_channel);
#endif // defined(GRANULAR)

    check_midi_channel(5, 40, 10, configuration.epiano.midi_channel);
    //------- row break ------------------------------------------
    check_midi_channel(6, 40, 12, microsynth[0].midi_channel);
    check_midi_channel(7, 40, 13, microsynth[1].midi_channel);
    check_midi_channel(8, 40, 14, braids_osc.midi_channel);
    check_midi_channel(9, 40, 15, msp[0].midi_channel);
    check_midi_channel(10, 40, 16, msp[1].midi_channel);
    check_midi_channel(11, 40, 17, drum_midi_channel);
    check_midi_channel(12, 40, 18, slices_midi_channel);

    if (count_omni() != 0)
    {
      display.setTextSize(1);
      setCursor_textGrid_small(8, 21);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(F("WARNING: "));
      display.setTextColor(RED, COLOR_BACKGROUND);

      display.print(count_omni());
      if (count_omni() == 1)
        display.print(F(" INSTRUMENT IS SET TO OMNI   "));
      else
        display.print(F(" INSTRUMENTS ARE SET TO OMNI "));
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      setCursor_textGrid_small(8, 22);
      display.print(F("ASSIGN EACH INSTRUMENT TO AN UNIQUE"));
      setCursor_textGrid_small(8, 23);
      display.print(F("MIDI CHANNEL AND "));
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(F("SAVE YOUR PERFORMANCE"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }
    else if (count_midi_channel_duplicates(false) != 0)
    {
      display.setTextSize(1);
      setCursor_textGrid_small(8, 21);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(F("NOTICE: "));
      display.setTextColor(RED, COLOR_BACKGROUND);

      if (count_midi_channel_duplicates(false) != 1)
      {
        display.print(count_midi_channel_duplicates(false));
        display.print(F(" "));
      }
      display.print(F("MIDI CHANNEL"));
      if (count_midi_channel_duplicates(false) != 1)
        display.print(F("S"));
      if (count_midi_channel_duplicates(false) == 1)
      {
        display.print(F(" #"));
        display.print(count_midi_channel_duplicates(true));
      }
      display.setTextColor(GREY2, COLOR_BACKGROUND);

      if (count_midi_channel_duplicates(false) == 1)
        display.print(F(" IS"));
      else
        display.print(F(" ARE"));
      display.print(F(" ASSIGNED"));
      if (count_midi_channel_duplicates(false) == 1)
        display.print(F("  "));

      setCursor_textGrid_small(8, 22);
      display.print(F("TO MULTIPLE INSTRUMENTS - YOU CAN  "));
      setCursor_textGrid_small(8, 23);
      display.print(F("IGNORE THIS WHEN IT IS DONE"));
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(F(" ON PURPOSE"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    }

    else if (check_and_confirm_midi_channels == true)
    {
      display.setTextSize(1);
      setCursor_textGrid_small(8, 21);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display.print(F("ALL MIDI CHANNEL SETTINGS"));
      display.setTextColor(GREEN, COLOR_BACKGROUND);
      display.print(F(" CLEAR !   "));

      display.setTextColor(GREY2, COLOR_BACKGROUND);

      setCursor_textGrid_small(8, 22);
      display.print(F("YOU NOW SHOULD SAVE YOUR CHANGES   "));
      setCursor_textGrid_small(8, 23);
      display.print(F("TO THE CURRENTLY LOADED PERFORMANCE   "));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      check_and_confirm_midi_channels = false;
    }
    else
    {
      if (remote_active)
        display.console = true;
      display.fillRect(7 * CHAR_width_small + 3, 25 * CHAR_height_small + 4, 231, 34, COLOR_BACKGROUND);
      if (remote_active)
        display.console = false;
    }
  }
  // ****** STABLE END *********
  if (LCDML.FUNC_close())
  {
    unregisterTouchHandler();
    unregisterScope();
    check_and_confirm_midi_channels = false;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}



FLASHMEM void _system_settings_long_help_texts()
{
  static uint8_t previous_select_menu = 255;

  //Exit if the selected menu item is unchanged
  if (generic_temp_select_menu != previous_select_menu) {
    previous_select_menu = generic_temp_select_menu;
  }
  else {
    if (generic_temp_select_menu != 9)
      return;
  }

  setCursor_textGrid_small(2, 18);
  print_empty_spaces(47, 1);
  setCursor_textGrid_small(2, 19);
  print_empty_spaces(47, 1);

  display.setTextColor(GREY1, COLOR_BACKGROUND);

  if (generic_temp_select_menu == 8) {
    setCursor_textGrid_small(2, 18);
    display.print(F("DEFAULT VALUE IS 183. DO NOT TOUCH UNLESS YOUR "));
    setCursor_textGrid_small(2, 19);
    display.print(F("DISPLAY IS UNCOMMON. RESULTS VARY WITH RUNTIME!"));
  }

  else if (generic_temp_select_menu == 9) {

    if (MicroDexed[0]->getEngineType() == 0)
    {
      setCursor_textGrid_small(2, 18);
      display.print(F("MODERN: THIS IS THE ORIGINAL 24-BIT MUSIC-     "));
      setCursor_textGrid_small(2, 19);
      display.print(F("SYNTHESIZER-FOR-ANDROID IMPLEMENTATION         "));
    }
    else if (MicroDexed[0]->getEngineType() == 1)
    {
      setCursor_textGrid_small(2, 18);
      display.print(F("MARK I: BASED ON THE OPL SERIES BUT AT A HIGHER"));
      setCursor_textGrid_small(2, 19);
      display.print(F("RESOLUTION. TARGET IS TO BE CLOSEST TO REAL DX7"));
    }
    else if (MicroDexed[0]->getEngineType() == 2)
    {
      setCursor_textGrid_small(2, 18);
      display.print(F("OPL: THIS IS AN EXPERIMENTAL IMPLEMENTATION OF "));
      setCursor_textGrid_small(2, 19);
      display.print(F("THE REVERSED ENGINEERED OPL FAMILY CHIPS, 8-BIT"));
    }
  }
  else if (generic_temp_select_menu == 10) {
    setCursor_textGrid_small(2, 18);
    display.print(F("ENABLE TO SKIP WARNINGS ABOUT DUPLICATE MIDI-  "));
    setCursor_textGrid_small(2, 19);
    display.print(F("CHANNELS OR MIDI SYNC IS SET TO EXTERNAL SOURCE"));
  }

  else if (generic_temp_select_menu == 11) {
    setCursor_textGrid_small(2, 18);
    display.print(F("TRANSMIT SYSEX WHEN CHANGING DEXED VOICE OR ROM"));
    setCursor_textGrid_small(2, 19);
    print_empty_spaces(47, 1);
  }

  else if (generic_temp_select_menu == 12) {
    setCursor_textGrid_small(2, 18);
    display.print(F("TRANSMIT MIDI CC ON VOICE CHANGE & STARTUP"));
    setCursor_textGrid_small(2, 19);
    print_empty_spaces(47, 1);
  }
  else if (generic_temp_select_menu == 14 || generic_temp_select_menu == 15) {
    setCursor_textGrid_small(2, 18);
    display.print(F("SET ENCODER RGB COLORS FOR DEFAULT AND"));
    setCursor_textGrid_small(2, 19);
    display.print(F("EDIT/ACTIVATED MODE"));
  }

}

#if defined (RGB_ENCODERS) && defined (MCP_23008) || defined (RGB_ENCODERS) && defined (MCP_23017)
extern void set_custom_enc_colors();
#endif

FLASHMEM void _render_rgb_enc_preview_system_settings(uint8_t line, uint8_t value)
{
  bool red, green, blue;
  red = bitRead(value, 0);
  green = bitRead(value, 1);
  blue = bitRead(value, 2);
  display.console = true;
  display.fillRect(280, 10 * (line), 16, 8, RGB24toRGB565(red * 254, green * 254, blue * 254));
  display.console = false;

#if defined (RGB_ENCODERS) && defined (MCP_23008) || defined (RGB_ENCODERS) && defined (MCP_23017)
  set_custom_enc_colors();
#endif

}



FLASHMEM void _render_misc_settings(uint8_t scroll_position, uint8_t total_menu_items)
{
  const uint8_t max_visible_settings = 10;

  auto clear_desc = [](uint8_t line) {
    setCursor_textGrid_small(2, line);
    print_empty_spaces(42, 1);
    };

  auto clear_val = [](uint8_t line) {
    setCursor_textGrid_small(42, line);
    print_empty_spaces(8, 1);
    };

  auto render_label = [](uint8_t setting_index, uint8_t line) {
    setCursor_textGrid_small(2, line);
    switch (setting_index) {
    case 0: display.print(F("GAMEPAD/NATIVE KEY SPEED")); break;
    case 1: display.print(F("SCREENSAVER START")); break;
    case 2: display.print(F("SCREENSAVER MODE")); break;
    case 3: display.print(F("DISPLAY ROTATION")); break;
    case 4: display.print(F("TOUCH SCREEN ROTATION")); break;
    case 5: display.print(F("SWAP ENCODER DIRECTION (A/B PINS)")); break;
    case 6: display.print(F("SKIP BOOT ANIMATION")); break;
    case 7: display.print(F("INVERT COLORS (EXPERIMENTAL)")); break;
    case 8: display.print(F("DISPLAY BRIGHTNESS")); break;
    case 9: display.print(F("DEXED ENGINE")); break;
    case 10: display.print(F("SKIP MIDI CHANNEL & MIDI SYNC WARNINGS")); break;
    case 11: display.print(F("SEND SYSEX ON VOICE CHANGE")); break;
    case 12: display.print(F("SEND MIDI CC DUMPS")); break;
    case 13: display.print(F("SWAP MIDI ACTIVITY LEDS")); break;
    case 14: display.print(F("RGB ENCODER DEFAULT COLOR"));
#ifndef RGB_ENCODERS
      display.print(F(" [NA]"));
#endif
      break;
    case 15: display.print(F("RGB ENCODER EDITOR COLOR"));
#ifndef RGB_ENCODERS
      display.print(F(" [NA]"));
#endif
      break;
    case 16: display.print(F("ANALOG VOLUME CONTROL"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;
    case 17: display.print(F("EXT. SUSTAIN PEDAL/DAMPER"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;
    case 18: display.print(F("EXTERNAL CV CLOCK INPUT"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;

    case 19: display.print(F("MIDI INPUT CC -> CV2"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;
    case 20: display.print(F("MIDI INPUT CC -> CV4"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;
    case 21: display.print(F("NOTE TRANSPOSE -> CV1"));
#ifndef MCP_CV
      display.print(F(" [NA]"));
#endif
      break;
    }
    };

  auto render_value = [](uint8_t setting_index, uint8_t line) {
    setModeColor(setting_index);
    setCursor_textGrid_small(42, line);
    switch (setting_index) {
    case 0: print_formatted_number(configuration.sys.gamepad_speed, 3); setCursor_textGrid_small(46, line); display.print(F("ms")); break;
    case 1: print_formatted_number(configuration.sys.screen_saver_start, 2); setCursor_textGrid_small(45, line); display.print(configuration.sys.screen_saver_start > 1 ? F("MINS") : F("MIN ")); break;
    case 2: print_screensaver_mode(); break;
    case 3: display.print(configuration.sys.display_rotation); break;
    case 4: display.print(configuration.sys.touch_rotation); break;
    case 5: display.print(configuration.sys.reverse_encoder_pins ? F("ON ") : F("OFF")); break;
    case 6: display.print(configuration.sys.boot_anim_skip ? F("YES") : F("NO ")); break;
    case 7: display.print(configuration.sys.invert_colors ? F("YES") : F("NO ")); break;
    case 8: print_formatted_number(configuration.sys.brightness, 3); break;
    case 9:
      if (MicroDexed[0]->getEngineType() == 0) display.print(F("MSFA"));
      else if (MicroDexed[0]->getEngineType() == 1) display.print(F("MK-I"));
      else if (MicroDexed[0]->getEngineType() == 2) display.print(F("OPL "));
      break;
    case 10: display.print(configuration.sys.skip_midi_channel_warning ? F("YES") : F("NO ")); break;
    case 11: display.print(configuration.sys.send_sysex_on_voice_change ? F("YES") : F("NO ")); break;
    case 12: display.print(configuration.sys.send_midi_cc ? F("YES") : F("NO ")); break;
    case 13: display.print(configuration.sys.swap_midi_leds ? F("YES") : F("NO ")); break;
    case 14: display.print(configuration.sys.rgb_enc_color_def);
      _render_rgb_enc_preview_system_settings(line, configuration.sys.rgb_enc_color_def);
      break;
    case 15: display.print(configuration.sys.rgb_enc_color_sel);
      _render_rgb_enc_preview_system_settings(line, configuration.sys.rgb_enc_color_sel);
      break;
    case 16: display.print(configuration.sys.vol_control ? F("YES") : F("NO ")); break;
    case 17: display.print(configuration.sys.sus_pedal ? F("YES") : F("NO ")); break;
    case 18: display.print(configuration.sys.ext_clock ? F("YES") : F("NO ")); break;

    case 19: print_formatted_number(configuration.sys.dac_cv_2, 2);  break;
    case 20: print_formatted_number(configuration.sys.dac_cv_4, 2);  break;
    case 21: print_formatted_number(configuration.sys.dac_cv_transpose, 2);  break;


      break;

    }
    };

  // Only update description if the scroll position changes
  if (scroll_position != previous_scroll_position) {

    for (uint8_t i = 0; i < max_visible_settings; i++) {
      uint8_t setting_index = scroll_position + i;
      if (setting_index >= total_menu_items) break;
      clear_desc(6 + i);
      render_label(setting_index, 6 + i);
    }
  }

  for (uint8_t i = 0; i < max_visible_settings; i++)
  {
    uint8_t setting_index = scroll_position + i;
    if (setting_index >= total_menu_items) break;
    if (menu_item_check(setting_index) || scroll_position != previous_scroll_position)
    {
      clear_val(6 + i);
      render_value(setting_index, 6 + i);
    }
  }

  if (scroll_position != previous_scroll_position)
    previous_scroll_position = scroll_position;

  drawScrollbar(308, 60, 10, total_menu_items, generic_temp_select_menu, 10);
  _system_settings_long_help_texts();
}

FLASHMEM void UI_func_system_settings(uint8_t param)
{
  static uint8_t scroll_position = 0; // Track the current scroll position
  static uint8_t total_menu_items = 22; // Zero Indexed

  if (LCDML.FUNC_setup())
  {
    registerTouchHandler(handle_touchscreen_settings_button_test);
    encoderDir[ENC_R].reset();
    generic_active_function = 0;
    generic_temp_select_menu = 0;
    scroll_position = 0; // Reset scroll position on setup

    display.fillScreen(COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("SYSTEM SETTINGS"));
    helptext_r(F("SELECT PARAMETER"));

    draw_button_on_grid(42, 1, "TOUCH", "TEST", 0);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    _render_misc_settings(scroll_position, total_menu_items);
  }

  if (LCDML.FUNC_loop())
  {
    // Handle encoders
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (generic_active_function == 0) {
        // Navigate through menu
        generic_temp_select_menu = constrain(generic_temp_select_menu + (e.dir * 1), 0, total_menu_items - 1);

        // Update scroll position if necessary
        if (generic_temp_select_menu < scroll_position) {
          scroll_position = generic_temp_select_menu;
        }
        else if (generic_temp_select_menu >= scroll_position + 10) {
          scroll_position = generic_temp_select_menu - 9;
        }
      }
      else {
        // Handle setting change and load save timer
        save_sys_flag = true;
        save_sys = SAVE_SYS_MS / 2;

        switch (generic_temp_select_menu) {
        case 0: configuration.sys.gamepad_speed = constrain(configuration.sys.gamepad_speed + (e.dir * 10), GAMEPAD_SPEED_MIN, GAMEPAD_SPEED_MAX); break;
        case 1: configuration.sys.screen_saver_start = constrain(configuration.sys.screen_saver_start + (e.dir * 1), SCREEN_SAVER_START_MIN, SCREEN_SAVER_START_MAX); setup_screensaver(); break;
        case 2: configuration.sys.screen_saver_mode = constrain(configuration.sys.screen_saver_mode + (e.dir * 1), SCREEN_SAVER_MODE_MIN, SCREEN_SAVER_MODE_MAX); setup_screensaver(); break;
        case 3: configuration.sys.display_rotation = constrain(configuration.sys.display_rotation + (e.dir * 1), DISPLAY_ROTATION_MIN, DISPLAY_ROTATION_MAX); display.setRotation(configuration.sys.display_rotation); break;
        case 4: configuration.sys.touch_rotation = constrain(configuration.sys.touch_rotation + (e.dir * 1), TOUCH_ROTATION_MIN, TOUCH_ROTATION_MAX); break;
        case 5: configuration.sys.reverse_encoder_pins = !configuration.sys.reverse_encoder_pins; _setup_rotation_and_encoders(false); break;
        case 6: configuration.sys.boot_anim_skip = !configuration.sys.boot_anim_skip; break;
        case 7: configuration.sys.invert_colors = !configuration.sys.invert_colors; display.invertDisplay(!configuration.sys.invert_colors); break;
        case 8: configuration.sys.brightness = constrain(configuration.sys.brightness + (e.dir * ENCODER[ENC_R].speed()), 100, 200); break;
        case 9:
          configuration.sys.dexed_engine_type = constrain(configuration.sys.dexed_engine_type + (e.dir * 1), 0, 2);
          for (uint8_t instance_id = 0; instance_id < NUM_DEXED; instance_id++)
            MicroDexed[instance_id]->setEngineType(configuration.sys.dexed_engine_type);
          break;
        case 10: configuration.sys.skip_midi_channel_warning = !configuration.sys.skip_midi_channel_warning; break;
        case 11: configuration.sys.send_sysex_on_voice_change = !configuration.sys.send_sysex_on_voice_change; break;
        case 12: configuration.sys.send_midi_cc = !configuration.sys.send_midi_cc; break;
        case 13: configuration.sys.swap_midi_leds = !configuration.sys.swap_midi_leds;
#if defined MIDI_ACTIVITY_LIGHTS
          set_midi_led_pins();
#endif
          break;
        case 14: configuration.sys.rgb_enc_color_def = constrain(configuration.sys.rgb_enc_color_def + (e.dir * 1), 1, 8); break;
        case 15: configuration.sys.rgb_enc_color_sel = constrain(configuration.sys.rgb_enc_color_sel + (e.dir * 1), 1, 8); break;
        case 16: configuration.sys.vol_control = !configuration.sys.vol_control; break;
        case 17: configuration.sys.sus_pedal = !configuration.sys.sus_pedal; break;
        case 18: configuration.sys.ext_clock = !configuration.sys.ext_clock;
          if (configuration.sys.ext_clock)
            seq.clock = 1;
          else
            seq.clock = 0;
          break;

        case 19: configuration.sys.dac_cv_2 = constrain(configuration.sys.dac_cv_2 + (e.dir * 1), 0, 99); break;
        case 20: configuration.sys.dac_cv_4 = constrain(configuration.sys.dac_cv_4 + (e.dir * 1), 0, 99); break;
        case 21: configuration.sys.dac_cv_transpose = constrain(configuration.sys.dac_cv_transpose + (e.dir * 1), 0, 36); break;

        }
      }
      _render_misc_settings(scroll_position, total_menu_items);
      set_sys_params();
    }

    // Handle button presses during menu
    if (e.pressed) {
      generic_active_function = !generic_active_function;
      _render_misc_settings(scroll_position, total_menu_items);
    }
  }

  if (LCDML.FUNC_close())
  {
    unregisterTouchHandler();
    generic_active_function = 99;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    previous_scroll_position = 255; //reset
  }
}

FLASHMEM void _setup_rotation_and_encoders(bool init)
{
  display.setRotation(configuration.sys.display_rotation);
#if defined GENERIC_DISPLAY
  touch.setRotation(configuration.sys.touch_rotation);
#endif

  ENCODER[ENC_L].swap_encoder_input_pins(configuration.sys.reverse_encoder_pins);
  ENCODER[ENC_R].swap_encoder_input_pins(configuration.sys.reverse_encoder_pins);
}

#ifdef MIDI_ACTIVITY_LIGHTS
FLASHMEM void set_midi_led_pins()
{
  if (configuration.sys.swap_midi_leds)
  {
    LED_MIDI_IN = 36;
    LED_MIDI_OUT = 40;
  }
  else
  { // default
    LED_MIDI_IN = 40;
    LED_MIDI_OUT = 36;
  }
}
#endif

FLASHMEM void print_mixer_text()
{
  // Dexed
  if (seq.temp_active_menu >= 0 && seq.temp_active_menu < 4)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 12);
  display.print(F("DEXED"));

  if (seq.temp_active_menu == 0)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 11);
  display.print(F("#1"));
  if (seq.temp_active_menu == 1)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(5, 11);
  display.print(F("#2"));
  if (seq.temp_active_menu == 2)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(9, 11);
  display.print(F("#3"));
  if (seq.temp_active_menu == 3)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(13, 11);
  display.print(F("#4"));

  // Epiano
  if (seq.temp_active_menu == 4)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(17, 12);
  display.print(F("EP"));

  // MicroSynth
  if (seq.temp_active_menu == 5 || seq.temp_active_menu == 6)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(21, 12);
  display.print(F("MSYNTH"));

  if (seq.temp_active_menu == 5)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(21, 11);
  display.print(F("#1"));
  if (seq.temp_active_menu == 6)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(25, 11);
  display.print(F("#2"));

  // Braids
  if (seq.temp_active_menu == 7)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  setCursor_textGrid_small(29, 12);
  display.print(F("BRD"));

  // MSP
  if (seq.temp_active_menu == 8 || seq.temp_active_menu == 9)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(33, 12);
  display.print(F("MULTSMP"));

  if (seq.temp_active_menu == 8)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(33, 11);
  display.print(F("#1"));
  if (seq.temp_active_menu == 9)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(37, 11);
  display.print(F("#2"));

  // Drums
  if (seq.temp_active_menu == 10)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  // setCursor_textGrid_small(28, 20);
  // display.print(F("L"));
  // setCursor_textGrid_small(32, 20);
  // display.print(F("R"));
  setCursor_textGrid_small(41, 12);
  display.print(F("DRM"));

  // Reverb
  if (seq.temp_active_menu == 11)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(38, 21);
  display.print(F("L"));
  setCursor_textGrid_small(42, 21);
  display.print(F("R"));
  setCursor_textGrid_small(38, 22);
  display.print(F("REVERB"));

  // Master
  if (seq.temp_active_menu == 12)
    display.setTextColor(RED, COLOR_BACKGROUND);
  else
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(46, 21);
  display.print(F("L"));
  setCursor_textGrid_small(50, 21);
  display.print(F("R"));
  setCursor_textGrid_small(46, 22);
  display.print(F("MASTER"));

  /// Values
  display.setTextColor(GREY2, COLOR_BACKGROUND);
  // print_small_panbar_mixer(0, 17, configuration.dexed[0].pan, 31);
  setCursor_textGrid_small(1, 10);
  print_formatted_number(configuration.dexed[0].sound_intensity, 3);
  // print_small_panbar_mixer(4, 17, configuration.dexed[1].pan, 31);
  setCursor_textGrid_small(5, 10);
  print_formatted_number(configuration.dexed[1].sound_intensity, 3);

#if (NUM_DEXED>2)
  setCursor_textGrid_small(9, 10);
  print_formatted_number(configuration.dexed[2].sound_intensity, 3);
  setCursor_textGrid_small(13, 10);
  print_formatted_number(configuration.dexed[3].sound_intensity, 3);
#endif

  // print_small_panbar_mixer(8, 17, configuration.epiano.pan, 31);
  setCursor_textGrid_small(17, 10);
  print_formatted_number(configuration.epiano.sound_intensity, 3);

  // print_small_panbar_mixer(12, 17, microsynth[0].pan, 31);
  setCursor_textGrid_small(21, 10);
  print_formatted_number(microsynth[0].sound_intensity, 3);
  // print_small_panbar_mixer(16, 17, microsynth[1].pan, 31);
  setCursor_textGrid_small(25, 10);
  print_formatted_number(microsynth[1].sound_intensity, 3);

  // print_small_panbar_mixer(20, 17, braids_osc.pan, 31);
  setCursor_textGrid_small(29, 10);
  print_formatted_number(braids_osc.sound_intensity, 3);

  // msp
  //  print_small_panbar_mixer(20, 17, braids_osc.pan, 31); // pan of the msp #1 zone played
  setCursor_textGrid_small(33, 10);
  print_formatted_number(msp[0].sound_intensity, 3);
  //  print_small_panbar_mixer(20, 17, braids_osc.pan, 31); // pan of the msp #2 zone played
  setCursor_textGrid_small(37, 10);
  print_formatted_number(msp[0].sound_intensity, 3);

  // drums
  temp_int = mapfloat(seq.drums_volume, 0.0, VOL_MAX_FLOAT, 0, 100);
  // setCursor_textGrid_small(28, 19);
  // print_formatted_number(temp_int, 3);
  setCursor_textGrid_small(41, 10);
  print_formatted_number(temp_int, 3);

  // reverb
  setCursor_textGrid_small(38, 20);
  print_formatted_number(configuration.fx.reverb_level, 3);
  setCursor_textGrid_small(42, 20);
  print_formatted_number(configuration.fx.reverb_level, 3);

  // Master
  setCursor_textGrid_small(46, 20);
  print_formatted_number(configuration.sys.vol, 3);
  setCursor_textGrid_small(50, 20);
  print_formatted_number(configuration.sys.vol, 3);
}

FLASHMEM void update_braids_volume()
{
  for (uint8_t instance_id = 0; instance_id < NUM_BRAIDS; instance_id++)
    braids_mixer.gain(instance_id, volume_transform(mapfloat(braids_osc.sound_intensity, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 0.7)));
}

FLASHMEM void UI_func_mixer(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_mixer);
    registerScope(210, 0, 98, 50);
    encoderDir[ENC_R].reset();
    seq.temp_active_menu = 0;
    display.fillScreen(COLOR_BACKGROUND);
    for (uint8_t j = 0; j < uint8_t(sizeof(ts.displayed_peak)); j++)
    {
      ts.displayed_peak[j] = 0;
    }

    setCursor_textGrid_small(0, 0);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print(F("MIXER"));
    helptext_l(back_text);
    helptext_r(F("< > SELECT CH"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    print_mixer_text();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (!seq.edit_state) // select channel
      {
        seq.temp_active_menu = constrain(seq.temp_active_menu + e.dir, 0, 12);
      }
      else
      {
        if (seq.temp_active_menu < 4 && seq.temp_active_menu < NUM_DEXED) // dexed instance #0 to #3
        {
          configuration.dexed[seq.temp_active_menu].sound_intensity = constrain(configuration.dexed[seq.temp_active_menu].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
        }

        else if (seq.temp_active_menu == 4) // epiano
        {
          configuration.epiano.sound_intensity = constrain(configuration.epiano.sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
          ep.setVolume(mapfloat(configuration.epiano.sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 1.0));
        }
        else if (seq.temp_active_menu > 4 && seq.temp_active_menu < 7) // microsynth
        {
          microsynth[seq.temp_active_menu - 5].sound_intensity = constrain(microsynth[seq.temp_active_menu - 5].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
        }
        else if (seq.temp_active_menu == 7) // braids
        {
          braids_osc.sound_intensity = constrain(braids_osc.sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
          update_braids_volume();
        }
        else if (seq.temp_active_menu == 8) // msp1
        {
          msp[0].sound_intensity = constrain(msp[0].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
        }
        else if (seq.temp_active_menu == 9) // msp2
        {
          msp[1].sound_intensity = constrain(msp[1].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
        }
        else if (seq.temp_active_menu == 10) // drums/samples
        {
          temp_int = constrain(temp_int + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
          seq.drums_volume = mapfloat(temp_int, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0, VOL_MAX_FLOAT);
          master_mixer_r.gain(MASTER_MIX_CH_DRUMS, volume_transform(seq.drums_volume));
          master_mixer_l.gain(MASTER_MIX_CH_DRUMS, volume_transform(seq.drums_volume));
        }
        else if (seq.temp_active_menu == 11) // reverb level
        {
          configuration.fx.reverb_level = constrain(configuration.fx.reverb_level + e.dir * e.speed, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX);
        }
        else if (seq.temp_active_menu == 12) // master level
        {
          configuration.sys.vol = constrain(configuration.sys.vol + e.dir * e.speed, VOLUME_MIN, VOLUME_MAX);
        }
      }
    }
    else if (LCDML.BT_checkEnter())
    {
      seq.edit_state = !seq.edit_state;
      border1_clear();
      if (!seq.edit_state)
      {
        display.setTextSize(1);
        setCursor_textGrid_small(0, 0);
        display.setTextColor(RED);
        display.print(F("MIXER"));
      }
    }
    if (seq.edit_state)
      helptext_r(F("CHANGE VOLUME"));
    else
      helptext_r(F("< > SELECT CHANNEL"));
    display.setTextSize(2);

    if (seq.temp_active_menu < 4 && seq.edit_state) // dexed 0-3 instance selected
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      if (NUM_DEXED > 2 || seq.temp_active_menu < NUM_DEXED)
        display_bar_int("", configuration.dexed[seq.temp_active_menu].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      else
        display_bar_int("", 0, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);

      setCursor_textGrid(1, 1);
      display.print("DEXED #");
      display.print(seq.temp_active_menu + 1);

      if (NUM_DEXED > 2 || seq.temp_active_menu < NUM_DEXED)
      {
        MD_sendControlChange(configuration.dexed[seq.temp_active_menu].midi_channel, 7, configuration.dexed[seq.temp_active_menu].sound_intensity);
        MicroDexed[seq.temp_active_menu]->setGain(midi_volume_transform(map(configuration.dexed[seq.temp_active_menu].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127)));
      }
    }
    else if (seq.temp_active_menu == 4 && seq.edit_state) // epiano
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display_bar_int("", configuration.epiano.sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      setCursor_textGrid(1, 1);
      display.print("EP");
    }
    else if (seq.temp_active_menu > 4 && seq.temp_active_menu < 7 && seq.edit_state) // microsynth
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display_bar_int("", microsynth[seq.temp_active_menu - 5].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      setCursor_textGrid(1, 1);
      display.print("MICROSYNTH #");
      display.print(seq.temp_active_menu - 4);
    }
    else if (seq.temp_active_menu == 7 && seq.edit_state) // braids
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display_bar_int("", braids_osc.sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      setCursor_textGrid(1, 1);
      display.print("BRAIDS");
    }
    else if (seq.temp_active_menu == 8 && seq.edit_state) // msp0
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display_bar_int("", msp[0].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      setCursor_textGrid(1, 1);
      display.print("MULTISAMPLE #");
      display.print(seq.temp_active_menu - 5);
    }
    else if (seq.temp_active_menu == 9 && seq.edit_state) // msp1
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      display_bar_int("", msp[1].sound_intensity, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, false);
      setCursor_textGrid(1, 1);
      display.print("MULTISAMPLE #");
      display.print(seq.temp_active_menu - 5);
    }
    else if (seq.temp_active_menu == 10 && seq.edit_state) // drums
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      temp_int = mapfloat(seq.drums_volume, 0.0, VOL_MAX_FLOAT, 0, 100);
      display_bar_int("DRUMS VOLUME", temp_int, 1.0, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 3, false, false, true);
      master_mixer_r.gain(MASTER_MIX_CH_DRUMS, volume_transform(mapfloat(temp_int, 0, 100, 0.0, VOL_MAX_FLOAT)));
      master_mixer_l.gain(MASTER_MIX_CH_DRUMS, volume_transform(mapfloat(temp_int, 0, 100, 0.0, VOL_MAX_FLOAT)));
    }
    else if (seq.temp_active_menu == 11 && seq.edit_state) // reverb level
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(1, 1);
      display.print(F("REVERB LEVEL"));
      display_bar_int("", configuration.fx.reverb_level, 1.0, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 3, false, false, false);
      master_mixer_r.gain(MASTER_MIX_CH_REVERB, volume_transform(mapfloat(configuration.fx.reverb_level, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, VOL_MAX_FLOAT)));
      master_mixer_l.gain(MASTER_MIX_CH_REVERB, volume_transform(mapfloat(configuration.fx.reverb_level, REVERB_LEVEL_MIN, REVERB_LEVEL_MAX, 0.0, VOL_MAX_FLOAT)));
    }
    else if (seq.temp_active_menu == 12 && seq.edit_state) // master volume
    {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      setCursor_textGrid(1, 1);
      display.print(F("MASTER VOLUME"));
      display_bar_int("", configuration.sys.vol, 1.0, VOLUME_MIN, VOLUME_MAX, 3, false, false, false);
      set_volume(configuration.sys.vol, configuration.sys.mono);
    }
    display.setTextSize(1);
    print_mixer_text();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void print_sidechain_static_texts()
{
  setCursor_textGrid_small(1, 1);
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("SIDECHAIN"));
  helptext_l(back_text);
  helptext_r(F("< > SELECT PARAM"));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  setCursor_textGrid_small(1, 4);
  display.print(F("TRIGGER"));

  setCursor_textGrid_small(1, 5);
  display.print(F("SOURCE"));

  setCursor_textGrid_small(1, 6);
  display.print(F("SPEED"));

  setCursor_textGrid_small(19, 6);
  display.print(F("OR"));
  setCursor_textGrid_small(25, 6);
  display.print(F("/16 STEPS"));

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(1, 8);
  display.print(F("TARGETS"));

  display.setTextColor(GREY2, COLOR_BACKGROUND);
  setCursor_textGrid_small(11, 8);
  display.print(F("COMPRESSED"));
  setCursor_textGrid_small(23, 8);
  display.print(F("UNCOMPRESSED"));

  setCursor_textGrid_small(1, 10);
  display.print(F("DEXED1"));
  setCursor_textGrid_small(1, 11);
  display.print(F("DEXED2"));
  setCursor_textGrid_small(1, 12);
  display.print(F("EPIANO"));
  setCursor_textGrid_small(1, 13);
  display.print(F("MSYNTH1"));
  setCursor_textGrid_small(1, 14);
  display.print(F("MSYNTH2"));
  setCursor_textGrid_small(1, 15);
  display.print(F("BRAIDS"));
  setCursor_textGrid_small(1, 17);
  display.print(F("DELAY A"));
  setCursor_textGrid_small(1, 18);
  display.print(F("DELAY B"));
  setCursor_textGrid_small(1, 19);
  display.print(F("REVERB"));

  // display.setTextSize(2);

  setModeColor(0);
  setCursor_textGrid_small(11, 4);
  if (sidechain_active)
  {
    display.print(F("ON "));
  }
  else
  {
    display.print(F("OFF"));
  }

  setModeColor(1);
  setCursor_textGrid_small(11, 5);
  print_formatted_number(sidechain_sample_number, 3);
  show_no_grid(6 * CHAR_height_small + 2, 15 * CHAR_width_small, 14, basename(drum_config[sidechain_sample_number].name));

  print_small_intbar(11, 10, configuration.dexed[0].sidechain_send, 3, 1, 1);
  print_small_intbar(11, 11, configuration.dexed[1].sidechain_send, 4, 1, 1);
  print_small_intbar(11, 12, configuration.epiano.sidechain_send, 5, 1, 1);
  print_small_intbar(11, 13, microsynth[0].sidechain_send, 6, 1, 1);
  print_small_intbar(11, 14, microsynth[0].sidechain_send, 7, 1, 1);
  print_small_intbar(11, 15, braids_osc.sidechain_send, 8, 1, 1);

  print_small_intbar(11, 17, configuration.fx.delay1_sidechain_send, 9, 1, 1);
  print_small_intbar(11, 18, configuration.fx.delay2_sidechain_send, 10, 1, 1);
  print_small_intbar(11, 19, configuration.fx.reverb_sidechain_send, 11, 1, 1);

  print_small_intbar(23, 10, 100 - configuration.dexed[0].sidechain_send, 3, 1, 1);
  print_small_intbar(23, 11, 100 - configuration.dexed[1].sidechain_send, 4, 1, 1);
  print_small_intbar(23, 12, 100 - microsynth[0].sidechain_send, 5, 1, 1);
  print_small_intbar(23, 13, 100 - microsynth[0].sidechain_send, 6, 1, 1);
  print_small_intbar(23, 14, 100 - configuration.epiano.sidechain_send, 7, 1, 1);
  print_small_intbar(23, 15, 100 - braids_osc.sidechain_send, 8, 1, 1);
  print_small_intbar(23, 17, 100 - configuration.fx.delay1_sidechain_send, 9, 1, 1);
  print_small_intbar(23, 18, 100 - configuration.fx.delay2_sidechain_send, 10, 1, 1);
  print_small_intbar(23, 19, 100 - configuration.fx.reverb_sidechain_send, 11, 1, 1);


  char displayname[8] = { 0, 0, 0, 0, 0, 0, 0 };
  setCursor_textGrid_small(11, 6);
  snprintf_P(displayname, sizeof(displayname), PSTR("%04d"), sidechain_speed);
  display.print(displayname);

  setCursor_textGrid_small(16, 6);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.print("MS");

  setCursor_textGrid_small(23, 6);
  snprintf_P(displayname, sizeof(displayname), PSTR("%02d"), sidechain_steps);
  display.print(displayname);

  setCursor_textGrid_small(36, 6);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  print_formatted_number(seq.bpm, 3);
  setCursor_textGrid_small(40, 6);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.print("BPM");


  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void print_sidechain_editor_values()
{
  char displayname[8] = { 0, 0, 0, 0, 0, 0, 0 };
  if (menu_item_check(0))
  {
    setModeColor(0);
    setCursor_textGrid_small(11, 4);
    if (sidechain_active)
    {
      display.print(F("ON "));
    }
    else
    {
      display.print(F("OFF"));
    }
  }

  if (menu_item_check(1))
  {
    setModeColor(1);
    setCursor_textGrid_small(11, 5);
    print_formatted_number(sidechain_sample_number, 3, 1, 1);
    show_no_grid(6 * CHAR_height_small + 2, 15 * CHAR_width_small, 14, basename(drum_config[sidechain_sample_number].name));
  }

  if (menu_item_check(3))
  {
    setModeColor(3);
    setCursor_textGrid_small(23, 6);
    //  snprintf_P(displayname, sizeof(displayname), PSTR("%02d"), sidechain_steps);
    //  display.print(displayname);
    print_formatted_number(sidechain_steps, 2, 3, 1);

    setCursor_textGrid_small(11, 6);
    snprintf_P(displayname, sizeof(displayname), PSTR("%04d"), sidechain_speed);
    display.print(displayname);
  }

  if (menu_item_check(2))
  {
    setModeColor(2);
    setCursor_textGrid_small(11, 6);
    //snprintf_P(displayname, sizeof(displayname), PSTR("%04d"), sidechain_speed);
    //display.print(displayname);
    print_formatted_number(sidechain_speed, 4, 2, 1);
  }



  if (menu_item_check(4))
  {
    setModeColor(4);
    print_small_intbar(11, 10, configuration.dexed[0].sidechain_send, 4, 1, 1);

    print_small_intbar(23, 10, 100 - configuration.dexed[0].sidechain_send, 99, 1, 1);

  }
  if (menu_item_check(5))
  {
    setModeColor(5);
    print_small_intbar(11, 11, configuration.dexed[1].sidechain_send, 5, 1, 1);

    print_small_intbar(23, 11, 100 - configuration.dexed[1].sidechain_send, 99, 1, 1);
  }

  if (menu_item_check(6))
  {
    setModeColor(6);
    print_small_intbar(11, 12, configuration.epiano.sidechain_send, 6, 1, 1);
    print_small_intbar(23, 12, 100 - configuration.epiano.sidechain_send, 99, 1, 1);
  }

  if (menu_item_check(7))
  {
    setModeColor(7);
    print_small_intbar(11, 13, microsynth[0].sidechain_send, 7, 1, 1);
    print_small_intbar(23, 13, 100 - microsynth[0].sidechain_send, 99, 1, 1);

    print_small_intbar(11, 14, microsynth[0].sidechain_send, 99, 1, 1);
    print_small_intbar(23, 14, 100 - microsynth[0].sidechain_send, 99, 1, 1);
  }

  if (menu_item_check(8))
  {
    setModeColor(8);
    print_small_intbar(11, 14, microsynth[0].sidechain_send, 8, 1, 1);
    print_small_intbar(23, 14, 100 - microsynth[0].sidechain_send, 99, 1, 1);

    print_small_intbar(11, 13, microsynth[0].sidechain_send, 7, 99, 1);
    print_small_intbar(23, 13, 100 - microsynth[0].sidechain_send, 99, 1, 1);

  }

  if (menu_item_check(9))
  {
    setModeColor(9);
    print_small_intbar(11, 15, braids_osc.sidechain_send, 9, 1, 1);
    print_small_intbar(23, 15, 100 - braids_osc.sidechain_send, 99, 1, 1);
  }

  if (menu_item_check(10))
  {
    setModeColor(10);
    print_small_intbar(11, 17, configuration.fx.delay1_sidechain_send, 10, 1, 1);
    print_small_intbar(23, 17, 100 - configuration.fx.delay1_sidechain_send, 99, 1, 1);
  }
  if (menu_item_check(11))
  {
    setModeColor(11);
    print_small_intbar(11, 18, configuration.fx.delay2_sidechain_send, 11, 1, 1);
    print_small_intbar(23, 18, 100 - configuration.fx.delay2_sidechain_send, 99, 1, 1);
  }
  if (menu_item_check(12))
  {
    setModeColor(12);
    print_small_intbar(11, 19, configuration.fx.reverb_sidechain_send, 12, 1, 1);
    print_small_intbar(23, 19, 100 - configuration.fx.reverb_sidechain_send, 99, 1, 1);
  }

}

extern void set_sidechain_levels();

FLASHMEM void UI_func_sidechain(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();

    seq.temp_active_menu = 0;
    generic_temp_select_menu = 0;
    // seq.edit_state=false;
    display.fillScreen(COLOR_BACKGROUND);

    print_sidechain_static_texts();
    print_sidechain_editor_values();
    //print_sidechain_level_indicators();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned)
    {
      if (!seq.edit_state)
      {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir * 1, 0, 12);
      }

      else if (seq.edit_state)
      {

        if (generic_temp_select_menu == 0) // sidechain on/off
        {
          sidechain_active = !sidechain_active;
        }

        else  if (generic_temp_select_menu == 1) // sidechain sample
        {
          sidechain_sample_number = constrain(sidechain_sample_number + e.dir * 1, 0, NUM_DRUMSET_CONFIG - 1);
        }

        if (generic_temp_select_menu == 2) // sidechain speed
        {
          sidechain_speed = constrain(sidechain_speed + e.dir * e.speed, 0, 5000);
        }
        if (generic_temp_select_menu == 3) // sidechain 1/16 steps
        {
          sidechain_steps = constrain(sidechain_steps + e.dir * 1, 1, 16);
          sidechain_speed = uint16_t(60000 / seq.bpm / 4) * sidechain_steps;
        }

        else if (generic_temp_select_menu == 4) // dexed 0 target 
        {
          configuration.dexed[0].sidechain_send = constrain(configuration.dexed[0].sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 5) // dexed 1 target 
        {
          configuration.dexed[1].sidechain_send = constrain(configuration.dexed[1].sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 6) // epiano target 
        {
          configuration.epiano.sidechain_send = constrain(configuration.epiano.sidechain_send + e.dir * e.speed, 0, 100);
        }

        else if (generic_temp_select_menu == 7) // microsynth1 target 
        {
          microsynth[0].sidechain_send = constrain(microsynth[0].sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 8) // microsynth2 target 
        {
          microsynth[0].sidechain_send = constrain(microsynth[0].sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 9) // braids target 
        {
          braids_osc.sidechain_send = constrain(braids_osc.sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 10) // delay 1 target 
        {
          configuration.fx.delay1_sidechain_send = constrain(configuration.fx.delay1_sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 11) // delay 2 target 
        {
          configuration.fx.delay2_sidechain_send = constrain(configuration.fx.delay2_sidechain_send + e.dir * e.speed, 0, 100);
        }
        else if (generic_temp_select_menu == 12) // reverb target 
        {
          configuration.fx.reverb_sidechain_send = constrain(configuration.fx.reverb_sidechain_send + e.dir * e.speed, 0, 100);
        }
      }
    }

    if (LCDML.BT_checkEnter())
    {
      seq.edit_state = !seq.edit_state;

    } // buttons END

    print_sidechain_editor_values();
    //print_sidechain_level_indicators();
    set_sidechain_levels();

    //   if (seq.edit_state)
    //     helptext_r(F("SIDECHAIN TIME"));
    //   else
    //     helptext_r(F("< > SELECT INST"));
    //   display.setTextSize(2);

  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_smart_filter(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 1);
    display.print(F("Drm Smart Filter"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      seq.smartfilter = !seq.smartfilter;
    }

    setCursor_textGrid(1, 2);
    if (seq.smartfilter)
      display.print(F("[ON ]"));
    else
      display.print(F("[OFF]"));
  }

  // #ifdef DEBUG
  //   print_used_samples();
  // #endif

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
  }
}

uint8_t dexed_onscreen_algo = 0;

FLASHMEM void print_voice_select_fav_search_button()
{
  switch (configuration.sys.favorites) {
  case 0:
    TouchButton::drawButton(GRID.X[5], GRID.Y[2], "ALL", "PRESET", TouchButton::BUTTON_NORMAL);
    break;

  case 1:
    TouchButton::drawButton(GRID.X[5], GRID.Y[2], "FAVs.", "ONLY", TouchButton::BUTTON_NORMAL);
    break;

  case 2:
    TouchButton::drawButton(GRID.X[5], GRID.Y[2], "NON", "FAVs.", TouchButton::BUTTON_NORMAL);
    break;

  case 3:
    TouchButton::drawButton(GRID.X[5], GRID.Y[2], "RND", "NONFAV", TouchButton::BUTTON_NORMAL);
    break;
  }

}

FLASHMEM void draw_algo(bool forceDraw)
{
  const uint8_t algo = MicroDexed[selected_instance_id]->getAlgorithm();
  if (forceDraw || (dexed_onscreen_algo != algo)) {
    UI_draw_FM_algorithm(algo, 120, 90);
    dexed_onscreen_algo = algo;
  }
}

FLASHMEM void set_algo(uint8_t value)
{
  MicroDexed[selected_instance_id]->setAlgorithm(value);
}
FLASHMEM uint8_t get_algo()
{
  return MicroDexed[selected_instance_id]->getAlgorithm();
}

FLASHMEM void panic_dexed_current_instance()
{
  MicroDexed[selected_instance_id]->panic();
}

FLASHMEM void print_voice_settings_in_dexed_voice_select(bool fullrefresh_text, bool fullrefresh_values)
{
  char note_name[4];
  display.setTextSize(1);

  // static content
  if (fullrefresh_text)
  {
    display.setTextSize(2);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(2 * CHAR_width_small, 2 * CHAR_height_small + 3);
    display.print(F("B"));

    display.setCursor(2 * CHAR_width_small, 4 * CHAR_height_small + 5);
    display.print(F("V"));

    display.setTextSize(1);

    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(2 * CHAR_width_small, 6);
    display.print(F("POOL "));
    display.setCursor(CHAR_width_small * 22, 6);
    display.print(F("DEXED"));
    setCursor_textGrid_small(2, 6);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("VOLUME"));
    setCursor_textGrid_small(2, 7);
    display.print(F("TRANSP."));
    if (seq.cycle_touch_element != 1)
    {
      setCursor_textGrid_small(2, 9);
      display.print(F("PAN"));
      setCursor_textGrid_small(2, 10);
      display.print(F("LOW NOTE"));
      setCursor_textGrid_small(2, 11);
      display.print(F("HI NOTE"));
      setCursor_textGrid_small(2, 12);
      display.print(F("MIDI CH"));

      setCursor_textGrid_small(2, 13);
      display.print(F("VOICE"));
      setCursor_textGrid_small(16, 13);
      display.print(F("FX"));

      setCursor_textGrid_small(2, 14);
      display.print(F("CHORUS"));
      setCursor_textGrid_small(2, 15);
      display.print(F("DELAY1"));
      setCursor_textGrid_small(2, 16);
      display.print(F("DELAY2"));
      setCursor_textGrid_small(2, 17);
      display.print(F("REVERB"));

      display.console = true;
      display.drawLine(1, CHAR_height * 5 - 2, DISPLAY_WIDTH - 2, CHAR_height * 5 - 2, GREY4);
      display.drawLine(CHAR_width * 18, 1, CHAR_width * 18, CHAR_height * 5 - 2, GREY4);
    }
    setCursor_textGrid_small(22, 9);
    display.setTextSize(1);
    display.setTextColor(GREY1);
    if (seq.cycle_touch_element != 1)
    {
      display.print(F("ALGO"));
      print_voice_select_fav_search_button();
      TouchButton::drawButton(GRID.X[5], GRID.Y[3], "CHANGE", "ALGO", TouchButton::BUTTON_NORMAL);
      TouchButton::drawButton(GRID.X[5], GRID.Y[5] - 23, "EDIT", "VOICE", TouchButton::BUTTON_NORMAL);
      draw_algo();
    }
  }

  display.setTextSize(1);
  if (menu_item_check(0) || fullrefresh_values)
  {
    setModeColor(0);
    display.setCursor(CHAR_width_small * 7, 6);
    print_formatted_number(configuration.dexed[selected_instance_id].pool, 2, 0, 1);
  }
  if (menu_item_check(4) || fullrefresh_values)
  {
    print_small_intbar(11, 6, configuration.dexed[selected_instance_id].sound_intensity, 4, 1, 1);
  }
  if (menu_item_check(5) || menu_item_check(0) || menu_item_check(2) || menu_item_check(3) || fullrefresh_values)
  {
    setModeColor(5);
    setCursor_textGrid_small(11, 7);
    print_formatted_number_signed(configuration.dexed[selected_instance_id].transpose - 24, 2, 5, 1);
  }
  if (seq.cycle_touch_element != 1)
  {

    if (menu_item_check(6) || fullrefresh_values)
      print_small_panbar(11, 9, configuration.dexed[selected_instance_id].pan, 6);
    if (menu_item_check(7) || fullrefresh_values)
    {
      setModeColor(7);
      setCursor_textGrid_small(11, 10);
      print_formatted_number(configuration.dexed[selected_instance_id].lowest_note, 3, 7, 1);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      getNoteName(note_name, configuration.dexed[selected_instance_id].lowest_note);
      setCursor_textGrid_small(15, 10);
      display.print(note_name);
    }
    if (menu_item_check(8) || fullrefresh_values)
    {
      setModeColor(8);
      setCursor_textGrid_small(11, 11);
      print_formatted_number(configuration.dexed[selected_instance_id].highest_note, 3, 8, 1);
      getNoteName(note_name, configuration.dexed[selected_instance_id].highest_note);
      setCursor_textGrid_small(15, 11);
      display.setTextColor(GREY1, COLOR_BACKGROUND);
      display.print(note_name);
    }
    if (menu_item_check(9) || fullrefresh_values)
    {
      setCursor_textGrid_small(11, 12);
      setModeColor(9);
      _print_midi_channel(configuration.dexed[selected_instance_id].midi_channel);
#if defined GLOW
      set_glow_show_text_no_grid(11 * CHAR_width_small, 12 * (CHAR_height_small + 2), 4, get_midi_channel_name(configuration.dexed[selected_instance_id].midi_channel), 9, 1);
#endif
    }

    if (menu_item_check(10) || fullrefresh_values)
    {
      setModeColor(10);
      // setCursor_textGrid_small(2, 13);
      // display.print(F("VOICE"));
      show_no_grid(13 * (CHAR_height_small + 2), 2 * CHAR_width_small, 5, "VOICE", 10, 1);
    }
    if (menu_item_check(11) || fullrefresh_values)
    {
      setModeColor(11);
      // setCursor_textGrid_small(16, 13);
      // display.print(F("FX"));
      show_no_grid(13 * (CHAR_height_small + 2), 16 * CHAR_width_small, 2, "FX", 11, 1);
    }

    if (menu_item_check(12) || fullrefresh_values)
      print_small_intbar(9, 14, configuration.fx.chorus_level[selected_instance_id], 12, 1, 1);

    if (menu_item_check(13) || fullrefresh_values)
      print_small_intbar(9, 15, configuration.fx.delay_level1[selected_instance_id], 13, 1, 1);

    if (menu_item_check(14) || fullrefresh_values)
      print_small_intbar(9, 16, configuration.fx.delay_level2[selected_instance_id], 14, 1, 1);

    if (menu_item_check(15) || fullrefresh_values)
      print_small_intbar(9, 17, configuration.fx.reverb_send[selected_instance_id], 15, 1, 1);

    if (menu_item_check(16) || fullrefresh_values)
    {
      setModeColor(16);
      setCursor_textGrid_small(28, 6);
      print_formatted_number(seq.bpm, 3, 16, 1);
    }
  }
}

FLASHMEM void increment_dexed_instance_in_voice_select_round_robin()
{
  increment_dexed_instance_round_robin();
  print_voice_settings_in_dexed_voice_select(false, true);
  UI_update_instance_icons();
};

FLASHMEM void UI_update_instance_icons()
{
  display.setTextSize(1);

  for (uint8_t i = 0; i < NUM_DEXED; i++)
  {
    display.console = true;
    if (i == selected_instance_id)
      display.fillRect((13 + i) * CHAR_width + 10, 5, 9, 9, COLOR_SYSTEXT);
    else
      display.fillRect((13 + i) * CHAR_width + 10, 5, 9, 9, GREY2);

    display.setCursor((13 + i) * CHAR_width + 12, 6);

    if (configuration.dexed[i].polyphony == 0)
      display.setTextColor(RED, COLOR_SYSTEXT);
    else
      if (i == selected_instance_id)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
      else
        display.setTextColor(COLOR_BACKGROUND, GREY2);

    display.print(i + 1);

  }
  display.console = false;
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void printLiveModButtons(void) {
  for (uint8_t step = 0; step < 4; step++) {
    printLiveModButton(step);
  }
}

FLASHMEM void printLiveModButton(int8_t step) {
  const char* labels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
  if (seq.cycle_touch_element != 1) {
    char buf[5];
    snprintf(buf, 5, "%+03i", dexed_live_mod.live_adsr_values[selected_instance_id][step]);
    const TouchButton::Color c = (dexed_live_mod.active_button - 1) == step ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL;
    TouchButton::drawButton(GRID.X[step], GRID.Y[5] - 23, labels[step], buf, c);
  }
}

FLASHMEM void print_voice_select_default_help()
{
  if (seq.cycle_touch_element != 1) {
    helptext_l(back_text);
    helptext_r(F("< > SELECT VOICE"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(0, DISPLAY_HEIGHT - (CHAR_height_small * 2) - 2);
    display.print(F("LONG:"));
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
    if (seq.running)
      display.print(F("STOP SEQUENCER "));
    else
      display.print(F("START SEQUENCER"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.setCursor(CHAR_width_small * 25 + 2, DISPLAY_HEIGHT - (CHAR_height_small * 2) - 2);
    display.print(F("SHORT:"));
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
    if (seq.edit_state == 1)
      display.print(F("APPLY"));
    else if (seq.edit_state == 0)
      display.print(F(" EDIT"));
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F(" LONG:"));
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
    display.print(F("TOGGLE INST"));
  }
}

FLASHMEM void loadDexedOrigADSR(int instance) {
  for (int adsr = 0; adsr < 4; adsr++) {
    for (int op = 0; op < 6; op++) {
      const uint8_t rate = MicroDexed[instance]->getOPRate(op, adsr);
      dexed_live_mod.orig_adsr_values[instance][adsr][op] = rate;
    }
  }
}

FLASHMEM void UI_func_voice_select_touch_loop(EncoderEvents e)
{
  uint8_t step = dexed_live_mod.active_button - 1; // buttons 1-4 -> ADSR 0-3

  if (e.turned) {
    const int8_t valueTemp = constrain(dexed_live_mod.live_adsr_values[selected_instance_id][step] + e.dir * e.speed, -MAX_PERF_MOD, +MAX_PERF_MOD);
    if (valueTemp != dexed_live_mod.live_adsr_values[selected_instance_id][step]) {
      dexed_live_mod.live_adsr_values[selected_instance_id][step] = valueTemp;
      for (uint8_t i = 0; i < 6; i++) {
        MicroDexed[selected_instance_id]->setOPRate(i, step, dexed_live_mod.orig_adsr_values[selected_instance_id][step][i] - dexed_live_mod.live_adsr_values[selected_instance_id][step]);
      }
      printLiveModButton(step);
    }
  }

  else if (e.pressed) {
    dexed_live_mod.active_button = 0;
    printLiveModButton(step);
    print_voice_select_default_help();
  }
}

FLASHMEM void reset_live_modifier(uint8_t step)
{
  if (dexed_live_mod.live_adsr_values[selected_instance_id][step] != 0) {
    dexed_live_mod.live_adsr_values[selected_instance_id][step] = 0;
    for (int op = 0; op < 6; op++) {
      MicroDexed[selected_instance_id]->setOPRate(op, step, dexed_live_mod.orig_adsr_values[selected_instance_id][step][op]);
    }
    printLiveModButton(step);
  }
}

FLASHMEM void reset_live_modifiers()
{
  for (int step = 0; step < 4; step++) {
    reset_live_modifier(step);
  }
}

FLASHMEM void UI_func_voice_select_loop(EncoderEvents e) {

  bool changed = true;

  if (configuration.sys.favorites == 0) {
    int8_t tempValue = 0;

    // show all presets
    switch (generic_temp_select_menu) {
    case MENU_VOICE_BANK:
      tempValue = constrain(configuration.dexed[selected_instance_id].bank + e.dir * e.speed, 0, MAX_BANKS - 1);
      changed = (tempValue != configuration.dexed[selected_instance_id].bank);
      configuration.dexed[selected_instance_id].bank = tempValue;
      break;

    case MENU_VOICE_SOUND:
      tempValue = configuration.dexed[selected_instance_id].voice + e.dir;
      // check select previous bank
      if (tempValue < 0)
      {
        if (configuration.dexed[selected_instance_id].bank > 0)
        {
          tempValue = MAX_VOICES - 1;
          configuration.dexed[selected_instance_id].bank--;
        }
        else
        {
          changed = false;
          tempValue = 0;
        }
      }
      // check select next bank
      if (tempValue >= MAX_VOICES) {
        if (configuration.dexed[selected_instance_id].bank + 1 < MAX_BANKS) {
          tempValue = 0;
          configuration.dexed[selected_instance_id].bank++;
        }
        else {
          changed = false;
          tempValue = MAX_VOICES - 1;
        }
      }
      configuration.dexed[selected_instance_id].voice = tempValue;
      break;
    }
  }
  else {
    // filter favourites
    switch (configuration.sys.favorites) {
    case 1: // only Favs
      if (e.down) {
        locate_next_favorite();
      }
      else {
        locate_previous_favorite();
      }
      break;

    case 2: // only non-Favs
      if (e.down) {
        locate_next_non_favorite();
      }
      else {
        locate_previous_non_favorite();
      }
      break;

    case 3: // random non-Favs
      locate_random_non_favorite();
      break;
    }
  }
  if (changed) {
    load_sd_voice(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
    reset_live_modifiers();
  }
}

FLASHMEM void drawVoiceNameButton() {
  if (seq.cycle_touch_element != 1)
    TouchButton::drawButton(GRID.X[4], GRID.Y[5] - 23, "VOICE", "NAME", TouchButton::BUTTON_NORMAL);
}

// Helper function to save voice name and exit naming mode
FLASHMEM void save_voice_name_and_exit() {
  strncpy(g_voice_name[selected_instance_id], edit_string_global, 10);
  MicroDexed[selected_instance_id]->getName(g_voice_name[selected_instance_id]);

  // Now save to SD card
  save_sd_voice(configuration.dexed[selected_instance_id].pool,
    configuration.dexed[selected_instance_id].bank,
    configuration.dexed[selected_instance_id].voice,
    selected_instance_id);

  load_sd_voice(configuration.dexed[selected_instance_id].pool,
    configuration.dexed[selected_instance_id].bank,
    configuration.dexed[selected_instance_id].voice,
    selected_instance_id);

  // Exit naming mode
  seq.cycle_touch_element = 0;
  edit_mode_global = false;
  unregisterTouchHandler();
  virtual_typewriter_active = false;
  LCDML.OTHER_jumpToFunc(UI_func_voice_select);
}

FLASHMEM void UI_func_set_voice_name(uint8_t param)
{
  static uint8_t mode;
  static uint8_t ui_select_name_state;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    registerTouchHandler(handle_generic_virtual_typewriter_touch);
    mode = 0;
    setCursor_textGrid(1, 1);
    display.print(F("VOICE NAME"));
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    const EncoderEvents e = getEncoderEvents(ENC_R);

    if (e.turned) {
      if (mode == 1) {
        ui_select_name_state = UI_select_name(2, 2, 10, false);
      }
    }
    else if (e.pressed) {
      if (mode == 1) {
        ui_select_name_state = UI_select_name(2, 2, 10, false);
        if (ui_select_name_state == true)
        {
          mode = 0xff;

          save_voice_name_and_exit();
        }
      }
    }
    else if (mode == 0)
    {
      mode = 1;
      strcpy(edit_string_global, g_voice_name[selected_instance_id]);
      setCursor_textGrid(1, 2);
      display.print(F("[          ] "));
      ui_select_name_state = UI_select_name(2, 2, 10, true);
      virtual_typewriter_init(0);
      virtual_typewriter_active = true;
    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    seq.cycle_touch_element = 0;
    unregisterTouchHandler();
    virtual_typewriter_active = false;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_voice_select(uint8_t param)
{
  if (generic_active_function > 98)
    generic_active_function = 0;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_voice_select);
    registerScope(217, 40, 102, 48);
    dexed_onscreen_algo = 88; // dummy value to force draw on screen init
    display.fillScreen(COLOR_BACKGROUND);
    ts.fav_buttton_state = 0; //clear touch button state when starting page
    seq.cycle_touch_element = 0;
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver)) {
      generic_temp_select_menu = 3;
    }
    TouchButton::drawVirtualKeyboardButton(GRID.X[5], GRID.Y[0]);
    print_voice_settings_in_dexed_voice_select(true, true);
    print_voice_select_default_help();
    printLiveModButtons();
    drawVoiceNameButton();
    UI_update_instance_icons();
    draw_favorite_icon(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(22, 6);
    display.print(F("P#"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    setCursor_textGrid_small(32, 6);
    display.print(F("BPM"));
    setCursor_textGrid_small(22, 7);
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    print_formatted_number(configuration.sys.performance_number, 2);
    display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
    show_smallfont_noGrid(9 * CHAR_height_small - 2, CHAR_width_small * 25, 10, seq.name);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    virtual_keyboard_smart_preselect_mode();
    encoderDir[ENC_R].reset();
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);

    if (seq.cycle_touch_element == 2)  //voice name editor
    {
      ;
    }
    else  if (dexed_live_mod.active_button != 0) { // handle live modify buttons
      UI_func_voice_select_touch_loop(e);
    }

    // handle main entries
    else {
      if (e.turned) {
        bool changed = false;
        if (seq.edit_state == 0) {
          generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 16);
        }
        else if (seq.edit_state == 1) {
          switch (generic_temp_select_menu) {
          case 0: // pool select
            configuration.dexed[selected_instance_id].pool = constrain(configuration.dexed[selected_instance_id].pool + e.dir, 0, DEXED_POOLS - 1);
            if (load_sd_voice(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id) == false) {
              load_sd_voice(configuration.dexed[selected_instance_id].pool, 0, 0, selected_instance_id);
            }
            break;

          case 1: // dexed select
            if (e.down) {
              changed = increment_dexed_instance();
            }
            else if (e.up) {
              changed = decrement_dexed_instance();
            }
            if (changed) {
              print_voice_settings_in_dexed_voice_select(false, true);
              UI_update_instance_icons();
              printLiveModButtons();
              drawVoiceNameButton();
              draw_favorite_icon(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
              load_sd_voice(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
            }
            break;

          case 2: // bank select
          case 3: // voice select
            UI_func_voice_select_loop(e);
            break;

          case 4: // volume
            configuration.dexed[selected_instance_id].sound_intensity = constrain(configuration.dexed[selected_instance_id].sound_intensity + e.dir * e.speed, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX);
            MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 7, configuration.dexed[selected_instance_id].sound_intensity);
            MicroDexed[selected_instance_id]->setGain(midi_volume_transform(map(configuration.dexed[selected_instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127)));
            break;

          case 5: // transpose
            configuration.dexed[selected_instance_id].transpose = constrain(configuration.dexed[selected_instance_id].transpose + e.dir, TRANSPOSE_MIN, TRANSPOSE_MAX);
            MicroDexed[selected_instance_id]->setTranspose(configuration.dexed[selected_instance_id].transpose);
            MicroDexed[selected_instance_id]->notesOff();
            send_sysex_param(configuration.dexed[selected_instance_id].midi_channel, 144, configuration.dexed[selected_instance_id].transpose, 0);
            break;

          case 6: // panorama
            configuration.dexed[selected_instance_id].pan = constrain(configuration.dexed[selected_instance_id].pan + e.dir * e.speed, PANORAMA_MIN, PANORAMA_MAX);
            MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 10, map(configuration.dexed[selected_instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, 0, 127));
            dexed_mono2stereo[selected_instance_id]->panorama(mapfloat(configuration.dexed[selected_instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
            break;

          case 7: // lowest note
            configuration.dexed[selected_instance_id].lowest_note = constrain(configuration.dexed[selected_instance_id].lowest_note + e.dir * e.speed, INSTANCE_LOWEST_NOTE_MIN, INSTANCE_LOWEST_NOTE_MAX);
            break;

          case 8: // highest note
            configuration.dexed[selected_instance_id].highest_note = constrain(configuration.dexed[selected_instance_id].highest_note + e.dir * e.speed, INSTANCE_HIGHEST_NOTE_MIN, INSTANCE_HIGHEST_NOTE_MAX);
            break;

          case 9: // MIDI CHANNEL
            configuration.dexed[selected_instance_id].midi_channel = constrain(configuration.dexed[selected_instance_id].midi_channel + e.dir, MIDI_CHANNEL_MIN, MIDI_CHANNEL_MAX + 1);
            break;

          case 12: // chorus
            configuration.fx.chorus_level[selected_instance_id] = constrain(configuration.fx.chorus_level[selected_instance_id] + e.dir * e.speed, CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX);
            MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 93, configuration.fx.chorus_level[selected_instance_id]);
            dexed_chorus_mixer_r[selected_instance_id].gain(1, mapfloat(configuration.fx.chorus_level[selected_instance_id], CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX, 0.0, 0.5));
            dexed_chorus_mixer_l[selected_instance_id].gain(1, mapfloat(configuration.fx.chorus_level[selected_instance_id], CHORUS_LEVEL_MIN, CHORUS_LEVEL_MAX, 0.0, 0.5));
            break;

          case 13: // DELAY (1)
            configuration.fx.delay_level1[selected_instance_id] = constrain(configuration.fx.delay_level1[selected_instance_id] + e.dir * e.speed, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
            set_dexed_delay_level(0, selected_instance_id, configuration.fx.delay_level1[selected_instance_id]);
            break;

          case 14: // DELAY (2)
            configuration.fx.delay_level2[selected_instance_id] = constrain(configuration.fx.delay_level2[selected_instance_id] + e.dir * e.speed, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX);
            set_dexed_delay_level(1, selected_instance_id, configuration.fx.delay_level2[selected_instance_id]);
            break;

          case 15: // REVERB SEND
            configuration.fx.reverb_send[selected_instance_id] = constrain(configuration.fx.reverb_send[selected_instance_id] + e.dir * e.speed, REVERB_SEND_MIN, REVERB_SEND_MAX);
            MD_sendControlChange(configuration.dexed[selected_instance_id].midi_channel, 91, configuration.fx.reverb_send[selected_instance_id]);
            reverb_mixer_r.gain(selected_instance_id, volume_transform(mapfloat(configuration.fx.reverb_send[selected_instance_id], REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
            reverb_mixer_l.gain(selected_instance_id, volume_transform(mapfloat(configuration.fx.reverb_send[selected_instance_id], REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
            break;

          case 16: // BPM
            seq.bpm = constrain(seq.bpm + e.dir, 40, 240);
            update_seq_speed();
            break;
          }
        }
      }

      if (e.pressed) // handle button presses during menu >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
      {
        if (seq.cycle_touch_element == 0) {
          switch (generic_temp_select_menu) {
          case 10:
            LCDML.FUNC_setGBAToLastFunc();
            LCDML.OTHER_jumpToFunc(UI_func_dexed_audio);
            break;

          case 11:
            LCDML.FUNC_setGBAToLastFunc();
            LCDML.OTHER_jumpToFunc(UI_func_master_effects);
            break;

          default:
            seq.edit_state = !seq.edit_state;
            print_voice_select_default_help();
            break;
          }
        }
      }
    }


    if (seq.cycle_touch_element != 2) { // not in voice name editor

      if (e.longPressed && dexed_live_mod.active_button != 99)
      {
        increment_dexed_instance_in_voice_select_round_robin();
        if (seq.cycle_touch_element == 0) {
          virtual_keyboard_smart_preselect_mode();
        }
        draw_favorite_icon(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
        printLiveModButtons();
        drawVoiceNameButton();
      }

      if (seq.cycle_touch_element == 1 && generic_temp_select_menu > 5) {
        generic_temp_select_menu = 5;
      }

      if (menu_item_check(0))
      {
        setModeColor(0);
        display.setTextSize(1);
        display.setCursor(CHAR_width_small * 2, 6);
        display.print(F("POOL"));
      }

      if (menu_item_check(1))
      {
        setModeColor(1);
        display.setTextSize(1);
        // display.setCursor(CHAR_width_small * 22, 6);
        // display.print(F("DEXED"));
        show_no_grid(6, CHAR_width_small * 22, 5, "DEXED", 1, 1);
      }

      if (generic_temp_select_menu != 1)
      {
        draw_favorite_icon(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
      }
      display.setTextSize(2);
      display.setTextColor(GREY2, COLOR_BACKGROUND);
      display.setCursor(5 * CHAR_width_small, 2 * CHAR_height_small + 3);
      print_formatted_number(configuration.dexed[selected_instance_id].bank, 2);
      display.setCursor(5 * CHAR_width_small, 4 * CHAR_height_small + 5);
      print_formatted_number(configuration.dexed[selected_instance_id].voice + 1, 2);

      setModeColor(2);
      show_no_grid(2 * CHAR_height_small + 3, 11 * CHAR_width_small, 8, g_bank_name[selected_instance_id], 2, 2);

      setModeColor(3);
      show_no_grid(4 * CHAR_height_small + 5, 11 * CHAR_width_small, 10, g_voice_name[selected_instance_id], 3, 2);

      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      if (seq.cycle_touch_element != 1)
      {
        if (dexed_live_mod.active_button == 0) {
          draw_algo();
        }
      }

      print_voice_settings_in_dexed_voice_select(false, false);

    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();

    dexed_live_mod.active_button = 0;
  }
}

// ==================
// DEXED voice editor
// ==================

// a single dexed voice parameter definition
struct voice_param
{
  const char* name;
  uint8_t max;
};

// list of all dexed voice operator parameter definitions.
// actual parameters are repeating this six times plus the global voice parameters.
const struct voice_param voice_op_params[] = {
    {"R1", 99},
    {"R2", 99},
    {"R3", 99},
    {"R4", 99},
    {"L1", 99},
    {"L2", 99},
    {"L3", 99},
    {"L4", 99},
    {"LEV SCL BRK PT", 99},
    {"SCL LEFT DEPTH", 99},
    {"SCL RGHT DEPTH", 99},
    {"SCL LEFT CURVE", 3},
    {"SCL RGHT CURVE", 3},
    {"OSC RATE SCALE", 7},
    {"AMP MOD SENS", 3},
    {"KEY VEL SENS", 7},
    {"OUTPUT LEV", 99},
    {"OSC MODE", 1},
    {"FREQ COARSE", 31},
    {"FREQ FINE", 99},
    {"OSC DETUNE", 14} };
const uint8_t num_voice_op_params = 21;

// list of all dexed global voice parameter definitions.
const struct voice_param voice_params[] = {
    {"R1", 99},
    {"R2", 99},
    {"R3", 99},
    {"R4", 99},
    {"L1", 99},
    {"L2", 99},
    {"L3", 99},
    {"L4", 99},
    {"ALGORITHM", 31},
    {"FEEDBACK", 7},
    {"OSC KEY SYNC", 1},
    {"LFO SPEED", 99},
    {"LFO DELAY", 99},
    {"LFO PITCH MOD DEP", 99},
    {"LFO AMP MOD DEP", 99},
    {"LFO SYNC", 1},
    {"LFO WAVE", 4},
    {"LFO PTCH M. SENS", 7},
    {"TRANSPOSE", 48},
    {"NAME", 127} };
const uint8_t num_voice_params = 19; // omit name for now

uint8_t current_voice_op = 5; // currently selected operator for edits

// the dexed engine global voice parameter getter and setters.
// all parameter values are uint8_t values, starting with 0.
// They are defined in the dexed VoiceData array:
// -Operator parameters are stored six times repeated for all six operators.
//  They are load and stored to the engine depending on the current selected_instance_id.
// -Global voice parameters go after the operator values, starting with DEXED_VOICE_OFFSET.
//  They are load and stored to the engine depending on the current selected_instance_id and current_voice_op operator.
//

FLASHMEM void dexed_op_number_renderer(Editor* editor, bool refresh)
{
  uint8_t mode = editor->get();
  print_small_scaled_bar(editor->x, editor->y, mode + 1, 1, 6, editor->select_id, 1, NULL);
}
FLASHMEM void dexed_algo_number_renderer(Editor* editor, bool refresh)
{
  uint8_t mode = editor->get();
  print_small_scaled_bar(editor->x, editor->y, mode + 1, 0, 31, editor->select_id, 1, NULL);
}
FLASHMEM int16_t dexed_getter(Editor* param)
{
  int8_t addr = param->select_id - 1 < num_voice_params ? param->select_id - 1 + DEXED_VOICE_OFFSET : param->select_id - 2 - num_voice_params + current_voice_op * num_voice_op_params;
  return MicroDexed[selected_instance_id]->getVoiceDataElement(addr);
};
FLASHMEM void dexed_setter(Editor* param, int16_t value)
{
  uint8_t addr = param->select_id - 1 < num_voice_params ? param->select_id - 1 + DEXED_VOICE_OFFSET : param->select_id - 2 - num_voice_params + current_voice_op * num_voice_op_params;
  MicroDexed[selected_instance_id]->setVoiceDataElement(addr, value);
};

// allow switching the currently edited operator.
FLASHMEM int16_t dexed_op_getter(Editor* param)
{
  return 5 - current_voice_op;  //dexed operators seem to be reversed when comparing with dexed on pc
};
FLASHMEM void dexed_op_setter(Editor* param, int16_t value)
{
  current_voice_op = 5 - value; //dexed operators seem to be reversed when comparing with dexed on pc
  ui.draw_editors(true); // as half of the editors have changed when a different operator is selected.
};

// the dexed voice edior, showing an UI for all 144 parameters of the current voice
// as not all editors fit on screen, only the global parameters and one set of operator parameters is shown.
// The current operator can be selected to access all parameters.
FLASHMEM void UI_func_voice_editor(uint8_t param)
{

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    ui.reset();

    ui.setCursor(0, 1);

    // allow switching between mutliple instances (also by long button press)
    addInstanceEditor(&dexed_voice_name_renderer);

    // voice global parameters
    display.setTextSize(1);
    setCursor_textGrid_small(0, 4);

    display.print(F("PITCH EG"));
    // display PITCH EG editor in two columns to save space
    ui.setCursor(0, 5);
    for (uint8_t i = 0; i < 4; i++)
      ui.addEditor(voice_params[i].name, 0, voice_params[i].max, &dexed_getter, &dexed_setter);
    ui.setCursor(14, 5);
    for (uint8_t i = 4; i < 8; i++)
      ui.addEditor(voice_params[i].name, 0, voice_params[i].max, &dexed_getter, &dexed_setter);
    ui.setCursor(0, 9);

    ui.addEditor(voice_params[8].name, 0, voice_params[8].max, &dexed_getter, &dexed_setter, &dexed_algo_number_renderer);
    setCursor_textGrid_small(10, 9);
    display.print(F("ALGORITHM"));

    // display the remaining global editors
    for (uint8_t i = 9; i < num_voice_params; i++)
      ui.addEditor(voice_params[i].name, 0, voice_params[i].max, &dexed_getter, &dexed_setter);

    // operator parameter set selector
    setCursor_textGrid_small(37, 3);
    display.print(F("EDIT OPERATOR"));

    ui.setCursor(27, 3);
    ui.addEditor((const char*)F(""), 0, 5, dexed_op_getter, dexed_op_setter, &dexed_op_number_renderer);
    ui.enableLeftEncoderEditor(); // also select operator by left encoder

    setCursor_textGrid_small(31, 4);
    display.print(F("EG RATE"));
    setCursor_textGrid_small(45, 4);
    display.print(F("EG LEVEL"));

    // display OPERATOR EG editor in two columns to save space
    ui.setCursor(27, 5);
    for (uint8_t i = 0; i < 4; i++)
      ui.addEditor(voice_op_params[i].name, 0, voice_op_params[i].max, &dexed_getter, &dexed_setter);
    ui.setCursor(41, 5);
    for (uint8_t i = 4; i < 8; i++)
      ui.addEditor(voice_op_params[i].name, 0, voice_op_params[i].max, &dexed_getter, &dexed_setter);
    ui.setCursor(27, 9);
    // display the remaining operator editors
    for (uint8_t i = 8; i < num_voice_op_params; i++)
      ui.addEditor(voice_op_params[i].name, 0, voice_op_params[i].max, &dexed_getter, &dexed_setter);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    ui.clear();
    encoderDir[ENC_R].reset();
    dexed_live_mod.active_button = 0;
  }
}

// ======================
// Dexed controller setup
// ======================

// display modes a controller can interact
FLASHMEM void dexed_mode_renderer(Editor* editor, bool refresh)
{
  prepare_multi_options(editor, refresh);
  display.print("          ");
  setCursor_textGrid_small(editor->x, editor->y);
  uint8_t mode = editor->get();
  if (mode == 0)
    display.print("LINEAR");
  if (mode == 1)
    display.print("REV.LINEAR");
  if (mode == 2)
    display.print("DIRECT");
}

// display the targets a controller can be assigned to (multiple choice bit field)
FLASHMEM void dexed_assign_renderer(Editor* editor, bool refresh)
{
  prepare_multi_options(editor, refresh);
  display.print("          ");
  setCursor_textGrid_small(editor->x, editor->y);
  uint8_t mode = editor->get();
  if (mode & 1)
    display.print("PTH ");
  if (mode & 2)
    display.print("AMP ");
  if (mode & 4)
    display.print("EG");
}

// compare edited parameter location to a given one and send SysEx message if they match
FLASHMEM void send_sysex_if_changed(uint8_t id, uint8_t* valuePtr, uint8_t* changedValuePtr)
{
  if (valuePtr == changedValuePtr)
    send_sysex_param(configuration.dexed[selected_instance_id].midi_channel, id, *((uint8_t*)valuePtr), 2);
}

// apply changed controller values (all at once)
// SysEx messages are only send for the actual chaged parameter.
//

FLASHMEM void dexed_controller_setter(Editor* editor, int16_t value)
{

  // call base setter to store editor value into our dexed parameter storage.
  dexed_current_instance_setter(editor, value);

  // send all editor changes to dexed engine.
  MicroDexed[selected_instance_id]->setPBController(configuration.dexed[selected_instance_id].pb_range, configuration.dexed[selected_instance_id].pb_step);
  MicroDexed[selected_instance_id]->setMWController(configuration.dexed[selected_instance_id].mw_range, configuration.dexed[selected_instance_id].mw_assign, configuration.dexed[selected_instance_id].mw_mode);
  MicroDexed[selected_instance_id]->setFCController(configuration.dexed[selected_instance_id].fc_range, configuration.dexed[selected_instance_id].fc_assign, configuration.dexed[selected_instance_id].fc_mode);
  MicroDexed[selected_instance_id]->setBCController(configuration.dexed[selected_instance_id].bc_range, configuration.dexed[selected_instance_id].bc_assign, configuration.dexed[selected_instance_id].bc_mode);
  MicroDexed[selected_instance_id]->setATController(configuration.dexed[selected_instance_id].at_range, configuration.dexed[selected_instance_id].at_assign, configuration.dexed[selected_instance_id].at_mode);
  MicroDexed[selected_instance_id]->ControllersRefresh();

  // send SysEx only for the value actually named by editor value pointer
  // to make sure we don't spam around SysEx messages for unchanged values!
  send_sysex_if_changed(65, &configuration.dexed[selected_instance_id].pb_range, (uint8_t*)editor->value);
  send_sysex_if_changed(66, &configuration.dexed[selected_instance_id].pb_step, (uint8_t*)editor->value);
  send_sysex_if_changed(70, &configuration.dexed[selected_instance_id].mw_range, (uint8_t*)editor->value);
  send_sysex_if_changed(71, &configuration.dexed[selected_instance_id].mw_assign, (uint8_t*)editor->value);
  send_sysex_if_changed(72, &configuration.dexed[selected_instance_id].fc_range, (uint8_t*)editor->value);
  send_sysex_if_changed(73, &configuration.dexed[selected_instance_id].fc_assign, (uint8_t*)editor->value);
  send_sysex_if_changed(74, &configuration.dexed[selected_instance_id].bc_range, (uint8_t*)editor->value);
  send_sysex_if_changed(75, &configuration.dexed[selected_instance_id].bc_assign, (uint8_t*)editor->value);
  send_sysex_if_changed(76, &configuration.dexed[selected_instance_id].at_range, (uint8_t*)editor->value);
  send_sysex_if_changed(77, &configuration.dexed[selected_instance_id].at_assign, (uint8_t*)editor->value);
}

// UI page to configure and assign the plentyful controllers a dexed engine can get input from:
//   Pitch Bend wheel, Modulation wheel, Foot pedal controller, Breath Controller, After Touch Pressure.
// Each of them (except pitch bend) can be assigned to zero or more of the controller channels:
//   Pitch modulation, Amplitude modulation, EG bias (a static offset to the operator EG values)
// The range and mapping can be altered for every controller.
//
FLASHMEM void UI_func_dexed_controllers(uint8_t param)
{

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    ui.reset();
    ui.setCursor(1, 1);
    // allow switching which dexed instance to edit
    addInstanceEditor(&dexed_voice_name_renderer);

    ui.setCursor(1, 5);
    ui.printLn("PITCH BEND WHEEL");
    ui.addEditor("PB RANGE", PB_RANGE_MIN, PB_RANGE_MAX, &configuration.dexed[0].pb_range,
      &dexed_current_instance_getter, &dexed_controller_setter);
    ui.addEditor("PB STEP", PB_STEP_MIN, PB_STEP_MAX, &configuration.dexed[0].pb_step,
      &dexed_current_instance_getter, &dexed_controller_setter);

    ui.printLn("");
    ui.printLn("MODULATION WHEEL");
    ui.addEditor("MW RANGE", MW_RANGE_MIN, MW_RANGE_MAX, &configuration.dexed[0].mw_range, &dexed_current_instance_getter, &dexed_controller_setter);
    ui.addEditor("MW ASSIGN", MW_ASSIGN_MIN, MW_ASSIGN_MAX, &configuration.dexed[0].mw_assign, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_assign_renderer);
    ui.addEditor("MW MODE", MW_MODE_MIN, MW_MODE_MAX, &configuration.dexed[0].mw_mode, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_mode_renderer);
    ui.printLn("");

    ui.printLn("FOOT CONTROLLER");
    ui.addEditor("FC RANGE", FC_RANGE_MIN, FC_RANGE_MAX, &configuration.dexed[0].fc_range, &dexed_current_instance_getter, &dexed_controller_setter);
    ui.addEditor("FC ASSIGN", FC_ASSIGN_MIN, FC_ASSIGN_MAX, &configuration.dexed[0].fc_assign, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_assign_renderer);
    ui.addEditor("FC MODE", FC_MODE_MIN, FC_MODE_MAX, &configuration.dexed[0].fc_mode, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_mode_renderer);

    ui.setCursor(29, 5);
    ui.printLn("BREATH CONTROLLER");

    ui.addEditor("BC RANGE", BC_RANGE_MIN, BC_RANGE_MAX, &configuration.dexed[0].bc_range, &dexed_current_instance_getter, &dexed_controller_setter);
    ui.addEditor("BC ASSIGN", BC_ASSIGN_MIN, BC_ASSIGN_MAX, &configuration.dexed[0].bc_assign, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_assign_renderer);
    ui.addEditor("BC MODE", BC_MODE_MIN, BC_MODE_MAX, &configuration.dexed[0].bc_mode, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_mode_renderer);

    ui.printLn("");
    ui.printLn("AFTERTOUCH");

    ui.addEditor("AT RANGE", AT_RANGE_MIN, AT_RANGE_MAX, &configuration.dexed[0].at_range, &dexed_current_instance_getter, &dexed_controller_setter);
    ui.addEditor("AT ASSIGN", AT_ASSIGN_MIN, AT_ASSIGN_MAX, &configuration.dexed[0].at_assign, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_assign_renderer);
    ui.addEditor("AT MODE", AT_MODE_MIN, AT_MODE_MAX, &configuration.dexed[0].at_mode, &dexed_current_instance_getter, &dexed_controller_setter, &dexed_mode_renderer);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    ui.clear();
  }
}

FLASHMEM void UI_func_volume(uint8_t param)
{
  static uint8_t old_volume;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_menu);
    registerScope(230, 18, 87, 64);
    old_volume = configuration.sys.vol;
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    encoderDir[ENC_L].reset();
    back_from_volume = 0;

    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 1);
    display.print(F("Master Volume"));

    if (multiband_active)
    {
      multiband_active = false;
      display.setTextSize(2);
      setCursor_textGrid(1, 4);
      display.setTextColor(RED, COLOR_BACKGROUND);
      display.print(F("MULTIBAND"));
      setCursor_textGrid(1, 5);
      display.print(F("DEACTIVATED"));
      display.setTextSize(2);
      //swap multiband test
      mb_set_master();
      // mb_set_mutes();
      // mb_set_compressor();
    }
    if (LCDML.MENU_getLastActiveFunctionID() == 0xff && ts.keyb_in_menu_activated == false)
      TouchButton::drawButton(GRID.X[4], GRID.Y[5], "MULTI", "BAND", multiband_active ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_L);
    if (e.turned) {
      back_from_volume = 0;
      const uint8_t volChange = (e.speed > 5) ? e.speed / 2 : 1;
      configuration.sys.vol = constrain(configuration.sys.vol + e.dir * volChange, VOLUME_MIN, VOLUME_MAX);
    }
    else {
      const EncoderEvents e = getEncoderEvents(ENC_R);
      if (e.turned || e.pressed) {
        back_from_volume = BACK_FROM_VOLUME_MS;
      }
    }

    // Master Volume
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    // setCursor_textGrid(1, 1);
    // display.print(F("Master Volume"));
    display_bar_int("Master Vol.", configuration.sys.vol, 1.0, VOLUME_MIN, VOLUME_MAX, 3, false, false, false);
    set_volume(configuration.sys.vol, configuration.sys.mono);
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    unregisterScope();
    encoderDir[ENC_L].reset();
    if (old_volume != configuration.sys.vol)
    {
      save_sys_flag = true;
      save_sys = SAVE_SYS_MS / 2;
    }
  }
}

FLASHMEM void _print_instances_in_save_voice()
{
  for (uint8_t i = 0; i < NUM_DEXED; i++)
  {
    if (i == selected_instance_id)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      display.setTextColor(GREY2, COLOR_BACKGROUND);

    // Display instance number
    setCursor_textGrid(1 + (i * 5), 2);
    display.print(i + 1);

    // Display voice name below instance number (using smaller text)
    display.setTextSize(1); // Smaller text for voice names

    // Position voice name below instance number
    setCursor_textGrid(1 + (i * 5), 3);

    // Truncate voice name if too long for display
    char display_name[10];
    strncpy(display_name, g_voice_name[i], 9);
    display_name[8] = '\0';

    // Handle empty voice names
    if (strlen(g_voice_name[i]) == 0) {
      strcpy(display_name, "---");
    }

    display.print(display_name);

    // Reset text size for other elements
    display.setTextSize(2);
  }
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}

FLASHMEM void UI_func_save_voice(uint8_t param)
{
  static bool yesno;
  static uint8_t mode;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    display.console = true;
    display.fillRect(CHAR_width * 18, 0, 9 * CHAR_width, CHAR_height * 5, COLOR_BACKGROUND);

    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    yesno = false;
    mode = 0;

    setCursor_textGrid(1, 1);
    display.print(F("Save Instance"));
    _print_instances_in_save_voice();

    helptext_r(F("< > SELECT INSTANCE"));
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      switch (mode)
      {
      case 0: // Instance selection
        if (e.down) {
          increment_dexed_instance();
        }
        if (e.up) {
          decrement_dexed_instance();
        }
        _print_instances_in_save_voice();
        break;

      case 1: // Bank selection
        configuration.dexed[selected_instance_id].bank = constrain(configuration.dexed[selected_instance_id].bank + e.dir * e.speed, 0, MAX_BANKS - 1);

        // get bank name from sysex on SD
        get_bank_name(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, g_bank_name[selected_instance_id]);

        show(2, 1, 2, configuration.dexed[selected_instance_id].bank);
        show(2, 5, 10, g_bank_name[selected_instance_id]);
        break;

      case 2: // Voice selection
        configuration.dexed[selected_instance_id].voice = constrain(configuration.dexed[selected_instance_id].voice + e.dir * e.speed, 0, MAX_VOICES - 1);

        // Get the destination voice name (what will be overwritten) using a temporary variable
        char dest_voice_name[11];
        get_voice_name(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, dest_voice_name);

        show(2, 1, 2, configuration.dexed[selected_instance_id].voice + 1);
        show(2, 5, 10, dest_voice_name);  // Show destination voice name, not source
        break;

      case 3: // Yes/No selection
        yesno = !yesno;
        show(2, 2, 3, yesno ? "YES" : "NO");
        break;
      }
    }
    else if (e.pressed)
    {
      mode++;
      switch (mode)
      {
      case 1:
        setCursor_textGrid(1, 1);
        display.print(F("Save to Bank "));
        show(2, 1, 2, configuration.dexed[selected_instance_id].bank);
        show(2, 5, 10, g_bank_name[selected_instance_id]);
        show(2, 3, 2, " [");
        show(2, 15, 3, "]  ");
        display.console = true;
        display.fillRect(CHAR_width * 1, CHAR_height * 3, 20 * CHAR_width, CHAR_height * 1, COLOR_BACKGROUND);
        helptext_r(F("< > SELECT DESTINATION BANK"));
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        break;

      case 2:
        show(1, 1, 14, "Save to Voice ");
        // show(1, 16, 2, configuration.dexed[selected_instance_id].bank);
        show(2, 1, 2, configuration.dexed[selected_instance_id].voice + 1);
        show(2, 5, 10, g_voice_name[selected_instance_id]);
        helptext_r(F("< > SELECT DESTINATION VOICE"));
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        break;

      case 3:
        show(1, 1, 16, "Overwrite?");
        show(2, 1, 15, "[NO");
        show(2, 5, 1, "]");
        break;
      default:
        if (yesno == true)
        {
#ifdef DEBUG
          bool ret = save_sd_voice(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
          if (ret == true)
            LOG.println(F("Saving voice OK."));
          else
            LOG.println(F("Error while saving voice."));
#else
          save_sd_voice(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice, selected_instance_id);
#endif
          show(2, 1, 16, "Done.");
          delay(MESSAGE_WAIT_TIME);
        }
        mode = 0xff;
        LCDML.FUNC_goBackToMenu();
        break;
      }
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    if (mode < 0xff)
    {
      _cancel_message_old_menus();
    }
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_sysex_receive_bank(uint8_t param)
{
  static bool yesno;
  static uint8_t mode;

  static uint8_t ui_select_name_state;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    yesno = false;
    mode = 0;
    sysex_pool_number = configuration.dexed[selected_instance_id].pool;
    sysex_bank_number = configuration.dexed[selected_instance_id].bank;
    memset(receive_bank_filename, 0, sizeof(receive_bank_filename));
    registerTouchHandler(handle_generic_virtual_typewriter_touch);
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 1);
    display.print(F("MIDI Recv Bank"));
    setCursor_textGrid(3, 2);
    display.print(F("["));
    setCursor_textGrid(15, 2);
    display.print(F("]"));
    get_bank_name(configuration.dexed[selected_instance_id].pool, sysex_bank_number, tmp_bank_name);
    strcpy(receive_bank_filename, tmp_bank_name);

    show(2, 1, 2, sysex_bank_number);
    show(2, 4, 10, receive_bank_filename);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      switch (mode) {
      case 0:
        sysex_pool_number = configuration.dexed[selected_instance_id].pool;
        sysex_bank_number = constrain(sysex_bank_number + e.dir * e.speed, 0, MAX_BANKS - 1);
        get_bank_name(configuration.dexed[selected_instance_id].pool, sysex_bank_number, tmp_bank_name);
        strcpy(receive_bank_filename, tmp_bank_name);
        show(2, 1, 2, sysex_bank_number);
        show(2, 4, 10, receive_bank_filename);
        break;

      case 1:
        yesno = !yesno;
        show(2, 13, 3, yesno ? "YES" : "NO");
        break;

      case 2:
        ui_select_name_state = UI_select_name(2, 2, BANK_NAME_LEN - 1, false);
        break;
      }
    }

    if (e.pressed) {
      if (mode == 0)
      {
        if (strcmp(receive_bank_filename, "*ERROR*") != 0)
        {
          yesno = true;
          strcpy(receive_bank_filename, "NONAME");
          mode = 2;
          setCursor_textGrid(1, 2);
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          display.print(F("[          ]    "));
          ui_select_name_state = UI_select_name(2, 2, BANK_NAME_LEN - 1, true);
        }
        else
        {
          mode = 1;
          setCursor_textGrid(1, 2);
          display.print(F("Overwrite: [NO ]"));
        }
      }
      else if (mode == 1 && yesno == true)
      {
        mode = 2;
        setCursor_textGrid(1, 2);
        // display.print(F("[          ]    "));
        ui_select_name_state = UI_select_name(2, 2, BANK_NAME_LEN - 1, true);
      }
      else if (mode == 2)
      {
        ui_select_name_state = UI_select_name(2, 2, BANK_NAME_LEN - 1, false);

        if (ui_select_name_state == true)
        {
          if (yesno == true)
          {

            char tmp[18];
            strcpy(tmp, edit_string_global);
            // sprintf(receive_bank_filename, "/%s/%d/%d/%s.syx", DEXED_CONFIG_PATH, sysex_pool_number, sysex_bank_number, tmp);
            sprintf(receive_bank_filename, "%s.syx", tmp);
            virtual_typewriter_active = false;

            mode = 0xff;
            setCursor_textGrid(1, 2);
            display.print(F("Waiting...         "));
            /// Storing is done in SYSEX code
          }
        }
      }
      else if (mode >= 1 && yesno == false)
      {
        LOG.println(mode, DEC);
        memset(receive_bank_filename, 0, sizeof(receive_bank_filename));
        mode = 0xff;
        _cancel_message_old_menus();
        LCDML.FUNC_goBackToMenu();
      }
    }

    if (mode == 0)
    {
      strcpy(edit_string_global, receive_bank_filename);
    }

    if (mode == 2 && virtual_typewriter_active == false)
    {
      virtual_typewriter_init(0);
      virtual_typewriter_active = true;
    }

    encoderDir[ENC_R].reset();
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    memset(receive_bank_filename, 0, sizeof(receive_bank_filename));
    unregisterTouchHandler();
    virtual_typewriter_active = false;
    display.fillScreen(COLOR_BACKGROUND);
    if (mode < 0xff)
    {
      _cancel_message_old_menus();
    }
  }
}

FLASHMEM void UI_func_set_performance_name(uint8_t param)
{
  static uint8_t mode;
  static uint8_t ui_select_name_state;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    registerTouchHandler(handle_generic_virtual_typewriter_touch);
    mode = 0;
    setCursor_textGrid(1, 1);
    display.print(F("PERFORMANCE NAME"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (mode == 1) {
        ui_select_name_state = UI_select_name(2, 2, 10, false);
      }
    }
    else if (e.pressed) {
      if (mode == 1) {
        ui_select_name_state = UI_select_name(2, 2, 10, false);
        if (ui_select_name_state == true)
        {
          mode = 0xff;
          update_performance_name();
        }
      }
    }

    else if (mode == 0)
    {
      mode = 1;
      strcpy(edit_string_global, seq.name);
      setCursor_textGrid(1, 2);
      display.print(F("[          ]    "));
      ui_select_name_state = UI_select_name(2, 2, 10, true);
      virtual_typewriter_init(0);
      virtual_typewriter_active = true;
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    virtual_typewriter_active = false;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_set_multisample_name(uint8_t param)
{
  static uint8_t mode;
  static uint8_t ui_select_name_state;
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    registerTouchHandler(handle_generic_virtual_typewriter_touch);

    display.fillScreen(COLOR_BACKGROUND);
    display.setTextSize(2);
    mode = 0;
    setCursor_textGrid(1, 1);

    display.print(F("MULTISAMPLE NAME"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (mode == 0) {
        seq.active_multisample = constrain(seq.active_multisample + e.dir * e.speed, 0, NUM_MULTISAMPLES - 1);
      }
      else if (mode == 2) {
        ui_select_name_state = UI_select_name(2, 4, BANK_NAME_LEN - 1, false);
      }
    }

    else if (e.pressed) {
      if (mode == 2)
      {
        ui_select_name_state = UI_select_name(2, 4, BANK_NAME_LEN - 1, false);
        if (ui_select_name_state == true)
        {
          mode = 0xff;
          update_multisample_name();
        }

      }
      if (mode == 0) {
        mode = 1;
      }
    }
    if (mode == 0)
    {
      setCursor_textGrid(1, 2);
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      print_formatted_number(seq.active_multisample, 2);
      setCursor_textGrid(3, 2);
      display.print(F("["));
      setCursor_textGrid(15, 2);
      display.print(F("]"));
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      show(2, 4, 11, msp[seq.active_multisample].name);
    }
    if (mode == 1)
    {
      mode = 2;
      strcpy(edit_string_global, msp[seq.active_multisample].name);
      setCursor_textGrid(3, 2);
      ui_select_name_state = UI_select_name(2, 4, BANK_NAME_LEN - 1, true);
      setCursor_textGrid(15, 2);
      display.print(F("]"));
      virtual_typewriter_init(0);
      virtual_typewriter_active = true;

    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    display.fillScreen(COLOR_BACKGROUND);
    virtual_typewriter_active = false;
    encoderDir[ENC_R].reset();
  }
}

FLASHMEM void UI_func_sysex_send_bank(uint8_t param)
{
  static uint8_t pool_number;
  static uint8_t bank_number;
  static uint8_t mode;
  static uint8_t sysex_send_channel;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    pool_number = configuration.dexed[selected_instance_id].pool;
    bank_number = configuration.dexed[selected_instance_id].bank;
    mode = 0;
    sysex_send_channel = 1;
    strcpy(tmp_bank_name, g_bank_name[selected_instance_id]);
    setCursor_textGrid(1, 1);
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("MIDI Send Bank"));
    show(2, 3, 1, "[");
    show(2, 15, 1, "]");
    show(2, 1, 2, configuration.dexed[selected_instance_id].bank);
    show(2, 4, 10, tmp_bank_name);
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      switch (mode)
      {
      case 0: // Bank selection
        bank_number = constrain(bank_number + e.dir, 0, MAX_BANKS - 1);
        get_bank_name(configuration.dexed[selected_instance_id].pool, bank_number, tmp_bank_name);
#ifdef DEBUG
        LOG.printf_P(PSTR("send bank sysex %d - bank:[%s]\n"), bank_number, tmp_bank_name);
#endif
        show(2, 1, 2, bank_number);
        show(2, 4, 10, tmp_bank_name);
        break;

      case 1: // Channel selection
        sysex_send_channel = constrain(sysex_send_channel + e.dir, 1, 16);
        show(2, 1, 16, "Channel");
        show(2, 12, 2, sysex_send_channel);
        break;
      }
    }

    if (e.pressed) {

      mode++;
      switch (mode)
      {
      case 1:
        show(2, 1, 16, "Channel");
        show(2, 12, 2, sysex_send_channel);
        helptext_r(F("< > SELECT CHANNEL"));
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        break;

      case 2:
        if (strcmp("*ERROR*", tmp_bank_name) != 0)
        {
          char filename[FILENAME_LEN + 10];
          snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%d/%s.syx"), DEXED_CONFIG_PATH, pool_number, bank_number, tmp_bank_name);
#ifdef DEBUG
          LOG.print(F("Send bank "));
          LOG.print(filename);
          LOG.println(F(" from SD."));
#endif
          File sysex = SD.open(filename);
          if (!sysex)
          {
#ifdef DEBUG
            LOG.println(F("Cannot read from SD."));
#endif
            show(2, 1, 16, "Read error.");
            bank_number = 0xff;
          }
          else
          {
            uint8_t bank_data[4104];
            sysex.read(bank_data, 4104);
            sysex.close();

            show(2, 1, 16, "Sending Ch");
            show(2, 12, 2, sysex_send_channel);
            send_sysex_bank(sysex_send_channel, bank_data);

            show(2, 1, 16, "Done.");
            bank_number = 0xff;
          }
        }
        else
        {
          show(2, 1, 16, "No bank.");
          bank_number = 0xff;
        }
        mode = 0xff;
        delay(MESSAGE_WAIT_TIME);
        LCDML.FUNC_goBackToMenu();
      }
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();

    if (bank_number < 0xff)
    {
      _cancel_message_old_menus();
    }
  }
}

FLASHMEM void UI_func_sysex_send_voice(uint8_t param)
{
  static uint8_t mode;
  static uint8_t pool_number;
  static uint8_t bank_number;
  static uint8_t voice_number;
  static uint8_t sysex_send_channel;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    clear_bottom_half_screen_without_backbutton();
    //The back button should be there automatically but it is not, need further check
    //TouchButton::drawButton(GRID.X[0], GRID.Y[5], "GO", back_text, TouchButton::BUTTON_NORMAL);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    mode = 0;
    pool_number = configuration.dexed[selected_instance_id].pool;
    bank_number = configuration.dexed[selected_instance_id].bank;
    voice_number = configuration.dexed[selected_instance_id].voice;
    sysex_send_channel = 1;

    strcpy(tmp_bank_name, g_bank_name[selected_instance_id]);
    strcpy(tmp_voice_name, g_voice_name[selected_instance_id]);
    MicroDexed[selected_instance_id]->getName(tmp_voice_name);
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 1);
    display.print(F("MIDI Send Voice"));
    show(2, 1, 2, bank_number);
    show(2, 5, 10, g_bank_name[selected_instance_id]);
    show(2, 4, 1, "[");
    show(2, 15, 1, "]");
    helptext_r(F("< > SELECT BANK"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      switch (mode)
      {
      case 0: // Bank selection
        bank_number = constrain(bank_number + e.dir, 0, MAX_BANKS - 1);
        get_bank_name(pool_number, bank_number, tmp_bank_name);
        show(2, 1, 2, bank_number);
        show(2, 5, 10, tmp_bank_name);
        get_voice_name(pool_number, bank_number, voice_number, tmp_voice_name);
        break;

      case 1: // Voice selection
        voice_number = constrain(voice_number + e.dir, 0, MAX_VOICES - 1);
        get_voice_name(pool_number, bank_number, voice_number, tmp_voice_name);
        MicroDexed[selected_instance_id]->getName(tmp_voice_name);
        show(2, 1, 2, voice_number + 1);
        show(2, 5, 10, tmp_voice_name);
        break;

      case 2: // Channel selection
        sysex_send_channel = constrain(sysex_send_channel + e.dir, 1, 16);
        show(2, 1, 16, "Channel");
        show(2, 12, 2, sysex_send_channel);
        break;
      }
    }
    if (e.pressed) {
      mode++;

      switch (mode) {
      case 1:
        show(2, 1, 2, voice_number + 1);
        show(2, 5, 10, tmp_voice_name);
        helptext_r(F("< > SELECT VOICE"));
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        break;
      case 2:
        show(2, 1, 16, "Channel");
        show(2, 12, 2, sysex_send_channel);
        helptext_r(F("< > SELECT CHANNEL"));
        display.setTextSize(2);
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        break;
      case 3:
        if (strcmp("*ERROR*", tmp_bank_name) != 0)
        {
          char filename[FILENAME_LEN + 10];
          snprintf(filename, sizeof(filename), "/%s/%d/%d/%s.syx", DEXED_CONFIG_PATH, pool_number, bank_number, tmp_bank_name);
#ifdef DEBUG
          LOG.print(F("Send voice "));
          LOG.print(voice_number);
          LOG.print(F(" of "));
          LOG.print(filename);
          LOG.println(F(" from SD."));
#endif
          File sysex = SD.open(filename);
          if (!sysex)
          {
#ifdef DEBUG
            LOG.println(F("Cannot read from SD."));
#endif
            show(2, 1, 16, "Read error.");
            bank_number = 0xff;
          }
          else
          {
            uint8_t voice_data[155];
            uint8_t encoded_voice_data[128];

            sysex.seek(6 + (voice_number * 128));
            sysex.read(encoded_voice_data, 128);

            MicroDexed[selected_instance_id]->decodeVoice(voice_data, encoded_voice_data);

            show(2, 1, 16, "Sending Ch");
            show(2, 12, 2, sysex_send_channel);
            send_sysex_voice(sysex_send_channel, voice_data);

            delay(MESSAGE_WAIT_TIME);
            show(2, 1, 16, "Done.");
            sysex.close();

            bank_number = 0xff;
          }
        }
        else
        {
          show(2, 1, 16, "No voice.");
          bank_number = 0xff;
        }
        mode = 0xff;
        delay(MESSAGE_WAIT_TIME);
        LCDML.FUNC_goBackToMenu();
        break;
      }
    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    if (mode < 0xff)
    {
      _cancel_message_old_menus();
    }
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_startup_performance(uint8_t param)
{
  bool stored = false;
  static uint8_t old_load_at_startup_performance;

  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.setTextSize(2);
    clear_bottom_half_screen_without_backbutton();
    helptext_r(F("< > SELECT PERFORMANCE"));
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    old_load_at_startup_performance = configuration.sys.load_at_startup_performance;
    encoderDir[ENC_R].reset();
    show(1, 1, 16, "Load at startup");
    if (configuration.sys.load_at_startup_performance == 255)
      show(2, 1, 16, "Last Performance");
    else if (configuration.sys.load_at_startup_performance <= PERFORMANCE_NUM_MAX)
    {
      show(2, 1, 16, "Fixed Perf. [");
      setCursor_textGrid(14, 2);
      print_formatted_number(configuration.sys.load_at_startup_performance, 2);
      show(2, 16, 1, "]");
    }
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.down)
    {
      if (configuration.sys.load_at_startup_performance == 255)
        configuration.sys.load_at_startup_performance = PERFORMANCE_NUM_MIN;
      else if (configuration.sys.load_at_startup_performance >= 0 && configuration.sys.load_at_startup_performance <= PERFORMANCE_NUM_MAX)
        configuration.sys.load_at_startup_performance++;
      if (configuration.sys.load_at_startup_performance > PERFORMANCE_NUM_MAX)
        configuration.sys.load_at_startup_performance = 255;
    }
    else if (e.up)
    {
      if (configuration.sys.load_at_startup_performance == 255)
        configuration.sys.load_at_startup_performance = PERFORMANCE_NUM_MAX;
      else if (configuration.sys.load_at_startup_performance >= PERFORMANCE_NUM_MIN && configuration.sys.load_at_startup_performance <= PERFORMANCE_NUM_MAX)
        configuration.sys.load_at_startup_performance--;
    }
    else if (e.pressed)
    {
      stored = true;
      show(2, 1, 16, "Done.");
      save_sd_sys_json();
      if (configuration.sys.load_at_startup_performance <= PERFORMANCE_NUM_MAX && configuration.sys.load_at_startup_performance != configuration.sys.performance_number)
        load_sd_performance_json(configuration.sys.load_at_startup_performance);
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }

    display.setCursor(1, 2);

    if (configuration.sys.load_at_startup_performance <= PERFORMANCE_NUM_MAX)
    {
      show(2, 1, 16, "Fixed Perf. [");
      setCursor_textGrid(14, 2);
      print_formatted_number(configuration.sys.load_at_startup_performance, 2);
      show(2, 16, 1, "]");
      display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
      if (check_sd_performance_exists(configuration.sys.load_at_startup_performance))
      {
        get_sd_performance_name_json(configuration.sys.load_at_startup_performance);
        if (seq.name_temp[0] != 0)
          show(3, 1, 16, seq.name_temp);
        else
          show(3, 1, 16, "----  DATA  ----");
      }
      else
        show(3, 1, 16, "-- EMPTY SLOT --");
    }
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    if (stored == false)
    {
      _cancel_message_old_menus();
      configuration.sys.load_at_startup_performance = old_load_at_startup_performance;
    }
    encoderDir[ENC_R].reset();
    helptext_r("");
  }
}

FLASHMEM void UI_func_startup_page(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    encoderDir[ENC_R].reset();
    clear_bottom_half_screen_without_backbutton();
    helptext_r(F("< > SELECT PAGE"));
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(0, 0);
    show(1, 1, 16, "Startup Page");
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    display.setTextSize(2);

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.down) {
      configuration.sys.load_at_startup_page++;
      if (configuration.sys.load_at_startup_page > 12)
        configuration.sys.load_at_startup_page = 50; // System Info Page
    }
    else if (e.up) {
      if (configuration.sys.load_at_startup_page > 0)
      {
        if (configuration.sys.load_at_startup_page == 50) // System Info Page
          configuration.sys.load_at_startup_page = 12;
        else
          configuration.sys.load_at_startup_page--;
      }
    }
    else if (e.pressed) {
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      save_sd_sys_json();
      show(2, 1, 18, "Done.");
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }
    display.setTextColor(COLOR_PITCHSMP, COLOR_BACKGROUND);
    if (configuration.sys.load_at_startup_page == 0)
      show(2, 1, 17, "DX Voice Select");
    else if (configuration.sys.load_at_startup_page == 1)
      show(2, 1, 17, "Song");
    else if (configuration.sys.load_at_startup_page == 2)
      show(2, 1, 17, "Pattern Editor");
    else if (configuration.sys.load_at_startup_page == 3)
      show(2, 1, 17, "MicroSynth");
    else if (configuration.sys.load_at_startup_page == 4)
      show(2, 1, 17, "Tracker");
    else if (configuration.sys.load_at_startup_page == 5)
      show(2, 1, 17, "MultiSample");
    else if (configuration.sys.load_at_startup_page == 6)
      show(2, 1, 17, "EPiano");
    else if (configuration.sys.load_at_startup_page == 7)
      show(2, 1, 17, "Braids");
    else if (configuration.sys.load_at_startup_page == 8)
      show(2, 1, 17, "Master Mixer");
    else if (configuration.sys.load_at_startup_page == 9)
      show(2, 1, 17, "Live Sequencer");
    else if (configuration.sys.load_at_startup_page == 10)
      show(2, 1, 17, "Main Menu     ");
    else if (configuration.sys.load_at_startup_page == 11)
      show(2, 1, 17, "Chord Arranger");
    else if (configuration.sys.load_at_startup_page == 12)
      show(2, 1, 17, "NoiseMaker");
    else if (configuration.sys.load_at_startup_page == 50)
      show(2, 1, 17, "System Info  ");
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    helptext_r("");
    display.setTextSize(2);
  }
}

extern uint8_t vtw_x;

FLASHMEM bool UI_select_name(uint8_t y, uint8_t x, uint8_t len, bool init)
{
  static uint8_t edit_value;
  static uint8_t last_char_pos;

  vtw_x = x;

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  if (init) {
    edit_mode_global = false;
    edit_pos_global = 0;

    edit_value = search_accepted_char(edit_string_global[edit_pos_global]);
    last_char_pos = strlen(edit_string_global);
    string_trim(edit_string_global); // just to be sure
    display.setTextSize(2);
    display.setTextColor(GREY3, COLOR_BACKGROUND);
    setCursor_textGrid(x + len + 3, y);
    display.print(" OK ");
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(x, y);
    edit_len_global = len;
    //virtual_typewriter_update_display();
    return false;
  }


  // Encoder handling
  if (LCDML.BT_checkDown() || LCDML.BT_checkUp()) {

    if (LCDML.BT_checkDown())
    {
      if (edit_mode_global == true)
      {
        edit_value = search_accepted_char(edit_string_global[edit_pos_global]);

        if (edit_value < sizeof(accepted_chars) - 2)
          edit_value++;
        if (edit_value == 0 && edit_string_global[constrain(edit_pos_global + 1, 0, len)] > 0)
          edit_value = 1;
        edit_string_global[edit_pos_global] = accepted_chars[edit_value];
        // display.setTextColor(COLOR_BACKGROUND, COLOR_DRUMS);
        // setCursor_textGrid(x + edit_pos_global, y);
        // display.print(edit_string_global[edit_pos_global]);
        // display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      }
      else
      {

        if (edit_pos_global == len - 1)
        {
          edit_pos_global++;
          display.setTextColor(COLOR_SYSTEXT, DARKGREEN);
          setCursor_textGrid(x + len + 3, y);
          display.print("[OK]");
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

        }
        else
          // if (edit_string[edit_pos] != 0 && edit_string[edit_pos] != 32)
          if (edit_string_global[edit_pos_global] != 0)
          {
            edit_pos_global = constrain(edit_pos_global + 1, 0, len);
            if (edit_pos_global < len - 1)
            {
              // setCursor_textGrid(x + edit_pos_global, y);
              // display.setTextColor(COLOR_BACKGROUND, COLOR_PITCHSMP);
              // display.print(edit_string_global[edit_pos_global]); // highlight current char
              // display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            }
            // if (edit_pos_global > 0)
            // {
            //   display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
            //   setCursor_textGrid(x + edit_pos_global - 1, y);
            //   display.print(edit_string_global[edit_pos_global - 1]); // normal char to the left
            // }
          }
        // else
         // {
         //   if (edit_pos_global + 1 > last_char_pos)
         //     edit_pos_global = len;
         // }

      }
    }
    else if (LCDML.BT_checkUp())
    {
      if (edit_mode_global == true)
      {
        edit_value = search_accepted_char(edit_string_global[edit_pos_global]);
        if (edit_value >= 1)
          edit_value--;
        if (edit_value == 0 && edit_string_global[constrain(edit_pos_global + 1, 0, len)] > 0)
          edit_value = 0;
        edit_string_global[edit_pos_global] = accepted_chars[edit_value];
        // display.setTextColor(COLOR_BACKGROUND, COLOR_DRUMS);
        // setCursor_textGrid(x + edit_pos_global, y);
        // display.print(edit_string_global[edit_pos_global]);
        // display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      }
      else
      {
        if (edit_pos_global == len)
        {
          display.setTextColor(GREY3, COLOR_BACKGROUND);
          setCursor_textGrid(x + len + 3, y);
          display.print("[OK]");
          display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
        }

        if (edit_pos_global > last_char_pos)
          edit_pos_global = last_char_pos;
        else
          edit_pos_global = constrain(edit_pos_global - 1, 0, len);
      }
    }

  }
  if (LCDML.BT_checkEnter()) {
    if (edit_pos_global == len) {
      edit_pos_global = 0;
      edit_mode_global = false;
      return true;
    }
    else {
      edit_mode_global = !edit_mode_global;
      // virtual_typewriter_update_display();
      return false;
    }
  }

  virtual_typewriter_update_display();

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  encoderDir[ENC_R].reset();

  return false;
}



extern FLASHMEM uint8_t search_accepted_char(uint8_t c)
{
  // if (c == 0)
  //   c = 32;

  for (uint8_t i = 0; i < sizeof(accepted_chars) - 1; i++)
  {
#ifdef DEBUG
    LOG.print(i, DEC);
    LOG.print(F(":"));
    LOG.print(c);
    LOG.print(F("=="));
    LOG.println(accepted_chars[i], DEC);
#endif
    if (c == accepted_chars[i])
      return (i);
  }
  return (0);
}

FLASHMEM void display_int(int16_t var, uint8_t size, bool zeros, bool brackets, bool sign)
{
  display_float(float(var), size, 0, zeros, brackets, sign);
}

FLASHMEM void display_float(float var, uint8_t size_number, uint8_t size_fraction, bool zeros, bool brackets, bool sign)
{
  char s[display_cols + 3];

  if (size_fraction > 0)
  {
    if (zeros == true && sign == true)
      snprintf_P(s, sizeof(s), PSTR("%+0*.*f"), size_number + size_fraction + 2, size_fraction, var);
    else if (zeros == true && sign == false)
      snprintf_P(s, sizeof(s), PSTR("%0*.*f"), size_number + size_fraction + 1, size_fraction, var);
    else if (zeros == false && sign == true)
      snprintf_P(s, sizeof(s), PSTR("%+*.*f"), size_number + size_fraction + 2, size_fraction, var);
    else if (zeros == false && sign == false)
      snprintf_P(s, sizeof(s), PSTR("%*.*f"), size_number + size_fraction + 1, size_fraction, var);
  }
  else
  {
    if (zeros == true && sign == true)
      snprintf_P(s, sizeof(s), PSTR("%+0*d"), size_number + 1, int(var));
    else if (zeros == true && sign == false)
      snprintf_P(s, sizeof(s), PSTR("%0*d"), size_number, int(var));
    else if (zeros == false && sign == true)
      snprintf_P(s, sizeof(s), PSTR("%+*d"), size_number + 1, int(var));
    else if (zeros == false && sign == false)
      snprintf_P(s, sizeof(s), PSTR("%*d"), size_number, int(var));
  }

  if (brackets == true)
  {
    char tmp[display_cols + 1];

    strcpy(tmp, s);
    sprintf(s, "[%s]", tmp);
  }

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(s);
  display.setTextColor(COLOR_SYSTEXT);
}

FLASHMEM void display_bar_int(const char* title, uint32_t value, float factor, int32_t min_value, int32_t max_value, uint8_t size, bool zeros, bool sign, bool init)
{
  display_bar_float(title, float(value), factor, min_value, max_value, size, 0, zeros, sign, init);
}

FLASHMEM void display_bar_float(const char* title, float value, float factor, int32_t min_value, int32_t max_value, uint8_t size_number, uint8_t size_fraction, bool zeros, bool sign, bool init)
{
  uint8_t size;
  // float v;
  // float _vi = 0.0;
  // uint8_t vf;
  // uint8_t vi;

  if (size_fraction == 0)
    size = size_number;
  else
    size = size_number + size_fraction + 1;
  if (sign == true)
    size++;

  // v = float((value - min_value) * (display_cols - size)) / (max_value - min_value);
  // vf = uint8_t(modff(v, &_vi) * 10.0 + 0.5);
  // vi = uint8_t(_vi);

  if (sign == true)
    size += 1;

  // Title
  if (init == true)
    show(1, 1, display_cols - 2, title);

  // Value
  display.setCursor(CHAR_width * (display_cols - size - 3), CHAR_height * 2);
  display_float(value * factor, size_number, size_fraction, zeros, false, sign); // TBD

  // Bar
  // if (vi == 0)
  display.console = true;
  if (value == 0)
  {
    display.fillRect(CHAR_width, 2 * CHAR_height, 12 * CHAR_width, CHAR_height - 2, COLOR_BACKGROUND);
  }
  else
  {
    display.fillRect(CHAR_width, 2 * CHAR_height, value * 1.43, CHAR_height - 2, COLOR_SYSTEXT);
    display.fillRect(CHAR_width + value * 1.43, 2 * CHAR_height, (max_value - value) * 1.43 + 1, CHAR_height - 2, COLOR_BACKGROUND);
  }
}

FLASHMEM inline void display_meter_int(const char* title, uint32_t value, float factor, float offset, int32_t min_value, int32_t max_value, uint8_t size, bool zeros, bool sign, bool init)
{
  display_meter_float(title, float(value), factor, offset, min_value, max_value, size, 0, zeros, sign, init);
}

FLASHMEM void display_meter_float(const char* title, float value, float factor, float offset, int32_t min_value, int32_t max_value, uint8_t size_number, uint8_t size_fraction, bool zeros, bool sign, bool init)
{
  uint8_t size = 0;
  float v;
  // float _vi = 0.0;
  // uint8_t vf;
  // uint8_t vi;

  if (size_fraction == 0)
    size = size_number;
  else
    size = size_number + size_fraction + 1;
  if (sign == true)
    size++;

  // v = float((value - min_value) * (display_cols - size)) / (max_value - min_value);
  v = float((value - min_value) * (16 * 8 + 2 - size)) / (max_value - min_value);
  // vf = uint8_t(modff(v, &_vi) * 10.0 + 0.5);
  // vi = uint8_t(_vi);

  if (init == true)
  {
    // Title
    display.setCursor(CHAR_width, CHAR_height);
    display.print(title);
    display.console = true;
    display.drawRect(CHAR_width, 2 * CHAR_height + 2, CHAR_width * 11 + 1, CHAR_height - 5, GREY1);
  }

  // Value

  display.setCursor((display_cols - size - 3) * CHAR_width, CHAR_height * 2);

  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print((value + offset) * factor);

  // new gauge
  display.console = true;
  display.fillRect(CHAR_width + 1, 2 * CHAR_height + 4, v + 2, 8, COLOR_BACKGROUND);
  display.fillRect(CHAR_width + v + 5, 2 * CHAR_height + 4, CHAR_width * 10 + 7 - v, 8, COLOR_BACKGROUND);
  display.fillRect(CHAR_width + v + 1, 2 * CHAR_height + 4, 4, 8, COLOR_SYSTEXT);
}

FLASHMEM void string_trim(char* s)
{
  int i;

  while (isspace(*s))
    s++; // skip left side white spaces
  for (i = strlen(s) - 1; (isspace(s[i])); i--)
    ; // skip right side white spaces
  s[i + 1] = '\0';
}

FLASHMEM void locate_previous_non_favorite()
{
  // find prev. non fav in current bank
  do
  {
    if (configuration.dexed[selected_instance_id].voice == 0)
    {
      configuration.dexed[selected_instance_id].voice = 32; //+1
      configuration.dexed[selected_instance_id].bank--;

      if (configuration.dexed[selected_instance_id].bank < 1) {
        configuration.dexed[selected_instance_id].bank = MAX_BANKS - 1;
      }
      favsearcher++;
    }
    configuration.dexed[selected_instance_id].voice--;
    favsearcher++;
  } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
    selected_instance_id) == true &&
    favsearcher < 170);
  favsearcher = 0;
}

FLASHMEM void locate_previous_favorite()
{
  // worst case, nothing found below voice 0 /  bank 0 - start loop at last bank
  if (configuration.dexed[selected_instance_id].voice < 2 && configuration.dexed[selected_instance_id].bank == 0 && favsearcher < 170)
  {
    configuration.dexed[selected_instance_id].bank = MAX_BANKS - 1;
    configuration.dexed[selected_instance_id].voice = 32;
  }
  else

    if (configuration.dexed[selected_instance_id].voice == 0 && configuration.dexed[selected_instance_id].bank < MAX_BANKS - 1)
    { // if at begin of any other bank
      configuration.dexed[selected_instance_id].bank--;
      configuration.dexed[selected_instance_id].voice = 32;
    }

  if (configuration.dexed[selected_instance_id].voice >= 0 && configuration.dexed[selected_instance_id].bank >= 0)
  {
    print_fav_search_text(LEFT);

    do
    { // first find previous fav in current bank

      if (configuration.dexed[selected_instance_id].voice == 0)
      {

        if (configuration.dexed[selected_instance_id].bank == 0)
        {
          configuration.dexed[selected_instance_id].bank = MAX_BANKS - 1;
          configuration.dexed[selected_instance_id].voice = 32;
        }
        else
          configuration.dexed[selected_instance_id].bank--;
        configuration.dexed[selected_instance_id].voice = 32;
      }
      else

        configuration.dexed[selected_instance_id].voice--;
      favsearcher++;

    } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
      selected_instance_id) == false &&
      configuration.dexed[selected_instance_id].voice >= 1 && favsearcher < 36);

    // if found, we are done. else quick check in previous banks

    if (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
      selected_instance_id) == false &&
      configuration.dexed[selected_instance_id].voice >= 0 && configuration.dexed[selected_instance_id].bank >= 0 && favsearcher < 170)
    {
      configuration.dexed[selected_instance_id].voice = 32;

      do
      { // seek for previous bank
        configuration.dexed[selected_instance_id].bank--;
        favsearcher++;
      } while (quick_check_favorites_in_bank(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, selected_instance_id) == false && favsearcher < 132 && configuration.dexed[selected_instance_id].bank >= 0);

      do
      { // last try to search if a bank with fav was found

        configuration.dexed[selected_instance_id].voice--;
        favsearcher++;
      } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
        selected_instance_id) == false &&
        configuration.dexed[selected_instance_id].voice >= 1 && favsearcher < 170);
    }
  }
  favsearcher = 0;
}

FLASHMEM void locate_next_favorite()
{
  bool RollOver = false;
  if (configuration.dexed[selected_instance_id].voice > 30 && configuration.dexed[selected_instance_id].bank >= MAX_BANKS - 1)
  { // if at end of all banks
    configuration.dexed[selected_instance_id].bank = 0;
    configuration.dexed[selected_instance_id].voice = 0;
    RollOver = true;
  }
  else if (configuration.dexed[selected_instance_id].voice > 30 && configuration.dexed[selected_instance_id].bank < MAX_BANKS - 1)
  { // if at end of any other bank
    configuration.dexed[selected_instance_id].bank++;
    configuration.dexed[selected_instance_id].voice = 0;
  }

  if (configuration.dexed[selected_instance_id].voice <= 30 && configuration.dexed[selected_instance_id].bank <= MAX_BANKS)
  {
    print_fav_search_text(RIGHT);

    do
    { // first find next fav in current bank

      if (RollOver == false)
        configuration.dexed[selected_instance_id].voice++;
      else
        RollOver = true;
      favsearcher++;

    } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
      selected_instance_id) == false &&
      configuration.dexed[selected_instance_id].voice <= 32 && favsearcher < 36);

    // if found, we are done. else quick check in next banks

    if (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
      selected_instance_id) == false &&
      configuration.dexed[selected_instance_id].bank < MAX_BANKS && favsearcher < 170)
    {
      configuration.dexed[selected_instance_id].voice = 0;

      do
      { // seek in next bank

        configuration.dexed[selected_instance_id].bank++;
        if (configuration.dexed[selected_instance_id].bank > MAX_BANKS - 1 && favsearcher < 190)
        {
          configuration.dexed[selected_instance_id].bank = 0;
          configuration.dexed[selected_instance_id].voice = 0;
        }
        favsearcher++;
      } while (quick_check_favorites_in_bank(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, selected_instance_id) == false && favsearcher < 132);

      if (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
        selected_instance_id) == false &&
        configuration.dexed[selected_instance_id].voice <= 32 && favsearcher < 190)
      {
        do
        { // last bank to search if a fav can be found

          configuration.dexed[selected_instance_id].voice++;
          favsearcher++;
        } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
          selected_instance_id) == false &&
          favsearcher < 170);
      }
    }
  }
  favsearcher = 0;
}

FLASHMEM void locate_next_non_favorite()
{
  // find next non-fav in current bank
  do
  {
    configuration.dexed[selected_instance_id].voice++;
    if (configuration.dexed[selected_instance_id].voice > 31)
    {
      configuration.dexed[selected_instance_id].voice = 0;
      configuration.dexed[selected_instance_id].bank++;
      if (configuration.dexed[selected_instance_id].bank > MAX_BANKS - 1) {
        configuration.dexed[selected_instance_id].bank = 0;
      }
      favsearcher++;
    }
    favsearcher++;
  } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
    selected_instance_id) == true &&
    favsearcher < 170);
  favsearcher = 0;
}

FLASHMEM void locate_random_non_favorite()
{
  // find random non-fav
  do
  {
    configuration.dexed[selected_instance_id].voice = random(32);
    configuration.dexed[selected_instance_id].bank = random(MAX_BANKS - 1);
    favsearcher++;
  } while (check_favorite(configuration.dexed[selected_instance_id].pool, configuration.dexed[selected_instance_id].bank, configuration.dexed[selected_instance_id].voice,
    selected_instance_id) == true &&
    favsearcher < 100);
  favsearcher = 0;
}

FLASHMEM bool check_favorite(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id)
{
  p = constrain(p, 0, DEXED_POOLS - 1);
  b = constrain(b, 0, MAX_BANKS - 1);
  v = constrain(v, 0, MAX_VOICES - 1);
  char tmp[CONFIG_FILENAME_LEN];
  File myFav;
  if (sd_card_internal > 0)
  {
    snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%s/%d/%d.fav"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b, v);
#ifdef DEBUG
    LOG.print(F("check if Voice is a Favorite: "));
    LOG.print(tmp);
    LOG.println();
#endif
    if (SD.exists(tmp))
    { // is Favorite
#ifdef DEBUG
      LOG.println(F(" - It is in Favorites."));
#endif
      return true;
    }
    else
    { // it was not a favorite

#ifdef DEBUG
      LOG.println(F(" - It is not in Favorites."));
#endif
      return false;
    }
  }
  else
    return false;
}

FLASHMEM float eraseBytesPerSecond(const unsigned char* id)
{
  if (id[0] == 0x20)
    return 152000.0; // Micron
  if (id[0] == 0x01)
    return 500000.0; // Spansion
  if (id[0] == 0xEF)
    return 419430.0; // Winbond
  if (id[0] == 0xC2)
    return 279620.0; // Macronix
  return 320000.0;   // guess?
}

FLASHMEM void load_sd_directory_files_only()
{

  File sd_root = SD.open("/MIDI");

  fm.sd_sum_files = 0;
  sdcard_infos.files.clear();

  while (true)
  {
    File sd_entry = sd_root.openNextFile();
    if (!sd_entry)
      break;

    if (sd_entry.isDirectory() == false && strstr(sd_entry.name(), "._") == NULL)
    {

      //new code
      storage_file_s entry;
      entry.name = sd_entry.name();
      entry.size = sd_entry.size();
      entry.isDirectory = sd_entry.isDirectory();
      sdcard_infos.files.emplace_back(entry);

      fm.sd_sum_files++;
    }

    sd_entry.close();
  }
  sd_root.close();

  // clear all the unused files in array

    // for (uint8_t i = fm.sd_sum_files; i < MAX_FILES; i++)
    // {
    //   strcpy(sdcard_infos.files[i].name, "");
    //   sdcard_infos.files[i].size = 0;
    //   sdcard_infos.files[i].isDirectory = false;
    // }

   //old
   // qsort(sdcard_infos.files, fm.sd_sum_files, sizeof(storage_file_t), compare_files_by_name);

  //new
  std::sort(sdcard_infos.files.begin(), sdcard_infos.files.end(), compare_files_by_name);

}

#if defined SMFPLAYER
uint8_t smf_line = 14;
extern bool smf_playing;
bool smf_track_state[16] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
uint8_t smf_track_inst[16] = { 1,1,1,1,1,1,1,1,1,8,1,1,1,1,1,1 };
static uint32_t smf_autoclear_last_check_ms = 0;

// ---------- SMF note cascade UI ----------
static const uint8_t  SMF_CASCADE_TOPLINE = 14;       // existing area (14..23)
static const uint8_t  SMF_CASCADE_LINES   = 10;       // 14..23 inclusive
static const uint8_t  SMF_CASCADE_ROWS    = 9;        // rows below top bar (line 15..23)
static const uint16_t SMF_ROW_H           = 9;        // matches current usage (smf_line * 9)
static const uint16_t SMF_COL_W           = (CHAR_width_small * 3);
static const uint16_t SMF_X_OFFSET = (CHAR_width_small * 2);
static const uint16_t SMF_TOPBAR_EXTRA_X = CHAR_width_small; // 1 char to the right
// FLASHMEM static inline uint16_t _smf_bar_x(uint8_t ch) { return _smf_col_x(ch) + SMF_TOPBAR_EXTRA_X; }
// FLASHMEM static inline uint16_t _smf_bar_w(void) { return SMF_COL_W - SMF_TOPBAR_EXTRA_X; }

static char    smf_cascade[16][SMF_CASCADE_ROWS][3];  // 2 chars + '\0' (e.g. "C#")
static uint8_t smf_last_active_note[16];              // 255 = none
static bool    smf_note_active[16][128];              // per-channel note state

FLASHMEM static inline uint16_t _smf_col_x(uint8_t ch) {
  return SMF_X_OFFSET + (ch * SMF_COL_W);
}


// text X offset inside each 3-char column (keeps your original alignment)
FLASHMEM static inline uint16_t _smf_text_x(uint8_t ch) {
  return _smf_col_x(ch) + (CHAR_width_small * 1);
}

FLASHMEM static inline uint16_t _smf_line_y(uint8_t line) {
  return line * SMF_ROW_H;
}

FLASHMEM static void smf_ui_clear_channel(uint8_t ch) {
  display.console = true;
  smf_last_noteon_note[ch] = 255;
  // Draw the continuous GREY3 topbar ONCE (avoid per-channel fragmented blocks)
  if (ch == 0) {
    const uint16_t bar_x = _smf_col_x(0) + CHAR_width_small;         // shift right by 1 char
    const uint16_t bar_y = _smf_line_y(SMF_CASCADE_TOPLINE);
    const uint16_t bar_right = _smf_col_x(15) + SMF_COL_W;           // last channel (0..15)
    const uint16_t bar_w = bar_right - bar_x;

    display.fillRect(bar_x, bar_y, bar_w, SMF_ROW_H, GREY3);
  }

  // Rows below: RESTORED (full width, black, original behavior)
  display.fillRect(_smf_col_x(ch),
                   _smf_line_y(SMF_CASCADE_TOPLINE + 1),
                   SMF_COL_W,
                   SMF_ROW_H * SMF_CASCADE_ROWS,
                   COLOR_BACKGROUND);

  display.console = false;

  for (uint8_t r = 0; r < SMF_CASCADE_ROWS; r++) {
    smf_cascade[ch][r][0] = '\0';
  }
  smf_last_active_note[ch] = 255;
  for (uint8_t n = 0; n < 128; n++) {
    smf_note_active[ch][n] = false;
  }
}

FLASHMEM static void smf_ui_draw_topbar(uint8_t ch) {
  // NOTE:
  // The continuous GREY3 bar is drawn elsewhere (e.g. in smf_ui_clear_channel when ch==0).
  // This function must NOT draw any GREY3 rectangles, otherwise the bar becomes fragmented.

  display.setTextColor(GREY1, GREY3);
  display.setCursor(_smf_text_x(ch), _smf_line_y(SMF_CASCADE_TOPLINE));

  if (smf_last_active_note[ch] < 128) {
    char tmp[3];
    snprintf_P(tmp, sizeof(tmp), PSTR("%-2s"), noteNames[smf_last_active_note[ch] % 12]);
    display.print(tmp);
  } else {
    // Clear the 2-char note field for this channel (keeps background GREY3)
    display.print(F("  "));
  }
}

FLASHMEM static void smf_ui_redraw_cascade(uint8_t ch) {
  for (uint8_t r = 0; r < SMF_CASCADE_ROWS; r++) {
    const uint8_t line = SMF_CASCADE_TOPLINE + 1 + r;

    display.console = true;
    display.fillRect(_smf_col_x(ch),
                     _smf_line_y(line),
                     SMF_COL_W,
                     SMF_ROW_H,
                     COLOR_BACKGROUND);
    display.console = false;

    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.setCursor(_smf_text_x(ch), _smf_line_y(line));

    if (smf_cascade[ch][r][0] != '\0') {
      display.print(smf_cascade[ch][r]);
    } else {
      display.print(F("  "));
    }
  }
}

FLASHMEM static void smf_ui_push_note(uint8_t ch, const char* note2) {
  // shift down: last row drops off
  for (int r = SMF_CASCADE_ROWS - 1; r > 0; r--) {
    smf_cascade[ch][r][0] = smf_cascade[ch][r - 1][0];
    smf_cascade[ch][r][1] = smf_cascade[ch][r - 1][1];
    smf_cascade[ch][r][2] = '\0';
  }
  smf_cascade[ch][0][0] = note2[0];
  smf_cascade[ch][0][1] = note2[1];
  smf_cascade[ch][0][2] = '\0';

  smf_ui_redraw_cascade(ch);
}

FLASHMEM static void smf_ui_update_active(uint8_t ch, uint8_t note, bool on) {
  if (note >= 128) return;
  smf_note_active[ch][note] = on;

  if (on) {
    smf_last_active_note[ch] = note;  // show most recent active note
  } else {
    // if we just released the displayed note, pick another active one (if any)
    if (smf_last_active_note[ch] == note) {
      smf_last_active_note[ch] = 255;
      for (int n = 127; n >= 0; n--) {
        if (smf_note_active[ch][n]) {
          smf_last_active_note[ch] = (uint8_t)n;
          break;
        }
      }
    }
  }
  smf_ui_draw_topbar(ch);
}

FLASHMEM static void smf_ui_reset_cascade_all(void)
{
  if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_midiplayer))
    return;

  for (uint8_t ch = 0; ch < 16; ch++)
  {
    smf_ui_clear_channel(ch);
    smf_ui_draw_topbar(ch);
    smf_ui_redraw_cascade(ch);
  }
}

static uint32_t smf_last_activity_ms[16] = { 0 };
static bool     smf_channel_has_history[16] = { false };

FLASHMEM static bool smf_channel_has_active_notes(uint8_t ch)
{
  for (uint8_t n = 0; n < 128; n++) {
    if (smf_note_active[ch][n]) return true;
  }
  return false;
}

FLASHMEM static void smf_ui_autoclear_inactive_channels_cb(uint32_t now_ms)
{
  // Only if we are on the SMF player page
  if (LCDML.FUNC_getID() != LCDML.OTHER_getIDFromFunction(UI_func_midiplayer))
    return;

  for (uint8_t ch = 0; ch < 16; ch++)
  {
    if (!smf_channel_has_history[ch])
      continue;

    // Do not clear if channel still holds notes
    if (smf_channel_has_active_notes(ch))
      continue;

    if (now_ms - smf_last_activity_ms[ch] >= 1000)
    {
      smf_ui_clear_channel(ch);
      smf_ui_draw_topbar(ch);
      smf_ui_redraw_cascade(ch);
      smf_channel_has_history[ch] = false;
    }
  }
}


// first note #35, B2, Acoustic BD
uint8_t smf_gm_mapping[48] = {
  MIDI_C5, //Acoustic Bass Drum 35
  MIDI_C5, //Bass Drum 1 36
  MIDI_CIS5, //Side Stick 37
  MIDI_B6, //Acoustic Snare 38
  MIDI_DIS5, //Hand Clap 39
  MIDI_E5, //Electric Snare 40
  MIDI_F3, //Low Floor Tom 41
  MIDI_G6, //Closed HH 42
  MIDI_A3, //High Floor Tom 43
  MIDI_GIS3, //Pedal HH 44
  MIDI_F3, //Low Tom 45
  MIDI_A6, //Open HH 46
  MIDI_A7, //Low-Mid Tom 47
  MIDI_C7, //Hi-Mid Tom 48
  MIDI_CIS4, //Crash 49
  MIDI_A3, //High Tom 50
  MIDI_AIS4, //Ride 51
  MIDI_CIS4, //Chinese Cymb 52
  MIDI_F4, //Ride Bell 53
  MIDI_FIS6, //Tamb 54
  MIDI_CIS4, //Splash Cymb 55
  MIDI_GIS6, //Cowbell 56
  MIDI_A4, //Crash 2 57
  MIDI_C6, //Vibraslap 58
  MIDI_AIS4, //Ride Cymb 59
  MIDI_A5, //Hi Bongo 60
  MIDI_GIS5, //Low Bongo 61
  MIDI_A5, //Mute Hi Conga 62
  MIDI_GIS5, //Open Hi Conga 63 
  MIDI_A5, //Low Conga 64
  MIDI_AIS6, //High Timbale 65
  MIDI_AIS6, //Low Timbale 66
  MIDI_AIS6, //High Agogo 67
  MIDI_AIS6, //Low Agogo 68 
  MIDI_D5, //Cabasa 69
  MIDI_D5, //Maracas 70
  MIDI_D5, //Short Whistle 71
  MIDI_D5, //Long Whistle 72
  MIDI_D5, //Short Guiro 73
  MIDI_D5, //Long Guiro 74
  MIDI_CIS7, //Claves 75
  MIDI_AIS6, //Hi Wood Block 76
  MIDI_AIS6, //Low Wood Block 77
  MIDI_AIS5, //Mute Cuica 78
  MIDI_AIS5, //Open Cuica 79
  MIDI_C6, //Mute Triange 80
  MIDI_CIS6, //Open Triangle 81
  MIDI_D5, //Shaker 82

};

//   midireader reader;
//   reader.open(filename);
//   //   setCursor_textGrid_small(0, 0);
//   //    display.print(filename);
//   // display.print("   ");

FLASHMEM static inline void smf_note_to_str(uint8_t note, char out3[3]) {
  snprintf_P(out3, 3, PSTR("%-2s"), noteNames[note % 12]);
}

FLASHMEM void midiCallback(midi_event* pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  char tmp[10];
  uint8_t inst_channel = 0;
  uint8_t in_note = 0;

  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xa0) && smf_track_state[pev->channel] == 1)
  {
    //Serial.write(pev->data[0] | pev->channel);
    //Serial.write(&pev->data[1], pev->size-1);

    if (smf_track_inst[pev->channel] == 1)
      inst_channel = configuration.dexed[0].midi_channel;
    else if (smf_track_inst[pev->channel] == 2)
      inst_channel = configuration.dexed[1].midi_channel;

#if defined(DRUMS_FILTERS) 
    else if (smf_track_inst[pev->channel] == 3)
      inst_channel = configuration.dexed[2].midi_channel;

    else if (smf_track_inst[pev->channel] == 4)
      inst_channel = configuration.dexed[3].midi_channel;
#endif

    else if (smf_track_inst[pev->channel] == 5)
      inst_channel = microsynth[0].midi_channel;
    else if (smf_track_inst[pev->channel] == 6)
      inst_channel = microsynth[1].midi_channel;

    else if (smf_track_inst[pev->channel] == 7)
      inst_channel = configuration.epiano.midi_channel;
    else if (smf_track_inst[pev->channel] == 8)
      inst_channel = drum_midi_channel;
    else if (smf_track_inst[pev->channel] == 9)
      inst_channel = braids_osc.midi_channel;

    if (smf_track_inst[pev->channel] == 8) //basic GM drum mapping
    {
      in_note = smf_gm_mapping[pev->data[1] - 35];
    }
    else
      in_note = pev->data[1];

snprintf_P(tmp, sizeof(tmp), PSTR("%-2s"), noteNames[pev->data[1] % 12]);

const uint8_t status = pev->data[0] & 0xF0;
const uint8_t vel    = pev->data[2];

// NOTE OFF can be either 0x80 or 0x90 with velocity 0
const bool isNoteOn  = (status == 0x90) && (vel > 0);
const bool isNoteOff = (status == 0x80) || ((status == 0x90) && (vel == 0));

if (isNoteOn)
{
  smf_last_activity_ms[pev->channel] = millis();
  smf_channel_has_history[pev->channel] = true;

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_midiplayer))
{
  const uint8_t ch = pev->channel;

  // Remember this NOTE ON (will be pushed to cascade on NOTE OFF)
  smf_last_noteon_note[ch] = pev->data[1];

  // Topbar shows current active note
  smf_ui_update_active(ch, pev->data[1], true);
}


  handleNoteOn(inst_channel, in_note, vel, 0);
}
else if (isNoteOff)
{
  smf_last_activity_ms[pev->channel] = millis();
  smf_channel_has_history[pev->channel] = true;

  if (LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_midiplayer))
{
  const uint8_t ch   = pev->channel;
  const uint8_t note = pev->data[1];

  // Push to cascade when the note ends (so cascade advances even without new notes)
  char tmp2[3];
  snprintf_P(tmp2, sizeof(tmp2), PSTR("%-2s"), noteNames[note % 12]);
  smf_ui_push_note(ch, tmp2);

  // Update active-note tracking for topbar
  smf_ui_update_active(ch, note, false);

  // Optional: if you want, clear "last noteon" only when the same note ends
  if (smf_last_noteon_note[ch] == note)
    smf_last_noteon_note[ch] = 255;
}
  handleNoteOff(inst_channel, in_note, vel, 0);
}

    flush_sysex();

    // ---- Auto-clear inactive channels even during playback (throttled) ----
    const uint32_t now_ms = millis();
    if (now_ms - smf_autoclear_last_check_ms >= 50) // 20 Hz is enough
    {
      smf_autoclear_last_check_ms = now_ms;
      smf_ui_autoclear_inactive_channels_cb(now_ms);
    }


    //    if (smf_line > 23)
    //        smf_line = 14;

    //--------------------
       // usbMIDI.send(pev->data[0], pev->data[1],pev->data[2],pev->channel, 0);
  }
}

FLASHMEM void _print_smf_track_state()
{
  for (uint8_t f = 0; f < 16; f++)
  {
    display.setCursor(
  SMF_X_OFFSET + f * (CHAR_width_small * 3) + (CHAR_width_small * 1),
  11 * CHAR_height_small + 5
    );
    if (generic_temp_select_menu == f + 2 && smf_track_state[f])
      display.setTextColor(COLOR_BACKGROUND, MIDDLEGREEN);
    else   if (generic_temp_select_menu == f + 2 && smf_track_state[f] == false)
      display.setTextColor(COLOR_BACKGROUND, RED);
    else
    {
      if (smf_track_state[f])
        display.setTextColor(MIDDLEGREEN, COLOR_BACKGROUND);
      else
        display.setTextColor(RED, COLOR_BACKGROUND);
    }
    print_formatted_number(f + 1, 2);
  }
}

FLASHMEM void _print_smf_track_instruments()
{
  for (uint8_t f = 0; f < 16; f++)
  {
    display.setCursor(
  SMF_X_OFFSET + f * (CHAR_width_small * 3) + (CHAR_width_small * 1),
  12 * CHAR_height_small + 5
    );

    if (generic_temp_select_menu == f + 2 + 16 && smf_track_inst[f] != 8)
    {
      display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    }
    else if (smf_track_inst[f] != 8)
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    else
      if (generic_temp_select_menu == f + 2 + 16 && smf_track_inst[f] == 8)
        display.setTextColor(COLOR_BACKGROUND, COLOR_DRUMS);
      else
        display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);

    if (smf_track_inst[f] == 1)
      display.print("D1");
    else if (smf_track_inst[f] == 2)
      display.print("D2");
    else if (smf_track_inst[f] == 3)
      display.print("D3");
    else if (smf_track_inst[f] == 4)
      display.print("D4");
    else  if (smf_track_inst[f] == 5)
      display.print("M1");
    else  if (smf_track_inst[f] == 6)
      display.print("M2");
    else  if (smf_track_inst[f] == 7)
      display.print("EP");
    else  if (smf_track_inst[f] == 8)
      display.print("DR");
    else  if (smf_track_inst[f] == 9)
      display.print("BR");
    else
      display.print("??");
  }
}

// FLASHMEM void midiSilence(void)
// // Turn everything off on every channel.
// // Some midi files are badly behaved and leave notes hanging, so between songs turn
// // off all the notes and sound
// {
//   midi_event ev;

//   // All sound off
//   // When All Sound Off is received all oscillators will turn off, and their volume
//   // envelopes are set to zero as soon as possible.
//   ev.size = 0;
//   ev.data[ev.size++] = 0xb0;
//   ev.data[ev.size++] = 120;
//   ev.data[ev.size++] = 0;

//   for (ev.channel = 0; ev.channel < 16; ev.channel++)
//     midiCallback(&ev);
// }

FLASHMEM void play_midi_file()
{
  int i;
  sprintf(filename, "/MIDI/%s", sdcard_infos.files[generic_menu].name.c_str());
  // SMF.load("/MIDI/HIDNSEEK.mid");
  i = SMF.load(filename);

  setCursor_textGrid_small(33, 7);
  display.print(F("                   "));
  setCursor_textGrid_small(33, 7);
  if (i == 0) {
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.print(F("OK "));
  }
  else
  {
    display.setTextColor(RED, COLOR_BACKGROUND);
    if (i == 1)
      display.print(F("BLANK FILENAME"));
    else if (i == 2)
      display.print(F("ERROR OPEN FILE"));
    else if (i == 3)
      display.print(F("NOT A MIDI FILE"));
    else if (i == 4)
      display.print(F("HEADER SIZE ERR."));
    else if (i == 5)
      display.print(F("NOT TYPE 0 OR 1"));
    else if (i == 6)
      display.print(F("FMT 0 BUT MORE TRKS"));
    else if (i == 7)
      display.print(F("MORE TRKS REQUIRED"));
    else
      display.print(i);

  }
}
FLASHMEM void print_smf_parameters()
{
  if (smf_playing == false)
  {
    display.setTextColor(GREY2, COLOR_BACKGROUND);

    // Format placeholder (1 char)
    setCursor_textGrid_small(33, 3);
    display.print(F("-"));

    // Track count placeholder (2 chars)
    setCursor_textGrid_small(49, 3);
    display.print(F("--"));

    // Tempo placeholder (3 chars)
    setCursor_textGrid_small(33, 4);
    display.print(F("---"));

    // Time signature placeholder
    setCursor_textGrid_small(49, 4);
    display.print(F("-/-"));
    return;
  }

  // smf_playing == true
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  // ---- Clear fields first (prevents leftover '-' when numbers are shorter) ----

  // Format field width = 1
  setCursor_textGrid_small(33, 3);
  display.print(F(" "));

  // Track count field width = 2
  setCursor_textGrid_small(49, 3);
  display.print(F("  "));

  // Tempo field width = 3
  setCursor_textGrid_small(33, 4);
  display.print(F("   "));

  // (Optional) clear time signature field width = 3 (e.g., "4/4")
  setCursor_textGrid_small(49, 4);
  display.print(F("   "));

  // ---- Print actual values ----

  setCursor_textGrid_small(33, 3);
  display.print(SMF.getFormat());

  setCursor_textGrid_small(49, 3);
  display.print(SMF.getTrackCount());

  setCursor_textGrid_small(33, 4);
  display.print(SMF.getTempo());

  setCursor_textGrid_small(49, 4);
  display.print(SMF.getTimeSignature() >> 8);
  display.print(F("/"));
  display.print(SMF.getTimeSignature() & 0xf);
}

FLASHMEM void UI_func_midiplayer(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    if (sd_card_internal > 0)
    {
      // Auto create missing folders if needed
      if (!SD.exists("MIDI"))
      {
        SD.mkdir("MIDI");
      }
    }
    fm.sd_sum_files = 0;
    seq.edit_state = 0;
    generic_menu = 0;

    if (smf_playing)
      generic_temp_select_menu = 1;
    else
      generic_temp_select_menu = 0;

    fm.sd_mode = FM_BROWSE_FILES;
    fm.active_window = 0;
    fm.sd_cap_rows = 3;

    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("SELECT MIDI FILE"));
    helptext_l(back_text);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(1, 1);
    display.print(F("MIDI FILE PLAYER"));

    setCursor_textGrid_small(1, 3);
    display.setTextColor(GREY2, COLOR_BACKGROUND);
    display.print(F("FILE BROWSER"));

    setCursor_textGrid_small(20, 1);
    display.print(F("SONG NAME:"));

    setCursor_textGrid_small(20, 3);
    display.print(F("MIDI FORMAT:"));
    setCursor_textGrid_small(37, 3);
    display.print(F("NUM.TRACKS:"));
    setCursor_textGrid_small(20, 4);
    display.print(F("SONG TEMPO:"));
    setCursor_textGrid_small(37, 4);
    display.print(F("TIME SIG:"));

    setCursor_textGrid_small(20, 6);
    display.print(F("STATUS:"));
    setCursor_textGrid_small(20, 7);
    display.print(F("ERROR CODE:"));

    print_smf_parameters();

    load_sd_directory_files_only();
        // init cascade UI area
    for (uint8_t ch = 0; ch < 16; ch++) {
      smf_ui_clear_channel(ch);
      smf_ui_draw_topbar(ch);
      smf_ui_redraw_cascade(ch);
    }

  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0)
      {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 17 + 16);
      }
      else if (seq.edit_state == 1)
      {
        if (generic_temp_select_menu == 0)
        {
          if (e.down)
          {
            if (fm.sd_selected_file == fm.sd_cap_rows && fm.sd_cap_rows > 2 && fm.sd_skip_files < fm.sd_sum_files - fm.sd_cap_rows - 1)
              fm.sd_skip_files++;
          }
          else
          {
            if (fm.sd_selected_file == 0 && fm.sd_skip_files > 0)
              fm.sd_skip_files--;
          }
          if (constrain(fm.sd_selected_file + e.dir, 0, fm.sd_cap_rows) < fm.sd_sum_files)
            fm.sd_selected_file = constrain(fm.sd_selected_file + e.dir, 0, fm.sd_cap_rows);
        }
      }
    }
    if (e.pressed && fm.sd_sum_files != 0) // handle button presses during menu
    {
      if (generic_temp_select_menu > 1 && generic_temp_select_menu < 18)
      {
        smf_track_state[generic_temp_select_menu - 2] = !smf_track_state[generic_temp_select_menu - 2];
      }
      else  if (generic_temp_select_menu > 17)
      {
        smf_track_inst[generic_temp_select_menu - 18]++;
        if (smf_track_inst[generic_temp_select_menu - 18] > 9)
          smf_track_inst[generic_temp_select_menu - 18] = 1;
      }

      else if (generic_temp_select_menu == 1 && smf_playing == false)
      {
        // Clear old note history before starting playback again
        smf_ui_reset_cascade_all();

        smf_playing = true;
        helptext_r(F("STOP PLAYING"));
        play_midi_file();
        seq.edit_state = false;
        print_smf_parameters();

      }
      else if (generic_temp_select_menu == 1 && smf_playing)
      {
        //midiSilence();
        for (uint8_t instance_id = 0; instance_id < NUM_DEXED; instance_id++)
        {
          MicroDexed[instance_id]->panic();
        }

        SMF.close();
        smf_playing = false;
        seq.edit_state = false;
        print_smf_parameters();
        helptext_r(F("START PLAYING"));
      }
      else
        seq.edit_state = !seq.edit_state;
    }

    if (fm.sd_sum_files == 0)
    {
      helptext_r(F("NO FILES IN MIDI FOLDER"));
      display.setTextColor(RED, COLOR_BACKGROUND);
    }
    else
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(37, 0);
    display.print("FILES: ");
    display.print(fm.sd_sum_files);

    if (fm.sd_sum_files != 0)
    {
      for (uint8_t f = 0; f < 4; f++)
      {
        if (f < fm.sd_sum_files)
        {
          storage_file_t sd_entry = sdcard_infos.files[fm.sd_skip_files + f];

          if (f == fm.sd_selected_file && fm.sd_parent_folder == false && seq.edit_state && generic_temp_select_menu == 0)
          {
            display.setTextColor(COLOR_SYSTEXT, RED);
            generic_menu = fm.sd_skip_files + f;
          }
          else if (f == fm.sd_selected_file && fm.sd_parent_folder == false && generic_temp_select_menu == 0)
          {
            display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
            generic_menu = fm.sd_skip_files + f;
          }
          else if (f == fm.sd_selected_file && fm.sd_parent_folder == false && generic_temp_select_menu != 0)
          {
            display.setTextColor(COLOR_BACKGROUND, GREY1);
            generic_menu = fm.sd_skip_files + f;
          }
          else
          {
            display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
          }
          show_smallfont_noGrid(f * 11 + 4 * 11, CHAR_width_small * 1, 17, sd_entry.name.c_str());
        }
        else
        {
          display.setTextColor(GREY3, COLOR_BACKGROUND);
          show_smallfont_noGrid(f * 11 + 4 * 11, CHAR_width_small * 1, 17, "---EMPTY---");
        }
      }

      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      sprintf(filename, "%s", sdcard_infos.files[generic_menu].name.c_str());
      show_smallfont_noGrid(CHAR_height_small * 1 + 2, CHAR_width_small * 33, 18, filename);

      if (generic_temp_select_menu == 0 && seq.edit_state == false)
        helptext_r(F("SELECT MIDI FILE"));
      else if (generic_temp_select_menu == 1 && smf_playing == false)
        helptext_r(F("START PLAYING"));
      else  if (generic_temp_select_menu == 1 && smf_playing)
        helptext_r(F("STOP PLAYING"));

      else if (generic_temp_select_menu > 1 && generic_temp_select_menu < 18)
      {
        helptext_r(F("MUTE/UNMUTE TRACK"));
      }
      else  if (generic_temp_select_menu > 17)
      {
        helptext_r(F("SELECT INSTR."));
      }

      if (seq.edit_state && generic_temp_select_menu == 1)
        display.setTextColor(COLOR_SYSTEXT, RED);
      else   if (seq.edit_state == false && generic_temp_select_menu == 1)
        display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);

      else
        display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

      setCursor_textGrid_small(33, 6);
      if (smf_playing)
        display.print("PLAYING");
      else
        display.print("STOPPED");

      _print_smf_track_state();
      _print_smf_track_instruments();
      
    }
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

#else
FLASHMEM void UI_func_midiplayer(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_l(back_text);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.setTextSize(1);
    setCursor_textGrid_small(1, 1);
    display.print(F("MIDI FILE PLAYER"));
    setCursor_textGrid_small(1, 3);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("NOT AVAILABLE IN THIS CONFIGURATION"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}
#endif

FLASHMEM void UI_func_test_mute(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("CONFIRM"));
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("TEST MENU"));
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      if (seq.DAC_mute_state)
        dac_unmute();
      else
        dac_mute();
    }
    setCursor_textGrid(1, 2);
    if (seq.DAC_mute_state)
      display.print(F("UNMUTE DAC"));
    else
      display.print(F("MUTE DAC  "));
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

#if !defined PSRAM  //PSRAM TEST can only run when a non-psram firmware is loaded
extern void psram_test();
#endif

FLASHMEM void UI_func_test_psram(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
#ifdef PSRAM
    ;
#else
    helptext_r(F("START"));
#endif
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("TEST PSRAM CHIP"));
    setCursor_textGrid_small(2, 4);
    display.setTextSize(1);
#ifdef PSRAM
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PSRAM TEST NOT POSSIBLE WHILE IT IS IN USE"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 6);
    display.print(F("FLASH A GENERIC MDT VERSION OR WHEN COMPILING"));
    setCursor_textGrid_small(2, 7);
    display.print(F("FROM SOURCE, DO NOT INCLUDE PSRAM OPTION"));
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 10);
    display.print(F("FAQ AND OTHER HELP IS AVAILABLE AT:"));
    setCursor_textGrid_small(2, 12);
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("https://"));
    setCursor_textGrid_small(2, 13);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.print(F("codeberg.org/positionhigh/MicroDexed-touch/wiki"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
#else
    display.setTextColor(GREY1, COLOR_BACKGROUND);
    display.print(F("TEST SHOULD RUN AROUND 52 SECONDS FOR 8 MB CHIP"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid_small(2, 5);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PUSH ENC_R TO START"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
#endif
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.pressed)
    {
#ifndef PSRAM
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      psram_test();
#endif
    }
    // setCursor_textGrid(1, 2);
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void sub_touchscreen_test_page_init()
{
  display.fillScreen(COLOR_BACKGROUND);
  encoderDir[ENC_R].reset();
  helptext_r(F("CLEAR"));
  helptext_l(back_text);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(2);
  setCursor_textGrid(1, 1);
  display.print(F("TEST TOUCHSCREEN"));
  setCursor_textGrid_small(2, 4);
  display.setTextSize(1);
  display.setTextColor(GREY1, COLOR_BACKGROUND);
  display.print(F("TEST TOUCH INPUT ON THE SCREEN"));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setCursor_textGrid_small(2, 5);
  display.setTextColor(RED, COLOR_BACKGROUND);
  display.print(F("PUSH ENC_R TO CLEAR SCREEN"));
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  draw_button_on_grid(42, 1, "CLEAR", "SCRN", 0);
};

FLASHMEM void UI_func_test_touchscreen(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_test_touchscreen);
    sub_touchscreen_test_page_init();
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      sub_touchscreen_test_page_init();
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    unregisterTouchHandler();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_clear_song(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("CLEAR SONG"));
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("CLEAR SONG?  "));
    setCursor_textGrid(1, 2);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PUSH "));
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("[ENC R]"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F(" TO CONFIRM"));
    display.setTextSize(1);
    setCursor_textGrid(1, 6);
    display.print(F("ALL SONG DATA WILL BE DELETED !"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 7);
    display.print(F("CHAINS, CHAIN TRANSPOSES AND PATTERNS"));
    setCursor_textGrid(1, 8);
    display.print(F("WILL NOT BE TOUCHED."));
    display.setTextSize(2);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      seq_clear_song_data();
      setCursor_textGrid(1, 1);
      display.print(F("Done."));
      print_empty_spaces(10, 1);
      setCursor_textGrid(1, 2);
      print_empty_spaces(23, 1);
      helptext_l("");
      border3_large_clear();
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    unregisterTouchHandler();
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_clear_song_chains(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("CLEAR ALL CHAINS"));
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("CLEAR CHAINS?"));
    setCursor_textGrid(1, 2);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PUSH "));
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("[ENC R]"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F(" TO CONFIRM"));
    display.setTextSize(1);
    setCursor_textGrid(1, 6);
    display.print(F("ALL CHAINS + CHAIN TRANSPOSES"));
    setCursor_textGrid(1, 7);
    display.print(F("WILL BE DELETED !"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 8);
    display.print(F("SONG DATA AND PATTERNS WILL NOT BE TOUCHED."));

    display.setTextSize(2);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      seq_clear_chain_data();
      setCursor_textGrid(1, 1);
      display.print(F("Done."));
      print_empty_spaces(10, 1);
      setCursor_textGrid(1, 2);
      print_empty_spaces(23, 1);
      helptext_l("");
      border3_large_clear();
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_clear_patterns(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("CLEAR ALL PATTERNS"));
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("CLEAR PATTERNS? "));
    setCursor_textGrid(1, 2);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PUSH "));
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("[ENC R]"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F(" TO CONFIRM"));
    display.setTextSize(1);
    setCursor_textGrid(1, 6);
    display.print(F("ALL PATTERNS WILL BE DELETED !"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    setCursor_textGrid(1, 8);
    display.print(F("SONG, CHAINS AND CHAIN TRANSPOSES"));
    setCursor_textGrid(1, 9);
    display.print(F("WILL NOT BE TOUCHED."));
    display.setTextSize(2);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      seq_clear_all_patterns();
      setCursor_textGrid(1, 1);
      display.print(F("Done."));
      print_empty_spaces(10, 1);
      setCursor_textGrid(1, 2);
      print_empty_spaces(23, 1);
      helptext_l("");
      border3_large_clear();
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_func_clear_all(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_r(F("CLEAR ALL"));
    helptext_l(back_text);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setTextSize(2);
    setCursor_textGrid(1, 1);
    display.print(F("CLEAR EVERYTHING?"));
    setCursor_textGrid(1, 2);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("PUSH "));
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.print(F("[ENC R]"));
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F(" TO CONFIRM"));
    display.setTextSize(1);
    setCursor_textGrid(1, 6);
    display.print(F("ALL SONG, CHAIN, CHAIN TRANSPOSES, "));
    setCursor_textGrid(1, 7);
    display.print(F("AND PATTERN DATA WILL BE CLEARED!"));
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    if (LCDML.BT_checkEnter())
    {
      seq_clear_song_data();
      seq_clear_chain_data();
      seq_clear_all_patterns();
      setCursor_textGrid(1, 1);
      display.print(F("Done.  "));
      print_empty_spaces(10, 1);
      setCursor_textGrid(1, 2);
      print_empty_spaces(23, 1);
      helptext_l("");
      border3_large_clear();
      delay(MESSAGE_WAIT_TIME);
      LCDML.FUNC_goBackToMenu();
    }
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void draw_favorite_icon(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id)
{
  p = constrain(p, 0, DEXED_POOLS - 1);
  b = constrain(b, 0, MAX_BANKS - 1);
  v = constrain(v, 0, MAX_VOICES - 1);
  char tmp[CONFIG_FILENAME_LEN];
  display.console = true;
  if (sd_card_internal > 0) {
    display.setTextSize(1);
    sprintf(tmp, "/%s/%d/%s/%d/%d.fav", DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b, v);
    if (SD.exists(tmp)) { // is Favorite
      display.fillRect(4 * CHAR_width + 18, 5, 9, 9, GREEN);
      display.setCursor(4 * CHAR_width + 20, 6);
      display.setTextColor(COLOR_BACKGROUND, GREEN);
      display.print(F("F"));
      if (ts.fav_buttton_state != 2) {
        TouchButton::drawButton(GRID.X[4], GRID.Y[0], "REMOVE", "FAV", TouchButton::BUTTON_RED);
        ts.fav_buttton_state = 2;
      }
    }
    else {
      // it is not a favorite
      display.fillRect(4 * CHAR_width + 18, 5, 9, 9, GREY2);
      display.setCursor(4 * CHAR_width + 20, 6);
      display.setTextColor(COLOR_BACKGROUND, GREY2);
      display.print(F("F"));
      if (ts.fav_buttton_state != 1) {
        TouchButton::drawButton(GRID.X[4], GRID.Y[0], "SET AS", "FAV", TouchButton::BUTTON_ACTIVE);
        ts.fav_buttton_state = 1;
      }
    }
  }
}

FLASHMEM bool quick_check_favorites_in_bank(uint8_t p, uint8_t b, uint8_t instance_id)
{
  p = constrain(p, 0, DEXED_POOLS - 1);
  b = constrain(b, 0, MAX_BANKS - 1);
  char tmp[CONFIG_FILENAME_LEN];

  if (sd_card_internal > 0)
  {
    snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%s/%d"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b);
#ifdef DEBUG
    LOG.print(F("check if there is a Favorite in Bank: "));
    LOG.print(tmp);
    LOG.println();
#endif
    if (SD.exists(tmp))
    { // this bank HAS at least 1 favorite(s)
#ifdef DEBUG
      LOG.println(F("quickcheck found a FAV in bank!"));
#endif
      return (true);
    }
    else
    { // no favorites in bank stored
      return (false);
#ifdef DEBUG
      LOG.println(F(" - It is no Favorite in current Bank."));
#endif
    }
  }
  else
    return false;
}

FLASHMEM void save_favorite(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id)
{
#ifdef DEBUG
  LOG.println(F("Starting saving Favorite."));
#endif
  p = constrain(p, 0, DEXED_POOLS - 1);
  b = constrain(b, 0, MAX_BANKS - 1);
  v = constrain(v, 0, MAX_VOICES - 1);
  char tmp[CONFIG_FILENAME_LEN];
  char tmpfolder[CONFIG_FILENAME_LEN];
  File myFav;
  uint8_t i = 0, countfavs = 0;
  if (sd_card_internal > 0)
  {
    snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%s/%d/%d.fav"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b, v);
    snprintf_P(tmpfolder, sizeof(tmpfolder), PSTR("/%s/%d/%s/%d"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b);
#ifdef DEBUG
    LOG.println(F("Save Favorite to SD card..."));
    LOG.println(tmp);
#endif
    if (!SD.exists(tmp))
    { // create Favorite Semaphore
      if (!SD.exists(tmpfolder))
      {
        SD.mkdir(tmpfolder);
      }
      myFav = SD.open(tmp, FILE_WRITE);
      myFav.close();
#ifdef DEBUG
      LOG.println(F("Favorite saved..."));
#endif
      // fav symbol
      draw_favorite_icon(p, b, v, instance_id);

#ifdef DEBUG
      LOG.println(F("Added to Favorites..."));
#endif
    }
    else
    { // delete the file, is no longer a favorite
      SD.remove(tmp);
      draw_favorite_icon(p, b, v, instance_id);
#ifdef DEBUG
      LOG.println(F("Removed from Favorites..."));
#endif
      for (i = 0; i < 32; i++)
      { // if no other favs exist in current bank, remove folder
        snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%s/%d/%d.fav"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b, i);
        if (SD.exists(tmp))
          countfavs++;
      }
      if (countfavs == 0)
      {
        snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%s/%d"), DEXED_CONFIG_PATH, p, FAV_CONFIG_PATH, b);
        SD.rmdir(tmp);
#ifdef DEBUG
        LOG.println(F("Fav count in bank:"));
        LOG.print(countfavs);
        LOG.println(F("Removed folder since no voice in bank flagged as favorite any more"));
#endif
      }

      ////remove fav symbol
      draw_favorite_icon(p, b, v, instance_id);

#ifdef DEBUG
      LOG.println(F("Removed from Favorites..."));
#endif
    }
  }
}

FLASHMEM char* basename(const char* filename)
{
  char* p = strrchr(filename, '/');
  return p ? p + 1 : (char*)filename;
}

#if defined COMPILE_FOR_PSRAM
FLASHMEM void fill_msz(char filename[], const uint8_t preset_number, const uint8_t zone_number, const uint8_t psram_entry)
{
  // fill the multisample zone information
  strcpy(msz[preset_number][zone_number].filename, basename(filename));

  if (psram_entry >= 0) {
    msz[preset_number][zone_number].psram_entry_number = psram_entry;
  }

  // Search root note from filename
  char root_note[4];
  memset(root_note, 0, sizeof(root_note));

  MatchState ms;
  ms.Target(filename);

  char result = ms.Match("[-_ ][A-G]#?[0-9]");
  if (result > 0)
  {
    memcpy(root_note, filename + ms.MatchStart + 1, ms.MatchLength - 1);
#ifdef DEBUG
    LOG.print(F("Found match at: "));
    LOG.println(ms.MatchStart + 1);
    LOG.print(F("Match length: "));
    LOG.println(ms.MatchLength - 1);
    LOG.print(F("Match root note: "));
    LOG.println(root_note);
#endif

    // get midi note from the root note string
    uint8_t offset = 0;
    switch (root_note[0])
    {
    case 'A':
      offset = 9;
      break;
    case 'B':
      offset = 11;
      break;
    case 'C':
      offset = 0;
      break;
    case 'D':
      offset = 2;
      break;
    case 'E':
      offset = 4;
      break;
    case 'F':
      offset = 5;
      break;
    case 'G':
      offset = 7;
      break;
    }

    if (root_note[ms.MatchLength - 2 - 1] == '#')
    {
      offset++;
    }
    uint8_t midi_root = (root_note[ms.MatchLength - 1 - 1] - '0' + 1) * 12 + offset;
#ifdef DEBUG
    LOG.printf_P(PSTR("root note found: %s\n"), root_note);
    LOG.printf_P(PSTR("midi root note found: %d\n"), midi_root);
#endif
    msz[preset_number][zone_number].rootnote = midi_root;

    // recalculate low and high notes for all zones
    calc_low_high(preset_number);
  }
  else
  {
#ifdef DEBUG
    LOG.println("No match.");
#endif
  }

#ifdef DEBUG
  LOG.print(F("MSZ preset #"));
  LOG.print(preset_number);
  LOG.print(F(" - zone #"));
  LOG.print(zone_number);
  LOG.print(F(": "));
  LOG.print(msz[preset_number][zone_number].filename);
  LOG.print(F(" root: "));
  LOG.println(msz[preset_number][zone_number].rootnote);
#endif
}
#endif

/*************************************************************************
  RLE_Uncompress() - Uncompress a block of data using an RLE decoder.
   in      - Input (compressed) buffer.
   out     - Output (uncompressed) buffer. This buffer must be large
             enough to hold the uncompressed data.
   insize  - Number of input bytes.
*************************************************************************/

FLASHMEM void RLE_Uncompress(const unsigned char* in, unsigned char* out,
  unsigned int insize)
{
  unsigned char marker, symbol;
  unsigned int i, inpos, outpos, count;

  /* Do we have anything to uncompress? */
  if (insize < 1)
  {
    return;
  }
  /* Get marker symbol from input stream */
  inpos = 0;
  marker = in[inpos++];

  /* Main decompression loop */
  outpos = 0;
  do
  {
    symbol = in[inpos++];
    if (symbol == marker)
    {
      /* We had a marker byte */
      count = in[inpos++];
      if (count <= 2)
      {
        /* Counts 0, 1 and 2 are used for marker byte repetition
           only */
        for (i = 0; i <= count; ++i)
        {
          out[outpos++] = marker;
        }
      }
      else
      {
        if (count & 0x80)
        {
          count = ((count & 0x7f) << 8) + in[inpos++];
        }
        symbol = in[inpos++];
        for (i = 0; i <= count; ++i)
        {
          out[outpos++] = symbol;
        }
      }
    }
    else
    {
      /* No marker, plain copy */
      out[outpos++] = symbol;
    }
  }

  while (inpos < insize);
}

FLASHMEM void splash_draw_header()
{
  display.setCursor(57, 7);
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(F("m   i   c   r   o"));
  display.fillRect(3, 25, DISPLAY_WIDTH - 7, 2, GREY2);
  display.setTextSize(1);
}
FLASHMEM void splash_draw_D()
{
  check_remote();

  display.fillRect(3, 34, 89, 2, COLOR_PITCHSMP);
  display.fillRect(3, 39, 97, 2, COLOR_PITCHSMP);
  display.fillRect(3, 44, 102, 2, COLOR_PITCHSMP);
  display.fillRect(3, 49, 25, 2, COLOR_PITCHSMP);
  display.fillRect(83, 49, 27, 2, COLOR_PITCHSMP);
  display.fillRect(3, 54, 25, 2, COLOR_PITCHSMP);
  display.fillRect(87, 54, 26, 2, COLOR_PITCHSMP);
  display.fillRect(3, 59, 25, 2, COLOR_PITCHSMP);
  display.fillRect(90, 59, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 64, 25, 2, COLOR_PITCHSMP);
  display.fillRect(92, 64, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 69, 25, 2, COLOR_PITCHSMP);
  display.fillRect(94, 69, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 74, 25, 2, COLOR_PITCHSMP);
  display.fillRect(95, 74, 24, 2, COLOR_PITCHSMP);
  display.fillRect(3, 79, 25, 2, COLOR_PITCHSMP);
  display.fillRect(94, 79, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 84, 25, 2, COLOR_PITCHSMP);
  display.fillRect(92, 84, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 89, 25, 2, COLOR_PITCHSMP);
  display.fillRect(90, 89, 25, 2, COLOR_PITCHSMP);
  display.fillRect(3, 94, 25, 2, COLOR_PITCHSMP);
  display.fillRect(87, 94, 26, 2, COLOR_PITCHSMP);
  display.fillRect(3, 99, 25, 2, COLOR_PITCHSMP);
  display.fillRect(83, 99, 27, 2, COLOR_PITCHSMP);
  display.fillRect(3, 104, 102, 2, COLOR_PITCHSMP);
  display.fillRect(3, 109, 97, 2, COLOR_PITCHSMP);
  display.fillRect(3, 114, 89, 2, COLOR_PITCHSMP);
  if (remote_active)
    display.console = false;
}

FLASHMEM void splash_draw_X(uint8_t c)
{
  uint16_t colors[2] = { COLOR_PITCHSMP, COLOR_SYSTEXT };

  check_remote();
  display.fillRect(107, 34, 27, 2, colors[c]);
  display.fillRect(186, 34, 27, 2, colors[c]);
  display.fillRect(112, 39, 27, 2, colors[c]);
  display.fillRect(181, 39, 27, 2, colors[c]);
  display.fillRect(117, 44, 27, 2, colors[c]);
  display.fillRect(176, 44, 27, 2, colors[c]);
  display.fillRect(122, 49, 27, 2, colors[c]);
  display.fillRect(172, 49, 27, 2, colors[c]);
  display.fillRect(127, 54, 27, 2, colors[c]);
  display.fillRect(167, 54, 27, 2, colors[c]);
  display.fillRect(132, 59, 27, 2, colors[c]);
  display.fillRect(162, 59, 27, 2, colors[c]);
  display.fillRect(137, 64, 46, 2, colors[c]);
  display.fillRect(142, 69, 36, 2, colors[c]);
  display.fillRect(147, 74, 26, 2, colors[c]);
  display.fillRect(142, 79, 36, 2, colors[c]);
  display.fillRect(137, 84, 46, 2, colors[c]);
  display.fillRect(132, 89, 27, 2, colors[c]);
  display.fillRect(162, 89, 28, 2, colors[c]);
  display.fillRect(127, 94, 27, 2, colors[c]);
  display.fillRect(167, 94, 28, 2, colors[c]);
  display.fillRect(122, 99, 27, 2, colors[c]);
  display.fillRect(172, 99, 27, 2, colors[c]);
  display.fillRect(117, 104, 27, 2, colors[c]);
  display.fillRect(176, 104, 27, 2, colors[c]);
  display.fillRect(112, 109, 27, 2, colors[c]);
  display.fillRect(181, 109, 28, 2, colors[c]);
  display.fillRect(107, 114, 27, 2, colors[c]);
  display.fillRect(186, 114, 27, 2, colors[c]);
  if (remote_active)
    display.console = false;
}

FLASHMEM void splash_draw_reverseD()
{
  check_remote();
  display.fillRect(227, 34, 89, 2, COLOR_PITCHSMP);
  display.fillRect(220, 39, 96, 2, COLOR_PITCHSMP);
  display.fillRect(214, 44, 102, 2, COLOR_PITCHSMP);
  display.fillRect(291, 49, 25, 2, COLOR_PITCHSMP);
  display.fillRect(210, 49, 27, 2, COLOR_PITCHSMP);
  display.fillRect(291, 54, 25, 2, COLOR_PITCHSMP);
  display.fillRect(207, 54, 26, 2, COLOR_PITCHSMP);
  display.fillRect(291, 59, 25, 2, COLOR_PITCHSMP);
  display.fillRect(204, 59, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 64, 25, 2, COLOR_PITCHSMP);
  display.fillRect(202, 64, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 69, 25, 2, COLOR_PITCHSMP);
  display.fillRect(200, 69, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 74, 25, 2, COLOR_PITCHSMP);
  display.fillRect(199, 74, 24, 2, COLOR_PITCHSMP);
  display.fillRect(291, 79, 25, 2, COLOR_PITCHSMP);
  display.fillRect(200, 79, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 84, 25, 2, COLOR_PITCHSMP);
  display.fillRect(202, 84, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 89, 25, 2, COLOR_PITCHSMP);
  display.fillRect(204, 89, 25, 2, COLOR_PITCHSMP);
  display.fillRect(291, 94, 25, 2, COLOR_PITCHSMP);
  display.fillRect(207, 94, 26, 2, COLOR_PITCHSMP);
  display.fillRect(291, 99, 25, 2, COLOR_PITCHSMP);
  display.fillRect(210, 99, 27, 2, COLOR_PITCHSMP);
  display.fillRect(214, 104, 102, 2, COLOR_PITCHSMP);
  display.fillRect(220, 109, 96, 2, COLOR_PITCHSMP);
  display.fillRect(227, 114, 89, 2, COLOR_PITCHSMP);
  if (remote_active)
    display.console = false;
}

FLASHMEM void splash_screen1()
{ // orig, first mdt logo
  splash_draw_header();
  splash_draw_D();
  splash_draw_reverseD();
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(1);
  display.setCursor(0, 124);
  display.print(F("(c) 2018-2021 H.WIRTZ"));
  display.setCursor(0, 133);
  display.print(F("(c) 2021-2023 H.WIRTZ, M.KOSLOWSKI, D.PERBAL"));
  display.setCursor(0, 142);
  display.print(F("(c) 2024-2025 H.WIRTZ, M.KOSLOWSKI, D.PERBAL, D.WEBER"));
}


FLASHMEM void draw_logo2(uint8_t yoffset, uint8_t progress)
{
  if (progress > 10) { // progress 0-10
    return;
  }
  const uint8_t incrementY = 11 - progress;

  unsigned char splash[23360]; // 73 x 320
  RLE_Uncompress(splash_image, splash, sizeof(splash_image));
  uint16_t c;
  uint16_t color;

  if (remote_active) {
    display.console = true;
  }

  for (uint8_t y = incrementY; y < 73; y += incrementY) {
    for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
      const uint8_t data = splash[x + y * DISPLAY_WIDTH];
      if (data != 0) {
        if (data > 130 || y < 26 || (x < 163 && y < 46) || x > 241 || x < 80 || (x > 189 && y > 64)) {
          color = RGB24toRGB565(data, data, data);
        }
        else {
          color = RGB24toRGB565(0, data * 1.4, data * 1.8); // cyan
        }
        for (uint16_t s = 3; s < 200; s++) {
          if (data == splash[(x + s) + y * DISPLAY_WIDTH] && x + s < DISPLAY_WIDTH) {
            c++;
          }
          else {
            break;
          }
        }
        if (color > 0) {
          if (c > 0) {
            display.fillRect(x, y + yoffset, c + 1, 1, color);
            x = x + c;
          }
          else {
            display.drawPixel(x, y + yoffset, color);
          }
        }
      }
      c = 0;
      if (y < 26 && x > 132) {
        break;
      }
    }
  }

  if (remote_active) {
    display.console = false;
  }
}

FLASHMEM void splash_screen2()
{  // modern mdt logo
  display.setTextColor(COLOR_SYSTEXT);
  display.setTextSize(1);
  display.setCursor(1, 90);
  display.print(F("(c) 2018-2021 H.WIRTZ"));
  display.setCursor(1, 100);
  display.print(F("(c) 2021-2023 H.WIRTZ, M.KOSLOWSKI, D.PERBAL"));
  display.setCursor(1, 110);
  display.print(F("(c) 2024-2025 H.WIRTZ, M.KOSLOWSKI, D.PERBAL, D.WEBER"));
  display.setCursor(1 + CHAR_width_small * 4, 128);
  display.setTextColor(GREY2);
  display.print(F("https://codeberg.org/positionhigh/"));
}

int UI_FM_offset_x = 0;
int UI_FM_offset_y = 0;
#define LINE_SZ 2

FLASHMEM void displayOp(char id, int _gridX, int _gridY, char link, char fb)
{
  //  bool opOn = opStatus[6-id] == '1';
  bool opOn = true;
  int x = _gridX * 24;
  x += 3 + UI_FM_offset_x;
  int y = _gridY * 21;
  y += 5 + UI_FM_offset_y;

  // Draw OP
  display.console = true;
  display.fillRect(x, y, 13, 11, _gridY == 3 ? MIDDLEGREEN : DX_DARKCYAN);
  display.setTextSize(1);
  if (opOn)
  {
    display.setTextColor(COLOR_SYSTEXT, _gridY == 3 ? MIDDLEGREEN : DX_DARKCYAN);
  }
  else
  {
    display.setTextColor(RED, GREY4);
  }
  display.setCursor(x + 4, y + 2);
  display.print(id + 0);

  // Draw lines
  const uint16_t color = opOn ? GREY2 : RED;

  display.console = true;
  switch (link)
  {
  case 0: // LINE DOWN
    display.fillRect(x + 6, y + 11, LINE_SZ, 10, color);
    break;
  case 1: // ARROW TO RIGHT
    display.fillRect(x + 6, y + 11, LINE_SZ, 6, color);
    display.fillRect(x + 6, y + 15, 25, LINE_SZ, color);
    break;
  case 2: // ARROW TO RIGHT JOIN
    display.fillRect(x + 6, y + 11, LINE_SZ, 6, color);
    break;
  case 3: // ARROW TO RIGHT AND DOWN
    display.fillRect(x + 6, y + 11, LINE_SZ, 10, color);
    display.fillRect(x + 6, y + 16, 25, LINE_SZ, color);
    display.fillRect(x + 31, y + 16, LINE_SZ, 5, color);
    break;
  case 4: // ARROW TO RIGHT+LEFT AND DOWN
    display.fillRect(x + 6, y + 11, LINE_SZ, 10, color);
    display.fillRect(x + 6, y + 16, 25, LINE_SZ, color);
    display.fillRect(x + 30, y + 16, LINE_SZ, 5, color);
    display.fillRect(x - 17, y + 16, 25, LINE_SZ, color);
    display.fillRect(x - 18, y + 16, LINE_SZ, 5, color);
    break;
  case 6:
    display.fillRect(x + 6, y + 11, LINE_SZ, 6, color);
    display.fillRect(x + 6, y + 15, 50, LINE_SZ, color);
    break;
  case 7: // ARROW TO LEFT
    display.fillRect(x + 6, y + 11, LINE_SZ, 6, color);
    display.fillRect(x - 17, y + 15, 25, LINE_SZ, color);
    break;
  case 8: // ARROW TO LEFT JOIN
    display.fillRect(x + 6, y + 11, LINE_SZ, 6, color);
    break;
  }


  switch (fb)
  {
  case 0:
    break;
  case 1: // single OP feedback
    display.fillRect(x + 6, y - 4, LINE_SZ, 4, color);
    display.fillRect(x + 6, y - 4, 10, LINE_SZ, color);
    display.fillRect(x + 15, y - 4, LINE_SZ, 19, color);
    display.fillRect(x + 6, y + 13, 10, LINE_SZ, color);
    break;
  case 2: // ALGO 4: 3 OPs feedback
    display.fillRect(x + 6, y - 4, LINE_SZ, 5, color);
    display.fillRect(x + 6, y - 4, 10, LINE_SZ, color);
    display.fillRect(x + 15, y - 4, LINE_SZ, 62, color);
    display.fillRect(x + 6, y + 56, 10, LINE_SZ, color);
    break;
  case 3: // ALGO 6: 2 OPs feedback
    display.fillRect(x + 6, y - 4, LINE_SZ, 5, color);
    display.fillRect(x + 6, y - 4, 10, LINE_SZ, color);
    display.fillRect(x + 15, y - 4, LINE_SZ, 45, color);
    display.fillRect(x + 6, y + 36, 10, LINE_SZ, color);
    break;
  case 4: // single OP feedback to the left
    display.fillRect(x + 6, y - 4, LINE_SZ, 4, color);
    display.fillRect(x - 4, y - 4, 10, LINE_SZ, color);
    display.fillRect(x - 4, y - 4, LINE_SZ, 19, color);
    display.fillRect(x - 4, y + 13, 10, LINE_SZ, color);
    break;
  }
}

FLASHMEM void UI_draw_FM_algorithm(uint8_t algo, uint8_t x, uint8_t y)
{
  UI_FM_offset_x = x;
  UI_FM_offset_y = y;
  display.console = true;

  display.fillRect(x + 75, y, 50, 22, COLOR_BACKGROUND);
  display.fillRect(x, y + 22, 150, 70, COLOR_BACKGROUND);

  setCursor_textGrid_small(27, 9);
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  print_formatted_number(algo + 1, 2);

  switch (algo)
  {
  case 0:
    displayOp(6, 3, 0, 0, 1);
    displayOp(5, 3, 1, 0, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 1:
    displayOp(6, 3, 0, 0, 0);
    displayOp(5, 3, 1, 0, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 1);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 2:
    displayOp(6, 3, 1, 0, 1);
    displayOp(5, 3, 2, 0, 0);
    displayOp(4, 3, 3, 2, 0);
    displayOp(3, 2, 1, 0, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 3:
    displayOp(6, 3, 1, 0, 2);
    displayOp(5, 3, 2, 0, 0);
    displayOp(4, 3, 3, 2, 0);
    displayOp(3, 2, 1, 0, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 4:
    displayOp(6, 4, 2, 0, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 1, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 5:
    displayOp(6, 4, 2, 0, 3);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 1, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 6:
    displayOp(6, 4, 1, 0, 1);
    displayOp(5, 4, 2, 7, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 7:
    displayOp(6, 4, 1, 0, 0);
    displayOp(5, 4, 2, 7, 0);
    displayOp(4, 3, 2, 0, 4);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 8:
    displayOp(6, 4, 1, 0, 0);
    displayOp(5, 4, 2, 7, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 1);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 9:
    displayOp(6, 2, 2, 0, 0);
    displayOp(5, 1, 2, 1, 0);
    displayOp(4, 2, 3, 1, 0);
    displayOp(3, 3, 1, 0, 1);
    displayOp(2, 3, 2, 0, 0);
    displayOp(1, 3, 3, 2, 0);
    break;
  case 10:
    displayOp(6, 2, 2, 0, 1);
    displayOp(5, 1, 2, 1, 0);
    displayOp(4, 2, 3, 1, 0);
    displayOp(3, 3, 1, 0, 0);
    displayOp(2, 3, 2, 0, 0);
    displayOp(1, 3, 3, 2, 0);
    break;
  case 11:
    displayOp(6, 3, 2, 7, 0);
    displayOp(5, 2, 2, 0, 0);
    displayOp(4, 1, 2, 1, 0);
    displayOp(3, 2, 3, 6, 0);
    displayOp(2, 4, 2, 0, 1);
    displayOp(1, 4, 3, 2, 0);
    break;
  case 12:
    displayOp(6, 3, 2, 7, 1);
    displayOp(5, 2, 2, 0, 0);
    displayOp(4, 1, 2, 1, 0);
    displayOp(3, 2, 3, 6, 0);
    displayOp(2, 4, 2, 0, 0);
    displayOp(1, 4, 3, 2, 0);
    break;
  case 13:
    displayOp(6, 3, 1, 0, 1);
    displayOp(5, 2, 1, 1, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 14:
    displayOp(6, 4, 1, 7, 0);
    displayOp(5, 3, 1, 0, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 2, 0);
    displayOp(2, 2, 2, 0, 4);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 15:
    displayOp(6, 4, 1, 0, 1);
    displayOp(5, 4, 2, 7, 0);
    displayOp(4, 3, 1, 0, 0);
    displayOp(3, 3, 2, 0, 0);
    displayOp(2, 2, 2, 1, 0);
    displayOp(1, 3, 3, 0, 0);
    break;
  case 16:
    displayOp(6, 4, 1, 0, 0);
    displayOp(5, 4, 2, 7, 0);
    displayOp(4, 3, 1, 0, 0);
    displayOp(3, 3, 2, 0, 0);
    displayOp(2, 2, 2, 1, 4);
    displayOp(1, 3, 3, 0, 0);
    break;
  case 17:
    displayOp(6, 4, 0, 0, 0);
    displayOp(5, 4, 1, 0, 0);
    displayOp(4, 4, 2, 7, 0);
    displayOp(3, 3, 2, 0, 4);
    displayOp(2, 2, 2, 1, 0);
    displayOp(1, 3, 3, 0, 0);
    break;
  case 18:
    displayOp(6, 3, 2, 3, 4);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 1, 0, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 19:
    displayOp(6, 4, 2, 0, 0);
    displayOp(5, 3, 2, 1, 0);
    displayOp(4, 4, 3, 2, 0);
    displayOp(3, 1, 2, 3, 4);
    displayOp(2, 2, 3, 6, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 20:
    displayOp(6, 3, 2, 3, 0);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 1, 2, 3, 1);
    displayOp(2, 2, 3, 1, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 21:
    displayOp(6, 3, 2, 4, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 3, 1, 0);
    displayOp(2, 1, 2, 0, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 22: // CC
    displayOp(6, 3, 2, 3, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 2, 0, 0);
    displayOp(2, 2, 3, 1, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 23: // CC
    displayOp(6, 3, 2, 4, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 3, 1, 0);
    displayOp(2, 1, 3, 1, 0);
    displayOp(1, 0, 3, 1, 0);
    break;
  case 24: // CC
    displayOp(6, 3, 2, 3, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 3, 1, 0);
    displayOp(2, 1, 3, 1, 0);
    displayOp(1, 0, 3, 1, 0);
    break;
  case 25:
    displayOp(6, 4, 2, 0, 1);
    displayOp(5, 3, 2, 1, 0);
    displayOp(4, 4, 3, 2, 0);
    displayOp(3, 2, 2, 0, 0);
    displayOp(2, 2, 3, 6, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 26:
    displayOp(6, 4, 2, 0, 0);
    displayOp(5, 3, 2, 1, 0);
    displayOp(4, 4, 3, 2, 0);
    displayOp(3, 2, 2, 0, 1);
    displayOp(2, 2, 3, 6, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 27:
    displayOp(6, 4, 3, 2, 0);
    displayOp(5, 3, 1, 0, 1);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 1, 0);
    displayOp(2, 2, 2, 0, 0);
    displayOp(1, 2, 3, 1, 0);
    break;
  case 28:
    displayOp(6, 4, 2, 0, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 1, 0);
    displayOp(2, 2, 3, 1, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 29:
    displayOp(6, 4, 3, 2, 0);
    displayOp(5, 3, 1, 0, 1);
    displayOp(4, 3, 2, 0, 0);
    displayOp(3, 3, 3, 1, 0);
    displayOp(2, 2, 3, 1, 0);
    displayOp(1, 1, 3, 1, 0);
    break;
  case 30:
    displayOp(6, 4, 2, 0, 1);
    displayOp(5, 4, 3, 2, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 3, 1, 0);
    displayOp(2, 1, 3, 1, 0);
    displayOp(1, 0, 3, 1, 0);
    break;
  case 31:
    displayOp(6, 5, 3, 2, 1);
    displayOp(5, 4, 3, 1, 0);
    displayOp(4, 3, 3, 1, 0);
    displayOp(3, 2, 3, 1, 0);
    displayOp(2, 1, 3, 1, 0);
    displayOp(1, 0, 3, 1, 0);
    break;
  default:
    break;
  }
}

FLASHMEM void _draw_volmeter(int x, int y, uint8_t meter, float height)
{
  // draw bar
  display.console = true;
  if (height > ts.displayed_peak[meter])
  {
    // draw a rainbow gradient rectangle, but only up to the old displayed_peak
    display.fillRectRainbow(x, y, 17, height, ts.displayed_peak[meter]);
    ts.displayed_peak[meter] = height;
  }
  else
  {
    if (ts.displayed_peak[meter] > 1)
    {
      display.fillRect(x, y - (ts.displayed_peak[meter]), 17, 2, COLOR_BACKGROUND);
      ts.displayed_peak[meter] = ts.displayed_peak[meter] - 2;
    }
    else if (ts.displayed_peak[meter] > 0)
    {
      display.fillRect(x, y - (ts.displayed_peak[meter]), 17, 1, COLOR_BACKGROUND);
      ts.displayed_peak[meter] = ts.displayed_peak[meter] - 1;
    }
  }
}

FLASHMEM void draw_volmeters_mixer()
{
  float meters[15] = {
    microdexed_peak_0.read(),
    microdexed_peak_1.read(),
     #if (NUM_DEXED>2)
    microdexed_peak_2.read(),
    microdexed_peak_3.read(),
    #else
    0,
    0,
    #endif
    ep_peak_l.available() && ep_peak_r.available() ? (ep_peak_l.read() + ep_peak_r.read()) / 2 : 0,
    microsynth_peak_osc_0.available() ? microsynth_peak_osc_0.read() : 0,
    microsynth_peak_osc_1.available() ? microsynth_peak_osc_1.read() : 0,
    braids_peak_l.available() && braids_peak_r.available() ? (braids_peak_l.read() + braids_peak_r.read()) / 2 : 0,
    mapfloat(ts.msp_peak[0], 0, 127, 0.0, 1.0),
    mapfloat(ts.msp_peak[1], 0, 127, 0.0, 1.0),
    drum_mixer_peak_l.available() && drum_mixer_peak_r.available() ? (drum_mixer_peak_l.read() + drum_mixer_peak_r.read()) / 2 : 0,
    reverb_return_peak_l.read(),
    reverb_return_peak_r.read(),
    master_peak_l.read(),
    master_peak_r.read()
  };

  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  // Instruments
  int y = 92;
  for (uint8_t i = 0; i < 11; i++) {
    int x = CHAR_width_small * i * 4 + CHAR_width_small;

    // draw text
    display.setCursor(x, y + 4);
    int height = mapfloat(meters[i], 0.0, 1.0, 0, 65);

    // draw bar
    _draw_volmeter(x, y, i, height);
  }

  // FX + master channels
  y = 192;
  for (uint8_t i = 10; i < 14; i++) {
    int x = CHAR_width_small * i * 4 - CHAR_width_small * 2;

    // draw text
    display.setCursor(x, y + 4);

    int height = mapfloat(meters[i + 1], 0.0, 1.0, 0, 65);

    // draw bar
    _draw_volmeter(x, y, i + 1, height);
  }
}

FLASHMEM void draw_volmeters_multiband_compressor()
{
  float l, r;
  l = mb_before_l.read();
  r = mb_before_r.read();

  float meters[4] = {
    l,
    r,
    multiband_active ? mb_after_l.read() : l,
    multiband_active ? mb_after_r.read() : r,
  };
  int y = 228;

  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);

  for (uint8_t i = 0; i < 4; i++) {
    int x = CHAR_width_small * i * 4 + (i >= 2 ? 38 * CHAR_width_small : 0);

    // draw text
    display.setCursor(x, 60);

    int height = mapfloat(meters[i], 0.0, 1.0, 0, 70);

    // draw bar
    _draw_volmeter(x, y, i, height);
  }
}

FLASHMEM void clear_volmeter(int x, int y)
{
  display.console = false;
  display.fillRect(x, y - 100, 17, 100, COLOR_BACKGROUND);
}

#ifdef GRANULAR

extern AudioChromaticGranularPoly granularPoly;

// Parameter descriptor structure
struct ParamDesc {
  const char* name;
  int16_t min;
  int16_t max;
};

FLASHMEM void updateGranularPan() {
  float pan = granular_params.pan / 100.0f; // Convert to -1.0 to 1.0

  // Calculate left and right gains using equal power panning
  float leftGain, rightGain;

  if (pan <= 0) {
    // Pan left or center
    leftGain = 1.0f;
    rightGain = 1.0f + pan; // pan is negative or zero, so rightGain <= 1.0
  }
  else {
    // Pan right
    leftGain = 1.0f - pan;
    rightGain = 1.0f;
  }

  // Ensure gains are never negative
  leftGain = max(0.0f, leftGain);
  rightGain = max(0.0f, rightGain);

  // Apply the gains directly to the master mixer channels for granular //swapped seems correct
  master_mixer_r.gain(MASTER_MIX_CH_GRANULAR, volume_transform(leftGain));
  master_mixer_l.gain(MASTER_MIX_CH_GRANULAR, volume_transform(rightGain));
}

FLASHMEM void updateGranularFilters() {
  float frequency, resonance;

  // Convert UI values to usable ranges
  // Assuming filter_freq is 0-127, map to reasonable frequency range
  frequency = map(granular_params.filter_freq, 0, 100, 90, 8000);  // 80Hz to 8kHz

  // Convert resonance (0-127 to 0.7-5.0 Q factor)
  resonance = map(granular_params.filter_resonance, 0, 100, 70, 500) / 100.0;  // 0.7 to 5.0

  // Apply filter mode and settings to both channels
  switch (granular_params.filter_mode) {
  case 0: // Bypass (no filtering)
    granular_filter_l.setLowpass(0, 20000, 0.7);  // Wide open lowpass
    granular_filter_r.setLowpass(0, 20000, 0.7);
    break;

  case 1:  // Low pass
    granular_filter_l.setLowpass(0, frequency / 1.5, resonance);
    granular_filter_r.setLowpass(0, frequency / 1.5, resonance);
    break;

  case 2:  // High pass  
    granular_filter_l.setHighpass(0, frequency, resonance);
    granular_filter_r.setHighpass(0, frequency, resonance);
    break;

  case 3:  // Band pass
    granular_filter_l.setBandpass(0, frequency, resonance);
    granular_filter_r.setBandpass(0, frequency, resonance);
    break;

  case 4:  // Notch
    granular_filter_l.setNotch(0, frequency, resonance);
    granular_filter_r.setNotch(0, frequency, resonance);
    break;
  }
}

FLASHMEM void updateGranularParams() {

  if (granular_params.active) {
    // Update the polyphonic granular with current parameters
    granularPoly.setParams(granular_params);
    updateGranularPan();
    updateGranularFilters();
    global_delay_in_mixer[0]->gain(18, mapfloat(granular_params.delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.3f));
    global_delay_in_mixer[1]->gain(18, mapfloat(granular_params.delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.3f));
    reverb_mixer_r.gain(REVERB_MIX_CH_GRANULAR, volume_transform(mapfloat(granular_params.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.8f)));
    reverb_mixer_l.gain(REVERB_MIX_CH_GRANULAR, volume_transform(mapfloat(granular_params.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.8f)));

    // Also update sample data if the sample selection changed
    static uint8_t last_sample_note = 255; // Initialize with invalid value
    if (granular_params.sample_note != last_sample_note) {
      last_sample_note = granular_params.sample_note;

      if (granular_params.sample_note < NUM_DRUMSET_CONFIG &&
        drum_config[granular_params.sample_note].drum_data != NULL) {

        uint32_t sampleFrames = drum_config[granular_params.sample_note].len /
          (drum_config[granular_params.sample_note].numChannels * sizeof(int16_t));
        granularPoly.setSampleData((const int16_t*)drum_config[granular_params.sample_note].drum_data,
          sampleFrames);
      }
    }

  }
}

FLASHMEM void drawGranularWaveform(bool fullRedraw) {
  uint8_t saved_active_sample = activeSample;
  const drum_config_t& slot = drum_config[granular_params.sample_note];

  // Extended to full display width (320 pixels)
  // Adjusted Y position to account for moved grain indicators
  uint16_t plotX = 0;
  uint16_t plotY = 32;  // Was 27, now 32 (grain indicators moved down 5 pixels)
  uint16_t plotW = 320;
  uint16_t plotH = 58;  // Reduced height to fit display with new layout

  static int16_t lastPosX = -1;
  static int16_t lastSpreadL = -1;
  static int16_t lastSpreadR = -1;
  static int16_t lastDensity = -1;

  // Compute sample length in frames
  uint32_t sampleFrames = slot.len / (sizeof(int16_t) * slot.numChannels);
  if (sampleFrames == 0) sampleFrames = 1;

  // Recompute X coordinates for full width
  uint16_t posX = plotX + (granular_params.grain_position * plotW) / 100;
  uint16_t spreadPixels = (granular_params.spread * plotW) / 100;
  int16_t spreadL = (int16_t)posX - (spreadPixels / 2);
  int16_t spreadR = (int16_t)posX + (spreadPixels / 2);
  if (spreadL < (int16_t)plotX) spreadL = plotX;
  if (spreadR > (int16_t)(plotX + plotW)) spreadR = plotX + plotW;

  uint8_t numTicks = map(granular_params.density, 0, 100, 1, 10);

  if (fullRedraw) {
    // Full waveform draw with extended width
    drawSampleWaveform(plotX, plotY, plotW, plotH,
      (int16_t*)slot.drum_data, slot.len, slot.numChannels,
      plotX, plotX + plotW);

    lastPosX = -1;
    lastSpreadL = -1;
    lastSpreadR = -1;
    lastDensity = -1;
  }

  // ---- Update spread lines ----
  if (spreadL != lastSpreadL) {
    if (lastSpreadL >= 0) {
      drawSampleWaveform(plotX, plotY, plotW, plotH,
        (int16_t*)slot.drum_data, slot.len, slot.numChannels,
        lastSpreadL, lastSpreadL + 1);
    }
    lastSpreadL = spreadL;
  }

  if (spreadR != lastSpreadR) {
    if (lastSpreadR >= 0) {
      drawSampleWaveform(plotX, plotY, plotW, plotH,
        (int16_t*)slot.drum_data, slot.len, slot.numChannels,
        lastSpreadR, lastSpreadR + 1);
    }
    lastSpreadR = spreadR;
  }

  // ---- Update position line ----
  if (posX != lastPosX) {
    if (lastPosX >= 0) {
      drawSampleWaveform(plotX, plotY, plotW, plotH,
        (int16_t*)slot.drum_data, slot.len, slot.numChannels,
        lastPosX, lastPosX + 1);
    }
    lastPosX = posX;
  }

  // ---- Draw overlays in correct order ----
  if (granular_params.spread > 0) {
    display.drawLine(spreadL, plotY, spreadL, plotY + plotH, GREY1);
    display.drawLine(spreadR, plotY, spreadR, plotY + plotH, GREY1);
  }
  display.drawLine(posX, plotY, posX, plotY + plotH, RED);

  // ---- Update density ticks ----
  if (numTicks != lastDensity) {
    // Erase old ticks
    if (lastDensity > 0) {
      for (uint8_t i = 0; i < lastDensity; i++) {
        uint16_t tickX = plotX + (i * plotW) / lastDensity;
        drawSampleWaveform(plotX, plotY, plotW, plotH,
          (int16_t*)slot.drum_data, slot.len, slot.numChannels,
          tickX, tickX + 1);
      }
    }
    // Draw new ticks
    check_remote();
    display.fillRect(plotX, plotY + plotH + 4, plotW - 1, 4, GREY3);
    for (uint8_t i = 0; i < numTicks; i++) {
      uint16_t tickX = plotX + (i * plotW) / numTicks;
      display.drawLine(tickX, plotY + plotH + 4, tickX, plotY + plotH + 7, GREY1);
    }
    lastDensity = numTicks;
  }

  activeSample = saved_active_sample;
}


// Static functions for parameter access
FLASHMEM static int16_t granular_get_param_value(uint8_t param_index) {
  switch (param_index) {
  case 0: return granular_params.sample_note;
  case 1: return granular_params.grain_size;
  case 2: return granular_params.grain_position;
  case 3: return granular_params.spread;
  case 4: return granular_params.density;
  case 5: return granular_params.volume;
  case 6: return granular_params.semitones;
  case 7: return granular_params.attack;
  case 8: return granular_params.release;
  case 9: return granular_params.filter_mode;
  case 10: return granular_params.filter_freq;
  case 11: return granular_params.filter_resonance;
  case 12: return granular_params.rev_send;
  case 13: return granular_params.delay_send_1;
  case 14: return granular_params.delay_send_2;
  case 15: return granular_params.pan;
  case 16: return granular_params.midi_channel;
  case 17: return granular_params.play_mode;  // NEW: Play mode
  default: return 0;
  }
}

FLASHMEM void granular_set_param_value(uint8_t param_index, int16_t value) {
  switch (param_index) {
  case 0:
    granular_params.sample_note = value; break;
  case 1: granular_params.grain_size = value; break;
  case 2: granular_params.grain_position = value; break;
  case 3: granular_params.spread = value; break;
  case 4: granular_params.density = value; break;
  case 5: granular_params.volume = value; break;
  case 6: granular_params.semitones = value; break;
  case 7: granular_params.attack = value; break;
  case 8: granular_params.release = value; break;
  case 9: granular_params.filter_mode = value;
    updateGranularFilters();
    break;
  case 10: granular_params.filter_freq = value;
    updateGranularFilters();
    break;
  case 11: granular_params.filter_resonance = value;
    updateGranularFilters();
    break;
  case 12: granular_params.rev_send = value;
    reverb_mixer_r.gain(REVERB_MIX_CH_GRANULAR, volume_transform(mapfloat(granular_params.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.8f)));
    reverb_mixer_l.gain(REVERB_MIX_CH_GRANULAR, volume_transform(mapfloat(granular_params.rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, 1.8f)));
    break;
  case 13: granular_params.delay_send_1 = value;
    global_delay_in_mixer[0]->gain(18, mapfloat(granular_params.delay_send_1, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.3f));
    break;
  case 14: granular_params.delay_send_2 = value;
    global_delay_in_mixer[1]->gain(18, mapfloat(granular_params.delay_send_2, DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 1.3f));
    break;
  case 15: granular_params.pan = value;
    updateGranularPan();
    break;
  case 16: granular_params.midi_channel = value; break;
  case 17: granular_params.play_mode = value; break;  // NEW: Play mode
  }
  updateGranularParams();
}

// =======================================================
// Updated parameter functions with visual gauges
// =======================================================

// Generic gauge renderer for granular parameters
FLASHMEM static void granularGaugeRenderer(Editor* e, bool refresh, const char* label, int16_t value) {

  // Draw the visual gauge
  print_small_scaled_bar(e->x, e->y, value, e->limit_min, e->limit_max, e->select_id, 1, NULL);
  setCursor_textGrid_small(e->x + 10, e->y);
  // display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  setModeColor(e->select_id);
  display.printf("%s", label);
}

// Custom panorama renderer that matches the drum page style
FLASHMEM static void granularPanRenderer(Editor* e, bool refresh) {
  // Draw the visual gauge similar to drum page
  print_small_scaled_bar(e->x, e->y, granular_params.pan, e->limit_min, e->limit_max, e->select_id, 1, NULL);
  setCursor_textGrid_small(e->x + 10, e->y);
  setModeColor(e->select_id);
  display.printf(F("PANORAMA"));
}

// Custom getter and setter for panorama parameter
FLASHMEM static int16_t granularPanGetter(Editor* e) {
  return granular_params.pan;
}

FLASHMEM static void granularPanSetter(Editor* e, int16_t v) {
  granular_set_param_value(15, v);
}

// Replace the existing pan parameter functions with our custom ones
#undef GRANULAR_PARAM_FUNCTIONS
#define GRANULAR_PARAM_FUNCTIONS(index, label) \
static int16_t granularGetter_##index(Editor* e) { return granular_get_param_value(index); } \
static void    granularSetter_##index(Editor* e, int16_t v) { granular_set_param_value(index, v); } \
static void    granularRenderer_##index(Editor* e, bool refresh) { \
    granularGaugeRenderer(e, refresh, label, granular_get_param_value(index)); \
}

// Generate standard params with gauges
GRANULAR_PARAM_FUNCTIONS(1, "GRAIN SIZE")
GRANULAR_PARAM_FUNCTIONS(2, "POSITION")
GRANULAR_PARAM_FUNCTIONS(3, "SPREAD")
GRANULAR_PARAM_FUNCTIONS(4, "DENSITY")
GRANULAR_PARAM_FUNCTIONS(5, "VOLUME")
GRANULAR_PARAM_FUNCTIONS(6, "PITCH")
GRANULAR_PARAM_FUNCTIONS(7, "ATTACK")
GRANULAR_PARAM_FUNCTIONS(8, "RELEASE")
GRANULAR_PARAM_FUNCTIONS(10, "FILTER FREQ")
GRANULAR_PARAM_FUNCTIONS(11, "FILTER RESO")
GRANULAR_PARAM_FUNCTIONS(12, "REVERB")
GRANULAR_PARAM_FUNCTIONS(13, "DELAY1")
GRANULAR_PARAM_FUNCTIONS(14, "DELAY2")

// Special cases with custom rendering
FLASHMEM static int16_t granularSampleGetter(Editor* e) { return granular_params.sample_note; }
FLASHMEM static void    granularSampleSetter(Editor* e, int16_t v) { granular_set_param_value(0, v); }
FLASHMEM static void granularSampleRenderer(Editor* e, bool refresh) {
  setCursor_textGrid_small(e->x, e->y);
  setModeColor(e->select_id);

  if (granular_params.sample_note < NUM_DRUMSET_CONFIG &&
    drum_config[granular_params.sample_note].name[0] != '\0') {

    uint32_t sample_size_bytes = drum_config[granular_params.sample_note].len * sizeof(int16_t);
    char size_str[20];
    const char* ch_str = "MONO";

    if (drum_config[granular_params.sample_note].numChannels == 2) {
      ch_str = "STEREO";
    }
    if (sample_size_bytes >= 1024 * 1024) {
      uint32_t size_mb_main = sample_size_bytes / (1024 * 1024);
      uint32_t size_mb_rem = (sample_size_bytes % (1024 * 1024)) / (1024 * 1024 / 100);
      sprintf(size_str, "%lu.%02lu MB", (unsigned long)size_mb_main, (unsigned long)size_mb_rem);
    }
    else {
      uint32_t size_kb = sample_size_bytes / 1024;
      sprintf(size_str, "%lu KB", (unsigned long)size_kb);
    }
    display.printf("%02d: %-34s (%s) %7s", granular_params.sample_note,
      drum_config[granular_params.sample_note].name, ch_str, size_str);
  }
}

FLASHMEM static int16_t granularFilterModeGetter(Editor* e) { return granular_params.filter_mode; }
FLASHMEM static void    granularFilterModeSetter(Editor* e, int16_t v) { granular_set_param_value(9, v); }
FLASHMEM static void    granularFilterModeRenderer(Editor* e, bool refresh) {
  static const char* modes[] = { "OFF", "LP ", "BP ", "HP " };

  // Draw gauge for filter mode
  print_small_scaled_bar(e->x, e->y, granular_params.filter_mode,
    e->limit_min, e->limit_max, e->select_id, 1, NULL);
  setCursor_textGrid_small(e->x + 10, e->y);
  setModeColor(e->select_id);
  display.printf("FILTER MODE: %s", modes[granular_params.filter_mode]);
}

FLASHMEM static int16_t granularMidiGetter(Editor* e) { return granular_params.midi_channel; }
FLASHMEM static void    granularMidiSetter(Editor* e, int16_t v) { granular_set_param_value(16, v); }
FLASHMEM static void    granularMidiRenderer(Editor* e, bool refresh) {

  // Draw gauge for MIDI channel
  print_small_scaled_bar(e->x, e->y, granular_params.midi_channel,
    e->limit_min, e->limit_max, e->select_id, 1, NULL);
  setCursor_textGrid_small(e->x + 10, e->y);
  setModeColor(e->select_id);
  display.printf("MIDI CH:");
  _print_midi_channel(granular_params.midi_channel);

}

FLASHMEM static int16_t granularPlayModeGetter(Editor* e) { return granular_params.play_mode; }
FLASHMEM static void    granularPlayModeSetter(Editor* e, int16_t v) { granular_set_param_value(17, v); }
FLASHMEM static void    granularPlayModeRenderer(Editor* e, bool refresh) {
  static const char* modes[] = { "FORWARD", "REVERSE", "RANDOM " };

  print_small_scaled_bar(e->x, e->y, granular_params.play_mode,
    e->limit_min, e->limit_max, e->select_id, 1, NULL);
  setCursor_textGrid_small(e->x + 10, e->y);
  setModeColor(e->select_id);
  display.printf("PLAY MODE: %s", modes[granular_params.play_mode]);
}

extern void   display_granular_status();

FLASHMEM void UI_func_granular(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    registerTouchHandler(handle_touchscreen_granular);
    ui.reset();
    ui.clear();
    display_granular_status();

    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", "SAMPLE", TouchButton::BUTTON_NORMAL);

    // Header
    setCursor_textGrid_small(0, 0);
    display.setTextColor(RED);
    display.print(F("GRANULAR SYNTH"));

    // Waveform
    drawGranularWaveform(true);

    // SAMPLE selector with gauge
    ui.setCursor(0, 11);
    ui.addEditor("SAMPLE", 0, NUM_DRUMSET_CONFIG - 2,
      granularSampleGetter,
      granularSampleSetter,
      granularSampleRenderer);

    // First column with gauges
    ui.setCursor(0, 13);
    ui.addEditor("GRAIN SIZE", 0, 100, granularGetter_1, granularSetter_1, granularRenderer_1);
    ui.addEditor("POSITION", 0, 100, granularGetter_2, granularSetter_2, granularRenderer_2);
    ui.addEditor("SPREAD", 0, 100, granularGetter_3, granularSetter_3, granularRenderer_3);
    ui.addEditor("DENSITY", 0, 100, granularGetter_4, granularSetter_4, granularRenderer_4);
    ui.addEditor("VOLUME", 0, 100, granularGetter_5, granularSetter_5, granularRenderer_5);
    ui.addEditor("PITCH", 0, 100, granularGetter_6, granularSetter_6, granularRenderer_6);
    ui.addEditor("ATTACK", 0, 100, granularGetter_7, granularSetter_7, granularRenderer_7);
    ui.addEditor("RELEASE", 0, 100, granularGetter_8, granularSetter_8, granularRenderer_8);

    // Second column with gauges
    ui.setCursor(22, 13);
    ui.addEditor("PLAY MODE", 0, 2, granularPlayModeGetter, granularPlayModeSetter, granularPlayModeRenderer);  // NEW
    ui.addEditor("FILTER MODE", 0, 3, granularFilterModeGetter, granularFilterModeSetter, granularFilterModeRenderer);
    ui.addEditor("FILTER FREQ", 0, 100, granularGetter_10, granularSetter_10, granularRenderer_10);
    ui.addEditor("FILTER RESO", 0, 100, granularGetter_11, granularSetter_11, granularRenderer_11);
    ui.addEditor("REVERB", 0, 100, granularGetter_12, granularSetter_12, granularRenderer_12);
    ui.addEditor("DELAY1", 0, 100, granularGetter_13, granularSetter_13, granularRenderer_13);
    ui.addEditor("DELAY2", 0, 100, granularGetter_14, granularSetter_14, granularRenderer_14);
    ui.addEditor("PAN", -99, 99, granularPanGetter, granularPanSetter, granularPanRenderer);
    ui.addEditor("MIDI CH", 1, 17, granularMidiGetter, granularMidiSetter, granularMidiRenderer);
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ui.handle_input();

    static uint8_t last_sample_index = granular_params.sample_note;
    static int last_position = -1, last_spread = -1, last_density = -1;

    if (last_position != granular_params.grain_position ||
      last_spread != granular_params.spread ||
      last_density != granular_params.density)
    {
      drawGranularWaveform(false);
      last_position = granular_params.grain_position;
      last_spread = granular_params.spread;
      last_density = granular_params.density;
    }
    else if (last_sample_index != granular_params.sample_note)
    {
      drawGranularWaveform(true);
      last_sample_index = granular_params.sample_note;
    }
  }

  if (LCDML.FUNC_close()) // ****** CLOSE *********
  {
    unregisterTouchHandler();
    ui.clear();
  }
}
#else
FLASHMEM void UI_func_granular(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    display.fillScreen(COLOR_BACKGROUND);
    encoderDir[ENC_R].reset();
    helptext_l(back_text);
    setCursor_textGrid(1, 1);
    display.setTextSize(1);
    display.setTextColor(RED, COLOR_BACKGROUND);
    display.print(F("NOT AVAILABLE IN THIS FIRMWARE"));
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  }

  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    ;
  }

  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }

}

#endif


#if defined BITCRUSH
#include "effect_bitcrush.h"
extern AudioEffectBitcrush    bitcrush_effect;
extern AudioMixer<8> bitcrush_mixer;

// Define min/max values (add to defines.h)
#define BITCRUSH_DRIVE_MIN 0
#define BITCRUSH_DRIVE_MAX 255
#define BITCRUSH_GAIN_MIN 0
#define BITCRUSH_GAIN_MAX 255  // Maps to 0.0-2.0
#define DEXED_INPUT_MIN 0
#define DEXED_INPUT_MAX 255
#define BITCRUSH_FX_SEND_MIN 0
#define BITCRUSH_FX_SEND_MAX 255
#define BITCRUSH_SENSITIVITY_MIN 0
#define BITCRUSH_SENSITIVITY_MAX 255

// Helper functions to convert between uint8_t and float
FLASHMEM float map_uint8_to_float(uint8_t value, float min_val, float max_val) {
    return mapfloat(value, 0, 255, min_val, max_val);
}

FLASHMEM uint8_t map_float_to_uint8(float value, float min_val, float max_val) {
    return (uint8_t)mapfloat(value, min_val, max_val, 0, 255);
}

FLASHMEM void update_bitcrush_parameters()
{
  // Apply bitcrush effect parameters
  float drive_float = map_uint8_to_float(configuration.fx.bitcrush_drive, 0.0f, 1.0f);
  bitcrush_effect.setDrive(drive_float);
  
  float gain_float = map_uint8_to_float(configuration.fx.bitcrush_output_gain, 0.0f, 2.0f);
  bitcrush_effect.setOutputGain(gain_float);
  
  // Apply DEXED input levels
  float dexed1_input = map_uint8_to_float(configuration.fx.bitcrush_dexed1_input, 0.0f, 1.0f);
  bitcrush_mixer.gain(0, dexed1_input);
  
  float dexed2_input = map_uint8_to_float(configuration.fx.bitcrush_dexed2_input, 0.0f, 1.0f);
  bitcrush_mixer.gain(1, dexed2_input);
  
  float dexed3_input = map_uint8_to_float(configuration.fx.bitcrush_dexed3_input, 0.0f, 1.0f);
  bitcrush_mixer.gain(2, dexed3_input);
  
  float dexed4_input = map_uint8_to_float(configuration.fx.bitcrush_dexed4_input, 0.0f, 1.0f);
  bitcrush_mixer.gain(3, dexed4_input);

  float samples_input_r = map_uint8_to_float(configuration.fx.bitcrush_samples_input_r, 0.0f, 1.0f);
  bitcrush_mixer.gain(4, samples_input_r);
  float samples_input_l = map_uint8_to_float(configuration.fx.bitcrush_samples_input_l, 0.0f, 1.0f);
  bitcrush_mixer.gain(4, samples_input_l);
  
  // Apply FX send levels
  float delay1_send = map_uint8_to_float(configuration.fx.bitcrush_delay1_send, 0.0f, 1.0f);
  float delay2_send = map_uint8_to_float(configuration.fx.bitcrush_delay2_send, 0.0f, 1.0f);
  float reverb_send = map_uint8_to_float(configuration.fx.bitcrush_reverb_send, 0.0f, 1.0f);
  
  // Set FX sends (adjust mixer channel numbers if needed)
  global_delay_in_mixer[0]->gain(19, delay1_send);
  global_delay_in_mixer[1]->gain(19, delay2_send);
  reverb_mixer_l.gain(REVERB_MIX_CH_BITCRUSH, reverb_send);
  reverb_mixer_r.gain(REVERB_MIX_CH_BITCRUSH, reverb_send);
}

FLASHMEM void print_bitcrush_static_texts()
{
  display.setTextSize(1);
  helptext_l(back_text);
  helptext_r(F("< > SELECT PARAM"));
  display.setTextSize(1);
  generic_active_function = 0;
  setCursor_textGrid_small(1, 1);
  display.setTextColor(RED);
  display.print(F("BITCRUSHER EFFECT"));
  display.setTextColor(COLOR_SYSTEXT);
  display.setTextColor(GREY1);
  
  // Basic parameters - added sensitivity
  setCursor_textGrid_small(1, 3);
  display.print(F("BIT REDUCTION"));
  setCursor_textGrid_small(1, 4);
  display.print(F("OUTPUT GAIN"));
  setCursor_textGrid_small(1, 5);  // NEW: Sensitivity
  display.print(F("SENSITIVITY"));
  
  display.setTextColor(GREY2);
  setCursor_textGrid_small(1, 7);  // Changed from 6 to 7
  display.print(F("DEXED INPUTS"));
  display.setTextColor(GREY1);
  
  setCursor_textGrid_small(1, 9);   // Changed from 8 to 9
  display.print(F("DEXED 1"));
  setCursor_textGrid_small(1, 10);  // Changed from 9 to 10
  display.print(F("DEXED 2"));
  setCursor_textGrid_small(1, 11);  // Changed from 10 to 11
  display.print(F("DEXED 3"));
  setCursor_textGrid_small(1, 12);  // Changed from 11 to 12
  display.print(F("DEXED 4"));

  setCursor_textGrid_small(1, 13);  
  display.print(F("SAMPLES L"));
  setCursor_textGrid_small(1, 14);  
  display.print(F("SAMPLES R"));

  display.setTextColor(GREY2);
  setCursor_textGrid_small(24, 1);
  display.print(F("EFFECTS SENDS"));
  display.setTextColor(GREY1);
  
  setCursor_textGrid_small(24, 3);
  display.print(F("DELAY 1 SEND"));
  setCursor_textGrid_small(24, 4);
  display.print(F("DELAY 2 SEND"));
  setCursor_textGrid_small(24, 5);
  display.print(F("REVERB SEND"));
}

FLASHMEM void update_selective_values_bitcrush()
{
  // Drive gauge
  if (menu_item_check(0))
    print_small_intbar(13, 3, 
                      map(configuration.fx.bitcrush_drive, BITCRUSH_DRIVE_MIN, BITCRUSH_DRIVE_MAX, 0, 100), 
                      0, 1, 0);
  
  // Apply drive parameter when editing
  if (generic_temp_select_menu == 0 && seq.edit_state) {
    float drive_float = map_uint8_to_float(configuration.fx.bitcrush_drive, 0.0f, 1.0f);
    bitcrush_effect.setDrive(drive_float);
  }
  
  // Output Gain gauge
  if (menu_item_check(1))
    print_small_intbar(13, 4, 
                      map(configuration.fx.bitcrush_output_gain, BITCRUSH_GAIN_MIN, BITCRUSH_GAIN_MAX, 0, 100), 
                      1, 1, 0);
  
  // Apply output gain parameter when editing
  if (generic_temp_select_menu == 1 && seq.edit_state ) {
    float gain_float = map_uint8_to_float(configuration.fx.bitcrush_output_gain, 0.0f, 2.0f);
    bitcrush_effect.setOutputGain(gain_float);
  }
  
  
  // DEXED 1 Input gauge
  if (menu_item_check(3))
    print_small_intbar(13, 9,   // Changed from 8 to 9
                      map(configuration.fx.bitcrush_dexed1_input, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      3, 1, 1);
  
  
  
  // DEXED 2 Input gauge
  if (menu_item_check(4))
    print_small_intbar(13, 10,  // Changed from 9 to 10
                      map(configuration.fx.bitcrush_dexed2_input, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      4, 1, 1);
  
  
  
  // DEXED 3 Input gauge
  if (menu_item_check(5))
    print_small_intbar(13, 11,  // Changed from 10 to 11
                      map(configuration.fx.bitcrush_dexed3_input, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      5, 1, 1);
  
  
  
  // DEXED 4 Input gauge
  if (menu_item_check(6))
    print_small_intbar(13, 12,  // Changed from 11 to 12
                      map(configuration.fx.bitcrush_dexed4_input, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      6, 1, 1);
  
if (menu_item_check(7))
    print_small_intbar(13, 13,  
                      map(configuration.fx.bitcrush_samples_input_r, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      7, 1, 1);

  if (menu_item_check(8))
    print_small_intbar(13, 14,  
                      map(configuration.fx.bitcrush_samples_input_l, DEXED_INPUT_MIN, DEXED_INPUT_MAX, 0, 100), 
                      8, 1, 1);




  // Delay 1 Send gauge
  if (menu_item_check(9))
    print_small_intbar(38, 3, 
                      map(configuration.fx.bitcrush_delay1_send, BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX, 0, 100), 
                      9, 1, 0);
  
  
  // Delay 2 Send gauge
  if (menu_item_check(10))
    print_small_intbar(38, 4, 
                      map(configuration.fx.bitcrush_delay2_send, BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX, 0, 100), 
                      10, 1, 0);
  
  
  // Reverb Send gauge
  if (menu_item_check(11))
    print_small_intbar(38, 5, 
                      map(configuration.fx.bitcrush_reverb_send, BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX, 0, 100), 
                      11, 1, 0);
  
 
   update_bitcrush_parameters();
}

FLASHMEM void UI_func_bitcrush(uint8_t param)
{
  if (LCDML.FUNC_setup()) // ****** SETUP *********
  {
    // setup function
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
    print_bitcrush_static_texts();
    
    if (LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(UI_func_volume) && 
        LCDML.MENU_getLastActiveFunctionID() != LCDML.OTHER_getIDFromFunction(mFunc_screensaver))
      generic_temp_select_menu = 0;

    generic_full_draw_required = true;
    update_selective_values_bitcrush(); // print all at init
    generic_full_draw_required = false;

    display.setTextSize(1);
  }
  if (LCDML.FUNC_loop()) // ****** LOOP *********
  {
    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      if (seq.edit_state == 0) {
        generic_temp_select_menu = constrain(generic_temp_select_menu + e.dir, 0, 11); 
      }
      else {
        switch(generic_temp_select_menu) {
          case 0: // Drive
          {
            configuration.fx.bitcrush_drive = constrain(configuration.fx.bitcrush_drive + (e.dir * e.speed), 
                                                          BITCRUSH_DRIVE_MIN, BITCRUSH_DRIVE_MAX);
            break;
          }
          
          case 1: // Output Gain
          {
            configuration.fx.bitcrush_output_gain = constrain(configuration.fx.bitcrush_output_gain + (e.dir * e.speed), 
                                                                BITCRUSH_GAIN_MIN, BITCRUSH_GAIN_MAX);
            break;
          }
          
          case 3: // DEXED 1 Input
          {
            configuration.fx.bitcrush_dexed1_input = constrain(configuration.fx.bitcrush_dexed1_input + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }
          
          case 4: // DEXED 2 Input
          {
            configuration.fx.bitcrush_dexed2_input = constrain(configuration.fx.bitcrush_dexed2_input + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }
          
          case 5: // DEXED 3 Input
          {
            configuration.fx.bitcrush_dexed3_input = constrain(configuration.fx.bitcrush_dexed3_input + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }
          
          case 6: // DEXED 4 Input
          {
            configuration.fx.bitcrush_dexed4_input = constrain(configuration.fx.bitcrush_dexed4_input + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }

 case 7: 
          {
            configuration.fx.bitcrush_samples_input_r = constrain(configuration.fx.bitcrush_samples_input_r + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }

           case 8: 
          {
            configuration.fx.bitcrush_samples_input_l = constrain(configuration.fx.bitcrush_samples_input_l + (e.dir * e.speed), 
                                                                DEXED_INPUT_MIN, DEXED_INPUT_MAX);
            break;
          }

          case 9: // Delay 1 Send
          {
            configuration.fx.bitcrush_delay1_send = constrain(configuration.fx.bitcrush_delay1_send + (e.dir * e.speed), 
                                                               BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX);
            break;
          }
          
          case 10: // Delay 2 Send
          {
            configuration.fx.bitcrush_delay2_send = constrain(configuration.fx.bitcrush_delay2_send + (e.dir * e.speed), 
                                                               BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX);
            break;
          }
          
          case 11: // Reverb Send
          {
            configuration.fx.bitcrush_reverb_send = constrain(configuration.fx.bitcrush_reverb_send + (e.dir * e.speed), 
                                                               BITCRUSH_FX_SEND_MIN, BITCRUSH_FX_SEND_MAX);
            break;
          }
        }
      }
    }
    if (e.pressed) {
      seq.edit_state = !seq.edit_state;
    }
    // button check end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    update_selective_values_bitcrush();
  }
  if (LCDML.FUNC_close()) // ****** STABLE END *********
  {
    generic_menu = 0;
    encoderDir[ENC_R].reset();
    display.fillScreen(COLOR_BACKGROUND);
  }
}
#endif
