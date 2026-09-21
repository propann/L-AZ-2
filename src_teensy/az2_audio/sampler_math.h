#pragma once

#include <cstdint>

namespace az2_sampler_math {

// Calcul pur, independant d'Arduino/AudioStream, utilise par le sampleur et
// couvert par les tests natifs. noteHz/rootHz sont separes du calcul MIDI
// pour conserver une seule implementation de midiNoteToFreq() dans le
// firmware.
inline float samplerPlaybackStep(uint32_t sourceRate, uint32_t outputRate,
                                 float noteHz, float rootHz) {
  if (sourceRate == 0 || outputRate == 0 || noteHz <= 0.0f || rootHz <= 0.0f)
    return 0.0f;
  return (static_cast<float>(sourceRate) / static_cast<float>(outputRate)) *
         (noteHz / rootHz);
}

inline float samplerLinearInterpolate(int16_t a, int16_t b, float fraction) {
  return static_cast<float>(a) +
         (static_cast<float>(b) - static_cast<float>(a)) * fraction;
}

}  // namespace az2_sampler_math
