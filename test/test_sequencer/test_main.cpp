#include <unity.h>
#include "sequencer.h"

void testSequencerTrackInit() {
    SequencerTrack track;
    TEST_ASSERT_FALSE(track.stepPlaying);
    TEST_ASSERT_EQUAL_UINT8(az2::kStepFxNone, track.activeFx);
    for (int i = 0; i < az2::kStepCount; ++i) {
        TEST_ASSERT_FALSE(track.stepOn[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testSequencerTrackInit);
    return UNITY_END();
}
