#ifdef COMPILE_FOR_PSRAM

#ifndef OFFLINE_REVERB_H
#define OFFLINE_REVERB_H

#include <Arduino.h>
#include <malloc.h>

// PSRAM allocation functions
extern "C" {
void *extmem_malloc(size_t size);
void extmem_free(void* ptr);
}

class OfflineReverb {
public:
    OfflineReverb();
    ~OfflineReverb();
    
    // Initialize reverb with sample rate
    void init(float sampleRate = 44100.0f);
    
    // Apply reverb to stereo buffer (interleaved L/R samples)
    void applyReverb(int16_t* buffer, uint32_t numFrames,bool isstereo);
    
    // Parameter setters
    void setRoomSize(float size);      // 0.0 to 1.0
    void setDamping(float damping);    // 0.0 to 1.0
    void setWetLevel(float wet);       // 0.0 to 1.0
    void setDryLevel(float dry);       // 0.0 to 1.0
    void setWidth(float width);        // 0.0 to 1.0
    void setFreezeMode(bool freeze);
    
    // Reset reverb state
    void reset();
    
private:
    struct CombFilter {
        float* buffer;
        uint32_t bufferSize;
        uint32_t bufferIndex;
        float feedback;
        float dampening;
        float filterState;
        
        void init(uint32_t size);
        float process(float input);
        void clear();
    };
    
    struct AllpassFilter {
        float* buffer;
        uint32_t bufferSize;
        uint32_t bufferIndex;
        float feedback;
        
        void init(uint32_t size, float fb);
        float process(float input);
        void clear();
    };

    // A new struct for simulating early reflections
    struct TappedDelayLine {
        float* buffer;
        uint32_t bufferSize;
        uint32_t bufferIndex;
        // The tap points for early reflections
        const uint32_t taps[6] = {20, 45, 70, 95, 120, 145};
        
        void init(uint32_t size);
        float process(float input);
        void clear();
    };
    
    // Reverb parameters
    float roomSize;
    float damping;
    float wetLevel;
    float dryLevel;
    float width;
    bool freezeMode;
    float sampleRate;
    
    // IMPROVEMENT: Increased filter count for a denser reverb tail
    CombFilter combL[16];
    CombFilter combR[16];
    
    AllpassFilter allpassL[8];
    AllpassFilter allpassR[8];

    // IMPROVEMENT: New early reflections stage
    TappedDelayLine earlyReflectionsL;
    TappedDelayLine earlyReflectionsR;
    
    // Low-pass filters for damping
    float lpFilterStateL;
    float lpFilterStateR;
    float lpCoeff;
    
    void updateParameters();
};

#endif // OFFLINE_REVERB_H
#endif
