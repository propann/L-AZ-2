#include "granular.h"
#ifdef GRANULAR

#include <math.h>
#include <string.h>
extern granular_params_t granular_params;

FLASHMEM ChromaticGranularVoice::ChromaticGranularVoice() {
    sampleData = nullptr;
    sampleLength = 0;
    noteOn = false;
    currentNote = 0;
    currentVelocity = 0;
    envelopeActive = false;
    envelopeLevel = 0.0f;
    envelopeTimeCounter = 0;
    inAttack = false;
    attackStartTime = 0;
    releaseStartTime = 0;
    inRelease = false;
    movingPosition = 0.0f;
    perNotePitchShift = 1.0f;
    memset(&currentParams, 0, sizeof(currentParams));

    // Initialize organic variation parameters (simplified, no LFO)
    uint32_t voiceSeed = (uint32_t)this + micros();
    randState = (uint16_t)voiceSeed;
    voiceTimeOffset = randState % 10000;

    // Generate unique amp contour per voice (for grains only)
    for (int i = 0; i < 4; i++) {
        randState = (uint16_t)((randState * 31337u) + 12345u);
        voiceAmpContour[i] = 0.8f + (float)(randState % 400) / 1000.0f;
    }

    // Generate grain selection biases
    for (int i = 0; i < 3; i++) {
        randState = (uint16_t)((randState * 31337u) + 12345u);
        voiceGrainBias[i] = -0.1f + (float)(randState % 200) / 1000.0f;
    }

    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) {
        grains[i].active = false;
        grains[i].sampleData = nullptr;
        grains[i].sampleLength = 0;
        grains[i].reverse = false;
    }
}

ChromaticGranularVoice::~ChromaticGranularVoice() {}

FLASHMEM void ChromaticGranularVoice::init() {
    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) {
        grains[i].active = false;
        grains[i].reverse = false;
    }
    noteOn = false;
    envelopeActive = false;
    envelopeLevel = 0.0f;
    envelopeTimeCounter = voiceTimeOffset;
    inAttack = false;
    attackStartTime = 0;
    releaseStartTime = 0;
    inRelease = false;
    movingPosition = 0.0f;
}

FLASHMEM void ChromaticGranularVoice::setParams(const granular_params_t& params) {
    currentParams = params;
}

FLASHMEM void ChromaticGranularVoice::setSampleData(const int16_t* data, uint32_t sampleFrames) {
    sampleData = data;
    sampleLength = sampleFrames;
}

FLASHMEM void ChromaticGranularVoice::setSampleDataBytes(const int16_t* data, uint32_t bytes, uint8_t numChannels) {
    if (!data) {
        sampleData = nullptr;
        sampleLength = 0;
        return;
    }
    uint32_t frames = bytes / (sizeof(int16_t) * numChannels);
    setSampleData(data, frames);
}

FLASHMEM void ChromaticGranularVoice::triggerNote(uint8_t note, uint8_t velocity) {
    if (!sampleData || sampleLength < 2) return;

    float semitones_shift = (float)note - 60.0f - 12.0f;
    perNotePitchShift = powf(2.0f, semitones_shift / 12.0f);

    noteOn = true;
    currentNote = note;
    currentVelocity = velocity;
    envelopeActive = true;
    envelopeLevel = 0.0f;  // Start at 0 for attack
    envelopeTimeCounter = voiceTimeOffset;
    releaseStartTime = 0;
    inRelease = false;
    inAttack = true;         // Start attack phase
    attackStartTime = envelopeTimeCounter;

    randState = (uint16_t)((randState * 31337u) + 12345u);
    movingPosition = (float)(randState & 0xFFFF) / 65536.0f;

    startGrain(currentParams);
}

FLASHMEM void ChromaticGranularVoice::noteOff() {
    if (!noteOn) return;
    noteOn = false;
    if (!envelopeActive) envelopeActive = true;
    releaseStartTime = envelopeTimeCounter;
    inRelease = true;
}

FLASHMEM bool ChromaticGranularVoice::isPlaying() const {
    if (noteOn) return true;
    if (envelopeActive) return true;
    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) {
        if (grains[i].active) return true;
    }
    return false;
}

FLASHMEM bool ChromaticGranularVoice::isActiveFor(uint8_t note) const {
    return (noteOn || envelopeActive) && (currentNote == note);
}

