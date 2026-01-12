/*
   MicroDexed

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

#include <Arduino.h>
#include "config.h"
#include <Wire.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <TeensyTimerTool.h>
using namespace TeensyTimerTool;
#include "dexed_sd.h"
#include "synth_dexed.h"
#include "microsynth.h"
#include "braids.h"
#include "touch.h"
#include "noisemaker.h"

#if NUM_DRUMS > 0
#include "drums.h"
extern void set_drums_volume(float vol);
extern drum_config_t drum_config[];
extern CustomSample customSamples[NUM_CUSTOM_SAMPLES];
extern custom_midi_map_t custom_midi_map[NUM_CUSTOM_MIDI_MAPPINGS];
#endif

#ifdef COMPILE_FOR_PSRAM
extern void load_custom_samples_to_psram();
extern void unload_all_custom_samples();
#endif

extern sdcard_t sdcard_infos;
extern fm_t fm;

extern char g_voice_name[NUM_DEXED][VOICE_NAME_LEN];
extern char g_bank_name[NUM_DEXED][BANK_NAME_LEN];
extern void init_MIDI_send_CC(void);
extern void check_configuration_dexed(uint8_t instance_id);
extern void check_configuration_performance(void);
extern void check_configuration_fx(void);
extern void check_configuration_epiano(void);
extern void update_euclidean(void);

extern microsynth_t microsynth[NUM_MICROSYNTH];

extern braids_t braids_osc;

extern uint8_t GAMEPAD_UP_0;
extern uint8_t GAMEPAD_UP_1;
extern uint8_t GAMEPAD_UP_BUTTONS;

extern uint8_t GAMEPAD_DOWN_0;
extern uint8_t GAMEPAD_DOWN_1;
extern uint8_t GAMEPAD_DOWN_BUTTONS;

extern uint8_t GAMEPAD_RIGHT_0;
extern uint8_t GAMEPAD_RIGHT_1;
extern uint8_t GAMEPAD_RIGHT_BUTTONS;

extern uint8_t GAMEPAD_LEFT_0;
extern uint8_t GAMEPAD_LEFT_1;
extern uint8_t GAMEPAD_LEFT_BUTTONS;

extern uint32_t GAMEPAD_SELECT;
extern uint32_t GAMEPAD_START;
extern uint32_t GAMEPAD_BUTTON_A;
extern uint32_t GAMEPAD_BUTTON_B;

#include "sequencer.h"
extern PeriodicTimer sequencer_timer;
extern void sequencer();
extern sequencer_t seq;

extern uint8_t drum_midi_channel;
extern uint8_t slices_midi_channel;

extern float midi_volume_transform(uint8_t midi_amp);
extern void set_sample_pitch(uint8_t sample, float playbackspeed);
extern void set_sample_p_offset(uint8_t sample, float s_offset);
extern void set_sample_pan(uint8_t sample, float s_pan);
extern void set_sample_vol_max(uint8_t sample, float s_max);
extern void set_sample_reverb_send(uint8_t sample, float s_reverb);

extern void set_sample_filter_mode(uint8_t sample, uint8_t s_filter_mode);
extern void set_sample_filter_freq(uint8_t sample, float s_filter_freq);
extern void set_sample_filter_q(uint8_t sample, float s_filter_q);

extern void set_sample_delay1(uint8_t sample, float s_delay1);
extern void set_sample_delay2(uint8_t sample, float s_delay2);

extern void handleStop(void);
extern void handleStart(void);
extern void dac_mute(void);
extern void dac_unmute(void);
extern void check_configuration_sys(void);
extern uint8_t get_sample_note(uint8_t sample);
extern float get_sample_pitch(uint8_t sample);
extern float get_sample_p_offset(uint8_t sample);
extern float get_sample_pan(uint8_t sample);
extern float get_sample_vol_max(uint8_t sample);
extern float get_sample_reverb_send(uint8_t sample);
extern uint8_t get_sample_filter_mode(uint8_t sample);
extern float get_sample_filter_freq(uint8_t sample);
extern float get_sample_filter_q(uint8_t sample);

extern float get_sample_delay1(uint8_t sample);
extern float get_sample_delay2(uint8_t sample);

extern uint8_t get_sample_env_attack(uint8_t sample);
extern uint8_t get_sample_env_hold(uint8_t sample);
extern uint8_t get_sample_env_decay(uint8_t sample);
extern uint8_t get_sample_env_sustain(uint8_t sample);
extern uint8_t get_sample_env_release(uint8_t sample);
extern void set_sample_env_attack(uint8_t sample, uint8_t value);
extern void set_sample_env_hold(uint8_t sample, uint8_t value);
extern void set_sample_env_decay(uint8_t sample, uint8_t value);
extern void set_sample_env_sustain(uint8_t sample, uint8_t value);
extern void set_sample_env_release(uint8_t sample, uint8_t value);


extern multisample_t msp[NUM_MULTISAMPLES];
extern multisample_zone_t msz[NUM_MULTISAMPLES][NUM_MULTISAMPLE_ZONES];

extern bool sidechain_active;
extern uint8_t sidechain_sample_number;
extern int sidechain_speed;
extern uint8_t sidechain_steps;

File json;
char filename[CONFIG_FILENAME_LEN];
const char* sError = "*ERROR*";

#include "livesequencer.h"
#include "ui_livesequencer.h"
extern LiveSequencer liveSeq;

/******************************************************************************
   SD BANK/VOICE LOADING
 ******************************************************************************/

FLASHMEM bool load_sd_voice(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id)
{
#ifdef DEBUG
  LOG.printf_P(PSTR("load voice, bank [%d] - voice [%d]\n"), b, v + 1);
#endif
  p = constrain(p, 0, DEXED_POOLS - 1);
  v = constrain(v, 0, MAX_VOICES - 1);
  b = constrain(b, 0, MAX_BANKS - 1);

  if (sd_card_internal > 0)
  {
    File sysex_dir;
    char bankdir[FILENAME_LEN];
    char bank_name[BANK_NAME_LEN];
    char voice_name[VOICE_NAME_LEN];
    uint8_t data[128];

    //snprintf_P(bankdir, sizeof(bankdir), PSTR("/%s/%d/%d"), DEXED_CONFIG_PATH, p, b);
    sprintf(bankdir, "/%s/%d/%d", DEXED_CONFIG_PATH, p, b);

    AudioNoInterrupts();
    sysex_dir = SD.open(bankdir);
    AudioInterrupts();
    if (!sysex_dir)
    {
      strcpy(g_bank_name[instance_id], sError);
      strcpy(g_voice_name[instance_id], sError);

#ifdef DEBUG
      LOG.print(F("E : Cannot open "));
      LOG.print(bankdir);
      LOG.println(F(" on SD."));
#endif
      return (false);
    }

    File entry;
    do
    {
      entry = sysex_dir.openNextFile();
    } while (entry.isDirectory());

    if (entry.isDirectory())
    {
      AudioNoInterrupts();
      entry.close();
      sysex_dir.close();
      AudioInterrupts();
      strcpy(g_bank_name[instance_id], sError);
      strcpy(g_voice_name[instance_id], sError);
      return (false);
    }

    strip_extension(entry.name(), bank_name, BANK_NAME_LEN);
    string_toupper(bank_name);
    strcpy(g_bank_name[instance_id], bank_name);
#ifdef DEBUG
    char filename[FILENAME_LEN + 5];
    //snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.syx"), DEXED_CONFIG_PATH, b, bank_name);
    sprintf(filename, "/%s/%d/%d/%s.syx", DEXED_CONFIG_PATH, p, b, bank_name);
    LOG.print(F("Loading voice from "));
    LOG.print(filename);
    LOG.print(F(" bank:["));
    LOG.print(g_bank_name[instance_id]);
    LOG.println(F("]"));
#endif
    // search voice name
    memset(voice_name, '\0', VOICE_NAME_LEN);
    entry.seek(124 + (v * 128));
    entry.read(voice_name, min(VOICE_NAME_LEN, 10));
    string_toupper(voice_name);
    strcpy(g_voice_name[instance_id], voice_name);

    if (get_sd_voice(entry, v, data))
    {
#ifdef DEBUG
      LOG.print(F("get_sd_voice:["));
      LOG.print(g_voice_name[instance_id]);
      LOG.println(F("]"));
#endif
      uint8_t tmp_data[156];
      bool ret = MicroDexed[instance_id]->decodeVoice(tmp_data, data);
      MicroDexed[instance_id]->loadVoiceParameters(tmp_data);
#ifdef DEBUG
      show_patch(instance_id);
#endif

      AudioNoInterrupts();
      entry.close();
      sysex_dir.close();
      AudioInterrupts();
      MicroDexed[instance_id]->setTranspose(configuration.dexed[instance_id].transpose);
      configuration.dexed[instance_id].pool = p;
      configuration.dexed[instance_id].bank = b;
      configuration.dexed[instance_id].voice = v;

      uint8_t data_copy[156];
      MicroDexed[instance_id]->getVoiceData(data_copy);
      if (configuration.sys.send_sysex_on_voice_change) {
        send_sysex_voice(configuration.dexed[instance_id].midi_channel, data_copy);
      }
      if (configuration.sys.send_midi_cc) {
        init_MIDI_send_CC();
      }
      loadDexedOrigADSR(instance_id);
      return (ret);
    }
    else
    {
      strcpy(g_voice_name[instance_id], sError);
#ifdef DEBUG
      LOG.println(F("E : Cannot load voice data"));
#endif
    }
    AudioNoInterrupts();
    entry.close();
    sysex_dir.close();
    AudioInterrupts();

    return true;
  }

  strcpy(g_bank_name[instance_id], sError);
  strcpy(g_voice_name[instance_id], sError);
  return false;
}

FLASHMEM bool save_sd_voice(uint8_t p, uint8_t b, uint8_t v, uint8_t instance_id)
{
#ifdef DEBUG
  LOG.printf_P(PSTR("save_sd_voice, b:%d - d:%d\n"), b, v);
#endif
  p = constrain(p, 0, DEXED_POOLS - 1);
  v = constrain(v, 0, MAX_VOICES - 1);
  b = constrain(b, 0, MAX_BANKS - 1);

  if (sd_card_internal > 0)
  {
    File sysex;
    char filename[FILENAME_LEN];
    uint8_t data[128];

    //  snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%d/%s.syx"), DEXED_CONFIG_PATH, p, b, g_bank_name[instance_id]);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%d/%s.syx"), DEXED_CONFIG_PATH, p, b, g_bank_name[instance_id]);

    AudioNoInterrupts();
    sysex = SD.open(filename, FILE_WRITE);
    AudioInterrupts();
    if (!sysex)
    {
#ifdef DEBUG
      LOG.print(F("E : Cannot open "));
      LOG.print(filename);
      LOG.println(F(" on SD."));
#endif
      return (false);
    }

    MicroDexed[instance_id]->encodeVoice(data);

    if (put_sd_voice(sysex, v, data))
    {
#ifdef DEBUG
      LOG.print(F("Saving voice to "));
      LOG.print(filename);
      LOG.print(F(" ["));
      LOG.print(g_voice_name[instance_id]);
      LOG.println(F("]"));
#endif
      AudioNoInterrupts();
      sysex.close();
      AudioInterrupts();

      return (true);
    }
#ifdef DEBUG
    else
      LOG.println(F("E : Cannot load voice data"));
#endif
    AudioNoInterrupts();
    sysex.close();
    AudioInterrupts();
  }

  return (false);
}

FLASHMEM bool get_sd_voice(File sysex, uint8_t voice_number, uint8_t* data)
{
  int32_t bulk_checksum_calc = 0;
  int8_t bulk_checksum;

  AudioNoInterrupts();
  if (sysex.size() != 4104) // check sysex size
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx file size wrong."));
#endif
    return (false);
  }

  sysex.seek(0);
  if (sysex.read() != 0xf0) // check sysex start-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx start byte not found."));
#endif
    return (false);
  }
  if (sysex.read() != 0x43) // check sysex vendor is Yamaha
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx vendor not Yamaha."));
#endif
    return (false);
  }
  sysex.seek(4103);
  if (sysex.read() != 0xf7) // check sysex end-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx end byte not found."));
#endif
    return (false);
  }
  sysex.seek(3);
  if (sysex.read() != 0x09) // check for sysex type (0x09=32 voices)
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx type not 32 voices."));
#endif
    return (false);
  }
  sysex.seek(4102); // Bulk checksum
  bulk_checksum = sysex.read();

  sysex.seek(6); // start of bulk data
  for (int n = 0; n < 4096; n++)
  {
    uint8_t d = sysex.read();
    if (n >= voice_number * 128 && n < (voice_number + 1) * 128)
      data[n - (voice_number * 128)] = d;
    bulk_checksum_calc -= d;
  }
  bulk_checksum_calc &= 0x7f;
  AudioInterrupts();

#ifdef DEBUG
  LOG.print(F("Bulk checksum : 0x"));
  LOG.print(bulk_checksum_calc, HEX);
  LOG.print(F(" [0x"));
  LOG.print(bulk_checksum, HEX);
  LOG.println(F("]"));
#endif

  if (bulk_checksum_calc != bulk_checksum)
  {
#ifdef DEBUG
    LOG.print(F("E : Bulk checksum mismatch : 0x"));

    LOG.print(bulk_checksum_calc, HEX);
    LOG.print(F(" != 0x"));
    LOG.println(bulk_checksum, HEX);
#endif
    return (false);
  }

  // MicroDexed[0]->resetRenderTimeMax(); // necessary?

  return (true);
}

FLASHMEM bool put_sd_voice(File sysex, uint8_t voice_number, uint8_t* data)
{
  int32_t bulk_checksum_calc = 0;

  if (!sysex) return false; // Check file is valid

  AudioNoInterrupts();

  // Check if file is writable
  if (sysex.size() != 4104) {
    AudioInterrupts();
    return false;
  }

  sysex.seek(0);

  if (sysex.size() != 4104) // check sysex size
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx file size wrong."));
#endif
    return (false);
  }
  if (sysex.read() != 0xf0) // check sysex start-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx start byte not found."));
#endif
    return (false);
  }
  if (sysex.read() != 0x43) // check sysex vendor is Yamaha
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx vendor not Yamaha."));
#endif
    return (false);
  }
  sysex.seek(4103);
  if (sysex.read() != 0xf7) // check sysex end-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx end byte not found."));
#endif
    return (false);
  }
  sysex.seek(3);
  if (sysex.read() != 0x09) // check for sysex type (0x09=32 voices)
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx type not 32 voices."));
#endif
    return (false);
  }

  sysex.seek(6 + (voice_number * 128));
  sysex.write(data, 128);

  // checksum calculation
  sysex.seek(6); // start of bulk data
  for (int n = 0; n < 4096; n++)
  {
    uint8_t d = sysex.read();
    bulk_checksum_calc -= d;
  }
  sysex.seek(4102); // Bulk checksum
  sysex.write(bulk_checksum_calc & 0x7f);
  AudioInterrupts();

#ifdef DEBUG
  LOG.print(F("Bulk checksum : 0x"));
  LOG.println(bulk_checksum_calc & 0x7f, HEX);
#endif

  return (true);
}

extern uint8_t sysex_pool_number;
extern uint8_t sysex_bank_number;

FLASHMEM bool save_sd_bank(const char* bank_filename, uint8_t* data)
{
  char tmp[30];
  char tmp2[35];
  //int bank_number;
  File root, entry;

  if (sd_card_internal > 0)
  {
#ifdef DEBUG
    LOG.print(F("Trying so store "));
    LOG.print(bank_filename);
    LOG.println(F("."));
#endif

    // first remove old bank => find the bank number
    //sscanf(bank_filename, "/%s/%d/%s", tmp, &bank_number, tmp2);
    //snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d"), DEXED_CONFIG_PATH, bank_number);

    snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%d"), DEXED_CONFIG_PATH, sysex_pool_number, sysex_bank_number);
    AudioNoInterrupts();
    root = SD.open(tmp);
    while (42 == 42)
    {
      entry = root.openNextFile();
      if (entry)
      {
        if (!entry.isDirectory())
        {
#ifdef DEBUG
          LOG.print(F("Removing "));
          LOG.print(tmp);
          LOG.print(F("/"));
          LOG.println(entry.name());
#endif
          //snprintf_P(tmp2, sizeof(tmp2), PSTR("/%s/%s/%s"), DEXED_CONFIG_PATH, tmp, entry.name());
          snprintf(tmp2, sizeof(tmp2), "%s/%s", tmp, entry.name());
          entry.close();
#ifdef DEBUG
          LOG.printf("Remove file %s\n", tmp2);
#else
          bool r = SD.remove(tmp2);
          if (r == false)
          {
            LOG.print(F("E: cannot remove "));
            LOG.print(tmp2);
            LOG.println(F("."));
          }
#endif
          break;
        }
      }
      else
      {
        break;
      }
    }
    root.close();

    // store new bank at /DEXED/pool/bank/bank_name.syx
#ifdef DEBUG
    LOG.print(F("Storing bank as "));
    LOG.print(bank_filename);
    LOG.print(F("..."));
#endif

    snprintf_P(tmp, sizeof(tmp), PSTR("/%s/%d/%d/%s"), DEXED_CONFIG_PATH, sysex_pool_number, sysex_bank_number, bank_filename);
    root = SD.open(tmp, FILE_WRITE);
    // root = SD.open(bank_filename, FILE_WRITE);

    root.write(data, 4104);
    root.close();
    AudioInterrupts();
#ifdef DEBUG
    LOG.println(F(" done."));
#endif
  }
  else
    return (false);

  return (true);
}

/******************************************************************************
   DRUM ENVELOPES
 ******************************************************************************/


FLASHMEM bool load_sd_envelopes_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, ENVELOPES_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load

      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

        for (uint8_t i = 0; i < NUM_DRUMSET_CONFIG - 1; i++)
        {

          set_sample_env_attack(i, data_json["env_attack"][i]);

          if (data_json["env_hold"][i] == 0 && data_json["env_decay"][i] == 0)
          {
            set_sample_env_hold(i, 50);
            set_sample_env_decay(i, 60);
          }
          else
          {
            set_sample_env_hold(i, data_json["env_hold"][i]);
            set_sample_env_decay(i, data_json["env_decay"][i]);
          }
          set_sample_env_sustain(i, data_json["env_sus"][i]);
          set_sample_env_release(i, data_json["env_rel"][i]);
        }

        return (true);
      }
    }
    else
    {
      AudioInterrupts();

      for (uint8_t i = 0; i < NUM_DRUMSET_CONFIG - 1; i++)
      {
        set_sample_env_attack(i, 0);
        set_sample_env_hold(i, 50);
        set_sample_env_decay(i, 60);
        set_sample_env_sustain(i, 50);
        set_sample_env_release(i, 10);
      }

      return (true);
    }
  }
  AudioInterrupts();
  return (false);
}

FLASHMEM bool save_sd_envelopes_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, ENVELOPES_CONFIG_NAME);

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      for (uint8_t i = 0; i < NUM_DRUMSET_CONFIG - 1; i++)
      {
        data_json["env_attack"][i] = get_sample_env_attack(i);
        data_json["env_hold"][i] = get_sample_env_hold(i);
        data_json["env_decay"][i] = get_sample_env_decay(i);
        data_json["env_sus"][i] = get_sample_env_sustain(i);
        data_json["env_rel"][i] = get_sample_env_release(i);
      }

      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  AudioInterrupts();
  return (false);
}


