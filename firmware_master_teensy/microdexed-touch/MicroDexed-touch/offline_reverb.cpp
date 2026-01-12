#ifdef COMPILE_FOR_PSRAM
#include "offline_reverb.h"
#include <math.h>
#include <string.h>
#include <Arduino.h>
#include <LCDMenuLib2.h>
#include "ILI9341_t3n.h"
extern ILI9341_t3n display;

// Comb filter implementation
FLASHMEM void OfflineReverb::CombFilter::init(uint32_t size) {
    // Check if a buffer already exists and free it to prevent memory leaks.
    if (buffer) {
        extmem_free(buffer);
        buffer = nullptr;
    }

    bufferSize = size;
    // Use extmem_malloc to allocate from external memory.
    buffer = (float*)extmem_malloc(bufferSize * sizeof(float));
    if (!buffer) {
        Serial.println("Error: Failed to allocate external memory for CombFilter!");
        bufferSize = 0; // Set size to 0 to prevent further use.
        return;
    }
    clear();
}

FLASHMEM float OfflineReverb::CombFilter::process(float input) {
    if (!buffer) return 0.0f;

    float output = buffer[bufferIndex];
    // Damping filter (a simple one-pole low-pass filter)
    output = filterState = (output * (1.0f - dampening)) + (filterState * dampening);

    buffer[bufferIndex] = input + (output * feedback);
    bufferIndex = (bufferIndex + 1) % bufferSize;

    return output;
}

FLASHMEM void OfflineReverb::CombFilter::clear() {
    if (buffer) {
        memset(buffer, 0, bufferSize * sizeof(float));
    }
    bufferIndex = 0;
    filterState = 0.0f;
}

// Allpass filter implementation
FLASHMEM void OfflineReverb::AllpassFilter::init(uint32_t size, float fb) {
    // Check if a buffer already exists and free it to prevent memory leaks.
    if (buffer) {
        extmem_free(buffer);
        buffer = nullptr;
    }

    bufferSize = size;
    feedback = fb;
    // Use extmem_malloc to allocate from external memory.
    buffer = (float*)extmem_malloc(bufferSize * sizeof(float));
    if (!buffer) {
        Serial.println("Error: Failed to allocate external memory for AllpassFilter!");
        bufferSize = 0; // Set size to 0 to prevent further use.
        return;
    }
    clear();
}

FLASHMEM float OfflineReverb::AllpassFilter::process(float input) {
    if (!buffer) return input;

    float bufferOut = buffer[bufferIndex];
    float output = bufferOut - input * feedback;
    buffer[bufferIndex] = input + bufferOut * feedback;
    bufferIndex = (bufferIndex + 1) % bufferSize;
    return output;
}

FLASHMEM void OfflineReverb::AllpassFilter::clear() {
    if (buffer) {
        memset(buffer, 0, bufferSize * sizeof(float));
    }
    bufferIndex = 0;
}

// Tapped Delay Line for early reflections
FLASHMEM void OfflineReverb::TappedDelayLine::init(uint32_t size) {
    if (buffer) {
        extmem_free(buffer);
        buffer = nullptr;
    }
    bufferSize = size;
    buffer = (float*)extmem_malloc(bufferSize * sizeof(float));
    if (!buffer) {
        Serial.println("Error: Failed to allocate external memory for TappedDelayLine!");
        bufferSize = 0;
        return;
    }
    clear();
}

FLASHMEM float OfflineReverb::TappedDelayLine::process(float input) {
    if (!buffer) return 0.0f;

    // Store the new sample
    buffer[bufferIndex] = input;

    // Read the tapped outputs
    float output = 0.0f;
    for (int i = 0; i < 6; i++) {
        uint32_t tapIndex = (bufferIndex - taps[i] + bufferSize) % bufferSize;
        output += buffer[tapIndex];
    }

    // Move to the next index
    bufferIndex = (bufferIndex + 1) % bufferSize;

    return output;
}

FLASHMEM void OfflineReverb::TappedDelayLine::clear() {
    if (buffer) {
        memset(buffer, 0, bufferSize * sizeof(float));
    }
    bufferIndex = 0;
}

