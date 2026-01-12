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

#ifndef MIDI_DEVICES_H
#define MIDI_DEVICES_H

#include "config.h"
#include "MIDI.h"
  
static const float midi_ticks_factor[10] = { 0.0, 0.25, 0.375, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };

void send_sysex_bank(uint8_t midi_channel, uint8_t* bank_data);
void send_sysex_param(uint8_t midi_channel, uint8_t var, uint8_t val, uint8_t param_group);

void MD_sendControlChange(uint8_t channel, uint8_t cc, uint8_t value);
void setup_midi_devices(void);
void check_midi_devices(void);

void midiSendRealtime(midi::MidiType event);
void midiNoteOn(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t device);
void midiNoteOff(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t device);

#endif // MIDI_DEVICES_H