/******************************************************************************
   SD DRUM CUSTOM MAPPINGS
 ******************************************************************************/

FLASHMEM bool load_sd_drummappings_json(uint8_t number)
{
#if NUM_DRUMS > 0
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, DRUMS_MAPPING_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found drum mapping ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
#ifdef DEBUG
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        for (uint8_t i = 0; i < NUM_CUSTOM_MIDI_MAPPINGS; i++)
        {
          custom_midi_map[i].type = data_json["type"][i];
          custom_midi_map[i].in = data_json["in"][i];
          custom_midi_map[i].out = data_json["out"][i];
          custom_midi_map[i].channel = data_json["channel"][i];
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
  }
#endif
  return (false);
}

FLASHMEM bool save_sd_drummappings_json(uint8_t number)
{
#if NUM_DRUMS > 0
  if (sd_card_internal > 0)
  {
    number = constrain(number, 0, 99);
    if (check_performance_directory(number))
    {
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, DRUMS_MAPPING_NAME);

#ifdef DEBUG
      LOG.print(F("Saving drum mapping "));
      LOG.print(number);
      LOG.print(F(" to "));
      LOG.println(filename);
#endif
      AudioNoInterrupts();
      if (SD.exists(filename))
      {
#ifdef DEBUG
        LOG.println(F("remove old drum mapping file"));
#endif
        SD.remove(filename);
      }
      json = SD.open(filename, FILE_WRITE);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        for (uint8_t i = 0; i < NUM_CUSTOM_MIDI_MAPPINGS; i++)
        {
          data_json["type"][i] = custom_midi_map[i].type;
          data_json["in"][i] = custom_midi_map[i].in;
          data_json["out"][i] = custom_midi_map[i].out;
          data_json["channel"][i] = custom_midi_map[i].channel;
        }
#ifdef DEBUG
        LOG.println(F("Write JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        serializeJsonPretty(data_json, json);
        json.close();
        AudioInterrupts();
        return (true);
      }
      else
      {
#ifdef DEBUG
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
#endif
        AudioInterrupts();
        return (false);
      }
    }
    else
    {
      AudioInterrupts();
      return (false);
    }
  }
#ifdef DEBUG
  else
  {
    LOG.println(F("E: SD card not available"));
  }
#endif
#endif
  return (false);
}

/******************************************************************************
   SD DRUMSETTINGS
 ******************************************************************************/
FLASHMEM bool load_sd_drumsettings_json(uint8_t number)
{
#if NUM_DRUMS > 0
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, DRUMS_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found drums configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        // LOG.println(F("length data:"));
        // LOG.println(data_json["note"].size());
        // LOG.println(F("--------------------------------------------------------------------------------------------"));

        // auto convert non-custom performances to new format. Should not hurt for new version data
        uint8_t offset1 = 0;
        uint8_t offset2 = 0;
        boolean oldformat = false;

        if (data_json["note"].size() == 70)  // old size without custom samples
        {
          offset1 = NUM_CUSTOM_SAMPLES;
          oldformat = true;
        }

        // LOG.println(F("--------------------------------------------------------------------------------------------"));
        // LOG.print("Old Format: ");
        // LOG.print(oldformat);
        // LOG.println(F("--------------------------------------------------------------------------------------------"));

        seq.drums_volume = data_json["drums_volume"];
        set_drums_volume(seq.drums_volume);
        for (uint8_t i = 0; i < NUM_DRUMSET_CONFIG - 1 - offset1; i++) {
          if (oldformat) {
            offset2 = (i > 6) ? NUM_CUSTOM_SAMPLES : 0;
          }
          const uint8_t index = i + offset2;
          // conversion end

          // LOG.println(F("--------------------------------------------------------------------------------------------"));
          // LOG.print("Drum Number: ");
          // LOG.print(i);
          // LOG.print("Index: ");
          // LOG.print(index);
          // LOG.print("  Delay2: ");
          // if (data_json["d2"][i] > 0.01f)
          //   LOG.print("FOUND");
          // LOG.println();

          set_sample_pitch(index, data_json["pitch"][i]);
          set_sample_p_offset(index, data_json["p_offset"][i]);
          set_sample_pan(index, data_json["pan"][i]);
          if (data_json["vol_max"][i] > 0.01f) {
            set_sample_vol_max(index, data_json["vol_max"][i]);
          }
          else {
            set_sample_vol_max(index, 1.00f);
          }
          set_sample_reverb_send(index, data_json["reverb_send"][i]);
          set_sample_filter_mode(index, data_json["f_mode"][i]);
          set_sample_filter_freq(index, data_json["f_freq"][i]);
          set_sample_filter_q(index, data_json["f_q"][i]);
          set_sample_delay1(index, data_json["d1"][i]);
          set_sample_delay2(index, data_json["d2"][i]);
        }

        if (oldformat) {
          for (uint8_t i = NUM_STATIC_PITCHED_SAMPLES; i < NUM_CUSTOM_SAMPLES + NUM_STATIC_PITCHED_SAMPLES; i++)
          {
            set_sample_filter_freq(i, 0);
          }
        }

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
  }
#endif
  return (false);
}

FLASHMEM bool save_sd_drumsettings_json(uint8_t number)
{
#if NUM_DRUMS > 0
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    if (check_performance_directory(number))
    {
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, DRUMS_CONFIG_NAME);

#ifdef DEBUG
      LOG.print(F("Saving drums config "));
      LOG.print(number);
      LOG.print(F(" to "));
      LOG.println(filename);
#endif
      AudioNoInterrupts();
      if (SD.exists(filename))
      {
#ifdef DEBUG
        LOG.println(F("remove old drumsettings file"));
#endif
        SD.remove(filename);
      }
      json = SD.open(filename, FILE_WRITE);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        data_json["drums_volume"] = seq.drums_volume;
        for (uint8_t i = 0; i < NUM_DRUMSET_CONFIG - 1; i++)
        {

          // LOG.println(F("--------------------------------------------------------------------------------------------"));
          // LOG.print("NUM_DRUMSET_CONFIG: ");
          // LOG.print(NUM_DRUMSET_CONFIG - 1);
          // LOG.print("Drum Number: ");
          // LOG.print(i);
          // LOG.print(" Delay2: ");
          // LOG.print(get_sample_delay2(i));
          // LOG.println();

          data_json["note"][i] = get_sample_note(i);
          data_json["pitch"][i] = get_sample_pitch(i);
          data_json["p_offset"][i] = get_sample_p_offset(i);
          data_json["pan"][i] = get_sample_pan(i);
          data_json["vol_max"][i] = get_sample_vol_max(i);
          data_json["reverb_send"][i] = get_sample_reverb_send(i);
          data_json["f_mode"][i] = get_sample_filter_mode(i);
          data_json["f_freq"][i] = get_sample_filter_freq(i);
          data_json["f_q"][i] = get_sample_filter_q(i);
          data_json["d1"][i] = get_sample_delay1(i);
          data_json["d2"][i] = get_sample_delay2(i);

        }
#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Write JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        serializeJsonPretty(data_json, json);
        json.close();
        AudioInterrupts();
        return (true);
      }
      else
      {
#ifdef DEBUG
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
#endif
        AudioInterrupts();
        return (false);
      }
    }
    else
    {
      AudioInterrupts();
      return (false);
    }
  }
#ifdef DEBUG
  else
  {
    LOG.println(F("E: SD card not available"));
  }
#endif
#endif
  return (false);
}

/******************************************************************************
   SD VOICECONFIG
 ******************************************************************************/
FLASHMEM bool load_sd_voiceconfig_json(uint8_t vc, uint8_t instance_id)
{
  if (sd_card_internal > 0)
  {
    vc = constrain(vc, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, vc, VOICE_CONFIG_NAME, instance_id + 1);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found voice configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        configuration.dexed[instance_id].pool = data_json["pool"];
        configuration.dexed[instance_id].bank = data_json["bank"];
        configuration.dexed[instance_id].voice = data_json["voice"];
        configuration.dexed[instance_id].lowest_note = data_json["lowest_note"];
        configuration.dexed[instance_id].highest_note = data_json["highest_note"];
        configuration.dexed[instance_id].transpose = data_json["transpose"];
        configuration.dexed[instance_id].tune = data_json["tune"];
        configuration.dexed[instance_id].sound_intensity = data_json["sound_intensity"];
        configuration.dexed[instance_id].pan = data_json["pan"];
        configuration.dexed[instance_id].polyphony = data_json["polyphony"];
        configuration.dexed[instance_id].velocity_level = data_json["velocity_level"];
        configuration.dexed[instance_id].monopoly = data_json["monopoly"];
        configuration.dexed[instance_id].note_refresh = data_json["note_refresh"];
        configuration.dexed[instance_id].pb_range = data_json["pb_range"];
        configuration.dexed[instance_id].pb_step = data_json["pb_step"];
        configuration.dexed[instance_id].mw_range = data_json["mw_range"];
        configuration.dexed[instance_id].mw_assign = data_json["mw_assign"];
        configuration.dexed[instance_id].mw_mode = data_json["mw_mode"];
        configuration.dexed[instance_id].fc_range = data_json["fc_range"];
        configuration.dexed[instance_id].fc_assign = data_json["fc_assign"];
        configuration.dexed[instance_id].fc_mode = data_json["fc_mode"];
        configuration.dexed[instance_id].bc_range = data_json["bc_range"];
        configuration.dexed[instance_id].bc_assign = data_json["bc_assign"];
        configuration.dexed[instance_id].bc_mode = data_json["bc_mode"];
        configuration.dexed[instance_id].at_range = data_json["at_range"];
        configuration.dexed[instance_id].at_assign = data_json["at_assign"];
        configuration.dexed[instance_id].at_mode = data_json["at_mode"];
        configuration.dexed[instance_id].portamento_mode = data_json["portamento_mode"];
        configuration.dexed[instance_id].portamento_glissando = data_json["portamento_glissando"];
        configuration.dexed[instance_id].portamento_time = data_json["portamento_time"];
        configuration.dexed[instance_id].op_enabled = data_json["op_enabled"];
        configuration.dexed[instance_id].midi_channel = data_json["midi_channel"];
        configuration.dexed[instance_id].sidechain_send = data_json["sidechain_send"];

        check_configuration_dexed(instance_id);
        set_voiceconfig_params(instance_id);

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
  }

  return (false);
}



FLASHMEM bool load_sd_voiceconfig_minimal_boot_json(uint8_t vc, uint8_t instance_id)
{
  if (sd_card_internal > 0)
  {
    vc = constrain(vc, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, vc, VOICE_CONFIG_NAME, instance_id + 1);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found voice configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        configuration.dexed[instance_id].polyphony = data_json["polyphony"];

        return (true);
      }

    }
  }

  return (false);
}



FLASHMEM bool save_sd_voiceconfig_json(uint8_t vc, uint8_t instance_id)
{
  if (sd_card_internal > 0)
  {
    vc = constrain(vc, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, vc, VOICE_CONFIG_NAME, instance_id + 1);

#ifdef DEBUG
    LOG.print(F("Saving voice config "));
    LOG.print(vc);
    LOG.print(F("["));
    LOG.print(instance_id);
    LOG.print(F("]"));
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["pool"] = configuration.dexed[instance_id].pool;
      data_json["bank"] = configuration.dexed[instance_id].bank;
      data_json["voice"] = configuration.dexed[instance_id].voice;
      data_json["lowest_note"] = configuration.dexed[instance_id].lowest_note;
      data_json["highest_note"] = configuration.dexed[instance_id].highest_note;
      data_json["transpose"] = configuration.dexed[instance_id].transpose;
      data_json["tune"] = configuration.dexed[instance_id].tune;
      data_json["sound_intensity"] = configuration.dexed[instance_id].sound_intensity;
      data_json["pan"] = configuration.dexed[instance_id].pan;
      data_json["polyphony"] = configuration.dexed[instance_id].polyphony;
      data_json["velocity_level"] = configuration.dexed[instance_id].velocity_level;
      data_json["monopoly"] = configuration.dexed[instance_id].monopoly;
      data_json["note_refresh"] = configuration.dexed[instance_id].note_refresh;
      data_json["pb_range"] = configuration.dexed[instance_id].pb_range;
      data_json["pb_step"] = configuration.dexed[instance_id].pb_step;
      data_json["mw_range"] = configuration.dexed[instance_id].mw_range;
      data_json["mw_assign"] = configuration.dexed[instance_id].mw_assign;
      data_json["mw_mode"] = configuration.dexed[instance_id].mw_mode;
      data_json["fc_range"] = configuration.dexed[instance_id].fc_range;
      data_json["fc_assign"] = configuration.dexed[instance_id].fc_assign;
      data_json["fc_mode"] = configuration.dexed[instance_id].fc_mode;
      data_json["bc_range"] = configuration.dexed[instance_id].bc_range;
      data_json["bc_assign"] = configuration.dexed[instance_id].bc_assign;
      data_json["bc_mode"] = configuration.dexed[instance_id].bc_mode;
      data_json["at_range"] = configuration.dexed[instance_id].at_range;
      data_json["at_assign"] = configuration.dexed[instance_id].at_assign;
      data_json["at_mode"] = configuration.dexed[instance_id].at_mode;
      data_json["portamento_mode"] = configuration.dexed[instance_id].portamento_mode;
      data_json["portamento_glissando"] = configuration.dexed[instance_id].portamento_glissando;
      data_json["portamento_time"] = configuration.dexed[instance_id].portamento_time;
      data_json["op_enabled"] = configuration.dexed[instance_id].op_enabled;
      data_json["midi_channel"] = configuration.dexed[instance_id].midi_channel;
      data_json["sidechain_send"] = configuration.dexed[instance_id].sidechain_send;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();

      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  return (false);
}

/******************************************************************************
   SD MICROSYNTH
 ******************************************************************************/
FLASHMEM bool load_sd_microsynth_json(uint8_t ms, uint8_t instance_id)
{
  if (sd_card_internal > 0)
  {
    ms = constrain(ms, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, ms, MICROSYNTH_CONFIG_NAME, instance_id + 1);

    microsynth_reset_instance(instance_id);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found micro synth configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        microsynth[instance_id].sound_intensity = data_json["sound_intensity"];
        microsynth[instance_id].wave = data_json["wave"];
        microsynth[instance_id].coarse = data_json["coarse"];
        microsynth[instance_id].detune = data_json["detune"];
        microsynth[instance_id].trigger_noise_with_osc = data_json["trigger_noise_with_osc"];
        microsynth[instance_id].env_attack = data_json["env_attack"];
        microsynth[instance_id].env_decay = data_json["env_decay"];
        microsynth[instance_id].env_sustain = data_json["env_sustain"];
        microsynth[instance_id].env_release = data_json["env_release"];
        microsynth[instance_id].filter_osc_mode = data_json["filter_osc_mode"];
        microsynth[instance_id].filter_osc_freq_from = data_json["filter_osc_freq_from"];
        microsynth[instance_id].filter_osc_freq_to = data_json["filter_osc_freq_to"];
        microsynth[instance_id].filter_osc_speed = data_json["filter_osc_speed"];
        microsynth[instance_id].filter_osc_resonance = data_json["filter_osc_resonance"];
        microsynth[instance_id].noise_vol = data_json["noise_vol"];
        microsynth[instance_id].noise_decay = data_json["noise_decay"];
        microsynth[instance_id].filter_noise_freq_from = data_json["filter_noise_freq_from"];
        microsynth[instance_id].filter_noise_freq_to = data_json["filter_noise_freq_to"];
        microsynth[instance_id].filter_noise_speed = data_json["filter_noise_speed"];
        microsynth[instance_id].filter_noise_mode = data_json["filter_noise_mode"];
        microsynth[instance_id].filter_noise_resonance = data_json["filter_noise_resonance"];
        microsynth[instance_id].lfo_intensity = data_json["lfo_intensity"];
        microsynth[instance_id].lfo_mode = data_json["lfo_mode"];
        microsynth[instance_id].lfo_delay = data_json["lfo_delay"];
        microsynth[instance_id].lfo_speed = data_json["lfo_speed"];
        microsynth[instance_id].pwm_from = data_json["pwm_from"];
        microsynth[instance_id].pwm_to = data_json["pwm_to"];
        microsynth[instance_id].pwm_speed = data_json["pwm_speed"];
        microsynth[instance_id].rev_send = data_json["rev_send"];
        microsynth[instance_id].chorus_send = data_json["chorus_send"];
        microsynth[instance_id].delay_send[0] = data_json["delay_send_1"];
        microsynth[instance_id].delay_send[1] = data_json["delay_send_2"];
        microsynth[instance_id].midi_channel = data_json["midi_channel"];
        microsynth[instance_id].pan = data_json["pan"];
        microsynth[instance_id].vel_mod_filter_osc = data_json["vel_mod_filter_osc"];
        microsynth[instance_id].vel_mod_filter_noise = data_json["vel_mod_filter_noise"];

        microsynth[instance_id].sidechain_send = data_json["sidechain_send"];

        microsynth_update_all_settings(instance_id);

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
  }
  return (false);
}

FLASHMEM bool save_sd_microsynth_json(uint8_t ms, uint8_t instance_id)
{
  if (sd_card_internal > 0)
  {
    ms = constrain(ms, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, ms, MICROSYNTH_CONFIG_NAME, instance_id + 1);

#ifdef DEBUG
    LOG.print(F("Saving microsynth config "));
    LOG.print(ms);
    LOG.print(F("["));
    LOG.print(instance_id);
    LOG.print(F("]"));
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["sound_intensity"] = microsynth[instance_id].sound_intensity;
      data_json["wave"] = microsynth[instance_id].wave;
      data_json["coarse"] = microsynth[instance_id].coarse;
      data_json["detune"] = microsynth[instance_id].detune;
      data_json["trigger_noise_with_osc"] = microsynth[instance_id].trigger_noise_with_osc;
      data_json["env_attack"] = microsynth[instance_id].env_attack;
      data_json["env_decay"] = microsynth[instance_id].env_decay;
      data_json["env_sustain"] = microsynth[instance_id].env_sustain;
      data_json["env_release"] = microsynth[instance_id].env_release;
      data_json["filter_osc_mode"] = microsynth[instance_id].filter_osc_mode;
      data_json["filter_osc_freq_from"] = microsynth[instance_id].filter_osc_freq_from;
      data_json["filter_osc_freq_to"] = microsynth[instance_id].filter_osc_freq_to;
      data_json["filter_osc_speed"] = microsynth[instance_id].filter_osc_speed;
      data_json["filter_osc_resonance"] = microsynth[instance_id].filter_osc_resonance;
      data_json["noise_vol"] = microsynth[instance_id].noise_vol;
      data_json["noise_decay"] = microsynth[instance_id].noise_decay;
      data_json["filter_noise_freq_from"] = microsynth[instance_id].filter_noise_freq_from;
      data_json["filter_noise_freq_to"] = microsynth[instance_id].filter_noise_freq_to;
      data_json["filter_noise_mode"] = microsynth[instance_id].filter_noise_mode;
      data_json["filter_noise_speed"] = microsynth[instance_id].filter_noise_speed;
      data_json["filter_noise_resonance"] = microsynth[instance_id].filter_noise_resonance;
      data_json["lfo_intensity"] = microsynth[instance_id].lfo_intensity;
      data_json["lfo_mode"] = microsynth[instance_id].lfo_mode;
      data_json["lfo_delay"] = microsynth[instance_id].lfo_delay;
      data_json["lfo_speed"] = microsynth[instance_id].lfo_speed;
      data_json["pwm_from"] = microsynth[instance_id].pwm_from;
      data_json["pwm_to"] = microsynth[instance_id].pwm_to;
      data_json["pwm_speed"] = microsynth[instance_id].pwm_speed;
      data_json["rev_send"] = microsynth[instance_id].rev_send;
      data_json["chorus_send"] = microsynth[instance_id].chorus_send;
      data_json["delay_send_1"] = microsynth[instance_id].delay_send[0];
      data_json["delay_send_2"] = microsynth[instance_id].delay_send[1];
      data_json["midi_channel"] = microsynth[instance_id].midi_channel;
      data_json["pan"] = microsynth[instance_id].pan;
      data_json["vel_mod_filter_osc"] = microsynth[instance_id].vel_mod_filter_osc;
      data_json["vel_mod_filter_noise"] = microsynth[instance_id].vel_mod_filter_noise;

      data_json["sidechain_send"] = microsynth[instance_id].sidechain_send;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();

      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
}

/******************************************************************************
   SD FX
 ******************************************************************************/
#ifdef BITCRUSH
extern void update_bitcrush_parameters();
#endif


FLASHMEM bool load_sd_fx_json(uint8_t number)
{
  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);

  load_sd_drumsettings_json(number);

  if (sd_card_internal > 0)
  {
    //snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, FX_CONFIG_NAME);
    sprintf(filename, "/%s/%d/%s.json", PERFORMANCE_CONFIG_PATH, number, FX_CONFIG_NAME);
    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found fx configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        for (uint8_t i = 0; i < NUM_DEXED; i++)
        {
          configuration.fx.filter_cutoff[i] = data_json["filter_cutoff"][i];
          configuration.fx.filter_resonance[i] = data_json["filter_resonance"][i];
          configuration.fx.chorus_frequency[i] = data_json["chorus_frequency"][i];
          configuration.fx.chorus_waveform[i] = data_json["chorus_waveform"][i];
          configuration.fx.chorus_depth[i] = data_json["chorus_depth"][i];
          configuration.fx.chorus_level[i] = data_json["chorus_level"][i];
          configuration.fx.delay_multiplier[i] = data_json["delay_multiplier"][i];
          configuration.fx.delay_time[i] = data_json["delay_time"][i];
          configuration.fx.delay_feedback[i] = data_json["delay_feedback"][i];
          configuration.fx.delay_level1[i] = data_json["delay_level"][i];
          configuration.fx.delay_level2[i] = data_json["delay_level2"][i];
          configuration.fx.delay_sync[i] = data_json["delay_sync"][i];
          configuration.fx.delay_pan[i] = data_json["delay_pan"][i];
          configuration.fx.reverb_send[i] = data_json["reverb_send"][i];
          configuration.fx.delay_to_reverb[i] = data_json["delay_to_reverb"][i];

          configuration.fx.delay_filter_mode[i] = data_json["delay_filter_mode"][i];
          configuration.fx.delay_filter_freq[i] = data_json["delay_filter_freq"][i];

          if (configuration.fx.delay_sync[i] > 0)
            configuration.fx.delay_time[i] = 0;
          configuration.fx.delay_level_global[i] = data_json["delay_level_global"][i];
        }

        configuration.fx.delay1_to_delay2 = data_json["delay1_to_delay2"];
        configuration.fx.delay2_to_delay1 = data_json["delay2_to_delay1"];
        configuration.fx.reverb_roomsize = data_json["reverb_roomsize"];
        configuration.fx.reverb_damping = data_json["reverb_damping"];
        configuration.fx.reverb_lowpass = data_json["reverb_lowpass"];
        configuration.fx.reverb_lodamp = data_json["reverb_lodamp"];
        configuration.fx.reverb_hidamp = data_json["reverb_hidamp"];
        configuration.fx.reverb_diffusion = data_json["reverb_diffusion"];
        configuration.fx.reverb_level = data_json["reverb_level"];
        configuration.fx.ep_chorus_frequency = data_json["ep_chorus_frequency"];
        configuration.fx.ep_chorus_waveform = data_json["ep_chorus_waveform"];
        configuration.fx.ep_chorus_depth = data_json["ep_chorus_depth"];
        configuration.fx.ep_chorus_level = data_json["ep_chorus_level"];
        configuration.fx.ep_reverb_send = data_json["ep_reverb_send"];
        configuration.fx.ep_delay_send_1 = data_json["ep_delay1"];
        configuration.fx.ep_delay_send_2 = data_json["ep_delay2"];

        configuration.fx.delay1_sidechain_send = data_json["delay1_sidechain_send"];
        configuration.fx.delay2_sidechain_send = data_json["delay2_sidechain_send"];
        configuration.fx.reverb_sidechain_send = data_json["reverb_sidechain_send"];

#ifdef BITCRUSH
        configuration.fx.bitcrush_drive = data_json["bitcrush_drive"];
        configuration.fx.bitcrush_output_gain = data_json["bitcrush_output_gain"];
        configuration.fx.bitcrush_dexed1_input = data_json["bitcrush_dexed1_input"];
        configuration.fx.bitcrush_dexed2_input = data_json["bitcrush_dexed2_input"];
        configuration.fx.bitcrush_dexed3_input = data_json["bitcrush_dexed3_input"];
        configuration.fx.bitcrush_dexed4_input = data_json["bitcrush_dexed4_input"];
        configuration.fx.bitcrush_samples_input_r = data_json["bitcrush_samples_input_r"];
        configuration.fx.bitcrush_samples_input_l = data_json["bitcrush_samples_input_l"];

        configuration.fx.bitcrush_delay1_send = data_json["bitcrush_delay1_send"];
        configuration.fx.bitcrush_delay2_send = data_json["bitcrush_delay2_send"];
        configuration.fx.bitcrush_reverb_send = data_json["bitcrush_reverb_send"];
#endif

#ifdef BITCRUSH
        update_bitcrush_parameters();
#endif

        check_configuration_fx();
        set_fx_params();

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
  }
  return (false);
}

FLASHMEM bool save_sd_fx_json(uint8_t number)
{
  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);

  save_sd_drumsettings_json(number);

  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, FX_CONFIG_NAME);

