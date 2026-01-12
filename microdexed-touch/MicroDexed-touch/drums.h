/*
   MicroDexed

   MicroDexed is a port of the Dexed sound engine
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-an
  droid

   (c)2018-2021 H. Wirtz <wirtz@parasitstudio.de>
   (c)2021      H. Wirtz <wirtz@parasitstudio.de>, M. Koslowski <positionhigh@gmx.de>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   THE SOFTWARE.
*/

#include <Arduino.h>
#include "arm_math.h" // float32_t

#ifndef _DRUMS_H
#define _DRUMS_H

typedef struct drum_config_s
{
  uint8_t drum_class;    // Type of drum
  uint8_t midinote;      // Triggered by note
  char name[FILENAME_MAX_LEN];
  char filename[FILENAME_MAX_LEN];
  const uint8_t* drum_data;
  char shortname[2];     // 1 char name for sequencer
  uint32_t len;          // number of elements in drum_data
  uint8_t numChannels;   // 1 for mono, 2 for stereo
  uint8_t pitch;         // variable pitch per note for sequencer
  uint8_t p_offset;      // "static" pitch offset to correct root note to root of other samples
  int8_t pan;            // Panorama (-99 - 99)
  uint8_t vol_max;       // max. Volume (0 - 100)
  uint8_t reverb_send;   // how much signal to send to the reverb (0 - 100)
  uint8_t filter_mode;   // multimode filter mode
  uint8_t filter_freq;   // multimode filter frequency
  uint8_t filter_q;      // multimode filter Q
  uint8_t delay1;        // delay1 send 
  uint8_t delay2;        // selay2 send
  uint8_t env_attack;
  uint8_t env_hold;
  uint8_t env_decay;
  uint8_t env_sustain;
  uint8_t env_release;
} drum_config_t;

struct CustomSample {
  char filepath[FULLPATH_MAX_LEN];
  int16_t start; // (length / 1000)
  int16_t end;   // (length / 1000)
  uint8_t loopType;
};

enum
{
  DRUM_NONE,
  DRUM_BASS,
  DRUM_SNARE,
  DRUM_HIHAT,
  DRUM_HANDCLAP,
  DRUM_RIDE,
  DRUM_CRASH,
  DRUM_LOWTOM,
  DRUM_MIDTOM,
  DRUM_HIGHTOM,
  DRUM_PERCUSSION,
  DRUM_POLY
};

#endif
