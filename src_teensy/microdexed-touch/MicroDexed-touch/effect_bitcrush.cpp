/*
 * AudioEffectBitcrush.cpp
 * 
 * Implementation of bit crusher effect for Teensy Audio Library
 * Using original class name for compatibility
 */

#if defined BITCRUSH
#include "effect_bitcrush.h"
#include <math.h>

AudioEffectBitcrush::AudioEffectBitcrush() : AudioStream(1, inputQueueArray) {
  _drive = 0.5f;          // Controls bit depth reduction (0.0 = 16-bit, 1.0 = 1-bit)
  _outputGain = 1.0f;     // Controls output volume after crushing
  _undersample_z1 = 0.0f; // Hold last sample for downsampling
  _undersample_count = 0; // Counter for sample rate reduction
}

void AudioEffectBitcrush::update(void) {
  audio_block_t *block;
  int16_t *p;
  
  block = receiveWritable();
  if (!block) return;
  
  p = block->data;
  
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    // Convert to float -1.0 to 1.0
    float sample = (float)(*p) / 32768.0f;
    
    // Process bit crushing
    float crushed = processBitcrush(sample);
    
    // Apply output gain
    crushed *= _outputGain;
    
    // Convert back to int16 with clipping
    int32_t val = (int32_t)(crushed * 32767.0f);
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    
    *p++ = (int16_t)val;
  }
  
  transmit(block);
  release(block);
}

float AudioEffectBitcrush::processBitcrush(float sample) {
  // drive controls bit depth reduction
  // drive = 0.0 → 16-bit (no reduction)
  // drive = 1.0 → 1-bit (maximum reduction)
  // Exponential mapping for more musical response
  float drive_squared = _drive * _drive;
  
  // Map drive to bit depth (16 to 1 bits)
  // Exponential curve: at drive=0.5, we're around 4-5 bits
  float bit_depth = 16.0f - drive_squared * 15.0f;
  if (bit_depth < 1.0f) bit_depth = 1.0f;
  
  // Calculate number of quantization levels
  int levels = (int)powf(2.0f, bit_depth);
  
  // Quantize (bit reduction)
  float quantized;
  if (levels > 1) {
    // Scale to number of levels, round, then scale back
    quantized = floorf(sample * (levels - 1) + 0.5f) / (levels - 1);
  } else {
    // 1-bit: hard clip to -1, 0, or +1
    if (sample > 0.0f) quantized = 1.0f;
    else quantized = -1.0f;
  }
  
  // Calculate downsampling factor based on drive
  // More drive = more aggressive downsampling (sample rate reduction)
  int downsample_factor = 1 + (int)(drive_squared * 31.0f); // 1x to 32x downsampling
  
  // Sample rate reduction (undersampling)
  _undersample_count++;
  if (_undersample_count >= downsample_factor) {
    _undersample_z1 = quantized;  // Update held sample
    _undersample_count = 0;
  }
  
  // Output held sample (creates stair-step waveform)
  float output = _undersample_z1;
  
  // No noise injection - clean bit crushing only
  
  // Final clipping
  if (output > 1.0f) output = 1.0f;
  if (output < -1.0f) output = -1.0f;
  
  return output;
}

void AudioEffectBitcrush::setDrive(float drive) {
  if (drive < 0.0f) drive = 0.0f;
  if (drive > 1.0f) drive = 1.0f;
  _drive = drive;
  
  // No automatic gain compensation - let user control output gain manually
  // This prevents unexpected volume changes
  // _outputGain remains unchanged unless setOutputGain is called
}

void AudioEffectBitcrush::setOutputGain(float gain) {
  if (gain < 0.0f) gain = 0.0f;
  if (gain > 2.0f) gain = 2.0f;
  _outputGain = gain;
}

#endif