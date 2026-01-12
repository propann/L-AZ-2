#include "config.h"
#include <Audio.h>
#include "microsynth.h"
#include "template_mixer.hpp"
#include "effect_stereo_panorama.h"

elapsedMillis microsynth_delay_timer[2];
extern AudioSynthWaveform microsynth_waveform[NUM_MICROSYNTH];
extern AudioSynthNoisePink microsynth_noise[NUM_MICROSYNTH];
extern AudioEffectEnvelope microsynth_envelope_osc[NUM_MICROSYNTH];
extern AudioEffectEnvelope microsynth_envelope_noise[NUM_MICROSYNTH];
extern AudioFilterStateVariable microsynth_filter_osc[NUM_MICROSYNTH];
extern AudioFilterStateVariable microsynth_filter_noise[NUM_MICROSYNTH];
extern AudioEffectStereoPanorama microsynth_stereo_panorama_osc[NUM_MICROSYNTH];
extern AudioEffectStereoPanorama microsynth_stereo_panorama_noise[NUM_MICROSYNTH];
extern AudioMixer<2> microsynth_mixer_r[NUM_MICROSYNTH];
extern AudioMixer<2> microsynth_mixer_l[NUM_MICROSYNTH];
extern AudioMixer<4> microsynth_mixer_filter_osc[NUM_MICROSYNTH];
extern AudioMixer<4> microsynth_mixer_filter_noise[NUM_MICROSYNTH];
extern AudioMixer<2> microsynth_mixer_reverb;

extern microsynth_t microsynth[NUM_MICROSYNTH];
extern uint8_t microsynth_selected_instance;
extern AudioMixer<2 * NUM_DRUMS + 4>* global_delay_in_mixer[2];
extern uint8_t generic_temp_select_menu;

extern float volume_transform(float amp);

static constexpr short wave_type[9] = {
  WAVEFORM_SINE,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SAWTOOTH,
  WAVEFORM_SQUARE,
  WAVEFORM_PULSE,
  WAVEFORM_BANDLIMIT_SAWTOOTH,
  WAVEFORM_BANDLIMIT_SQUARE,
  WAVEFORM_BANDLIMIT_PULSE,
  WAVEFORM_SAMPLE_HOLD
};

static uint8_t default_microsynth_midi_channel(uint8_t instance_id)
{
  switch (instance_id)
  {
  case 0:
    return DEFAULT_MICROSYNTH_MIDI_CHANNEL_INST0;
#if NUM_MICROSYNTH > 1
  case 1:
    return DEFAULT_MICROSYNTH_MIDI_CHANNEL_INST1;
#endif
  default:
    return MIDI_CHANNEL_OFF;
  }
}

FLASHMEM void microsynth_reset_instance(uint8_t instance_id)
{
  if (instance_id >= NUM_MICROSYNTH)
    return;

  microsynth_t& synth = microsynth[instance_id];

  synth.coarse = 0;
  synth.detune = 0;
  synth.lfo_intensity = 0;
  synth.lfo_delay = 0;
  synth.lfo_mode = 0;
  synth.lfo_speed = 0;
  synth.lfo_direction = false;
  synth.lfo_value = 0;
  synth.lfo_fade = 0;
  synth.trigger_noise_with_osc = false;
  synth.pan = PANORAMA_DEFAULT;
  synth.wave = 0;
  synth.midi_channel = default_microsynth_midi_channel(instance_id);
  synth.sound_intensity = SOUND_INTENSITY_DEFAULT;
  synth.env_attack = 0;
  synth.env_decay = 0;
  synth.env_sustain = 0;
  synth.env_release = 0;
  synth.filter_osc_mode = 0;
  synth.osc_freq_current = 0;
  synth.filter_osc_freq_from = 0;
  synth.filter_osc_freq_to = 0;
  synth.filter_osc_freq_current = synth.filter_osc_freq_from;
  synth.filter_osc_freq_last_displayed = 99;
  synth.filter_osc_speed = 0;
  synth.filter_osc_resonance = 0;
  synth.noise_vol = 0;
  synth.noise_decay = 0;
  synth.filter_noise_mode = 0;
  synth.filter_noise_freq_from = 0;
  synth.filter_noise_freq_to = 0;
  synth.filter_noise_freq_current = synth.filter_noise_freq_from;
  synth.filter_noise_speed = 0;
  synth.filter_noise_resonance = 0;
  synth.pwm_from = 0;
  synth.pwm_to = 0;
  synth.pwm_speed = 0;
  synth.pwm_current = 0;
  synth.pwm_last_displayed = 0xFFFF;
  synth.rev_send = 0;
  synth.chorus_send = 0;
  for (uint8_t i = 0; i < NUM_MICROSYNTH; i++)
  {
    synth.delay_send[i] = 0;
  }
  synth.vel_mod_filter_osc = 0;
  synth.vel_mod_filter_noise = 0;
  synth.sidechain_send = 0;

  microsynth_delay_timer[instance_id] = 0;
  microsynth_update_all_settings(instance_id);
}

