#ifdef COMPILE_FOR_PSRAM

#ifndef _NOISEMAKER_H_
#define _NOISEMAKER_H_


enum {
  NM_MODE_TR808,
  NM_MODE_TR909
};

enum {
  NM_BD = 0,
  NM_SD,
  NM_TOM,
  NM_CONGA,
  NM_RIM,
  NM_CLAP,
  NM_HH,
  NM_CRASH,
  NM_RIDE,
  NM_COWBELL,
  NM_CLAVES,
  NM_MARACAS,
  NM_ZAP,
  NM_SNAP,
  NM_SYNTH,
  NM_CATEGORY_COUNT
};

// ------------------------ Parameter Structure -------------------------------
struct NoiseMakerParams {
  uint8_t category;
  bool  mode = 0;         // NM_MODE_TR808 or NM_MODE_TR909
  bool  randomizer;
  bool stereo_mode;  // 0: mono, 1: stereo
  uint8_t stereo_width; // 0-100% stereo width
  bool reverb_enable;
  uint8_t reverb_room_size;  // 0-100 mapped to 0.0-1.0
  uint8_t reverb_damping;    // 0-100 mapped to 0.0-1.0
  uint8_t reverb_wet;        // 0-100 mapped to 0.0-1.0
  uint8_t reverb_dry;        // 0-100 mapped to 0.0-1.0
  uint8_t reverb_width;      // 0-100 mapped to 0.0-1.0

  // -------- Accent / global gain ----------
  uint8_t  accent;       // % scales final level

  // -------- Bass Drum --------
  uint8_t bd_pitch;         // Hz (base)
  uint8_t  bd_pitch_env;     // % amount (maps to startRatio)
  uint16_t bd_pitch_decay;   // ms (extra tail consideration)
  uint8_t  bd_attack;        // % -> ms
  uint8_t  bd_decay;         // % -> ms
  uint8_t  bd_release;       // % -> ms
  uint8_t  bd_tone;          // % sine<->softclip
  uint8_t  bd_click;         // % click level
  uint8_t  bd_noise;         // % tail noise
  uint8_t  bd_drive;         // %
  uint8_t  bd_comp;          // %
  uint8_t  bd_spread;        // % stereo spread

  // -------- Snare --------
  uint16_t sn_tone1;         // Hz (body osc 1)
  uint16_t sn_tone2;         // Hz (body osc 2)
  uint8_t  sn_tone_mix;      // % osc mix
  uint8_t  sn_pitch_env;     // % depth for tone drop
  uint8_t  sn_decay;         // % -> ms
  uint8_t  sn_noise;         // % noise level
  uint16_t sn_bp_freq;       // Hz bandpass for noise
  uint8_t  sn_bp_q;          // Q (0..100)
  uint8_t  sn_snap;          // % short click burst
  uint8_t  sn_drive;         // %
  uint8_t  sn_spread;        // % stereo spread

  // -------- Toms (808) / Toms (909) --------
  uint16_t tom_pitch;     // Hz
  uint8_t  tom_decay;        // % -> ms
  uint8_t  tom_pitch_env;    // % slight drop
  uint8_t  tom_tone;         // % sine<->softclip
  uint8_t  tom_noise;        // % attack noise
  uint8_t tom_spread;        // % stereo spread

  uint16_t conga_pitch;  // base pitch (~Low conga)
  uint8_t conga_decay;   // envelope length
  uint8_t conga_tone;   // sine ↔ woody balance
  uint8_t conga_pitch_env;   // pitch drop at onset
  uint8_t conga_noise;   // noise contribution
  uint8_t conga_spread;      // % stereo spread

  uint8_t  rim_decay;
  uint8_t  rim_tone;
  uint16_t rim_pitch;  // Base tone frequency in Hz
  uint16_t rim_mod_freq; // Ring modulation frequency in Hz
  uint16_t rim_noise_bp_freq; // New: Bandpass filter freq for noise in Hz
  uint8_t  rim_drive;
  uint8_t rim_click_level; // 0–100, controls sine burst amplitude
  uint8_t rim_noise_level; // 0–100, controls noise layer amplitude
  uint8_t  rim_spread;

  // -------- Clap --------
  uint8_t  clap_repeats;     // 2..4 typical
  uint8_t  clap_spread;      // % (delay between bursts)
  uint8_t  clap_tail;        // % tail amount
  uint16_t clap_bp_hz;       // Hz
  uint8_t  clap_bp_q;        // Q 0..100
  uint8_t  clap_decay;       // % -> ms
  uint8_t  clap_drive;       // %