#ifdef DEBUG
    LOG.print(F("Saving fx config "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < NUM_DEXED; i++)
      {
        data_json["filter_cutoff"][i] = configuration.fx.filter_cutoff[i];
        data_json["filter_resonance"][i] = configuration.fx.filter_resonance[i];
        data_json["chorus_frequency"][i] = configuration.fx.chorus_frequency[i];
        data_json["chorus_waveform"][i] = configuration.fx.chorus_waveform[i];
        data_json["chorus_depth"][i] = configuration.fx.chorus_depth[i];
        data_json["chorus_level"][i] = configuration.fx.chorus_level[i];
        data_json["delay_multiplier"][i] = configuration.fx.delay_multiplier[i];
        data_json["delay_time"][i] = configuration.fx.delay_time[i];
        data_json["delay_feedback"][i] = configuration.fx.delay_feedback[i];
        data_json["delay_level"][i] = configuration.fx.delay_level1[i];
        data_json["delay_level2"][i] = configuration.fx.delay_level2[i];
        data_json["delay_level_global"][i] = configuration.fx.delay_level_global[i];
        data_json["delay_sync"][i] = configuration.fx.delay_sync[i];
        data_json["delay_pan"][i] = configuration.fx.delay_pan[i];
        data_json["reverb_send"][i] = configuration.fx.reverb_send[i];
        data_json["delay_to_reverb"][i] = configuration.fx.delay_to_reverb[i];

        data_json["delay_filter_mode"][i] = configuration.fx.delay_filter_mode[i];
        data_json["delay_filter_freq"][i] = configuration.fx.delay_filter_freq[i];
      }
      data_json["delay1_to_delay2"] = configuration.fx.delay1_to_delay2;
      data_json["delay2_to_delay1"] = configuration.fx.delay2_to_delay1;
      data_json["reverb_roomsize"] = configuration.fx.reverb_roomsize;
      data_json["reverb_damping"] = configuration.fx.reverb_damping;
      data_json["reverb_lowpass"] = configuration.fx.reverb_lowpass;
      data_json["reverb_lodamp"] = configuration.fx.reverb_lodamp;
      data_json["reverb_hidamp"] = configuration.fx.reverb_hidamp;
      data_json["reverb_diffusion"] = configuration.fx.reverb_diffusion;
      data_json["reverb_level"] = configuration.fx.reverb_level;
      data_json["ep_chorus_frequency"] = configuration.fx.ep_chorus_frequency;
      data_json["ep_chorus_waveform"] = configuration.fx.ep_chorus_waveform;
      data_json["ep_chorus_depth"] = configuration.fx.ep_chorus_depth;
      data_json["ep_chorus_level"] = configuration.fx.ep_chorus_level;
      data_json["ep_reverb_send"] = configuration.fx.ep_reverb_send;
      data_json["ep_delay1"] = configuration.fx.ep_delay_send_1;
      data_json["ep_delay2"] = configuration.fx.ep_delay_send_2;

      data_json["delay1_sidechain_send"] = configuration.fx.delay1_sidechain_send;
      data_json["delay2_sidechain_send"] = configuration.fx.delay2_sidechain_send;
      data_json["reverb_sidechain_send"] = configuration.fx.reverb_sidechain_send;

#ifdef BITCRUSH
      data_json["bitcrush_drive"] = configuration.fx.bitcrush_drive;
      data_json["bitcrush_output_gain"] = configuration.fx.bitcrush_output_gain;
      data_json["bitcrush_dexed1_input"] = configuration.fx.bitcrush_dexed1_input;
      data_json["bitcrush_dexed2_input"] = configuration.fx.bitcrush_dexed2_input;
      data_json["bitcrush_dexed3_input"] = configuration.fx.bitcrush_dexed3_input;
      data_json["bitcrush_dexed4_input"] = configuration.fx.bitcrush_dexed4_input;
      data_json["bitcrush_samples_input_r"] = configuration.fx.bitcrush_samples_input_r;
      data_json["bitcrush_samples_input_l"] = configuration.fx.bitcrush_samples_input_l;
      data_json["bitcrush_delay1_send"] = configuration.fx.bitcrush_delay1_send;
      data_json["bitcrush_delay2_send"] = configuration.fx.bitcrush_delay2_send;
      data_json["bitcrush_reverb_send"] = configuration.fx.bitcrush_reverb_send;
#endif


#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

/******************************************************************************
   SD EPIANO
 ******************************************************************************/
FLASHMEM bool load_sd_epiano_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, EPIANO_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found epiano configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        configuration.epiano.decay = data_json["decay"];
        configuration.epiano.release = data_json["release"];
        configuration.epiano.hardness = data_json["hardness"];
        configuration.epiano.treble = data_json["treble"];
        configuration.epiano.pan_tremolo = data_json["pan_tremolo"];
        configuration.epiano.pan_lfo = data_json["pan_lfo"];
        configuration.epiano.velocity_sense = data_json["velocity_sense"];
        configuration.epiano.stereo = data_json["stereo"];
        configuration.epiano.polyphony = data_json["polyphony"];
        configuration.epiano.tune = data_json["tune"];
        configuration.epiano.detune = data_json["detune"];
        configuration.epiano.overdrive = data_json["overdrive"];
        configuration.epiano.lowest_note = data_json["lowest_note"];
        configuration.epiano.highest_note = data_json["highest_note"];
        configuration.epiano.transpose = data_json["transpose"];
        configuration.epiano.sound_intensity = data_json["sound_intensity"];
        configuration.epiano.pan = data_json["pan"];
        configuration.epiano.midi_channel = data_json["midi_channel"];
        configuration.epiano.sidechain_send = data_json["sidechain_send"];

        check_configuration_epiano();
        set_epiano_params();

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool save_sd_epiano_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, EPIANO_CONFIG_NAME);

#ifdef DEBUG
    LOG.print(F("Saving epiano config "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["decay"] = configuration.epiano.decay;
      data_json["release"] = configuration.epiano.release;
      data_json["hardness"] = configuration.epiano.hardness;
      data_json["treble"] = configuration.epiano.treble;
      data_json["pan_tremolo"] = configuration.epiano.pan_tremolo;
      data_json["pan_lfo"] = configuration.epiano.pan_lfo;
      data_json["velocity_sense"] = configuration.epiano.velocity_sense;
      data_json["stereo"] = configuration.epiano.stereo;
      data_json["polyphony"] = configuration.epiano.polyphony;
      data_json["tune"] = configuration.epiano.tune;
      data_json["detune"] = configuration.epiano.detune;
      data_json["overdrive"] = configuration.epiano.overdrive;
      data_json["lowest_note"] = configuration.epiano.lowest_note;
      data_json["highest_note"] = configuration.epiano.highest_note;
      data_json["transpose"] = configuration.epiano.transpose;
      data_json["sound_intensity"] = configuration.epiano.sound_intensity;
      data_json["pan"] = configuration.epiano.pan;
      data_json["midi_channel"] = configuration.epiano.midi_channel;
      data_json["sidechain_send"] = configuration.epiano.sidechain_send;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  AudioInterrupts();
  return (false);
  }

FLASHMEM void serializeEventToJSON(JsonObject& o, LiveSequencer::EventVector::iterator it) {
  o["source"] = it->source;
  o["patternMs"] = it->patternMs;
  o["patternNumber"] = it->patternNumber;
  o["track"] = it->track;
  o["layer"] = it->layer;
  o["event"] = it->event;
  o["note_in"] = it->note_in;
  o["note_in_velocity"] = it->note_in_velocity;
}

FLASHMEM void deserializeJSONToEvent(JsonObject& o, LiveSequencer::MidiEvent& e) {
  e.source = o["source"];
  e.patternMs = o["patternMs"];
  e.patternNumber = o["patternNumber"];
  e.track = o["track"];
  e.layer = o["layer"];
  e.event = o["event"];
  e.note_in = o["note_in"];
  e.note_in_velocity = o["note_in_velocity"];
}

FLASHMEM void writeChunk(const char* filename, int chunkNumber, const int& NUM_EVENTS_PER_FILE, uint16_t numEvents, LiveSequencer::EventVector::iterator& it) {
  SD.remove(filename);
  json = SD.open(filename, FILE_WRITE);
  if (json) {
    const uint16_t eventsWritten = chunkNumber * NUM_EVENTS_PER_FILE;
    const uint16_t numChunkEvents = min(numEvents - eventsWritten, NUM_EVENTS_PER_FILE);
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    doc["number_of_events"] = numChunkEvents;
    //Serial.printf("save: %i chunk events\n", numChunkEvents);
    // Serial.printf("has %i events\n", numChunkEvents);
    for (uint16_t i = 0; i < numChunkEvents; i++) {
      JsonObject o = doc.createNestedObject(i);
      serializeEventToJSON(o, it++);
    }
    serializeJsonPretty(doc, json);
    //serializeJsonPretty(doc, Serial);
    json.close();
  }
}

/**
 * Converts old layer mute events to the new format.
 *
 * In old automation:
 * note_in is the layer
 * note_in_velocity is the mute state: 0 - mute,  1 - unmute
 *
 * In new automation (note_in and note_in_velocity are swapped and mute states have new values):
 * note_in is the mute state: TYPE_MUTE_ON or TYPE_MUTE_OFF
 * note_in_velocity is the layer number
 *
 * This should be used when loading legacy song chunks to ensure compatibility.
 */
static void convert_old_layer_mute_event(LiveSequencer::MidiEvent& e) {
  // Only process if note_in_velocity is 0 or 1 and layer is valid
  if (e.note_in_velocity > 1 || e.note_in >= LiveSequencer::LIVESEQUENCER_NUM_LAYERS)
    return;

#ifdef DEBUG
  LOG.print("Converting layer mute event ");
  LOG.print(F("[old : note_in="));
  LOG.print(e.note_in);
  LOG.print(F(", note_in_velocity="));
  LOG.print(e.note_in_velocity);
  LOG.print(F("] "));
#endif

  // from old automation:
  uint8_t layer = e.note_in;
  bool isMuted = e.note_in_velocity == 0; // old automation: 0 - muted, 1 - unmuted

  // to new automation:
  e.note_in = isMuted ? LiveSequencer::AutomationType::TYPE_MUTE_ON
    : LiveSequencer::AutomationType::TYPE_MUTE_OFF;
  e.note_in_velocity = layer;

#ifdef DEBUG
  LOG.print(F("[new: note_in="));
  LOG.print(e.note_in);
  LOG.print(F(", note_in_velocity="));
  LOG.print(e.note_in_velocity);
  LOG.println(F("]"));
#endif
}

FLASHMEM void readChunk(const char* filename, LiveSequencer::EventVector& list, bool isSongChunk = false) {
  if (SD.exists(filename)) {
    // Serial.printf("success\n", c, filename);
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    json = SD.open(filename, FILE_READ);
    doc.clear();
    deserializeJson(doc, json);
    json.close();

    uint16_t numChunkEvents = doc["number_of_events"];
    //Serial.printf("load: %i chunk events\n", numChunkEvents);

    for (uint16_t i = 0; i < numChunkEvents; i++) {
      LiveSequencer::MidiEvent e;
      JsonObject o = doc[String(i)];
      deserializeJSONToEvent(o, e);
      if (e.event == midi::MidiType::ControlChange && isSongChunk) {
        // If this is a song chunk, convert old layer mute events to the current format
        convert_old_layer_mute_event(e);
      }
      list.emplace_back(e);
    }
  }
}

FLASHMEM bool save_sd_livesequencer_json(uint8_t number)
{
  if (sd_card_internal > 0) {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME);

#ifdef DEBUG
    LOG.print(F("Saving live sequencer config "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    uint16_t numPatternChunks = 0;
    uint8_t lastSongPattern = 0;
    LiveSequencer::LiveSeqData* data = liveSeq.getData();

    static constexpr int NUM_EVENTS_PER_FILE = 50; // never change this!
    uint16_t numPatternEvents = data->eventsList.size();

    if (json) {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["num_bars"] = data->numberOfBars;
      data_json["num_tracks"] = LiveSequencer::LIVESEQUENCER_NUM_TRACKS;
      data_json["hasTrackInstruments"] = true; // has individual track instrument mapping
      for (uint8_t track = 0; track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS; track++) {
        data_json["device"][track] = data->trackSettings[track].device;
        data_json["instrument"][track] = data->trackSettings[track].instrument;
        data_json["layer_count"][track] = data->trackSettings[track].layerCount;
        data_json["quant_denom"][track] = data->trackSettings[track].quantizeDenom;
        data_json["velocity"][track] = data->trackSettings[track].velocityLevel;
        // if we already have recorded a song start, save its start mute states. otherwise save pattern mutes
        if (data->songLayerCount == 0) {
          data->trackSettings[track].songStartLayerMutes = data->tracks[track].layerMutes;
        }
        data_json["layer_mutes"][track] = data->trackSettings[track].songStartLayerMutes;
      }

      data_json["hasArpSettings"] = true; // has arp settings
      data_json["arpAmount"] = data->arpSettings.amount;
      data_json["arpEnabled"] = data->arpSettings.enabled;
      data_json["arpFreerun"] = data->arpSettings.freerun;
      data_json["arpLatch"] = data->arpSettings.latch;
      data_json["arpLength"] = data->arpSettings.length;
      data_json["arpLoadPerBar"] = data->arpSettings.loadPerBar;
      data_json["arpMode"] = data->arpSettings.mode;
      data_json["arpNoteRepeat"] = data->arpSettings.noteRepeat;
      data_json["arpOctaves"] = data->arpSettings.octaves;
      data_json["arpSource"] = data->arpSettings.source;
      data_json["arpSwing"] = data->arpSettings.swing;
      data_json["arpVelocity"] = data->arpSettings.velocityLevel;

      data_json["num_pattern_events"] = numPatternEvents;
      lastSongPattern = data->lastSongEventPattern;
      data_json["last_song_pattern"] = lastSongPattern;
      data_json["song_layer_count"] = data->songLayerCount;
      for (int i = 0; i <= lastSongPattern; i++) {
        // write num song pattern events per song pattern
        data_json["song_pattern_events"][i] = data->songEvents[i].size();
      }
      data_json["chunk_size"] = NUM_EVENTS_PER_FILE;
      numPatternChunks = ceil(numPatternEvents / float(NUM_EVENTS_PER_FILE)); // 50 events per file

      serializeJsonPretty(data_json, json);
      //serializeJsonPretty(data_json, Serial);
      json.close();
    }

    // save pattern event chunks
    //Serial.printf("pattern chunks: %i:\n", numPatternChunks);
    LiveSequencer::EventVector::iterator it = data->eventsList.begin();
    for (int chunkNumber = 0; chunkNumber < numPatternChunks; chunkNumber++) {
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s_pattern%03i.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME, chunkNumber);
      writeChunk(filename, chunkNumber, NUM_EVENTS_PER_FILE, numPatternEvents, it);
    }

    // loop through song patterns
    for (uint8_t songPattern = 0; songPattern <= lastSongPattern; songPattern++) {
      LiveSequencer::EventVector songPatternEvents = data->songEvents[songPattern];
      const uint16_t numSongPatternEvents = songPatternEvents.size();
      const uint16_t numSongPatternChunks = ceil(numSongPatternEvents / float(NUM_EVENTS_PER_FILE)); // 50 events per file
      //Serial.printf("song pattern %i has %i chunks:\n", songPattern, numSongPatternChunks);
      // save song pattern event chunks
      LiveSequencer::EventVector::iterator it = songPatternEvents.begin();
      for (int chunkNumber = 0; chunkNumber < numSongPatternChunks; chunkNumber++) {
        snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s_song%03i_%03i.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME, songPattern, chunkNumber);
        writeChunk(filename, chunkNumber, NUM_EVENTS_PER_FILE, numSongPatternEvents, it);
      }
    }
  }

  AudioInterrupts();
  return (true);
}

