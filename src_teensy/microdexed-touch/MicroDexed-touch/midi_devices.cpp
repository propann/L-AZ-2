/*
   MicroDexed

   MicroMDAEPiano is a port of the MDA-EPiano sound engine
   (https://sourceforge.net/projects/mda-vst/) for the Teensy-3.5/3.6/4.x with audio shield.

   (c)2019-2021 H. Wirtz <wirtz@parasitstudio.de>

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

#include "midi_devices.h"

#ifdef MIDI_DEVICE_USB_HOST
#include <USBHost_t36.h>
#endif
#include "sequencer.h"
#include "livesequencer.h"

// override default sysex size settings
struct MicroDexedSettings : public midi::DefaultSettings
{
  static const unsigned SysExMaxSize = 4104; // Accept SysEx messages up to 1024 bytes long.
  static const bool Use1ByteParsing = false;  //seems to improve rapid MIDI input from ARPs etc.
};

#ifdef MIDI_DEVICE_DIN
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, MIDI_DEVICE_DIN, midi_serial, MicroDexedSettings);
#endif

#define MAX_USB_MIDI_DEVICES 3

#ifdef MIDI_DEVICE_USB_HOST
USBHost usb_host;

// MIDIDevice midi_usb(usb_host);

MIDIDevice midi_usb_1(usb_host);
MIDIDevice midi_usb_2(usb_host);
MIDIDevice midi_usb_3(usb_host);

MIDIDevice* midi_usb_devices[MAX_USB_MIDI_DEVICES] = { &midi_usb_1, &midi_usb_2, &midi_usb_3 };

#define COUNT_JOYSTICKS 1
JoystickController joysticks[COUNT_JOYSTICKS]{ usb_host };
#endif

#ifdef USB_KEYPAD
KeyboardController keyboard1(usb_host);
#endif

USBHub hub1(usb_host);
USBHIDParser hid1(usb_host);

USBDriver* drivers[] = {
  &hub1,
  &joysticks[0],
  &joysticks[1],
  &joysticks[2],
  &joysticks[3],
  &midi_usb_1,
  &midi_usb_2,
  &midi_usb_3,
  &hid1 };

#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))

bool driver_active[CNT_DEVICES] = { false, false, false, false };

// Lets also look at HID Input devices
USBHIDInput* hiddrivers[] = { &joysticks[0], &joysticks[1], &joysticks[2], &joysticks[3] };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char* hid_driver_names[CNT_DEVICES] = { "joystick[0H]", "joystick[1H]", "joystick[2H]", "joystick[3H]" };
bool hid_driver_active[CNT_DEVICES] = { false };
bool show_changed_only = true;
uint64_t joystick_full_notify_mask = (uint64_t)-1;
int8_t counter_ignore_fader_apc;
uint8_t fader_previous_apc;
extern uint8_t chord_input[4];

/* #ifdef MIDI_DEVICE_USB
  static const unsigned sUsbTransportBufferSize = 16;
  typedef midi::UsbTransport<sUsbTransportBufferSize> UsbTransport;
  UsbTransport sUsbTransport;
  MIDI_CREATE_INSTANCE(UsbTransport, sUsbTransport, midi_onboard_usb);
  #endif */

#define MIDI_BY_DIN "MIDI_DIN"
#define MIDI_BY_USB_HOST "MIDI_USB_HOST"
#define MIDI_BY_USB "USB_MIDI"

#ifdef MIDI_ACTIVITY_LIGHTS
extern uint8_t LED_MIDI_IN_counter;
#endif

extern LiveSequencer liveSeq;
extern config_t configuration;
extern sequencer_t seq;
extern bool disableDirectMIDIinput;

extern void handleNoteOnInput(byte inChannel, byte inNumber, byte inVelocity, byte device);
extern void handleAfterTouch(byte inChannel, byte inPressure);
extern void handlePitchBend(byte inChannel, int16_t inPitch);
extern void handleControlChange(byte inChannel, byte inData1, byte inData2);
extern void handleProgramChange(byte inChannel, byte inProgram);
extern void handleAfterTouchPoly(byte inChannel, byte inNumber, byte inVelocity);
extern void handleSystemExclusive(byte* data, unsigned len);
extern void handleTimeCodeQuarterFrame(byte data);
extern void handleSongSelect(byte inSong);
extern void handleTuneRequest(void);
extern void handleClock(void);
extern void handleContinue(void);
extern void handleActiveSensing(void);
extern void handleSystemReset(void);
extern void handleStart(void);
extern void handleStop(void);
extern void handleNoteOn(byte inChannel, byte inNumber, byte inVelocity, byte device);
extern void handleNoteOff(byte inChannel, byte inNumber, byte inVelocity, byte device);

#ifdef MIDI_DEVICE_USB_HOST
// Helper to send to all USB MIDI devices
void midiSendToAllUSB(midi::MidiType event) {
  for (int i = 0; i < MAX_USB_MIDI_DEVICES; i++) {
    if (midi_usb_devices[i]) {
      midi_usb_devices[i]->sendRealTime(event);
    }
  }
}

// Helper to check all USB MIDI devices
void checkAllUSBMIDI() {
  for (int i = 0; i < MAX_USB_MIDI_DEVICES; i++) {
    if (midi_usb_devices[i]) {
      midi_usb_devices[i]->read();
    }
  }
}
#endif


FLASHMEM void midiSendRealtime(midi::MidiType event) {
#ifdef MIDI_DEVICE_USB_HOST
  // midi_usb.sendRealTime(event);
  midiSendToAllUSB(event);
#endif

#ifdef MIDI_DEVICE_DIN
  midi_serial.sendRealTime(event);
#endif

#ifdef MIDI_DEVICE_USB
  usbMIDI.sendRealTime(event);
#endif
}

void midiNoteOn(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t device) {
  switch (device) {
#ifdef MIDI_DEVICE_USB_HOST
  case 1:
    midi_usb_1.sendNoteOn(note, velocity, channel);
    midi_usb_2.sendNoteOn(note, velocity, channel);
    midi_usb_3.sendNoteOn(note, velocity, channel);
    break;
#endif

#ifdef MIDI_DEVICE_DIN
  case 2:
    midi_serial.sendNoteOn(note, velocity, channel);
    break;
#endif

  case 3:
    usbMIDI.sendNoteOn(note, velocity, channel);
    break;
  }
}

void midiNoteOff(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t device) {
  switch (device) {
#ifdef MIDI_DEVICE_USB_HOST
  case 1:
    midi_usb_1.sendNoteOff(note, velocity, channel);
    midi_usb_2.sendNoteOff(note, velocity, channel);
    midi_usb_3.sendNoteOff(note, velocity, channel);
    break;
#endif

#ifdef MIDI_DEVICE_DIN
  case 2:
    midi_serial.sendNoteOff(note, velocity, channel);
    break;
#endif

  case 3:
    usbMIDI.sendNoteOff(note, velocity, channel);
    break;
  }
}