FLASHMEM int ChromaticGranularVoice::getActiveGrainCount() const {
    int c = 0;
    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) if (grains[i].active) c++;
    return c;
}

FLASHMEM void ChromaticGranularVoice::stopAllGrains() {
    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) grains[i].active = false;
    noteOn = false;
    envelopeActive = false;
    envelopeLevel = 0.0f;
    envelopeTimeCounter = voiceTimeOffset;
    releaseStartTime = 0;
    inRelease = false;
}

FLASHMEM void ChromaticGranularVoice::startGrain(const granular_params_t& params) {
    if (!sampleData || sampleLength < 2) return;

    int idx = -1;
    for (int i = 0; i < MAX_GRAINS_PER_VOICE; i++) {
        if (!grains[i].active) { idx = i; break; }
    }
    if (idx < 0) return;

    GranularGrain& g = grains[idx];
    g.sampleData = sampleData;
    g.sampleLength = sampleLength;

    // Grain size with variation
    float sizeVariation = 1.0f + voiceGrainBias[1];
    float grainSizeMs = (10.0f + (params.grain_size / 100.0f) * 195.0f) * sizeVariation;
    g.size = (uint32_t)((grainSizeMs * 0.001f) * AUDIO_SAMPLE_RATE_EXACT);
    if (g.size < 4) g.size = 4;
    g.invSize = 1.0f / (float)g.size;

    // Pitch calculation
    float baseSemitones = (params.semitones / 100.0f) * 48.0f - 24.0f;
    float basePitchRatio = powf(2.0f, baseSemitones / 12.0f) * perNotePitchShift;

    // Determine play direction based on play_mode
    bool grainReverse = false;
    if (params.play_mode == PLAYMODE_REVERSE) {
        grainReverse = true;
    } else if (params.play_mode == PLAYMODE_RANDOM) {
        randState = (uint16_t)((randState * 31337u) + 12345u);
        grainReverse = (randState & 1) == 1;  // 50% chance of reverse
    }
    g.reverse = grainReverse;

    // Set pitch ratio (negative for reverse playback)
    g.pitchRatio = grainReverse ? -basePitchRatio : basePitchRatio;

    // Position calculation
    uint32_t maxStartPos = (g.sampleLength > g.size + 2) ?
        (g.sampleLength - (uint32_t)(g.size * fabsf(g.pitchRatio)) - 2) : 1;

    float basePosRatio = params.grain_position / 100.0f;
    float spreadRatio = params.spread / 100.0f;
    int32_t maxSpreadOffset = (int32_t)(spreadRatio * maxStartPos * 0.3f);

    int32_t offset = 0;
    if (maxSpreadOffset > 0) {
        randState = (uint16_t)((randState * 31337u) + 12345u);
        offset = (randState % (2 * maxSpreadOffset + 1)) - maxSpreadOffset;
    }

    int32_t chosenPos = (int32_t)(basePosRatio * maxStartPos) + offset;
    if (chosenPos < 0) chosenPos = 0;
    if ((uint32_t)chosenPos > maxStartPos) chosenPos = maxStartPos;

    // If reverse, start from further position to play backwards
    if (grainReverse) {
        chosenPos += (int32_t)(g.size * fabsf(g.pitchRatio));
        if ((uint32_t)chosenPos >= g.sampleLength - 1) {
            chosenPos = g.sampleLength - 2;
        }
    }
    
    g.position = (float)chosenPos;

    // Volume calculation
    float velGain = (float)currentVelocity * 0.007874f;
    randState = (uint16_t)((randState * 31337u) + 12345u);
    float volumeVariation = 0.9f + (float)(randState % 200) / 1000.0f;

    g.volume = (params.volume * 0.01f) * velGain * 0.7f * volumeVariation;

    g.age = 0;
    g.active = true;
}

