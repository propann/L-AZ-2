#ifdef COMPILE_FOR_PSRAM

#include <Arduino.h>
#include <math.h>
#include <noisemaker.h>
#include "config.h"
#include "drums.h"
#include "psramloader.h"
#include <Audio.h>
#include "offline_reverb.h"
#include "PitchSamplePlayer.h"
extern PitchSamplePlayer WAV_preview_PSRAM;

OfflineReverb noiseReverb;

extern void drawDrumsSampleWave(bool fullRedraw);
extern PsramLoader sampleLoader();
extern void play_sample(uint8_t index);
extern drum_config_t drum_config[NUM_DRUMSET_CONFIG];
extern config_t configuration;
extern bool drumScreenActive;
extern uint8_t activeSample;
extern CustomSample customSamples[NUM_CUSTOM_SAMPLES];

 const char* noisemaker_category_names[] = {
   "BASSDRUM" , "SNARE", "TOM", "CONGA", "RIMSHOT", "CLAP", "HIHAT", "CRASH", "RIDE", "COWBELL", "CLAVES", "MARACAS", "ZAP", "SNAP", "SYNTH"
};

 const char* noisemaker_short_category_names[] = {
   "BD" , "SNARE", "TOM", "CONGA", "RIM", "CLAP", "HH", "CRASH", "RIDE", "COW", "CLAV", "MARA", "ZAP", "SNAP", "SYNTH"
};

enum SynthWaveform { SINE, SAW, SQUARE, SQ80_BASS };

FLASHMEM void addAutoUpdateParameterEditor_uint8_t(const char* name, uint8_t minv, uint8_t maxv, uint8_t* value);
FLASHMEM void addAutoUpdateParameterEditor_uint16_t(const char* name, uint16_t minv, uint16_t maxv, uint16_t* value);

NoiseMakerParams nm_params = { NM_BD };
uint8_t noisemaker_custom_slot = 0;
int16_t* nm_preview_data = nullptr;
uint32_t nm_preview_len = 0;
//void generateDrum(NoiseMakerParams &p, int16_t* buf, uint32_t n, uint8_t channels);

// Add stereo helper functions
FLASHMEM inline float stereoPanLeft(float sample, float pan) {
  return sample * sqrtf(1.0f - pan);
}

FLASHMEM inline float stereoPanRight(float sample, float pan) {
  return sample * sqrtf(pan);
}

FLASHMEM inline float stereoSpread(float sample, float width, int channel) {
  if (channel == 0) { // left
    return sample * (1.0f - width * 0.5f);
  }
  else { // right
    return sample * (1.0f + width * 0.5f);
  }
}


// ---- helpers: PSRAM alloc/free with safe fallback -----------------------
void* nm_psram_malloc(size_t size) {
  return extmem_malloc(size);
}
void nm_psram_free(void* ptr) {
  if (ptr) extmem_free(ptr);
}

FLASHMEM  void nm_preview_free() {
  if (nm_preview_data) {
    nm_psram_free(nm_preview_data);
    nm_preview_data = nullptr;
  }
  nm_preview_len = 0;

}

FLASHMEM bool nm_preview_alloc(uint32_t frames, uint8_t channels) {
  nm_preview_free();

  uint32_t targetFrames = frames;

  if (nm_params.reverb_enable) {
    const uint32_t REVERB_BUFFER_SECONDS = 5;
    targetFrames = (uint32_t)(SAMPLE_RATE * REVERB_BUFFER_SECONDS);
  }
  else if (channels == 2) {
    const uint32_t STEREO_EXTRA_MS = 60;
    const uint32_t extraFrames = (uint32_t)(SAMPLE_RATE * (STEREO_EXTRA_MS / 1000.0f));
    targetFrames += extraFrames;
  }

  const uint32_t bytes = targetFrames * channels * sizeof(int16_t);

  nm_preview_data = (int16_t*)nm_psram_malloc(bytes);
  if (!nm_preview_data) {
    nm_preview_len = 0;
    return false;
  }

  memset(nm_preview_data, 0, bytes);

  nm_preview_len = targetFrames;
  return true;
}

// helper function to convert a uint8_t value (0-100) to a float (0.0-1.0)
FLASHMEM float convertUint8ToFloat(uint8_t value) {
  return static_cast<float>(value) / 100.0f;
}

// ------------------------------- Sample Analysis ---------------------------------
// A function to check if the generated sample itself has high-pitched noise.
FLASHMEM bool isSampleDangerous() {

  if (nm_params.category != NM_ZAP && nm_params.category != NM_MARACAS) //ZAP is supposed to have high frequencies like this
  {
    const float DANGER_THRESHOLD = 0.05f; // Empirical value for detecting harsh high-freq content
    const uint8_t channels = nm_params.stereo_mode ? 2 : 1;

    float high_pass_energy = 0.0f;
    float prev_sample = 0.0f;
    uint32_t samples_analyzed = 0;

    for (uint32_t i = 0; i < nm_preview_len; i += channels) {
      if (nm_preview_data[i] != 0) {
        float sample = nm_preview_data[i] / 32768.0f; // Normalize to -1.0 to 1.0

        // Simple 1st-order high-pass filter to isolate high frequencies
        float high_passed_sample = sample - prev_sample;
        prev_sample = sample;

        high_pass_energy += fabsf(high_passed_sample);
        samples_analyzed++;
      }
    }

    if (samples_analyzed == 0) return false; // Avoid division by zero

    // Calculate the average energy over the analysis window
    float average_energy = high_pass_energy / (float)samples_analyzed;
    return average_energy > DANGER_THRESHOLD;
  }
  else
    return false;
}

FLASHMEM void setupNoiseReverb() {
  noiseReverb.init(44100.0f); // Assuming 44.1kHz sample rate
  // Set default reverb parameters
  noiseReverb.setRoomSize(convertUint8ToFloat(nm_params.reverb_room_size));
  noiseReverb.setDamping(convertUint8ToFloat(nm_params.reverb_damping));
  noiseReverb.setWetLevel(float(convertUint8ToFloat(nm_params.reverb_wet)) / 102);
  noiseReverb.setDryLevel(convertUint8ToFloat(nm_params.reverb_dry));
  noiseReverb.setWidth(convertUint8ToFloat(nm_params.reverb_width));
}

FLASHMEM void generateAndPlayNoise() {
  setupNoiseReverb();
  // if (!nm_preview_data || nm_preview_len == 0) {
  generateNoisePreview();
  // }
  if (!nm_preview_data || nm_preview_len == 0) return;

  if (nm_params.reverb_enable)
    noiseReverb.applyReverb(nm_preview_data, nm_preview_len, nm_params.stereo_mode);

  uint8_t channels = (nm_params.stereo_mode == 0) ? 1 : 2;
  if ((const uint8_t*)nm_preview_data != NULL && nm_preview_len > 0)
  {
    if (isSampleDangerous() == false)
      WAV_preview_PSRAM.playRaw((int16_t*)(const uint8_t*)nm_preview_data, nm_preview_len, channels);
  }

  // Give the audio engine a moment to start reading
  delay(40);
}

FLASHMEM void PlayOnly() {
  if (!nm_preview_data || nm_preview_len == 0) return;
  uint8_t channels = (nm_params.stereo_mode == 0) ? 1 : 2;
  if ((const uint8_t*)nm_preview_data != NULL && nm_preview_len > 0)
  {
    if (isSampleDangerous() == false)
      WAV_preview_PSRAM.playRaw((int16_t*)(const uint8_t*)nm_preview_data, nm_preview_len, channels);
  }
  // Give the audio engine a moment to start reading
  delay(40);
}


FLASHMEM void PlaySlot() {
  play_sample(NUM_STATIC_PITCHED_SAMPLES + noisemaker_custom_slot);
  delay(40);
}

extern FLASHMEM void saveNoiseToSdCard();

FLASHMEM void saveNoiseToCustomSlot() {
  if (!nm_preview_data || nm_preview_len == 0) {
    generateNoisePreview();
    if (!nm_preview_data || nm_preview_len == 0) return;
  }

  uint8_t targetSlot = NUM_STATIC_PITCHED_SAMPLES + noisemaker_custom_slot;
  uint8_t channels = (nm_params.stereo_mode == 0) ? 1 : 2;
  // Free existing data if any
  if (drum_config[targetSlot].drum_data != nullptr) {
    extmem_free(const_cast<uint8_t*>(drum_config[targetSlot].drum_data));
    drum_config[targetSlot].drum_data = nullptr;
  }

  // Corrected size calculation. nm_preview_len is the total number of samples.
  // It's already the number of samples * channels.
  size_t bytes = nm_preview_len * sizeof(int16_t);
  uint8_t* newData = (uint8_t*)extmem_malloc(bytes);

  if (newData) {
    // Correctly copy the data
    memcpy(newData, nm_preview_data, bytes);

    // Update drum config
    drum_config[targetSlot].drum_data = newData;
    drum_config[targetSlot].len = nm_preview_len;
    drum_config[targetSlot].numChannels = channels;
    drum_config[targetSlot].vol_max = 100;
    drum_config[targetSlot].pan = 0;

    // Update name
    char name[20];
    char file_name[20];
    char filep[FULLPATH_MAX_LEN];

    snprintf(name, sizeof(name), "NM-%s-%02d", noisemaker_short_category_names[nm_params.category], noisemaker_custom_slot + 1);
    snprintf(file_name, sizeof(file_name), "NM-%s-%02d.wav", noisemaker_short_category_names[nm_params.category], noisemaker_custom_slot + 1);
    strncpy(drum_config[targetSlot].name, name, sizeof(drum_config[targetSlot].name) - 1);
    strncpy(drum_config[targetSlot].filename, file_name, sizeof(drum_config[targetSlot].filename) - 1);
    snprintf(filep, sizeof(filep), "/CUSTOM/%s", noisemaker_short_category_names[nm_params.category]);
    strncpy(customSamples[noisemaker_custom_slot].filepath, filep, sizeof(filep));

    // Update custom sample settings
    customSamples[noisemaker_custom_slot].start = 0;
    customSamples[noisemaker_custom_slot].end = 1000; //this is just for the trimming function, not lenght of the sample

    switch (nm_params.category) {
    case NM_BD:
      drum_config[targetSlot].drum_class = DRUM_BASS;
      break;
    case NM_SD:
      drum_config[targetSlot].drum_class = DRUM_SNARE;
      break;
    case NM_TOM:
      drum_config[targetSlot].drum_class = DRUM_MIDTOM;
      break;
    case NM_CONGA:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_RIM:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_SNAP:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_COWBELL:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_CLAVES:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_MARACAS:
      drum_config[targetSlot].drum_class = DRUM_PERCUSSION;
      break;
    case NM_CLAP:
      drum_config[targetSlot].drum_class = DRUM_HANDCLAP;
      break;
    case NM_HH:
      drum_config[targetSlot].drum_class = DRUM_HIHAT;
      break;
    case NM_CRASH:
      drum_config[targetSlot].drum_class = DRUM_CRASH;
      break;
    case NM_RIDE:
      drum_config[targetSlot].drum_class = DRUM_RIDE;
      break;
    default:
      drum_config[targetSlot].drum_class = DRUM_NONE;
      break;
    }

    // Update waveform display
    drawNoiseMakerWaveform(true);
    saveNoiseToSdCard();
  }
}

// ==================================================
// Helpers
// ==================================================

FLASHMEM inline uint32_t msToSamples(float ms) {
  // convert milliseconds -> samples, rounding to nearest sample
  const float samplesF = (ms / 1000.0f) * SAMPLE_RATE;
  return (uint32_t)lrintf(samplesF);
}

FLASHMEM inline float clamp1(float x) {
  if (x < -1.f) return -1.f;
  if (x > 1.f) return  1.f;
  return x;
}