FLASHMEM void microsynth_reset_all()
{
  for (uint8_t instance_id = 0; instance_id < NUM_MICROSYNTH; instance_id++)
    microsynth_reset_instance(instance_id);
}

FLASHMEM void clear_filter_mixer_gains(uint8_t instance_id) {
    for (uint8_t i = 0; i < 4; i++) {
        microsynth_mixer_filter_osc[instance_id].gain(i, 0.0);
        microsynth_mixer_filter_noise[instance_id].gain(i, 0.0);
    }
}

FLASHMEM void set_filter_modes(uint8_t instance_id) {
    microsynth_t& synth = microsynth[instance_id];
    
    // Set oscillator filter mode
    if (synth.filter_osc_mode == 0)
        microsynth_mixer_filter_osc[instance_id].gain(3, 1.0);
    else if (synth.filter_osc_mode == 1)
        microsynth_mixer_filter_osc[instance_id].gain(0, 1.0);
    else if (synth.filter_osc_mode == 2)
        microsynth_mixer_filter_osc[instance_id].gain(1, 1.0);
    else if (synth.filter_osc_mode == 3)
        microsynth_mixer_filter_osc[instance_id].gain(2, 1.0);
    
    // Set noise filter mode  
    if (synth.filter_noise_mode == 0)
        microsynth_mixer_filter_noise[instance_id].gain(3, 1.0);
    else if (synth.filter_noise_mode == 1)
        microsynth_mixer_filter_noise[instance_id].gain(0, 1.0);
    else if (synth.filter_noise_mode == 2)
        microsynth_mixer_filter_noise[instance_id].gain(1, 1.0);
    else if (synth.filter_noise_mode == 3)
        microsynth_mixer_filter_noise[instance_id].gain(2, 1.0);
}