void ChromaticGranularVoice::mixInto(float* leftFloat, float* rightFloat, uint16_t blockSize) {
    if (!sampleData || sampleLength < 2) return;

    // Simplified density calculation
    float densityMod = 1.0f + voiceGrainBias[2];
    float grainsPerSecond = (1.0f + (currentParams.density / 100.0f) * 35.0f) * densityMod;
    uint32_t grainInterval = (grainsPerSecond > 0.0f) ? (uint32_t)(AUDIO_SAMPLE_RATE_EXACT / grainsPerSecond) : 1;

    for (int i = 0; i < blockSize; i++) {
        envelopeTimeCounter++;

        if (noteOn) {
            if (inAttack) {
                float attackTimeSeconds = (currentParams.attack / 100.0f) * 0.5f;
                
                if (attackTimeSeconds <= 0.001f) {
                    envelopeLevel = 1.0f;
                    inAttack = false;
                } else {
                    uint32_t attackSamples = (uint32_t)(attackTimeSeconds * AUDIO_SAMPLE_RATE_EXACT);
                    uint32_t elapsed = envelopeTimeCounter - attackStartTime;
                    
                    if (elapsed >= attackSamples) {
                        envelopeLevel = 1.0f;
                        inAttack = false;
                    } else {
                        float progress = (float)elapsed / (float)attackSamples;
                        envelopeLevel = progress;
                    }
                }
            } else {
                envelopeLevel = 1.0f;
            }
        } else if (envelopeActive && inRelease) {
            float releaseTimeSeconds = (currentParams.release / 100.0f) * 4.0f;
            
            if (releaseTimeSeconds <= 0.001f) {
                envelopeLevel = 0.0f;
                envelopeActive = false;
                inRelease = false;
            } else {
                uint32_t releaseSamples = (uint32_t)(releaseTimeSeconds * AUDIO_SAMPLE_RATE_EXACT);
                uint32_t elapsed = envelopeTimeCounter - releaseStartTime;
                
                if (elapsed >= releaseSamples) {
                    envelopeLevel = 0.0f;
                    envelopeActive = false;
                    inRelease = false;
                } else {
                    float progress = (float)elapsed / (float)releaseSamples;
                    envelopeLevel = 1.0f - progress;
                }
            }
        } else {
            envelopeLevel = 0.0f;
            envelopeActive = false;
        }

        // Spawn grains
        if ((noteOn || (envelopeActive && inRelease)) && envelopeLevel > 0.0f) {
            if (grainInterval > 0 && (envelopeTimeCounter % grainInterval) == 0) {
                startGrain(currentParams);
            }
        }

        float sampleOut = 0.0f;

        // Process grains with boundary checking for reverse playback
        for (int gidx = 0; gidx < MAX_GRAINS_PER_VOICE; gidx++) {
            GranularGrain& g = grains[gidx];
            if (!g.active) continue;

            // Check boundaries based on play direction
            bool outOfBounds = false;
            if (g.reverse) {
                // Reverse: stop if position goes below 1
                if (g.position < 1.0f) {
                    outOfBounds = true;
                }
            } else {
                // Forward: stop if position exceeds sample length
                if ((uint32_t)(g.position + fabsf(g.pitchRatio)) >= g.sampleLength - 1) {
                    outOfBounds = true;
                }
            }

            if (g.age >= g.size || outOfBounds) {
                g.active = false;
                continue;
            }

            float ageRatio = (float)g.age * g.invSize;
            float grainEnv = 1.0f;

            if (ageRatio < 0.15f) {
                grainEnv = ageRatio / 0.15f;
            }
            else if (ageRatio > 0.85f) {
                grainEnv = (1.0f - ageRatio) / 0.15f;
            }

            uint32_t intPos = (uint32_t)g.position;
            float frac = g.position - intPos;
            
            // Ensure we don't read out of bounds
            if (intPos < g.sampleLength - 1) {
                float s1 = g.sampleData[intPos] * (1.0f - frac);
                float s2 = g.sampleData[intPos + 1] * frac;
                float s = (s1 + s2) * 0.000030518f;

                sampleOut += s * grainEnv * g.volume;
            }
            
            g.position += g.pitchRatio;
            g.age++;
        }

        // Apply the voice envelope
        sampleOut *= envelopeLevel;
        leftFloat[i] += sampleOut;
        rightFloat[i] += sampleOut;
    }
}

