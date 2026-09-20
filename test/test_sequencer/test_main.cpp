#include <cassert>
#include "sequencer.h"

void testSequencerTrackInit() {
    SequencerTrack track;
    assert(track.stepPlaying == false);
    assert(track.activeFx == az2::kStepFxNone);
    for (int i = 0; i < az2::kStepCount; ++i) {
        assert(track.stepOn[i] == false);
    }
}

int main() {
    testSequencerTrackInit();
    return 0;
}