void midiPitchBend(int pitch, uint8_t channel, uint8_t device) {
  switch (device) {
#ifdef MIDI_DEVICE_USB_HOST
  case 1:
    midi_usb_1.sendPitchBend(pitch, channel);
    midi_usb_2.sendPitchBend(pitch, channel);
    midi_usb_3.sendPitchBend(pitch, channel);
    break;
#endif

#ifdef MIDI_DEVICE_DIN
  case 2:
    midi_serial.sendPitchBend(pitch, channel);
    break;
#endif

  case 3:
    usbMIDI.sendPitchBend(pitch, channel);
    break;

  case 0:
    // Handle MDT devices
    handlePitchBend(channel, pitch);
    break;

  default:
#ifdef DEBUG
    LOG.printf_P(PSTR("Unknown device for PB: %d"), device);
#endif
    break;
  }
}

FLASHMEM void midiControlChange(uint8_t control, uint8_t value, uint8_t channel, uint8_t device) {
  switch (device) {
#ifdef MIDI_DEVICE_USB_HOST
  case 1:
    midi_usb_1.sendControlChange(control, value, channel);
    midi_usb_2.sendControlChange(control, value, channel);
    midi_usb_3.sendControlChange(control, value, channel);
    break;
#endif

#ifdef MIDI_DEVICE_DIN
  case 2:
    midi_serial.sendControlChange(control, value, channel);
    break;
#endif

  case 3:
    usbMIDI.sendControlChange(control, value, channel);
    break;

  case 0:
    // Handle MDT devices
    handleControlChange(channel, control, value);
    break;

  default:
#ifdef DEBUG
    LOG.printf_P(PSTR("Unknown device for CC: %d"), device);
#endif
    break;
  }
}

#if defined APC

#if (APC != 1) //LUC regular apc mini mk2
#define APC_MINI_BUTTON_SHIFT 122
#else
#define APC_MINI_BUTTON_SHIFT 98
#endif

extern uint8_t apc_out_channel;
bool apc_shift_key = false;
extern uint8_t APC_TONAL_SCROLL_OFFSET;
extern void apc_print_right_buttons();
extern void apc_NoteOn(byte inChannel, byte inData1, byte inData2);
extern void apc_trigger_print_in_lcd(void);
extern uint8_t APC_SAMPLE_SCROLL_OFFSET;
extern uint8_t APC_MODE;
extern void print_apc_source_selection_pads();
extern void apc_fader_control(uint8_t in1, uint8_t in2);

extern void start_MIDI_input_timer();
extern void stop_MIDI_input_timer();

FLASHMEM void apc(uint8_t a, uint8_t b, uint8_t c)
{
#if (APC != 1) //LUC regular apc mini mk2
  midi_usb_1.sendNoteOn(a, b, c);
#else
  static uint8_t a_old = 255, b_old = 255;
  //a=inNoteNumber, b=inVelocity (128 colour), c=inChannel (brightness,blink)
  if ((a_old != a) || (b_old != b)) { //avoid repeating same command without meaning, to avoid freeze, but it still freezes
    a_old = a; b_old = b;
    switch (b) { //wrapper for colour from APC Mini Mk2 to Mk1
    case 0: break; //OFF colour is the same for Mk2 and Mk1
    case 5:        //red Mk2
      b = 3;break;   //red for Mk1
    case 1:        //colour #1E1E1E Mk2 for mute matrix
      b = 5;break;   //yellow for Mk1
    case 78:       //colour #00A9FF Mk2 for mute matrix
      b = 1;break;   //green for Mk1
    case 2:        //colour #7F7F7F Mk2 for mute matrix for instr pattern
      b = 5 + 1;break; //yellow blink for Mk1
    case 40:       //colour #4C88FF Mk2 for mute matrix for instr pattern
      b = 1 + 1;break; //green blink for Mk1
    case 124:      //colour #B9B000 Mk2 for mute matrix for drum pattern
      b = 3 + 1;break; //red blink for Mk1
    case 36:       //colour #4CC3FF Mk2 for mute matrix for pitched sample
      b = 3;break;   //red for Mk1
    default:       //for several sample pads colours put all 3 mk1 colors in round robin order
      b = (b % 3) * 2 + 1;break;//get 1,3 or 5 (green,red,yellow) from any colour number for Mk1
    }
    //increment colour for blink mode request (1,3,5 to 2,4,6), 11 is blink for Mk2
    //avoid blink if colour is 0 (OFF) or if colour is already blinking (even number)
    b += ((c == 11) && b && (b & 1u)) ? 1 : 0;
    midi_usb_1.sendNoteOn(a, b, 0); //LUC apc mini mk1 doesn't have brightness control, so set channel to 0, only channel 0 is used
  }
  else {
    a_old = a; b_old = b;
  }
#endif
}

#endif

uint8_t chord_input[MAX_CHORD_NOTES] = { 0 };
uint8_t chord_note_count = 0;
elapsedMillis since_last_note;
extern void printSuggestions();
extern void  show_chord();

FLASHMEM void resetChordBuffer() {
  memset(chord_input, 0, sizeof(chord_input));
  chord_note_count = 0;
}

extern void resetCurrentChord();
extern void resetChordTransformation();
extern ChordInfo currentChord;
extern void updateCurrentChord(const uint8_t midiNotes[], uint8_t size);
extern bool chord_autostart;

