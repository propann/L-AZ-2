#if defined APC

/*
Note by lucian2599 13.05.2025: 
this adds support for older Akai APC Mini Mk1. 
One advantage over Akai APC Mini Mk2 is that Mk1 needs only one midi channel, not all,
 because Mk1 has only 4 colours (red, green, yellow, white), with optional blinking.

Very helpful was the detailed APC Mk1 picture from
 https://github.com/TomasHubelbauer/akai-apc-mini 

Unfortunately, it crashes at inserting/removing steps, in pattern mode.
It also crashes at changing APC mode with Shift key and CLIP_STOP/SOLO/REC_ARM.
What works:
- play/pause with REC_ARM key
- pattern is shown, when changed view from MDT screen
- changing to other pattern with CLIP_STOP/SOLO
- editing pattern velocities, with middle rows pads and sliders
- sample triggering in pattern, with 2nd row from buttom
- changing sample with track row (VOLUME, PAN, SEND, DEVICE)
- changing sample level in pattern with faders (no support for pad brightness in Mk1)
- song mode is shown, when changed view from MDT screen
- mute matrix works too, when changed view from MDT screen.
- screensaver

I manually patched the original 1.9.8.6 APC hex with the Tracks/Scenes/Shift MIDI Note codes,
 so that the provided hex provides also USB BUS Power to APC Mk1:
  mdt_PSRAM-CAPACITIVE_TOUCH-APCMK1_1_9_8_6patchedWithHexEditor.hex
Explained reason of patching, extract from MDT manual:
"Connect the APC to the MDT USB HOST Port. It should power up by the power provided from the Teensy Micro USB Port if you have
loaded a binary firmware from the Codeberg MDT Homepage. It will not power up, when you have compiled the code from source. This is
because some low level modification to the USB Host Library was needed to make it startup without any external power source."

Differences summary between Akai APC Mini Mk2 and Mk1:
| MIDI Diffs      |   APC Mini MK2    |   APC Mini MK1      |
|-----------------|-------------------|---------------------|
|NoteOn Channel   | brightness,blink  | not used, always 0  |
|NoteOn Note      | Pad number  0-63  | Pad number  0-63    |
|NoteOn Velocity  | 128 colours       | 4 colours and blink |
|NoteOn Tracks1-8 | notes 100-107     | notes 64-71         |
|NoteOn Scenes1-8 | notes 112-119     | notes 82-89         |
|Shift key        | note  122         | note  98            |
|Faders 1-9       | CC 48-56          | CC 48-56 (the same) |

*/
#if (APC != 1) //regular apc mini mk2
#define APC_MINI_BUTTON_RIGHT_FIRST  112 //1st scene/vertical button
#define APC_MINI_BUTTON_BOTTOM_FIRST 100 //1st track/horizontal button
#else // for Akai APC Mini Mk1
#define APC_MINI_BUTTON_RIGHT_FIRST   82 //1st scene/vertical button
#define APC_MINI_BUTTON_BOTTOM_FIRST  64 //1st track/horizontal button
#endif

#define APC_MINI_BUTTON_RIGHT_PATTERN_EDITOR APC_MINI_BUTTON_RIGHT_FIRST     //112 or 82
#define APC_MINI_BUTTON_RIGHT_SONG_EDITOR    (APC_MINI_BUTTON_RIGHT_FIRST+1) //113 or 83
#define APC_MINI_BUTTON_RIGHT_MUTE_MATRIX    (APC_MINI_BUTTON_RIGHT_FIRST+2) //114 or 84

#define APC_MINI_BUTTON_RIGHT_PATTERN_UP     APC_MINI_BUTTON_RIGHT_FIRST     //112 or 82
#define APC_MINI_BUTTON_RIGHT_PATTERN_DOWN   (APC_MINI_BUTTON_RIGHT_FIRST+1) //113 or 83
#define APC_MINI_BUTTON_RIGHT_SEQ_START_STOP (APC_MINI_BUTTON_RIGHT_FIRST+2) //114 or 84
#define APC_MINI_BUTTON_RIGHT_VELOCITY_UPPER (APC_MINI_BUTTON_RIGHT_FIRST+3) //115 or 85
#define APC_MINI_BUTTON_RIGHT_VELOCITY_LOWER (APC_MINI_BUTTON_RIGHT_FIRST+4) //116 or 86
#define APC_MINI_BUTTON_RIGHT_DISAB1_PADSEL  (APC_MINI_BUTTON_RIGHT_FIRST+5) //117 or 87
#define APC_MINI_BUTTON_RIGHT_ENABLE_PADSEL  (APC_MINI_BUTTON_RIGHT_FIRST+6) //118 or 88
#define APC_MINI_BUTTON_RIGHT_DISAB2_PADSEL  (APC_MINI_BUTTON_RIGHT_FIRST+7) //119 or 89

#define APC_MINI_BUTTON_BOTTOM_SAMPLE_P8     (APC_MINI_BUTTON_BOTTOM_FIRST+4) //104 or 68, pad +8
#define APC_MINI_BUTTON_BOTTOM_SAMPLE_M8     (APC_MINI_BUTTON_BOTTOM_FIRST+5) //105 or 69, pad -8
#define APC_MINI_BUTTON_BOTTOM_SAMPLE_M1     (APC_MINI_BUTTON_BOTTOM_FIRST+6) //106 or 70, pad -1
#define APC_MINI_BUTTON_BOTTOM_SAMPLE_P1     (APC_MINI_BUTTON_BOTTOM_FIRST+7) //107 or 71, pad +1

#ifndef apc_h_
#define apc_h_
extern bool APC_BUTTONS_RIGHT[8];
void apc(uint8_t a, uint8_t b, uint8_t c);
void apc_clear_grid();
void apc_clear_right_buttons();
void print_apc_source_selection_pads();
void apc_print_right_buttons();
void apc_trigger_print_in_lcd(void);
void apc_print_volume_pads();
void apc_NoteOn(byte inChannel, byte inData1, byte inData2);
void apc_fader_control(uint8_t in1, uint8_t in2);
#endif
#endif