inline float nm_rand_white() {
  return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

// ==================================================
// Envelopes
// ==================================================
FLASHMEM inline float amplitudeEnvelopeADSR(uint32_t i, uint32_t n,
  uint16_t a_ms, uint16_t d_ms,
  uint16_t s_pct, uint16_t r_ms) {
  uint32_t a = msToSamples(a_ms);
  uint32_t d = msToSamples(d_ms);
  uint32_t r = msToSamples(r_ms);
  float s = s_pct / 100.0f;

  if (i < a) return (float)i / (float)a;
  else if (i < a + d) {
    float t = (float)(i - a) / (float)d;
    return 1.f + t * (s - 1.f);
  }
  else if (i < n - r) return s;
  else {
    float t = (float)(i - (n - r)) / (float)r;
    return s * (1.f - t);
  }
}

FLASHMEM inline float expPitchEnv(uint32_t i, uint32_t n, float startRatio, float endRatio = 1.f) {
  float t = (float)i / (float)n;
  return startRatio * powf(endRatio / startRatio, t);
}

FLASHMEM inline float softclip(float x) {
  return x / (1.f + fabsf(x));
}

// ---------------- Small helpers (branch-light where it matters) -------------
static inline float nm_clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float nm_mix(float a, float b, float t) { return a + (b - a) * t; }
static inline float nm_softclip(float x) { return tanhf(x); }
static inline float nm_clip1(float x) { return nm_clamp(x, -1.f, 1.f); }
static inline float nm_abs(float x) { return x < 0 ? -x : x; }

// Map 0..100 to [minV, maxV]
static inline float map01_100(uint8_t v, float minV, float maxV) {
  return minV + (maxV - minV) * (float)v * (1.0f / 100.0f);
}

// ----------------------------- Envelopes ------------------------------------
// Simple AR (attack, release) in milliseconds
FLASHMEM static inline float envAR(uint32_t i, uint32_t n, float attackMs, float releaseMs) {
  const float aS = attackMs * 0.001f * SAMPLE_RATE + 1.f;
  const float rS = releaseMs * 0.001f * SAMPLE_RATE + 1.f;
  if (i < (uint32_t)aS) return (float)i / aS;
  const uint32_t r0 = (n > (uint32_t)rS) ? (n - (uint32_t)rS) : 0u;
  if (i < r0) return 1.f;
  const float t = (float)(i - r0) / rS;
  return 1.f - t;
}

// Exponential percussion envelope (fast attack ~0ms, mono-decay in ms)
FLASHMEM static inline float envPercExp(uint32_t i, float decayMs) {
  const float lambda = 1.0f / (decayMs * 0.001f * SAMPLE_RATE + 1.f);
  // ~-60 dB by 'decayMs'
  const float c = 6.9f;
  return expf(-(float)i * lambda * c);
}

FLASHMEM static inline float envPercExpMsCymbal(float tMs, float decayMs) {
  float x = tMs / decayMs;
  if (x > 1.f) x = 1.f;
  return expf(-5.0f * x); // adjust 5.0f for sharper or softer decay
}


// ADSR with times in ms and sustain 0..1
FLASHMEM static inline float envADSR_ms(uint32_t i, uint32_t n, float aMs, float dMs, float sLvl, float rMs) {
  const float aS = aMs * 0.001f * SAMPLE_RATE + 1.f;
  const float dS = dMs * 0.001f * SAMPLE_RATE + 1.f;
  const float rS = rMs * 0.001f * SAMPLE_RATE + 1.f;

  if (i < (uint32_t)aS) return (float)i / aS; // attack
  const uint32_t d0 = (uint32_t)aS;
  if (i < d0 + (uint32_t)dS) {
    const float t = (float)(i - d0) / dS;
    return nm_mix(1.f, sLvl, t);
  }
  const uint32_t r0 = (n > (uint32_t)rS) ? (n - (uint32_t)rS) : n;
  if (i < r0) return sLvl;
  const float t = (float)(i - r0) / rS;
  return sLvl * (1.f - t);
}

// Exponential pitch envelope from startRatio -> 1.0 over N samples
FLASHMEM static inline float pitchEnvExp(uint32_t i, uint32_t n, float startRatio) {
  const float t = (float)i / (float)std::max<uint32_t>(1, n - 1);
  return powf(startRatio, 1.0f - t);
}

// --------------------------- Filters (OnePole) -------------------------------
// Lightweight one-pole LP/HP (alpha in (0,1)), simple but musical
struct OnePole {
  float z = 0.f;
  float a = 0.2f;
  bool  hp = false; // false=LP(true)=HP

  void setLP(float alpha) { a = nm_clamp(alpha, 0.001f, 0.999f); hp = false; }
  void setHP(float alpha) { a = nm_clamp(alpha, 0.001f, 0.999f); hp = true; }

  inline float process(float x) {
    z += a * (x - z);
    return hp ? (x - z) : z;
  }
};

// ------------------------- State Variable Filter (SVF) -----------------------
// TPT-SVF (Zavalishin) with LP/BP/HP outputs.
struct SVF {
  float g = 0.f, R = 0.5f;
  float ic1eq = 0.f, ic2eq = 0.f;

  void set(float cutHz, float Q) {
    cutHz = nm_clamp(cutHz, 10.f, 0.49f * SAMPLE_RATE);
    Q = nm_clamp(Q, 0.1f, 20.f);
    const float K = tanf((float)M_PI * cutHz / SAMPLE_RATE);
    g = K;
    R = 1.f / Q;
  }

  void setLP(float cutHz, float Q) {
    set(cutHz, Q);
  }
  void setBP(float cutHz, float Q) {
    set(cutHz, Q);
  }
  void setHP(float cutHz, float Q) {
    set(cutHz, Q);
  }

  inline void process(float x, float& lp, float& bp, float& hp) {
    const float v1 = (x - ic2eq - R * ic1eq) / (1.f + R * g + g * g);
    const float v2 = ic1eq + g * v1;
    const float v3 = ic2eq + g * v2;
    ic1eq = 2.f * v2 - ic1eq;
    ic2eq = 2.f * v3 - ic2eq;
    lp = v3;
    bp = v2;
    hp = x - R * bp - lp;
  }
  inline float process(float x) {
    float l, b, h;
    process(x, l, b, h);
    return h;
  }

  inline float lpOut(float x) { float l, b, h; process(x, l, b, h); return l; }
  inline float bpOut(float x) { float l, b, h; process(x, l, b, h); return b; }
  inline float hpOut(float x) { float l, b, h; process(x, l, b, h); return h; }
};

// ------------------------ Defaults ------------------------------------------
FLASHMEM void initNoiseMakerParams(NoiseMakerParams& p) {

  p.reverb_room_size = 100;
  p.reverb_damping = 30;
  p.reverb_wet = 100;
  p.reverb_dry = 95;
  p.reverb_width = 100;

  setupNoiseReverb();
  p.accent = 90;

  // BD
  p.bd_pitch = 20;
  p.bd_pitch_env = 10;
  p.bd_pitch_decay = 30;
  p.bd_decay = 50;
  p.bd_attack = 0;
  p.bd_release = 40;
  p.bd_tone = 75;
  p.bd_click = 25;
  p.bd_noise = 5;
  p.bd_drive = 10;
  p.bd_comp = 12;

  // SN
  p.sn_tone1 = 180;
  p.sn_tone2 = 330;
  p.sn_tone_mix = 50;
  p.sn_pitch_env = 20;
  p.sn_decay = 65;
  p.sn_noise = 70;
  p.sn_bp_freq = 4800;
  p.sn_bp_q = 25;
  p.sn_snap = 35;
  p.sn_drive = 8;

  // Toms
  p.tom_pitch = 140;
  p.tom_decay = 65;
  p.tom_pitch_env = 20;
  p.tom_tone = 70;
  p.tom_noise = 10;

  // Congas (808)
// Congas
  p.conga_pitch = 220;  // base pitch (~Low conga)
  p.conga_decay = 70;   // envelope length
  p.conga_tone = 60;   // sine ↔ woody balance
  p.conga_pitch_env = 20;   // pitch drop at onset
  p.conga_noise = 30;   // noise contribution

  p.rim_decay = 50;   // medium decay
  p.rim_tone = 60;   // tone vs noise balance
  p.rim_pitch = 1600; // base resonator frequency (Hz)
  p.rim_mod_freq = 1200; // second modal frequency (Hz)
  p.rim_noise_bp_freq = 3500; // noise filter center freq (Hz)
  p.rim_drive = 8;    // moderate drive
  p.rim_spread = 15;   // slight stereo detune

  // New user-controlled levels
  p.rim_click_level = 60;   // sine burst amplitude (0–100)
  p.rim_noise_level = 40;   // noise layer amplitude (0–100)

  // Clap
  p.clap_repeats = 3;
  p.clap_spread = 50;
  p.clap_tail = 60;
  p.clap_bp_hz = 1200;
  p.clap_bp_q = 80;
  p.clap_decay = 65;
  p.clap_drive = 8;

  p.hh_tone = 40;   // Balance between metallic and noise
  p.hh_noise = 60;   // Emphasize noise for open hats
  p.hh_bp_hz = 3000;
  p.hh_bp_q = 5;
  p.hh_decay = 60;   // Base decay time
  p.hh_drive = 10;
  p.hh_detune = 4;

  // Cymbals
  p.crash_tone = 60;
  p.crash_bp_hz = 6500;
  p.crash_bp_q = 60;
  p.crash_decay = 85;
  p.ride_tone = 55;
  p.ride_bp_hz = 7000;
  p.ride_bp_q = 65;
  p.ride_decay = 90;

  // Cowbell
  p.cb_freq1 = 560;
  p.cb_freq2 = 840;
  p.cb_amp_attack = 0;
  p.cb_amp_decay = 55;
  p.cb_tone_balance = 50;
  p.cb_noise = 5;
  p.cb_drive = 20;

  // Claves
  p.claves_hz = 1700;
  p.claves_decay = 40;
  p.claves_drive = 5;

  // Maracas
  p.mar_bp_hz = 1800; // base BP center
  p.mar_bp_q = 70;   // resonance
  p.mar_decay = 50;   // ~70 ms total AR

  p.zap_pitch_start = 6000;
  p.zap_pitch_end = 200;
  p.zap_decay = 50;

  p.snap_decay = 80;      // 20–80%
  p.snap_tone = 50;     // 20–100% (sine ↔ noise)
  p.snap_pitch = 1500;  // 1200–2800 Hz (woody ↔ metallic)
  p.snap_drive = 10;       // 0–20%
  p.snap_spread = 50;       // % stereo spread

  p.synth_midi_note = 36;
  p.synth_attack = 0;
  p.synth_decay = 40;
  p.synth_sustain_level = 1;
  p.synth_release = 20;
  p.synth_filter_cutoff = 100;
  p.synth_filter_resonance = 90;
  p.synth_filter_env_amount = 40;
  p.synth_filter_env_attack = 0;
  p.synth_filter_env_decay = 10;
  p.synth_filter_env_release = 10;
  p.synth_waveform_mix = 60;
  p.synth_octave_mix = 50;
  p.synth_noise_level = 50;
  p.synth_noise_decay = 50;
}

// --------------------------- Time helpers (ms) -------------------------------
FLASHMEM static inline float u8_to_ms_fast(uint8_t v, float minMs, float maxMs) {
  return map01_100(v, minMs, maxMs);
}
FLASHMEM static inline float u8_to_q(uint8_t v, float minQ = 0.4f, float maxQ = 12.f) {
  return map01_100(v, minQ, maxQ);
}

// ============================================================================
//                            SOUND GENERATORS
// ============================================================================

FLASHMEM static inline void generateBassdrum(NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  float phase = 0.f;

  const float f0 = mode909 ? 20.0f + ((float)p.bd_pitch / 100.0f) * 50.0f : 5.0f + ((float)p.bd_pitch / 100.0f) * 25.0f;
  const float decayMs = u8_to_ms_fast(p.bd_decay, 60.f, 800.f);
  const float releaseMs = mode909 ? 0.0f : u8_to_ms_fast(p.bd_release, 20.f, 600.f);
  const float pitchEnvFactor = mode909 ? (1.f + 0.05f * (p.bd_pitch_env)) : (1.f + 0.02f * (p.bd_pitch_env));
  const float startRatio = (mode909 ? 4.0f : 14.0f) * pitchEnvFactor;
  OnePole clickHP_L, clickHP_R;
  clickHP_L.setHP(mode909 ? 0.35f : 0.2f);
  clickHP_R.setHP(mode909 ? 0.35f : 0.2f);

  const float pitchEnvDurationMs = mode909 ? 10.0f + (p.bd_pitch_decay / 100.0f) * 90.0f : 50.0f + (p.bd_pitch_decay / 100.0f) * 450.0f;
  const uint32_t pitchEnvN = (uint32_t)(SAMPLE_RATE * (pitchEnvDurationMs / 1000.0f));
  const uint32_t clickSamples = (uint32_t)(SAMPLE_RATE * (mode909 ? 0.0015f : 0.002f));

  const float toneAmt = p.bd_tone * 0.01f;
  const float clickAmt = p.bd_click * 0.01f;
  const float noiseAmt = p.bd_noise * 0.001f;
  const float driveAmt = p.bd_drive * 0.01f;
  const float compAmt = p.bd_comp * 0.01f;
  const float accent = p.accent * 0.01f;
  const float spreadAmt = p.bd_spread * 0.01f; // New spread parameter

  const uint32_t fade_start_idx = (2 * n) / 3;

  for (uint32_t i = 0; i < n; ++i) {
    const float a = envPercExp(i, decayMs + releaseMs);
    const float pitch = pitchEnvExp(std::min(i, pitchEnvN), pitchEnvN, startRatio);
    const float freq = f0 * pitch;

    phase += 2.f * (float)M_PI * freq / SAMPLE_RATE;
    if (phase >= 2.f * (float)M_PI) phase -= 2.f * (float)M_PI;
    float s = sinf(phase);
    float body = nm_mix(s, nm_softclip(1.8f * s), toneAmt);

    // Stereo processing for click and noise
    float click_L = 0.f, click_R = 0.f;
    if (i < clickSamples) {
      click_L = clickHP_L.process(nm_rand_white()) * (1.f - (float)i / (float)clickSamples);
      click_R = clickHP_R.process(nm_rand_white()) * (1.f - (float)i / (float)clickSamples);
    }
    float tailNoise_L = noiseAmt * nm_rand_white() * (1.0f - (float)i / (float)n);
    float tailNoise_R = noiseAmt * nm_rand_white() * (1.0f - (float)i / (float)n);
    if (mode909) {
      tailNoise_L *= 1.2f;
      tailNoise_R *= 1.2f;
    }

    // Combine mono body with stereo click/noise
    float y_L = body + nm_mix(clickAmt * click_L, clickAmt * click_R, 1.0f - spreadAmt) + nm_mix(tailNoise_L, tailNoise_R, 1.0f - spreadAmt);
    float y_R = body + nm_mix(clickAmt * click_R, clickAmt * click_L, 1.0f - spreadAmt) + nm_mix(tailNoise_R, tailNoise_L, 1.0f - spreadAmt);

    y_L = nm_mix(y_L, nm_softclip(3.0f * y_L), driveAmt);
    y_R = nm_mix(y_R, nm_softclip(3.0f * y_R), driveAmt);

    if (!mode909 && compAmt > 0.001f) {
      const float c_L = tanhf(y_L * (1.0f + compAmt * 2.2f));
      const float c_R = tanhf(y_R * (1.0f + compAmt * 2.2f));
      y_L = nm_mix(y_L, c_L, compAmt);
      y_R = nm_mix(y_R, c_R, compAmt);
    }

    if (mode909) {
      y_L *= 1.15f;
      y_R *= 1.15f;
    }

    y_L *= a * accent;
    y_R *= a * accent;

    if (i >= fade_start_idx) {
      float fade_factor = 1.0f - ((float)(i - fade_start_idx) / (float)(n - fade_start_idx));
      y_L *= fade_factor;
      y_R *= fade_factor;
    }

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Snare --------

FLASHMEM float nm_rand_pink() {
  // A simple approximation of pink noise
  static float white_noise_history[32] = { 0 };
  static int index = 0;

  // Generate a new white noise sample
  float white = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

  // Apply a simple low-pass filter to approximate pink noise
  float pink = (white + white_noise_history[index]) * 0.5f;

  white_noise_history[index] = white;
  index = (index + 1) % 32;

  return pink;
}

FLASHMEM static inline void generateSnare(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  float ph1_L = 0.f, ph1_R = 0.f, ph2_L = 0.f, ph2_R = 0.f, ph3_L = 0.f, ph3_R = 0.f;

  const float f1 = mode909 ? p.sn_tone1 : 170.f;
  const float f2 = mode909 ? p.sn_tone2 : 250.f;
  const float f3 = mode909 ? p.sn_tone1 * 0.5f : 170.f * 0.5f;

  const float decayMs = u8_to_ms_fast(p.sn_decay, 60.f, 900.f);
  const float snapAmt = p.sn_snap * 0.01f;
  const float noiseAmt = p.sn_noise * 0.001f;
  const float toneMix = p.sn_tone_mix * 0.01f;
  const float driveAmt = p.sn_drive * 0.01f;
  const float accent = p.accent * 0.01f;
  const float spreadAmt = p.sn_spread * 0.01f; // New spread parameter

  SVF noiseFilter_L, noiseFilter_R;
  float noiseFreq = (float)p.sn_bp_freq;
  float filterQ = 1.0f + ((float)p.sn_bp_q / 100.0f) * 9.0f;
  float freqOffset = 0.05f * spreadAmt * noiseFreq; // Up to 5% freq difference
  noiseFilter_L.set(noiseFreq - freqOffset, filterQ);
  noiseFilter_R.set(noiseFreq + freqOffset, filterQ);

  const float startRatio = mode909 ? 1.0f + 0.01f * p.sn_pitch_env : 1.0f + 0.05f * p.sn_pitch_env;
  const uint32_t pitchEnvN = (uint32_t)(SAMPLE_RATE * (mode909 ? 0.005f : 0.008f));
  const uint32_t snapN = (uint32_t)(SAMPLE_RATE * (mode909 ? 0.004f : 0.002f));
  float gainCompensation_L = 1.0f / (1.0f + (filterQ - 1.0f) * 0.75f);
  float gainCompensation_R = 1.0f / (1.0f + (filterQ - 1.0f) * 0.75f);

  for (uint32_t i = 0; i < n; ++i) {
    const float a = envPercExp(i, decayMs);
    const float envPitch = pitchEnvExp(std::min(i, pitchEnvN), pitchEnvN, startRatio);
    const float ft1 = f1 * envPitch;
    const float ft2 = f2 * envPitch;
    const float ft3 = f3 * envPitch;

    ph1_L += 2.f * (float)M_PI * ft1 / SAMPLE_RATE; if (ph1_L > 2.f * (float)M_PI) ph1_L -= 2.f * (float)M_PI;
    ph1_R += 2.f * (float)M_PI * ft1 / SAMPLE_RATE; if (ph1_R > 2.f * (float)M_PI) ph1_R -= 2.f * (float)M_PI;
    ph2_L += 2.f * (float)M_PI * ft2 / SAMPLE_RATE; if (ph2_L > 2.f * (float)M_PI) ph2_L -= 2.f * (float)M_PI;
    ph2_R += 2.f * (float)M_PI * ft2 / SAMPLE_RATE; if (ph2_R > 2.f * (float)M_PI) ph2_R -= 2.f * (float)M_PI;
    ph3_L += 2.f * (float)M_PI * ft3 / SAMPLE_RATE; if (ph3_L > 2.f * (float)M_PI) ph3_L -= 2.f * (float)M_PI;
    ph3_R += 2.f * (float)M_PI * ft3 / SAMPLE_RATE; if (ph3_R > 2.f * (float)M_PI) ph3_R -= 2.f * (float)M_PI;

    float tone_L = nm_mix(nm_mix(sinf(ph1_L), sinf(ph2_L), toneMix), sinf(ph3_L), 0.5f);
    float tone_R = nm_mix(nm_mix(sinf(ph1_R), sinf(ph2_R), toneMix), sinf(ph3_R), 0.5f);

    float lp_L, bpOut_L, hpOut_L;
    float lp_R, bpOut_R, hpOut_R;
    if (mode909) {
      noiseFilter_L.process(nm_rand_pink(), lp_L, bpOut_L, hpOut_L);
      noiseFilter_R.process(nm_rand_pink(), lp_R, bpOut_R, hpOut_R);
      bpOut_L = hpOut_L;
      bpOut_R = hpOut_R;
    }
    else {
      noiseFilter_L.process(nm_rand_white(), lp_L, bpOut_L, hpOut_L);
      noiseFilter_R.process(nm_rand_white(), lp_R, bpOut_R, hpOut_R);
    }
    float nShape_L = bpOut_L * gainCompensation_L;
    float nShape_R = bpOut_R * gainCompensation_R;

    float snap_L = 0.f, snap_R = 0.f;
    if (i < snapN) {
      float t = 1.f - (float)i / (float)snapN;
      snap_L = nm_rand_white() * t;
      snap_R = nm_rand_white() * t;
    }

    float y_L = (1.f - noiseAmt) * tone_L + noiseAmt * nShape_L + snapAmt * snap_L;
    float y_R = (1.f - noiseAmt) * tone_R + noiseAmt * nShape_R + snapAmt * snap_R;

    y_L = nm_mix(y_L, nm_softclip(3.0f * y_L), driveAmt);
    y_R = nm_mix(y_R, nm_softclip(3.0f * y_R), driveAmt);

    y_L *= a * accent;
    y_R *= a * accent;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Toms (808) / Toms (909) --------

FLASHMEM static inline void generateTom(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float f0 = mode909 ? (float)p.tom_pitch * 0.5f : (float)p.tom_pitch;
  float ph_L = 0.f, ph_R = 0.f;

  const float decayMs = u8_to_ms_fast(p.tom_decay, 80.f, 1200.f);
  const float toneAmt = p.tom_tone * 0.01f;
  const float noiseAmt = p.tom_noise * 0.01f;
  const float accent = p.accent * 0.01f;
  const float startRatio = 1.f + (mode909 ? 0.015f : 0.02f) * p.tom_pitch_env;
  const float spreadAmt = p.tom_spread * 0.01f;

  OnePole hp_L, hp_R; hp_L.setHP(0.2f); hp_R.setHP(0.2f);
  const uint32_t tickSamples = (uint32_t)(SAMPLE_RATE * (mode909 ? 0.0015f : 0.002f));

  for (uint32_t i = 0; i < n; ++i) {
    const float a = envPercExp(i, decayMs);
    const float freq = f0 * pitchEnvExp(std::min(i, (uint32_t)(0.3f * n)), (uint32_t)(0.3f * n), startRatio);
    float freq_L = freq * (1.0f - spreadAmt * 0.01f);
    float freq_R = freq * (1.0f + spreadAmt * 0.01f);

    ph_L += 2.f * (float)M_PI * freq_L / SAMPLE_RATE;
    if (ph_L > 2.f * (float)M_PI) ph_L -= 2.f * (float)M_PI;
    ph_R += 2.f * (float)M_PI * freq_R / SAMPLE_RATE;
    if (ph_R > 2.f * (float)M_PI) ph_R -= 2.f * (float)M_PI;

    float s_L = nm_mix(sinf(ph_L), nm_softclip(1.7f * sinf(ph_L)), toneAmt);
    float s_R = nm_mix(sinf(ph_R), nm_softclip(1.7f * sinf(ph_R)), toneAmt);

    float tick_L = 0.0f, tick_R = 0.0f;
    if (i < tickSamples) {
      tick_L = noiseAmt * hp_L.process(nm_rand_white()) * (1.0f - (float)i / (float)tickSamples);
      tick_R = noiseAmt * hp_R.process(nm_rand_white()) * (1.0f - (float)i / (float)tickSamples);
    }
    s_L += tick_L;
    s_R += tick_R;

    if (mode909) {
      s_L = nm_softclip(1.6f * s_L);
      s_R = nm_softclip(1.6f * s_R);
    }

    s_L *= a * accent;
    s_R *= a * accent;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(s_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(s_R) * 32767.f);
    }
    else {
      float mono_mix = (s_L + s_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Congas (808/909 improved v3) --------

FLASHMEM static inline void generateConga(NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  float basePitch = (float)p.conga_pitch;
  float f0 = basePitch;
  f0 *= 1.3f;
  float ph1_L = 0.f, ph1_R = 0.f, ph2_L = 0.f, ph2_R = 0.f;

  const float decayMs = u8_to_ms_fast(p.conga_decay, 80.f, 1000.f);
  const float toneAmt = p.conga_tone * 0.01f;
  const float accent = p.accent * 0.01f;
  const float noiseAmt = p.conga_noise * 0.01f;
  const float pitchEnvAmt = p.conga_pitch_env * 0.001f;
  const float spreadAmt = p.conga_spread * 0.01f;
  const float detune = 1.012f;


  float x1_L = 0.f, x2_L = 0.f, x1_R = 0.f, x2_R = 0.f, y1_L = 0.f, y2_L = 0.f, y1_R = 0.f, y2_R = 0.f;
  float b0, b1, b2, a1, a2;
  {
    float w0 = 2.f * (float)M_PI * f0 / SAMPLE_RATE;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.f * 4.0f);
    b0 = alpha; b1 = 0.f; b2 = -alpha;
    float a0 = 1.f + alpha; a1 = -2.f * cosw0; a2 = 1.f - alpha;
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
  }

  for (uint32_t i = 0;i < n;++i) {
    float a = envPercExp(i, decayMs);
    float pitchMod = 1.0f + pitchEnvAmt * expf(-0.0008f * (float)i * (1000.f / SAMPLE_RATE));
    float f0_L = f0 * pitchMod * (1.0f - spreadAmt * 0.01f);
    float f0_R = f0 * pitchMod * (1.0f + spreadAmt * 0.01f);

    ph1_L += 2.f * (float)M_PI * f0_L / SAMPLE_RATE; if (ph1_L > 2.f * (float)M_PI) ph1_L -= 2.f * (float)M_PI;
    ph1_R += 2.f * (float)M_PI * f0_R / SAMPLE_RATE; if (ph1_R > 2.f * (float)M_PI) ph1_R -= 2.f * (float)M_PI;
    ph2_L += 2.f * (float)M_PI * f0_L * detune / SAMPLE_RATE; if (ph2_L > 2.f * (float)M_PI) ph2_L -= 2.f * (float)M_PI;
    ph2_R += 2.f * (float)M_PI * f0_R * detune / SAMPLE_RATE; if (ph2_R > 2.f * (float)M_PI) ph2_R -= 2.f * (float)M_PI;

    float osc_L = 0.65f * sinf(ph1_L) + 0.35f * sinf(ph2_L);
    float osc_R = 0.65f * sinf(ph1_R) + 0.35f * sinf(ph2_R);

    float x0_L = ((rand() & 0x7fff) / 16384.f - 1.f);
    float x0_R = ((rand() & 0x7fff) / 16384.f - 1.f);
    float yn_L = b0 * x0_L + b1 * x1_L + b2 * x2_L - a1 * y1_L - a2 * y2_L;
    float yn_R = b0 * x0_R + b1 * x1_R + b2 * x2_R - a1 * y1_R - a2 * y2_R;
    x2_L = x1_L; x1_L = x0_L; y2_L = y1_L; y1_L = yn_L;
    x2_R = x1_R; x1_R = x0_R; y2_R = y1_R; y1_R = yn_R;

    float s_L = nm_mix(osc_L, osc_L * (1.0f - noiseAmt) + yn_L * noiseAmt, toneAmt);
    float s_R = nm_mix(osc_R, osc_R * (1.0f - noiseAmt) + yn_R * noiseAmt, toneAmt);

    s_L *= a * accent; s_R *= a * accent;
    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(s_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(s_R) * 32767.f);
    }
    else {
      float mono_mix = (s_L + s_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

FLASHMEM static inline void generateRimshot(const NoiseMakerParams& p,
  int16_t* psram_buf,
  uint32_t n,
  bool mode909,
  uint8_t channels)
{
  const bool stereo = (channels == 2);
  const float accent = p.accent * 0.01f;
  const float toneBal = p.rim_tone * 0.01f;
  const float baseFreq = (float)p.rim_pitch;

  // Envelope (shorter for 909, longer for 808)
  const float decayMs = u8_to_ms_fast(p.rim_decay,
    mode909 ? 200.f : 300.f,
    mode909 ? 800.f : 1500.f);

  // Resonators (two slightly detuned band-pass filters)
  SVF toneResonator_L, toneResonator_R;
  // Adjusted Q values for a more authentic resonance
  float toneQ = mode909 ? 4.5f : 2.5f;
  float spread = baseFreq * (p.rim_spread * 0.0005f); // +/- detune
  toneResonator_L.setBP(baseFreq - spread, toneQ);
  toneResonator_R.setBP(baseFreq + spread, toneQ);

  // Noise bandpass (clicky attack, adds realism)
  SVF noiseFilter_L, noiseFilter_R;
  // Adjusted Q and center frequency for a less harsh, more classic noise sound
  float noiseQ = mode909 ? 2.0f : 2.5f;
  float noiseFreq = (float)p.rim_noise_bp_freq;
  // Lowered the center frequency and narrowed the spread
  noiseFilter_L.setBP(noiseFreq * 0.98f, noiseQ);
  noiseFilter_R.setBP(noiseFreq * 1.02f, noiseQ);

  for (uint32_t i = 0; i < n; ++i) {
    float env = envPercExp(i, decayMs);

    // -----------------------------
    // Noise burst exciter
    // Made the burst shorter and more balanced
    float excNoise = 0.f;
    if (i < 500) {
      float burstEnv = 1.0f - (float)i / 500.f;
      // Adjusted noise gain for better balance
      excNoise = nm_rand_white() * burstEnv * 0.02f * p.rim_noise_level;
    }

    // Sine burst exciter (click)
    // Made the burst much shorter and punchier
    float excSine = 0.f;
    if (i < SAMPLE_RATE / 200) { // ~5ms burst
      float ph = 2.f * (float)M_PI * (baseFreq * 0.5f) * (float)i / SAMPLE_RATE;
      float sineEnv = expf(-0.01f * (float)i);
      excSine = sinf(ph) * sineEnv * 0.02f * p.rim_click_level; // scaled by 0–1
    }

    // Combine exciters
    float exc = excNoise + excSine;

    // -----------------------------
    // Resonant tone
    float toneL = toneResonator_L.bpOut(exc);
    float toneR = toneResonator_R.bpOut(exc);

    // -----------------------------
    // Noise layer (user-scaled)
    // Reduced the noise level to 1/4 by scaling the multiplier
    float noiseL = noiseFilter_L.bpOut(nm_rand_white()) * 0.00025f * p.rim_noise_level;
    float noiseR = noiseFilter_R.bpOut(nm_rand_white()) * 0.00025f * p.rim_noise_level;

    // -----------------------------
    // Mix tone vs noise
    float sigL = nm_mix(toneL, noiseL, 1.f - toneBal);
    float sigR = nm_mix(toneR, noiseR, 1.f - toneBal);

    // -----------------------------
    // Drive + softclip
    float driveAmt = p.rim_drive * 0.01f;
    float driveGain = 1.0f + driveAmt * 2.0f;
    float yL = nm_softclip(driveGain * sigL) * env * accent;
    float yR = nm_softclip(driveGain * sigR) * env * accent;

    // -----------------------------
    // Output
    if (stereo) {
      psram_buf[i * 2] = (int16_t)lrintf(nm_clip1(yL) * 32767.f);
      psram_buf[i * 2 + 1] = (int16_t)lrintf(nm_clip1(yR) * 32767.f);
    }
    else {
      float mono = 0.5f * (yL + yR);
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono) * 32767.f);
    }
  }
}


// ------------------------------- Clap ---------------------------------------

FLASHMEM static inline void generateClap(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);

  const int bursts = mode909 ? p.clap_repeats : 3;
  const float spread = map01_100(p.clap_spread, 8.f, 24.f) * 0.001f;
  const int burstGap = (int)((spread / (float)bursts) * SAMPLE_RATE);

  const float tailAmt = p.clap_tail * 0.01f;
  const float decayMs = u8_to_ms_fast(p.clap_decay, 120.f, 900.f);
  const float driveAmt = p.clap_drive * 0.01f;

  // The base frequency for the bandpass filter
  float base_bp_hz = (float)p.clap_bp_hz;

  // The amount of frequency variation based on the spread parameter
  float freq_var = map01_100(p.clap_spread, 0.0f, 50.0f); // Vary by up to 1000 Hz

  // Create two separate filter objects with slightly different frequencies
  float bp_q = mode909 ? u8_to_q(p.clap_bp_q) * 0.6f : u8_to_q(p.clap_bp_q);
  SVF bp_L; bp_L.set(base_bp_hz - freq_var, bp_q);
  SVF bp_R; bp_R.set(base_bp_hz + freq_var, bp_q);

  for (uint32_t i = 0; i < n; ++i) {
    float y_L = 0.f;
    float y_R = 0.f;

    // burst cluster
    for (int b = 0; b < bursts; ++b) {
      int idx = (int)i - b * burstGap;
      if (idx >= 0) {
        float w = 1.f - (float)b * (1.f / std::max(1, bursts));

        // Process unique noise streams through their respective filters
        float lp_L, bpOut_L, hp_L; bp_L.process(nm_rand_white(), lp_L, bpOut_L, hp_L);
        float lp_R, bpOut_R, hp_R; bp_R.process(nm_rand_white(), lp_R, bpOut_R, hp_R);

        float envB = 1.f - (float)idx / (float)std::max(1, (int)(0.006f * SAMPLE_RATE));
        if (envB < 0.f) envB = 0.f;

        y_L += w * bpOut_L * envB;
        y_R += w * bpOut_R * envB;
      }
    }

    // tail (smoother, longer filtered noise)
    float lp_L_tail, bpOut_L_tail, hp_L_tail; bp_L.process(nm_rand_white(), lp_L_tail, bpOut_L_tail, hp_L_tail);
    float lp_R_tail, bpOut_R_tail, hp_R_tail; bp_R.process(nm_rand_white(), lp_R_tail, bpOut_R_tail, hp_R_tail);
    float tail_L = bpOut_L_tail * envPercExp(i, decayMs);
    float tail_R = bpOut_R_tail * envPercExp(i, decayMs);

    y_L = nm_mix(y_L, tail_L, tailAmt);
    y_R = nm_mix(y_R, tail_R, tailAmt);

    if (mode909) {
      y_L = nm_softclip(1.8f * y_L);
      y_R = nm_softclip(1.8f * y_R);
    }

    y_L = nm_mix(y_L, nm_softclip(2.5f * y_L), driveAmt);
    y_R = nm_mix(y_R, nm_softclip(2.5f * y_R), driveAmt);

    y_L *= (p.accent * 0.01f);
    y_R *= (p.accent * 0.01f);

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}


// -------- Hi-Hats --------

FLASHMEM static inline void generateHiHat(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float freqs808[6] = { 4000, 4800, 5600, 6400, 7200, 8000 };
  const float freqs909[6] = { 5000, 6000, 7000, 8000, 9000, 10000 };
  const float* ff = mode909 ? freqs909 : freqs808;
  float ph_L[6] = { 0 }, ph_R[6] = { 0 };

  float decayMetal = (p.hh_mode == 1) ? u8_to_ms_fast(p.hh_decay, 500.f, 1000.f)
    : u8_to_ms_fast(p.hh_decay, 80.f, 250.f);
  float decayNoise = (p.hh_mode == 1) ? u8_to_ms_fast(p.hh_decay, 600.f, 1200.f)
    : u8_to_ms_fast(p.hh_decay, 80.f, 250.f);

  SVF bp_L, bp_R;
  float scaled_q_hh = p.hh_bp_q * (mode909 ? 0.7f : 1.0f);
  float spreadFreq = p.hh_spread * 0.01f * 500.0f; // up to 500 Hz spread
  bp_L.set((float)p.hh_bp_hz - spreadFreq, u8_to_q((uint8_t)scaled_q_hh));
  bp_R.set((float)p.hh_bp_hz + spreadFreq, u8_to_q((uint8_t)scaled_q_hh));
  OnePole hp_L, hp_R; hp_L.setHP(0.12f); hp_R.setHP(0.12f);

  for (uint32_t i = 0;i < n;++i) {
    float aMetal = envPercExp(i, decayMetal);
    float aNoise = envPercExp(i, decayNoise);
    float metallic_L = 0.f, metallic_R = 0.f;

    for (int k = 0;k < 6;++k) {
      if (i == 0) {
        ph_L[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
        ph_R[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
      }
      float freqDetune = ff[k] * 0.005f * (((rand() & 0x3fff) / 16384.f) - 0.5f);
      float freq = ff[k] + freqDetune;
      float freq_L = freq * (1.0f - p.hh_detune * 0.001f);
      float freq_R = freq * (1.0f + p.hh_detune * 0.001f);

      ph_L[k] += 2.f * (float)M_PI * freq_L / SAMPLE_RATE;
      if (ph_L[k] > 2.f * (float)M_PI) ph_L[k] -= 2.f * (float)M_PI;
      ph_R[k] += 2.f * (float)M_PI * freq_R / SAMPLE_RATE;
      if (ph_R[k] > 2.f * (float)M_PI) ph_R[k] -= 2.f * (float)M_PI;

      float amp = (p.hh_mode == 1) ? 0.6f : 0.55f;
      metallic_L += amp * (sinf(ph_L[k]) >= 0.f ? 1.f : -1.f);
      metallic_R += amp * (sinf(ph_R[k]) >= 0.f ? 1.f : -1.f);
    }
    metallic_L /= 6.f; metallic_R /= 6.f;
    float mlp_L, mbp_L, mhp_L; bp_L.process(metallic_L, mlp_L, mbp_L, mhp_L);
    float mlp_R, mbp_R, mhp_R; bp_R.process(metallic_R, mlp_R, mbp_R, mhp_R);
    metallic_L = nm_mix(metallic_L, mbp_L, 0.5f) * aMetal;
    metallic_R = nm_mix(metallic_R, mbp_R, 0.5f) * aMetal;

    float noise_L = hp_L.process(nm_rand_white()) * aNoise * ((p.hh_mode == 1) ? 0.25f : 0.4f);
    float noise_R = hp_R.process(nm_rand_white()) * aNoise * ((p.hh_mode == 1) ? 0.25f : 0.4f);
    float mixFactor = (p.hh_mode == 1) ? 0.25f : 0.35f;
    float y_L = nm_mix(metallic_L, noise_L, mixFactor);
    float y_R = nm_mix(metallic_R, noise_R, mixFactor);

    y_L = nm_mix(y_L, nm_softclip(2.0f * y_L), p.hh_drive * 0.01f);
    y_R = nm_mix(y_R, nm_softclip(2.0f * y_R), p.hh_drive * 0.01f);
    y_L *= p.accent * 0.01f;
    y_R *= p.accent * 0.01f;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Crash --------

FLASHMEM static inline void generateCrash(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float freqs808[6] = { 450, 630, 810, 1200, 1800, 2700 };
  const float freqs909[7] = { 550, 770, 990, 1470, 2205, 3307, 4960 };
  const float* ff = mode909 ? freqs909 : freqs808;
  const int nPartials = mode909 ? 7 : 6;
  float ph_L[7] = { 0 }, ph_R[7] = { 0 };

  float decayMetal = mode909 ? u8_to_ms_fast(p.crash_decay, 1800.f, 5000.f) : u8_to_ms_fast(p.crash_decay, 1200.f, 4000.f);
  float decayNoise = mode909 ? u8_to_ms_fast(p.crash_decay, 2200.f, 5500.f) : u8_to_ms_fast(p.crash_decay, 1500.f, 4500.f);

  SVF bp_L, bp_R;
  float crashQ = 1.0f + ((float)p.crash_bp_q / 100.0f) * 0.9f;
  float spreadFreq = p.crash_spread * 0.01f * 50.0f;
  bp_L.set((float)p.crash_bp_hz - spreadFreq, crashQ);
  bp_R.set((float)p.crash_bp_hz + spreadFreq, crashQ);
  OnePole hp_L, hp_R; hp_L.setHP(0.1f); hp_R.setHP(0.1f);
  float gainComp = 1.0f / crashQ;

  for (uint32_t i = 0; i < n; ++i) {
    float aMetal = envPercExp(i, decayMetal);
    float aNoise = envPercExp(i, decayNoise);
    float metallic_L = 0.f, metallic_R = 0.f;

    for (int k = 0; k < nPartials; ++k) {
      if (i == 0) {
        ph_L[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
        ph_R[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
      }
      float det = ff[k] * 0.003f * (((rand() & 0x3fff) / 16384.f) - 0.5f);
      float freq = ff[k] + det;

      ph_L[k] += 2.f * (float)M_PI * freq / SAMPLE_RATE;
      if (ph_L[k] > 2.f * (float)M_PI) ph_L[k] -= 2.f * (float)M_PI;
      ph_R[k] += 2.f * (float)M_PI * freq / SAMPLE_RATE;
      if (ph_R[k] > 2.f * (float)M_PI) ph_R[k] -= 2.f * (float)M_PI;

      metallic_L += (sinf(ph_L[k]) >= 0.f ? 1.f : -1.f);
      metallic_R += (sinf(ph_R[k]) >= 0.f ? 1.f : -1.f);
    }
    metallic_L /= (float)nPartials; metallic_R /= (float)nPartials;
    metallic_L *= aMetal; metallic_R *= aMetal;

    float mlp_L, mbp_L, mhp_L; bp_L.process(nm_rand_white(), mlp_L, mbp_L, mhp_L);
    float mlp_R, mbp_R, mhp_R; bp_R.process(nm_rand_white(), mlp_R, mbp_R, mhp_R);
    float noise_L = hp_L.process(mbp_L) * aNoise * (mode909 ? 0.35f : 0.4f) * gainComp;
    float noise_R = hp_R.process(mbp_R) * aNoise * (mode909 ? 0.35f : 0.4f) * gainComp;

    float y_L = nm_mix(metallic_L, noise_L, p.crash_tone * 0.01f);
    float y_R = nm_mix(metallic_R, noise_R, p.crash_tone * 0.01f);

    if (mode909) {
      y_L = nm_softclip(1.5f * y_L);
      y_R = nm_softclip(1.5f * y_R);
    }
    y_L *= p.accent * 0.01f;
    y_R *= p.accent * 0.01f;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Ride --------

FLASHMEM static inline void generateRide(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float freqs808[6] = { 300, 450, 900, 1350, 2025, 4050 };
  const float freqs909[7] = { 320, 480, 960, 1440, 2160, 4320, 6400 };
  const float* ff = mode909 ? freqs909 : freqs808;
  const int nPartials = mode909 ? 7 : 6;
  float ph_L[7] = { 0 }, ph_R[7] = { 0 };

  float decayMetal = mode909 ? u8_to_ms_fast(p.ride_decay, 2500.f, 6000.f) : u8_to_ms_fast(p.ride_decay, 2000.f, 5000.f);
  float decayNoise = mode909 ? u8_to_ms_fast(p.ride_decay, 2800.f, 6500.f) : u8_to_ms_fast(p.ride_decay, 2200.f, 5500.f);

  SVF bp_L, bp_R;
  float rideQ = 1.0f + ((float)p.ride_bp_q / 100.0f) * 0.10f;
  float spreadFreq = p.ride_spread * 0.01f * 20.0f;
  bp_L.set((float)p.ride_bp_hz - spreadFreq, rideQ);
  bp_R.set((float)p.ride_bp_hz + spreadFreq, rideQ);
  OnePole hp_L, hp_R; hp_L.setHP(0.1f); hp_R.setHP(0.1f);
  float gainComp = 1.0f / rideQ;

  for (uint32_t i = 0; i < n; ++i) {
    float aMetal = envPercExp(i, decayMetal);
    float aNoise = envPercExp(i, decayNoise);
    float metallic_L = 0.f, metallic_R = 0.f;

    for (int k = 0; k < nPartials; ++k) {
      if (i == 0) {
        ph_L[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
        ph_R[k] = ((rand() & 0xffff) / 65535.f) * 2.f * (float)M_PI;
      }
      float det = ff[k] * 0.002f * (((rand() & 0x3fff) / 16384.f) - 0.5f);
      float freq = ff[k] + det;
      ph_L[k] += 2.f * (float)M_PI * freq / SAMPLE_RATE;
      if (ph_L[k] > 2.f * (float)M_PI) ph_L[k] -= 2.f * (float)M_PI;
      ph_R[k] += 2.f * (float)M_PI * freq / SAMPLE_RATE;
      if (ph_R[k] > 2.f * (float)M_PI) ph_R[k] -= 2.f * (float)M_PI;

      metallic_L += (sinf(ph_L[k]) >= 0.f ? 1.f : -1.f);
      metallic_R += (sinf(ph_R[k]) >= 0.f ? 1.f : -1.f);
    }
    metallic_L /= (float)nPartials; metallic_R /= (float)nPartials;
    metallic_L *= aMetal; metallic_R *= aMetal;

    float mlp_L, mbp_L, mhp_L; bp_L.process(nm_rand_white(), mlp_L, mbp_L, mhp_L);
    float mlp_R, mbp_R, mhp_R; bp_R.process(nm_rand_white(), mlp_R, mbp_R, mhp_R);
    float noise_L = hp_L.process(mbp_L) * aNoise * (mode909 ? 0.3f : 0.35f) * gainComp;
    float noise_R = hp_R.process(mbp_R) * aNoise * (mode909 ? 0.3f : 0.35f) * gainComp;

    float y_L = nm_mix(metallic_L, noise_L, p.ride_tone * 0.01f);
    float y_R = nm_mix(metallic_R, noise_R, p.ride_tone * 0.01f);

    if (mode909) {
      y_L = nm_softclip(1.4f * y_L);
      y_R = nm_softclip(1.4f * y_R);
    }
    y_L *= p.accent * 0.01f;
    y_R *= p.accent * 0.01f;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Maracas (808-style, Multi-Grain) --------

FLASHMEM static inline void generateMaracas(NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float sr = SAMPLE_RATE;
  const float accentGain = (p.accent * 0.01f);

  const float totalMs = u8_to_ms_fast(p.mar_decay, 25.f, 120.f);
  const float attackMs = totalMs * 0.7f;
  const float releaseMs = totalMs * 0.3f;
  const uint32_t atkSamples = (uint32_t)(attackMs * sr / 1000.f);
  const uint32_t relSamples = (uint32_t)(releaseMs * sr / 1000.f);
  const float attackSlope = 1.0f / max(1u, atkSamples);
  const float releaseSlope = 1.0f / max(1u, relSamples);

  float cutoffHz = float(p.mar_bp_hz);
  float q = 0.7f + (float(p.mar_bp_q) / 100.0f) * 0.5f;
  float spreadFreq = p.mar_spread * 0.01f * 1000.0f; // Up to 1000 Hz spread

  SVF bp_L; bp_L.set(cutoffHz - spreadFreq, q);
  SVF bp_R; bp_R.set(cutoffHz + spreadFreq, q);

  const int numGrains = 3;
  int grainOffsets[numGrains] = { 0, 3, 7 };

  for (uint32_t i = 0; i < n; ++i) {
    float env = 0.0f;
    for (int g = 0; g < numGrains; ++g) {
      uint32_t offset = static_cast<uint32_t>(grainOffsets[g]);
      uint32_t idx = (i >= offset) ? i - offset : 0;
      float e = 0.0f;
      if (idx < atkSamples) e = float(idx) * attackSlope;
      else if (idx < atkSamples + relSamples) e = 1.0f - (float(idx - atkSamples) * releaseSlope);
      env += e;
    }
    env = env / numGrains;
    float noise_L = nm_rand_white();
    float noise_R = nm_rand_white();
    float shaped_L = noise_L * env * accentGain;
    float shaped_R = noise_R * env * accentGain;

    float lp_L, bpOut_L, hp_L; bp_L.process(shaped_L, lp_L, bpOut_L, hp_L);
    float lp_R, bpOut_R, hp_R; bp_R.process(shaped_R, lp_R, bpOut_R, hp_R);

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(bpOut_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(bpOut_R) * 32767.f);
    }
    else {
      float mono_mix = (bpOut_L + bpOut_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Cowbell (808) --------

FLASHMEM void generateCowbell(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  float phase1_L = 0.f, phase1_R = 0.f, phase2_L = 0.f, phase2_R = 0.f;
  const float f1 = (float)p.cb_freq1;
  const float f2 = (float)p.cb_freq2;
  const float balance = p.cb_tone_balance * 0.01f;
  const float noiseAmt = p.cb_noise * 0.01f;
  const float driveAmt = p.cb_drive * 0.01f;
  const float spreadAmt = p.cb_spread * 0.01f;
  float detune_factor = 1.0f + spreadAmt * 0.05f;

  for (uint32_t i = 0; i < n; ++i) {
    float env = expf(-15.0f * (float)i / (float)n);
    phase1_L += 2.f * PI * f1 / SAMPLE_RATE; if (phase1_L >= 2.f * PI) phase1_L -= 2.f * PI;
    phase1_R += 2.f * PI * f1 * detune_factor / SAMPLE_RATE; if (phase1_R >= 2.f * PI) phase1_R -= 2.f * PI;
    phase2_L += 2.f * PI * f2 / SAMPLE_RATE; if (phase2_L >= 2.f * PI) phase2_L -= 2.f * PI;
    phase2_R += 2.f * PI * f2 * detune_factor / SAMPLE_RATE; if (phase2_R >= 2.f * PI) phase2_R -= 2.f * PI;

    float tone1_L = sinf(phase1_L); float tone1_R = sinf(phase1_R);
    float tone2_L = sinf(phase2_L); float tone2_R = sinf(phase2_R);
    float tone_L = (1.0f - balance) * tone1_L + balance * tone2_L;
    float tone_R = (1.0f - balance) * tone1_R + balance * tone2_R;
    float noise_L = nm_rand_white() * noiseAmt; float noise_R = nm_rand_white() * noiseAmt;

    float s_L = (tone_L + noise_L) * env;
    float s_R = (tone_R + noise_R) * env;
    s_L = (1.f - driveAmt) * s_L + driveAmt * tanhf(2.0f * s_L);
    s_R = (1.f - driveAmt) * s_R + driveAmt * tanhf(2.0f * s_R);

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(s_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(s_R) * 32767.f);
    }
    else {
      float mono_mix = (s_L + s_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Claves (808) --------

FLASHMEM static inline void generateClaves(NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  float ph_L = 0.f, ph_R = 0.f;
  const float decayMs = u8_to_ms_fast(p.claves_decay, 20.f, 300.f);
  const float drive = p.claves_drive * 0.01f;
  const float spreadAmt = p.claves_spread * 0.01f;
  float freq_L = (float)p.claves_hz * (1.0f - spreadAmt * 0.005f);
  float freq_R = (float)p.claves_hz * (1.0f + spreadAmt * 0.005f);

  for (uint32_t i = 0;i < n;++i) {
    float a = envPercExp(i, decayMs);
    ph_L += 2.f * (float)M_PI * freq_L / SAMPLE_RATE; if (ph_L > 2.f * (float)M_PI) ph_L -= 2.f * (float)M_PI;
    ph_R += 2.f * (float)M_PI * freq_R / SAMPLE_RATE; if (ph_R > 2.f * (float)M_PI) ph_R -= 2.f * (float)M_PI;
    float s_L = sinf(ph_L); float s_R = sinf(ph_R);
    s_L = nm_mix(s_L, nm_softclip(2.0f * s_L), drive);
    s_R = nm_mix(s_R, nm_softclip(2.0f * s_R), drive);
    s_L *= a * (p.accent * 0.01f);
    s_R *= a * (p.accent * 0.01f);

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(s_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(s_R) * 32767.f);
    }
    else {
      float mono_mix = (s_L + s_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// -------- Zap --------

FLASHMEM static inline void generateZap(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float startHz = (float)p.zap_pitch_start;
  const float endHz = (float)p.zap_pitch_end;
  const float decayMs = u8_to_ms_fast(p.zap_decay, 10.0f, 100.0f);
  const float accent = p.accent * 0.01f;
  const float spreadAmt = p.zap_spread * 0.01f;

  float phase_L = 0.0f, phase_R = 0.0f;
  const float totalSamples = SAMPLE_RATE * decayMs / 1000.0f;

  for (uint32_t i = 0; i < n; ++i) {
    const float pitch_t = (float)i / totalSamples;
    const float currentHz_L = startHz + (endHz - startHz) * pitch_t * (1.0f - spreadAmt * 0.05f);
    const float currentHz_R = startHz + (endHz - startHz) * pitch_t * (1.0f + spreadAmt * 0.05f);

    phase_L += 2.0f * (float)M_PI * currentHz_L / SAMPLE_RATE;
    if (phase_L >= 2.0f * (float)M_PI) phase_L -= 2.0f * (float)M_PI;
    phase_R += 2.0f * (float)M_PI * currentHz_R / SAMPLE_RATE;
    if (phase_R >= 2.0f * (float)M_PI) phase_R -= 2.0f * (float)M_PI;

    float sine_wave_L = sinf(phase_L);
    float sine_wave_R = sinf(phase_R);
    const float env = envPercExp(i, decayMs);

    float y_L = sine_wave_L * env * accent;
    float y_R = sine_wave_R * env * accent;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

FLASHMEM static inline void generateSnap(const NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  const float accent = p.accent * 0.01f;
  const float driveAmt = p.snap_drive * 0.01f;
  const float toneBal = p.snap_tone * 0.01f;
  const float baseFreq = (float)p.snap_pitch;
  const float decayMs = u8_to_ms_fast(p.snap_decay, 40.f, mode909 ? 150.f : 250.f);
  const float spreadAmt = p.snap_spread * 0.01f;
  float freqOffset = baseFreq * spreadAmt * 0.05f;

  float x1_L = 0.f, x2_L = 0.f, y1_L = 0.f, y2_L = 0.f;
  float x1_R = 0.f, x2_R = 0.f, y1_R = 0.f, y2_R = 0.f;
  float b0_L, b1_L, b2_L, a1_L, a2_L;
  float b0_R, b1_R, b2_R, a1_R, a2_R;
  {
    float Q = mode909 ? 2.5f : 6.0f;
    float w0_L = 2.f * (float)M_PI * (baseFreq - freqOffset) / SAMPLE_RATE;
    float w0_R = 2.f * (float)M_PI * (baseFreq + freqOffset) / SAMPLE_RATE;

    float cosw0_L = cosf(w0_L); float alpha_L = sinf(w0_L) / (2.f * Q);
    b0_L = alpha_L; b1_L = 0.f; b2_L = -alpha_L;
    float a0_L = 1.f + alpha_L; a1_L = -2.f * cosw0_L; a2_L = 1.f - alpha_L;
    b0_L /= a0_L; b1_L /= a0_L; b2_L /= a0_L; a1_L /= a0_L; a2_L /= a0_L;

    float cosw0_R = cosf(w0_R); float alpha_R = sinf(w0_R) / (2.f * Q);
    b0_R = alpha_R; b1_R = 0.f; b2_R = -alpha_R;
    float a0_R = 1.f + alpha_R; a1_R = -2.f * cosw0_R; a2_R = 1.f - alpha_R;
    b0_R /= a0_R; b1_R /= a0_R; b2_R /= a0_R; a1_R /= a0_R; a2_R /= a0_R;
  }

  for (uint32_t i = 0;i < n;++i) {
    float a = envPercExp(i, decayMs);
    float exc_L = (i < (uint32_t)(SAMPLE_RATE * 0.001f)) ? 1.f : 0.f;
    float exc_R = (i < (uint32_t)(SAMPLE_RATE * 0.001f)) ? 1.f : 0.f;

    float noise_L = 0.f, noise_R = 0.f;
    if (mode909 && i < (uint32_t)(SAMPLE_RATE * 0.003f)) {
      noise_L = ((rand() & 0x7fff) / 16384.f - 1.f) * 0.2f;
      noise_R = ((rand() & 0x7fff) / 16384.f - 1.f) * 0.2f;
    }

    float in_L = exc_L + noise_L;
    float in_R = exc_R + noise_R;
    float yn_L = b0_L * in_L + b1_L * x1_L + b2_L * x2_L - a1_L * y1_L - a2_L * y2_L;
    float yn_R = b0_R * in_R + b1_R * x1_R + b2_R * x2_R - a1_R * y1_R - a2_R * y2_R;
    x2_L = x1_L; x1_L = in_L; y2_L = y1_L; y1_L = yn_L;
    x2_R = x1_R; x1_R = in_R; y2_R = y1_R; y1_R = yn_R;

    float y_L = nm_mix(yn_L, noise_L, powf(toneBal, 2.0f) * 0.3f);
    float y_R = nm_mix(yn_R, noise_R, powf(toneBal, 2.0f) * 0.3f);
    y_L = nm_mix(y_L, nm_softclip(3.0f * y_L), driveAmt);
    y_R = nm_mix(y_R, nm_softclip(3.0f * y_R), driveAmt);
    y_L *= a * accent; y_R *= a * accent;

    if (stereo) {
      psram_buf[i * channels] = (int16_t)lrintf(nm_clip1(y_L) * 32767.f);
      psram_buf[i * channels + 1] = (int16_t)lrintf(nm_clip1(y_R) * 32767.f);
    }
    else {
      float mono_mix = (y_L + y_R) * 0.5f;
      psram_buf[i] = (int16_t)lrintf(nm_clip1(mono_mix) * 32767.f);
    }
  }
}

// Map uint8_t 0–100 into a squared/log curve in ms
inline float u8_to_ms(uint8_t value, float min_ms, float max_ms) {
  float normalized = value / 100.0f;
  return min_ms + powf(normalized, 2.0f) * (max_ms - min_ms);
}

//Synth

// ---------- Fast helpers ----------
inline float fast_tanhf(float x) { return tanhf(x); } // or a polynomial approx if needed

struct OnePoleSmooth {
  float z = 0.0f, alpha = 1.0f; // alpha ~ 1 - exp(-1/(tau*fs))
  void setTimeConstant(float tauSeconds, float sampleRate) {
    alpha = 1.0f - expf(-1.0f / (tauSeconds * sampleRate));
  }
  inline float process(float x) {
    z += alpha * (x - z);
    return z;
  }
};

// Map knob 0..100 to ~20..18000 Hz exponentially
FLASHMEM  inline float knob_to_cutoff_hz(uint8_t knob01_100) {
  const float minHz = 20.0f, maxHz = 18000.0f;
  float t = std::clamp(knob01_100 / 100.0f, 0.0f, 1.0f);
  // exponential between min and max
  return minHz * powf(maxHz / minHz, t);
}

// -----------------------------
// Utility
// -----------------------------
FLASHMEM inline float lerp(float a, float b, float t) {
  return a + t * (b - a);
}

// MIDI note to Hz
FLASHMEM inline float midi_to_hz(uint8_t midi_note) {
  return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

// -----------------------------
// PolyBLEP Oscillator
// -----------------------------
 inline float poly_blep(float t, float dt) {
  if (t < dt) {
    float x = t / dt;
    return x + x - x * x - 1.0f;
  }
  else if (t > 1.0f - dt) {
    float x = (t - 1.0f) / dt;
    return x * x + x + x + 1.0f;
  }
  else {
    return 0.0f;
  }
}

 inline float polyblep_saw(float& phase, float freq) {
  float dt = freq / SAMPLE_RATE;
  float t = phase;
  float value = 2.0f * t - 1.0f; // naive saw
  value -= poly_blep(t, dt);
  phase += dt;
  if (phase >= 1.0f) phase -= 1.0f;
  return value;
}

 inline float sine_osc(float& phase, float freq) {
  float dt = freq / SAMPLE_RATE;
  float value = sinf(2.0f * PI * phase);
  phase += dt;
  if (phase >= 1.0f) phase -= 1.0f;
  return value;
}

// pulse wave oscillator function
 inline float pulse_osc(float& phase, float freq, float duty_cycle) {
  float dt = freq / SAMPLE_RATE;
  float value = (phase < duty_cycle) ? 1.0f : -1.0f;
  // Add polyblep to remove aliasing
  value += poly_blep(phase, dt);
  phase += dt;
  if (phase >= 1.0f) phase -= 1.0f;
  return value;
}


// -----------------------------
// Exponential ADSR Envelope
// -----------------------------
struct ADSR {
  enum Stage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
  Stage stage = IDLE;
  float value = 0.0f;
  float attack_inc, decay_rate, release_rate;
  float sustain_level;

  void init(float attack_ms, float decay_ms, float sustain, float release_ms) {
    // Attack is linear and is a common approach.
    attack_inc = 1.0f / (attack_ms * 0.001f * SAMPLE_RATE);

    // Corrected exponential decay and release rates
    decay_rate = expf(-2.2f / (decay_ms * 0.001f * SAMPLE_RATE));
    release_rate = expf(-2.2f / (release_ms * 0.001f * SAMPLE_RATE));

    sustain_level = sustain;
    stage = ATTACK;
    value = 0.0f;
  }

  float process() {
    switch (stage) {
    case ATTACK:
      value += attack_inc;
      if (value >= 1.0f) {
        value = 1.0f;
        stage = DECAY;
      }
      break;
    case DECAY:
      // Exponential decay towards the sustain level
      value = sustain_level + (value - sustain_level) * decay_rate;
      if (value <= sustain_level) {
        value = sustain_level;
        stage = SUSTAIN;
      }
      break;
    case SUSTAIN:
      value = sustain_level;
      break;
    case RELEASE:
      // Exponential release towards 0.0
      value *= release_rate;
      if (value <= 0.001f) { // Use a small tolerance
        value = 0.0f;
        stage = IDLE;
      }
      break;
    case IDLE:
      value = 0.0f;
      break;
    }
    return value;
  }

  void noteOff() {
    if (stage == ATTACK || stage == DECAY || stage == SUSTAIN) {
      stage = RELEASE;
    }
  }

  void noteOn() {
    stage = ATTACK;
    value = 0.0f;
  }
};

// -----------------------------
// 4-Pole Ladder Filter (TPT)
// -----------------------------
class LadderFilter {
  float z[4] = { 0 };
  float cutoff, resonance;
  float g, R;

public:
  LadderFilter() : cutoff(1000.0f), resonance(0.1f) {}

  void set(float cutoff_hz, float reso) {
    cutoff = std::clamp(cutoff_hz, 10.0f, SAMPLE_RATE * 0.45f);
    resonance = std::clamp(reso, 0.0f, 4.0f);
    float wd = 2.0f * PI * cutoff;
    float T = 1.0f / SAMPLE_RATE;
    float wa = (2.0f / T) * tanf(wd * T / 2.0f); // bilinear transform
    g = wa * T / 2.0f;
    R = resonance;
  }

  float process(float input) {
    // feedback
    float u = (input - R * z[3]) / (1.0f + g);
    // 4 cascaded one-pole filters
    for (int i = 0; i < 4; i++) {
      z[i] = z[i] + g * (u - z[i]);
      u = z[i];
    }
    // soft saturation
    return tanh(u);
  }
};

// -----------------------------
// DC Blocker
// -----------------------------
struct DCBlocker {
  float R = 0.995f;
  float x1 = 0, y1 = 0;
  float process(float x) {
    float y = x - x1 + R * y1;
    x1 = x;
    y1 = y;
    return y;
  }
};

// -----------------------------
// Limiter
// -----------------------------
inline float soft_limit(float x) {
  return tanh(x);
}
FLASHMEM void generateSynth(const NoiseMakerParams& p, int16_t* psram_buffer, uint32_t n, bool mode909, uint8_t channels) {
  const bool stereo = (channels == 2);
  bool note_off_triggered = false; // Add a flag to track the trigger
  const float detune = (p.synth_detune / 100.0f) * 0.01f;
  const float spread_detune = (p.synth_spread / 100.0f) * 0.01f;

  float base_freq = midi_to_hz(p.synth_midi_note);
  float phase1 = 0.0f, phase2 = 0.0f, phase3 = 0.0f, phase4 = 0.0f;

  // ADSR envelopes
  ADSR amp_env, filter_env, noise_env;
  amp_env.init(
    u8_to_ms(p.synth_attack, 1.0f, 1000.0f),
    u8_to_ms(p.synth_decay, 1.0f, 1000.0f),
    p.synth_sustain_level / 100.0f,
    u8_to_ms(p.synth_release, 1.0f, 1000.0f)
  );

  noise_env.init(
    u8_to_ms(0, 1.0f, 1000.0f),  // Attack time of 0ms
    u8_to_ms(p.synth_noise_decay, 1.0f, 1000.0f), // Decay time from parameter
    0.0f,                       // Sustain level of 0
    u8_to_ms(0, 1.0f, 1000.0f)   // Release time of 0ms
  );

  filter_env.init(
    u8_to_ms(p.synth_filter_env_attack, 1.0f, 1000.0f),
    u8_to_ms(p.synth_filter_env_decay, 1.0f, 1000.0f),
    p.synth_sustain_level / 100.0f,
    u8_to_ms(p.synth_filter_env_release, 1.0f, 1000.0f)
  );

  LadderFilter filterL, filterR;
  DCBlocker dcL, dcR;

  const float pulse_width_base = p.synth_pulse_width / 100.0f;
  const float sweep_range = 0.3f; // A fixed sweep amount, e.g., +/- 20%
  const float start_duty = std::clamp(pulse_width_base - sweep_range, 0.0f, 1.0f);
  const float end_duty = std::clamp(pulse_width_base + sweep_range, 0.0f, 1.0f);

  for (uint32_t i = 0; i < n; i++) {

    if (!note_off_triggered && i > n * 0.66f) { // for example, release at 66% of the way through the buffer
      amp_env.noteOff();
      filter_env.noteOff();
      noise_env.noteOff();
      note_off_triggered = true;
    }

    // Envelopes
    float envA = amp_env.process();
    float envF = filter_env.process();
    float current_duty = lerp(start_duty, end_duty, envA);

    // Stereo spread
    float spreadL = 1.0f - spread_detune;
    float spreadR = 1.0f + spread_detune;

    // Oscillators
    float freq1 = base_freq * spreadL;
    float freq2 = base_freq * (1.0f + detune) * spreadR;
    float freq3 = base_freq * (1.0f - detune) * spreadL;
    float freq4 = base_freq * 2.0f * spreadR;   // octave-up osc

    float oscL_base = 0.0f;
    float oscR_base = 0.0f;
    float osc_octave = 0.0f;

    // Waveform selector using switch statement
    // You'll need to define synth_waveform_select and synth_pulse_width in NoiseMakerParams
    // This example maps 0-100 to four discrete choices
    switch (p.synth_waveform) {
    case 0:
      // Default Waveform
    {
      float saw1 = polyblep_saw(phase1, freq1);
      float saw2 = polyblep_saw(phase2, freq2);
      float saw3 = polyblep_saw(phase3, freq3);
      float saw4 = polyblep_saw(phase4, freq4);

      float combinedL = (saw1 + saw3) * 0.5f;
      float combinedR = (saw2 + saw4) * 0.5f;

      oscL_base = (1.0f - p.synth_waveform_mix / 100.0f) * sine_osc(phase1, freq1) + (p.synth_waveform_mix / 100.0f) * combinedL;
      oscR_base = (1.0f - p.synth_waveform_mix / 100.0f) * sine_osc(phase2, freq2) + (p.synth_waveform_mix / 100.0f) * combinedR;

      // Octave is a saw wave for this default case
      osc_octave = saw4;
    }
    break;
    case 1:
      // Sine wave only
      oscL_base = sine_osc(phase1, freq1);
      oscR_base = sine_osc(phase2, freq2);

      // Octave is also a sine wave
      osc_octave = sine_osc(phase4, freq4);
      break;

    case 2:
      // Sawtooth wave only
      oscL_base = polyblep_saw(phase1, freq1);
      oscR_base = polyblep_saw(phase2, freq2);

      // Octave is also a sawtooth wave
      osc_octave = polyblep_saw(phase4, freq4);
      break;
    case 3:
      // Pulse wave with animated PWM
    {
      oscL_base = pulse_osc(phase1, freq1, current_duty);
      oscR_base = pulse_osc(phase2, freq2, current_duty);
      osc_octave = pulse_osc(phase4, freq4, current_duty);
    }

    break;
    }

    // Octave mix is now applied to the base waveform and the octave-up version
    float oct_mix = p.synth_octave_mix / 100.0f;
    float oscL = (1.0f - oct_mix) * oscL_base + oct_mix * osc_octave;
    float oscR = (1.0f - oct_mix) * oscR_base + oct_mix * osc_octave;

    float mixL = oscL;
    float mixR = oscR;

    // --- New: Noise oscillator with envelope ---
    float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f; // white noise [-1,1]
    float noise_level = p.synth_noise_level / 100.0f;
    float envN = noise_env.process();

    // The noise signal is modulated by its envelope.
    float enveloped_noise = noise * noise_level * envN;

    mixL += enveloped_noise;
    mixR += enveloped_noise;

    mixL *= envA;
    mixR *= envA;

    // Filter cutoff
    float user_cutoff = 20.0f * powf(2.0f, (p.synth_filter_cutoff / 100.0f) * 10.0f);
    float cutoff = user_cutoff + envF * (p.synth_filter_env_amount / 400.0f) * (18000.0f - user_cutoff);
    float resonance = (p.synth_filter_resonance / 100.0f) * 4.5f;

    filterL.set(cutoff, resonance);
    filterR.set(cutoff, resonance);

    float outL = filterL.process(dcL.process(mixL));
    float outR = filterR.process(dcR.process(mixR));

    // Apply soft limiting
    outL = soft_limit(outL * 2.0f);
    outR = soft_limit(outR * 2.0f);

    // Write to buffer
    if (stereo) {
      psram_buffer[i * channels] = static_cast<int16_t>(std::clamp(outL, -1.0f, 1.0f) * 32767.0f);
      psram_buffer[i * channels + 1] = static_cast<int16_t>(std::clamp(outR, -1.0f, 1.0f) * 32767.0f);
    }
    else {
      float mono = 0.5f * (outL + outR);
      psram_buffer[i] = static_cast<int16_t>(std::clamp(mono, -1.0f, 1.0f) * 32767.0f);
    }
  }
}

FLASHMEM static inline uint32_t getSampleLength(const NoiseMakerParams& p) {
  float lenMs = 0.0f;
  switch (p.category) {
  case NM_BD: {
    float decayMs = u8_to_ms_fast(p.bd_decay, 60.f, 800.f);
    float releaseMs = u8_to_ms_fast(p.bd_release, 20.f, 600.f);
    float pitchTail = (float)p.bd_pitch_decay * (p.mode == NM_MODE_TR909 ? 0.5f : 1.0f);
    lenMs = decayMs + releaseMs + pitchTail;
    lenMs = std::max(120.0f, lenMs);
  } break;

  case NM_SD: {
    float d = u8_to_ms_fast(p.sn_decay, 60.f, 900.f);
    lenMs = d + 80.0f;
  } break;

  case NM_TOM: {
    float d = u8_to_ms_fast(p.tom_decay, 80.f, 1200.f);
    lenMs = d + 40.0f;
  } break;

  case NM_CONGA: {
    float d = u8_to_ms_fast(p.conga_decay, 80.f, 1000.f);
    lenMs = d + 20.0f;
  } break;

  case NM_SNAP: {
    float d = u8_to_ms_fast(p.snap_decay, 10.f, 200.f);
    lenMs = d + 10.0f;
  } break;

  case NM_CLAP: {
    float d = u8_to_ms_fast(p.clap_decay, 120.f, 900.f);
    float spreadMs = map01_100(p.clap_spread, 8.f, 24.f);
    lenMs = d + spreadMs + 40.0f;
  } break;

  case NM_HH: {
    if (p.hh_mode == 1) { // open hat
      float d = u8_to_ms_fast(p.hh_decay, 500.f, 1200.f);
      lenMs = d + 40.0f;
    }
    else {
      float d = u8_to_ms_fast(p.hh_decay, 80.f, 250.f);
      lenMs = d + 20.0f;
    }
  } break;

  case NM_CRASH: {
    lenMs = u8_to_ms_fast(p.crash_decay, 1000.f, 4000.f);
    lenMs = std::max(lenMs, 1000.f);
    lenMs += 60.f;

  }break;
  case NM_RIDE: {
    lenMs = u8_to_ms_fast(p.ride_decay, 1500.f, 5000.f);
    lenMs = std::max(lenMs, 1500.f);
    lenMs += 60.f;

  }break;

  case NM_COWBELL: {
    float d = u8_to_ms_fast(p.cb_amp_decay, 100.f, 500.f);
    lenMs = d + 10.0f;
  } break;

  case NM_CLAVES: {
    float d = u8_to_ms_fast(p.claves_decay, 20.f, 300.f);
    lenMs = d + 10.0f;
  } break;

  case NM_MARACAS: {
    float d = u8_to_ms_fast(p.mar_decay, 40.f, 160.f);  // 808 is short & crisp
    lenMs = d + 10.0f;
  } break;

  case NM_SYNTH: {
    float attack_ms = u8_to_ms(p.synth_attack, 10.0f, 1000.0f);
    float decay_ms = u8_to_ms(p.synth_decay, 10.0f, 1000.0f);
    float release_ms = u8_to_ms(p.synth_release, 10.0f, 1000.0f);

    // The sustain stage is a level, not a time. We add a fixed duration
    // to ensure the note plays long enough to reach the release stage.
    float sustain_duration_ms = 1000.0f;

    lenMs = attack_ms + decay_ms + sustain_duration_ms + release_ms;
  } break;


  default: lenMs = 500.0f; break;
  }

  // Safety clamp in ms (float)
  lenMs = std::min<float>(lenMs, 5000.0f); // max 5s
  lenMs = std::max<float>(lenMs, 32.0f);   // min 32ms

  // Convert to samples (rounded)
  return msToSamples(lenMs);
}

// ============================================================================
// DISPATCHER (call with precomputed n)
// ============================================================================
FLASHMEM static inline void generateDrum(NoiseMakerParams& p, int16_t* psram_buf, uint32_t n, uint8_t channels) {
  switch (p.category) {
  case NM_BD:      generateBassdrum(p, psram_buf, n, p.mode, channels); break;
  case NM_SD:      generateSnare(p, psram_buf, n, p.mode, channels); break;
  case NM_TOM:      generateTom(p, psram_buf, n, p.mode, channels); break;
  case NM_CONGA:      generateConga(p, psram_buf, n, p.mode, channels); break;
  case NM_RIM:     generateRimshot(p, psram_buf, n, p.mode, channels); break;
  case NM_CLAP:    generateClap(p, psram_buf, n, p.mode, channels); break;
  case NM_HH:      generateHiHat(p, psram_buf, n, p.mode, channels); break;
  case NM_CRASH:   generateCrash(p, psram_buf, n, p.mode, channels); break;
  case NM_RIDE:    generateRide(p, psram_buf, n, p.mode, channels); break;
  case NM_COWBELL: generateCowbell(p, psram_buf, n, p.mode, channels); break;
  case NM_CLAVES:  generateClaves(p, psram_buf, n, p.mode, channels); break;
  case NM_MARACAS: generateMaracas(p, psram_buf, n, p.mode, channels); break;
  case NM_ZAP: generateZap(p, psram_buf, n, p.mode, channels); break;
  case NM_SNAP:     generateSnap(p, psram_buf, n, p.mode, channels); break;
  case NM_SYNTH:     generateSynth(p, psram_buf, n, p.mode, channels); break;
  default: // silence
    for (uint32_t i = 0;i < n;++i) psram_buf[i] = 0;
    break;
  }
}

// Update generateNoisePreview to handle stereo
FLASHMEM void generateNoisePreview() {

  // Compute how many samples we need for the current instrument
  uint32_t length = getSampleLength(nm_params);
  if (length == 0) return;

  // Determine channel count
  uint8_t channels = (nm_params.stereo_mode == 0) ? 1 : 2;
  if (channels == 2)
    length = length + ((length / 100) * 110);
  // Allocate or reallocate the PSRAM preview buffer
  if (!nm_preview_alloc(length, channels)) return;

  // Generate the waveform directly into the PSRAM buffer
  generateDrum(nm_params, nm_preview_data, length, channels);
}

extern void print_nm_style();


// Curated rimshot randomization (usable 808/909-ish settings)
FLASHMEM void randomizeRimshotParams() {
  // Decay
  nm_params.rim_decay = nm_params.mode ? random(25, 65) : random(40, 80);

  // Tone balance (mid-heavy, occasional extremes)
  int t = random(0, 100);
  if (t < 15) nm_params.rim_tone = random(20, 40);
  else if (t < 80) nm_params.rim_tone = random(45, 80);
  else nm_params.rim_tone = random(81, 100);

  // Base pitch
  nm_params.rim_pitch = nm_params.mode ? random(1800, 2600) : random(1200, 2000);

  // Modal frequency (slightly inharmonic)
  int ratio = random(135, 170);
  nm_params.rim_mod_freq = (nm_params.rim_pitch * ratio) / 100;

  // Noise filter center
  nm_params.rim_noise_bp_freq = random(2800, 4200);

  // Drive
  int d = random(0, 100);
  if (d < 60) nm_params.rim_drive = random(2, 10);
  else if (d < 90) nm_params.rim_drive = random(11, 18);
  else nm_params.rim_drive = random(19, 21);

  // Stereo spread
  int s = random(0, 100);
  if (s < 70) nm_params.rim_spread = random(0, 25);
  else if (s < 95) nm_params.rim_spread = random(26, 60);
  else nm_params.rim_spread = random(61, 100);

  // -------------------------
  // New click + noise level parameters
  nm_params.rim_click_level = random(40, 80); // sine burst amplitude
  nm_params.rim_noise_level = random(20, 60); // filtered noise amplitude
}

extern void print_noisemaker_synth_wave();

FLASHMEM void randomizeNoiseMakerParams() {

  if (nm_params.randomizer)
  {
    // Randomize mode (808 or 909)
    nm_params.mode = random(0, 2);
  }
  print_nm_style();
  // Randomize accent level
  //nm_params.accent = 40;

  randomizeRimshotParams();

  // Bass Drum parameters
  if (nm_params.mode == NM_MODE_TR909)
    nm_params.bd_pitch = random(10, 80);        // 30-79 Hz
  else
    nm_params.bd_pitch = random(10, 60);        // 30-79 Hz

  nm_params.bd_pitch_env = random(20, 95);    // 0-100%
  nm_params.bd_pitch_decay = random(5, 95); // 100-500 ms
  nm_params.bd_decay = random(10, 60);       // 30-100%
  nm_params.bd_attack = random(0, 1);        // 0-10%
  nm_params.bd_release = random(5, 70);     // 10-100%
  nm_params.bd_tone = random(20, 80);        // 50-100%
  nm_params.bd_click = random(0, 81);         // 0-50%
  nm_params.bd_noise = random(0, 31);         // 0-20%
  nm_params.bd_drive = random(0, 31);         // 0-30%
  nm_params.bd_comp = random(0, 21);          // 0-20%
  nm_params.bd_spread = random(0, 101);       // 0-100%

  // Snare parameters
  // The first tone is the fundamental body, with a focused range for more musical sounds.
  nm_params.sn_tone1 = random(150, 350);

  // The second tone is the higher harmonic, with a range that complements the first.
  nm_params.sn_tone2 = random(250, 500);

  // The mix between the two tones should be broad to allow for various timbres.
  nm_params.sn_tone_mix = random(20, 101);

  // A noticeable pitch drop is key for a percussive attack.
  nm_params.sn_pitch_env = random(50, 101);

  // Decay provides a wide range from tight to ringing snares.
  nm_params.sn_decay = random(40, 101);

  // Noise level is crucial for the sizzle; a high range is suitable.
  nm_params.sn_noise = random(40, 99);

  // The noise filter frequency is now more focused on the high-end.
  nm_params.sn_bp_freq = random(1500, 6000);

  // The Q factor is set for a more resonant, musical sound.
  nm_params.sn_bp_q = random(0, 25);

  // The snap transient provides the initial punch.
  nm_params.sn_snap = random(0, 101);

  // Drive adds subtle soft clipping for grit.
  nm_params.sn_drive = random(0, 21);
  nm_params.sn_spread = random(0, 101);       // 0-100%

  // Tom parameters
  nm_params.tom_pitch = random(60, 301);      // 80-300 Hz
  nm_params.tom_decay = random(10, 30);      // 40-100%
  nm_params.tom_pitch_env = random(0, 80);   // 0-100%
  nm_params.tom_tone = random(50, 101);       // 50-100%
  nm_params.tom_noise = random(0, 21);        // 0-20%
  nm_params.tom_spread = random(0, 101);      // 0-100%

  // Conga parameters
  nm_params.conga_pitch = random(180, 501);  // 180–300 Hz base
  nm_params.conga_decay = random(10, 40);   // 40–100%
  nm_params.conga_tone = random(30, 100);   // 30–100%
  nm_params.conga_pitch_env = random(0, 51);     // 0–40% (subtle bend)
  nm_params.conga_noise = random(0, 81);    // 10–60% (woodiness)
  nm_params.conga_spread = random(0, 101);     // 0-100%

  // nm_params.rim_decay = random(20, 81);
  //   nm_params.rim_tone = random(20, 101);
  //   nm_params.rim_pitch = random(1000, 2501); // Adjusted pitch range for resonator
  //   nm_params.rim_mod_freq = random(800, 1501); 
  //   nm_params.rim_noise_bp_freq = random(3000, 6001); // Randomize new parameter
  //   nm_params.rim_drive = random(0, 21);
  //   nm_params.rim_spread = random(0, 101);

    // Clap parameters
  nm_params.clap_repeats = random(2, 8);      // 2-5 repeats
  nm_params.clap_spread = random(20, 101);    // 20-100%
  nm_params.clap_tail = random(40, 100);      // 40-100%
  nm_params.clap_bp_hz = random(90, 370);   // 200-2000 Hz
  nm_params.clap_bp_q = random(15, 50);      // 50-100%
  nm_params.clap_decay = random(50, 101);     // 50-100%
  nm_params.clap_drive = random(0, 21);       // 0-20%

  nm_params.hh_mode = random(0, 2);        // 0=closed, 1=open
  nm_params.hh_tone = random(30, 50);      // metallic %
  nm_params.hh_noise = random(40, 70);      // noise %
  nm_params.hh_bp_hz = random(2500, 8000);  // bandpass center Hz
  nm_params.hh_bp_q = random(6, 12);       // bandpass Q
  nm_params.hh_decay = random(50, 80);      // decay %
  nm_params.hh_drive = random(0, 20);       // drive %
  nm_params.hh_detune = random(2, 5);        // oscillator detune
  nm_params.hh_spread = random(0, 101);       // 0-100%

  // Crash parameters
  nm_params.crash_tone = random(40, 101);     // 40-100%
  nm_params.crash_bp_hz = random(5000, 8001); // 5000-8000 Hz
  nm_params.crash_bp_q = random(0, 50);     // 40-100%
  nm_params.crash_decay = random(70, 101);    // 70-100%
  nm_params.crash_spread = random(0, 101);    // 0-100%

  // Ride parameters
  nm_params.ride_tone = random(40, 101);      // 40-100%
  nm_params.ride_bp_hz = random(6000, 9001);  // 6000-9000 Hz
  nm_params.ride_bp_q = random(0, 50);      // 50-100%
  nm_params.ride_decay = random(80, 101);     // 80-100%
  nm_params.ride_spread = random(0, 101);     // 0-100%

  // Cowbell parameters
  nm_params.cb_freq1 = random(440, 680);
  nm_params.cb_freq2 = random(700, 1040);
  nm_params.cb_amp_attack = 0;
  nm_params.cb_amp_decay = random(25, 245);
  nm_params.cb_tone_balance = random(20, 80);
  nm_params.cb_noise = random(0, 10);
  nm_params.cb_drive = random(0, 60);
  nm_params.cb_spread = random(0, 101);      // 0-100%

  // Claves parameters
  nm_params.claves_hz = random(1500, 2001);   // 1500-2000 Hz
  nm_params.claves_decay = random(20, 101);   // 20-100%
  nm_params.claves_drive = random(0, 11);     // 0-10%
  nm_params.claves_spread = random(0, 101);    // 0-100%

  // Maracas parameters
  nm_params.mar_bp_hz = random(2000, 6001);  // 1–6 kHz
  nm_params.mar_bp_q = random(50, 95);     // 50–100%
  nm_params.mar_decay = random(20, 71);      // 30–80%
  nm_params.mar_spread = random(0, 101);      // 0-100%

  nm_params.zap_pitch_start = random(4000, 10000); // Varies the starting pitch of the sweep
  nm_params.zap_pitch_end = random(100, 800);      // Changes the ending pitch, from a sharp zap to a bassy thud
  nm_params.zap_decay = random(10, 100);       // Randomizes the length of the sound
  nm_params.zap_spread = random(0, 101);      // 0-100%

  nm_params.snap_decay = random(20, 81);      // 20–80% decay
  nm_params.snap_tone = random(20, 101);     // 20–100% (tone vs noise)
  nm_params.snap_pitch = random(200, 2801);  // 1.2–2.8 kHz
  nm_params.snap_drive = random(0, 21);       // 0–20% drive
  nm_params.snap_spread = random(0, 101);       // 0-100%

  // Synth Randomization
  //nm_params.synth_midi_note = random(36, 49);        // C2 to C3

  nm_params.synth_waveform = random(0, 4);
  print_noisemaker_synth_wave();
  nm_params.synth_pulse_width = random(0, 99);
  nm_params.synth_attack = 0;           // 0-15%
  nm_params.synth_decay = random(35, 95);            // 5-30%
  nm_params.synth_sustain_level = 0;   // 10-80%
  nm_params.synth_release = 0;        // 10-50%
  nm_params.synth_filter_cutoff = random(0, 99);   // 10-40%
  nm_params.synth_filter_resonance = random(0, 99);  // 10-60%
  nm_params.synth_filter_env_amount = random(0, 91); // 30-80%
  nm_params.synth_filter_env_attack = 0; // 0-10%
  nm_params.synth_filter_env_decay = random(5, 51);  // 5-40%
  nm_params.synth_filter_env_release = random(0, 5); // 0-20%
  nm_params.synth_waveform_mix = random(20, 101);   // 80-100%
  nm_params.synth_octave_mix = random(0, 81);       // 0-20%
  nm_params.synth_spread = random(0, 51);           // 0-20%
  nm_params.synth_detune = random(0, 99);
  nm_params.synth_noise_level = random(0, 99);

}
#endif
