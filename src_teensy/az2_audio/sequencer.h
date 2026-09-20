#pragma once
#include <cstdint>

namespace az2 {
    constexpr uint8_t kStepCount = 16;
    constexpr uint8_t kTrackCount = 8;
    constexpr uint8_t kPatternCount = 8;

    enum StepFx : uint8_t { kStepFxNone = 0, kStepFxArp = 1, kStepFxCut = 2, kStepFxRetrig = 3 };
}

struct SequencerTrack {
    bool stepOn[az2::kStepCount] = {};
    uint8_t stepNote[az2::kStepCount] = {};
    uint8_t stepPatch[az2::kStepCount];
    uint8_t stepFx[az2::kStepCount] = {};
    uint8_t stepFxVal[az2::kStepCount] = {};
    uint8_t stepProb[az2::kStepCount];
    uint8_t stepCondition[az2::kStepCount] = {};
    uint8_t playingNote = 0;
    bool stepPlaying = false;
    uint8_t activeFx = az2::kStepFxNone;
    uint8_t activeFxVal = 0;
    uint8_t baseNote = 0;
    uint8_t ticksSinceTrigger = 0;
};
