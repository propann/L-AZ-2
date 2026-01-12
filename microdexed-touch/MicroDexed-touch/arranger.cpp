#include "arranger.h"
#include "config.h"

ChordInfo currentChord = { 0 };

// Simple sort function (bubble sort)
FLASHMEM void sortArray(uint8_t arr[], uint8_t size) {
    for (uint8_t i = 0; i < size - 1; ++i) {
        for (uint8_t j = i + 1; j < size; ++j) {
            if (arr[i] > arr[j]) {
                uint8_t temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

FLASHMEM const char* midiToNote(uint8_t midiNote) {
    static const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return notes[midiNote % 12];
}

FLASHMEM bool containsInterval(const uint8_t intervals[], uint8_t size, uint8_t interval) {
    for (uint8_t i = 0; i < size; i++) {
        if (intervals[i] == interval) return true;
    }
    return false;
}

//  Function to calculate intervals from root
FLASHMEM void getIntervals(const uint8_t notes[], uint8_t size, uint8_t root, uint8_t intervals[]) {
    for (uint8_t i = 0; i < size; ++i) {
        intervals[i] = (notes[i] - root + 12) % 12;  // Intervals from the root, modulo 12
    }
}

FLASHMEM const char* detectChordType(const uint8_t intervals[], uint8_t size) {
    if (size < 3) return NULL;

    bool hasMinorThird = containsInterval(intervals, size, 3);
    bool hasMajorThird = containsInterval(intervals, size, 4);
    bool hasDimFifth = containsInterval(intervals, size, 6);
    bool hasPerfFifth = containsInterval(intervals, size, 7);
    bool hasAugFifth = containsInterval(intervals, size, 8);
    bool hasMinorSeventh = containsInterval(intervals, size, 10);
    bool hasMajorSixth = containsInterval(intervals, size, 9);
    bool hasMajorSeventh = containsInterval(intervals, size, 11);

    // First check 4-note chords if we have enough notes
    if (size >= 4) {
        // Dominant 7th (major triad + minor 7th)
        if (hasMajorThird && hasPerfFifth && hasMinorSeventh) return "7th";
        // Major 7th (major triad + major 7th)
        if (hasMajorThird && hasPerfFifth && hasMajorSeventh) return "Maj7";
        // Minor 7th (minor triad + minor 7th)
        if (hasMinorThird && hasPerfFifth && hasMinorSeventh) return "m7";
        // Minor Major 7th (minor triad + major 7th)
        if (hasMinorThird && hasPerfFifth && hasMajorSeventh) return "mMaj7";
        // Diminished 7th (dim triad + dim 7th)
        if (hasMinorThird && hasDimFifth && containsInterval(intervals, size, 9)) return "dim7";
        // Half-diminished 7th (dim triad + min 7th)
        if (hasMinorThird && hasDimFifth && hasMinorSeventh) return "m7b5";
        // Augmented 7th (aug triad + min 7th)
        if (hasMajorThird && hasAugFifth && hasMinorSeventh) return "aug7";
        // 6th chords
        if (hasMajorThird && hasPerfFifth && hasMajorSixth) return "6";
        if (hasMinorThird && hasPerfFifth && hasMajorSixth) return "m6";
    }

    // Then check standard triads (original functionality)
    if (hasMajorThird && hasPerfFifth) return "Major";
    if (hasMinorThird && hasPerfFifth) return "Minor";
    if (hasMinorThird && hasDimFifth) return "Diminished";
    if (hasMajorThird && hasAugFifth) return "Augmented";

    return NULL;
}

FLASHMEM void updateCurrentChord(const uint8_t midiNotes[], uint8_t size) {
    if (size < 3) {
        currentChord.active = false;
        return;
    }

    uint8_t sortedNotes[size];
    memcpy(sortedNotes, midiNotes, size * sizeof(uint8_t));
    sortArray(sortedNotes, size);

    uint8_t intervals[size];
    const char* chordType = NULL;
    uint8_t root = 0;
    uint8_t inversion = 0;
    bool hasSeventh = false;

    for (uint8_t i = 0; i < size; i++) {
        root = sortedNotes[i];
        getIntervals(sortedNotes, size, root, intervals);
        chordType = detectChordType(intervals, size);

        if (chordType != NULL) {
            inversion = i;
            hasSeventh = (strstr(chordType, "7") != NULL) || (size >= 4);
            break;
        }
    }

    // Fallback: default to major triad on bass note if no match
    if (chordType == NULL && size == 3) {
        root = sortedNotes[0];
        chordType = "Major";
        inversion = 0;
        hasSeventh = false;
    }

    if (chordType == NULL) {
        currentChord.active = false;
        return;
    }

    currentChord.active = true;
    currentChord.type = chordType;
    currentChord.root = root % 12;
    currentChord.inversion = inversion;
    currentChord.hasSeventh = hasSeventh;

    // Default to avoid undefined state
    currentChord.third = 0;
    currentChord.fifth = 0;
    currentChord.seventh = 0;

    // Set chord tones based on detected type
    if (strstr(chordType, "Major") || strstr(chordType, "6")) {
        currentChord.third = (currentChord.root + 4) % 12;
        currentChord.fifth = (currentChord.root + 7) % 12;
        if (strstr(chordType, "Maj7")) {
            currentChord.seventh = (currentChord.root + 11) % 12;
            currentChord.hasSeventh = true;
        }
    }
    else if (strstr(chordType, "Minor") || strstr(chordType, "m")) {
        currentChord.third = (currentChord.root + 3) % 12;
        currentChord.fifth = (currentChord.root + 7) % 12;
        if (strstr(chordType, "mMaj7")) {
            currentChord.seventh = (currentChord.root + 11) % 12;
            currentChord.hasSeventh = true;
        }
        else if (strstr(chordType, "m7")) {
            currentChord.seventh = (currentChord.root + 10) % 12;
            currentChord.hasSeventh = true;
        }
    }
    else if (strstr(chordType, "Diminished")) {
        currentChord.third = (currentChord.root + 3) % 12;
        currentChord.fifth = (currentChord.root + 6) % 12;
        if (strstr(chordType, "dim7")) {
            currentChord.seventh = (currentChord.root + 9) % 12;
            currentChord.hasSeventh = true;
        }
    }
    else if (strstr(chordType, "Augmented")) {
        currentChord.third = (currentChord.root + 4) % 12;
        currentChord.fifth = (currentChord.root + 8) % 12;
        if (strstr(chordType, "aug7")) {
            currentChord.seventh = (currentChord.root + 10) % 12;
            currentChord.hasSeventh = true;
        }
    }
    else if (strstr(chordType, "7")) {  // Dominant 7th
        currentChord.third = (currentChord.root + 4) % 12;
        currentChord.fifth = (currentChord.root + 7) % 12;
        currentChord.seventh = (currentChord.root + 10) % 12;
        currentChord.hasSeventh = true;
    }
}

static uint8_t voiceIndex = 0; // Tracks which chord voice to assign next

FLASHMEM uint8_t transformNoteToChord(uint8_t inputNote) {
    if (!currentChord.active) return inputNote;

    static uint8_t voiceIndex = 0;
    uint8_t chordSize = currentChord.hasSeventh ? 4 : 3;
    uint8_t resultNote;

    // Get chord tones as pitch classes (0-11)
    uint8_t chordTones[4] =
    {
(uint8_t)(currentChord.root % 12),
(uint8_t)(currentChord.third % 12),
(uint8_t)(currentChord.fifth % 12),
(uint8_t)(currentChord.seventh % 12)
    };
    // Select chord tone based on voice index and inversion
    uint8_t toneIndex = (voiceIndex + currentChord.inversion) % chordSize;
    uint8_t destPitchClass = chordTones[toneIndex];

    // Calculate input octave and middle C reference
    int8_t inputOctave = inputNote / 12;
    const uint8_t middleC = 60;  // MIDI note 60 = middle C

    if (voiceIndex == 0) {
        // Bass voice handling
        switch (currentChord.inversion) {
        case 1: // First inversion (3rd in bass)
            resultNote = destPitchClass + (inputOctave - 1) * 12;
            break;
        case 2: // Second inversion (5th in bass)
            resultNote = destPitchClass + (inputOctave - 2) * 12;
            break;
        default: // Root position
            resultNote = destPitchClass + inputOctave * 12;
            break;
        }

        // Ensure bass stays in range (C2 to middle C)
        while (resultNote > middleC) resultNote -= 12;
        while (resultNote < 36) resultNote += 12;
    }
    else {
        // Upper voices - find best octave
        int16_t candidates[3] =
        {
            (int16_t)(destPitchClass + (inputOctave - 1) * 12), // Octave below
            (int16_t)(destPitchClass + inputOctave * 12),       // Same octave
            (int16_t)(destPitchClass + (inputOctave + 1) * 12)  // Octave above
        };
        // Start with same octave
        resultNote = candidates[1];

        // Find version closest to input that's not too high
        for (uint8_t i = 0; i < 3; i++) {
            if (abs(candidates[i] - inputNote) < abs(resultNote - inputNote)) {
                if (candidates[i] <= middleC + 12) { // Prefer <= C5
                    resultNote = candidates[i];
                }
            }
        }
        // Ensure reasonable voice leading
        if (resultNote > inputNote + 12) resultNote -= 12;
        if (resultNote < inputNote - 12) resultNote += 12;
    }
    // Final range safety check
    if (resultNote < 36) resultNote = 36;
    if (resultNote > 91) resultNote = 91;

    // Move to next voice (wrap around) - CRITICAL FIX: This must happen unconditionally
    voiceIndex = (voiceIndex + 1) % chordSize;
    return (uint8_t)resultNote;
}

FLASHMEM uint8_t transformNoteToChordMelody(uint8_t inputNote) {
    if (!currentChord.active) return inputNote;

    // Get current chord tones (0-11)
    const uint8_t root = currentChord.root % 12;
    const uint8_t third = currentChord.third % 12;
    const uint8_t fifth = currentChord.fifth % 12;
    const bool hasSeventh = currentChord.hasSeventh;
    const uint8_t seventh = hasSeventh ? currentChord.seventh % 12 : 255;

    // Input properties
    const uint8_t inputPC = inputNote % 12;
    const uint8_t inputOctave = inputNote / 12;

    // Check if input is already a chord tone
    if (inputPC == root || inputPC == third || inputPC == fifth || inputPC == seventh) {
        // For chord tones, just ensure they're not too high
        if (inputNote > 91) return inputNote - 12;
        if (inputNote < 36) return inputNote + 12;
        return inputNote;
    }

    // For non-chord tones, find the best matching chord tone
    uint8_t closestTone = root;
    uint8_t minDistance = 12;

    const uint8_t chordTones[] = { root, third, fifth, seventh };
    for (uint8_t i = 0; i < (hasSeventh ? 4 : 3); i++) {
        uint8_t distance = abs(static_cast<int8_t>(chordTones[i]) - static_cast<int8_t>(inputPC));
        distance = distance > 6 ? 12 - distance : distance;

        if (distance < minDistance) {
            minDistance = distance;
            closestTone = chordTones[i];
        }
    }

    // Create output note in same octave as input
    uint8_t outputNote = closestTone + inputOctave * 12;

    // Apply strict height limit (max +3 semitones)
    if (outputNote > inputNote + 3) {
        // Try going down an octave if that helps
        if (outputNote >= 12 && (outputNote - 12) >= (inputNote > 12 ? inputNote - 12 : 0)) {
            outputNote -= 12;
        }
        else {
            // If still too high, use input note (last resort)
            outputNote = inputNote;
        }
    }

    // Ensure we stay in playable range (36-91)
    if (outputNote > 91) outputNote -= 12;
    if (outputNote < 36) outputNote += 12;

    // Final height check (absolute guarantee)
    if (outputNote > inputNote + 3) {
        outputNote = inputNote;
    }

    return outputNote;
}

// Call this when starting a new chord transformation
FLASHMEM void resetChordTransformation() {
    voiceIndex = 0;
}

FLASHMEM bool getChordName(const uint8_t midiNotes[], uint8_t size, char* output) {
    if (size < 3) return false;

    uint8_t sortedNotes[size];
    memcpy(sortedNotes, midiNotes, size * sizeof(uint8_t));
    sortArray(sortedNotes, size);

    uint8_t intervals[size];
    const char* chordType = NULL;
    uint8_t root = 0;

    // Try each note as potential root to find a known chord
    for (uint8_t i = 0; i < size; i++) {
        root = sortedNotes[i];
        getIntervals(sortedNotes, size, root, intervals);
        chordType = detectChordType(intervals, size);
        if (chordType != NULL) break;
    }

    if (chordType == NULL) return false;

    // Determine inversion (simplified version that works for both 3 and 4 note chords)
    const char* inversion = "Root Pos.";
    uint8_t rootPitchClass = root % 12;
    uint8_t lowestPitchClass = sortedNotes[0] % 12;

    if (lowestPitchClass != rootPitchClass) {
        // Check for first inversion (third in bass)
        uint8_t third = (strchr(chordType, 'm') != NULL) ? 3 : 4;
        uint8_t thirdPitchClass = (rootPitchClass + third) % 12;

        if (lowestPitchClass == thirdPitchClass) {
            inversion = "1st Inv.";
        }
        // Check for second inversion (fifth in bass)
        else {
            uint8_t fifth = 7; // Default perfect fifth
            if (strcmp(chordType, "Diminished") == 0) fifth = 6;
            else if (strcmp(chordType, "Augmented") == 0) fifth = 8;

            uint8_t fifthPitchClass = (rootPitchClass + fifth) % 12;
            if (lowestPitchClass == fifthPitchClass) {
                inversion = "2nd Inv.";
            }
            // For 4-note chords, check for third inversion (7th in bass)
            else if (size >= 4 && (
                strstr(chordType, "7") != NULL ||
                strstr(chordType, "6") != NULL)) {
                uint8_t seventh = 10; // Default minor 7th
                if (strcmp(chordType, "Maj7") == 0 || strcmp(chordType, "mMaj7") == 0) seventh = 11;
                else if (strcmp(chordType, "dim7") == 0) seventh = 9;
                else if (strcmp(chordType, "6") == 0 || strcmp(chordType, "m6") == 0) seventh = 9;

                uint8_t seventhPitchClass = (rootPitchClass + seventh) % 12;
                if (lowestPitchClass == seventhPitchClass) {
                    inversion = "3rd Inv.";
                }
            }
        }
    }
    snprintf(output, 30, "%s %s (%s)", midiToNote(root), chordType, inversion);
    return true;
}

FLASHMEM uint8_t transformBassNote(uint8_t inputNote) {
    if (!currentChord.active) return inputNote;

    uint8_t pitchClass = inputNote % 12;
    uint8_t octave = inputNote / 12;
    static uint8_t lastBassNote = 0;

    // 1. Always prefer chord tones in bass (root > fifth > third > seventh)
    if (pitchClass == currentChord.root) return inputNote;
    if (pitchClass == currentChord.fifth) return inputNote;
    if (pitchClass == currentChord.third) return inputNote;
    if (currentChord.seventh && pitchClass == currentChord.seventh) return inputNote;

    // 2. Handle chromatic approaches (walking bass)
    if (lastBassNote != 0) {
        int8_t movement = inputNote - lastBassNote;

        if (abs(movement) == 1) {
            uint8_t nextPitchClass = (pitchClass + (movement > 0 ? 1 : -1)) % 12;

            if (nextPitchClass == currentChord.root ||
                nextPitchClass == currentChord.fifth ||
                nextPitchClass == currentChord.third ||
                (currentChord.seventh && nextPitchClass == currentChord.seventh)) {
                lastBassNote = inputNote;
                return inputNote;
            }
        }
    }
    // 3. Calculate all possible chord tones
    uint8_t rootNote = currentChord.root + octave * 12;
    uint8_t fifthNote = currentChord.fifth + octave * 12;
    uint8_t thirdNote = currentChord.third + octave * 12;
    uint8_t seventhNote = currentChord.seventh ? currentChord.seventh + octave * 12 : 0;

    // 4. Calculate distances to input note
    int16_t rootDist = abs((int16_t)rootNote - (int16_t)inputNote);
    int16_t fifthDist = abs((int16_t)fifthNote - (int16_t)inputNote);
    int16_t thirdDist = abs((int16_t)thirdNote - (int16_t)inputNote);
    int16_t seventhDist = currentChord.seventh ? abs((int16_t)seventhNote - (int16_t)inputNote) : INT16_MAX;

    // 5. Find closest chord tone
    uint8_t closestNote = rootNote;
    int16_t minDist = rootDist;

    if (fifthDist < minDist) {
        minDist = fifthDist;
        closestNote = fifthNote;
    }
    if (thirdDist < minDist) {
        minDist = thirdDist;
        closestNote = thirdNote;
    }
    if (currentChord.seventh && seventhDist < minDist) {
        closestNote = seventhNote;
    }

    // 6. Weighted random selection favoring closest note
    uint8_t random = rand() % 100;

    // 70% chance for closest note
    if (random < 70) {
        lastBassNote = closestNote;
        return closestNote;
    }
    // 20% chance for root (harmonic stability)
    else if (random < 90) {
        lastBassNote = rootNote;
        return rootNote;
    }
    // 10% chance for other tones
    else {
        // Create array of available tones
        uint8_t availableTones[4] = { rootNote, fifthNote, thirdNote, 0 };
        uint8_t toneCount = 3;
        if (currentChord.seventh) {
            availableTones[3] = seventhNote;
            toneCount = 4;
        }

        // Select random tone (excluding the closest one which we already tried)
        uint8_t selected;
        do {
            selected = availableTones[rand() % toneCount];
        } while (selected == closestNote && toneCount > 1);

        lastBassNote = selected;
        return selected;
    }
}

FLASHMEM uint8_t getChordRootNote(uint8_t inputNote)
{
    if (!currentChord.active) return inputNote;

    uint8_t inputOctave = inputNote / 12;

    // Extract the root's pitch class (C=0, C#=1, ..., B=11)
    uint8_t rootPitchClass = currentChord.root % 12;

    // Apply the input octave to the root pitch class
    return rootPitchClass + (inputOctave * 12);

}

FLASHMEM void suggestNextChords(ChordInfo current, SuggestedChord suggestions[6]) {
    if (!current.active) {
        memset(suggestions, 0, 6 * sizeof(SuggestedChord));
        return;
    }

    uint8_t root = current.root;
    // bool isSeventh = (current.seventh != 0);

     // Determine chord quality
    bool isMajor = (strstr(current.type, "Major") != NULL);
    // bool isMinor = (strstr(current.type, "Minor") != NULL);
    bool isDominant = (strstr(current.type, "7th") != NULL);

    // Clear and initialize all suggestions as invalid
    memset(suggestions, 0, 6 * sizeof(SuggestedChord));
    uint8_t valid_count = 0;

    // Helper function to add a triad suggestion
#define ADD_TRIAD_SUGGESTION(base, name_fmt, third_offset) \
        if (valid_count < 6) { \
            suggestions[valid_count].notes[0] = (base) % 12 + 60; \
            suggestions[valid_count].notes[1] = (base + (third_offset)) % 12 + 60; \
            suggestions[valid_count].notes[2] = (base + 7) % 12 + 60; \
            snprintf(suggestions[valid_count].name, 20, (name_fmt), midiToNote((base) % 12)); \
            valid_count++; \
        }

    // 1. DOMINANT (V chord) - Perfect cadence
    uint8_t dominant = (root + 7) % 12;
    ADD_TRIAD_SUGGESTION(dominant, "%s", isMajor ? 4 : 3);

    // 2. SUBDOMINANT (IV or iv) - Plagal cadence
    uint8_t subdominant = (root + 5) % 12;
    ADD_TRIAD_SUGGESTION(subdominant, isMajor ? "%s" : "%sm", isMajor ? 4 : 3);

    // 3. RELATIVE CHORD (vi or III)
    uint8_t relative = isMajor ? (root + 9) % 12 : (root + 3) % 12;
    ADD_TRIAD_SUGGESTION(relative, isMajor ? "%sm" : "%s", isMajor ? 3 : 4);

    // 4. PARALLEL CHORD (mode change)
    ADD_TRIAD_SUGGESTION(root, isMajor ? "%sm" : "%s", isMajor ? 3 : 4);

    // 5. DIATONIC STEP MOVEMENT (II or ii)
    uint8_t step = (root + 2) % 12;
    ADD_TRIAD_SUGGESTION(step, isMajor ? "%s" : "%sm", isMajor ? 4 : 3);

    // 6. CHROMATIC NEIGHBOR (up or down semitone)
    uint8_t chrom = (root + (rand() % 2 ? 1 : 11)) % 12;
    ADD_TRIAD_SUGGESTION(chrom, "%s", 4); // Default to major for chromatic

    // If we have a dominant chord, replace last suggestion with tritone sub
    if (isDominant && valid_count > 0) {
        uint8_t tritoneSub = (root + 6) % 12;
        suggestions[valid_count - 1].notes[0] = tritoneSub + 60;
        suggestions[valid_count - 1].notes[1] = (tritoneSub + 4) % 12 + 60;
        suggestions[valid_count - 1].notes[2] = (tritoneSub + 7) % 12 + 60;
        snprintf(suggestions[valid_count - 1].name, 20, "%s7", midiToNote(tritoneSub));
    }

    // Ensure all notes are valid (non-zero)
    for (int i = 0; i < 6; i++) {
        if (suggestions[i].notes[0] == 0) {
            // Fallback to root chord if any suggestion is invalid
            suggestions[i].notes[0] = root + 60;
            suggestions[i].notes[1] = (root + (isMajor ? 4 : 3)) % 12 + 60;
            suggestions[i].notes[2] = (root + 7) % 12 + 60;
            snprintf(suggestions[i].name, 20, isMajor ? "%s" : "%sm", midiToNote(root));
        }
    }
}