FLASHMEM bool load_sd_livesequencer_json(uint8_t number)
{
  AudioNoInterrupts();

  liveSeq.deleteLiveSequencerData();

  if (sd_card_internal > 0) {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME);
    if (SD.exists(filename)) {
#ifdef DEBUG
      LOG.print(F("Found livesequencer configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif
      json = SD.open(filename, FILE_READ);
      if (json) {
        LiveSequencer::LiveSeqData* data = liveSeq.getData();
        uint16_t numPatternEvents = 0;
        int numPatternChunks = 0;
        uint16_t chunksize = 0;
        uint8_t lastSongPattern = 0;
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        std::vector<int> numSongPatternEvents;
        {
          doc.clear();
          deserializeJson(doc, json);
          json.close();
          data->numberOfBars = doc["num_bars"];
          const uint8_t numTracksLoaded = doc["num_tracks"];
          const uint8_t numTracks = std::min(numTracksLoaded, LiveSequencer::LIVESEQUENCER_NUM_TRACKS); // clamp for compatibility
          const bool hasTrackInstruments = doc["hasTrackInstruments"];
          if (hasTrackInstruments == false) {
            liveSeq.loadOldTrackInstruments();
          }

          const bool hasArpSettings = doc["hasArpSettings"];
          if (hasArpSettings) {
            data->arpSettings.amount = doc["arpAmount"];
            data->arpSettings.enabled = doc["arpEnabled"];
            data->arpSettings.freerun = doc["arpFreerun"];
            data->arpSettings.latch = doc["arpLatch"];
            data->arpSettings.length = doc["arpLength"];
            data->arpSettings.loadPerBar = doc["arpLoadPerBar"];
            data->arpSettings.mode = doc["arpMode"];
            data->arpSettings.noteRepeat = doc["arpNoteRepeat"];
            data->arpSettings.octaves = doc["arpOctaves"];
            data->arpSettings.source = doc["arpSource"];
            data->arpSettings.swing = doc["arpSwing"];
            data->arpSettings.velocityLevel = doc["arpVelocity"];
          }

          for (uint8_t track = 0; track < numTracks; track++) {
            if (hasTrackInstruments) {
              data->trackSettings[track].device = doc["device"][track];
              data->trackSettings[track].instrument = doc["instrument"][track];
            }
            const uint8_t numLayersLoaded = doc["layer_count"][track];
            data->trackSettings[track].layerCount = std::min(numLayersLoaded, LiveSequencer::LIVESEQUENCER_NUM_LAYERS); // clamp for compatibility;
            data->trackSettings[track].quantizeDenom = doc["quant_denom"][track];
            data->trackSettings[track].quantizeGrid = LiveSequencer::mapQuantizeDenom(data->trackSettings[track].quantizeDenom);
            data->trackSettings[track].velocityLevel = doc["velocity"][track];
            data->trackSettings[track].songStartLayerMutes = doc["layer_mutes"][track];
            data->tracks[track].layerMutes = data->trackSettings[track].songStartLayerMutes;
          }

          numPatternEvents = doc["num_pattern_events"];
          lastSongPattern = doc["last_song_pattern"];
          data->songLayerCount = doc["song_layer_count"];
          for (int i = 0; i <= lastSongPattern; i++) {
            int num = doc["song_pattern_events"][i];
            numSongPatternEvents.push_back(num);
          }
          chunksize = doc["chunk_size"];
          numPatternChunks = ceil(numPatternEvents / float(chunksize)); // 50 events per file
        }

        if (numPatternChunks > 0) {
          // load pattern chunks
          for (int patternChunk = 0; patternChunk < numPatternChunks; patternChunk++) {
            snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s_pattern%03i.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME, patternChunk);
            //Serial.printf("load chunk %i from file %s...", c, filename);
            readChunk(filename, data->eventsList);
          }
        }

        for (uint8_t songPattern = 0; songPattern <= lastSongPattern; songPattern++) {
          // read numSongPatternEvents per song pattern
          const uint16_t numSongPatternChunks = ceil(numSongPatternEvents[songPattern] / float(chunksize)); // 50 events per file

          for (int chunkNumber = 0; chunkNumber < numSongPatternChunks; chunkNumber++) {
            snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s_song%03i_%03i.json"), PERFORMANCE_CONFIG_PATH, number, LIVESEQUENCER_CONFIG_NAME, songPattern, chunkNumber);
            readChunk(filename, data->songEvents[songPattern], true);
          }
        }
        data->currentBpm = seq.bpm;
        data->performanceID = number;
        data->songPatternCount = lastSongPattern;
        liveSeq.init();

        AudioInterrupts();
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  AudioInterrupts();
  return (true);
    }

FLASHMEM bool save_sd_gridsong_json(uint8_t number)
{
  if (sd_card_internal > 0) {
    LiveSequencer::LiveSeqData* data = liveSeq.getData();
    number = constrain(number, 0, 99);

    // Use the GRID_STEP macro for accessing grid data
#define GRID_STEP(step, track, layer) \
        data->gridSongSteps[(step) * (LiveSequencer::LIVESEQUENCER_NUM_TRACKS * LiveSequencer::LIVESEQUENCER_NUM_LAYERS) + \
                           (track) * LiveSequencer::LIVESEQUENCER_NUM_LAYERS + (layer)]

    for (uint8_t track = 0; track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS; track++) {
      // check if track is completely empty
      bool emptyTrack = true;
      for (uint8_t step = 0; step < LiveSequencer::LiveSeqData::GRIDSONGLEN && emptyTrack; step++) {
        for (uint8_t layer = 0; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS && emptyTrack; layer++) {
          if (GRID_STEP(step, track, layer) != 0) {
            emptyTrack = false;
          }
        }
      }

      if (emptyTrack) {
        // remove old file if it exists
        snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/gridsng%02d.json"),
          PERFORMANCE_CONFIG_PATH, number, track + 1);
        if (SD.exists(filename)) {
          SD.remove(filename);
        }
        continue;
      }

      // create JSON
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/gridsng%02d.json"),
        PERFORMANCE_CONFIG_PATH, number, track + 1);

#ifdef DEBUG
      LOG.print(F("Saving grid song track "));
      LOG.print(track + 1);
      LOG.print(F(" -> "));
      LOG.println(filename);
#endif

      SD.remove(filename);
      File json = SD.open(filename, FILE_WRITE);
      if (json) {
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        doc["track"] = track + 1;
        JsonArray stepsArr = doc.createNestedArray("steps");

        for (uint8_t step = 0; step < LiveSequencer::LiveSeqData::GRIDSONGLEN; step++) {
          JsonArray layersArr = stepsArr.createNestedArray();
          for (uint8_t layer = 0; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; layer++) {
            layersArr.add(GRID_STEP(step, track, layer));
          }
        }

        serializeJsonPretty(doc, json);
        json.close();
      }
    }

#undef GRID_STEP
  }
  return true;
}

FLASHMEM bool load_sd_gridsong_json(uint8_t number)
{
  // get pointer to sequencer data
  LiveSequencer::LiveSeqData* data = liveSeq.getData();

  // start with empty grid (zeros) - use memset for the linear array
  if (data->gridSongSteps) {
    memset(data->gridSongSteps, 0,
      LiveSequencer::LiveSeqData::GRIDSONGLEN *
      LiveSequencer::LIVESEQUENCER_NUM_TRACKS *
      LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
  }

  liveSeq.resetGridEditorDefaults();

  if (sd_card_internal > 0) {
    number = constrain(number, 0, 99);

    // Use the GRID_STEP macro for accessing grid data
#define GRID_STEP(step, track, layer) \
        data->gridSongSteps[(step) * (LiveSequencer::LIVESEQUENCER_NUM_TRACKS * LiveSequencer::LIVESEQUENCER_NUM_LAYERS) + \
                           (track) * LiveSequencer::LIVESEQUENCER_NUM_LAYERS + (layer)]

    for (uint8_t track = 0; track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS; track++) {
      snprintf_P(filename, sizeof(filename),
        PSTR("/%s/%d/gridsng%02d.json"),
        PERFORMANCE_CONFIG_PATH, number, track + 1);

      if (!SD.exists(filename)) {
        continue; // missing file → keep zeros
      }

#ifdef DEBUG
      LOG.print(F("Loading grid song track "));
      LOG.print(track + 1);
      LOG.print(F(" <- "));
      LOG.println(filename);
#endif

      File json = SD.open(filename, FILE_READ);
      if (json) {
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
        DeserializationError err = deserializeJson(doc, json);
        json.close();

        if (!err) {
          JsonArray stepsArr = doc["steps"];
          uint8_t stepIndex = 0;

          for (JsonArray layersArr : stepsArr) {
            if (stepIndex >= LiveSequencer::LiveSeqData::GRIDSONGLEN) break;

            uint8_t layerIndex = 0;
            for (JsonVariant val : layersArr) {
              if (layerIndex >= LiveSequencer::LIVESEQUENCER_NUM_LAYERS) break;

              GRID_STEP(stepIndex, track, layerIndex) = val.as<uint8_t>();
              layerIndex++;
            }
            stepIndex++;
          }
        }
      }
    }

#undef GRID_STEP
  }
  liveSeq.sanitizeGridSongSteps();
  return true;
}


/******************************************************************************
   SD SYS
 ******************************************************************************/


FLASHMEM bool load_sd_sys_json(void)
{

#ifdef RGB_ENCODERS
  configuration.sys.reverse_encoder_pins = true;
#endif

  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s.json"), SYS_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.println(F("Found sys configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        configuration.sys.vol = data_json["vol"];
        configuration.sys.mono = data_json["mono"];
        configuration.sys.dexed_engine_type = data_json["dexed_engine_type"];
        configuration.sys.soft_midi_thru = data_json["soft_midi_thru"];
        configuration.sys.performance_number = data_json["performance_number"];
        configuration.sys.favorites = data_json["favorites"];
        configuration.sys.load_at_startup_performance = data_json["load_at_startup_performance"];
        configuration.sys.load_at_startup_page = data_json["load_at_startup_page"];
        configuration.sys.screen_saver_start = data_json["screen_saver_start"];
        configuration.sys.screen_saver_mode = data_json["screen_saver_mode"];
        configuration.sys.boot_anim_skip = data_json["boot_anim_skip"];
        configuration.sys.reverse_encoder_pins = data_json["encoder_reversed"];
        configuration.sys.invert_colors = data_json["invert_colors"];
        configuration.sys.skip_midi_channel_warning = data_json["skip_midi_channel_warning"];
        configuration.sys.send_sysex_on_voice_change = data_json["send_sysex_on_voice_change"];
        configuration.sys.send_midi_cc = data_json["send_midi_cc"];
        configuration.sys.swap_midi_leds = data_json["swap_midi_leds"];

        configuration.sys.vol_control = data_json["vol_control"];
        configuration.sys.sus_pedal = data_json["sus_pedal"];
        configuration.sys.ext_clock = data_json["ext_clock"];
        configuration.sys.dac_cv_transpose = data_json["cvtranspose"];
        configuration.sys.dac_cv_2 = data_json["cv2cc"];
        configuration.sys.dac_cv_4 = data_json["cv4cc"];
        configuration.sys.cv_midi_channel = data_json["cv_midi_channel"];

        if (data_json["rgb_enc_color_def"] > 0)
          configuration.sys.rgb_enc_color_def = data_json["rgb_enc_color_def"];

        if (data_json["rgb_enc_color_sel"] > 0)
          configuration.sys.rgb_enc_color_sel = data_json["rgb_enc_color_sel"];

#ifdef RGB_ENCODERS
        configuration.sys.reverse_encoder_pins = true;
#endif

        if (data_json["brightness"] == 0)
          configuration.sys.brightness = 183;
        else
          configuration.sys.brightness = data_json["brightness"];

        if (data_json.containsKey("display_rotation"))
        {
          configuration.sys.display_rotation = data_json["display_rotation"];
          configuration.sys.touch_rotation = data_json["touch_rotation"];
        }
        if (data_json.containsKey("gp_speed"))
          configuration.sys.gamepad_speed = data_json["gp_speed"];
        if (data_json["gp_a"] != data_json["gp_b"])
        {
          GAMEPAD_UP_0 = data_json["gp_up_0"];
          GAMEPAD_UP_1 = data_json["gp_up_1"];
          GAMEPAD_DOWN_0 = data_json["gp_down_0"];
          GAMEPAD_DOWN_1 = data_json["gp_down_1"];
          GAMEPAD_LEFT_0 = data_json["gp_left_0"];
          GAMEPAD_LEFT_1 = data_json["gp_left_1"];
          GAMEPAD_RIGHT_0 = data_json["gp_right_0"];
          GAMEPAD_RIGHT_1 = data_json["gp_right_1"];
          GAMEPAD_SELECT = data_json["gp_select"];
          GAMEPAD_START = data_json["gp_start"];
          GAMEPAD_BUTTON_A = data_json["gp_a"];
          GAMEPAD_BUTTON_B = data_json["gp_b"];
        }

        check_configuration_sys();
        set_sys_params();

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }

  AudioInterrupts();
  return (false);
    }

FLASHMEM bool save_sd_sys_json(void)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s.json"), SYS_CONFIG_NAME);

#ifdef DEBUG
    LOG.print(F("Saving sys config to "));
    LOG.println(filename);
#endif

    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["vol"] = configuration.sys.vol;
      data_json["dexed_engine_type"] = configuration.sys.dexed_engine_type;
      data_json["mono"] = configuration.sys.mono;
      data_json["soft_midi_thru"] = configuration.sys.soft_midi_thru;
      data_json["performance_number"] = configuration.sys.performance_number;
      data_json["favorites"] = configuration.sys.favorites;
      data_json["load_at_startup_performance"] = configuration.sys.load_at_startup_performance;
      data_json["load_at_startup_page"] = configuration.sys.load_at_startup_page;
      data_json["display_rotation"] = configuration.sys.display_rotation;
      data_json["touch_rotation"] = configuration.sys.touch_rotation;
      data_json["encoder_reversed"] = configuration.sys.reverse_encoder_pins;
      data_json["boot_anim_skip"] = configuration.sys.boot_anim_skip;
      data_json["invert_colors"] = configuration.sys.invert_colors;
      data_json["screen_saver_start"] = configuration.sys.screen_saver_start;
      data_json["screen_saver_mode"] = configuration.sys.screen_saver_mode;
      data_json["brightness"] = configuration.sys.brightness;
      data_json["skip_midi_channel_warning"] = configuration.sys.skip_midi_channel_warning;
      data_json["send_sysex_on_voice_change"] = configuration.sys.send_sysex_on_voice_change;
      data_json["send_midi_cc"] = configuration.sys.send_midi_cc;
      data_json["swap_midi_leds"] = configuration.sys.swap_midi_leds;
      data_json["vol_control"] = configuration.sys.vol_control;
      data_json["sus_pedal"] = configuration.sys.sus_pedal;
      data_json["ext_clock"] = configuration.sys.ext_clock;

      data_json["cvtranspose"] = configuration.sys.dac_cv_transpose;
      data_json["cv2cc"] = configuration.sys.dac_cv_2;
      data_json["cv4cc"] = configuration.sys.dac_cv_4;
      data_json["cv_midi_channel"] = configuration.sys.cv_midi_channel;

      if (configuration.sys.rgb_enc_color_def != 0)
        data_json["rgb_enc_color_def"] = configuration.sys.rgb_enc_color_def;
      if (configuration.sys.rgb_enc_color_sel != 0)
        data_json["rgb_enc_color_sel"] = configuration.sys.rgb_enc_color_sel;

      data_json["gp_speed"] = configuration.sys.gamepad_speed;
      if (GAMEPAD_BUTTON_A != GAMEPAD_BUTTON_B)
      {
        data_json["gp_up_0"] = GAMEPAD_UP_0;
        data_json["gp_up_1"] = GAMEPAD_UP_1;
        data_json["gp_down_0"] = GAMEPAD_DOWN_0;
        data_json["gp_down_1"] = GAMEPAD_DOWN_1;
        data_json["gp_left_0"] = GAMEPAD_LEFT_0;
        data_json["gp_left_1"] = GAMEPAD_LEFT_1;
        data_json["gp_right_0"] = GAMEPAD_RIGHT_0;
        data_json["gp_right_1"] = GAMEPAD_RIGHT_1;
        data_json["gp_select"] = GAMEPAD_SELECT;
        data_json["gp_start"] = GAMEPAD_START;
        data_json["gp_a"] = GAMEPAD_BUTTON_A;
        data_json["gp_b"] = GAMEPAD_BUTTON_B;
      }

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  AudioInterrupts();
  return (false);
  }

/******************************************************************************
   SD NOISEMAKER
 ******************************************************************************/

#ifdef COMPILE_FOR_PSRAM

