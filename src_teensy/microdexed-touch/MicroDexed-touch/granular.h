// ==================== granular.h ====================
#ifndef GRANULAR_H
#define GRANULAR_H

#ifdef GRANULAR

#include <Arduino.h>
#include "AudioStream.h"

// ---------- USER TUNABLE ----------
#define MAX_POLY_VOICES 6
#define MAX_GRAINS_PER_VOICE 8
// ----------------------------------

// ---------- Play Mode Constants ----------
#define PLAYMODE_FORWARD  0
#define PLAYMODE_REVERSE  1
#define PLAYMODE_RANDOM   2
// ------------------------------------------

// ---------- Parameter struct ----------
typedef struct {
  uint8_t grain_size;      // 0..100 -> ms mapping inside voice
  uint8_t grain_position;  // 0..100
  uint8_t semitones;       // 0..100 where 50 == 0 semitone shift
  uint8_t volume;          // 0..100
  uint8_t density;         // 0..100
  uint8_t spread;          // 0..100
  uint8_t sample_note;

  uint8_t rev_send;
  uint8_t filter_mode;
  uint8_t filter_freq;
  uint8_t filter_resonance;
  uint8_t delay_send_1;
  uint8_t delay_send_2;
  int8_t pan;
  uint8_t attack;
  uint8_t release;
  uint8_t play_mode;       // 0=Forward, 1=Reverse, 2=Random
  uint8_t midi_channel;

  bool active;

} granular_params_t;

// ---------- Grain ----------
struct GranularGrain {
  bool active;
  const int16_t* sampleData;
  uint32_t sampleLength;
  float position;      // fractional position in samples
  float pitchRatio;    // increment per sample (can be negative for reverse)
  uint32_t size;       // grain size in samples
  float invSize;
  float volume;        // 0..1 per-grain
  uint32_t age;        // samples played
  bool reverse;        // true if this grain plays backwards
};

// ---------- Chromatic granular voice (one voice) ----------
class ChromaticGranularVoice {
public:
  ChromaticGranularVoice();
  ~ChromaticGranularVoice();

  void init();
  void setParams(const granular_params_t& params);
  void setSampleData(const int16_t* data, uint32_t sampleFrames);
  void setSampleDataBytes(const int16_t* data, uint32_t bytes, uint8_t numChannels = 1);
  void triggerNote(uint8_t note, uint8_t velocity);
  void noteOff();
  bool isPlaying() const;
  bool isActiveFor(uint8_t note) const;

  void mixInto(float* leftFloat, float* rightFloat, uint16_t blockSize);
  int getActiveGrainCount() const;
  void stopAllGrains();
  GranularGrain grains[MAX_GRAINS_PER_VOICE];
private:
  void startGrain(const granular_params_t& params);

  const int16_t* sampleData;
  uint32_t sampleLength; // frames (mono)

  bool noteOn;

  uint8_t currentNote;
  uint8_t currentVelocity;

  bool envelopeActive;
  float envelopeLevel;
  uint32_t envelopeTimeCounter;
  bool inAttack;
  uint32_t attackStartTime;
  uint32_t releaseStartTime;
  bool inRelease;

  granular_params_t currentParams;

  float movingPosition;
  uint16_t randState;
  float perNotePitchShift;  // Per-note pitch adjustment

  // Add voice-specific timing offset
  uint32_t voiceTimeOffset;

  // New organic variation members
  float voiceAmpContour[4];      // Attack, decay, sustain, release curve variations
  float voiceGrainBias[3];       // Position, size, density bias per voice  

};

// ---------- Polyphonic manager (AudioStream) ----------
class AudioChromaticGranularPoly : public AudioStream {
public:
  AudioChromaticGranularPoly();
  ~AudioChromaticGranularPoly();

  void begin();
  void setParams(const granular_params_t& params);
  void setSampleData(const int16_t* data, uint32_t sampleFrames);
  void setSampleDataBytes(const int16_t* data, uint32_t bytes, uint8_t numChannels = 1);
  void triggerNote(uint8_t note, uint8_t velocity);
  void noteOff(uint8_t note);
  void stopAllVoices();
  int getActiveVoiceCount() const;
  int getActiveGrainCount() const;

  virtual void update(void); // AudioStream callback
  ChromaticGranularVoice* voices[MAX_POLY_VOICES];

private:

  granular_params_t sharedParams;
  uint32_t polyRandSeed;
};

#endif // GRANULAR
#endif // GRANULAR_H