void handle_generic(byte inChannel, byte inData1, byte inData2, const char* midi_device, midi::MidiType event)
{

 
  liveSeq.handleMidiEvent(inChannel, event, inData1, inData2);

if (disableDirectMIDIinput)
return;

#ifdef DEBUG
  char text[10];
#endif
  bool _internal = false;

  switch (event)
  {
  case midi::NoteOn:
    if (seq.arr_split_note == 0 || inData1 > seq.arr_split_note)
    {
      seq.note_in = inData1;
      seq.note_in_velocity = inData2;
    }
    else   //live chord arranger
    {

      // Shift all notes down (making room for new note)
         // memmove(chord_input, chord_input+1, 3);  // Moves elements 1,2,3 to 0,1,2
         // chord_input[3] = inData1;  // Add new note at end


      if (!seq.running)
      {
        seq.note_in = inData1;
        seq.note_in_velocity = inData2;
      }

      // Chord note handling
      if (since_last_note > CHORD_CAPTURE_WINDOW || chord_note_count >= MAX_CHORD_NOTES) {
        resetChordBuffer();
        resetCurrentChord();
      }

      // Store the note (sorted insertion)
      for (int i = 0; i < MAX_CHORD_NOTES; i++) {
        if (chord_input[i] == 0) { // Empty slot
          chord_input[i] = inData1;
          break;
        }
        if (inData1 < chord_input[i]) { // Insert in order
          memmove(&chord_input[i + 1], &chord_input[i], MAX_CHORD_NOTES - i - 1);
          chord_input[i] = inData1;
          break;
        }
      }

      chord_note_count++;
      since_last_note = 0;

      // Process chord if we have enough notes
      if (chord_note_count >= 3) {
        updateCurrentChord(chord_input, min(chord_note_count, MAX_CHORD_NOTES));
        printSuggestions();
        show_chord();
        if (chord_autostart && !seq.running)
        {
          handleStart();
        }

        resetChordTransformation();
      }
    }

#if defined APC

    if (strcmp(MIDI_BY_USB, midi_device) && inChannel == 1)
    {
      if (inData1 == APC_MINI_BUTTON_SHIFT) //shift key LUC 122 or 98
        apc_shift_key = true;
      if (APC_MODE == APC_PATTERN_EDITOR)
      {
        apc_NoteOn(inChannel, inData1, inData2);
        if ((inData1 > 7 && inData1 < 16) && (seq.content_type[seq.active_pattern] == 0 || seq.content_type[seq.active_pattern] == 3)) // drum sample
          inData1 = inData1 + APC_SAMPLE_SCROLL_OFFSET;
        else if ((inData1 < 24) && (seq.content_type[seq.active_pattern] == 1 || seq.content_type[seq.active_pattern] == 2)) // tonal instrument
          inData1 = inData1 + APC_TONAL_SCROLL_OFFSET;

        inChannel = apc_out_channel;
      }
      else
        if (APC_MODE == APC_MUTE_MATRIX)
        {
          apc_NoteOn(inChannel, inData1, inData2);
        }
        else
          if (APC_MODE == APC_SONG)
          {
            apc_NoteOn(inChannel, inData1, inData2);

          }
    }
#endif

    if (seq.arr_split_note == 0 || inData1 > seq.arr_split_note)
      handleNoteOnInput(inChannel, inData1, inData2, 0);
    else if (!seq.running)
    {
      handleNoteOnInput(inChannel, inData1, inData2, 0);
    }

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
      print_apc_source_selection_pads();
#endif

#ifdef DEBUG
    strcpy(text, "NoteOn");
#endif
    break;
  case midi::NoteOff:
    if (seq.arr_split_note == 0 || inData1 > seq.arr_split_note)
    {
      seq.note_in = inData1;
      seq.note_in_velocity = inData2;
    }
    else
    {
      if (!seq.running)
      {
        seq.note_in = inData1;
        seq.note_in_velocity = inData2;
      }

      // Only remove the released note from the buffer
      for (int i = 0; i < MAX_CHORD_NOTES; i++) {
        if (chord_input[i] == inData1) {
          memmove(&chord_input[i], &chord_input[i + 1], MAX_CHORD_NOTES - i - 1);
          chord_input[MAX_CHORD_NOTES - 1] = 0;
          chord_note_count--;
          break;
        }
      }

      // // If we dropped below 3 notes, reset chord state
      // if (chord_note_count < 3) {
      //     resetCurrentChord();
      // }
    }

#if defined APC
    if (inData1 == APC_MINI_BUTTON_SHIFT) //shift key LUC 122 or 98
      apc_shift_key = false;
    if (APC_MODE == APC_PATTERN_EDITOR)
    {
      if ((inData1 < 24) && (seq.content_type[seq.active_pattern] == 1 || seq.content_type[seq.active_pattern] == 2)) // tonal instrument
        inData1 = inData1 + APC_TONAL_SCROLL_OFFSET;
      apc_print_right_buttons();
    }
#endif

    if (seq.arr_split_note == 0 || inData1 > seq.arr_split_note)
      handleNoteOff(inChannel, inData1, inData2, 0);
    else if (!seq.running)
      handleNoteOff(inChannel, inData1, inData2, 0);

#ifdef DEBUG
    strcpy(text, "NoteOff");
#endif
    break;
  case midi::ControlChange:
    // Internal CC ?
    if (inData1 >= 20 && inData1 <= 31)
      _internal = true;

#if defined APC
    if (APC_MODE == APC_PATTERN_EDITOR)
    {
      if (counter_ignore_fader_apc <= 0)
      {
        if (strcmp(MIDI_BY_USB, midi_device) && inData1 >= 48 && inData1 < 56 && inChannel == 1) //fader 56 (global volume?) is not used
        {
          apc_fader_control(inData1, inData2);
          if (fader_previous_apc != inData1) //put delay if other fader is moved
#if (APC != 1) //LUC regular apc mini mk2
            counter_ignore_fader_apc = 0; //Before compared with 30, put to 0 to avoid delay.
#else
            /* Test conditions: seq stop, sample pad blinking. Test: move all APC faders.
              With counter 1 it freezes after some attempts, with 10 freeze after more attempts, with 30 it's harder to freeze.
              Only the UI freezes.
              If seq stopped and I move all faders crazily, it freezes and I'm unable to change screen, start sequencer.
              If seq started and I move all faders crazily, it freezes and I'm unable to change screen, stop sequencer. It keeps playing infinitely! */
            counter_ignore_fader_apc = 30;
          else
            counter_ignore_fader_apc = 1; //needed because MDT got frozen when moving 4 faders
#endif
          fader_previous_apc = inData1;
        }
      }
      else counter_ignore_fader_apc--; //dirty fix delay to ignore some received fader messages, to avoid spam, to reduce chance of freeze
    }
#endif

    handleControlChange(inChannel, inData1, inData2);
#ifdef DEBUG
    strcpy(text, "CC");
#endif
    break;
  case midi::AfterTouchChannel:
    handleAfterTouch(inChannel, inData1);
#ifdef DEBUG
    strcpy(text, "Mono AT");
#endif
    break;
  case midi::PitchBend:
    handlePitchBend(inChannel, ((inData1 & 0x7F) | ((inData2 & 0x7F) << 7)) - 8192); // Combine LSB and MSB as per MIDI spec. Center the value around 0
#ifdef DEBUG
    strcpy(text, "PB");
#endif
    break;
  case midi::ProgramChange:
    handleProgramChange(inChannel, inData1);
#ifdef DEBUG
    strcpy(text, "PC");
#endif
    break;
  case midi::AfterTouchPoly:
    handleAfterTouchPoly(inChannel, inData1, inData2);
#ifdef DEBUG
    strcpy(text, "Poly AT");
#endif
    break;
  default:
    break;
  }
#ifdef DEBUG
  LOG.printf_P(PSTR("MIDI handle_generic [%s] by [%s], ch:%d d1:%d d2:%d"), text, midi_device, inChannel, inData1, inData2);
#endif

#ifdef MIDI_ACTIVITY_LIGHTS
  LED_MIDI_IN_counter = 1;
#endif

  // MIDI THRU (only for non _internal MDT)
  if (configuration.sys.soft_midi_thru == 1 && !_internal)
  {
#ifdef MIDI_DEVICE_USB
    if (strcmp(MIDI_BY_USB, midi_device))
    {
      switch (event)
      {
      case midi::NoteOn:
        usbMIDI.sendNoteOn(inData1, inData2, inChannel);
        break;
      case midi::NoteOff:
        usbMIDI.sendNoteOff(inData1, inData2, inChannel);
        break;
      case midi::ControlChange:
        usbMIDI.sendControlChange(inData1, inData2, inChannel);
        break;
      case midi::AfterTouchChannel:
        usbMIDI.sendAfterTouch(inData1, inChannel);
        break;
      case midi::PitchBend:
        usbMIDI.sendPitchBend(inData1, inChannel);
        break;
      case midi::ProgramChange:
        usbMIDI.sendProgramChange(inData1, inChannel);
        break;
      case midi::AfterTouchPoly:
        usbMIDI.sendAfterTouch(inData1, inData2, inChannel);
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_DIN
    if (strcmp(MIDI_BY_DIN, midi_device))
    {
      switch (event)
      {
      case midi::NoteOn:
        midi_serial.sendNoteOn(inData1, inData2, inChannel);
        break;
      case midi::NoteOff:
        midi_serial.sendNoteOff(inData1, inData2, inChannel);
        break;
      case midi::ControlChange:
        midi_serial.sendControlChange(inData1, inData2, inChannel);
        break;
      case midi::AfterTouchChannel:
        midi_serial.sendAfterTouch(inData1, inChannel);
        break;
      case midi::PitchBend:
        midi_serial.sendPitchBend(((inData1 & 0x7F) | ((inData2 & 0x7F) << 7)) - 8192, inChannel); // Combine LSB and MSB as per MIDI spec. Center the value around 0
        break;
      case midi::ProgramChange:
        midi_serial.sendProgramChange(inData1, inChannel);
        break;
      case midi::AfterTouchPoly:
        midi_serial.sendAfterTouch(inData1, inData2, inChannel);
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_DIN"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_USB_HOST
    if (strcmp(MIDI_BY_USB_HOST, midi_device))
    {
      switch (event)
      {
      case midi::NoteOn:
        midi_usb_1.sendNoteOn(inData1, inData2, inChannel);
        midi_usb_2.sendNoteOn(inData1, inData2, inChannel);
        midi_usb_3.sendNoteOn(inData1, inData2, inChannel);
        break;
      case midi::NoteOff:
        midi_usb_1.sendNoteOff(inData1, inData2, inChannel);
        midi_usb_2.sendNoteOff(inData1, inData2, inChannel);
        midi_usb_3.sendNoteOff(inData1, inData2, inChannel);
        break;
      case midi::ControlChange:
        midi_usb_1.sendControlChange(inData1, inData2, inChannel);
        midi_usb_2.sendControlChange(inData1, inData2, inChannel);
        midi_usb_3.sendControlChange(inData1, inData2, inChannel);
        break;
      case midi::AfterTouchChannel:
        midi_usb_1.sendAfterTouch(inData1, inChannel);
        midi_usb_2.sendAfterTouch(inData1, inChannel);
        midi_usb_3.sendAfterTouch(inData1, inChannel);
        break;
      case midi::PitchBend:
        midi_usb_1.sendPitchBend(inData1, inChannel);
        midi_usb_2.sendPitchBend(inData1, inChannel);
        midi_usb_3.sendPitchBend(inData1, inChannel);
        break;
      case midi::ProgramChange:
        midi_usb_1.sendProgramChange(inData1, inChannel);
        midi_usb_2.sendProgramChange(inData1, inChannel);
        midi_usb_3.sendProgramChange(inData1, inChannel);
        break;
      case midi::AfterTouchPoly:
        midi_usb_1.sendAfterTouch(inData1, inData2, inChannel);
        midi_usb_2.sendAfterTouch(inData1, inData2, inChannel);
        midi_usb_3.sendAfterTouch(inData1, inData2, inChannel);
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB_HOST"));
#endif
    }
#endif
  }

#ifdef DEBUG
  LOG.println();
#endif
}

int toPitchBend(uint8_t lsb, uint8_t msb) {
  return ((int16_t)((msb << 7) | lsb) - 8192);
}

void handle_generic_by_pitch(byte inChannel, int inPitch, const char* midi_device, midi::MidiType event)
{
  // Split 14-bit pitch value into LSB and MSB as per MIDI spec
  int inPitchCentered = inPitch + 8192; // Center the value around 0
  byte lsb = inPitchCentered & 0x7F;
  byte msb = (inPitchCentered >> 7) & 0x7F;
  handle_generic(inChannel, lsb, msb, midi_device, event);
}

FLASHMEM void handleSystemExclusive_generic(byte* data, uint len, const char* midi_device)
{
  handleSystemExclusive(data, len);
#ifdef DEBUG
  LOG.printf_P(PSTR("[%s] SysEx"), midi_device);
#endif

  // MIDI THRU
  if (configuration.sys.soft_midi_thru == 1)
  {
#ifdef MIDI_DEVICE_USB
    if (strcmp(MIDI_BY_USB, midi_device))
    {
      usbMIDI.sendSysEx(len, data);
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_DIN
    if (strcmp(MIDI_BY_DIN, midi_device))
    {
      midi_serial.sendSysEx(len, data);
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_DIN"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_USB_HOST
    if (strcmp(MIDI_BY_USB_HOST, midi_device))
    {
      midi_usb_1.sendSysEx(len, data);
      midi_usb_2.sendSysEx(len, data);
      midi_usb_2.sendSysEx(len, data);
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB_HOST"));
#endif
    }
#endif
  }

#ifdef DEBUG
  LOG.println();
#endif
}

FLASHMEM void handleSystemCommon_generic(byte inData1, const char* midi_device, midi::MidiType event)
{
  //char text[10];

  switch (event)
  {
  case midi::TimeCodeQuarterFrame:
    handleTimeCodeQuarterFrame(inData1);
    //strcpy(text, "TimeCodeQuarterFrame");
    break;
  case midi::SongSelect:
    handleSongSelect(inData1);
    //strcpy(text, "SongSelect");
    break;
  case midi::TuneRequest:
    handleTuneRequest();
    //strcpy(text, "TuneRequest");
    break;
  default:
    break;
  }
#ifdef DEBUG
  //LOG.printf_P(PSTR("[%s] %s"), midi_device, text);
#endif

  // MIDI THRU
  if (configuration.sys.soft_midi_thru == 1)
  {
#ifdef MIDI_DEVICE_USB
    if (strcmp(MIDI_BY_USB, midi_device))
    {
      switch (event)
      {
      case midi::TimeCodeQuarterFrame:
        usbMIDI.sendTimeCodeQuarterFrame(0xF1, inData1);
        break;
      case midi::SongSelect:
        usbMIDI.sendSongSelect(inData1);
        break;
      case midi::TuneRequest:
        usbMIDI.sendTuneRequest();
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_DIN
    if (strcmp(MIDI_BY_DIN, midi_device))
    {
      switch (event)
      {
      case midi::TimeCodeQuarterFrame:
        midi_serial.sendTimeCodeQuarterFrame(inData1);
        break;
      case midi::SongSelect:
        midi_serial.sendSongSelect(inData1);
        break;
      case midi::TuneRequest:
        midi_serial.sendTuneRequest();
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_DIN"));
#endif
    }
#endif

#ifdef MIDI_DEVICE_USB_HOST
    if (strcmp(MIDI_BY_USB_HOST, midi_device))
    {
      switch (event)
      {
      case midi::TimeCodeQuarterFrame:
        midi_usb_1.sendTimeCodeQuarterFrame(0xF1, inData1);
        break;
      case midi::SongSelect:
        midi_usb_1.sendSongSelect(inData1);
        break;
      case midi::TuneRequest:
        midi_usb_1.sendTuneRequest();
        break;
      default:
        break;
      }
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB_HOST"));
#endif
    }
#endif
  }

#ifdef DEBUG
  LOG.println();
#endif
}

void handleRealtime_generic(const char* midi_device, midi::MidiType event)
{
  //char text[16];

  switch (event)
  {
  case midi::Clock:
    handleClock();
    //strcpy(text, "Clock");
    break;
  case midi::Start:
    handleStart();
    //strcpy(text, "Start");
    break;
  case midi::Continue:
    handleContinue();
    //strcpy(text, "Continue");
    break;
  case midi::Stop:
    handleStop();
    //strcpy(text, "Stop");
    break;
  case midi::ActiveSensing:
    handleActiveSensing();
    //strcpy(text, "ActiveSensing");
    break;
  case midi::SystemReset:
    handleSystemReset();
    //strcpy(text, "SystemReset");
    break;
  default:
    break;
  }
  // #ifdef DEBUG
  //   LOG.printf_P(PSTR("[%s] %s"), midi_device, text);
  // #endif

    // MIDI THRU
  if (configuration.sys.soft_midi_thru == 1)
  {
#ifdef MIDI_DEVICE_USB
    if (strcmp(MIDI_BY_USB, midi_device))
    {
      usbMIDI.sendRealTime(event);
      // #ifdef DEBUG
      //       LOG.print(F(" THRU->MIDI_USB"));
      // #endif
    }
#endif

#ifdef MIDI_DEVICE_DIN
    if (strcmp(MIDI_BY_DIN, midi_device))
    {
      midi_serial.sendRealTime(event);
      // #ifdef DEBUG
      //       LOG.print(F(" THRU->MIDI_DIN"));
      // #endif
    }
#endif

#ifdef MIDI_DEVICE_USB_HOST
    if (strcmp(MIDI_BY_USB_HOST, midi_device))
    {
      midi_usb_1.sendRealTime(event);
      midi_usb_2.sendRealTime(event);
      midi_usb_3.sendRealTime(event);
#ifdef DEBUG
      LOG.print(F(" THRU->MIDI_USB_HOST"));
#endif
    }
#endif
  }

  // #ifdef DEBUG
  //   LOG.println();
  // #endif
}

///* void handleSystemExclusiveChunk_MIDI_DEVICE_DIN(byte *data, uint len, bool last)

// void handlRealTimeSystem_generic(byte inRealTime, byte midi_device) {
//   handleRealTimeSystem();
// #ifdef DEBUG
//   switch(midi_device) {
//     case MIDI_DIN:
//       LOG.print(F("[MIDI_DIN] RealTimeSystem"));
//       break;
//     case MIDI_USB_HOST:
//       LOG.print(F("[MIDI_USB_HOST] RealTimeSystem"));
//       break;
//     case USB_MIDI:
//       LOG.print(F("[USB_MIDI] RealTimeSystem"));
//       break;
//   }
// #endif
//   if (configuration.sys.soft_midi_thru == 1)
//   {
// #ifdef MIDI_DEVICE_USB
//     if(midi_device != USB_MIDI) {
//         usbMIDI.sendRealTime(inRealTime);
//   #ifdef DEBUG
//         LOG.print(F(" THRU->MIDI_USB"));
//   #endif
//     }
// #endif

// #ifdef MIDI_DEVICE_DIN
//     if(midi_device != MIDI_DIN) {
//       midi_serial.sendRealTime((midi::MidiType)inRealTime);
//   #ifdef DEBUG
//         LOG.print(F(" THRU->MIDI_DIN"));
//   #endif
//     }
// #endif

// #ifdef MIDI_DEVICE_USB_HOST
//     if(midi_device != MIDI_USB_HOST) {
//         midi_usb.sendRealTime(inRealTime);
//   #ifdef DEBUG
//         LOG.print(F(" THRU->MIDI_USB_HOST"));
//   #endif
//     }
// #endif
//   }

// #ifdef DEBUG
//   LOG.println();
// #endif
// }


/*****************************************
   MIDI_DEVICE_DIN
 *****************************************/
#ifdef MIDI_DEVICE_DIN

void handleNoteOn_MIDI_DEVICE_DIN(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_DIN, midi::NoteOn);
}

void handleNoteOff_MIDI_DEVICE_DIN(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_DIN, midi::NoteOff);
}

void handleControlChange_MIDI_DEVICE_DIN(byte inChannel, byte inData1, byte inData2)
{
  handle_generic(inChannel, inData1, inData2, MIDI_BY_DIN, midi::ControlChange);
}

void handleAfterTouch_MIDI_DEVICE_DIN(byte inChannel, byte inPressure)
{
  handle_generic(inChannel, inPressure, '\0', MIDI_BY_DIN, midi::AfterTouchChannel);
}

void handlePitchBend_MIDI_DEVICE_DIN(byte inChannel, int inPitch)
{
  handle_generic_by_pitch(inChannel, inPitch, MIDI_BY_DIN, midi::PitchBend);
}

void handleProgramChange_MIDI_DEVICE_DIN(byte inChannel, byte inProgram)
{
  handle_generic(inChannel, inProgram, '\0', MIDI_BY_DIN, midi::ProgramChange);
}

void handleAfterTouchPoly_MIDI_DEVICE_DIN(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_DIN, midi::AfterTouchPoly);
}

void handleSystemExclusive_MIDI_DEVICE_DIN(byte* data, uint len)
{
  handleSystemExclusive_generic(data, len, MIDI_BY_DIN);
}

/* void handleSystemExclusiveChunk_MIDI_DEVICE_DIN(byte *data, uint len, bool last)
  {
  handleSystemExclusiveChunk(data, len, last);
  #ifdef DEBUG
  LOG.print(F("[MIDI_DIN] SysExChunk"));
  #endif
    if (configuration.sys.soft_midi_thru == 1)
  {
  #ifdef MIDI_DEVICE_USB_HOST
  midi_usb.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_USB_HOST"));
  #endif
  #endif
  #ifdef MIDI_DEVICE_USB
  usbMIDI.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_USB"));
  #endif
  #endif
  }
  #ifdef DEBUG
  LOG.println();
  #endif
  } */

void handleTimeCodeQuarterFrame_MIDI_DEVICE_DIN(byte data)
{
  handleSystemCommon_generic(data, MIDI_BY_DIN, midi::TimeCodeQuarterFrame);
}

void handleSongSelect_MIDI_DEVICE_DIN(byte inSong)
{
  handleSystemCommon_generic(inSong, MIDI_BY_DIN, midi::SongSelect);
}

void handleTuneRequest_MIDI_DEVICE_DIN(void)
{
  handleSystemCommon_generic('\0', MIDI_BY_DIN, midi::TuneRequest);
}

void handleClock_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::Clock);
}

void handleStart_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::Start);
}

void handleContinue_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::Continue);
}

void handleStop_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::Stop);
}