FLASHMEM void microsynth_update_all_settings(uint8_t instance_id)
{
  
clear_filter_mixer_gains(instance_id);
set_filter_modes(instance_id);
 
  microsynth_envelope_osc[instance_id].attack(microsynth[instance_id].env_attack * 4);
  microsynth_envelope_osc[instance_id].decay(microsynth[instance_id].env_decay * 4);
  microsynth_envelope_osc[instance_id].sustain(microsynth[instance_id].env_sustain / 50.1);
  microsynth_envelope_osc[instance_id].release(microsynth[microsynth_selected_instance].env_release * microsynth[microsynth_selected_instance].env_release);
  microsynth_mixer_reverb.gain(instance_id, volume_transform(mapfloat(microsynth[instance_id].rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
  microsynth_filter_noise[instance_id].frequency(microsynth[instance_id].filter_noise_freq_from);
  microsynth_filter_osc[instance_id].frequency(microsynth[instance_id].filter_osc_freq_from);
  microsynth_filter_noise[instance_id].resonance(microsynth[instance_id].filter_noise_resonance / 20);
  microsynth_filter_osc[instance_id].resonance(microsynth[instance_id].filter_osc_resonance / 20);
  microsynth_envelope_noise[instance_id].decay(microsynth[instance_id].noise_decay + 20);
  microsynth_envelope_noise[instance_id].sustain(0);
  microsynth_envelope_noise[instance_id].release(microsynth[instance_id].noise_decay * 20 + 20);
  microsynth_waveform[instance_id].pulseWidth(microsynth[instance_id].pwm_from / 2000.1);
  microsynth[instance_id].pwm_current = microsynth[instance_id].pwm_from;

  microsynth_noise[instance_id].amplitude(0.9f);
  microsynth_waveform[instance_id].amplitude(0.5f);
  microsynth_mixer_l[instance_id].gain(0, mapfloat(microsynth[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.15f));
  microsynth_mixer_r[instance_id].gain(0, mapfloat(microsynth[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.15f));

  microsynth_waveform[instance_id].begin(wave_type[microsynth[instance_id].wave]);
  microsynth_stereo_panorama_osc[instance_id].panorama(mapfloat(microsynth[instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
  microsynth_stereo_panorama_noise[instance_id].panorama(mapfloat(microsynth[instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));

  global_delay_in_mixer[0]->gain(4, mapfloat(microsynth[0].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 2
  global_delay_in_mixer[1]->gain(4, mapfloat(microsynth[0].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 2

  global_delay_in_mixer[0]->gain(5, mapfloat(microsynth[1].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 3
  global_delay_in_mixer[1]->gain(5, mapfloat(microsynth[1].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 3
}

FLASHMEM void update_microsynth_params()
{
  for (uint8_t d = 0; d < NUM_MICROSYNTH; d++)
  {
    if (microsynth_envelope_osc[d].isActive()) // pwm down
    {
      if (microsynth[d].pwm_from > microsynth[d].pwm_to)
      {
        if (microsynth[d].pwm_current > microsynth[d].pwm_to)
        {
          if (microsynth[d].pwm_current - microsynth[d].pwm_speed >= 0)
            microsynth[d].pwm_current = microsynth[d].pwm_current - microsynth[d].pwm_speed;
          else
            microsynth[d].pwm_current = 0;
          microsynth_waveform[d].pulseWidth(microsynth[d].pwm_current / 2000.1);
        }
      }
      else
      {
        if (microsynth[d].pwm_current < microsynth[d].pwm_to) // pwm up
        {
          if (microsynth[d].pwm_current + microsynth[d].pwm_speed <= 2000)
            microsynth[d].pwm_current = microsynth[d].pwm_current + microsynth[d].pwm_speed;
          else
            microsynth[d].pwm_current = 2000;
          microsynth_waveform[d].pulseWidth(microsynth[d].pwm_current / 2000.1);
        }
      }
    }
    if (microsynth[d].filter_osc_freq_from > microsynth[d].filter_osc_freq_to && microsynth[d].filter_osc_speed != 0)
    {
      if (microsynth[d].filter_osc_freq_current > microsynth[d].filter_osc_freq_to) // osc filter down
      {
        if (int(microsynth[d].filter_osc_freq_current / float((1.01 + (microsynth[d].filter_osc_speed * 0.001)))) >= 0)
          microsynth[d].filter_osc_freq_current = int(microsynth[d].filter_osc_freq_current / float((1.01 + (microsynth[d].filter_osc_speed * 0.001))));
        else
          microsynth[d].filter_osc_freq_current = 0;

        microsynth_filter_osc[d].frequency(microsynth[d].filter_osc_freq_current);
      }
    }
    else
    {
      if (microsynth[d].filter_osc_freq_current < microsynth[d].filter_osc_freq_to && microsynth[d].filter_osc_speed != 0)
      { // osc filter up
        if (microsynth[d].filter_osc_freq_current + microsynth[d].filter_osc_speed <= 15000)
          microsynth[d].filter_osc_freq_current = microsynth[d].filter_osc_freq_current + microsynth[d].filter_osc_speed;
        else
          microsynth[d].filter_osc_freq_current = 15000;
        microsynth_filter_osc[d].frequency(microsynth[d].filter_osc_freq_current);
      }
    }

    if (microsynth[d].filter_noise_freq_from > microsynth[d].filter_noise_freq_to && microsynth[d].filter_noise_speed != 0)
    {
      if (microsynth[d].filter_noise_freq_current > microsynth[d].filter_noise_freq_to)
      {
        if (int(microsynth[d].filter_noise_freq_current / float((1.01 + (microsynth[d].filter_noise_speed * 0.001)))) >= 0)
          microsynth[d].filter_noise_freq_current = int(microsynth[d].filter_noise_freq_current / float((1.01 + (microsynth[d].filter_noise_speed * 0.001))));
        else
          microsynth[d].filter_noise_freq_current = 0;
        microsynth_filter_noise[d].frequency(microsynth[d].filter_noise_freq_current);
      }
    }
    else
    {
      if (microsynth[d].filter_noise_freq_current < microsynth[d].filter_noise_freq_to && microsynth[d].filter_noise_speed != 0)
      {
        if (microsynth[d].filter_noise_freq_current + microsynth[d].filter_noise_speed <= 15000)
          microsynth[d].filter_noise_freq_current = microsynth[d].filter_noise_freq_current + microsynth[d].filter_noise_speed;
        else
          microsynth[d].filter_noise_freq_current = 15000;
        microsynth_filter_noise[d].frequency(microsynth[d].filter_noise_freq_current);
      }
    } //  --------------------------------------------------------- OSC LFO ----------------------------------------------------------------------

    if (microsynth[d].lfo_speed > 0 && microsynth[d].lfo_mode == 0) // LFO U&D
    {
      if (microsynth[d].lfo_direction == false && microsynth[d].lfo_value > microsynth[d].lfo_intensity * -1)
        microsynth[d].lfo_value = microsynth[d].lfo_value - microsynth[d].lfo_speed;

      else if (microsynth[d].lfo_direction == true && microsynth[d].lfo_value < microsynth[d].lfo_intensity)
        microsynth[d].lfo_value = microsynth[d].lfo_value + microsynth[d].lfo_speed;

      if (microsynth[d].lfo_value <= microsynth[d].lfo_intensity * -1) // switch mode 0 LFO direction
        microsynth[d].lfo_direction = !microsynth[d].lfo_direction;
      else if (microsynth[d].lfo_mode == 0 && microsynth[d].lfo_value >= microsynth[d].lfo_intensity)
        microsynth[d].lfo_direction = !microsynth[d].lfo_direction;
    }
    else if (microsynth[d].lfo_speed > 0 && microsynth[d].lfo_mode == 1) // LFO Up
    {
      if (microsynth[d].lfo_value < microsynth[d].lfo_intensity * 10)
        microsynth[d].lfo_value = microsynth[d].lfo_value + microsynth[d].lfo_speed;
    }
    else if (microsynth[d].lfo_speed > 0 && microsynth[d].lfo_mode == 2) // LFO Down
    {
      if (microsynth[d].lfo_value > microsynth[d].lfo_intensity * -10)
        microsynth[d].lfo_value = microsynth[d].lfo_value - microsynth[d].lfo_speed;
    }

    //--------------------------------------------------------------------------- LFO FADE/DELAY ---------------------------------------------------------

    if (microsynth[d].lfo_delay == 0) // no delay, instant lfo mod
    {
      microsynth_waveform[d].frequency(microsynth[d].osc_freq_current + microsynth[d].lfo_value / 10);
    }
    else if ((int)microsynth_delay_timer[d] / 10 > microsynth[d].lfo_delay && microsynth[d].lfo_fade == 0) // init lfo fade in
    {
      microsynth[d].lfo_fade = microsynth[d].lfo_delay;
    }
    if (microsynth[d].lfo_fade > 0) // fade in to max lfo intensity
    {
      if (microsynth[d].osc_freq_current + ((microsynth[d].lfo_value / 10) / (1 + float(microsynth[d].lfo_fade / 10))) > 20 && microsynth[d].osc_freq_current + ((microsynth[d].lfo_value / 10) / (1 + float(microsynth[d].lfo_fade / 10))) < 10000)
        microsynth_waveform[d].frequency(microsynth[d].osc_freq_current + ((microsynth[d].lfo_value / 10) / (1 + float(microsynth[d].lfo_fade / 10))));
      if (microsynth[d].lfo_fade > 1) // only count down to multiplier of 1, not 0
        microsynth[d].lfo_fade = microsynth[d].lfo_fade - 1;
    }
  }
}

FLASHMEM void microsynth_update_single_setting(uint8_t instance_id)
{
clear_filter_mixer_gains(instance_id);
set_filter_modes(instance_id);

  if (generic_temp_select_menu == 4)
    microsynth_envelope_osc[instance_id].attack(microsynth[instance_id].env_attack * 4);
  if (generic_temp_select_menu == 5)
    microsynth_envelope_osc[instance_id].decay(microsynth[instance_id].env_decay * 4);
  if (generic_temp_select_menu == 6)
    microsynth_envelope_osc[instance_id].sustain(microsynth[instance_id].env_sustain / 50.1);
  if (generic_temp_select_menu == 7)
    microsynth_envelope_osc[instance_id].release(microsynth[microsynth_selected_instance].env_release * microsynth[microsynth_selected_instance].env_release);
  microsynth_mixer_reverb.gain(instance_id, volume_transform(mapfloat(microsynth[instance_id].rev_send, REVERB_SEND_MIN, REVERB_SEND_MAX, 0.0, VOL_MAX_FLOAT)));
  if (generic_temp_select_menu == 20)
    microsynth_filter_noise[instance_id].frequency(microsynth[instance_id].filter_noise_freq_from);
  microsynth_filter_osc[instance_id].frequency(microsynth[instance_id].filter_osc_freq_from);
  if (generic_temp_select_menu == 22)
    microsynth_filter_noise[instance_id].resonance(microsynth[instance_id].filter_noise_resonance / 20);
  microsynth_filter_osc[instance_id].resonance(microsynth[instance_id].filter_osc_resonance / 20);

  if (generic_temp_select_menu == 17)
    microsynth_envelope_noise[instance_id].decay(microsynth[instance_id].noise_decay + 20);
  microsynth_envelope_noise[instance_id].sustain(0);
  microsynth_envelope_noise[instance_id].release(microsynth[instance_id].noise_decay * 20 + 20);
  microsynth_waveform[instance_id].pulseWidth(microsynth[instance_id].pwm_from / 2000.1);
  microsynth[instance_id].pwm_current = microsynth[instance_id].pwm_from;
  if (generic_temp_select_menu == 16)
  {
    microsynth_noise[instance_id].amplitude(0.9);
    microsynth_mixer_l[instance_id].gain(1, mapfloat(microsynth[instance_id].noise_vol, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.25f));
    microsynth_mixer_r[instance_id].gain(1, mapfloat(microsynth[instance_id].noise_vol, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.25f));
  }

  if (generic_temp_select_menu == 0)
  {
    // microsynth_waveform[instance_id].amplitude(mapfloat(microsynth[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.15f));
    microsynth_mixer_l[instance_id].gain(0, mapfloat(microsynth[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.15f));
    microsynth_mixer_r[instance_id].gain(0, mapfloat(microsynth[instance_id].sound_intensity, SOUND_INTENSITY_MIN, SOUND_INTENSITY_MAX, 0.0f, 0.15f));
  }
  if (generic_temp_select_menu == 1)
    microsynth_waveform[instance_id].begin(wave_type[microsynth[instance_id].wave]);
  microsynth_stereo_panorama_osc[instance_id].panorama(mapfloat(microsynth[instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));
  microsynth_stereo_panorama_noise[instance_id].panorama(mapfloat(microsynth[instance_id].pan, PANORAMA_MIN, PANORAMA_MAX, -1.0, 1.0));

  if (generic_temp_select_menu == 30 || generic_temp_select_menu == 31)
  {
    global_delay_in_mixer[0]->gain(4, mapfloat(microsynth[0].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 2
    global_delay_in_mixer[1]->gain(4, mapfloat(microsynth[0].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 2
    global_delay_in_mixer[0]->gain(5, mapfloat(microsynth[1].delay_send[0], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 3
    global_delay_in_mixer[1]->gain(5, mapfloat(microsynth[1].delay_send[1], DELAY_LEVEL_MIN, DELAY_LEVEL_MAX, 0.0, 0.80)); //previously 3
  }
}