FLASHMEM AudioChromaticGranularPoly::AudioChromaticGranularPoly() : AudioStream(0, NULL) {
    sharedParams.grain_size = 50;
    sharedParams.grain_position = 50;
    sharedParams.semitones = 50;
    sharedParams.volume = 100;
    sharedParams.density = 50;
    sharedParams.spread = 50;
    sharedParams.attack = 0;
    sharedParams.release = 33;
    sharedParams.play_mode = PLAYMODE_FORWARD;  // Default to forward playback
    sharedParams.active = true;
    polyRandSeed = micros();

    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        voices[i] = new ChromaticGranularVoice();
        voices[i]->init();
        voices[i]->setParams(sharedParams);
    }
}

FLASHMEM AudioChromaticGranularPoly::~AudioChromaticGranularPoly() {
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        if (voices[i]) {
            delete voices[i];
            voices[i] = nullptr;
        }
    }
}

FLASHMEM void AudioChromaticGranularPoly::begin() {
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        if (voices[i]) voices[i]->init();
    }
}

FLASHMEM void AudioChromaticGranularPoly::setParams(const granular_params_t& params) {
    sharedParams = params;
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        voices[i]->setParams(sharedParams);
    }
}

FLASHMEM void AudioChromaticGranularPoly::setSampleData(const int16_t* data, uint32_t sampleFrames) {
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        voices[i]->setSampleData(data, sampleFrames);
    }
}

FLASHMEM void AudioChromaticGranularPoly::setSampleDataBytes(const int16_t* data, uint32_t bytes, uint8_t numChannels) {
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        voices[i]->setSampleDataBytes(data, bytes, numChannels);
    }
}

FLASHMEM void AudioChromaticGranularPoly::triggerNote(uint8_t note, uint8_t velocity) {
    ChromaticGranularVoice* target = nullptr;

    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        if (!voices[i]->isPlaying()) {
            target = voices[i];
            break;
        }
    }

    if (!target) {
        static uint8_t lastStolenVoice = 0;
        target = voices[lastStolenVoice];
        lastStolenVoice = (lastStolenVoice + 1) % MAX_POLY_VOICES;
        target->stopAllGrains();
    }

    if (target) {
        target->setParams(sharedParams);
        target->triggerNote(note, velocity);
    }
}

FLASHMEM void AudioChromaticGranularPoly::noteOff(uint8_t note) {
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        if (voices[i]->isActiveFor(note)) voices[i]->noteOff();
    }
}

FLASHMEM void AudioChromaticGranularPoly::stopAllVoices() {
    for (int i = 0; i < MAX_POLY_VOICES; i++) voices[i]->stopAllGrains();
}

FLASHMEM int AudioChromaticGranularPoly::getActiveVoiceCount() const {
    int c = 0;
    for (int i = 0; i < MAX_POLY_VOICES; i++) if (voices[i]->isPlaying()) c++;
    return c;
}

FLASHMEM int AudioChromaticGranularPoly::getActiveGrainCount() const {
    int s = 0;
    for (int i = 0; i < MAX_POLY_VOICES; i++) s += voices[i]->getActiveGrainCount();
    return s;
}

void AudioChromaticGranularPoly::update(void) {
    audio_block_t* leftBlock = allocate();
    if (!leftBlock) return;
    audio_block_t* rightBlock = allocate();
    if (!rightBlock) { release(leftBlock); return; }

    float mixL[AUDIO_BLOCK_SAMPLES] = { 0 };
    float mixR[AUDIO_BLOCK_SAMPLES] = { 0 };

    for (int v = 0; v < MAX_POLY_VOICES; v++) {
        voices[v]->mixInto(mixL, mixR, AUDIO_BLOCK_SAMPLES);
    }

    const int activeVoices = getActiveVoiceCount();
    const float dynamicScale = 1.0f / (1.0f + 0.2f * (float)activeVoices);
    const float masterGain = 0.5f * dynamicScale;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float lf = mixL[i] * masterGain;
        float rf = mixR[i] * masterGain;

        lf = (lf > 1.0f) ? 1.0f : ((lf < -1.0f) ? -1.0f : lf);
        rf = (rf > 1.0f) ? 1.0f : ((rf < -1.0f) ? -1.0f : rf);

        leftBlock->data[i] = (int16_t)(lf * 32767.0f);
        rightBlock->data[i] = (int16_t)(rf * 32767.0f);
    }

    transmit(leftBlock, 0);
    transmit(rightBlock, 1);
    release(leftBlock);
    release(rightBlock);
}

#endif // GRANULAR