void handleActiveSensing_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::ActiveSensing);
}

void handleSystemReset_MIDI_DEVICE_DIN(void)
{
  handleRealtime_generic(MIDI_BY_DIN, midi::SystemReset);
}

/* void handlRealTimeSysteme_MIDI_DEVICE_DIN(byte inRealTime)
  {
  handleRealTimeSystem_generic(MIDI_DIN);
  } */
#endif // MIDI_DEVICE_DIN

  /*****************************************
     MIDI_DEVICE_USB_HOST
   *****************************************/
#ifdef MIDI_DEVICE_USB_HOST
void handleNoteOn_MIDI_DEVICE_USB_HOST(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB_HOST, midi::NoteOn);
}

void handleNoteOff_MIDI_DEVICE_USB_HOST(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB_HOST, midi::NoteOff);
}

void handleControlChange_MIDI_DEVICE_USB_HOST(byte inChannel, byte inData1, byte inData2)
{
  handle_generic(inChannel, inData1, inData2, MIDI_BY_USB_HOST, midi::ControlChange);
}

void handleAfterTouch_MIDI_DEVICE_USB_HOST(byte inChannel, byte inPressure)
{
  handle_generic(inChannel, inPressure, '\0', MIDI_BY_USB_HOST, midi::AfterTouchChannel);
}