  // -------- Hi-Hats --------
  uint8_t  hh_mode;        // 0=closed, 1=open
  uint8_t  hh_tone;        // % metallic vs noise
  uint8_t  hh_noise;       // % noise level
  uint16_t hh_bp_hz;       // Bandpass center frequency (Hz)
  uint8_t  hh_bp_q;        // Bandpass resonance Q
  uint8_t  hh_decay;       // Base decay % (mapped to ms inside generator)
  uint8_t  hh_drive;       // Softclip / drive %
  uint8_t  hh_detune;      // Detune between oscillators
  uint8_t hh_spread;       // % stereo spread

  // -------- Cymbals --------
  uint8_t  crash_tone;       // % metallic vs noise
  uint16_t crash_bp_hz;      // Hz
  uint8_t  crash_bp_q;       // Q
  uint8_t  crash_decay;      // % -> ms
  uint8_t  crash_spread;      // % stereo spread
  uint8_t  ride_tone;        // %
  uint16_t ride_bp_hz;       // Hz
  uint8_t  ride_bp_q;        // Q
  uint8_t  ride_decay;       // % -> ms
  uint8_t ride_spread;       // % stereo spread

  // -------- Cowbell (808) --------
  uint16_t cb_freq1;        // First frequency component (Hz)
  uint16_t cb_freq2;        // Second frequency component (Hz)
  uint8_t  cb_amp_attack;   // Attack time (%)
  uint8_t  cb_amp_decay;    // Decay time (%)
  uint8_t  cb_tone_balance; // Balance between the two frequencies
  uint8_t  cb_noise;        // Noise component (%)
  uint8_t  cb_drive;        // Saturation amount (%)
  uint8_t cb_spread;        // % stereo spread

  // -------- Claves (808) --------
  uint16_t claves_hz;        // Hz
  uint8_t  claves_decay;     // % -> ms
  uint8_t  claves_drive;     // %
  uint8_t claves_spread;     // % stereo spread

  // -------- Maracas (808) --------
  uint16_t mar_bp_hz;   // Hz
  uint8_t  mar_bp_q;    // Q
  uint8_t  mar_decay;   // % -> ms
  uint8_t  mar_spread;   // % stereo spread

  // -------- Zap --------
  uint16_t zap_pitch_start; // Starting pitch of the sweep (Hz)
  uint16_t zap_pitch_end;   // Ending pitch of the sweep (Hz)
  uint8_t  zap_decay;       // Decay time (%)
  uint8_t zap_spread;        // % stereo spread

    uint8_t  snap_decay;      // 20–80%
  uint8_t  snap_tone;     // 20–100% (sine ↔ noise)
  uint16_t snap_pitch;  // 1200–2800 Hz (woody ↔ metallic)
  uint8_t  snap_drive;       // 0–20%
  uint8_t  snap_spread;       // % stereo spread

  uint8_t synth_midi_note;
   uint8_t synth_pulse_width;
   uint8_t synth_waveform;
  uint8_t synth_attack;
  uint8_t synth_decay;
  uint8_t synth_sustain_level;
  uint8_t synth_release;
  uint8_t synth_filter_cutoff;
  uint8_t synth_filter_resonance;
  uint8_t synth_filter_env_amount; 
  uint8_t synth_filter_env_attack; 
  uint8_t synth_filter_env_decay;  
  uint8_t synth_filter_env_release; 
  uint8_t synth_waveform_mix; 
  uint8_t synth_octave_mix; 
  uint8_t synth_spread;
  uint8_t synth_detune;
  uint8_t synth_noise_level;
  uint8_t synth_noise_decay;
};

void generateAndPlayNoise();
void saveNoiseToCustomSlot();
void generateNoisePreview();
void saveNoiseToCustomSlot();
void PlaySlot();

// ==================================================
// Constants
// ==================================================

extern FLASHMEM void initNoiseMakerParams(NoiseMakerParams& p);
extern void randomizeNoiseMakerParams();

extern int16_t* nm_preview_data;
extern uint32_t nm_preview_len;
extern uint8_t noisemaker_custom_slot;
extern FLASHMEM void setupNoiseMakerDefaults(NoiseMakerParams& p);
extern FLASHMEM void addAutoUpdateParameterEditor_uint8_t(const char* name, uint8_t minv, uint8_t maxv, uint8_t* value);
extern FLASHMEM void addAutoUpdateParameterEditor_uint16_t(const char* name, uint16_t minv, uint16_t maxv, uint16_t* value);
extern FLASHMEM void nm_preview_free();

// Forward declarations
void drawNoiseMakerWaveform(bool fullRedraw);
void handle_touchscreen_noisemaker();
// External variables
extern const char* noisemaker_category_names[];
extern const char* noisemaker_short_category_names[];
extern  NoiseMakerParams nm_params;

#endif //_NOISEMAKER_H_
#endif