FLASHMEM void load_sd_noisemaker_json(void)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s.json"), CONFIG_NOISEMAKER);

    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();

        nm_params.category = data_json["category"];
        nm_params.mode = data_json["mode"];
        nm_params.randomizer = data_json["randomizer"];
        nm_params.stereo_mode = data_json["stereo_mode"];
        nm_params.stereo_width = data_json["stereo_width"];
        nm_params.reverb_enable = data_json["reverb_enable"];
        nm_params.reverb_room_size = data_json["reverb_room_size"];
        nm_params.reverb_damping = data_json["reverb_damping"];
        nm_params.reverb_wet = data_json["reverb_wet"];
        nm_params.reverb_dry = data_json["reverb_dry"];
        nm_params.reverb_width = data_json["reverb_width"];
        nm_params.accent = data_json["accent"];
        nm_params.bd_pitch = data_json["bd_pitch"];
        nm_params.bd_pitch_env = data_json["bd_pitch_env"];
        nm_params.bd_pitch_decay = data_json["bd_pitch_decay"];
        nm_params.bd_attack = data_json["bd_attack"];
        nm_params.bd_decay = data_json["bd_decay"];
        nm_params.bd_release = data_json["bd_release"];
        nm_params.bd_tone = data_json["bd_tone"];
        nm_params.bd_click = data_json["bd_click"];
        nm_params.bd_noise = data_json["bd_noise"];
        nm_params.bd_drive = data_json["bd_drive"];
        nm_params.bd_comp = data_json["bd_comp"];
        nm_params.bd_spread = data_json["bd_spread"];
        nm_params.sn_tone1 = data_json["sn_tone1"];
        nm_params.sn_tone2 = data_json["sn_tone2"];
        nm_params.sn_tone_mix = data_json["sn_tone_mix"];
        nm_params.sn_pitch_env = data_json["sn_pitch_env"];
        nm_params.sn_decay = data_json["sn_decay"];
        nm_params.sn_noise = data_json["sn_noise"];
        nm_params.sn_bp_freq = data_json["sn_bp_freq"];
        nm_params.sn_bp_q = data_json["sn_bp_q"];
        nm_params.sn_snap = data_json["sn_snap"];
        nm_params.sn_drive = data_json["sn_drive"];
        nm_params.sn_spread = data_json["sn_spread"];
        nm_params.tom_pitch = data_json["tom_pitch"];
        nm_params.tom_decay = data_json["tom_decay"];
        nm_params.tom_pitch_env = data_json["tom_pitch_env"];
        nm_params.tom_tone = data_json["tom_tone"];
        nm_params.tom_noise = data_json["tom_noise"];
        nm_params.tom_spread = data_json["tom_spread"];
        nm_params.conga_pitch = data_json["conga_pitch"];
        nm_params.conga_decay = data_json["conga_decay"];
        nm_params.conga_tone = data_json["conga_tone"];
        nm_params.conga_pitch_env = data_json["conga_pitch_env"];
        nm_params.conga_noise = data_json["conga_noise"];
        nm_params.conga_spread = data_json["conga_spread"];
        nm_params.rim_decay = data_json["rim_decay"];
        nm_params.rim_tone = data_json["rim_tone"];
        nm_params.rim_pitch = data_json["rim_pitch"];
        nm_params.rim_mod_freq = data_json["rim_mod_freq"];
        nm_params.rim_noise_bp_freq = data_json["rim_noise_bp_freq"];
        nm_params.rim_drive = data_json["rim_drive"];
        nm_params.rim_click_level = data_json["rim_click_level"];
        nm_params.rim_noise_level = data_json["rim_noise_level"];
        nm_params.rim_spread = data_json["rim_spread"];
        nm_params.clap_repeats = data_json["clap_repeats"];
        nm_params.clap_spread = data_json["clap_spread"];
        nm_params.clap_tail = data_json["clap_tail"];
        nm_params.clap_bp_hz = data_json["clap_bp_hz"];
        nm_params.clap_bp_q = data_json["clap_bp_q"];
        nm_params.clap_decay = data_json["clap_decay"];
        nm_params.clap_drive = data_json["clap_drive"];
        nm_params.hh_mode = data_json["hh_mode"];
        nm_params.hh_tone = data_json["hh_tone"];
        nm_params.hh_noise = data_json["hh_noise"];
        nm_params.hh_bp_hz = data_json["hh_bp_hz"];
        nm_params.hh_bp_q = data_json["hh_bp_q"];
        nm_params.hh_decay = data_json["hh_decay"];
        nm_params.hh_drive = data_json["hh_drive"];
        nm_params.hh_detune = data_json["hh_detune"];
        nm_params.hh_spread = data_json["hh_spread"];
        nm_params.crash_tone = data_json["crash_tone"];
        nm_params.crash_bp_hz = data_json["crash_bp_hz"];
        nm_params.crash_bp_q = data_json["crash_bp_q"];
        nm_params.crash_decay = data_json["crash_decay"];
        nm_params.crash_spread = data_json["crash_spread"];
        nm_params.ride_tone = data_json["ride_tone"];
        nm_params.ride_bp_hz = data_json["ride_bp_hz"];
        nm_params.ride_bp_q = data_json["ride_bp_q"];
        nm_params.ride_decay = data_json["ride_decay"];
        nm_params.ride_spread = data_json["ride_spread"];
        nm_params.cb_freq1 = data_json["cb_freq1"];
        nm_params.cb_freq2 = data_json["cb_freq2"];
        nm_params.cb_amp_attack = data_json["cb_amp_attack"];
        nm_params.cb_amp_decay = data_json["cb_amp_decay"];
        nm_params.cb_tone_balance = data_json["cb_tone_balance"];
        nm_params.cb_noise = data_json["cb_noise"];
        nm_params.cb_drive = data_json["cb_drive"];
        nm_params.cb_spread = data_json["cb_spread"];
        nm_params.claves_hz = data_json["claves_hz"];
        nm_params.claves_decay = data_json["claves_decay"];
        nm_params.claves_drive = data_json["claves_drive"];
        nm_params.claves_spread = data_json["claves_spread"];
        nm_params.mar_bp_hz = data_json["mar_bp_hz"];
        nm_params.mar_bp_q = data_json["mar_bp_q"];
        nm_params.mar_decay = data_json["mar_decay"];
        nm_params.mar_spread = data_json["mar_spread"];
        nm_params.zap_pitch_start = data_json["zap_pitch_start"];
        nm_params.zap_pitch_end = data_json["zap_pitch_end"];
        nm_params.zap_decay = data_json["zap_decay"];
        nm_params.zap_spread = data_json["zap_spread"];
        nm_params.snap_decay = data_json["snap_decay"];
        nm_params.snap_tone = data_json["snap_tone"];
        nm_params.snap_pitch = data_json["snap_pitch"];
        nm_params.snap_drive = data_json["snap_drive"];
        nm_params.snap_spread = data_json["snap_spread"];
        nm_params.synth_midi_note = data_json["synth_midi_note"];
        nm_params.synth_attack = data_json["synth_attack"];
        nm_params.synth_decay = data_json["synth_decay"];
        nm_params.synth_sustain_level = data_json["synth_sustain_level"];
        nm_params.synth_release = data_json["synth_release"];
        nm_params.synth_filter_cutoff = data_json["synth_filter_cutoff"];
        nm_params.synth_filter_resonance = data_json["synth_filter_resonance"];
        nm_params.synth_filter_env_amount = data_json["synth_filter_env_amount"];
        nm_params.synth_filter_env_attack = data_json["synth_filter_env_attack"];
        nm_params.synth_filter_env_decay = data_json["synth_filter_env_decay"];
        nm_params.synth_filter_env_release = data_json["synth_filter_env_release"];
        nm_params.synth_waveform_mix = data_json["synth_waveform_mix"];
        nm_params.synth_octave_mix = data_json["synth_octave_mix"];
        nm_params.synth_detune = data_json["synth_detune"];
        nm_params.synth_noise_level = data_json["synth_noise_level"];
        nm_params.synth_noise_decay = data_json["synth_noise_decay"];

        nm_params.synth_pulse_width = data_json["synth_pulse_width"];
        nm_params.synth_waveform = data_json["synth_waveform"];

      }
    }
  }
  AudioInterrupts();
}

FLASHMEM void save_sd_noisemaker_json(void)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s.json"), CONFIG_NOISEMAKER);

#ifdef DEBUG
    LOG.print(F("Saving sys config to "));
    LOG.println(filename);
#endif

    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      data_json["category"] = nm_params.category;
      data_json["mode"] = nm_params.mode;
      data_json["randomizer"] = nm_params.randomizer;
      data_json["stereo_mode"] = nm_params.stereo_mode;
      data_json["stereo_width"] = nm_params.stereo_width;
      data_json["reverb_enable"] = nm_params.reverb_enable;
      data_json["reverb_room_size"] = nm_params.reverb_room_size;
      data_json["reverb_damping"] = nm_params.reverb_damping;
      data_json["reverb_wet"] = nm_params.reverb_wet;
      data_json["reverb_dry"] = nm_params.reverb_dry;
      data_json["reverb_width"] = nm_params.reverb_width;
      data_json["accent"] = nm_params.accent;
      data_json["bd_pitch"] = nm_params.bd_pitch;
      data_json["bd_pitch_env"] = nm_params.bd_pitch_env;
      data_json["bd_pitch_decay"] = nm_params.bd_pitch_decay;
      data_json["bd_attack"] = nm_params.bd_attack;
      data_json["bd_decay"] = nm_params.bd_decay;
      data_json["bd_release"] = nm_params.bd_release;
      data_json["bd_tone"] = nm_params.bd_tone;
      data_json["bd_click"] = nm_params.bd_click;
      data_json["bd_noise"] = nm_params.bd_noise;
      data_json["bd_drive"] = nm_params.bd_drive;
      data_json["bd_comp"] = nm_params.bd_comp;
      data_json["bd_spread"] = nm_params.bd_spread;
      data_json["sn_tone1"] = nm_params.sn_tone1;
      data_json["sn_tone2"] = nm_params.sn_tone2;
      data_json["sn_tone_mix"] = nm_params.sn_tone_mix;
      data_json["sn_pitch_env"] = nm_params.sn_pitch_env;
      data_json["sn_decay"] = nm_params.sn_decay;
      data_json["sn_noise"] = nm_params.sn_noise;
      data_json["sn_bp_freq"] = nm_params.sn_bp_freq;
      data_json["sn_bp_q"] = nm_params.sn_bp_q;
      data_json["sn_snap"] = nm_params.sn_snap;
      data_json["sn_drive"] = nm_params.sn_drive;
      data_json["sn_spread"] = nm_params.sn_spread;
      data_json["tom_pitch"] = nm_params.tom_pitch;
      data_json["tom_decay"] = nm_params.tom_decay;
      data_json["tom_pitch_env"] = nm_params.tom_pitch_env;
      data_json["tom_tone"] = nm_params.tom_tone;
      data_json["tom_noise"] = nm_params.tom_noise;
      data_json["tom_spread"] = nm_params.tom_spread;
      data_json["conga_pitch"] = nm_params.conga_pitch;
      data_json["conga_decay"] = nm_params.conga_decay;
      data_json["conga_tone"] = nm_params.conga_tone;
      data_json["conga_pitch_env"] = nm_params.conga_pitch_env;
      data_json["conga_noise"] = nm_params.conga_noise;
      data_json["conga_spread"] = nm_params.conga_spread;
      data_json["rim_decay"] = nm_params.rim_decay;
      data_json["rim_tone"] = nm_params.rim_tone;
      data_json["rim_pitch"] = nm_params.rim_pitch;
      data_json["rim_mod_freq"] = nm_params.rim_mod_freq;
      data_json["rim_noise_bp_freq"] = nm_params.rim_noise_bp_freq;
      data_json["rim_drive"] = nm_params.rim_drive;
      data_json["rim_click_level"] = nm_params.rim_click_level;
      data_json["rim_noise_level"] = nm_params.rim_noise_level;
      data_json["rim_spread"] = nm_params.rim_spread;
      data_json["clap_repeats"] = nm_params.clap_repeats;
      data_json["clap_spread"] = nm_params.clap_spread;
      data_json["clap_tail"] = nm_params.clap_tail;
      data_json["clap_bp_hz"] = nm_params.clap_bp_hz;
      data_json["clap_bp_q"] = nm_params.clap_bp_q;
      data_json["clap_decay"] = nm_params.clap_decay;
      data_json["clap_drive"] = nm_params.clap_drive;
      data_json["hh_mode"] = nm_params.hh_mode;
      data_json["hh_tone"] = nm_params.hh_tone;
      data_json["hh_noise"] = nm_params.hh_noise;
      data_json["hh_bp_hz"] = nm_params.hh_bp_hz;
      data_json["hh_bp_q"] = nm_params.hh_bp_q;
      data_json["hh_decay"] = nm_params.hh_decay;
      data_json["hh_drive"] = nm_params.hh_drive;
      data_json["hh_detune"] = nm_params.hh_detune;
      data_json["hh_spread"] = nm_params.hh_spread;
      data_json["crash_tone"] = nm_params.crash_tone;
      data_json["crash_bp_hz"] = nm_params.crash_bp_hz;
      data_json["crash_bp_q"] = nm_params.crash_bp_q;
      data_json["crash_decay"] = nm_params.crash_decay;
      data_json["crash_spread"] = nm_params.crash_spread;
      data_json["ride_tone"] = nm_params.ride_tone;
      data_json["ride_bp_hz"] = nm_params.ride_bp_hz;
      data_json["ride_bp_q"] = nm_params.ride_bp_q;
      data_json["ride_decay"] = nm_params.ride_decay;
      data_json["ride_spread"] = nm_params.ride_spread;
      data_json["cb_freq1"] = nm_params.cb_freq1;
      data_json["cb_freq2"] = nm_params.cb_freq2;
      data_json["cb_amp_attack"] = nm_params.cb_amp_attack;
      data_json["cb_amp_decay"] = nm_params.cb_amp_decay;
      data_json["cb_tone_balance"] = nm_params.cb_tone_balance;
      data_json["cb_noise"] = nm_params.cb_noise;
      data_json["cb_drive"] = nm_params.cb_drive;
      data_json["cb_spread"] = nm_params.cb_spread;
      data_json["claves_hz"] = nm_params.claves_hz;
      data_json["claves_decay"] = nm_params.claves_decay;
      data_json["claves_drive"] = nm_params.claves_drive;
      data_json["claves_spread"] = nm_params.claves_spread;
      data_json["mar_bp_hz"] = nm_params.mar_bp_hz;
      data_json["mar_bp_q"] = nm_params.mar_bp_q;
      data_json["mar_decay"] = nm_params.mar_decay;
      data_json["mar_spread"] = nm_params.mar_spread;
      data_json["zap_pitch_start"] = nm_params.zap_pitch_start;
      data_json["zap_pitch_end"] = nm_params.zap_pitch_end;
      data_json["zap_decay"] = nm_params.zap_decay;
      data_json["zap_spread"] = nm_params.zap_spread;
      data_json["snap_decay"] = nm_params.snap_decay;
      data_json["snap_tone"] = nm_params.snap_tone;
      data_json["snap_pitch"] = nm_params.snap_pitch;
      data_json["snap_drive"] = nm_params.snap_drive;
      data_json["snap_spread"] = nm_params.snap_spread;
      data_json["synth_midi_note"] = nm_params.synth_midi_note;
      data_json["synth_attack"] = nm_params.synth_attack;
      data_json["synth_decay"] = nm_params.synth_decay;
      data_json["synth_sustain_level"] = nm_params.synth_sustain_level;
      data_json["synth_release"] = nm_params.synth_release;
      data_json["synth_filter_cutoff"] = nm_params.synth_filter_cutoff;
      data_json["synth_filter_resonance"] = nm_params.synth_filter_resonance;
      data_json["synth_filter_env_amount"] = nm_params.synth_filter_env_amount;
      data_json["synth_filter_env_attack"] = nm_params.synth_filter_env_attack;
      data_json["synth_filter_env_decay"] = nm_params.synth_filter_env_decay;
      data_json["synth_filter_env_release"] = nm_params.synth_filter_env_release;
      data_json["synth_waveform_mix"] = nm_params.synth_waveform_mix;
      data_json["synth_octave_mix"] = nm_params.synth_octave_mix;
      data_json["synth_detune"] = nm_params.synth_detune;
      data_json["synth_noise_level"] = nm_params.synth_noise_level;
      data_json["synth_noise_decay"] = nm_params.synth_noise_decay;
      data_json["synth_pulse_width"] = nm_params.synth_pulse_width;
      data_json["synth_waveform"] = nm_params.synth_waveform;

      serializeJsonPretty(data_json, json);
      json.close();
    }
    json.close();
  }
  AudioInterrupts();
}

#endif

/******************************************************************************
   SD BRAIDS
 ******************************************************************************/
extern uint8_t braids_presetno;

bool save_sd_braids_json(uint8_t number, uint8_t braidsmode);

FLASHMEM bool get_sd_braids_name(uint8_t number, uint8_t mode)
{

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/BRAIDS/pb%02d.json"), number);
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
        if (mode == 0)
        {
          if (check_sd_braids_patch_exists(number))
            strcpy(braids_osc.name, data_json["name"]);
          else
            strcpy(braids_osc.name, "-----EMPTY-----");
        }
        else if (mode == 1)
        {
          if (check_sd_braids_patch_exists(number))
            strcpy(braids_osc.temp_name, data_json["name"]);
          else
            strcpy(braids_osc.temp_name, "-----EMPTY-----");
        }
      }
      return (true);
    }
  }
  AudioInterrupts();
  return (false);
}

FLASHMEM bool load_sd_braids_json(uint8_t number, uint8_t braidsmode)
{
  // mode 0 = default/performance , mode 1 = individual braids patch/preset system in /BRAIDS/ folder
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    if (braidsmode == 0)
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, BRAIDS_CONFIG_NAME);
    else if (braidsmode == 1)
      snprintf_P(filename, sizeof(filename), PSTR("/BRAIDS/pb%02d.json"), number);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found braids configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        // serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        braids_osc.sound_intensity = data_json["vol"];
        braids_osc.algo = data_json["algo"];
        braids_osc.color = data_json["color"];
        braids_osc.timbre = data_json["timbre"];
        braids_osc.coarse = data_json["coarse"];
        braids_osc.env_attack = data_json["attack"];
        braids_osc.env_decay = data_json["decay"];
        braids_osc.env_sustain = data_json["sustain"];
        braids_osc.env_release = data_json["release"];
        braids_osc.filter_mode = data_json["filter_mode"];
        braids_osc.filter_freq_from = data_json["freq_from"];
        braids_osc.filter_freq_to = data_json["freq_to"];
        braids_osc.filter_resonance = data_json["res"];
        braids_osc.filter_speed = data_json["filter_speed"];
        braids_osc.rev_send = data_json["rev"];
        braids_osc.flanger = data_json["flanger"];
        braids_osc.flanger_spread = data_json["flanger_spread"];
        braids_osc.delay_send_1 = data_json["delay_1"];
        braids_osc.delay_send_2 = data_json["delay_2"];
        braids_osc.midi_channel = data_json["midi"];
        braids_osc.pan = data_json["pan"];
        braids_osc.sidechain_send = data_json["sidechain_send"];

        if (braidsmode == 0)
        {
          strcpy(braids_osc.name, "---FROM PERF---");
          braids_presetno = 0;
          save_sd_braids_json(0, 1);
        }
        else if (braidsmode == 1)
        {
          if (check_sd_braids_patch_exists(number))
            strcpy(braids_osc.name, data_json["name"]);
          else
            strcpy(braids_osc.name, "-----EMPTY-----");
        }
        braids_update_all_settings();
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }

  AudioInterrupts();
  return (false);
    }