void handlePitchBend_MIDI_DEVICE_USB_HOST(byte inChannel, int inPitch)
{
  handle_generic_by_pitch(inChannel, inPitch, MIDI_BY_USB_HOST, midi::PitchBend);
}

void handleProgramChange_MIDI_DEVICE_USB_HOST(byte inChannel, byte inPrg)
{
  handle_generic(inChannel, inPrg, '\0', MIDI_BY_USB_HOST, midi::ProgramChange);
}

void handleAfterTouchPoly_MIDI_DEVICE_USB_HOST(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB_HOST, midi::AfterTouchPoly);
}

void handleSystemExclusive_MIDI_DEVICE_USB_HOST(byte* data, uint len)
{
  handleSystemExclusive_generic(data, len, MIDI_BY_USB_HOST);
}

/* void handleSystemExclusiveChunk_MIDI_DEVICE_USB_HOST(byte *data, uint len, bool last)
  {
  handleSystemExclusiveChunk(data, len, last);
  #ifdef DEBUG
  LOG.print(F("[MIDI_USB_HOST] SysExChunk"));
  #endif
    if (configuration.sys.soft_midi_thru == 1)
  {
  #ifdef MIDI_DEVICE_DIN
  midi_serial.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_DIN"));
  #endif
  #endif
  #ifdef MIDI_DEVICE_USB
  usbMIDI.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_USB"));
  #endif
  #endif
  }
  #ifdef DEBUG
  LOG.println();
  #endif
  } */

