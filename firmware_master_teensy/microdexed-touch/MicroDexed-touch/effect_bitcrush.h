
#if defined BITCRUSH

#ifndef AudioEffectBitcrush_h
#define AudioEffectBitcrush_h

#include <Arduino.h>
#include <AudioStream.h>

class AudioEffectBitcrush : public AudioStream {
public:
  AudioEffectBitcrush();
  
  virtual void update(void);
  
  // Set bit crushing intensity (0.0 = clean, 1.0 = maximum bit/sample rate reduction)
  void setDrive(float drive);
  
  // Manual output gain adjustment (0.0 to 2.0)
  void setOutputGain(float gain);
  
  // Get current settings
  float getDrive() const { return _drive; }
  float getOutputGain() const { return _outputGain; }

private:
  audio_block_t *inputQueueArray[1];
  float _drive;            // Controls bit depth reduction (0.0 = 16-bit, 1.0 = 1-bit)
  float _outputGain;       // Output volume compensation
  float _undersample_z1;   // Holds sample for downsampling
  int _undersample_count;  // Counter for sample rate reduction
  
  float processBitcrush(float sample);
};

#endif
#endif