FLASHMEM bool save_sd_braids_json(uint8_t number, uint8_t braidsmode)
{
  // mode 0 = default/performance , mode 1 = individual braids patch/preset system in /BRAIDS/ folder
  if (sd_card_internal > 0)
  {
    if (braidsmode == 0)
      snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, BRAIDS_CONFIG_NAME);
    else if (braidsmode == 1)
      snprintf_P(filename, sizeof(filename), PSTR("/BRAIDS/pb%02d.json"), number);

#ifdef DEBUG
    LOG.print(F("Saving braids"));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["vol"] = braids_osc.sound_intensity;
      data_json["algo"] = braids_osc.algo;
      data_json["color"] = braids_osc.color;
      data_json["timbre"] = braids_osc.timbre;
      data_json["coarse"] = braids_osc.coarse;
      data_json["attack"] = braids_osc.env_attack;
      data_json["decay"] = braids_osc.env_decay;
      data_json["sustain"] = braids_osc.env_sustain;
      data_json["release"] = braids_osc.env_release;
      data_json["filter_mode"] = braids_osc.filter_mode;
      data_json["freq_from"] = braids_osc.filter_freq_from;
      data_json["freq_to"] = braids_osc.filter_freq_to;
      data_json["res"] = braids_osc.filter_resonance;
      data_json["filter_speed"] = braids_osc.filter_speed;
      data_json["rev"] = braids_osc.rev_send;
      data_json["flanger"] = braids_osc.flanger;
      data_json["flanger_spread"] = braids_osc.flanger_spread;
      data_json["delay_1"] = braids_osc.delay_send_1;
      data_json["delay_2"] = braids_osc.delay_send_2;
      data_json["midi"] = braids_osc.midi_channel;
      data_json["pan"] = braids_osc.pan;
      data_json["sidechain_send"] = braids_osc.sidechain_send;

      if (braidsmode == 0)
        data_json["name"] = "-FROM PERF-";
      else if (braidsmode == 1)
        data_json["name"] = braids_osc.name;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  AudioInterrupts();
  return (false);
  }

/******************************************************************************
   SD MULTIBAND
 ******************************************************************************/
#include "MultiBandCompressor.h"
extern MultiBandCompressor_UI_t MultiBandUIparams;
extern AudioMultiBandCompressor MultiBandCompressor;

FLASHMEM bool load_sd_multiband_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, MULTIBAND_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found multiband configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        // serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        MultiBandUIparams.master_gain = data_json["master_gain"];
        MultiBandUIparams.master_mix = data_json["master_mix"];
        MultiBandUIparams.crossover1 = data_json["crossover1"];
        MultiBandUIparams.crossover2 = data_json["crossover2"];
        MultiBandUIparams.crossover3 = data_json["crossover3"];
        MultiBandUIparams.band1_thresh = data_json["band1_thresh"];
        MultiBandUIparams.band1_ratio = data_json["band1_ratio"];
        MultiBandUIparams.band1_attack = data_json["band1_attack"];
        MultiBandUIparams.band1_release = data_json["band1_release"];
        MultiBandUIparams.band1_gain = data_json["band1_gain"];
        MultiBandUIparams.band2_thresh = data_json["band2_thresh"];
        MultiBandUIparams.band2_ratio = data_json["band2_ratio"];
        MultiBandUIparams.band2_attack = data_json["band2_attack"];
        MultiBandUIparams.band2_release = data_json["band2_release"];
        MultiBandUIparams.band2_gain = data_json["band2_gain"];
        MultiBandUIparams.band3_thresh = data_json["band3_thresh"];
        MultiBandUIparams.band3_ratio = data_json["band3_ratio"];
        MultiBandUIparams.band3_attack = data_json["band3_attack"];
        MultiBandUIparams.band3_release = data_json["band3_release"];
        MultiBandUIparams.band3_gain = data_json["band3_gain"];
        MultiBandUIparams.band4_thresh = data_json["band4_thresh"];
        MultiBandUIparams.band4_ratio = data_json["band4_ratio"];
        MultiBandUIparams.band4_attack = data_json["band4_attack"];
        MultiBandUIparams.band4_release = data_json["band4_release"];
        MultiBandUIparams.band4_gain = data_json["band4_gain"];

        MultiBandCompressor.setUI(MultiBandUIparams);

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }

  AudioInterrupts();
  return (false);
    }

FLASHMEM bool save_sd_multiband_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, MULTIBAND_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving multiband"));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      data_json["master_gain"] = MultiBandUIparams.master_gain;
      data_json["master_mix"] = MultiBandUIparams.master_mix;
      data_json["crossover1"] = MultiBandUIparams.crossover1;
      data_json["crossover2"] = MultiBandUIparams.crossover2;
      data_json["crossover3"] = MultiBandUIparams.crossover3;
      data_json["band1_thresh"] = MultiBandUIparams.band1_thresh;
      data_json["band1_ratio"] = MultiBandUIparams.band1_ratio;
      data_json["band1_attack"] = MultiBandUIparams.band1_attack;
      data_json["band1_release"] = MultiBandUIparams.band1_release;
      data_json["band1_gain"] = MultiBandUIparams.band1_gain;
      data_json["band2_thresh"] = MultiBandUIparams.band2_thresh;
      data_json["band2_ratio"] = MultiBandUIparams.band2_ratio;
      data_json["band2_attack"] = MultiBandUIparams.band2_attack;
      data_json["band2_release"] = MultiBandUIparams.band2_release;
      data_json["band2_gain"] = MultiBandUIparams.band2_gain;
      data_json["band3_thresh"] = MultiBandUIparams.band3_thresh;
      data_json["band3_ratio"] = MultiBandUIparams.band3_ratio;
      data_json["band3_attack"] = MultiBandUIparams.band3_attack;
      data_json["band3_release"] = MultiBandUIparams.band3_release;
      data_json["band3_gain"] = MultiBandUIparams.band3_gain;
      data_json["band4_thresh"] = MultiBandUIparams.band4_thresh;
      data_json["band4_ratio"] = MultiBandUIparams.band4_ratio;
      data_json["band4_attack"] = MultiBandUIparams.band4_attack;
      data_json["band4_release"] = MultiBandUIparams.band4_release;
      data_json["band4_gain"] = MultiBandUIparams.band4_gain;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  AudioInterrupts();
  return (false);
  }


/******************************************************************************
   SD CUSTOM SAMPLES
 ******************************************************************************/

#ifdef COMPILE_FOR_PSRAM
FLASHMEM bool load_sd_samples_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SAMPLES_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found custom samples configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        // serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        unload_all_custom_samples();

        for (int i = 0; i < NUM_CUSTOM_SAMPLES; i++) {
          strcpy(drum_config[i + NUM_STATIC_PITCHED_SAMPLES].name, data_json[i]["name"]);

          char fullpath[FULLPATH_MAX_LEN] = { 0 };
          strcpy(fullpath, data_json[i]["filename"]);
          char* lastSlash = strrchr(fullpath, '/');
          if (lastSlash != nullptr) {
            strcpy(drum_config[i + NUM_STATIC_PITCHED_SAMPLES].filename, lastSlash + 1);
            *lastSlash = '\0';
            strcpy(customSamples[i].filepath, fullpath);
          }
          if (data_json[i].containsKey("start")) {
            customSamples[i].start = data_json[i]["start"];
            customSamples[i].end = data_json[i]["end"];
            customSamples[i].loopType = data_json[i]["loop"];
          }
          else {
            customSamples[i].start = 0;    // 0%
            customSamples[i].end = 1000;   // 100%
            customSamples[i].loopType = 0; // loop_type::looptype_none
          }

          uint8_t drumClass = data_json[i]["class"];
          if (drumClass == 0) {
            drumClass = DRUM_POLY;
          }
          drum_config[i + NUM_STATIC_PITCHED_SAMPLES].drum_class = drumClass;
        }
        load_custom_samples_to_psram();

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }

  AudioInterrupts();
  return (false);
    }

// FLASHMEM bool save_sd_samples_json(uint8_t number)
// {
//   if (sd_card_internal > 0)
//   {
//     snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SAMPLES_CONFIG_NAME);
// #ifdef DEBUG
//     LOG.print(F("Saving custom samples"));
//     LOG.print(number);
//     LOG.print(F(" to "));
//     LOG.println(filename);
// #endif

//     AudioNoInterrupts();
//     SD.remove(filename);
//     json = SD.open(filename, FILE_WRITE);
//     if (json)
//     {
//       StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

//       for (int i = 0; i < NUM_CUSTOM_SAMPLES; i++) {
//         data_json[i]["name"] = drum_config[i + NUM_STATIC_PITCHED_SAMPLES].name;
//         String fullpath = customSamples[i].filepath;
//         fullpath += "/";
//         fullpath += drum_config[i + NUM_STATIC_PITCHED_SAMPLES].filename;
//         data_json[i]["filename"] = fullpath;
//         data_json[i]["start"] = customSamples[i].start;
//         data_json[i]["end"] = customSamples[i].end;
//         data_json[i]["loop"] = customSamples[i].loopType;
//         data_json[i]["class"] = drum_config[i + NUM_STATIC_PITCHED_SAMPLES].drum_class;
//       }

// #if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
//       LOG.println(F("Write JSON data:"));
//       serializeJsonPretty(data_json, Serial);
//       LOG.println();
// #endif
//       serializeJsonPretty(data_json, json);
//       json.close();
//       AudioInterrupts();
//       return (true);
//     }
//     json.close();
//   }
//   else
//   {
// #ifdef DEBUG
//     LOG.print(F("E : Cannot open "));
//     LOG.print(filename);
//     LOG.println(F(" on SD."));
// #endif
//   }
//   AudioInterrupts();
//   return (false);
// }

FLASHMEM bool save_sd_samples_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SAMPLES_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving custom samples"));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      int json_index = 0; // Separate index for JSON array
      for (int i = 0; i < NUM_CUSTOM_SAMPLES; i++) {
        // Create the full path safely using character arrays
        char fullpath[FULLPATH_MAX_LEN];

        // Check if the combined path will fit in the buffer
        const char* filepath = customSamples[i].filepath;
        const char* sample_filename = drum_config[i + NUM_STATIC_PITCHED_SAMPLES].filename;

        // Safe length check - use strnlen with max length
        size_t filepath_len = strnlen(filepath, FULLPATH_MAX_LEN);
        size_t filename_len = strnlen(sample_filename, FULLPATH_MAX_LEN);

        // Total needed: filepath + '/' + filename + null terminator
        if (filepath_len + filename_len + 2 > FULLPATH_MAX_LEN) {
          // Path too long, skip this entire entry
#ifdef DEBUG
          LOG.print(F("Warning: Path too long for sample "));
          LOG.print(i);
          LOG.print(F(", skipping entry: "));
          LOG.print(filepath);
          LOG.print(F("/"));
          LOG.println(sample_filename);
#endif
          continue; // Skip to next iteration
        }

        // Path fits, create full path safely
        // Build path manually to avoid snprintf warning
        char* dest = fullpath;

        // Copy filepath
        memcpy(dest, filepath, filepath_len);
        dest += filepath_len;

        // Add separator
        *dest = '/';
        dest++;

        // Copy filename
        memcpy(dest, sample_filename, filename_len);
        dest += filename_len;

        // Add null terminator
        *dest = '\0';

        // Only add entry if path was valid
        data_json[json_index]["name"] = drum_config[i + NUM_STATIC_PITCHED_SAMPLES].name;
        data_json[json_index]["filename"] = fullpath;
        data_json[json_index]["start"] = customSamples[i].start;
        data_json[json_index]["end"] = customSamples[i].end;
        data_json[json_index]["loop"] = customSamples[i].loopType;
        data_json[json_index]["class"] = drum_config[i + NUM_STATIC_PITCHED_SAMPLES].drum_class;

        json_index++; // Only increment when we add an entry
      }

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  AudioInterrupts();
  return (false);
  }

#endif


/******************************************************************************
   SLICES
 ******************************************************************************/

#ifdef COMPILE_FOR_PSRAM

extern uint8_t num_slices[2];
extern uint8_t selected_slice_sample[2];
extern uint32_t slice_start[2][16];
extern uint32_t slice_end[2][16];
extern uint16_t slices_scrollspeed;
extern bool slices_autoalign;

FLASHMEM bool load_sd_slices_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SLICES_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load

      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

        slices_scrollspeed = data_json["slices_scrollspeed"];
        slices_autoalign = data_json["slices_autoalign"];

        for (int j = 0; j < 2; j++)
        {
          num_slices[j] = data_json["num_slices"][j];
          selected_slice_sample[j] = data_json["selected_slice_sample"][j];

          for (int i = 0; i < 16; i++)
          {
            slice_start[j][i] = data_json["start"][j][i];
            slice_end[j][i] = data_json["end"][j][i];
          }
        }

        return (true);
      }
    }
  }
  AudioInterrupts();
  return (false);
}

FLASHMEM bool save_sd_slices_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SLICES_CONFIG_NAME);

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      data_json["slices_scrollspeed"] = slices_scrollspeed;
      data_json["slices_autoalign"] = slices_autoalign;

      for (int j = 0; j < 2; j++)
      {
        data_json["num_slices"][j] = num_slices[j];
        data_json["selected_slice_sample"][j] = selected_slice_sample[j];

        for (int i = 0; i < 16; i++)
        {
          data_json["start"][j][i] = slice_start[j][i];
          data_json["end"][j][i] = slice_end[j][i];
        }
      }

      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  AudioInterrupts();
  return (false);
}

#endif


/******************************************************************************
   SD SIDECHAIN
 ******************************************************************************/

FLASHMEM bool load_sd_sidechain_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SIDECHAIN_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found sidechain configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        // serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        sidechain_active = data_json["active"];
        sidechain_sample_number = data_json["sample_number"];
        if (data_json["speed"] > 0)
          sidechain_speed = data_json["speed"];
        if (data_json["steps"] > 0)
          sidechain_steps = data_json["steps"];

        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }

  AudioInterrupts();
  return (false);
    }

FLASHMEM bool save_sd_sidechain_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SIDECHAIN_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving sidechain"));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      data_json["active"] = sidechain_active;

      data_json["sample_number"] = sidechain_sample_number;

      data_json["speed"] = sidechain_speed;
      data_json["steps"] = sidechain_steps;


#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }

  AudioInterrupts();
  return (false);
  }

/******************************************************************************
   SD GRANULAR
 ******************************************************************************/
#ifdef GRANULAR
#include "granular.h"
extern void updateGranularParams();
extern granular_params_t granular_params;

FLASHMEM bool load_sd_granular_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, "granular");

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found granular configuration"));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

        // Load granular parameters

        granular_params.sample_note = data_json["sample_note"];
        granular_params.attack = data_json["attack"];
        granular_params.release = data_json["release"];
        granular_params.grain_size = data_json["grain_size"];
        granular_params.grain_position = data_json["grain_position"];
        granular_params.play_mode = data_json["play_mode"];
        granular_params.semitones = data_json["semitones"];
        granular_params.volume = data_json["volume"];
        granular_params.density = data_json["density"];
        granular_params.spread = data_json["spread"];
        granular_params.filter_mode = data_json["filter_mode"];
        granular_params.rev_send = data_json["rev_send"];
        granular_params.filter_freq = data_json["filter_freq"] | 0;
        granular_params.filter_resonance = data_json["filter_resonance"];
        granular_params.delay_send_1 = data_json["delay_send_1"];
        granular_params.delay_send_2 = data_json["delay_send_2"];
        granular_params.pan = data_json["pan"];
        granular_params.midi_channel = data_json["midi_channel"] | 11;

        if (granular_params.midi_channel == 0)
          granular_params.midi_channel = DEFAULT_GRANULAR_MIDI_CHANNEL;

        updateGranularParams();
        return (true);
      }
    }

  }
  // Update the granular synth with loaded parameters
  if (granular_params.midi_channel == 0)
    granular_params.midi_channel = DEFAULT_GRANULAR_MIDI_CHANNEL;
  updateGranularParams();
  AudioInterrupts();
  return (false);
}

FLASHMEM bool save_sd_granular_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, "granular");

    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      // Save granular parameters

      data_json["sample_note"] = granular_params.sample_note;
      data_json["grain_size"] = granular_params.grain_size;
      data_json["grain_position"] = granular_params.grain_position;
      data_json["play_mode"] = granular_params.play_mode;
      data_json["semitones"] = granular_params.semitones;
      data_json["volume"] = granular_params.volume;
      data_json["density"] = granular_params.density;
      data_json["spread"] = granular_params.spread;
      data_json["attack"] = granular_params.attack;
      data_json["release"] = granular_params.release;
      data_json["filter_mode"] = granular_params.filter_mode;
      data_json["rev_send"] = granular_params.rev_send;
      data_json["filter_freq"] = granular_params.filter_freq;
      data_json["filter_resonance"] = granular_params.filter_resonance;
      data_json["delay_send_1"] = granular_params.delay_send_1;
      data_json["delay_send_2"] = granular_params.delay_send_2;
      data_json["pan"] = granular_params.pan;
      data_json["midi_channel"] = granular_params.midi_channel;

      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
    ;
  }

  AudioInterrupts();
  return (false);
}
#endif

/******************************************************************************
   SD SEQUENCER
 ******************************************************************************/

FLASHMEM bool load_sd_chain_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, CHAIN_CONFIG_NAME);
    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
        int total = sizeof(seq.chain);
        int columns = sizeof(seq.chain[0]);
        int rows = total / columns;
        int count = 0;
        for (uint8_t i = 0; i < rows; i++)
        {
          for (uint8_t j = 0; j < columns; j++)
          {
            seq.chain[i][j] = data_json["c"][count];
            count++;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E: Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool save_sd_chain_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, CHAIN_CONFIG_NAME);

    int count = 0;
    int total = sizeof(seq.chain);
    int columns = sizeof(seq.chain[0]);
    int rows = total / columns;
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < rows; i++)
      {
        for (uint8_t j = 0; j < columns; j++)
        {
          data_json["c"][count] = seq.chain[i][j];
          count++;
        }
      }
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

FLASHMEM bool load_sd_transpose_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, TRANSPOSE_CONFIG_NAME);
    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
        int total = sizeof(seq.chain_transpose);
        int columns = sizeof(seq.chain_transpose[0]);
        int rows = total / columns;
        int count = 0;
        for (uint8_t i = 0; i < rows; i++)
        {
          for (uint8_t j = 0; j < columns; j++)
          {
            seq.chain_transpose[i][j] = data_json["t"][count];
            count++;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E: Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool save_sd_transpose_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, TRANSPOSE_CONFIG_NAME);

    int count = 0;
    int total = sizeof(seq.chain_transpose);
    int columns = sizeof(seq.chain_transpose[0]);
    int rows = total / columns;
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < rows; i++)
      {
        for (uint8_t j = 0; j < columns; j++)
        {
          data_json["t"][count] = seq.chain_transpose[i][j];
          count++;
        }
      }
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