void handleTimeCodeQuarterFrame_MIDI_DEVICE_USB_HOST(midi::DataByte data)
{
  handleSystemCommon_generic(data, MIDI_BY_USB_HOST, midi::TimeCodeQuarterFrame);
}

void handleSongSelect_MIDI_DEVICE_USB_HOST(byte inSong)
{
  handleSystemCommon_generic(inSong, MIDI_BY_USB_HOST, midi::SongSelect);
}

void handleTuneRequest_MIDI_DEVICE_USB_HOST(void)
{
  handleSystemCommon_generic('\0', MIDI_BY_USB_HOST, midi::TuneRequest);
}

void handleClock_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::Clock);
}

void handleStart_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::Start);
}

void handleContinue_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::Continue);
}

void handleStop_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::Stop);
}

void handleActiveSensing_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::ActiveSensing);
}

void handleSystemReset_MIDI_DEVICE_USB_HOST(void)
{
  handleRealtime_generic(MIDI_BY_USB_HOST, midi::SystemReset);
}

/* void handlRealTimeSystem_MIDI_DEVICE_USB_HOST(midi::MidiType inRealTime)
  {
  handleRealTimeSystem_generic(inRealTime, MIDI_USB_HOST);
  } */
#endif // MIDI_DEVICE_USB_HOST

  /*****************************************
     MIDI_DEVICE_USB
   *****************************************/
#ifdef MIDI_DEVICE_USB
void handleNoteOn_MIDI_DEVICE_USB(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB, midi::NoteOn);
}

void handleNoteOff_MIDI_DEVICE_USB(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB, midi::NoteOff);
}

void handleControlChange_MIDI_DEVICE_USB(byte inChannel, byte inData1, byte inData2)
{
  handle_generic(inChannel, inData1, inData2, MIDI_BY_USB, midi::ControlChange);
}

void handleAfterTouch_MIDI_DEVICE_USB(byte inChannel, byte inPressure)
{
  handle_generic(inChannel, inPressure, '\0', MIDI_BY_USB, midi::AfterTouchChannel);
}

void handlePitchBend_MIDI_DEVICE_USB(byte inChannel, int inPitch)
{
  handle_generic_by_pitch(inChannel, inPitch, MIDI_BY_USB, midi::PitchBend);
}

void handleProgramChange_MIDI_DEVICE_USB(byte inChannel, byte inProgram)
{
  handle_generic(inChannel, inProgram, '\0', MIDI_BY_USB, midi::ProgramChange);
}

void handleAfterTouchPoly_MIDI_DEVICE_USB(byte inChannel, byte inNoteNumber, byte inVelocity)
{
  handle_generic(inChannel, inNoteNumber, inVelocity, MIDI_BY_USB, midi::AfterTouchPoly);
}

void handleSystemExclusive_MIDI_DEVICE_USB(byte* data, uint len)
{
  handleSystemExclusive_generic(data, len, MIDI_BY_USB);
}

/* FLASHMEM void handleSystemExclusiveChunk_MIDI_DEVICE_USB(byte *data, uint len, bool last)
  {
  handleSystemExclusiveChunk(data, len, last);
  #ifdef DEBUG
  LOG.print(F("[MIDI_USB] SysExChunk"));
  #endif
    if (configuration.sys.soft_midi_thru == 1)
  {
  #ifdef MIDI_DEVICE_DIN
  midi_serial.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_DIN"));
  #endif
  #endif
  #ifdef MIDI_DEVICE_USB_HOST
  midi_usb.sendSysEx(len, data, last);
  #ifdef DEBUG
  LOG.print(F(" THRU->MIDI_USB_HOST"));
  #endif
  #endif
  }
  #ifdef DEBUG
  LOG.println();
  #endif
  } */

void handleTimeCodeQuarterFrame_MIDI_DEVICE_USB(midi::DataByte data)
{
  handleSystemCommon_generic(data, MIDI_BY_USB, midi::TimeCodeQuarterFrame);
}

void handleSongSelect_MIDI_DEVICE_USB(byte inSong)
{
  handleSystemCommon_generic(inSong, MIDI_BY_USB, midi::SongSelect);
}

void handleTuneRequest_MIDI_DEVICE_USB(void)
{
  handleSystemCommon_generic('\0', MIDI_BY_USB, midi::TuneRequest);
}

void handleClock_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::Clock);
}

void handleStart_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::Start);
}

void handleContinue_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::Continue);
}

void handleStop_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::Stop);
}

void handleActiveSensing_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::ActiveSensing);
}

void handleSystemReset_MIDI_DEVICE_USB(void)
{
  handleRealtime_generic(MIDI_BY_USB, midi::SystemReset);
}

/* FLASHMEM void handleRealTimeSystem_MIDI_DEVICE_USB(byte inRealTime)
  {
  handleRealTimeSystem_generic(inRealTime, USB_MIDI);
  } */
#endif // MIDI_DEVICE_USB

