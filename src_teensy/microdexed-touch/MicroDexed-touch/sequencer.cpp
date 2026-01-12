/*
   MicroDexed

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6/4.x with audio shield.
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-android

   (c)2018-2021 M. Koslowski <positionhigh@gmx.de>
                H. Wirtz <wirtz@parasitstudio.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "config.h"
#include "sequencer.h"
#include <LCDMenuLib2.h>
#include "synth_dexed.h"
#include "ILI9341_t3n.h"

extern ILI9341_t3n display;
extern LCDMenuLib2 LCDML;
extern bool remote_active;
extern config_t configuration;
extern uint8_t drum_midi_channel;
extern uint8_t slices_midi_channel;
extern uint8_t activeSample;
extern uint8_t get_sample_note(uint8_t sample);
extern void handleNoteOn(byte, byte, byte, byte);
extern void handleNoteOff(byte, byte, byte, byte);
extern void set_sample_pitch(uint8_t, float); // float32_t not working
extern float get_sample_vol_max(uint8_t);
extern float get_sample_p_offset(uint8_t);
extern AudioSynthDexed* MicroDexed[NUM_DEXED];
extern microsynth_t microsynth[2];
extern braids_t braids_osc;
extern multisample_s msp[NUM_MULTISAMPLES];

#include "livesequencer.h"
extern LiveSequencer liveSeq;

sequencer_t seq;

FLASHMEM void seq_live_recording(void)
{
  // record to sequencer if sequencer menu is active and recording is active
  if (seq.note_in > 0 && seq.recording == true && LCDML.FUNC_getID() == LCDML.OTHER_getIDFromFunction(UI_func_seq_pattern_editor))
  {
    seq.note_data[seq.active_pattern][seq.step] = seq.note_in;
    if (seq.content_type[seq.active_pattern] == 0 && get_sample_note(activeSample) > 209) // pitched sample
    {
      seq.vel[seq.active_pattern][seq.step] = get_sample_note(activeSample);
    }
    else
      seq.vel[seq.active_pattern][seq.step] = seq.note_in_velocity;

    seq.note_in = 0;
    seq.note_in_velocity = 0;
  }
}

FLASHMEM uint8_t get_song_length()
{
  uint8_t best = 254;
  uint8_t stepcounter = 0;

  for (uint8_t t = 0; t < NUM_SEQ_TRACKS; t++) // loop tracks
  {
    stepcounter = 0;
    for (uint8_t s = SONG_LENGTH - 1; s > 0; s--) // search song length from back to front
    {
      if (seq.song[t][s] == 99)
        stepcounter++;
      else
        break;
    }
    if (stepcounter < best)
      best = stepcounter;
  }
  return SONG_LENGTH - best;
}

FLASHMEM uint8_t get_chain_length_from_current_track(uint8_t tr)
{
  uint8_t stepcounter = 0;

  for (uint8_t s = 0; s < 16; s++)
  {
    if (seq.current_chain[tr] == 99)
      break;
    if (seq.chain[seq.current_chain[tr]][s] == 99)
      break;
    else
      stepcounter++;
  }

  return stepcounter;
}

FLASHMEM uint8_t find_longest_chain()
{
  uint8_t longest = 0;
  uint8_t stepcounter = 0;

  for (uint8_t t = 0; t < NUM_SEQ_TRACKS; t++) // loop tracks
  {
    stepcounter = 0;
    for (uint8_t c = 0; c < 16; c++)
    {
      if (seq.current_chain[t] == 99)
        break;

      if (seq.chain[seq.current_chain[t]][c] == 99)
        break;
      else
        stepcounter++;
    }
    if (stepcounter > longest)
      longest = stepcounter;
  }
  return longest;
}

FLASHMEM bool check_probability(uint8_t patt)
{
  if (seq.pat_chance[patt] == 100)
    return true;
  else
  {
    uint8_t r = random(99);

    if (r <= seq.pat_chance[patt])
      return true;
    else
      return false;
  }
}

FLASHMEM int check_vel_variation(uint8_t patt, uint8_t in_velocity)
{
  if (seq.pat_vel_variation[patt] == 0)
    return in_velocity;
  else
  {
    int result = in_velocity;
    uint8_t rnd = random(seq.pat_vel_variation[patt]);
    uint8_t d = random(1); // + or - velocity

    if (d == 0)
      result = in_velocity + rnd * -1;
    else
      result = in_velocity + rnd;

    if (result >= 127)
      result = 127;
    else if (result < 0)
      result = 0;

    return result;
  }
}

FLASHMEM void handle_pattern_end_in_song_mode()
{
  //if (seq.step > 15)  // change to vari length
  if (seq.step > 15 - seq.pattern_len_dec)  // change to vari length
  {
    seq.step = 0;
    //seq.total_played_patterns++;//MIDI SLAVE SYNC TEST
    if (seq.play_mode == false) // play mode = full song
    {
      bool songstep_increased = false;

      for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
      {
        if (get_chain_length_from_current_track(d) > seq.chain_counter[d])
          seq.chain_counter[d]++;

        if (get_chain_length_from_current_track(d) > 0 && get_chain_length_from_current_track(d) == seq.chain_counter[d] && seq.chain_counter[d] < find_longest_chain())
          seq.chain_counter[d] = 0;

        if (seq.loop_end == 99) // no loop set
        {
          if (seq.current_song_step >= get_song_length())
          {
            seq.current_song_step = 0;
            seq.chain_counter[d] = 0;
          }
        }
        else
        {
          if (seq.loop_start == seq.loop_end && seq.current_song_step > seq.loop_start) // loop only a single step
          {
            seq.current_song_step = seq.loop_start;
            seq.chain_counter[d] = 0;
          }

          // if loop is on and changed during playback and new loop values are lower than current playing range, get back into the new loop range
          else if (seq.loop_start > seq.loop_end && seq.current_song_step == seq.loop_start + 1) // start is higher than end - > loop around
          {
            seq.current_song_step = 0;
            seq.chain_counter[d] = 0;
          }
          else if (seq.loop_start > seq.loop_end && seq.current_song_step == seq.loop_end + 1 && seq.loop_start != seq.loop_end + 1) // end is lower than start, jump to start at end
          {
            seq.current_song_step = seq.loop_start;
            seq.chain_counter[d] = 0;
          }
          else if (seq.loop_start < seq.loop_end && seq.current_song_step >= seq.loop_end + 1) // normal case, loop from end to start
          {
            seq.current_song_step = seq.loop_start;
            seq.chain_counter[d] = 0;
          }

          else if (seq.current_song_step >= get_song_length() && seq.current_song_step > seq.loop_start && seq.current_song_step > seq.loop_end)
          {
            seq.current_song_step = 0;
            seq.chain_counter[d] = 0;
          }
        }
        if (seq.chain_counter[d] == find_longest_chain() && songstep_increased == false)
        {
          seq.current_song_step++;
          for (uint8_t z = 0; z < NUM_SEQ_TRACKS; z++)
          {
            seq.chain_counter[z] = 0;
          }
          songstep_increased = true;
        }
        if (songstep_increased == true)
          seq.chain_counter[d] = 0;
      }
    }
  }
}

FLASHMEM uint8_t map_to_dexed_instance(uint8_t input)
{
  if (input < 2)
    return input;
  else
    return input - 68;
}


extern ChordInfo currentChord;
extern uint8_t transformNoteToChord(uint8_t inputNote);
extern uint8_t getChordRootNote(uint8_t inputNote);
extern uint8_t transformBassNote(uint8_t inputNote);
extern uint8_t  transformNoteToChordMelody(uint8_t inputNote);

FLASHMEM uint8_t harmonize(uint8_t inputNote, uint8_t track)
{
  if (!currentChord.active) return inputNote;

  uint8_t transformedNote = 0;

  if (seq.arr_type[track] == 0) // 0 = off(do not transform), 1 = transform, 2= bass, 3 = transform to root note only
    transformedNote = inputNote;
  else  if (seq.arr_type[track] == 1)
    transformedNote = currentChord.active ? transformNoteToChord(inputNote) : inputNote;
  else if (seq.arr_type[track] == 2)
    transformedNote = currentChord.active ? transformNoteToChordMelody(inputNote) : inputNote;

  else if (seq.arr_type[track] == 3)
    transformedNote = currentChord.active ? transformBassNote(inputNote) : inputNote;
  else if (seq.arr_type[track] == 4)
    transformedNote = currentChord.active ? getChordRootNote(inputNote) : inputNote;

  return transformedNote;

}

#ifdef GRANULAR
#include "granular.h"
extern granular_params_t granular_params;
#endif

FLASHMEM void arp_track(uint8_t d)
{
  if (seq.track_type[d] != 3) return;
  if (seq.arp_num_notes_count >= seq.arp_num_notes_max) return;
  if (seq.euclidean_active && !seq.euclidean_state[seq.step]) return;

  // Arp timing conditions 
  bool process_arp = false;
  if (seq.arp_speed == 0 && seq.ticks == 0) {
    process_arp = true;
  }
  else if (seq.arp_speed == 1 && seq.ticks == 0 && seq.arp_counter == 0) {
    process_arp = true;
  }
  else if (seq.arp_speed == 2) {
    if ((seq.instrument[d] == 3 || seq.instrument[d] == 4) &&
      (seq.ticks == 0 || seq.ticks == 2 || seq.ticks == 4 || seq.ticks == 6)) {
      process_arp = true;
    }
  }
  else if (seq.arp_speed == 3) {
    if ((seq.instrument[d] == 3 || seq.instrument[d] == 4) && seq.ticks != seq.ticks_max) {
      process_arp = true;
    }
  }

  if (!process_arp) {
    seq.arp_counter++;
    return;
  }

  // Process arpeggio based on style
  uint8_t transformedNote;
  uint8_t velocity;

  switch (seq.arp_style) {
  case 0: // arp up
    transformedNote = harmonize(seq.arp_note + seq.arps[seq.arp_chord][seq.arp_step + seq.element_shift], d);
    velocity = (seq.instrument[d] == 3 || seq.instrument[d] == 4) ?
      seq.arp_volume_base - (seq.arp_num_notes_count * (seq.arp_volume_base / seq.arp_num_notes_max)) :
      check_vel_variation(seq.current_pattern[d], seq.chord_vel);
    break;

  case 1: // arp down  
    transformedNote = harmonize(seq.arp_note + seq.arps[seq.arp_chord][seq.arp_length - seq.arp_step + seq.element_shift], d);
    velocity = (seq.instrument[d] == 3 || seq.instrument[d] == 4) ?
      seq.arp_volume_base - (seq.arp_num_notes_count * (seq.arp_volume_base / seq.arp_num_notes_max)) :
      check_vel_variation(seq.current_pattern[d], seq.chord_vel);
    break;

  case 2: // arp up & down
    if (seq.arp_step <= seq.arp_length) {
      transformedNote = harmonize(seq.arp_note + seq.arps[seq.arp_chord][seq.arp_step], d);
    }
    else {
      transformedNote = harmonize(seq.arp_note + seq.arps[seq.arp_chord][seq.arp_length * 2 - seq.arp_step], d);
    }
    velocity = (seq.instrument[d] == 3 || seq.instrument[d] == 4) ?
      check_vel_variation(seq.current_pattern[d], 90) :
      check_vel_variation(seq.current_pattern[d], seq.chord_vel);
    break;

  case 3: // arp random
    transformedNote = harmonize(seq.arp_note + seq.arps[seq.arp_chord][random(seq.arp_length) + seq.element_shift] + (seq.oct_shift * 12), d);
    velocity = check_vel_variation(seq.current_pattern[d],
      (seq.instrument[d] == 3 || seq.instrument[d] == 4) ? 90 : seq.chord_vel);
    break;

  default:
    return;
  }

  // Send note (preserving all original instrument routing)
  if (check_probability(seq.current_pattern[d])) {
    if (seq.instrument[d] == 3 || seq.instrument[d] == 4) {
      handleNoteOn(microsynth[seq.instrument[d] - 3].midi_channel, transformedNote, velocity, 0);
    }
    else if (seq.instrument[d] < 2 || seq.instrument[d] == 70 || seq.instrument[d] == 71) {
      handleNoteOn(configuration.dexed[map_to_dexed_instance(seq.instrument[d])].midi_channel, transformedNote, velocity, 0);
    }
    else if (seq.instrument[d] == 2) {
      handleNoteOn(configuration.epiano.midi_channel, transformedNote, velocity, 0);
    }
#ifdef GRANULAR
    else if (seq.instrument[d] == 72) {
      handleNoteOn(granular_params.midi_channel, transformedNote, velocity, 0);
    }
#endif
#ifdef MIDI_DEVICE_USB_HOST
    else if (seq.instrument[d] > 15 && seq.instrument[d] < 32) {
      handleNoteOn(seq.instrument[d] - 15, transformedNote, velocity, 1);
    }
#endif
#ifdef MIDI_DEVICE_DIN
    else if (seq.instrument[d] > 31 && seq.instrument[d] < 48) {
      handleNoteOn(seq.instrument[d] - 31, transformedNote, velocity, 2);
    }
#endif
    else if (seq.instrument[d] > 47 && seq.instrument[d] < 64) {
      handleNoteOn(seq.instrument[d] - 47, transformedNote, velocity, 3);
    }
    else if (seq.instrument[d] == 5) {
      handleNoteOn(braids_osc.midi_channel, transformedNote, velocity, 4);
    }
  }

  seq.arp_note_prev = transformedNote;
  seq.arp_num_notes_count++;

  // Microsynth-specific step increment (preserving original logic)
  if ((seq.instrument[d] == 3 || seq.instrument[d] == 4) && seq.arp_speed > 1 && seq.arp_style != 2) {
    seq.arp_step++;
    if (seq.arp_step >= seq.arp_length) seq.arp_step = 0;
  }

  // Global arp step management (preserving original logic exactly)
  seq.arp_counter++;

  if (seq.arp_speed == 0) {
    seq.arp_step++;
  }
  else if (seq.arp_speed == 1) {
    if (seq.arp_counter > 1) {
      seq.arp_counter = 0;
      seq.arp_step++;
    }
  }

  // Arp step reset conditions (preserving original logic exactly)
  if (seq.arp_style != 2) {
    if ((seq.arp_step > 1 && seq.arps[seq.arp_chord][seq.arp_step] == 0) || seq.arp_step == seq.arp_length) {
      seq.arp_step = 0;
    }
  }
  else {
    if ((seq.arp_step > 1 && seq.arps[seq.arp_chord][seq.arp_step] == 0) || seq.arp_step == seq.arp_length * 2) {
      seq.arp_step = 0;
    }
  }

  // Minimum arp length for certain styles
  if ((seq.arp_style == 1 || seq.arp_style == 2) && seq.arp_length == 0) {
    seq.arp_length = 9;
  }
}

uint8_t current_played_pitched_sample = 0;


FLASHMEM void process_drum_track(uint8_t d, int tr)
{
#if NUM_DRUMS > 0
  if (seq.vel[seq.current_pattern[d]][seq.step] > 209) {
    current_played_pitched_sample = seq.vel[seq.current_pattern[d]][seq.step] - 210;
    if (seq.arr_type[d] == 0) {
      set_sample_pitch(current_played_pitched_sample, (float)pow(2, (seq.note_data[seq.current_pattern[d]][seq.step] - 72 + tr) / 12.00) * get_sample_p_offset(current_played_pitched_sample));
    }
    else {
      uint8_t transformed_note = harmonize(seq.note_data[seq.current_pattern[d]][seq.step] - 72, d);
      set_sample_pitch(current_played_pitched_sample, (float)pow(2, transformed_note / 12.00) * get_sample_p_offset(current_played_pitched_sample));
    }
    if (check_probability(seq.current_pattern[d])) {
      handleNoteOn(drum_midi_channel, seq.vel[seq.current_pattern[d]][seq.step], check_vel_variation(seq.current_pattern[d], 90), 0);
    }
  }
  else if (check_probability(seq.current_pattern[d])) {
    handleNoteOn(drum_midi_channel, seq.note_data[seq.current_pattern[d]][seq.step], check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
  }
#endif
}

FLASHMEM void process_instrument_track(uint8_t d, int tr)
{
  if (seq.note_data[seq.current_pattern[d]][seq.step] == 130) return;

  uint8_t transformedNote = harmonize(seq.note_data[seq.current_pattern[d]][seq.step] + tr, d);
  bool isNoise = (seq.note_data[seq.current_pattern[d]][seq.step] == MIDI_C8);

  if (check_probability(seq.current_pattern[d])) {
    if (seq.instrument[d] == 5) {
      handleNoteOn(braids_osc.midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 4);
    }
    else if (seq.instrument[d] > 5 && seq.instrument[d] < 16) {
      handleNoteOn(msp[seq.instrument[d] - 6].midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
    }
    else if (seq.instrument[d] > 15 && seq.instrument[d] < 32) {
#ifdef MIDI_DEVICE_USB_HOST
      handleNoteOn(seq.instrument[d] - 15, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 1);
#endif
    }
    else if (seq.instrument[d] > 31 && seq.instrument[d] < 48) {
#ifdef MIDI_DEVICE_DIN
      handleNoteOn(seq.instrument[d] - 31, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 2);
#endif
    }
    else if (seq.instrument[d] == 64) {
      handleNoteOn(21, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
    }
    else if (seq.instrument[d] > 47 && seq.instrument[d] < 64) {
      handleNoteOn(seq.instrument[d] - 47, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 3);
    }
    else if (seq.instrument[d] < 2 || seq.instrument[d] == 70 || seq.instrument[d] == 71) {
      handleNoteOn(configuration.dexed[map_to_dexed_instance(seq.instrument[d])].midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
    }
    else if (seq.instrument[d] == 2) {
      handleNoteOn(configuration.epiano.midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
#ifdef GRANULAR
    }
    else if (seq.instrument[d] == 72) {
      handleNoteOn(granular_params.midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
#endif
    }
    else if ((seq.instrument[d] == 3 || seq.instrument[d] == 4) && seq.track_type[d] != 3) {
      handleNoteOn(microsynth[seq.instrument[d] - 3].midi_channel, isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote,
        check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
    }
  }

  seq.prev_note[d] = isNoise ? seq.note_data[seq.current_pattern[d]][seq.step] : transformedNote;
  seq.prev_vel[d] = seq.vel[seq.current_pattern[d]][seq.step];
}

FLASHMEM void process_chord_track(uint8_t d, int tr)
{
  if (seq.vel[seq.current_pattern[d]][seq.step] <= 199) return;

  for (uint8_t x = seq.element_shift; x < seq.element_shift + seq.chord_key_ammount; x++) {
    uint8_t note = harmonize(seq.note_data[seq.current_pattern[d]][seq.step] + tr + (seq.oct_shift * 12) + seq.arps[seq.vel[seq.current_pattern[d]][seq.step] - 200][x], d);

    if (check_probability(seq.current_pattern[d])) {
      if (seq.instrument[d] < 2 || seq.instrument[d] == 70 || seq.instrument[d] == 71) {
        handleNoteOn(configuration.dexed[map_to_dexed_instance(seq.instrument[d])].midi_channel, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 0);
#ifdef GRANULAR
      }
      else if (seq.instrument[d] == 72) {
        handleNoteOn(granular_params.midi_channel, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 0);
#endif
      }
      else if (seq.instrument[d] == 2) {
        handleNoteOn(configuration.epiano.midi_channel, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 0);
      }
      else if (seq.instrument[d] == 5) {
        handleNoteOn(braids_osc.midi_channel, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 4);
#ifdef MIDI_DEVICE_USB_HOST
      }
      else if (seq.instrument[d] > 15 && seq.instrument[d] < 32) {
        handleNoteOn(seq.instrument[d] - 15, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 1);
#endif
#ifdef MIDI_DEVICE_DIN
      }
      else if (seq.instrument[d] > 31 && seq.instrument[d] < 48) {
        handleNoteOn(seq.instrument[d] - 31, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 2);
#endif
      }
      else if (seq.instrument[d] > 47 && seq.instrument[d] < 64) {
        handleNoteOn(seq.instrument[d] - 47, note, check_vel_variation(seq.current_pattern[d], seq.chord_vel), 3);
      }
    }
  }

  seq.prev_note[d] = seq.note_data[seq.current_pattern[d]][seq.step] + tr + (seq.oct_shift * 12);
  seq.prev_vel[d] = seq.vel[seq.current_pattern[d]][seq.step];
}

FLASHMEM void process_arp_track(uint8_t d, int tr)
{
  seq.arp_step = 0;
  seq.arp_num_notes_count = 0;
  seq.arp_counter = 0;
  if (seq.instrument[d] == 3 || seq.instrument[d] == 4) {
    seq.arp_volume_base = microsynth[seq.instrument[d] - 3].sound_intensity;
  }
  seq.arp_note = harmonize(seq.note_data[seq.current_pattern[d]][seq.step] + tr + (seq.oct_shift * 12), d);
  seq.arp_chord = seq.vel[seq.current_pattern[d]][seq.step] - 200;
}

FLASHMEM void process_sliced_drums(uint8_t d)
{
#if NUM_DRUMS > 0
  if (check_probability(seq.current_pattern[d])) {
    handleNoteOn(slices_midi_channel, seq.note_data[seq.current_pattern[d]][seq.step],
      check_vel_variation(seq.current_pattern[d], seq.vel[seq.current_pattern[d]][seq.step]), 0);
  }
#endif
}


FLASHMEM void sequencer_part1(void)
{
  for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
  {
    int tr = 0;

    // Pattern selection logic (identical to original)
    if (!seq.play_mode) {
      seq.current_chain[d] = seq.song[d][seq.current_song_step];
      seq.current_pattern[d] = seq.chain[seq.current_chain[d]][seq.chain_counter[d]];
    }
    else if (seq.hunt_pattern) {
      seq.current_song_step = find_first_song_step_with_pattern(seq.active_pattern);
      if (get_chain_length_from_current_track(d) >= find_first_chain_step_with_pattern(seq.active_pattern)) {
        seq.chain_counter[d] = find_first_chain_step_with_pattern(seq.active_pattern);
      }
      else if (get_chain_length_from_current_track(d) == 1) {
        seq.chain_counter[d] = 0;
      }
      seq.current_chain[d] = seq.song[d][seq.current_song_step];
      seq.current_pattern[d] = seq.chain[seq.current_chain[d]][seq.chain_counter[d]];
    }
    else {
      seq.current_chain[d] = seq.song[d][seq.current_song_step];
      seq.current_pattern[d] = seq.chain[seq.current_chain[d]][seq.chain_counter[d]];
    }

    // Transpose calculation (identical to original)
    if (seq.chain_transpose[seq.current_chain[d]][seq.chain_counter[d]] < NUM_CHAINS) {
      tr = seq.chain_transpose[seq.current_chain[d]][seq.chain_counter[d]];
    }
    else if (seq.chain_transpose[seq.current_chain[d]][seq.chain_counter[d]] < NUM_CHAINS * 2) {
      tr = NUM_CHAINS - seq.chain_transpose[seq.current_chain[d]][seq.chain_counter[d]];
    }

    // Main processing (identical conditions to original)
    if (seq.current_pattern[d] < NUM_SEQ_PATTERN && seq.current_chain[d] != 99 && !seq.track_mute[d]) {
      if (seq.note_data[seq.current_pattern[d]][seq.step] > 0) {
        if (seq.track_type[d] == 0) {
          process_drum_track(d, tr);
        }
        else {
          if (seq.track_type[d] == 1) {
            process_instrument_track(d, tr);
          }
          else if (seq.track_type[d] == 2 && seq.ticks == 0 && seq.note_data[seq.current_pattern[d]][seq.step] != 130) {
            process_chord_track(d, tr);
          }
          else if (seq.track_type[d] == 3 && seq.ticks == 0) {
            process_arp_track(d, tr);
          }
          else if (seq.track_type[d] == 4) {
            process_sliced_drums(d);
          }
        }
      }
    }
    seq.noteoffsent[d] = false;
  }
}

// Helper to route NoteOff correctly for any instrument
FLASHMEM static inline void sendNoteOff(uint8_t instrument, uint8_t note, uint8_t ticks, uint8_t ticks_max, bool force)
{
  // Dexed group (0,1,70,71)
  if (instrument < 2 || instrument == 70 || instrument == 71)
  {
    if (force || ticks == ticks_max)
      handleNoteOff(configuration.dexed[map_to_dexed_instance(instrument)].midi_channel, note, 0, 0);
  }
  else if (instrument == 2) // Epiano
  {
    if (force || ticks == ticks_max)
      handleNoteOff(configuration.epiano.midi_channel, note, 0, 0);
  }
#ifdef GRANULAR
  else if (instrument == 72) // Granular synth
  {
    if (force || ticks == ticks_max)
      handleNoteOff(granular_params.midi_channel, note, 0, 0);
  }
#endif
  else if (instrument > 2 && instrument < 5) // Microsynth
  {
    // Microsynth has no tick condition in original
    handleNoteOff(microsynth[instrument - 3].midi_channel, note, 0, 0);
  }
  else if (instrument == 5) // Braids osc
  {
    if (force || ticks == ticks_max)
      handleNoteOff(braids_osc.midi_channel, note, 0, 4);
  }
  else if (instrument > 5 && instrument < 16) // MultiSampler
  {
    // no tick condition in original
    handleNoteOff(msp[instrument - 6].midi_channel, note, 0, 0);
  }
#ifdef MIDI_DEVICE_USB_HOST
  else if (instrument > 15 && instrument < 32) // USB MIDI
  {
    // no tick condition in original
    handleNoteOff(instrument - 15, note, 0, 1);
  }
#endif
#ifdef MIDI_DEVICE_DIN
  else if (instrument > 31 && instrument < 48) // DIN MIDI
  {
    if (force || ticks == ticks_max)
      handleNoteOff(instrument - 31, note, 0, 2);
  }
#endif
  else if (instrument > 47 && instrument < 64) // Internal Micro MIDI
  {
    if (force || ticks == ticks_max)
      handleNoteOff(instrument - 47, note, 0, 3);
  }
  else if (instrument == 64) // CV out
  {
    if (force || ticks == ticks_max)
      handleNoteOff(21, note, 0, 0);
  }
}

void sequencer_part2(void)
{
  for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
  {
    if (seq.noteoffsent[d]) continue;
    if (seq.prev_note[d] == 0 || seq.track_type[d] == 0) continue;

    uint8_t curNote = seq.prev_note[d];
    uint8_t instr = seq.instrument[d];

    // Normal note case
    if (seq.note_data[seq.current_pattern[d]][seq.step] != 130)
    {
      sendNoteOff(instr, curNote, seq.ticks, seq.ticks_max, false);
      seq.noteoffsent[d] = true;
    }

    // Chords
    if (seq.track_type[d] == 2 && seq.note_data[seq.current_pattern[d]][seq.step] != 130)
    {
      if (seq.prev_vel[d] > 199)
      {
        for (uint8_t x = seq.element_shift; x < seq.element_shift + seq.chord_key_ammount; x++)
        {
          uint8_t chordNote = harmonize(curNote + seq.arps[seq.prev_vel[d] - 200][x], d);
          sendNoteOff(instr, chordNote, seq.ticks, seq.ticks_max, true); // force noteoff (no tick check)
          seq.noteoffsent[d] = true;
        }
      }
    }

    // Arpeggiator
    if (seq.track_type[d] == 3)
    {
      sendNoteOff(instr, seq.arp_note_prev, seq.ticks, seq.ticks_max, true); // force noteoff (no tick check)
      seq.noteoffsent[d] = true;
    }
  }
}


extern void send_midi_clock();
// #ifdef MUSIKANDMORE
// extern void setled(uint8_t pin, bool state);
// #endif
extern void updateCurrentChord(const uint8_t midiNotes[], uint8_t size);
extern uint8_t chord_input[];

void sequencer(void)
{
  if (seq.running) {
    // if (seq.ticks < 4)
    //   seq_live_recording();
    if (seq.ticks == 0 && seq.step == 0) {
      liveSeq.handlePatternBegin();
    }

    if (seq.clock == 2) // MDT is MIDI MASTER
    {
      seq.ticks_max = 5; //(0-5 = 6)
      send_midi_clock();
    }

    if (seq.ticks == 0)
    {

      //        if (seq.step == 0 ||  seq.step == 8)
      // updateCurrentChord(chord_input, 3);

      sequencer_part1();
      seq.step++;

#ifdef MUSIKANDMORE
      digitalWrite(CV_CLOCK_OUT, LOW); //tested ok 24/06/2025
      //setled(LED_DOWN, true);
#endif
    }

    if (seq.ticks == 0 || (seq.ticks == 0 && seq.step % 2 == 0) ||

      (seq.arp_speed == 2 && seq.ticks != seq.ticks_max) || (seq.arp_speed == 3 && seq.ticks != seq.ticks_max))
    {
      for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
      {
        if (seq.track_type[d] == 3)

          arp_track(d);

      }
      //   seq.arp_counter++;
    }

    if (seq.ticks == seq.ticks_max)
    {
      sequencer_part2();

#ifdef MUSIKANDMORE
      digitalWrite(CV_CLOCK_OUT, HIGH);  //tested ok 24/06/2025
      //setled(LED_DOWN, false);
#endif
    }

    seq.ticks++;
    if (seq.ticks > seq.ticks_max)
    {
      seq.ticks = 0;
      handle_pattern_end_in_song_mode();
    }
  }
}

FLASHMEM void set_pattern_content_type_color(uint8_t pattern)
{
  if (seq.content_type[pattern] == 0) // Drumpattern
    display.setTextColor(COLOR_DRUMS, COLOR_BACKGROUND);
  else if (seq.content_type[pattern] == 1) // Instrument Pattern
    display.setTextColor(COLOR_INSTR, COLOR_BACKGROUND);
  else if (seq.content_type[pattern] == 2) //  chord / arp pattern
    display.setTextColor(COLOR_CHORDS, COLOR_BACKGROUND);
  else if (seq.content_type[pattern] == 3) //   arp pattern / slices
    display.setTextColor(COLOR_ARP, COLOR_BACKGROUND);
  else
    display.setTextColor(GREY2, COLOR_BACKGROUND);
}

FLASHMEM int get_pattern_content_type_color(uint8_t pattern)
{
  int col = 0;

  if (seq.content_type[pattern] == 0) // Drumpattern
    col = COLOR_DRUMS;
  else if (seq.content_type[pattern] == 1) // Instrument Pattern
    col = COLOR_INSTR;
  else
    col = COLOR_CHORDS;
  return col;
}

#if defined GLOW
extern void set_glow_print_number(uint16_t x, uint8_t y, int glow_input_value, uint8_t length, uint8_t menuitem, uint8_t textsize);
extern void set_glow_print_number(uint16_t x, uint8_t y, int glow_input_value, uint8_t length, uint8_t track, uint8_t menuitem, uint8_t textsize, bool state);
extern uint8_t generic_temp_select_menu;
#endif

FLASHMEM void print_formatted_number(uint16_t number, uint8_t length)
{

  switch (length) {
  case 4:
    display.printf("%04d", number);
    break;

  case 3:
    display.printf("%03d", number);
    break;

  default:
    display.printf("%02d", number);
    break;
  }
}

FLASHMEM void print_formatted_number(uint16_t number, uint8_t length, uint8_t menuitem, uint8_t textsize)
{
#if defined GLOW
  if (seq.edit_state == 0)
    set_glow_print_number(display.getCursorX(), display.getCursorY(), number, length, menuitem, textsize);
#endif
  print_formatted_number(number, length);
}

FLASHMEM void print_formatted_number(uint16_t number, uint8_t length, uint8_t track, uint8_t menuitem, uint8_t textsize, bool state)
{
  //test song page
#if defined GLOW
  if (seq.edit_state == 0)
    set_glow_print_number(display.getCursorX(), display.getCursorY(), number, length, track, menuitem, textsize, state);
#endif
  //not printing anything here directly
}

FLASHMEM void print_formatted_number_trailing_space(uint16_t number, uint8_t length) {
  if (length == 4) {
    display.printf("%-4d", number);
  }
  else {// if not 4 then length defaults to 3    
    display.printf("%-3d", number);
  }
}

FLASHMEM void print_formatted_number_signed(int number, uint8_t length)
{
  if (number > -1) {
    display.printf("+%02d", number);
  }
  else {
    display.printf("%03d", number); // minus sign comes automatically
  }
}
#if defined GLOW
extern void set_glow_print_formatted_number_signed(uint16_t x, uint8_t y, int input_value, uint8_t length, uint8_t menuitem, uint8_t textsize);
#endif
FLASHMEM void print_formatted_number_signed(int number, uint8_t length, uint8_t menuitem, uint8_t textsize)
{
#if defined GLOW
  if (seq.edit_state == 0)
    set_glow_print_formatted_number_signed(display.getCursorX(), display.getCursorY(), number, length, menuitem, textsize);
#endif
  if (number > -1) {
    display.printf("+%02d", number);
  }
  else {
    display.printf("%03d", number); // minus sign comes automatically
  }
}


FLASHMEM void print_chord_name(uint8_t currentstep)
{
  for (uint8_t i = 0; i < 7; i++)
  {
    if (seq.vel[seq.active_pattern][currentstep] > 199)
      display.print(seq.chord_names[seq.vel[seq.active_pattern][currentstep] - 200][i]);
    else
      display.print(seq.chord_names[6][i]);
  }
}

FLASHMEM void update_keyboard_current_step(int ypos, uint8_t octave, uint8_t current_step)
{
  // draw grid
  for (uint8_t y = 0; y < 34; y++)
  {
    display.fillRect(34 + current_step * 7, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, COLOR_SYSTEXT); // active step
    if (current_step > 0)
    {
      if (seq.piano[y] == 0)                                                                           // is a white key
        display.fillRect(34 - 7 + current_step * 7, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY3); // GRID white key
      else
        display.fillRect(34 - 7 + current_step * 7, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY4); // GRID black key
    }
    else if (current_step == 0)
    {
      if (seq.piano[y] == 0)                                                             // is a white key
        display.fillRect(34 + 63 * 7, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY3); // GRID white key
      else
        display.fillRect(34 + 63 * 7, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY4); // GRID black key
    }
  }
}

FLASHMEM void print_keyboard(int ypos, uint8_t octave)
{
  uint8_t offset[5] = { 12, 12, 14, 12, 11 }; //+ is up
  int offcount = 0;
  uint8_t oct_count = 0;
  uint8_t patternspacer = 0;
  uint8_t barspacer = 0;
  display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  display.setTextSize(1);

  // draw white keys
  for (uint8_t y = 0; y < 14; y++)
  {
    display.fillRect(0, ypos - CHAR_height - (y * 14), 30, 13, COLOR_SYSTEXT); // pianoroll white key
    if (y == 0 || y == 7 || y == 14)
    {
      display.setCursor(17, ypos - 14 - (y * 14));
      display.print("C");
      display.print(octave - 1 + oct_count);
      oct_count++;
    }
  }
  for (uint8_t y = 0; y < 23; y++)
  {
    if (seq.piano[y] == 1)
    {
      display.fillRect(0, ypos - (y * 8.15) - offset[offcount], 12, 8, COLOR_BACKGROUND); // BLACK key
      offcount++;
      if (offcount == 5)
        offcount = 0;
    }
  }
  // draw grid

  for (uint8_t y = 0; y < 24; y++)
  {
    patternspacer = 0;
    barspacer = 0;
    for (uint8_t x = 0; x < 64; x++)
    {
      if (seq.piano[y] == 0)                                                                                        // is a white key
        display.fillRect(40 + patternspacer + barspacer + x * 4, ypos + 6 - CHAR_height - (y * 8.15), 3, 6, GREY3); // GRID white key
      else
        display.fillRect(40 + patternspacer + barspacer + x * 4, ypos + 6 - CHAR_height - (y * 8.15), 3, 6, GREY4); // GRID black key
      if ((x + 1) % 16 == 0)
        patternspacer = patternspacer + 2;
      if ((x + 1) % 4 == 0)
        barspacer = barspacer + 1;
    }
  }
}

FLASHMEM void seq_print_current_note_from_step(uint8_t step)
{
  if (seq.note_data[seq.active_pattern][step] == 130) // it is a latched note
  {
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.write(0x7E);
    display.print(" ");
    // display.print("LATCH "); //Tilde Symbol for latched note
  }
  else
  {
    if (seq.vel[seq.active_pattern][step] > 209) // pitched sample
    {
      // display.print("P");
      display.print(noteNames[seq.note_data[seq.active_pattern][step] % 12][0]);
    }
    else
    {
      display.print(noteNames[seq.note_data[seq.active_pattern][step] % 12][0]);
      if (noteNames[seq.note_data[seq.active_pattern][step] % 12][1] != '\0')
      {
        display.print(noteNames[seq.note_data[seq.active_pattern][step] % 12][1]);
      }
      display.print((seq.note_data[seq.active_pattern][step] / 12) - 1); // print octave
      display.print(" ");
    }
  }
}

FLASHMEM void print_keyboard_small(int xpos, int ypos, uint8_t octave, uint8_t actstep, bool fullredraw)
{
  uint8_t offset[5] = { 12, 12, 14, 12, 11 }; //+ is up
  int offcount = 0;
  uint8_t oct_count = 0;
  uint8_t to_step = 16;
  if (fullredraw || seq.pianoroll_octave != octave)
  {
    seq.pianoroll_octave = octave;
    display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
    // draw white keys
    for (uint8_t y = 0; y < 11; y++)
    {
      if (remote_active)
        display.console = true;
      display.fillRect(xpos, ypos - CHAR_height - (y * 14), 30, 13, COLOR_SYSTEXT); // pianoroll white key
      if (y == 0 || y == 7 || y == 14)
      {
        display.setCursor(xpos + 17, ypos - 14 - (y * 14));
        display.print("C");
        display.print(octave - 1 + oct_count);
        oct_count++;
      }
    }
    for (uint8_t y = 0; y < 20; y++) // draw BLACK keys
    {
      if (seq.piano[y] == 1)
      {
        if (remote_active)
          display.console = true;

        display.fillRect(xpos, ypos - (y * 8.15) - offset[offcount], 12, 8, COLOR_BACKGROUND); // BLACK key
        offcount++;
        if (offcount == 5)
          offcount = 0;
      }
    }
  }
  else
  {
    if (actstep < 15)
      to_step = actstep + 1;
  }
  // draw grid
  for (uint8_t y = 0; y < 19; y++)
  {
    for (uint8_t x = 0; x < to_step; x++)
    {
      if (remote_active)
        display.console = true;

      if (x > 15 - seq.pattern_len_dec)
        display.fillRect(xpos + 36 + x * 10, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY4); // disabled
      else if (seq.piano[y] == 0)                                                                    // is a white key
        display.fillRect(xpos + 36 + x * 10, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY3); // GRID white key
      else
        display.fillRect(xpos + 36 + x * 10, ypos + 6 - CHAR_height - (y * 8.15), 5, 6, GREY4); // GRID black key
    }
  }
}

FLASHMEM void print_single_pattern_pianoroll_in_pattern_editor(int xpos, int ypos, uint8_t pattern, uint8_t actstep, bool fullredraw)
{
  uint8_t lowest_note = 128;
  int notes_display_shift = 0;
  uint8_t last_valid_note = 254;
  uint8_t from_step = 0;
  uint8_t to_step = 16;
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  // find lowest note
  for (uint8_t f = 0; f < 16; f++)
  {
    if (seq.note_data[pattern][f] < lowest_note && seq.note_data[pattern][f] > 0)
    {
      lowest_note = seq.note_data[pattern][f];
    }
  }
  notes_display_shift = lowest_note % 12;
  if (lowest_note > 120)
    lowest_note = 24;
  print_keyboard_small(xpos, ypos, lowest_note / 12, actstep, fullredraw);
  display.setTextColor(COLOR_SYSTEXT);

  for (from_step = 0; from_step < to_step; from_step++)
  {
    if (remote_active)
      display.console = true;
    if ((ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (seq.note_data[pattern][from_step] - lowest_note))) > 4 * CHAR_height + 8)
    {
      if (seq.note_data[pattern][from_step] != 0 && seq.note_data[pattern][from_step] != 130)
      {
        if (from_step == actstep)
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (seq.note_data[pattern][from_step] - lowest_note)), 5, 5, COLOR_SYSTEXT);
        else
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (seq.note_data[pattern][from_step] - lowest_note)), 5, 5, COLOR_INSTR);
        last_valid_note = seq.note_data[pattern][from_step];
      }
      else if (seq.note_data[pattern][from_step] == 130) // last valid note was latch
      {
        if (from_step == actstep)
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, COLOR_SYSTEXT);
        else
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, GREEN);
      }
      else if (from_step == actstep)
      {
        // if (last_valid_note != 0)
        if (from_step > 15 - seq.pattern_len_dec)
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, GREY2);
        else
          display.fillRect(xpos + 36 + from_step * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, COLOR_SYSTEXT);
      }
      if (from_step < 15)
      {
        if (seq.note_data[pattern][from_step + 1] == 0)
        {
          if (from_step >= 15 - seq.pattern_len_dec)
            display.fillRect(xpos + 36 + (from_step + 1) * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, GREY4);
          else
            if (seq.piano[last_valid_note % 12 - lowest_note] == 0)                                                                                                 // is a white key
              display.fillRect(xpos + 36 + (from_step + 1) * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, GREY3); // GRID white key
            else
              display.fillRect(xpos + 36 + (from_step + 1) * 10, ypos - 10 - (8.15 * notes_display_shift) - (8.15 * (last_valid_note - lowest_note)), 5, 5, GREY4); // GRID black key
        }
      }
    }
  }
  display.setTextSize(2);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
}