FLASHMEM bool load_sd_song_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SONG_CONFIG_NAME);
    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();
        int total = sizeof(seq.song);
        int columns = sizeof(seq.song[0]);
        int rows = total / columns;
        int count = 0;
        for (uint8_t i = 0; i < rows; i++)
        {
          for (uint8_t j = 0; j < columns; j++)
          {
            //  if (i<6)
            seq.song[i][j] = data_json["s"][count];
            //    else
            //    seq.song[i][j] = 99;
            count++;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E: Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool save_sd_song_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SONG_CONFIG_NAME);

    int count = 0;
    int total = sizeof(seq.song);
    int columns = sizeof(seq.song[0]);
    int rows = total / columns;
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < rows; i++)
      {
        for (uint8_t j = 0; j < columns; j++)
        {
          data_json["s"][count] = seq.song[i][j];
          count++;
        }
      }
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

FLASHMEM bool save_sd_seq_sub_vel_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, VELOCITY_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving sequencer velocity "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif
    int count = 0;
    int total = sizeof(seq.vel);
    int columns = sizeof(seq.vel[0]);
    int rows = total / columns;
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < rows; i++)
      {
        for (uint8_t j = 0; j < columns; j++)
        {
          data_json["seq_velocity"][count] = seq.vel[i][j];
          count++;
        }
      }

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

FLASHMEM bool save_sd_seq_sub_patterns_json(uint8_t number)
{
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, PATTERN_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving sequencer patterns "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif
    int count = 0;
    int total = sizeof(seq.note_data);
    int columns = sizeof(seq.note_data[0]);
    int rows = total / columns;
    AudioNoInterrupts();
    SD.remove(filename);
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      for (uint8_t i = 0; i < rows; i++)
      {
        for (uint8_t j = 0; j < columns; j++)
        {
          data_json["seq_data"][count] = seq.note_data[i][j];
          count++;
        }
      }

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    json.close();
  }
  else
  {
#ifdef DEBUG
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
#endif
  }
  return (false);
  }

FLASHMEM bool save_sd_performance_json(uint8_t number)
{
  bool seq_was_running = false;
  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);

  if (seq.running == true)
  {
    seq_was_running = true;
    handleStop();
  }
  dac_mute();

  AudioNoInterrupts();

  check_performance_directory(number);

#ifdef DEBUG
  LOG.print(F("Write performance config "));
  LOG.println(number);
#endif

  save_sd_seq_sub_vel_json(number);
  save_sd_seq_sub_patterns_json(number);
  save_sd_drummappings_json(number);
  save_sd_song_json(number);
  save_sd_transpose_json(number);
  save_sd_chain_json(number);
  save_sd_fx_json(number);
  save_sd_epiano_json(number);
#if defined COMPILE_FOR_PSRAM
  save_sd_multisample_presets_json(number);
#endif

  save_sd_braids_json(number, 0);
  save_sd_multiband_json(number);
  save_sd_sidechain_json(number);
  save_sd_livesequencer_json(number);
  save_sd_gridsong_json(number);
  save_sd_envelopes_json(number);
#ifdef COMPILE_FOR_PSRAM
  save_sd_samples_json(number);
  save_sd_slices_json(number);
#endif

#ifdef GRANULAR
  save_sd_granular_json(number);
#endif

  for (uint8_t i = 0; i < NUM_DEXED; i++)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s%d.json"), PERFORMANCE_CONFIG_PATH, number, VOICE_CONFIG_NAME, i);
#ifdef DEBUG
    LOG.print(F("Write Voice-Config for sequencer"));
    LOG.println(filename);
#endif
    if (i < 2)  //only 2 instances of microsynth
      save_sd_microsynth_json(number, i);

    save_sd_voiceconfig_json(number, i);
  }
  if (sd_card_internal > 0)
  {
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SEQUENCER_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving sequencer config "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif

    LOG.print(F("  "));
    SD.remove(filename);
    LiveSequencer::LiveSeqData* data = liveSeq.getData();
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      data_json["seq_tempo_ms"] = seq.tempo_ms;
      data_json["pattern_len_dec"] = seq.pattern_len_dec;
      data_json["clock"] = seq.clock;
      data_json["seq_bpm"] = seq.bpm;
      data_json["arp_speed"] = seq.arp_speed;
      data_json["arp_length"] = seq.arp_length;
      data_json["arp_volume_fade"] = seq.arp_volume_fade;
      data_json["arp_style"] = seq.arp_style;
      data_json["arp_num_notes_max"] = seq.arp_num_notes_max;
      data_json["seq_chord_vel"] = seq.chord_vel;
      data_json["arr_split_note"] = seq.arr_split_note;
      data_json["chord_key_ammount"] = seq.chord_key_ammount;
      data_json["seq_oct_shift"] = seq.oct_shift;
      data_json["seq_element_shift"] = seq.element_shift;
      data_json["euclidean_active"] = seq.euclidean_active;
      data_json["euclidean_offset"] = seq.euclidean_offset;
      data_json["hihat_randomizer"] = seq.hihat_randomizer;
      data_json["grid_song_mode"] = data->LiveSequencer::LiveSeqData::useGridSongMode;

      if (data->LiveSequencer::LiveSeqData::songLayerCount > 0)
        data_json["isSongMode"] = data->LiveSequencer::LiveSeqData::isSongMode;


      for (uint8_t track = 0; track < NUM_SEQ_TRACKS; track++)
      {
        data_json["arr_type"][track] = seq.arr_type[track];
      }

      for (uint8_t i = 0; i < sizeof(seq.track_type); i++)
      {
        data_json["track_type"][i] = seq.track_type[i];
      }
      for (uint8_t i = 0; i < sizeof(seq.content_type); i++)
      {
        data_json["content_type"][i] = seq.content_type[i];
      }
      for (uint8_t i = 0; i < sizeof(seq.instrument); i++)
      {
        data_json["seq_inst_dexed"][i] = seq.instrument[i];
      }
      for (uint8_t i = 0; i < FILENAME_LEN; i++)
      {
        data_json["seq_name"][i] = seq.name[i];
      }
      for (uint8_t pat = 0; pat < NUM_SEQ_PATTERN; pat++)
      {
        data_json["chance"][pat] = seq.pat_chance[pat];
        data_json["vel_variation"][pat] = seq.pat_vel_variation[pat];
      }
      data_json["drum_midi_channel"] = drum_midi_channel;
      data_json["slices_midi_channel"] = slices_midi_channel;

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      dac_unmute();
      if (seq_was_running == true)
        handleStart();
      return (true);
    }
    // json.close();
    // AudioInterrupts();
  }
#ifdef DEBUG
  else
  {
    LOG.print(F("E : Cannot open "));
    LOG.print(filename);
    LOG.println(F(" on SD."));
    AudioInterrupts();
  }
#endif

  return (false);
  }

FLASHMEM bool check_performance_directory(uint8_t number)
{
  char dir[CONFIG_FILENAME_LEN];

  if (sd_card_internal > 0)
  {
    snprintf_P(dir, sizeof(dir), PSTR("/%s/%d"), PERFORMANCE_CONFIG_PATH, number);

    AudioNoInterrupts();
    if (!SD.exists(dir))
    {
#ifdef DEBUG
      if (SD.mkdir(dir))
      {
        LOG.print(F("Creating directory "));
        LOG.println(dir);
      }
      else
      {
        LOG.print(F("E: Cannot create "));
        LOG.println(dir);
        AudioInterrupts();
        return (false);
      }
#else
      SD.mkdir(dir);
#endif
      }
    AudioInterrupts();
    return (true);
    }
#ifdef DEBUG
  else
  {
    LOG.println(F("E: SD card not available"));
  }
#endif
  return (false);
  }

FLASHMEM void get_sd_performance_name_json(uint8_t number)
{
  memset(seq.name_temp, 0, FILENAME_LEN);
  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SEQUENCER_CONFIG_NAME);

    // first check if file exists...
    // AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load

      json = SD.open(filename);
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
      if (json)
      {
        deserializeJson(data_json, json);
        json.close();
        // AudioInterrupts();
      }
      if (data_json["seq_name"][0] != 0)
      {
        for (uint8_t i = 0; i < FILENAME_LEN; i++)
        {
          seq.name_temp[i] = data_json["seq_name"][i];
        }
#ifdef DEBUG
        LOG.print(F("Get performance name for "));
        LOG.print(number);
        LOG.print(F(": "));
        LOG.print(seq.name_temp);
        LOG.println();
#endif
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("Cannot get performance name for "));
        LOG.print(number);
        LOG.println();
      }
#endif
    }
      }
      }