FLASHMEM void MD_sendControlChange(uint8_t channel, uint8_t cc, uint8_t value)
{
#ifdef DEBUG
  LOG.print(F("[MD] SendControlChange CH:"));
  LOG.print(channel, DEC);
  LOG.print(F(" CC:"));
  LOG.print(cc);
  LOG.print(F(" VAL:"));
  LOG.print(value);
#endif
#ifdef MIDI_DEVICE_DIN
  midi_serial.sendControlChange(cc, value, channel);
#ifdef DEBUG
  LOG.print(F(" MIDI-DIN"));
#endif
#endif
#ifdef MIDI_DEVICE_USB_HOST
  midi_usb_1.sendControlChange(cc, value, channel);
  midi_usb_2.sendControlChange(cc, value, channel);
  midi_usb_3.sendControlChange(cc, value, channel);
#ifdef DEBUG
  LOG.print(F(" MIDI-USB-HOST"));
#endif
#endif
#ifdef MIDI_DEVICE_USB
  usbMIDI.sendControlChange(cc, value, channel);
#ifdef DEBUG
  LOG.print(F(" MIDI-USB"));
#endif
#endif
#ifdef DEBUG
  LOG.println();
#endif
}

/*****************************************
   HELPER FUNCTIONS
 *****************************************/

#ifdef USB_KEYPAD
uint8_t USB_KEY = 0;

void OnPress(int key)
{
  USB_KEY = key;
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key)
{
#ifdef KEYBOARD_INTERFACE
  if (top == 0xc0000)
  {
    Keyboard.release(0XE400 | key);
  }
#endif
  // #ifdef SHOW_KEYBOARD_DATA
  //   LOG.print(F("HID ("));
  //   LOG.print(top, HEX);
  //   LOG.print(F(") key release:"));
  //   LOG.println(key, HEX);
  // #endif
}

void OnRawPress(uint8_t keycode)
{
#ifdef KEYBOARD_INTERFACE
  if (keyboard_leds != keyboard_last_leds)
  {
    // LOG.printf_P(PSTR("New LEDS: %x\n"), keyboard_leds);
    keyboard_last_leds = keyboard_leds;
    keyboard1.LEDS(keyboard_leds);
  }
  if (keycode >= 103 && keycode < 111)
  {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    uint8_t keybit = 1 << (keycode - 103);
    keyboard_modifiers |= keybit;
    Keyboard.set_modifier(keyboard_modifiers);
  }
  else
  {
    if (keyboard1.getModifiers() != keyboard_modifiers)
    {
#ifdef SHOW_KEYBOARD_DATA
      LOG.printf_P(PSTR("Mods mismatch: %x != %x\n"), keyboard_modifiers, keyboard1.getModifiers());
#endif
      keyboard_modifiers = keyboard1.getModifiers();
      Keyboard.set_modifier(keyboard_modifiers);
    }
    Keyboard.press(0XF000 | keycode);
  }
#endif
  // #ifdef SHOW_KEYBOARD_DATA
  //   LOG.print(F("OnRawPress keycode: "));
  //   LOG.print(keycode, HEX);
  //   LOG.print(F(" Modifiers: "));
  //   LOG.println(keyboard_modifiers, HEX);
  // #endif
}

void OnRawRelease(uint8_t keycode)
{

  USB_KEY = 0;

#ifdef KEYBOARD_INTERFACE
  if (keycode >= 103 && keycode < 111)
  {
    // one of the modifier keys was pressed, so lets turn it
    // on global..
    uint8_t keybit = 1 << (keycode - 103);
    keyboard_modifiers &= ~keybit;
    Keyboard.set_modifier(keyboard_modifiers);
  }
  else
  {
    Keyboard.release(0XF000 | keycode);
  }
#endif
  // #ifdef SHOW_KEYBOARD_DATA
  //   LOG.print(F("OnRawRelease keycode: "));
  //   LOG.print(keycode, HEX);
  //   LOG.print(F(" Modifiers: "));
  //   LOG.println(keyboard1.getModifiers(), HEX);
  // #endif
}
#endif

FLASHMEM void setup_midi_devices(void)
{
#ifdef MIDI_DEVICE_DIN
  // Start serial MIDI
  midi_serial.begin(DEFAULT_MIDI_CHANNEL);
  midi_serial.setHandleNoteOn(handleNoteOn_MIDI_DEVICE_DIN);
  midi_serial.setHandleNoteOff(handleNoteOff_MIDI_DEVICE_DIN);
  midi_serial.setHandleControlChange(handleControlChange_MIDI_DEVICE_DIN);
  midi_serial.setHandleAfterTouchChannel(handleAfterTouch_MIDI_DEVICE_DIN);
  midi_serial.setHandlePitchBend(handlePitchBend_MIDI_DEVICE_DIN);
  midi_serial.setHandleProgramChange(handleProgramChange_MIDI_DEVICE_DIN);
  midi_serial.setHandleSystemExclusive(handleSystemExclusive_MIDI_DEVICE_DIN);
  // midi_serial.setHandleSystemExclusiveChunk(handleSystemExclusiveChunk_MIDI_DEVICE_DIN);
  midi_serial.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame_MIDI_DEVICE_DIN);
  midi_serial.setHandleAfterTouchPoly(handleAfterTouchPoly_MIDI_DEVICE_DIN);
  midi_serial.setHandleSongSelect(handleSongSelect_MIDI_DEVICE_DIN);
  midi_serial.setHandleTuneRequest(handleTuneRequest_MIDI_DEVICE_DIN);
  midi_serial.setHandleClock(handleClock_MIDI_DEVICE_DIN);
  midi_serial.setHandleStart(handleStart_MIDI_DEVICE_DIN);
  midi_serial.setHandleContinue(handleContinue_MIDI_DEVICE_DIN);
  midi_serial.setHandleStop(handleStop_MIDI_DEVICE_DIN);
  midi_serial.setHandleActiveSensing(handleActiveSensing_MIDI_DEVICE_DIN);
  midi_serial.setHandleSystemReset(handleSystemReset_MIDI_DEVICE_DIN);
  // midi_serial.setHandleRealTimeSystem(handleRealTimeSystem_MIDI_DEVICE_DIN);
#ifdef DEBUG
  LOG.println(F("MIDI_DEVICE_DIN enabled"));
#endif
#endif

  // start up USB host
#ifdef MIDI_DEVICE_USB_HOST
  usb_host.begin();

#ifdef USB_KEYPAD
  keyboard1.attachPress(OnPress);
  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);
  // keyboard1.attachExtrasPress(OnHIDExtrasPress);
  // keyboard1.attachExtrasRelease(OnHIDExtrasRelease);
