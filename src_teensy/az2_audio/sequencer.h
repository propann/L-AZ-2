#pragma once
#include <cstdint>

namespace az2 {
    constexpr uint8_t kStepsPerMeasure = 16;
    constexpr uint8_t kMaxPatternMeasures = 8;
    constexpr uint8_t kStepCount = kStepsPerMeasure * kMaxPatternMeasures;
    constexpr uint8_t kTrackCount = 8;
    constexpr uint8_t kPatternCount = 8;

    // CRUSH/DELAY ajoutes le 2026-09-23 ("on regroupe tous les effets dans
    // le bouton EFFET") : les effets audio par piste (bitcrusher/echo,
    // jusque-la seulement pilotables par commande serie CRUSH:/DELAY: sans
    // aucun acces UI) rejoignent ARP/CUT/RET comme un 5e/6e choix de la
    // meme colonne FX du tracker -- stepFxVal devient un NUMERO DE PATCH
    // (voir kCrushPresets[]/kDelayPresets[] dans main.cpp), pas une valeur
    // brute, pour rester coherent avec "des patch qu'on peut appeler".
    enum StepFx : uint8_t {
      kStepFxNone = 0,
      kStepFxArp = 1,
      kStepFxCut = 2,
      kStepFxRetrig = 3,
      kStepFxCrush = 4,
      kStepFxDelay = 5,
    };
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