// Main reverb class
FLASHMEM OfflineReverb::OfflineReverb() :
    roomSize(0.8f), damping(0.5f), wetLevel(0.3f),
    dryLevel(0.5f), width(1.0f), freezeMode(false),
    sampleRate(44100.0f),
    lpFilterStateL(0.0f), lpFilterStateR(0.0f), lpCoeff(0.5f) {
}

// The destructor now relies on the individual filter destructors.
FLASHMEM OfflineReverb::~OfflineReverb() {
    // The filter destructors will handle freeing the memory.
}

FLASHMEM void OfflineReverb::init(float sampleRate) {
    this->sampleRate = sampleRate;

    // IMPROVEMENT: More comb filters for a denser reverb tail.
    // Use a new set of prime numbers for better diffusion.
    uint32_t combTimesL[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617, 1693, 1759, 1827, 1891, 1969, 2039, 2111, 2187 };
    uint32_t combTimesR[] = { 1116 + 23, 1188 + 23, 1277 + 23, 1356 + 23, 1422 + 23, 1491 + 23, 1557 + 23, 1617 + 23, 1693 + 23, 1759 + 23, 1827 + 23, 1891 + 23, 1969 + 23, 2039 + 23, 2111 + 23, 2187 + 23 };

    for (int i = 0; i < 16; i++) {
        combL[i].init(combTimesL[i]);
        combR[i].init(combTimesR[i]);
    }

    // IMPROVEMENT: More allpass filters for better diffusion and density.
    uint32_t allpassTimes[] = { 225, 341, 441, 556, 607, 721, 803, 917 };
    float allpassFeedback[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

    for (int i = 0; i < 8; i++) {
        allpassL[i].init(allpassTimes[i], allpassFeedback[i]);
        allpassR[i].init(allpassTimes[i] + 11, allpassFeedback[i]);
    }

    // IMPROVEMENT: Initialize the early reflections stage with a short buffer
    earlyReflectionsL.init(256);
    earlyReflectionsR.init(256);

    updateParameters();
    reset();
}

extern int32_t reverb_generation_process;
extern FLASHMEM void print_formatted_number(uint16_t number, uint8_t length);

FLASHMEM void progress_display(uint8_t value)
{
    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.setCursor(278, 181);
    print_formatted_number(value, 2);
    display.print("%");
}

FLASHMEM void OfflineReverb::applyReverb(int16_t* buffer, uint32_t numFrames, bool isStereo) {
    if (!buffer || numFrames == 0) return;

    // Process stereo samples, always outputting stereo
    if (isStereo) {
        for (uint32_t i = 0; i < numFrames / 2; i++) {

            if (i % 22000 == 0)
            {
                // Calculate progress percentage
                uint8_t progressPercent = (static_cast<float>(i) / numFrames) * 100;
                progress_display(progressPercent);
            }

            float inputL = (float)buffer[i * 2] / 32768.0f;
            float inputR = (float)buffer[i * 2 + 1] / 32768.0f;

            float earlyL = earlyReflectionsL.process(inputL);
            float earlyR = earlyReflectionsR.process(inputR);

            float reverbInL = earlyL + earlyR * 0.1f;
            float reverbInR = earlyR + earlyL * 0.1f;

            float outL = 0.0f, outR = 0.0f;
            for (int j = 0; j < 16; j++) {
                outL += combL[j].process(reverbInL);
                outR += combR[j].process(reverbInR);
            }
            outL /= 4.0f;
            outR /= 4.0f;

            lpFilterStateL = outL * lpCoeff + lpFilterStateL * (1.0f - lpCoeff);
            lpFilterStateR = outR * lpCoeff + lpFilterStateR * (1.0f - lpCoeff);
            outL = lpFilterStateL;
            outR = lpFilterStateR;

            for (int j = 0; j < 8; j++) {
                outL = allpassL[j].process(outL);
                outR = allpassR[j].process(outR);
            }

            float wetL = outL * wetLevel;
            float wetR = outR * wetLevel;

            float mid = (wetL + wetR) * 0.5f;
            float side = (wetL - wetR) * 0.5f;
            wetL = mid + side * width;
            wetR = mid - side * width;

            float dryL = inputL * dryLevel;
            float dryR = inputR * dryLevel;

            buffer[i * 2] = (int16_t)(fmaxf(fminf((dryL + wetL) * 32767.0f, 32767.0f), -32768.0f));
            buffer[i * 2 + 1] = (int16_t)(fmaxf(fminf((dryR + wetR) * 32767.0f, 32767.0f), -32768.0f));
        }
    }
    else { // Process mono samples, outputting mono
        for (uint32_t i = 0; i < numFrames; i++) {

            if (i % 22000 == 0)
            {
                // Calculate progress percentage
                uint8_t progressPercent = (static_cast<float>(i) / numFrames) * 100;
                progress_display(progressPercent);
            }

            float input = (float)buffer[i] / 32768.0f;

            float early = earlyReflectionsL.process(input);
            float reverbIn = early;

            float out = 0.0f;
            for (int j = 0; j < 16; j++) {
                out += combL[j].process(reverbIn);
            }
            out /= 4.0f;

            lpFilterStateL = out * lpCoeff + lpFilterStateL * (1.0f - lpCoeff);
            out = lpFilterStateL;

            for (int j = 0; j < 8; j++) {
                out = allpassL[j].process(out);
            }

            // Average the stereo channels to create a single mono wet signal
            float monoWet = (out * wetLevel);

            float dry = input * dryLevel;

            // Write a single mono sample to the buffer
            buffer[i] = (int16_t)(fmaxf(fminf((dry + monoWet) * 32767.0f, 32767.0f), -32768.0f));
        }
    }

    display.setTextSize(2);
    display.setTextColor(COLOR_SYSTEXT, DX_DARKCYAN);
    display.setCursor(272, 181);
    display.print(" ON ");
}

FLASHMEM void OfflineReverb::setRoomSize(float size) {
    roomSize = fmaxf(fminf(size, 1.0f), 0.0f);
    updateParameters();
}

FLASHMEM void OfflineReverb::setDamping(float damping) {
    this->damping = fmaxf(fminf(damping, 1.0f), 0.0f);
    updateParameters();
}

FLASHMEM void OfflineReverb::setWetLevel(float wet) {
    wetLevel = fmaxf(fminf(wet, 1.0f), 0.0f);
}

FLASHMEM void OfflineReverb::setDryLevel(float dry) {
    dryLevel = fmaxf(fminf(dry, 1.0f), 0.0f);
}

FLASHMEM void OfflineReverb::setWidth(float width) {
    this->width = fmaxf(fminf(width, 1.0f), 0.0f);
}

FLASHMEM void OfflineReverb::setFreezeMode(bool freeze) {
    freezeMode = freeze;
    updateParameters();
}

FLASHMEM void OfflineReverb::reset() {
    for (int i = 0; i < 16; i++) {
        combL[i].clear();
        combR[i].clear();
    }
    for (int i = 0; i < 8; i++) {
        allpassL[i].clear();
        allpassR[i].clear();
    }
    earlyReflectionsL.clear();
    earlyReflectionsR.clear();

    lpFilterStateL = 0.0f;
    lpFilterStateR = 0.0f;
}

FLASHMEM void OfflineReverb::updateParameters() {
    float feedback = freezeMode ? 1.0f : roomSize * 0.94f;
    float damp = freezeMode ? 0.0f : damping * 0.4f + 0.05f;

    // Experiment with these multipliers to change the reverb's character.
    // A smaller multiplier on `roomSize` will result in a shorter decay time.
    // Changing the `0.4f` and `0.05f` will adjust the damping effect.
    for (int i = 0; i < 16; i++) {
        combL[i].feedback = feedback;
        combR[i].feedback = feedback;
        combL[i].dampening = damp;
        combR[i].dampening = damp;
    }

    // Update low-pass filter coefficient based on damping
    lpCoeff = 0.1f + damping * 0.4f;
}
#endif