#endif


  midi_usb_1.setHandleNoteOn(handleNoteOn_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleNoteOff(handleNoteOff_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleControlChange(handleControlChange_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleAfterTouchChannel(handleAfterTouch_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandlePitchChange(handlePitchBend_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleProgramChange(handleProgramChange_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleSystemExclusive(handleSystemExclusive_MIDI_DEVICE_USB_HOST);
  // midi_usb.setHandleSystemExclusiveChunk(handleSystemExclusiveChunk_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleAfterTouchPoly(handleAfterTouchPoly_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleSongSelect(handleSongSelect_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleTuneRequest(handleTuneRequest_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleClock(handleClock_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleStart(handleStart_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleContinue(handleContinue_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleStop(handleStop_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleActiveSensing(handleActiveSensing_MIDI_DEVICE_USB_HOST);
  midi_usb_1.setHandleSystemReset(handleSystemReset_MIDI_DEVICE_USB_HOST);
  // midi_usb.setHandleRealTimeSystem(handleRealTimeSystem_MIDI_DEVICE_USB_HOST);

  midi_usb_2.setHandleNoteOn(handleNoteOn_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandleNoteOff(handleNoteOff_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandleControlChange(handleControlChange_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandleAfterTouchChannel(handleAfterTouch_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandlePitchChange(handlePitchBend_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandleProgramChange(handleProgramChange_MIDI_DEVICE_USB_HOST);
  midi_usb_2.setHandleSystemExclusive(handleSystemExclusive_MIDI_DEVICE_USB_HOST);

  midi_usb_3.setHandleNoteOn(handleNoteOn_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandleNoteOff(handleNoteOff_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandleControlChange(handleControlChange_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandleAfterTouchChannel(handleAfterTouch_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandlePitchChange(handlePitchBend_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandleProgramChange(handleProgramChange_MIDI_DEVICE_USB_HOST);
  midi_usb_3.setHandleSystemExclusive(handleSystemExclusive_MIDI_DEVICE_USB_HOST);

#ifdef DEBUG
  LOG.println(F("MIDI_DEVICE_USB_HOST enabled."));
#endif
#endif

  // check for onboard USB-MIDI
#ifdef MIDI_DEVICE_USB
  usbMIDI.begin();
  usbMIDI.setHandleNoteOn(handleNoteOn_MIDI_DEVICE_USB);
  usbMIDI.setHandleNoteOff(handleNoteOff_MIDI_DEVICE_USB);
  usbMIDI.setHandleControlChange(handleControlChange_MIDI_DEVICE_USB);
  usbMIDI.setHandleAfterTouchChannel(handleAfterTouch_MIDI_DEVICE_USB);
  usbMIDI.setHandlePitchChange(handlePitchBend_MIDI_DEVICE_USB);
  usbMIDI.setHandleProgramChange(handleProgramChange_MIDI_DEVICE_USB);
  usbMIDI.setHandleSystemExclusive(handleSystemExclusive_MIDI_DEVICE_USB);
  // usbMIDI.setHandleSystemExclusiveChunk(handleSystemExclusiveChunk_MIDI_DEVICE_USB);
  usbMIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame_MIDI_DEVICE_USB);
  usbMIDI.setHandleAfterTouchPoly(handleAfterTouchPoly_MIDI_DEVICE_USB);
  usbMIDI.setHandleSongSelect(handleSongSelect_MIDI_DEVICE_USB);
  usbMIDI.setHandleTuneRequest(handleTuneRequest_MIDI_DEVICE_USB);
  usbMIDI.setHandleClock(handleClock_MIDI_DEVICE_USB);
  usbMIDI.setHandleStart(handleStart_MIDI_DEVICE_USB);
  usbMIDI.setHandleContinue(handleContinue_MIDI_DEVICE_USB);
  usbMIDI.setHandleStop(handleStop_MIDI_DEVICE_USB);
  usbMIDI.setHandleActiveSensing(handleActiveSensing_MIDI_DEVICE_USB);
  usbMIDI.setHandleSystemReset(handleSystemReset_MIDI_DEVICE_USB);
  // usbMIDI.setHandleRealTimeSystem(handleRealTimeSystem_MIDI_DEVICE_USB);
#ifdef DEBUG
  LOG.println(F("MIDI_DEVICE_USB enabled."));
#endif
#endif
}

#ifdef MIDI_ACTIVITY_LIGHTS
extern void midi_activity_leds();
#endif

void check_midi_devices(void)
{
#ifdef MIDI_DEVICE_DIN
  midi_serial.read();
#endif
#ifdef MIDI_DEVICE_USB
  usbMIDI.read();
#endif

#ifdef MIDI_DEVICE_USB_HOST
  usb_host.Task();
  // midi_usb.read();
  checkAllUSBMIDI();  // Check all USB MIDI devices
#endif

#ifdef MIDI_ACTIVITY_LIGHTS
  midi_activity_leds();
#endif
}

FLASHMEM void send_sysex_voice(uint8_t midi_channel, uint8_t* data)
{
  uint8_t checksum = 0;
  uint8_t vd[161];

  // Send SYSEX data also via MIDI
  // vd[0] = 0xF0; // SysEx start
  vd[0] = 0x43;         // ID=Yamaha
  vd[1] = midi_channel - 1; // Sub-status and MIDI channel: n=0, ch=1
  vd[2] = 0x00;         // Format number (0=1 voice)
  vd[3] = 0x01;         // Byte count MSB
  vd[4] = 0x1B;         // Byte count LSB
  for (uint8_t n = 0; n < 155; n++)
  {
    checksum -= data[n];
    vd[5 + n] = data[n];
  }
  vd[160] = checksum & 0x7f; // Checksum
  // vd[162] = 0xF7; // SysEx end

#ifdef MIDI_DEVICE_DIN
  midi_serial.sendSysEx(161, vd); // Send to DIN MIDI
#endif
#ifdef MIDI_DEVICE_USB
  usbMIDI.sendSysEx(161, vd); // Send to USB MIDI
#endif
#ifdef MIDI_DEVICE_USB_HOST
  midi_usb_1.sendSysEx(161, vd); // Send to USB-HOST MIDI
#endif
}

FLASHMEM void send_sysex_bank(uint8_t midi_channel, uint8_t* bank_data)
{
  bank_data[2] = midi_channel - 1; //n=0, ch=1 

#ifdef MIDI_DEVICE_DIN
  midi_serial.sendSysEx(4104, bank_data); // Send to DIN MIDI
#endif
#ifdef MIDI_DEVICE_USB
  // Sysex bank dump is splitted due to Windows USB driver limitations
  usbMIDI.sendSysEx(2048, bank_data, true); // Send to USB MIDI
  delay(50);
  usbMIDI.sendSysEx(2048, bank_data + 2048, true);
  delay(50);
  usbMIDI.sendSysEx(8, bank_data + 4096, true);
#endif
#ifdef MIDI_DEVICE_USB_HOST
  midi_usb_1.sendSysEx(4104, bank_data); // Send to USB-HOST MIDI
#endif
}

FLASHMEM void send_sysex_param(uint8_t midi_channel, uint8_t var, uint8_t val, uint8_t param_group)
{
  uint8_t s[5];

  s[0] = 0x43;                   // ID=Yamaha
  s[1] = midi_channel;           // Sub-status and MIDI channel
  s[2] = (param_group & 5) << 2; // Format number (0=1 voice)
  if (param_group == 0)
  {
    s[2] |= 1;
    s[3] = var & 0x7f;
  }
  else
  {
    s[3] = var & 0x7f;
  }
  s[4] = val & 0x7f;

#ifdef MIDI_DEVICE_DIN
  midi_serial.sendSysEx(5, s); // Send to DIN MIDI
#endif
#ifdef MIDI_DEVICE_USB
  usbMIDI.sendSysEx(5, s); // Send to USB MIDI
#endif
#ifdef MIDI_DEVICE_USB_HOST
  midi_usb_1.sendSysEx(5, s); // Send to USB-HOST MIDI
#endif
}
