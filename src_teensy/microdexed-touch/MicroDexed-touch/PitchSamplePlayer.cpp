#include "PitchSamplePlayer.h"

uint32_t loop_start_ = 0;
uint32_t loop_finish_ = 0;
loop_type loop_type_ = looptype_none;
float phase_direction = 1.0f; // For ping-pong loop

PitchSamplePlayer::PitchSamplePlayer() :
    AudioStream(0, NULL) // This is a source stream, no inputs
{
    sampleData = NULL;
    totalFrames = 0;
    numChannels = 1;
    phase = 0.0f;
    pitchMultiplier = 1.0f;
    playing = false;
    interpolationEnabled = true;
    loopingEnabled = false;
}

FLASHMEM void PitchSamplePlayer::play(int16_t* data, uint32_t numFrames, uint8_t channels) {
    if (playing) stop();

    sampleData = data;
    totalFrames = numFrames;
    numChannels = channels;
    phase = 0.0f;
    playing = true;
}

FLASHMEM void PitchSamplePlayer::playRaw(int16_t* data, uint32_t totalLength, uint8_t channels) {
    if (playing) stop();

    sampleData = data;
    numChannels = channels;
    totalFrames = (channels > 0) ? (totalLength / channels) : totalLength;
    phase = 0.0f;
    playing = true;
}

FLASHMEM void PitchSamplePlayer::stop() {
    playing = false;
    phase = 0.0f;
}

FLASHMEM bool PitchSamplePlayer::isPlaying() {
    return playing;
}

FLASHMEM void PitchSamplePlayer::setPlaybackRate(float multiplier) {
    if (multiplier < 0.01f) multiplier = 0.01f;
    if (multiplier > 4.0f) multiplier = 4.0f;
    pitchMultiplier = multiplier;
}

FLASHMEM bool PitchSamplePlayer::enableInterpolation(bool enable) {
    interpolationEnabled = enable;
    if (!enable) {
        setPlaybackRate(1.0f); // force normal speed when disabled
    }
    return interpolationEnabled;
}

FLASHMEM void PitchSamplePlayer::setLoopType(loop_type t) {
    loop_type_ = t;
    loopingEnabled = (t != looptype_none);
}

FLASHMEM void PitchSamplePlayer::setLoopStart(uint32_t loop_start) {
    loop_start_ = loop_start;
}

FLASHMEM void PitchSamplePlayer::setLoopFinish(uint32_t loop_finish) {
    loop_finish_ = loop_finish;
}

FLASHMEM void PitchSamplePlayer::setLooping(bool enable) {
    //loopingEnabled = enable;
    if (!enable) {
      loop_type_ = looptype_none;
    } else if (loop_type_ == looptype_none) {
      // If looping is enabled but no type is set, default to repeat
      loop_type_ = looptype_repeat;
    }
}

void PitchSamplePlayer::update(void) {
    if (!playing || sampleData == NULL) {
        return;
    }

    audio_block_t* blockL = allocate();
    audio_block_t* blockR = allocate();

    if (!(blockL && blockR)) {
        if (blockL) release(blockL);
        if (blockR) release(blockR);
        return;
    }

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        // Handle looping based on loop_type
        if (loopingEnabled) {
            if (loop_type_ == looptype_repeat) {
                if (phase >= (float)loop_finish_) {
                    phase = (float)loop_start_;
                }
            } else if (loop_type_ == looptype_pingpong) {
                if (phase >= (float)loop_finish_) {
                    phase = (float)loop_finish_;
                    phase_direction = -1.0f;
                } else if (phase <= (float)loop_start_) {
                    phase = (float)loop_start_;
                    phase_direction = 1.0f;
                }
            }
        } else {
            // No looping, check for end of sample
            if (phase >= (float)(totalFrames - 1)) {
                stop();
                release(blockL);
                release(blockR);
                return;
            }
        }

        int idx = (int)phase;
        float frac = phase - idx;

        int16_t left = 0, right = 0;

        if (numChannels == 1) {
            // Mono → duplicate to both L and R
            int16_t s1 = sampleData[idx];
            int16_t s2 = sampleData[min(idx + 1, (int)totalFrames - 1)];
            float interp = interpolationEnabled ? 
                ((1.0f - frac) * s1 + frac * s2) : s1;

            left = right = (int16_t)interp;
        } else if (numChannels == 2) {
            // Stereo → interleaved
            int16_t s1L = sampleData[idx * 2];
            int16_t s1R = sampleData[idx * 2 + 1];
            int16_t s2L = sampleData[min(idx + 1, (int)totalFrames - 1) * 2];
            int16_t s2R = sampleData[min(idx + 1, (int)totalFrames - 1) * 2 + 1];

            if (interpolationEnabled) {
                left  = (int16_t)((1.0f - frac) * s1L + frac * s2L);
                right = (int16_t)((1.0f - frac) * s1R + frac * s2R);
            } else {
                left  = s1L;
                right = s1R;
            }
        }

        blockL->data[i] = left;
        blockR->data[i] = right;

        phase += pitchMultiplier * phase_direction;
    }

    transmit(blockL, 0);
    transmit(blockR, 1);
    release(blockL);
    release(blockR);
}