FLASHMEM bool load_sd_seq_sub_vel_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, VELOCITY_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found velocity data ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
      LOG.println(F(" "));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        int total = sizeof(seq.vel);
        int columns = sizeof(seq.vel[0]);
        int rows = total / columns;
        int count = 0;
        for (uint8_t i = 0; i < rows; i++)
        {
          for (uint8_t j = 0; j < columns; j++)
          {
            seq.vel[i][j] = data_json["seq_velocity"][count];
            count++;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E: Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool load_sd_seq_sub_patterns_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, PATTERN_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found pattern data ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
      LOG.println(F(" "));
#endif
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);

        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        int total = sizeof(seq.note_data);
        int columns = sizeof(seq.note_data[0]);
        int rows = total / columns;
        int count = 0;

        for (uint8_t i = 0; i < rows; i++)
        {
          for (uint8_t j = 0; j < columns; j++)
          {
            seq.note_data[i][j] = data_json["seq_data"][count];
            count++;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

extern void update_seq_speed();
extern uint8_t perf_load_options;
extern void set_seq_timing();

FLASHMEM bool load_sd_performance_json(uint8_t number)
{
  bool seq_was_running = false;
  if (seq.running)
  {
    seq_was_running = true;
    handleStop();
  }
  dac_mute();
  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
  AudioNoInterrupts();

  if (bitRead(perf_load_options, perf_seq))
  {
    load_sd_seq_sub_patterns_json(number);
    load_sd_seq_sub_vel_json(number);
    load_sd_song_json(number);
    load_sd_transpose_json(number);
    load_sd_chain_json(number);
  }

  if (bitRead(perf_load_options, perf_epiano))
  {
    load_sd_epiano_json(number);
  }

  load_sd_drummappings_json(number);

#if defined COMPILE_FOR_PSRAM
  if (bitRead(perf_load_options, perf_samples))
    load_sd_multisample_presets_json(number);
#endif

  if (bitRead(perf_load_options, perf_braids))
    load_sd_braids_json(number, 0);

  if (bitRead(perf_load_options, perf_effects))
  {
    load_sd_multiband_json(number);
    load_sd_sidechain_json(number);
  }

  load_sd_envelopes_json(number);

#ifdef COMPILE_FOR_PSRAM
  load_sd_samples_json(number);
  load_sd_slices_json(number);
#endif

#ifdef GRANULAR
  load_sd_granular_json(number);
#endif

  configuration.sys.performance_number = number;

  if (sd_card_internal > 0)
  {
    //snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SEQUENCER_CONFIG_NAME);
    sprintf(filename, "/%s/%d/%s.json", PERFORMANCE_CONFIG_PATH, number, SEQUENCER_CONFIG_NAME);
    // first check if file exists...
    if (SD.exists(filename))
    {
      // ... and if: load
#ifdef DEBUG
      LOG.print(F("Found Performance configuration ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
#endif

      LiveSequencer::LiveSeqData* data = liveSeq.getData();
      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif

        if (bitRead(perf_load_options, perf_seq))
        {
          for (uint8_t i = 0; i < sizeof(seq.track_type); i++)
          {
            seq.track_type[i] = data_json["track_type"][i];
          }
          for (uint8_t i = 0; i < sizeof(seq.content_type); i++)
          {
            seq.content_type[i] = data_json["content_type"][i];
          }
          for (uint8_t i = 0; i < sizeof(seq.instrument); i++)
          {
            seq.instrument[i] = data_json["seq_inst_dexed"][i];
          }

          if (data_json["seq_name"][0] != 0)
          {
            for (uint8_t i = 0; i < FILENAME_LEN; i++)
            {
              seq.name[i] = data_json["seq_name"][i];
            }
          }

          for (uint8_t pat = 0; pat < NUM_SEQ_PATTERN; pat++)
          {
            if (data_json["chance"][pat] > 0)
            {
              seq.pat_chance[pat] = data_json["chance"][pat];
              seq.pat_vel_variation[pat] = data_json["vel_variation"][pat];
            }
            else
            {
              seq.pat_chance[pat] = 100;
              seq.pat_vel_variation[pat] = 0;
            }
          }


          for (uint8_t track = 0; track < NUM_SEQ_TRACKS; track++)
          {
            seq.arr_type[track] = data_json["arr_type"][track];
          }

          seq.tempo_ms = data_json["seq_tempo_ms"];
          seq.bpm = data_json["seq_bpm"];
          seq.pattern_len_dec = data_json["pattern_len_dec"];
          seq.clock = data_json["clock"];
          seq.arp_speed = data_json["arp_speed"];
          seq.arp_length = data_json["arp_length"];
          seq.arp_volume_fade = data_json["arp_volume_fade"];
          seq.arp_style = data_json["arp_style"];
          if (data_json["arp_num_notes_max"] > 0)
            seq.arp_num_notes_max = data_json["arp_num_notes_max"];
          else
            seq.arp_num_notes_max = 16;
          seq.chord_vel = data_json["seq_chord_vel"];
          seq.arr_split_note = data_json["arr_split_note"];
          seq.chord_key_ammount = data_json["chord_key_ammount"];
          seq.oct_shift = data_json["seq_oct_shift"];
          seq.element_shift = data_json["seq_element_shift"];
          seq.hihat_randomizer = data_json["hihat_randomizer"];
          seq.euclidean_active = data_json["euclidean_active"];
          seq.euclidean_offset = data_json["euclidean_offset"];
        }

        if (data_json["drum_midi_channel"] > 0) //do not set to onmi when it was never saved before. Better to use the default channel in this case.
          drum_midi_channel = data_json["drum_midi_channel"];
        if (data_json["slices_midi_channel"] > 0) //same as above
          slices_midi_channel = data_json["slices_midi_channel"];

        data->LiveSequencer::LiveSeqData::useGridSongMode = data_json["grid_song_mode"];
        data->LiveSequencer::LiveSeqData::isSongMode = data_json["isSongMode"];

        AudioNoInterrupts();

        for (uint8_t instance_id = 0; instance_id < NUM_DEXED; instance_id++)
        {
#ifdef DEBUG
          LOG.print(F("Load Voice-Config "));
          LOG.print(instance_id + 1);
          LOG.print(F(" for sequencer"));
#endif

          if (instance_id < 2 && bitRead(perf_load_options, perf_msynth))
            load_sd_microsynth_json(number, instance_id);

          if (bitRead(perf_load_options, perf_dexed))
          {
            load_sd_voiceconfig_json(number, instance_id);
            load_sd_voice(configuration.dexed[instance_id].pool, configuration.dexed[instance_id].bank, configuration.dexed[instance_id].voice, instance_id);
            MicroDexed[instance_id]->setGain(midi_volume_transform(map(configuration.dexed[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0, 127)));
            MicroDexed[instance_id]->panic();
          }

        }

        if (NUM_DEXED > 2)
        {
          if (configuration.dexed[2].midi_channel == 0)
            configuration.dexed[2].midi_channel = MIDI_CHANNEL_OFF;
          if (configuration.dexed[3].midi_channel == 0)
            configuration.dexed[3].midi_channel = MIDI_CHANNEL_OFF;
        }

        //load_sd_drumsettings_json(number); check if necessary phtodo

        if (bitRead(perf_load_options, perf_effects))
          load_sd_fx_json(configuration.sys.performance_number); //loaded here since bpm must be loaded first

        AudioInterrupts();
        dac_unmute();
        if (seq.euclidean_active)
          update_euclidean();

        set_seq_timing();

        for (uint8_t d = 0; d < NUM_SEQ_TRACKS; d++)
        {
          seq.chain_counter[d] = 0;
        }

        if (bitRead(perf_load_options, perf_liveseq))
        {
          load_sd_livesequencer_json(number); // before handleStart()
          load_sd_gridsong_json(number);
        }

        update_seq_speed();

        if (seq_was_running)
        {
          handleStart();
        }
        else
          sequencer_timer.begin(sequencer, seq.tempo_ms / (seq.ticks_max + 1), false);

        return (true);
      }
#ifdef DEBUG
      else
      {
        AudioInterrupts();
        LOG.print(F("E : Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      AudioInterrupts();
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }

FLASHMEM bool check_sd_performance_exists(uint8_t number)
{
  if (number < 0)
    return (false);

  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
  // AudioNoInterrupts();
  if (sd_card_internal > 0)
  {
    char filename[CONFIG_FILENAME_LEN];

    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, SEQUENCER_CONFIG_NAME);

    // check if file exists...
    if (SD.exists(filename))
    {
      // AudioInterrupts();
      return (true);
    }
    else
    {
      // AudioInterrupts();
      return (false);
    }
  }
  else
  {
    // AudioInterrupts();
    return (false);
  }
}

FLASHMEM bool check_sd_braids_patch_exists(uint8_t number)
{
  if (number < 0)
    return (false);

  number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
  AudioNoInterrupts();
  if (sd_card_internal > 0)
  {
    char filename[CONFIG_FILENAME_LEN];

    snprintf_P(filename, sizeof(filename), PSTR("/BRAIDS/pb%02d.json"), number);

    // check if file exists...
    if (SD.exists(filename))
    {
      AudioInterrupts();
      return (true);
    }
    else
    {
      AudioInterrupts();
      return (false);
    }
  }
  else
  {
    AudioInterrupts();
    return (false);
  }
}



FLASHMEM bool check_sd_noisemaker_config_exists()
{

  AudioNoInterrupts();
  if (sd_card_internal > 0)
  {
    char filename[CONFIG_FILENAME_LEN];
    snprintf_P(filename, sizeof(filename), PSTR("/%s.json"), CONFIG_NOISEMAKER);

    // check if file exists...
    if (SD.exists(filename))
    {
      AudioInterrupts();
      return (true);
    }
    else
    {
      AudioInterrupts();
      return (false);
    }
  }
  else
  {
    AudioInterrupts();
    return (false);
  }
}


/******************************************************************************
   HELPER FUNCTIONS
 ******************************************************************************/
FLASHMEM bool get_sd_data(File sysex, uint8_t format, uint8_t* conf)
{
  uint16_t n;
  int32_t bulk_checksum_calc = 0;
  int8_t bulk_checksum;

#ifdef DEBUG
  LOG.print(F("Reading "));
  LOG.print(sysex.size());
  LOG.println(F(" bytes."));
#endif

  AudioNoInterrupts();
  if (sysex.read() != 0xf0) // check sysex start-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx start byte not found."));
#endif
    return (false);
  }
  if (sysex.read() != 0x67) // check sysex vendor is unofficial SYSEX-ID for MicroDexed
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx vendor not unofficial SYSEX-ID for MicroDexed."));
#endif
    return (false);
  }
  if (sysex.read() != format) // check for sysex type
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx type not found."));
#endif
    return (false);
  }
  sysex.seek(sysex.size() - 1);
  if (sysex.read() != 0xf7) // check sysex end-byte
  {
#ifdef DEBUG
    LOG.println(F("E : SysEx end byte not found."));
#endif
    return (false);
  }

  sysex.seek(sysex.size() - 2); // Bulk checksum
  bulk_checksum = sysex.read();

  sysex.seek(3); // start of bulk data
  for (n = 0; n < sysex.size() - 6; n++)
  {
    uint8_t d = sysex.read();
    bulk_checksum_calc -= d;
#ifdef DEBUG
    LOG.print(F("SYSEX data read: 0x"));
    LOG.println(d, HEX);
#endif
  }
  bulk_checksum_calc &= 0x7f;

  if (int8_t(bulk_checksum_calc) != bulk_checksum)
  {
#ifdef DEBUG
    LOG.print(F("E : Bulk checksum mismatch : 0x"));
    LOG.print(int8_t(bulk_checksum_calc), HEX);
    LOG.print(F(" != 0x"));
    LOG.println(bulk_checksum, HEX);
#endif
    return (false);
  }
#ifdef DEBUG
  else
  {
    LOG.print(F("Bulk checksum : 0x"));
    LOG.print(int8_t(bulk_checksum_calc), HEX);
    LOG.print(F(" [0x"));
    LOG.print(bulk_checksum, HEX);
    LOG.println(F("]"));
  }
#endif

  sysex.seek(3); // start of bulk data
  for (n = 0; n < sysex.size() - 6; n++)
  {
    uint8_t d = sysex.read();
    *(conf++) = d;
  }
  AudioInterrupts();

#ifdef DEBUG
  LOG.println(F("SD data loaded."));
#endif

  return (true);
  }

FLASHMEM bool write_sd_data(File sysex, uint8_t format, uint8_t* data, uint16_t len)
{
#ifdef DEBUG
  LOG.print(F("Storing SYSEX format 0x"));
  LOG.print(format, HEX);
  LOG.print(F(" with length of "));
  LOG.print(len, DEC);
  LOG.println(F(" bytes."));
#endif

  // write sysex start
  AudioNoInterrupts();
  sysex.write(0xf0);
#ifdef DEBUG
  LOG.println(F("Write SYSEX start:    0xf0"));
#endif
  // write sysex vendor is unofficial SYSEX-ID for MicroDexed
  sysex.write(0x67);
#ifdef DEBUG
  LOG.println(F("Write SYSEX vendor:   0x67"));
#endif
  // write sysex format number
  sysex.write(format);
#ifdef DEBUG
  LOG.print(F("Write SYSEX format:   0x"));
  LOG.println(format, HEX);
#endif
  // write data
  sysex.write(data, len);
#ifdef DEBUG
  for (uint16_t i = 0; i < len; i++)
  {
    LOG.print(F("Write SYSEX data:     0x"));
    LOG.println(data[i], HEX);
  }
#endif
  // write checksum
  sysex.write(calc_checksum(data, len));
#ifdef DEBUG
  uint8_t checksum = calc_checksum(data, len);
  sysex.write(checksum);
  LOG.print(F("Write SYSEX checksum: 0x"));
  LOG.println(checksum, HEX);
#endif
  // write sysex end
  sysex.write(0xf7);
  AudioInterrupts();

#ifdef DEBUG
  LOG.println(F("Write SYSEX end:      0xf7"));
#endif

  return (true);
  }

FLASHMEM bool get_bank_name(uint8_t p, uint8_t b, char* bank_name)
{
#ifdef DEBUG
  LOG.printf_P(PSTR("get bank name for bank [%d]\n"), b);
#endif
  b = constrain(b, 0, MAX_BANKS - 1);

  if (sd_card_internal > 0)
  {
    File sysex_dir;
    char bankdir[FILENAME_LEN + 4];

    snprintf_P(bankdir, sizeof(bankdir), PSTR("/%s/%d/%d"), DEXED_CONFIG_PATH, p, b);

    AudioNoInterrupts();
    sysex_dir = SD.open(bankdir);
    AudioInterrupts();
    if (!sysex_dir)
    {
      strcpy(bank_name, sError);

#ifdef DEBUG
      LOG.print(F("E : Cannot open "));
      LOG.print(bankdir);
      LOG.println(F(" on SD."));
#endif
      return false;
    }

    File entry;
    do
    {
      entry = sysex_dir.openNextFile();
    } while (entry.isDirectory());

    if (entry.isDirectory())
    {
      strcpy(bank_name, sError);
      AudioNoInterrupts();
      entry.close();
      sysex_dir.close();
      AudioInterrupts();
      return false;
    }

    strip_extension(entry.name(), bank_name, BANK_NAME_LEN);
    string_toupper(bank_name);

    AudioNoInterrupts();
    entry.close();
    sysex_dir.close();
    AudioInterrupts();

    return true;
  }

  strcpy(bank_name, sError);
  return false;
}

FLASHMEM bool get_voice_name(uint8_t p, uint8_t b, uint8_t v, char* voice_name)
{
#ifdef DEBUG
  LOG.printf_P(PSTR("get voice name for voice [%d]\n"), v + 1);
#endif
  b = constrain(b, 0, MAX_BANKS - 1);

  if (sd_card_internal > 0)
  {
    File sysex_dir;
    char bankdir[FILENAME_LEN + 4];

    snprintf_P(bankdir, sizeof(bankdir), PSTR("/%s/%d/%d"), DEXED_CONFIG_PATH, p, b);

    AudioNoInterrupts();
    sysex_dir = SD.open(bankdir);
    AudioInterrupts();
    if (!sysex_dir)
    {
      strcpy(voice_name, sError);

#ifdef DEBUG
      LOG.print(F("E : Cannot open "));
      LOG.print(bankdir);
      LOG.println(F(" on SD."));
#endif
      return false;
    }

    File entry;
    do
    {
      entry = sysex_dir.openNextFile();
    } while (entry.isDirectory());

    if (entry.isDirectory())
    {
      strcpy(voice_name, sError);
      AudioNoInterrupts();
      entry.close();
      sysex_dir.close();
      AudioInterrupts();
      return false;
    }

    // load name of voices of the bank
#ifdef DEBUG
    char bank_name[BANK_NAME_LEN];
    strip_extension(entry.name(), bank_name, BANK_NAME_LEN);
    string_toupper(bank_name);
    LOG.printf_P(PSTR("Get voice name from [/%s/%d/%d/%s.syx]\n"), DEXED_CONFIG_PATH, b, p, bank_name);
#endif
    memset(voice_name, 0, VOICE_NAME_LEN);
    entry.seek(124 + (v * 128));
    entry.read(voice_name, min(VOICE_NAME_LEN, 10));
    string_toupper(voice_name);

#ifdef DEBUG
    LOG.printf_P(PSTR("Found voice-name [%s] for bank [%d] and voice [%d]\n"), voice_name, b, v + 1);
#endif

    AudioNoInterrupts();
    entry.close();
    sysex_dir.close();
    AudioInterrupts();

    return true;
  }

  strcpy(voice_name, sError);
  return false;
}

FLASHMEM uint8_t calc_checksum(uint8_t* data, uint16_t len)
{
  int32_t bulk_checksum_calc = 0;

  for (uint16_t n = 0; n < len; n++)
    bulk_checksum_calc -= data[n];

  return (bulk_checksum_calc & 0x7f);
}

FLASHMEM void strip_extension(const char* s, char* target, uint8_t len)
{
  char tmp[CONFIG_FILENAME_LEN];
  char* token;

  strcpy(tmp, s);
  token = strtok(tmp, ".");
  if (token == NULL)
    strcpy(target, sError);
  else
    strcpy(target, token);

  target[len] = '\0';
}

FLASHMEM void string_toupper(char* s)
{
  while (*s)
  {
    *s = toupper((unsigned char)*s);
    s++;
  }
}

#if defined COMPILE_FOR_PSRAM

FLASHMEM bool save_sd_multisample_presets_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, MULTISAMPLE_PRESETS_CONFIG_NAME);
#ifdef DEBUG
    LOG.print(F("Saving multisample slot "));
    LOG.print(number);
    LOG.print(F(" to "));
    LOG.println(filename);
#endif
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
#ifdef DEBUG
      LOG.println(F("remove old multisample file"));
#endif
      SD.remove(filename);
    }
    json = SD.open(filename, FILE_WRITE);
    if (json)
    {
      char zone_filename[CONFIG_FILENAME_LEN];
      StaticJsonDocument<JSON_BUFFER_SIZE> data_json;

      for (uint8_t i = 0; i < NUM_MULTISAMPLES; i++)
      {
        data_json[i]["name"] = msp[i].name;
        data_json[i]["sound_intensity"] = msp[i].sound_intensity;
        data_json[i]["midi_channel"] = msp[i].midi_channel;

        for (uint8_t j = 0; j < NUM_MULTISAMPLE_ZONES; j++)
        {
          strcpy(zone_filename, msz[i][j].filename);
          if (strchr(zone_filename, '.'))
          {
            *(strchr(zone_filename, '.')) = '\0';
          }
          data_json[i]["zones"]["filename"][j] = zone_filename;
          data_json[i]["zones"]["root"][j] = msz[i][j].rootnote;
          data_json[i]["zones"]["psram_no"][j] = msz[i][j].psram_entry_number;
          data_json[i]["zones"]["low"][j] = msz[i][j].low;
          data_json[i]["zones"]["high"][j] = msz[i][j].high;
          data_json[i]["zones"]["playmode"][j] = msz[i][j].playmode;
          data_json[i]["zones"]["vol"][j] = msz[i][j].vol;
          data_json[i]["zones"]["pan"][j] = msz[i][j].pan;
          data_json[i]["zones"]["rev"][j] = msz[i][j].rev;
          data_json[i]["zones"]["tune"][j] = msz[i][j].tune;
          data_json[i]["zones"]["loop_type"][j] = msz[i][j].loop_type;
          data_json[i]["zones"]["loop_start"][j] = msz[i][j].loop_start;
          data_json[i]["zones"]["loop_end"][j] = msz[i][j].loop_end;

        }
      }

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
      LOG.println(F("Write JSON data:"));
      serializeJsonPretty(data_json, Serial);
      LOG.println();
#endif
      serializeJsonPretty(data_json, json);
      json.close();
      AudioInterrupts();
      return (true);
    }
    else
    {
#ifdef DEBUG
      LOG.print(F("E : Cannot open "));
      LOG.print(filename);
      LOG.println(F(" on SD."));
#endif
    }
    AudioInterrupts();
    json.close();
    }
  return (false);
  }

FLASHMEM bool load_sd_multisample_presets_json(uint8_t number)
{
  if (number < 0)
    return (false);

  if (sd_card_internal > 0)
  {
    number = constrain(number, PERFORMANCE_NUM_MIN, PERFORMANCE_NUM_MAX);
    snprintf_P(filename, sizeof(filename), PSTR("/%s/%d/%s.json"), PERFORMANCE_CONFIG_PATH, number, MULTISAMPLE_PRESETS_CONFIG_NAME);

    // first check if file exists...
    AudioNoInterrupts();
    if (SD.exists(filename))
    {
#ifdef DEBUG
      LOG.print(F("Found msp presets data ["));
      LOG.print(filename);
      LOG.println(F("]... loading..."));
      LOG.println(F(" "));
#endif

      json = SD.open(filename);
      if (json)
      {
        StaticJsonDocument<JSON_BUFFER_SIZE> data_json;
        //JsonDocument data_json(JSON_BUFFER_SIZE);
        deserializeJson(data_json, json);
        json.close();
        AudioInterrupts();

#if defined(DEBUG) && defined(DEBUG_SHOW_JSON)
        LOG.println(F("Read JSON data:"));
        serializeJsonPretty(data_json, Serial);
        LOG.println();
#endif
        for (uint8_t i = 0; i < NUM_MULTISAMPLES; i++)
        {
          strcpy(msp[i].name, data_json[i]["name"]);
          msp[i].sound_intensity = data_json[i]["sound_intensity"];
          msp[i].midi_channel = data_json[i]["midi_channel"];

          for (uint8_t j = 0; j < NUM_MULTISAMPLE_ZONES; j++)
          {
            strcpy(msz[i][j].filename, data_json[i]["zones"]["filename"][j]);
            if (strlen(msz[i][j].filename) > 0)
              strcat(msz[i][j].filename, ".wav");
            msz[i][j].rootnote = data_json[i]["zones"]["root"][j];
            msz[i][j].psram_entry_number = data_json[i]["zones"]["psram_no"][j];
            msz[i][j].low = data_json[i]["zones"]["low"][j];
            msz[i][j].high = data_json[i]["zones"]["high"][j];
            msz[i][j].playmode = data_json[i]["zones"]["playmode"][j];
            msz[i][j].vol = data_json[i]["zones"]["vol"][j];
            msz[i][j].pan = data_json[i]["zones"]["pan"][j];
            msz[i][j].rev = data_json[i]["zones"]["rev"][j];
            msz[i][j].tune = data_json[i]["zones"]["tune"][j];
            msz[i][j].loop_type = data_json[i]["zones"]["loop_type"][j];
            msz[i][j].loop_start = data_json[i]["zones"]["loop_start"][j];
            msz[i][j].loop_end = data_json[i]["zones"]["loop_end"][j];

            if (msz[i][j].tune == 0)
              msz[i][j].tune = 100;
          }
        }
        return (true);
      }
#ifdef DEBUG
      else
      {
        LOG.print(F("E: Cannot open "));
        LOG.print(filename);
        LOG.println(F(" on SD."));
      }
    }
    else
    {
      LOG.print(F("No "));
      LOG.print(filename);
      LOG.println(F(" available."));
#endif
    }
      }
  return (false);
    }
#endif

FLASHMEM int compare_files_by_name(storage_file_s fileA, storage_file_s fileB)
{
  String strA = fileA.name.toLowerCase();
  String strB = fileB.name.toLowerCase();

  if (strA.length() == 1)
  {
    strA = "0" + strA;
  }
  if (strB.length() == 1)
  {
    strB = "0" + strB;
  }

  if (fileA.isDirectory == fileB.isDirectory) {
    return strA < strB;
  }
  else { // sort folders before files
    return fileA.isDirectory;
  }
}

#ifdef  SECOND_SD
extern SDClass SD_EXTERNAL;
#endif

FLASHMEM void load_sd_directory()
{
  fm.sd_prev_dir = fm.sd_new_name;

  File sd_root;

  if (fm.SD_CARD_READER_EXT == false || fm.active_window == 0)
    sd_root = SD.open(fm.sd_new_name.c_str());
#ifdef  SECOND_SD
  else
    if (fm.SD_CARD_READER_EXT)
      sd_root = SD_EXTERNAL.open(fm.sd_new_name.c_str());
#endif

  fm.sd_sum_files = 0;
  sdcard_infos.files.clear();

  while (true)
  {
    File sd_entry = sd_root.openNextFile();
    if (!sd_entry)
      break;

    // skip special files
    if (strcmp(sd_entry.name(), "System Volume Information") && strstr(sd_entry.name(), "._") == NULL)
    {
      storage_file_s entry;
      entry.name = sd_entry.name();
      entry.size = sd_entry.size();
      entry.isDirectory = sd_entry.isDirectory();
#ifdef DEBUG
      LOG.print(fm.sd_sum_files);
      LOG.print(F("  "));
      LOG.print(entry.name);
      LOG.print(F("  "));
      LOG.print(entry.size);
      LOG.print(F(" bytes"));
      LOG.println();
#endif
      sdcard_infos.files.emplace_back(entry);
      fm.sd_sum_files++;
    }
    sd_entry.close();
  }
  sd_root.close();

  std::sort(sdcard_infos.files.begin(), sdcard_infos.files.end(), compare_files_by_name);
}


#ifdef COMPILE_FOR_PSRAM
#include <noisemaker.h>
extern NoiseMakerParams nm_params;
extern FLASHMEM void writeWavHeader(File file, unsigned int sampleRate, unsigned int channelCount);
extern FLASHMEM void encodeUint32(File& file, uint32_t value);
// Function to save the generated noise sample to the SD card
// It creates the necessary directory structure if it doesn't exist
FLASHMEM void saveNoiseToSdCard()
{
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  uint8_t targetSlot = NUM_STATIC_PITCHED_SAMPLES + noisemaker_custom_slot;

  // Construct the full path
  char path[FULLPATH_MAX_LEN];
  snprintf(path, sizeof(path), "/CUSTOM/%s/%s", noisemaker_short_category_names[nm_params.category], drum_config[targetSlot].filename);

  // Create directories if they don't exist
  char dir_path[FULLPATH_MAX_LEN];
  snprintf(dir_path, sizeof(dir_path), "/CUSTOM/%s", noisemaker_short_category_names[nm_params.category]);
  AudioNoInterrupts();
  if (!SD.exists(dir_path)) {
    if (!SD.mkdir(dir_path)) {
      return;
    }
  }

  if (SD.exists(path))
    SD.remove(path);

  File file;

  file = SD.open(path, FILE_WRITE);
  writeWavHeader(file, 44100, drum_config[targetSlot].numChannels);

  uint32_t totalToWrite = drum_config[targetSlot].len * sizeof(int16_t);
  //uint32_t totalToWrite = drum_config[targetSlot].len * drum_config[targetSlot].numChannels * sizeof(int16_t);
  uint8_t* dataPtr = (uint8_t*)drum_config[targetSlot].drum_data;
  uint32_t totalWritten = 0;

  while (totalWritten < totalToWrite) {
    int n = file.write(dataPtr + totalWritten, totalToWrite - totalWritten);
    if (n <= 0) {
      break;
    }
    totalWritten += n;
  }

  uint32_t dataChunkSize = totalWritten;
  uint32_t mainChunkSize = 36 + dataChunkSize;

  file.seek(4);
  encodeUint32(file, mainChunkSize);
  file.seek(40);
  encodeUint32(file, dataChunkSize);

  file.flush();
  file.close();

  AudioInterrupts();
}

FLASHMEM void saveCustomSampleToSdCard(uint8_t sample_id)
{
  if (!SD.begin(BUILTIN_SDCARD)) {
    ;
    return;
  }

  if (sample_id >= NUM_DRUMSET_CONFIG) return;

  // Get the directory path and filename
  const char* directoryPath = customSamples[sample_id - NUM_STATIC_PITCHED_SAMPLES].filepath;
  const char* fileName = drum_config[sample_id].filename;

  if (!directoryPath || strlen(directoryPath) == 0 || !fileName || strlen(fileName) == 0) {
    return;
  }

  // Construct the full file path
  char fullPath[FULLPATH_MAX_LEN]; // Use a buffer large enough for the full path
  snprintf(fullPath, sizeof(fullPath), "%s/%s", directoryPath, fileName);

  // Check if SD card is initialized
  if (!SD.begin(BUILTIN_SDCARD)) {
    return;
  }

  // Remove existing file to prevent corruption
  if (SD.exists(fullPath)) {
    SD.remove(fullPath);
  }

  File file = SD.open(fullPath, FILE_WRITE);
  if (!file) {
    return;
  }

  writeWavHeader(file, 44100, drum_config[sample_id].numChannels);

  uint32_t totalToWrite = drum_config[sample_id].len * sizeof(int16_t);
  uint8_t* dataPtr = (uint8_t*)drum_config[sample_id].drum_data;
  uint32_t totalWritten = 0;

  while (totalWritten < totalToWrite) {
    int n = file.write(dataPtr + totalWritten, totalToWrite - totalWritten);
    if (n <= 0) {
      break;
    }
    totalWritten += n;
  }

  uint32_t dataChunkSize = totalWritten;
  uint32_t mainChunkSize = 36 + dataChunkSize;

  file.seek(4);
  encodeUint32(file, mainChunkSize);
  file.seek(40);
  encodeUint32(file, dataChunkSize);

  file.flush();
  file.close();

  AudioInterrupts();
}
#endif
