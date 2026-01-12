#include "ui_liveseq_pianoroll.h"
#include "UI.h"
#include "config.h"
#include "touch.h"
#include "ILI9341_t3n.h"
#include "LCDMenuLib2.h"
#include "ui_livesequencer.h"
#include "MD_REncoder.h"
#include <map>

extern LCDMenuLib2 LCDML;
extern ILI9341_t3n display;
extern MD_REncoder ENCODER[NUM_ENCODER];

UI_LiveSeq_PianoRoll* ui_pianoroll;



FLASHMEM void UI_LiveSeq_PianoRoll::storeCurrentStateForUndo() {
  quantizationUndoState.clear();
  
  // Only store state for regular tracks (0-11) or song notes track (12)
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    return; // Skip mute automation track
  }
  
  quantizationUndoState.track = data.activeTrack;
  quantizationUndoState.layer = selectedLayer;
  quantizationUndoState.quantizeLevel = quantizeLevel;
  
  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Store all song notes in selected layer for undo
    for (const auto& patternEntry : data.songEvents) {
      uint16_t songPatternIndex = patternEntry.first;
      const auto& patternEvents = patternEntry.second; // This is a vector<MidiEvent>
      
      // Collect NoteOn events in selected layer
      for (size_t i = 0; i < patternEvents.size(); i++) {
        const auto& event = patternEvents[i];
        if (event.event == midi::NoteOn && event.layer == selectedLayer) {
          // Store this NoteOn
          OriginalNoteState noteState;
          noteState.isSongMode = true;
          noteState.songPatternIndex = songPatternIndex;
          noteState.noteOn = event;
          noteState.indexInData = i;
          
          // Find matching NoteOff
          bool foundNoteOff = false;
          for (size_t j = 0; j < patternEvents.size(); j++) {
            const auto& offEvent = patternEvents[j];
            if (offEvent.event == midi::NoteOff &&
                offEvent.layer == event.layer &&
                offEvent.track == event.track &&
                offEvent.note_in == event.note_in &&
                offEvent.note_in_velocity == event.note_in_velocity) {
              
              noteState.noteOff = offEvent;
              quantizationUndoState.originalNotes.push_back(noteState);
              foundNoteOff = true;
              break;
            }
          }
          
          // If no NoteOff found, create a default one
          if (!foundNoteOff) {
            LiveSequencer::MidiEvent defaultOff = event;
            defaultOff.event = midi::NoteOff;
            defaultOff.patternMs = event.patternMs + (data.patternLengthMs / 16); // 1/16 note length
            noteState.noteOff = defaultOff;
            quantizationUndoState.originalNotes.push_back(noteState);
          }
        }
      }
    }
  } 
  else if (data.activeTrack < SONG_MODE_NOTES_TRACK) {
    // Store pattern track notes for undo - use the current notePairs
    for (size_t i = 0; i < notePairs.size(); i++) {
      const auto& notePair = notePairs[i];
      if (notePair.noteOn.layer == selectedLayer) {
        OriginalNoteState noteState;
        noteState.isSongMode = false;
        noteState.songPatternIndex = 0; // Not used for pattern tracks
        noteState.noteOn = notePair.noteOn;
        noteState.noteOff = notePair.noteOff;
        noteState.indexInData = i;
        quantizationUndoState.originalNotes.push_back(noteState);
      }
    }
  }
  
  quantizationUndoState.hasUndoData = !quantizationUndoState.originalNotes.empty();
}

FLASHMEM void UI_LiveSeq_PianoRoll::applyQuantizationUndo() {
  if (!quantizationUndoState.hasUndoData) {
    return;
  }
  
  // Only apply undo if we're on the same track and layer
  if (data.activeTrack != quantizationUndoState.track || 
      selectedLayer != quantizationUndoState.layer) {
    return;
  }
  
  bool changed = false;
  
  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // For song mode: restore original notes
    // First remove all notes in this layer (both NoteOn and NoteOff)
    for (auto& patternEntry : data.songEvents) {
      auto& patternEvents = patternEntry.second; // This is a vector<MidiEvent>
      
      // Remove NoteOn events in selected layer
      for (size_t i = 0; i < patternEvents.size(); ) {
        if (patternEvents[i].event == midi::NoteOn && patternEvents[i].layer == selectedLayer) {
          patternEvents.erase(patternEvents.begin() + i);
          changed = true;
        } else {
          i++;
        }
      }
      
      // Remove NoteOff events in selected layer
      for (size_t i = 0; i < patternEvents.size(); ) {
        if (patternEvents[i].event == midi::NoteOff && patternEvents[i].layer == selectedLayer) {
          patternEvents.erase(patternEvents.begin() + i);
          changed = true;
        } else {
          i++;
        }
      }
    }
    
    // Now add back the original notes
    for (const auto& noteState : quantizationUndoState.originalNotes) {
      // Ensure the song pattern exists
      if (data.songEvents.find(noteState.songPatternIndex) == data.songEvents.end()) {
        data.songEvents[noteState.songPatternIndex] = {};
      }
      
      // Add NoteOn and NoteOff back
      data.songEvents[noteState.songPatternIndex].push_back(noteState.noteOn);
      data.songEvents[noteState.songPatternIndex].push_back(noteState.noteOff);
      changed = true;
    }
  }
  else if (data.activeTrack < SONG_MODE_NOTES_TRACK) {
    // For pattern tracks: mark all current notes in this layer for deletion
    // and add back the original notes
    
    // Mark current notes in this layer for deletion
    for (auto& notePair : notePairs) {
      if (notePair.noteOn.layer == selectedLayer) {
        notePair.noteOn.event = midi::InvalidType;
        notePair.noteOff.event = midi::InvalidType;
        changed = true;
      }
    }
    
    // Add back original notes using liveSeq.addNotePair
    for (const auto& noteState : quantizationUndoState.originalNotes) {
      // Add the note back through the sequencer
      liveSeq.addNotePair(noteState.noteOn, noteState.noteOff);
      changed = true;
    }
  }
  
  if (changed) {
    // Request sorting
    liveSeq.requestSortEvents();
    
    // Reload and redraw
    reloadNotes();
    drawFlags |= DRAW_CONTENT_FULL;
    
    // Clear undo state
    quantizationUndoState.clear();
    
    // Show undo confirmation
    display.console = true;
    display.setTextSize(1);
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.fillRect(GRID.X[2] - 8, CONTENT_HEIGHT - 40, 80, CHAR_height_small, HEADER_BG_COLOR);
    display.setCursor(GRID.X[2] - 8, CONTENT_HEIGHT - 40);
    
    if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
      display.printf("SONG L%d UNDO!", selectedLayer + 1);
    } else {
      display.printf("TRACK%d LAYER%d UNDO!", data.activeTrack + 1, selectedLayer + 1);
    }
    delay(1000);
    
    // Update Play button
    if (buttonPlay) {
      buttonPlay->drawNow();
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::clearUndoState() {
  // Only clear if we actually have undo data
  if (quantizationUndoState.hasUndoData) {
    quantizationUndoState.clear();
    // Also update the Play button to show it no longer has undo
    if (buttonPlay) {
      buttonPlay->drawNow();
    }
  }
}

///

FLASHMEM void UI_LiveSeq_PianoRoll::quantizeAllNotesInCurrentTrack() {
  if (quantizeLevel == QUANTIZE_OFF) {
    return; // No quantization selected
  }
  
  // Only store undo for regular tracks or song notes track
  if (data.activeTrack != SONG_MODE_MUTE_TRACK) {
    // Store current state for undo BEFORE quantization
    storeCurrentStateForUndo();
  }

  // Get grid size from current quantization level
  uint32_t gridSize = getQuantizationGridStep();

  // Check what type of track we're editing
  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Song mode: quantize all song notes in selected layer
    quantizeAllSongNotes(gridSize);
  }
  else if (data.activeTrack < SONG_MODE_NOTES_TRACK) {
    // Pattern mode: quantize notes in the active pattern track in selected layer
    quantizeAllPatternNotes(gridSize);
  }
  // Note: Mute track (SONG_MODE_MUTE_TRACK) is handled separately and not here

  // Force complete redraw
  drawFlags |= DRAW_CONTENT_FULL | DRAW_NOTES | DRAW_VERTICAL_BARS | DRAW_BACKGROUND;

  // Request event sorting for proper playback
  liveSeq.requestSortEvents();
  
  // Update the Play button to show it can be used for undo
  if (quantizationUndoState.hasUndoData && buttonPlay) {
    buttonPlay->drawNow();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::quantizeAllSongNotes(uint32_t gridSize) {
  bool changed = false;

  // Calculate minimum note length (1/16 note)
  uint32_t minNoteLength = data.patternLengthMs / 16;

  // Iterate through song patterns
  auto patternIt = data.songEvents.begin();
  while (patternIt != data.songEvents.end()) {
    uint16_t songPatternIndex = patternIt->first;
    auto& events = patternIt->second;

    // First pass: collect all NoteOn events that need quantization
    struct NoteOnToQuantize {
      LiveSequencer::MidiEvent* eventPtr;
      uint32_t originalTime;
      uint32_t noteLength; // Store the original note length
      uint16_t songPatternIndex;
      size_t eventIndex;
    };

    std::vector<NoteOnToQuantize> noteOnsToQuantize;

    // Collect NoteOn events first
    for (size_t i = 0; i < events.size(); i++) {
      auto& event = events[i];

      // Only quantize note-on events in the SELECTED LAYER
      if (event.event == midi::NoteOn && event.layer == selectedLayer) {
        // Get current pattern-relative time
        uint32_t patternRelativeTime = getPatternRelativeTime(event);

        // Store note-on info for quantization
        NoteOnToQuantize info;
        info.eventPtr = &event;
        info.originalTime = patternRelativeTime;
        info.songPatternIndex = songPatternIndex;
        info.eventIndex = i;
        noteOnsToQuantize.push_back(info);
      }
    }

    // Second pass: find matching NoteOff events and store their times
    for (auto& noteOnInfo : noteOnsToQuantize) {
      // Find matching NoteOff for this NoteOn
      bool foundNoteOff = false;

      for (size_t i = 0; i < events.size(); i++) {
        auto& event = events[i];

        if (event.event == midi::NoteOff &&
          event.layer == noteOnInfo.eventPtr->layer &&
          event.track == noteOnInfo.eventPtr->track &&
          event.note_in == noteOnInfo.eventPtr->note_in &&
          event.note_in_velocity == noteOnInfo.eventPtr->note_in_velocity) {

          // Found the matching NoteOff
          uint32_t noteOffTime = getPatternRelativeTime(event);
          noteOnInfo.noteLength = noteOffTime - noteOnInfo.originalTime;

          // Ensure minimum note length (at least 1/16 note)
          if (noteOnInfo.noteLength < minNoteLength) {
            noteOnInfo.noteLength = minNoteLength;
          }
          foundNoteOff = true;
          break;
        }
      }

      if (!foundNoteOff) {
        // No NoteOff found, use minimum length
        noteOnInfo.noteLength = minNoteLength;
      }
    }

    // Third pass: actually quantize the NoteOn events
    for (const auto& noteOnInfo : noteOnsToQuantize) {
      uint32_t patternRelativeTime = noteOnInfo.originalTime;

      // Quantize NoteOn time only
      uint32_t quantizedTimeOn = (patternRelativeTime / gridSize) * gridSize;
      uint32_t remainder = patternRelativeTime % gridSize;

      if (remainder > gridSize / 2) {
        quantizedTimeOn += gridSize;
      }

      // Ensure it stays within 4-bar cycle
      uint32_t maxTime = data.numberOfBars * data.patternLengthMs;
      if (quantizedTimeOn >= maxTime) {
        quantizedTimeOn = maxTime - gridSize;
      }

      // Only update if changed
      if (quantizedTimeOn != patternRelativeTime) {
        // Calculate what the new note-off time would be if we keep the same length
        uint32_t quantizedTimeOff = quantizedTimeOn + noteOnInfo.noteLength;
        
        // Check if note-off would be invalid (before or at note-on)
        bool needToAdjustNoteOff = (quantizedTimeOff <= quantizedTimeOn);
        
        // Update NoteOn event
        setPatternRelativeTime(*noteOnInfo.eventPtr, quantizedTimeOn, noteOnInfo.songPatternIndex);

        // Find and update matching NoteOff only if needed
        if (needToAdjustNoteOff) {
          // Only adjust note-off if it would be invalid
          quantizedTimeOff = quantizedTimeOn + gridSize; // Use grid size as minimum
          
          // Find matching NoteOff
          bool foundNoteOff = false;
          for (size_t i = 0; i < events.size(); i++) {
            auto& event = events[i];

            if (event.event == midi::NoteOff &&
              event.layer == noteOnInfo.eventPtr->layer &&
              event.track == noteOnInfo.eventPtr->track &&
              event.note_in == noteOnInfo.eventPtr->note_in &&
              event.note_in_velocity == noteOnInfo.eventPtr->note_in_velocity) {

              // Update NoteOff only if adjustment is needed
              setPatternRelativeTime(event, quantizedTimeOff, noteOnInfo.songPatternIndex);
              foundNoteOff = true;
              changed = true;
              break;
            }
          }

          if (!foundNoteOff) {
            // Create NoteOff if it doesn't exist (shouldn't happen)
            changed = true;
          }
        }
        // If note-off doesn't need adjustment, leave it as is (keeping original length)
        changed = true;
      }
    }

    ++patternIt;
  }

  // Remove duplicates after quantization (only in selected layer)
  if (changed) {
    removeDuplicateNotesInLayer(selectedLayer);

    // Show feedback with layer info
    display.console = true;
    display.setTextSize(1);
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.fillRect(GRID.X[2] - 8, CONTENT_HEIGHT - 40, 80, CHAR_height_small, HEADER_BG_COLOR);
    display.setCursor(GRID.X[2] - 8, CONTENT_HEIGHT - 40);
    display.printf("SONG L%d QUANTIZED!", selectedLayer + 1);
    delay(1000);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::quantizeAllPatternNotes(uint32_t gridSize) {
  bool notesQuantized = false;

  // Calculate minimum note length (1/16 note)
  uint32_t minNoteLength = data.patternLengthMs / 16;

  // First, collect all NoteOn events in SELECTED LAYER with their original lengths
  struct NoteInfo {
    LiveSequencer::MidiEvent* noteOnPtr;
    LiveSequencer::MidiEvent* noteOffPtr;
    uint32_t originalTimeOn;
    uint32_t originalTimeOff;
    uint32_t noteLength;
    size_t index;
  };

  std::vector<NoteInfo> noteInfos;

  // Iterate through notePairs using index
  for (size_t i = 0; i < notePairs.size(); i++) {
    LiveSequencer::NotePair& note = notePairs[i];

    // Only process notes in the SELECTED LAYER
    if (note.noteOn.event == midi::NoteOn && note.noteOn.layer == selectedLayer) {
      // Calculate absolute times
      uint32_t timeOn = getEventTime(note.noteOn);
      uint32_t timeOff = getEventTime(note.noteOff);

      // Ensure valid note length (at least 1/16 note)
      uint32_t noteLength = (timeOff > timeOn) ? (timeOff - timeOn) : minNoteLength;

      if (noteLength < minNoteLength) {
        noteLength = minNoteLength;
      }

      // Store note information
      NoteInfo info;
      info.noteOnPtr = &note.noteOn;
      info.noteOffPtr = &note.noteOff;
      info.originalTimeOn = timeOn;
      info.originalTimeOff = timeOn + noteLength; // Use adjusted note length
      info.noteLength = noteLength;
      info.index = i;
      noteInfos.push_back(info);
    }
  }

  // Find and keep only the highest velocity notes for each unique position in SELECTED LAYER
  std::vector<bool> isBestNote(noteInfos.size(), false);

  for (size_t i = 0; i < noteInfos.size(); i++) {
    const NoteInfo& info1 = noteInfos[i];
    bool isBest = true;

    // Check if this is the best note for its position
    for (size_t j = 0; j < noteInfos.size(); j++) {
      if (i == j) continue;

      const NoteInfo& info2 = noteInfos[j];

      // Check if same quantized position (after quantization)
      uint32_t quantizedTime1 = (info1.originalTimeOn / gridSize) * gridSize;
      uint32_t remainder1 = info1.originalTimeOn % gridSize;
      if (remainder1 > gridSize / 2) {
        quantizedTime1 += gridSize;
      }

      uint32_t quantizedTime2 = (info2.originalTimeOn / gridSize) * gridSize;
      uint32_t remainder2 = info2.originalTimeOn % gridSize;
      if (remainder2 > gridSize / 2) {
        quantizedTime2 += gridSize;
      }

      if (info1.noteOnPtr->layer == info2.noteOnPtr->layer &&
        info1.noteOnPtr->note_in == info2.noteOnPtr->note_in &&
        quantizedTime1 == quantizedTime2) {

        // Compare velocities
        if (info2.noteOnPtr->note_in_velocity > info1.noteOnPtr->note_in_velocity) {
          isBest = false;
          break;
        }
        else if (info2.noteOnPtr->note_in_velocity == info1.noteOnPtr->note_in_velocity && j < i) {
          // Same velocity, keep the first one
          isBest = false;
          break;
        }
      }
    }

    isBestNote[i] = isBest;
  }

  // Now process the notes in SELECTED LAYER, keeping only the best ones
  for (size_t i = 0; i < notePairs.size(); i++) {
    LiveSequencer::NotePair& note = notePairs[i];

    // Only process notes in the SELECTED LAYER
    if (note.noteOn.event == midi::NoteOn && note.noteOn.layer == selectedLayer) {
      // Find if this note is in our noteInfos list
      bool foundInList = false;
      size_t infoIndex = 0;
      for (size_t j = 0; j < noteInfos.size(); j++) {
        if (noteInfos[j].index == i) {
          foundInList = true;
          infoIndex = j;
          break;
        }
      }

      if (foundInList) {
        const NoteInfo& info = noteInfos[infoIndex];

        // Check if this is the best note for this position
        if (isBestNote[infoIndex]) {
          // This is the best note for this position - quantize only the NoteOn
          uint32_t quantizedTimeOn = (info.originalTimeOn / gridSize) * gridSize;
          uint32_t remainder = info.originalTimeOn % gridSize;

          if (remainder > gridSize / 2) {
            quantizedTimeOn += gridSize;
          }

          if (quantizedTimeOn != info.originalTimeOn) {
            // Update NoteOn to quantized position
            setEventTime(note.noteOn, quantizedTimeOn);

            // Calculate what the new note-off time would be with original length
            uint32_t quantizedTimeOff = quantizedTimeOn + info.noteLength;

            // Check if note-off would be invalid (before or at note-on)
            bool needToAdjustNoteOff = (quantizedTimeOff <= quantizedTimeOn);
            
            if (needToAdjustNoteOff) {
              // Only adjust note-off if it would be invalid
              quantizedTimeOff = quantizedTimeOn + gridSize; // Use grid size as minimum
            }
            // Otherwise keep the original length (don't modify note-off)

            // Update NoteOff only if adjustment is needed
            if (needToAdjustNoteOff) {
              setEventTime(note.noteOff, quantizedTimeOff);
            }
            notesQuantized = true;
          }
        }
        else {
          // This is a duplicate with lower velocity in SELECTED LAYER - mark for removal
          note.noteOn.event = midi::InvalidType;
          note.noteOff.event = midi::InvalidType;
          notesQuantized = true;
        }
      }
    }
  }

  // Clean up invalidated notes in SELECTED LAYER
  if (notesQuantized) {
    // Remove notes marked as invalid
    std::vector<LiveSequencer::NotePair> validNotes;
    for (size_t i = 0; i < notePairs.size(); i++) {
      if (notePairs[i].noteOn.event != midi::InvalidType) {
        validNotes.push_back(notePairs[i]);
      }
    }
    notePairs.clear();
    for (size_t i = 0; i < validNotes.size(); i++) {
      notePairs.push_back(validNotes[i]);
    }

    // Show feedback with layer info
    display.console = true;
    display.setTextSize(1);
    display.setTextColor(GREEN, COLOR_BACKGROUND);
    display.fillRect(GRID.X[2] - 8, CONTENT_HEIGHT - 40, 80, CHAR_height_small, HEADER_BG_COLOR);
    display.setCursor(GRID.X[2] - 8, CONTENT_HEIGHT - 40);
    display.printf("TRACK%d LAYER%d QUANTIZED!", data.activeTrack + 1, selectedLayer + 1);
    delay(1000);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::removeDuplicateNotesInLayer(uint8_t layer) {
  // Remove duplicate notes from songEvents in specific layer after quantization
  // Keep the note with higher velocity when duplicates are found

  // Iterate through song patterns
  auto patternIt = data.songEvents.begin();
  while (patternIt != data.songEvents.end()) {
    auto& events = patternIt->second;

    // Mark duplicates as invalid
    for (size_t i = 0; i < events.size(); i++) {
      auto& event1 = events[i];

      // Only check note events in the specified layer
      if (!((event1.event == midi::NoteOn || event1.event == midi::NoteOff) &&
        event1.layer == layer)) {
        continue;
      }

      // Skip if already invalid
      if (event1.event == midi::InvalidType) continue;

      for (size_t j = i + 1; j < events.size(); j++) {
        auto& event2 = events[j];

        // Only check note events in the specified layer
        if (!((event2.event == midi::NoteOn || event2.event == midi::NoteOff) &&
          event2.layer == layer)) {
          continue;
        }

        // Skip if already invalid
        if (event2.event == midi::InvalidType) continue;

        // Check if these are the same note (same properties)
        if (event1.track == event2.track &&
          event1.layer == event2.layer &&
          event1.note_in == event2.note_in &&
          event1.event == event2.event &&
          event1.patternNumber == event2.patternNumber &&
          event1.patternMs == event2.patternMs) {

          // Duplicate found! Compare velocities for NoteOn events
          if (event1.event == midi::NoteOn) {
            if (event1.note_in_velocity < event2.note_in_velocity) {
              // event1 has lower velocity, mark it as invalid
              event1.event = midi::InvalidType;
              break; // No need to check further for event1
            }
            else {
              // event2 has lower or equal velocity, mark it as invalid
              event2.event = midi::InvalidType;
            }
          }
          else {
            // For NoteOff, mark the later one as invalid
            event2.event = midi::InvalidType;
          }
        }
      }
    }

    // Remove invalid events
    size_t writeIdx = 0;
    for (size_t readIdx = 0; readIdx < events.size(); readIdx++) {
      if (events[readIdx].event != midi::InvalidType) {
        if (writeIdx != readIdx) {
          events[writeIdx] = events[readIdx];
        }
        writeIdx++;
      }
    }

    // Trim vector to new size
    if (writeIdx < events.size()) {
      // Resize vector (should handle custom allocator correctly)
      while (events.size() > writeIdx) {
        events.pop_back();
      }
    }

    // Check if pattern is now empty
    if (events.empty()) {
      // Remove empty pattern
      auto emptyIt = patternIt;
      ++patternIt;
      data.songEvents.erase(emptyIt);
    }
    else {
      ++patternIt;
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawBackgroundForNote(uint8_t note, uint16_t fromX, uint16_t toX) const {
  const uint8_t noteIndex = view.highestNote - note - 1;
  const uint16_t y = round(noteIndex * view.noteHeight);
  uint16_t bgColor = (note % 2) ? GREY4 : COLOR_BACKGROUND;

  display.console = true;
  const uint16_t width = toX - fromX;
  const uint16_t xFrom = std::max(ROLL_WIDTH, fromX);

  if (width > 1) {
    display.fillRect(xFrom, y, toX - fromX, ceil(view.noteHeight), bgColor);
  }
  else {
    display.drawLine(xFrom, y, xFrom, y + ceil(view.noteHeight) - 1, bgColor);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::addNoteUpdateRegion(uint32_t timeOn, uint32_t timeOff, uint8_t notePitch, bool includeBars) {
  uint16_t x = getNoteCoord(timeOn);

  // Calculate width
  uint32_t lenMs = (timeOff > timeOn) ? (timeOff - timeOn) : 1;
  uint16_t w = std::max(1, int(roundf(lenMs * view.msToPix)));

  UpdateRange region;
  region.fromX = std::max(ROLL_WIDTH, (uint16_t)(x - 3));
  region.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + w + 3));
  region.notes = { notePitch };
  drawRegionsX.push_back(region);

  if (includeBars) {
    UpdateRange barRegion = region;
    barRegion.notes = {};
    drawRegionsX.push_back(barRegion);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::addFullUpdateRegion(uint32_t timeOn1, uint32_t timeOff1, uint32_t timeOn2, uint32_t timeOff2) {
  uint16_t x1 = getNoteCoord(timeOn1);
  uint16_t x2 = getNoteCoord(timeOn2);

  // Calculate widths
  uint32_t lenMs1 = (timeOff1 > timeOn1) ? (timeOff1 - timeOn1) : 1;
  uint32_t lenMs2 = (timeOff2 > timeOn2) ? (timeOff2 - timeOn2) : 1;
  uint16_t w1 = std::max(1, int(roundf(lenMs1 * view.msToPix)));
  uint16_t w2 = std::max(1, int(roundf(lenMs2 * view.msToPix)));

  UpdateRange region;
  region.fromX = std::min(x1, x2) - 5;
  region.toX = std::max(x1 + w1, x2 + w2) + 5;
  region.notes = {};
  drawRegionsX.push_back(region);
}

FLASHMEM uint32_t UI_LiveSeq_PianoRoll::getQuantizationGridStep() const {
  switch (quantizeLevel) {
  case QUANTIZE_OFF:
    return 2; // 2ms = effectively no quantization (better for smooth movement)
  case QUANTIZE_64:
    return data.patternLengthMs / 64;
  case QUANTIZE_32:
    return data.patternLengthMs / 32;
  case QUANTIZE_16T: // 16th note triplets (added before regular 16ths)
    return data.patternLengthMs / 24; // 3 notes in time of 8th note = patternLength/8/3 = patternLength/24
  case QUANTIZE_16:
    return data.patternLengthMs / 16;
  case QUANTIZE_8T:  // 8th note triplets
    return data.patternLengthMs / 12; // 3 notes in time of quarter note = patternLength/4/3 = patternLength/12
  case QUANTIZE_8:
    return data.patternLengthMs / 8;
  case QUANTIZE_4:
    return data.patternLengthMs / 4;
  default:
    return 2; // Fallback to no quantization
  }
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::getActualBarForNote(const LiveSequencer::NotePair& note) const {
  if (!data.isSongMode || data.useGridSongMode) {
    return note.noteOn.patternNumber;
  }

  // For song mode, find which map entry this event belongs to
  for (const auto& patternEntry : data.songEvents) {
    const uint16_t songPatternIndex = patternEntry.first;
    const auto& events = patternEntry.second;

    // Look for matching event by content
    for (const auto& event : events) {
      if (event.track == note.noteOn.track &&
        event.layer == note.noteOn.layer &&
        event.note_in == note.noteOn.note_in &&
        event.event == note.noteOn.event &&
        event.patternMs == note.noteOn.patternMs &&
        event.note_in_velocity == note.noteOn.note_in_velocity) {
        // Found it! Calculate absolute bar
        return (songPatternIndex * data.numberOfBars) + event.patternNumber;
      }
    }
  }

  // Fallback: use the pattern number from the note (for editing context)
  return note.noteOn.patternNumber;
}


FLASHMEM uint32_t UI_LiveSeq_PianoRoll::getPatternRelativeTime(const LiveSequencer::MidiEvent& e) const {
  if (data.isSongMode && !data.useGridSongMode) {
    // Relative to 4-bar cycle: bar index * barLength + time within that bar
    return (e.patternNumber * data.patternLengthMs) + e.patternMs;
  }
  else {
    return getEventTime(e);
  }
}


FLASHMEM void UI_LiveSeq_PianoRoll::setPatternRelativeTime(LiveSequencer::MidiEvent& e, uint32_t time, uint16_t songPatternIndex) {
  if (data.isSongMode && !data.useGridSongMode) {
    // time is 0..(4*barLength)
    uint32_t absoluteTime = (songPatternIndex * data.numberOfBars * data.patternLengthMs) + time;

    e.patternNumber = (absoluteTime / data.patternLengthMs) % data.numberOfBars; // 0..3
    e.patternMs = absoluteTime % data.patternLengthMs;                       // offset inside bar
  }
  else {
    setEventTime(e, time);
  }
}


FLASHMEM uint16_t UI_LiveSeq_PianoRoll::getSongPatternForEvent(const LiveSequencer::MidiEvent& e) const {
  if (!data.isSongMode || data.useGridSongMode) {
    return 0;
  }

  // Find which song pattern this event belongs to
  for (const auto& patternEntry : data.songEvents) {
    const uint16_t songPatternIndex = patternEntry.first;
    const auto& events = patternEntry.second;

    for (const auto& event : events) {
      if (event.track == e.track &&
        event.layer == e.layer &&
        event.note_in == e.note_in &&
        event.event == e.event &&
        event.patternMs == e.patternMs &&
        event.note_in_velocity == e.note_in_velocity) {
        return songPatternIndex;
      }
    }
  }
  return 0;
}

FLASHMEM UI_LiveSeq_PianoRoll::UI_LiveSeq_PianoRoll(LiveSequencer& sequencer, LiveSequencer::LiveSeqData& d) :
  liveSeq(sequencer),
  data(d),
  isRunningHere(d.isRunning),
  selectedLayer(0) {
  ui_pianoroll = this;

  // Initialize editingNote safely
  editingNote.reset();

  layerColors.push_back(COLOR_ARP); // layer 0
  layerColors.push_back(COLOR_CHORDS);
  layerColors.push_back(COLOR_DRUMS);
  layerColors.push_back(COLOR_PITCHSMP);
  initGUI();

  // initial view settings
  view.cursorPos = ROLL_WIDTH;
  view.cursorPosPrev = ROLL_WIDTH;
  view.startBar = 0;
  view.startOctave = 0;
  view.numOctaves = 5;
  view.numNotes = view.numOctaves * 12;
  view.noteHeight = CONTENT_HEIGHT / float(view.numNotes);
  view.lowestNote = 24 + view.startOctave * 12;
  view.highestNote = (view.lowestNote + view.numNotes);

  recordSteps = new EditableValueVector<uint8_t>(stepRecordSteps, { 1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64 }, 16, [](auto* v) {});
}



FLASHMEM uint16_t UI_LiveSeq_PianoRoll::getMaxBarsForCurrentMode() const {
  if (data.isSongMode && !data.useGridSongMode) {
    uint16_t maxBar = 0;

    // Check if songEvents is empty
    if (data.songEvents.empty()) {
      return 1; // At least 1 bar even if empty
    }

    for (const auto& patternEntry : data.songEvents) {
      const uint16_t songPatternIndex = patternEntry.first; // Map key

      // Skip empty event lists
      if (patternEntry.second.empty()) {
        continue;
      }

      for (const auto& event : patternEntry.second) {
        // Calculate absolute bar from map key and cyclic pattern number
        uint16_t absoluteBar = (songPatternIndex * data.numberOfBars) + event.patternNumber;

        if (absoluteBar > maxBar) {
          maxBar = absoluteBar;
        }
      }
    }

    // Return maxBar + 1 to convert from index to count, but ensure at least 1
    return (maxBar == 0) ? 1 : (maxBar + 1);
  }
  else {
    return data.numberOfBars;
  }
}

FLASHMEM void UI_func_liveseq_graphic(uint8_t param) {  // for Livesequencer
  ui_pianoroll->processLCDM(param);
}

FLASHMEM void handle_touchscreen_pianoroll(void) {
  ui_pianoroll->handleTouchScreen();
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleTouchScreen() {
  for (TouchButton* b : buttons) {
    b->processPressed();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawLayersIndicator(void) {
  display.setTextSize(1);
  static constexpr uint16_t layerViewSizeX = ROLL_WIDTH / 2;
  static constexpr uint16_t layerViewY = CONTENT_HEIGHT + LINE_HEIGHT;

  if (data.activeTrack < SONG_MODE_NOTES_TRACK) {
    // Pattern mode: show layers for current pattern track
    for (uint8_t l = 0; l < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; l++) {
      const uint16_t x = GRID.X[0] + (l % 2) * (layerViewSizeX);
      const uint16_t y = layerViewY + (l / 2) * (HEADER_HEIGHT / 2);
      const bool isMuted = data.tracks[data.activeTrack].layerMutes & (1 << l);
      const bool hasNotes = data.trackSettings[data.activeTrack].layerCount > l;

      bool isActiveLayer = (l == selectedLayer);

      display.console = true;
      if (hasNotes) {
        if (isMuted) {
          display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), COLOR_BACKGROUND);
          display.drawRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), layerColors[l]);
          display.setTextColor(layerColors[l], COLOR_BACKGROUND);
        }
        else if (isActiveLayer) {
          // Active layer: filled with color
          display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), layerColors[l]);
          display.setTextColor(COLOR_BACKGROUND, layerColors[l]);
        }
        else {
          // Non-active layer: outline only
          display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), COLOR_BACKGROUND);
          display.drawRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), layerColors[l]);
          display.setTextColor(layerColors[l], COLOR_BACKGROUND);
        }
      }
      else {
        display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), COLOR_BACKGROUND);
        display.drawRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), GREY2);
        display.setTextColor(GREY3, COLOR_BACKGROUND); // no text
      }
      display.setCursor(x + 2, y + 2);
      display.print(l + 1);
    }
  }
  else if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Song mode: show song tracks (layers) instead of pattern layers
    for (uint8_t l = 0; l < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; l++) {
      const uint16_t x = GRID.X[0] + (l % 2) * (layerViewSizeX);
      const uint16_t y = layerViewY + (l / 2) * (HEADER_HEIGHT / 2);

      // For song mode, check if the layer has any notes in the current view
      bool hasNotes = false;
      for (const auto& note : notePairs) {
        if (note.noteOn.layer == l) {
          hasNotes = true;
          break;
        }
      }

      // Check if this is the active layer (for filtering)
      bool isActiveLayer = (l == selectedLayer);
      // if (selectedNote > -1) {
      //   isActiveLayer = (notePairs.at(selectedNote).noteOn.layer == l);
      // }

      display.console = true;
      if (hasNotes) {
        if (isActiveLayer) {
          // Active layer: filled with color
          display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), layerColors[l]);
          display.setTextColor(COLOR_BACKGROUND, layerColors[l]);
        }
        else {
          // Non-active layer: outline only
          display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), COLOR_BACKGROUND);
          display.drawRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), layerColors[l]);
          display.setTextColor(layerColors[l], COLOR_BACKGROUND);
        }
      }
      else {
        // No notes in this layer: grey outline
        display.fillRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), COLOR_BACKGROUND);
        display.drawRect(x, y, layerViewSizeX, (HEADER_HEIGHT / 2), GREY2);
        display.setTextColor(GREY2, COLOR_BACKGROUND);
      }
      display.setCursor(x + 2, y + 2);
      display.print(l + 1); // Show layer number 1-4
    }
  }
  else {
    // Mute automation track or other tracks: clear the area
    display.fillRect(GRID.X[0], layerViewY, ROLL_WIDTH, HEADER_HEIGHT, COLOR_BACKGROUND);
  }
}

FLASHMEM uint32_t UI_LiveSeq_PianoRoll::getAbsoluteEventTime(const LiveSequencer::MidiEvent& e) const {
  if (data.isSongMode && !data.useGridSongMode) {
    // For editing notes, use a simple calculation based on stored chunks
    if (editingNote.active) {
      if (&e == &editingNote.editingNoteOn || &e == &editingNote.originalNoteOn) {
        // Use the target chunk for calculation
        uint32_t absoluteBar = (editingNote.targetSongPatternOn * data.numberOfBars) + e.patternNumber;
        return (absoluteBar * data.patternLengthMs) + e.patternMs;
      }
      if (&e == &editingNote.editingNoteOff || &e == &editingNote.originalNoteOff) {
        // Use the target chunk for calculation
        uint32_t absoluteBar = (editingNote.targetSongPatternOff * data.numberOfBars) + e.patternNumber;
        return (absoluteBar * data.patternLengthMs) + e.patternMs;
      }
    }

    // Rest of the normal event lookup code remains the same
    for (const auto& patternEntry : data.songEvents) {
      const uint16_t songPatternIndex = patternEntry.first;
      const auto& events = patternEntry.second;

      for (const auto& ev : events) {
        if (&ev == &e) {
          uint32_t absoluteBar = (songPatternIndex * data.numberOfBars) + ev.patternNumber;
          return (absoluteBar * data.patternLengthMs) + ev.patternMs;
        }
      }
    }

    // Fallback to content comparison
    for (const auto& patternEntry : data.songEvents) {
      const uint16_t songPatternIndex = patternEntry.first;
      const auto& events = patternEntry.second;

      for (const auto& ev : events) {
        if (ev.track == e.track &&
          ev.layer == e.layer &&
          ev.note_in == e.note_in &&
          ev.event == e.event &&
          ev.patternNumber == e.patternNumber &&
          ev.patternMs == e.patternMs &&
          ev.note_in_velocity == e.note_in_velocity) {
          uint32_t absoluteBar = (songPatternIndex * data.numberOfBars) + ev.patternNumber;
          return (absoluteBar * data.patternLengthMs) + ev.patternMs;
        }
      }
    }
  }

  // Fallback (relative to current 4-bar cycle)
  return uint32_t(e.patternNumber) * data.patternLengthMs + e.patternMs;
}



FLASHMEM void UI_LiveSeq_PianoRoll::queueDrawNote(UpdateRange coords) {
  drawRegionsX.push_back(coords);
  drawFlags |= DRAW_NOTES;
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawNote(LiveSequencer::NotePair note, bool isSelected, bool drawInGrey) const {
  uint32_t timeOn, timeOff;
  uint16_t x;
  int w;

  if (data.isSongMode && !data.useGridSongMode) {
    // SONG MODE: Handle boundary crossings properly
    timeOn = getAbsoluteEventTime(note.noteOn);
    timeOff = getAbsoluteEventTime(note.noteOff);

    // Ensure timeOff is always after timeOn
    if (timeOff <= timeOn) {
      timeOff = timeOn + data.patternLengthMs / 16;
    }

    // Calculate viewport boundaries
    const uint32_t viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
    const uint32_t viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);

    // If note starts before viewport, clamp start to viewport
    uint32_t visibleTimeOn = std::max(timeOn, viewStartMs);
    uint32_t visibleTimeOff = std::min(timeOff, viewEndMs);

    // Only draw if there's a visible portion
    if (visibleTimeOn >= visibleTimeOff) {
      return;
    }

    x = getNoteCoord(visibleTimeOn);  // Use clamped start time

    // Calculate width based on VISIBLE portion only
    uint32_t visibleLenMs = visibleTimeOff - visibleTimeOn;
    w = std::max(1, int(roundf(visibleLenMs * view.msToPix)));

    // Additional clamp to prevent drawing beyond display
    uint16_t endX = x + w;
    if (endX > DISPLAY_WIDTH) {
      w = DISPLAY_WIDTH - x;
    }

    if (w < 1) w = 1;
  }
  else {
    // PATTERN MODE: Original working calculation
    timeOn = getEventTime(note.noteOn);
    timeOff = getEventTime(note.noteOff);

    x = getNoteCoord(timeOn);
    const uint32_t lenMs = (timeOff > timeOn) ? (timeOff - timeOn) : 1;
    w = std::max(1, int(roundf(lenMs * view.msToPix)));
  }

  // Simplified region checking
  if (!drawRegionsX.empty()) {
    bool shouldDraw = false;
    uint8_t notePitch = note.noteOn.note_in;

    for (auto& u : drawRegionsX) {
      bool inX = (x <= u.toX) && ((x + w) >= u.fromX);
      bool inNotes = u.notes.empty() || u.notes.count(notePitch);

      if (inX && inNotes) {
        shouldDraw = true;
        break;
      }
    }

    if (!shouldDraw) return;
  }

  drawNoteAtPosition(note, isSelected, x, uint16_t(w), drawInGrey);
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawAreaIndicator(void) {
  display.console = true;
  static constexpr uint16_t layerViewY = CONTENT_HEIGHT + LINE_HEIGHT;
  display.drawRect(ROLL_WIDTH, layerViewY,
    TouchButton::BUTTON_SIZE_X - ROLL_WIDTH,
    HEADER_HEIGHT, GREY1);

  static constexpr uint16_t wTotal = TouchButton::BUTTON_SIZE_X - ROLL_WIDTH - 2;
  static constexpr uint16_t hTotal = HEADER_HEIGHT - 2;

  display.fillRect(ROLL_WIDTH + 1, layerViewY + 1, wTotal, hTotal, COLOR_BACKGROUND);

  uint16_t maxBars = getMaxBarsForCurrentMode();
  uint32_t totalTimeMs, viewTimeMs, startTimeMs;

  if ((data.isSongMode && !data.useGridSongMode) || data.activeTrack == SONG_MODE_MUTE_TRACK) {
    // Song mode and mute automation: use 1:1 bar mapping (no 4-bar compression)
    totalTimeMs = uint32_t(maxBars) * data.patternLengthMs;
    viewTimeMs = uint32_t(view.numBars) * data.patternLengthMs;
    startTimeMs = uint32_t(view.startBar) * data.patternLengthMs;
  }
  else {
    // Pattern mode
    totalTimeMs = uint32_t(maxBars) * data.patternLengthMs;
    viewTimeMs = uint32_t(view.numBars) * data.patternLengthMs;
    startTimeMs = uint32_t(view.startBar) * data.patternLengthMs;
  }

  if (totalTimeMs == 0) return;

  const uint16_t x = ROLL_WIDTH + 1 + roundf((float)wTotal * (float)startTimeMs / (float)totalTimeMs);

  // Calculate height and Y position - full height for mute automation
  uint16_t h, y;
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    // Mute automation: full height (no Y scrolling/zooming)
    h = hTotal;
    y = layerViewY + 1;
  }
  else {
    // Pattern/song mode: normal height based on octave view
    h = roundf(hTotal * view.numOctaves / float(TOTAL_OCTAVES));
    y = layerViewY + 1 + hTotal - h -
      roundf(hTotal * view.startOctave / float(TOTAL_OCTAVES));
  }

  int w = int(roundf((float)wTotal * (float)viewTimeMs / (float)totalTimeMs));
  if (w < 1) w = 1;
  if (x + w > ROLL_WIDTH + wTotal) w = (ROLL_WIDTH + wTotal) - x;

  display.fillRect(x, y, uint16_t(w), h, MIDDLEGREEN);

  // EXTENDED: Clear text areas with background color before drawing new text
  display.console = true;
  display.fillRect(GRID.X[1], layerViewY + 2, 80, CHAR_height_small, COLOR_BACKGROUND);
  display.console = true;
  display.fillRect(GRID.X[1], layerViewY + CHAR_height_small + 4, 80, CHAR_height_small, COLOR_BACKGROUND);

  // Show bar info
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(1);
  display.setCursor(GRID.X[1], layerViewY + 3);

  if ((data.isSongMode && !data.useGridSongMode) || data.activeTrack == SONG_MODE_MUTE_TRACK) {
    // Song mode and mute automation: show absolute bar numbers
    uint16_t startBar = view.startBar + 1;
    uint16_t endBar = view.startBar + view.numBars;
    display.printf("BAR:%i-%i", startBar, endBar);
  }
  else {
    // Pattern mode: show relative bar numbers
    display.printf("BAR:%i-%i", view.startBar + 1, view.startBar + view.numBars);
  }

  display.setCursor(GRID.X[1], layerViewY + CHAR_height_small + 5);

  // Show octave info only for pattern/song notes, not for mute automation
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    //display.print("");
    ;
  }
  else {
    display.printf("OCT:%i-%i", view.startOctave, view.startOctave + view.numOctaves - 1);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawBackground(void) {
  // Process all regions in one pass
  for (auto& u : drawRegionsX) {
    if (u.notes.empty()) {
      // Full region - process all notes
      for (uint8_t note = view.lowestNote; note < view.highestNote; note++) {
        drawBackgroundForNote(note, u.fromX, u.toX);
      }
    }
    else {
      // Partial region - only specific notes
      for (uint8_t note : u.notes) {
        if (note >= view.lowestNote && note < view.highestNote) {
          drawBackgroundForNote(note, u.fromX, u.toX);
        }
      }
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::queueDrawNote(LiveSequencer::NotePair note) {
  UpdateRange coords;

  // Use the same timing calculation as drawNote
  uint32_t timeOn, timeOff;
  if (data.isSongMode && !data.useGridSongMode) {
    // SONG MODE: Use getAbsoluteEventTime for consistent boundary handling
    timeOn = getAbsoluteEventTime(note.noteOn);
    timeOff = getAbsoluteEventTime(note.noteOff);

    if (timeOff <= timeOn) {
      timeOff = timeOn + data.patternLengthMs / 16;
    }
  }
  else {
    // PATTERN MODE: Use original working calculation
    timeOn = getEventTime(note.noteOn);
    timeOff = getEventTime(note.noteOff);
  }

  coords.fromX = getNoteCoord(timeOn);
  coords.toX = getNoteCoord(timeOff);
  coords.notes = { note.noteOn.note_in };

  // Add padding for redraw regions, ensuring we don't go out of bounds
  coords.fromX = (coords.fromX < 2 + ROLL_WIDTH) ? ROLL_WIDTH : (coords.fromX - 2);
  coords.toX = (coords.toX > DISPLAY_WIDTH - 2) ? DISPLAY_WIDTH : (coords.toX + 2);

  drawRegionsX.push_back(coords);
  drawFlags |= DRAW_NOTES | DRAW_VERTICAL_BARS;
}

extern bool disableDirectMIDIinput;

FLASHMEM void UI_LiveSeq_PianoRoll::processLCDM(uint8_t param) {
  if (LCDML.FUNC_setup()) { // ****** SETUP *********
    openendFromLiveSequencer = data.processMidiIn;
    if (openendFromLiveSequencer == false) {
      data.processMidiIn = true;
    }

    disableDirectMIDIinput = true;

    if (param != 0xFF) {
      data.activeTrack = param;
      view.startBar = 0;
      view.numBars = 4;
      view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);
    }

    // RESET TO LAYER 0 WHEN OPENING WITH SPECIFIC TRACK
    selectedLayer = 0;

    liveSeq.setActiveTrack(data.activeTrack);
    isVisible = true;
    registerTouchHandler(handle_touchscreen_pianoroll);

    reloadNotes(true);

    // setup function
    display.console = true;
    display.fillScreen(COLOR_BACKGROUND);
    LCDML.FUNC_setLoopInterval(25); // 40Hz gui refresh

    display.drawLine(0, CONTENT_HEIGHT, DISPLAY_WIDTH, CONTENT_HEIGHT, GREY2);

    drawLayersIndicator();
    drawAreaIndicator();

    drawHeader(HEADER_DRAW_ALL);

    drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;

    for (auto* b : buttons) {
      b->drawNow();
    }
  }
  if (LCDML.FUNC_loop()) {

    // RESET Y ZOOM AND SCROLL WHEN IN MUTE EDITOR
    if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
      if (view.startOctave != 0 || view.numOctaves != 5) {
        view.startOctave = 0;
        view.numOctaves = 5;
        view.numNotes = view.numOctaves * 12;
        view.noteHeight = CONTENT_HEIGHT / float(view.numNotes);
        view.lowestNote = 24 + view.startOctave * 12;
        view.highestNote = (view.lowestNote + view.numNotes);
        view.startBar = 0;
        uint16_t maxBars = getMaxBarsForCurrentMode();
        view.numBars = std::min(uint16_t(8), maxBars); // Default to 8 bars or available max
        // Recalculate msToPix for the new view
        view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);
        drawAreaIndicator();
        reloadNotes();
        drawFlags |= DRAW_CONTENT_FULL; // Force redraw
      }
    }

    const EncoderEvents e = getEncoderEvents(ENC_R);
    if (e.turned) {
      handleEncoderRight(e);
    }
    if (e.pressed) {
      toggleSettingActivated();
    }

    const bool runningChanged = isRunningHere != data.isRunning;
    const bool justStopped = isRunningHere && !data.isRunning;
    if (runningChanged) {
      isRunningHere = data.isRunning;
      buttonPlay->drawNow();
      buttonStep->drawNow(); // handle active state
    }

    if (isRunningHere || justStopped) {
      uint32_t currentTimeMs = 0;
      uint32_t viewStartMs = 0;
      uint32_t viewEndMs = 0;

      if (data.isSongMode && !data.useGridSongMode) {
        // Simple counter for current song pattern
        static uint16_t currentSongPattern = 0;

        // Reset when playback starts or stops
        static bool wasRunning = false;
        if (data.isRunning && !wasRunning) {
          currentSongPattern = 0;  // Reset when song starts

          buttonPlay->drawNow(); // Update button
        }
        wasRunning = data.isRunning;

        static uint8_t lastActiveTrack = data.activeTrack;
        if (data.activeTrack != lastActiveTrack) {

          lastActiveTrack = data.activeTrack;
        }

        // Advance to next song pattern when we wrap around
        static uint8_t lastPattern = 0;
        if (data.currentPattern == 0 && lastPattern == 3) {
          // We just wrapped from pattern 3 to 0 - advance song pattern
          currentSongPattern++;
        }
        lastPattern = data.currentPattern;

        // Calculate absolute time
        currentTimeMs = (currentSongPattern * data.numberOfBars * data.patternLengthMs) +
          (data.currentPattern * data.patternLengthMs) + data.patternTimer;

        // Calculate viewport time range
        viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
        viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);
      }
      else {
        // Pattern mode: calculate relative time within pattern
        currentTimeMs = data.currentPattern * data.patternLengthMs + data.patternTimer;
        viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
        viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);
      }

      // Calculate cursor position
      if (currentTimeMs >= viewStartMs && currentTimeMs <= viewEndMs) {
        // Cursor is within viewport - calculate position
        float ratio = float(currentTimeMs - viewStartMs) / float(viewEndMs - viewStartMs);
        view.cursorPos = ROLL_WIDTH + roundf(ratio * CONTENT_WIDTH);
      }
      else if (currentTimeMs < viewStartMs) {
        // Cursor is before viewport
        view.cursorPos = ROLL_WIDTH;
      }
      else {
        // Cursor is after viewport
        view.cursorPos = DISPLAY_WIDTH - 1;
      }

      // Handle cursor color based on whether it's in viewport
      const bool inViewport = (currentTimeMs >= viewStartMs && currentTimeMs <= viewEndMs);
      const uint16_t color = inViewport ? MIDDLEGREEN : RED;
      const bool colorChanged = color != view.cursorColor;
      const bool cursorPosChanged = (view.cursorPos != view.cursorPosPrev);

      if (justStopped || colorChanged || cursorPosChanged) {
        // Redraw the area where the cursor was
        if (view.cursorPosPrev >= ROLL_WIDTH && view.cursorPosPrev < DISPLAY_WIDTH) {
          UpdateRange cursorRegion;
          cursorRegion.fromX = view.cursorPosPrev - 1;
          cursorRegion.toX = view.cursorPosPrev + 1;
          cursorRegion.notes = {}; // Empty set = all notes (for background redraw)
          drawRegionsX.push_back(cursorRegion);
          drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
        }

        if (!justStopped) {
          view.cursorColor = color;
          drawFlags |= DRAW_CURSOR;
        }
        view.cursorPosPrev = view.cursorPos;
      }
    }

    drawGUI();
  }
  if (LCDML.FUNC_close()) {
    if (openendFromLiveSequencer == false) {
      data.processMidiIn = false;
    }
    isVisible = false;
    unregisterTouchHandler();
    display.fillScreen(COLOR_BACKGROUND);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawCursor(void) {
  display.console = true;
  display.drawLine(view.cursorPos, 0, view.cursorPos, CONTENT_HEIGHT - 1, view.cursorColor);
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawStepCursor(void) {
  display.console = true;
  display.drawLine(stepCursorPosition, 0, stepCursorPosition, CONTENT_HEIGHT - 1, RED);
}

FLASHMEM std::vector<LiveSequencer::NotePair>
UI_LiveSeq_PianoRoll::getNotePairsFromSongTrack(
  uint8_t track, uint8_t& lowestNote, uint8_t& highestNote,
  uint8_t patternFrom, uint8_t patternTo) {

  std::vector<LiveSequencer::NotePair> result;
  uint8_t lowestFound = 0xFF;
  uint8_t highestFound = 0x00;

  if (data.songEvents.empty()) {
    lowestNote = view.lowestNote;
    highestNote = view.highestNote;
    return result;
  }

  struct EventRef {
    uint16_t songPatternIndex;
    LiveSequencer::MidiEvent* eventPtr;
    uint32_t absoluteTime;
    size_t eventIndex;
  };

  std::vector<EventRef> noteOnEvents;
  std::vector<EventRef> noteOffEvents;

  auto calcAbsoluteTime = [this](uint16_t songPatternIndex, const LiveSequencer::MidiEvent& event) -> uint32_t {
    uint32_t absoluteBar = (songPatternIndex * data.numberOfBars) + event.patternNumber;
    return (absoluteBar * data.patternLengthMs) + event.patternMs;
    };

  // Collect POINTERS to actual events in data.songEvents
  for (auto& patternEntry : data.songEvents) {
    const uint16_t songPatternIndex = patternEntry.first;
    auto& events = patternEntry.second;

    for (size_t i = 0; i < events.size(); i++) {
      LiveSequencer::MidiEvent* eventPtr = &events[i];

      // Skip invalid events
      if (eventPtr->event == midi::InvalidType) {
        continue;
      }

      if ((track == 0xFF || eventPtr->track == track) &&
        eventPtr->note_in >= view.lowestNote && eventPtr->note_in < view.highestNote) {

        uint32_t absTime = calcAbsoluteTime(songPatternIndex, *eventPtr);

        if (eventPtr->event == midi::NoteOn) {
          noteOnEvents.push_back({ songPatternIndex, eventPtr, absTime, i });
        }
        else if (eventPtr->event == midi::NoteOff) {
          noteOffEvents.push_back({ songPatternIndex, eventPtr, absTime, i });
        }
      }
    }
  }

  // Sort both by absolute time
  std::sort(noteOnEvents.begin(), noteOnEvents.end(),
    [](const auto& a, const auto& b) { return a.absoluteTime < b.absoluteTime; });

  std::sort(noteOffEvents.begin(), noteOffEvents.end(),
    [](const auto& a, const auto& b) { return a.absoluteTime < b.absoluteTime; });

  // Track which note-offs have been used
  std::vector<bool> noteOffUsed(noteOffEvents.size(), false);

  // Calculate view range
  uint32_t viewStartTime = uint32_t(patternFrom) * uint32_t(data.patternLengthMs);
  uint32_t viewEndTime = uint32_t(patternTo + 1) * uint32_t(data.patternLengthMs);

  // For each note-on, find the CLOSEST matching note-off
  for (const auto& noteOn : noteOnEvents) {
    int bestOffIndex = -1;
    uint32_t bestTimeDiff = UINT32_MAX;

    for (size_t i = 0; i < noteOffEvents.size(); i++) {
      if (noteOffUsed[i]) continue;

      const auto& noteOff = noteOffEvents[i];

      // Must match track, layer, and pitch
      if (noteOff.eventPtr->track != noteOn.eventPtr->track ||
        noteOff.eventPtr->layer != noteOn.eventPtr->layer ||
        noteOff.eventPtr->note_in != noteOn.eventPtr->note_in) {
        continue;
      }

      // Note-off must be after note-on
      if (noteOff.absoluteTime <= noteOn.absoluteTime) {
        continue;
      }

      uint32_t timeDiff = noteOff.absoluteTime - noteOn.absoluteTime;

      // Sanity check: skip unreasonably long notes
      if (timeDiff > 16 * data.patternLengthMs) {
        continue;
      }

      if (timeDiff < bestTimeDiff) {
        bestTimeDiff = timeDiff;
        bestOffIndex = i;
      }
    }

    if (bestOffIndex != -1) {
      const auto& noteOff = noteOffEvents[bestOffIndex];
      noteOffUsed[bestOffIndex] = true;

      if ((noteOn.absoluteTime < viewEndTime && noteOff.absoluteTime > viewStartTime)) {
        LiveSequencer::NotePair p = {
          *noteOn.eventPtr,
          *noteOff.eventPtr,
          false
        };

        result.emplace_back(p);

        // Store the original event locations
        SongEventLocation loc;
        loc.noteOnPattern = noteOn.songPatternIndex;
        loc.noteOnIndex = noteOn.eventIndex;
        loc.noteOffPattern = noteOff.songPatternIndex;
        loc.noteOffIndex = noteOff.eventIndex;
        eventLocations.push_back(loc);

        highestFound = std::max(highestFound, noteOn.eventPtr->note_in);
        lowestFound = std::min(lowestFound, noteOn.eventPtr->note_in);
      }
    }
  }

  lowestNote = (lowestFound == 0xFF) ? view.lowestNote : lowestFound;
  highestNote = (highestFound == 0x00) ? view.highestNote : highestFound;
  return result;
}

FLASHMEM std::vector<LiveSequencer::MidiEvent>
UI_LiveSeq_PianoRoll::getMuteAutomationEvents(
  uint8_t track, uint8_t patternFrom, uint8_t patternTo) {

  std::vector<LiveSequencer::MidiEvent> result;

  if (data.songEvents.empty()) {
    return result;
  }

  for (const auto& patternEntry : data.songEvents) {
    const uint16_t songPatternIndex = patternEntry.first;
    const uint16_t absoluteBarStart = songPatternIndex * data.numberOfBars;

    if (patternEntry.second.empty()) {
      continue;
    }

    for (const auto& e : patternEntry.second) {
      if ((track == 0xFF || e.track == track) &&
        e.event == midi::ControlChange &&
        (e.note_in == LiveSequencer::TYPE_MUTE_ON || e.note_in == LiveSequencer::TYPE_MUTE_OFF)) {

        const uint16_t eventAbsoluteBar = absoluteBarStart + e.patternNumber;

        if (eventAbsoluteBar >= patternFrom && eventAbsoluteBar <= patternTo) {
          // Keep original cyclic pattern number - don't modify!
          result.emplace_back(e);
        }
      }
    }
  }

  return result;
}

FLASHMEM void UI_LiveSeq_PianoRoll::reloadNotes(bool autoAdaptView) {
  // Clear previous locations
  eventLocations.clear();
  // DISABLE EDIT MODE WHEN TRACK CHANGES
  static uint8_t lastActiveTrack = data.activeTrack;
  if (data.activeTrack != lastActiveTrack) {
    mode = MODE_VIEW; // Switch to view mode
    isSettingActivated = false;
    buttonEdit->drawNow();
    buttonView->drawNow();
    lastActiveTrack = data.activeTrack;
  }

  const uint8_t patternFrom = view.startBar;
  const uint8_t patternTo = view.startBar + view.numBars - 1;
  uint8_t lowestNote = view.lowestNote;
  uint8_t highestNote = view.highestNote;

  // Ensure active track is valid for current mode
  const uint8_t maxTrack = getMaxTrackForCurrentMode();
  if (data.activeTrack > maxTrack) {
    data.activeTrack = 0;
  }

  if (autoAdaptView) {
    lowestNote = 0x00;
    highestNote = 0xFF;
  }

  // Store previous state to detect changes
  size_t prevNotePairsCount = notePairs.size();
  size_t prevMuteEventsCount = muteAutomationEvents.size();

  // Load note pairs based on track type
  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Song mode: show ALL notes from ALL tracks
    notePairs = getNotePairsFromSongTrack(0xFF, lowestNote, highestNote, patternFrom, patternTo);
    muteAutomationEvents.clear();
    selectedMuteEvent = -1;

    // Reset selected note if it's now out of bounds
    if (selectedNote >= static_cast<int16_t>(notePairs.size())) {
      selectedNote = notePairs.empty() ? -1 : 0;
    }
  }
  else if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    // Song mode: show only automation events 
    notePairs.clear();
    muteAutomationEvents = getMuteAutomationEvents(0xFF, patternFrom, patternTo);
    selectedNote = -1;

    // Reset selected mute event if it's now out of bounds
    if (selectedMuteEvent >= static_cast<int16_t>(muteAutomationEvents.size())) {
      selectedMuteEvent = muteAutomationEvents.empty() ? -1 : 0;
    }
  }
  else {
    // Pattern mode: use the existing method for specific track
    notePairs = liveSeq.getNotePairsFromTrack(data.activeTrack, lowestNote, highestNote, patternFrom, patternTo);
    muteAutomationEvents.clear();
    selectedMuteEvent = -1;

    // Reset selected note if it's now out of bounds
    if (selectedNote >= static_cast<int16_t>(notePairs.size())) {
      selectedNote = notePairs.empty() ? -1 : 0;
    }
  }

  // Only set draw flags if data actually changed
  bool dataChanged = (prevNotePairsCount != notePairs.size()) ||
    (prevMuteEventsCount != muteAutomationEvents.size());

  if (autoAdaptView && !notePairs.empty()) {
    view.startOctave = std::min((lowestNote - 24) / 12, TOTAL_OCTAVES - view.numOctaves);

    // FORCE ONE OCTAVE HIGHER FOR ALL MODES
    if (view.startOctave > 0) {
      view.startOctave = std::min(view.startOctave - 1, TOTAL_OCTAVES - view.numOctaves);
    }

    setNumOctaves(5);

    // Use same calculation for both modes
    view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);

    drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;
  }
  else if (dataChanged) {
    // Only redraw if data actually changed
    drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;
  }

  // Always update header when reloading
  drawHeader(HEADER_DRAW_ALL);
  drawLayersIndicator();
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawMuteAutomation(void) {
  if (!data.isSongMode || data.useGridSongMode) return;
  if (data.activeTrack != SONG_MODE_MUTE_TRACK) return;

  display.setTextSize(1);
  display.console = true;

  const uint32_t viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
  const uint32_t viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);

  for (size_t i = 0; i < muteAutomationEvents.size(); i++) {
    const auto& event = muteAutomationEvents.at(i);

    // Calculate absolute bar on the fly for this event
    uint16_t songPatternIndex = getSongPatternForEvent(event);
    uint16_t absoluteBar = (songPatternIndex * data.numberOfBars) + event.patternNumber;
    const uint32_t eventTime = absoluteBar * data.patternLengthMs + event.patternMs;

    // Skip events outside viewport
    if (eventTime < viewStartMs || eventTime > viewEndMs) {
      continue;
    }

    const uint16_t x = getNoteCoord(eventTime);
    const uint16_t y = CONTENT_HEIGHT - 12;

    const bool isSelected = (static_cast<int16_t>(i) == selectedMuteEvent);

    uint16_t arrowColor = isSelected ? RED :
      (event.note_in == LiveSequencer::TYPE_MUTE_ON ? GREY2 : GREEN);

    if (event.note_in == LiveSequencer::TYPE_MUTE_ON) {
      display.setTextColor(arrowColor, COLOR_BACKGROUND);
      display.setCursor(x - 2, y);
      display.write(0x1F);
    }
    else if (event.note_in == LiveSequencer::TYPE_MUTE_OFF) {
      display.setTextColor(arrowColor, COLOR_BACKGROUND);
      display.setCursor(x - 2, y);
      display.write(0x1E);
    }

    display.setTextColor(isSelected ? RED : COLOR_SYSTEXT, COLOR_BACKGROUND);
    display.setCursor(x - 2, y - 8);
    display.printf("%d", event.note_in_velocity + 1);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawBarNumbers(void) {
  display.console = true;
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(1);

  static constexpr uint8_t substeps = 4;
  const float stepWidth = CONTENT_WIDTH / float(view.numBars * substeps);

  for (uint8_t bar = 0; bar <= view.numBars; bar++) {
    const uint16_t x = ROLL_WIDTH + round(bar * stepWidth * substeps);
    if (bar < view.numBars) {
      const uint16_t displayBar = view.startBar + bar;
      char barInfo[16];

      if (data.isSongMode && !data.useGridSongMode) {
        // Song mode: show absolute bar numbers
        snprintf(barInfo, sizeof(barInfo), "%d", displayBar + 1);
      }
      else {
        // Pattern mode: check if we're viewing beyond the actual pattern length
        if (view.numBars > data.numberOfBars || view.startBar > 0) {
          // Extended view: show absolute bar numbers
          snprintf(barInfo, sizeof(barInfo), "%d", displayBar + 1);
        }
        else {
          // Normal pattern view: show relative bar numbers
          snprintf(barInfo, sizeof(barInfo), "%d", (displayBar % data.numberOfBars) + 1);
        }
      }

      display.setCursor(x - 4, 5);
      display.print(barInfo);
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawGUI(void) {
  // Only draw if there are actual draw flags set
  if (drawFlags == 0) {
    return;
  }

  // NOTE: order of drawing is important
  if (drawFlags & DRAW_CONTENT_FULL) {
    drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES | DRAW_MUTE_AUTOMATION;
    queueDrawNote({ ROLL_WIDTH, DISPLAY_WIDTH, {} }); // force redraw all

    // Clear the content area for full redraw
    display.console = true;
    display.fillRect(ROLL_WIDTH, 0, CONTENT_WIDTH, CONTENT_HEIGHT, COLOR_BACKGROUND);
  }
  if (drawFlags & DRAW_BACKGROUND) {
    drawBackground();
  }
  if (drawFlags & DRAW_VERTICAL_BARS) {
    drawVerticalBars();
  }
  if (drawFlags & DRAW_CURSOR) {
    drawCursor();
  }
  if (drawFlags & DRAW_STEP_CURSOR) {
    drawStepCursor();
  }
  if (drawFlags & DRAW_NOTES) {
    drawNotes();
  }
  if (drawFlags & DRAW_MUTE_AUTOMATION) {
    drawMuteAutomation();
  }
  if (drawFlags & DRAW_PIANOROLL) {
    drawPianoRoll();
  }

  //  draw bar numbers last, after everything else
  if (!data.isRunning)
    drawBarNumbers();

  // Clear all draw flags after processing
  drawFlags = 0;
  drawRegionsX.clear();
}

template<typename T>
FLASHMEM bool UI_LiveSeq_PianoRoll::hasConstrainedChanged(T& value, int16_t diff, T min, T max) {
  const T newValue = constrain(value + diff, min, max);
  const bool changed = newValue != value;
  value = newValue;
  return changed;
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleKeyChanged(uint8_t key, midi::MidiType event, uint8_t velocity) {

  if (isVisible) {
    switch (event) {
    case midi::NoteOn:
      keysPressed.insert(key);
      drawFlags |= DRAW_CURSOR;
      break;

    case midi::NoteOff:
      keysPressed.erase(key);
      drawFlags |= DRAW_CURSOR;
      break;

    default:
      break;
    }

    if ((key >= view.lowestNote) && (key <= view.highestNote)) {
      queueDrawNote({ ROLL_WIDTH, DISPLAY_WIDTH, { key } });
    }

    if ((data.isRunning == false) && (mode == MODE_STEP)) {
      if (event == midi::NoteOn) {
        // Calculate absolute time in milliseconds from viewport position
        uint32_t absoluteTimeMs = view.startBar * data.patternLengthMs +
          static_cast<uint32_t>(stepCursorPositionIndex * stepRecordStepSizeMs);

        // Create note on event
        LiveSequencer::MidiEvent on;
        on.event = midi::NoteOn;
        on.note_in = key;
        on.note_in_velocity = velocity;

        // FIX: Determine the correct track based on what's being displayed
        bool recordingToSongTrack = false;

        if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
          // We're viewing the song notes track - record to song track
          on.track = data.stepRecordTargetTrack;
          on.source = LiveSequencer::EVENT_SONG;
          recordingToSongTrack = true;
        }
        else if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
          // We're viewing mute automation - shouldn't record notes here
          return;
        }
        else {
          // We're viewing a pattern track - record to that pattern track
          on.track = data.activeTrack;
          on.source = LiveSequencer::EVENT_PATTERN;
          recordingToSongTrack = false;
        }

        on.layer = stepRecordLayer;

        // Create note off event (copy from note on)
        LiveSequencer::MidiEvent off = on;
        off.event = midi::NoteOff;

        // Use recordingToSongTrack instead of data.isSongMode for the storage logic
        if (recordingToSongTrack) {
          // ====== SONG TRACK RECORDING: Convert absolute time to chunk-relative storage ======
          const uint32_t chunkLengthMs = data.numberOfBars * data.patternLengthMs;

          // Calculate note ON position
          uint16_t songPatternIndexOn = absoluteTimeMs / chunkLengthMs;
          uint32_t timeInChunkOn = absoluteTimeMs % chunkLengthMs;

          // Convert to cyclic pattern number (0-3) and offset within that bar
          on.patternNumber = (timeInChunkOn / data.patternLengthMs) % data.numberOfBars;
          on.patternMs = timeInChunkOn % data.patternLengthMs;

          // Bounds check for patternMs (should never exceed patternLengthMs)
          if (on.patternMs >= data.patternLengthMs) {
            on.patternMs = data.patternLengthMs - 1;
          }

          // Calculate note OFF position (half step length later)
          uint32_t noteOffTimeAbs = absoluteTimeMs + static_cast<uint32_t>(stepRecordStepSizeMs / 2);
          uint16_t songPatternIndexOff = noteOffTimeAbs / chunkLengthMs;
          uint32_t timeInChunkOff = noteOffTimeAbs % chunkLengthMs;

          off.patternNumber = (timeInChunkOff / data.patternLengthMs) % data.numberOfBars;
          off.patternMs = timeInChunkOff % data.patternLengthMs;

          // Bounds check for patternMs
          if (off.patternMs >= data.patternLengthMs) {
            off.patternMs = data.patternLengthMs - 1;
          }

          // Ensure target chunks exist in songEvents map
          if (data.songEvents.find(songPatternIndexOn) == data.songEvents.end()) {
            data.songEvents[songPatternIndexOn]; // Auto-creates empty vector with correct allocator
          }
          if (songPatternIndexOn != songPatternIndexOff &&
            data.songEvents.find(songPatternIndexOff) == data.songEvents.end()) {
            data.songEvents[songPatternIndexOff]; // Auto-creates empty vector with correct allocator
          }

          // Add both events to their respective chunks
          data.songEvents[songPatternIndexOn].push_back(on);
          data.songEvents[songPatternIndexOff].push_back(off);

          // Request event sorting for proper playback order
          liveSeq.requestSortEvents();

        }
        else {
          // ====== PATTERN TRACK RECORDING: Simple relative time calculation ======
          on.patternNumber = absoluteTimeMs / data.patternLengthMs;
          on.patternMs = absoluteTimeMs % data.patternLengthMs;

          uint32_t noteOffTime = absoluteTimeMs + static_cast<uint32_t>(stepRecordStepSizeMs / 2);
          off.patternNumber = noteOffTime / data.patternLengthMs;
          off.patternMs = noteOffTime % data.patternLengthMs;

          liveSeq.addNotePair(on, off);
        }

        // Force full redraw if this is the first note
        if (notePairs.empty()) {
          drawFlags |= DRAW_CONTENT_FULL;
        }

        // Update layer count if needed
        uint8_t targetTrack = on.track; // Use the track we actually assigned to the event
        if (targetTrack < MAX_TRACKS && stepRecordLayer + 1 > data.trackSettings[targetTrack].layerCount) {
          data.trackSettings[targetTrack].layerCount++;
          drawLayersIndicator();
        }

        // Reload notes to show the newly added note
        reloadNotes();
      }
      else if (keysPressed.empty()) {
        // Advance to next step when all keys released
        step(+1);
      }
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::setNumBars(int8_t num) {
  if (num == view.numBars) return;

  uint16_t maxBars = getMaxBarsForCurrentMode();

  // For pattern mode, ensure we can zoom out to at least 4 bars
  if (!data.isSongMode && data.activeTrack < SONG_MODE_NOTES_TRACK) {
    maxBars = std::max(maxBars, uint16_t(4));
  }

  uint16_t newNumBars = constrain(num, 1, maxBars);

  if (newNumBars != view.numBars) {
    view.numBars = newNumBars;

    if (view.startBar > (maxBars - view.numBars)) {
      setBarStart(maxBars - view.numBars);
    }

    view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);

    reloadNotes();
    drawAreaIndicator();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::setBarStart(int8_t num) {
  if (num == view.startBar) return;

  uint16_t maxBars = getMaxBarsForCurrentMode();

  // Calculate maximum start bar that still shows content
  uint16_t maxStartBar = 0;
  if (maxBars > view.numBars) {
    maxStartBar = maxBars - view.numBars;
  }

  // Additional safety: ensure we don't scroll past actual content
  if (maxBars < view.numBars) {
    maxStartBar = 0;
  }

  uint16_t newStartBar = constrain(num, 0, maxStartBar);

  if (newStartBar != view.startBar) {
    view.startBar = newStartBar;
    reloadNotes();
    drawAreaIndicator();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::toggleSettingActivated(void) {
  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    if (isSettingActivated && editingNote.active) {
      // Committing changes - note should return to normal color
      commitEditingNote();
    }
    else if (!isSettingActivated && editingNote.active) {
      // Canceling changes - note should return to normal color  
      cancelEditingNote();
    }
    else if (!isSettingActivated && selectedNote > -1) {
      // Entering edit mode - but only turn red if we're NOT in selection mode
      bool isParameterEditMode = (editMode != EDIT_SELECT_NOTE && editMode != EDIT_LAYER);
      if (isParameterEditMode) {
        startEditingNote(selectedNote);
      }
    }
  }
  else if (data.activeTrack < SONG_MODE_NOTES_TRACK) {
    // PATTERN MODE (not mute editor): Handle the selected note redraw
    if (selectedNote > -1) {
      // Create a region that covers the note area with EMPTY notes
      // This ensures everything in that area gets redrawn properly
      uint32_t timeOn = getEventTime(notePairs.at(selectedNote).noteOn);
      uint32_t timeOff = getEventTime(notePairs.at(selectedNote).noteOff);
      uint16_t fromX = getNoteCoord(timeOn);
      uint16_t toX = getNoteCoord(timeOff);

      UpdateRange fullRegion;
      fullRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(fromX - 5));
      fullRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(toX + 5));
      fullRegion.notes = {}; // EMPTY = redraw ALL notes in this X range
      drawRegionsX.push_back(fullRegion);

      drawFlags |= DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }

  isSettingActivated = !isSettingActivated;

  if (mode == MODE_EDIT) {
    buttonEdit->drawNow();
  }
  drawHeader(HEADER_DRAW_ALL);
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::getSongPatternForNote(const LiveSequencer::NotePair& note) const {
  if (!data.isSongMode || data.useGridSongMode) {
    return 0;
  }

  // note.noteOn may be a copy inside notePairs, so pointer comparisons can fail.
  // Reuse the content-based event matcher already implemented.
  return getSongPatternForEvent(note.noteOn);
}

FLASHMEM void UI_LiveSeq_PianoRoll::updateSongEvent(LiveSequencer::MidiEvent& updatedEvent) {
  // Find and update the event in songEvents
  for (auto& patternEntry : data.songEvents) {
    for (auto& songEvent : patternEntry.second) {
      // Match by all immutable properties
      if (songEvent.track == updatedEvent.track &&
        songEvent.layer == updatedEvent.layer &&
        songEvent.source == updatedEvent.source) {

        // For notes, also match by the original note value and event type
        bool isMatch = false;
        if (updatedEvent.event == midi::NoteOn || updatedEvent.event == midi::NoteOff) {
          // Match by event type (NoteOn/Off) and similar timing
          isMatch = (songEvent.event == updatedEvent.event) &&
            (abs(int(songEvent.patternNumber) - int(updatedEvent.patternNumber)) <= 1);
        }
        else {
          // For CC/automation, match by note_in (controller number or mute type)
          isMatch = (songEvent.event == updatedEvent.event) &&
            (songEvent.note_in == updatedEvent.note_in ||
              (updatedEvent.event == midi::ControlChange &&
                songEvent.patternNumber == updatedEvent.patternNumber &&
                songEvent.patternMs == updatedEvent.patternMs));
        }

        if (isMatch) {
          songEvent = updatedEvent;
          return;
        }
      }
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleEncoderRight(EncoderEvents e) {
  if (isSettingActivated) {
    handleSettingActivatedEncoder(e);
  }
  else {
    handleNormalEncoder(e);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleSettingActivatedEncoder(EncoderEvents e) {
  uint16_t headerFlags = 0;

  switch (mode) {
  case MODE_VIEW:
    headerFlags = handleViewModeSettingActivated(e);
    break;

  case MODE_EDIT:
    headerFlags = handleEditModeSettingActivated(e);
    break;

  case MODE_STEP:
    headerFlags = handleStepModeSettingActivated(e);
    break;
  }

  if (drawFlags && data.isRunning) {
    drawFlags |= DRAW_CURSOR; // handle cursor off on viewport changed redraw
  }

  drawHeader(headerFlags);
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleNormalEncoder(EncoderEvents e) {
  uint8_t oldMode = 0;
  uint8_t newMode = 0;

  switch (mode) {
  case MODE_VIEW:
    handleViewModeNormal(e, oldMode, newMode);
    break;

  case MODE_EDIT:
    handleEditModeNormal(e, oldMode, newMode);
    break;

  case MODE_STEP:
    handleStepModeNormal(e, oldMode, newMode);
    break;
  }

  if (oldMode != newMode) {
    if (mode == MODE_VIEW || mode == MODE_EDIT) {
      drawHeader((1 << oldMode) | (1 << newMode));
    }
  }
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleViewModeSettingActivated(EncoderEvents e) {
  uint16_t headerFlags = 0;

  switch (viewMode) {
  case VIEW_ZOOM_Y:
    if (hasConstrainedChanged(view.numOctaves, (-2 * e.dir), int8_t(1), int8_t(5))) {
      view.startOctave += e.dir; // zoom octave-centered
      setNumOctaves(view.numOctaves);
      drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;
    }
    break;

  case VIEW_SCROLL_Y:
    handleViewScrollY(e);
    break;

  case VIEW_ZOOM_X:
    handleViewZoomX(e);
    break;

  case VIEW_SCROLL_X:
    handleViewScrollX(e);
    break;
  }

  return headerFlags;
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleViewScrollY(EncoderEvents e) {
  int8_t newStartOctave = view.startOctave + e.dir;
  int8_t maxStartOctave = TOTAL_OCTAVES - view.numOctaves;
  if (maxStartOctave < 0) maxStartOctave = 0;

  if (newStartOctave < 0) newStartOctave = 0;
  if (newStartOctave > maxStartOctave) newStartOctave = maxStartOctave;

  if (newStartOctave != view.startOctave) {
    view.startOctave = newStartOctave;
    view.lowestNote = 24 + view.startOctave * 12;
    view.highestNote = (view.lowestNote + view.numNotes);

    drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;
    drawAreaIndicator();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleViewZoomX(EncoderEvents e) {
  if ((data.isSongMode && !data.useGridSongMode) || data.activeTrack == SONG_MODE_MUTE_TRACK) {
    uint16_t maxBars = getMaxBarsForCurrentMode();

    int16_t newNumBars = view.numBars - e.dir;
    if (newNumBars < 1) newNumBars = 1;
    if (newNumBars > int16_t(maxBars)) newNumBars = int16_t(maxBars);

    if (newNumBars != view.numBars) {
      setNumBars(newNumBars);
      drawFlags |= DRAW_CONTENT_FULL;
    }
  }
  else {
    // PATTERN MODE: Allow zooming out to at least 4 bars, or more if available
    int16_t newNumBars = view.numBars + e.dir;

    // Set minimum to 1 bar, maximum to at least 4 bars or the actual number of bars if larger
    uint16_t minBars = 1;
    uint16_t maxBars = std::max(uint16_t(4), uint16_t(data.numberOfBars));

    if (newNumBars < minBars) newNumBars = minBars;
    if (newNumBars > int16_t(maxBars)) newNumBars = int16_t(maxBars);

    if (newNumBars != view.numBars) {
      setNumBars(newNumBars);
      drawFlags |= DRAW_CONTENT_FULL;
    }
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleViewScrollX(EncoderEvents e) {
  if ((data.isSongMode && !data.useGridSongMode) || data.activeTrack == SONG_MODE_MUTE_TRACK) {
    uint16_t maxBars = getMaxBarsForCurrentMode();

    int16_t newStartBar = view.startBar + e.dir;

    uint16_t maxStartBar = 0;
    if (maxBars > view.numBars) {
      maxStartBar = maxBars - view.numBars;
    }

    if (newStartBar < 0) newStartBar = 0;
    if (newStartBar > maxStartBar) newStartBar = maxStartBar;

    if (newStartBar != view.startBar) {
      setBarStart(newStartBar);
      drawFlags |= DRAW_CONTENT_FULL;
    }
  }
  else {
    // PATTERN MODE: Allow scrolling with extended bar range
    int16_t newStartBar = view.startBar + e.dir;

    // Calculate maximum start bar based on extended range (at least 4 bars total)
    uint16_t totalAvailableBars = std::max(uint16_t(4), uint16_t(data.numberOfBars));
    int16_t maxStartBar = int16_t(totalAvailableBars) - view.numBars;
    if (maxStartBar < 0) maxStartBar = 0;

    if (newStartBar < 0) newStartBar = 0;
    if (newStartBar > maxStartBar) newStartBar = maxStartBar;

    if (newStartBar != view.startBar) {
      setBarStart(newStartBar);
      drawFlags |= DRAW_CONTENT_FULL;
    }
  }
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditModeSettingActivated(EncoderEvents e) {
  uint16_t headerFlags = (1 << editMode);

  switch (editMode) {
  case EDIT_SELECT_NOTE:
    headerFlags = handleEditSelectNote(e, headerFlags);
    break;

  case EDIT_LAYER:
    headerFlags = handleEditLayer(e, headerFlags);
    break;

  case EDIT_NOTEON:
    headerFlags = handleEditNoteOn(e, headerFlags);
    break;

  case EDIT_NOTEOFF:
    headerFlags = handleEditNoteOff(e, headerFlags);
    break;

  case EDIT_VELOCITY:
    headerFlags = handleEditVelocity(e, headerFlags);
    break;

  case EDIT_NOTE:
    headerFlags = handleEditNote(e, headerFlags);
    break;
  }

  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditSelectNote(EncoderEvents e, uint16_t headerFlags) {
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    return handleMuteAutomationSelection(e, headerFlags);
  }
  else {
    return handleNoteSelection(e, headerFlags);
  }
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleMuteAutomationSelection(EncoderEvents e, uint16_t headerFlags) {
  if (!muteAutomationEvents.empty()) {
    std::vector<std::pair<int16_t, uint16_t>> eventsWithPos;

    for (size_t i = 0; i < muteAutomationEvents.size(); i++) {
      auto& event = muteAutomationEvents[i];
      uint16_t songPatternIndex = getSongPatternForEvent(event);
      uint32_t absoluteTime = (songPatternIndex * data.numberOfBars * data.patternLengthMs) +
        (event.patternNumber * data.patternLengthMs) + event.patternMs;
      const uint16_t screenX = getNoteCoord(absoluteTime);

      if (screenX >= ROLL_WIDTH && screenX < DISPLAY_WIDTH) {
        eventsWithPos.push_back({ (int16_t)i, screenX });
      }
    }

    std::sort(eventsWithPos.begin(), eventsWithPos.end(),
      [](const auto& a, const auto& b) {
        return a.second < b.second;
      });

    std::vector<int16_t> sortedEvents;
    for (const auto& pair : eventsWithPos) {
      sortedEvents.push_back(pair.first);
    }

    if (!sortedEvents.empty()) {
      int16_t newSelection;

      if (selectedMuteEvent == -1) {
        newSelection = sortedEvents[0];
      }
      else {
        int16_t currentPos = -1;
        for (size_t i = 0; i < sortedEvents.size(); i++) {
          if (sortedEvents[i] == selectedMuteEvent) {
            currentPos = i;
            break;
          }
        }

        if (currentPos == -1) {
          newSelection = sortedEvents[0];
        }
        else {
          int16_t newPos = currentPos + e.dir;
          if (newPos < 0) newPos = sortedEvents.size() - 1;
          else if (newPos >= static_cast<int16_t>(sortedEvents.size())) newPos = 0;

          newSelection = sortedEvents[newPos];
        }
      }

      if (newSelection != selectedMuteEvent) {
        if (selectedMuteEvent > -1) {
          const auto& oldEvent = muteAutomationEvents.at(selectedMuteEvent);
          uint16_t oldSongPattern = getSongPatternForEvent(oldEvent);
          uint32_t oldAbsoluteTime = (oldSongPattern * data.numberOfBars * data.patternLengthMs) +
            (oldEvent.patternNumber * data.patternLengthMs) + oldEvent.patternMs;
          UpdateRange oldRegion;
          oldRegion.fromX = getNoteCoord(oldAbsoluteTime) - 5;
          oldRegion.toX = getNoteCoord(oldAbsoluteTime) + 5;
          drawRegionsX.push_back(oldRegion);
        }

        selectedMuteEvent = newSelection;

        if (selectedMuteEvent > -1) {
          const auto& newEvent = muteAutomationEvents.at(selectedMuteEvent);
          uint16_t newSongPattern = getSongPatternForEvent(newEvent);
          uint32_t newAbsoluteTime = (newSongPattern * data.numberOfBars * data.patternLengthMs) +
            (newEvent.patternNumber * data.patternLengthMs) + newEvent.patternMs;
          UpdateRange newRegion;
          newRegion.fromX = getNoteCoord(newAbsoluteTime) - 5;
          newRegion.toX = getNoteCoord(newAbsoluteTime) + 5;
          drawRegionsX.push_back(newRegion);
        }

        headerFlags = (1 << EDIT_SELECT_NOTE);
        drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_MUTE_AUTOMATION;
      }
    }
    else {
      if (selectedMuteEvent > -1) {
        const auto& oldEvent = muteAutomationEvents.at(selectedMuteEvent);
        uint16_t oldSongPattern = getSongPatternForEvent(oldEvent);
        uint32_t oldAbsoluteTime = (oldSongPattern * data.numberOfBars * data.patternLengthMs) +
          (oldEvent.patternNumber * data.patternLengthMs) + oldEvent.patternMs;
        UpdateRange oldRegion;
        oldRegion.fromX = getNoteCoord(oldAbsoluteTime) - 5;
        oldRegion.toX = getNoteCoord(oldAbsoluteTime) + 5;
        drawRegionsX.push_back(oldRegion);

        selectedMuteEvent = -1;
        drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_MUTE_AUTOMATION;
      }
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleNoteSelection(EncoderEvents e, uint16_t headerFlags) {
  if (!notePairs.empty()) {
    bool selectedLayerHasNotes = false;
    for (const auto& note : notePairs) {
      if (note.noteOn.layer == selectedLayer) {
        selectedLayerHasNotes = true;
        break;
      }
    }

    if (!selectedLayerHasNotes) {
      for (int8_t layer = 0; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; layer++) {
        for (const auto& note : notePairs) {
          if (note.noteOn.layer == layer) {
            selectedLayer = layer;
            selectedLayerHasNotes = true;
            break;
          }
        }
        if (selectedLayerHasNotes) break;
      }
    }

    std::vector<std::pair<int16_t, uint16_t>> notesWithPos;

    for (size_t i = 0; i < notePairs.size(); i++) {
      auto& note = notePairs[i];
      if (note.noteOn.layer != selectedLayer) {
        continue;
      }

      uint32_t timeOn;
      if (data.isSongMode && !data.useGridSongMode) {
        uint16_t absoluteBarOn = getActualBarForNote(note);
        timeOn = absoluteBarOn * data.patternLengthMs + note.noteOn.patternMs;
      }
      else {
        timeOn = getEventTime(note.noteOn);
      }

      const uint16_t screenX = getNoteCoord(timeOn);

      if (screenX >= ROLL_WIDTH && screenX < DISPLAY_WIDTH) {
        notesWithPos.push_back({ (int16_t)i, screenX });
      }
    }

    if (!notesWithPos.empty()) {
      int16_t newSelection;

      if (selectedNote == -1) {
        newSelection = notesWithPos[0].first;
      }
      else {
        int16_t currentPos = -1;
        for (size_t i = 0; i < notesWithPos.size(); i++) {
          if (notesWithPos[i].first == selectedNote) {
            currentPos = i;
            break;
          }
        }

        if (currentPos == -1) {
          newSelection = notesWithPos[0].first;
        }
        else {
          int16_t newPos = currentPos + e.dir;
          if (newPos < 0) newPos = notesWithPos.size() - 1;
          else if (newPos >= static_cast<int16_t>(notesWithPos.size())) newPos = 0;

          newSelection = notesWithPos[newPos].first;
        }
      }

      if (newSelection != selectedNote) {
        // Handle old selection
        if (selectedNote > -1) {
          const auto& oldNote = notePairs.at(selectedNote);
          uint32_t oldTimeOn, oldTimeOff;

          if (data.isSongMode && !data.useGridSongMode) {
            uint16_t absoluteBarOn = getActualBarForNote(oldNote);
            oldTimeOn = absoluteBarOn * data.patternLengthMs + oldNote.noteOn.patternMs;
            uint16_t absoluteBarOff = getActualBarForNote(oldNote);
            oldTimeOff = absoluteBarOff * data.patternLengthMs + oldNote.noteOff.patternMs;
          }
          else {
            oldTimeOn = getEventTime(oldNote.noteOn);
            oldTimeOff = getEventTime(oldNote.noteOff);
          }

          // Create update region for old selection
          addSelectionUpdateRegion(oldTimeOn, oldTimeOff, oldNote.noteOn.note_in);
        }

        selectedNote = newSelection;

        if (selectedNote > -1) {
          const auto& newNote = notePairs.at(selectedNote);
          uint32_t newTimeOn, newTimeOff;

          if (data.isSongMode && !data.useGridSongMode) {
            uint16_t absoluteBarOn = getActualBarForNote(newNote);
            newTimeOn = absoluteBarOn * data.patternLengthMs + newNote.noteOn.patternMs;
            uint16_t absoluteBarOff = getActualBarForNote(newNote);
            newTimeOff = absoluteBarOff * data.patternLengthMs + newNote.noteOff.patternMs;
          }
          else {
            newTimeOn = getEventTime(newNote.noteOn);
            newTimeOff = getEventTime(newNote.noteOff);
          }

          // Create update region for new selection
          addSelectionUpdateRegion(newTimeOn, newTimeOff, newNote.noteOn.note_in);
        }

        drawFlags |= DRAW_BACKGROUND | DRAW_NOTES;
        headerFlags = HEADER_DRAW_ALL;
      }
    }
    else {
      if (selectedNote > -1) {
        const auto& oldNote = notePairs.at(selectedNote);
        uint32_t oldTimeOn, oldTimeOff;

        if (data.isSongMode && !data.useGridSongMode) {
          uint16_t absoluteBarOn = getActualBarForNote(oldNote);
          oldTimeOn = absoluteBarOn * data.patternLengthMs + oldNote.noteOn.patternMs;
          uint16_t absoluteBarOff = getActualBarForNote(oldNote);
          oldTimeOff = absoluteBarOff * data.patternLengthMs + oldNote.noteOff.patternMs;
        }
        else {
          oldTimeOn = getEventTime(oldNote.noteOn);
          oldTimeOff = getEventTime(oldNote.noteOff);
        }

        // Create update region for old selection
        addSelectionUpdateRegion(oldTimeOn, oldTimeOff, oldNote.noteOn.note_in);

        selectedNote = -1;
        headerFlags = HEADER_DRAW_ALL;
        drawFlags |= DRAW_BACKGROUND | DRAW_NOTES;
      }
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditLayer(EncoderEvents e, uint16_t headerFlags) {
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    if (selectedMuteEvent > -1 && !muteAutomationEvents.empty()) {
      LiveSequencer::MidiEvent& event = muteAutomationEvents.at(selectedMuteEvent);
      int8_t layer = event.note_in_velocity;
      if (hasConstrainedChanged(layer, e.dir, int8_t(0), int8_t(LiveSequencer::LIVESEQUENCER_NUM_LAYERS - 1))) {
        uint16_t songPattern = getSongPatternForEvent(event);
        uint32_t absoluteTime = (songPattern * data.numberOfBars * data.patternLengthMs) +
          (event.patternNumber * data.patternLengthMs) + event.patternMs;
        UpdateRange region;
        region.fromX = getNoteCoord(absoluteTime) - 5;
        region.toX = getNoteCoord(absoluteTime) + 5;
        drawRegionsX.push_back(region);

        event.note_in_velocity = layer;
        updateSongEvent(event);

        drawRegionsX.push_back(region);
        headerFlags |= (1 << EDIT_LAYER);
        drawFlags |= DRAW_MUTE_AUTOMATION | DRAW_BACKGROUND;
      }
    }
  }
  else {
    // Always allow selection from ALL layers (0-3)
    int8_t newSelectedLayer = selectedLayer + e.dir;

    // Wrap around using modulo arithmetic
    if (newSelectedLayer < 0) {
      newSelectedLayer = LiveSequencer::LIVESEQUENCER_NUM_LAYERS - 1;
    }
    else if (newSelectedLayer >= LiveSequencer::LIVESEQUENCER_NUM_LAYERS) {
      newSelectedLayer = 0;
    }

    if (newSelectedLayer != selectedLayer) {
      // CLEAR UNDO STATE WHEN CHANGING LAYERS
      clearUndoState();
      
      if (selectedNote > -1) {
        queueDrawNote(notePairs.at(selectedNote));
        selectedNote = -1;
      }

      selectedLayer = newSelectedLayer;
      reloadNotes();
      headerFlags |= (1 << EDIT_LAYER);
      drawFlags |= DRAW_NOTES;
      queueDrawNote({ ROLL_WIDTH, DISPLAY_WIDTH, {} });
      drawLayersIndicator();
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditNoteOn(EncoderEvents e, uint16_t headerFlags) {

  if (data.activeTrack == SONG_MODE_NOTES_TRACK && selectedNote > -1) {
    if (!editingNote.active) {
      startEditingNote(selectedNote);
    }

    // Get current absolute time of the editing note
    uint32_t currentAbsTimeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
    uint32_t currentAbsTimeOff = getAbsoluteEventTime(editingNote.editingNoteOff);
    uint32_t noteLength = (currentAbsTimeOff > currentAbsTimeOn) ? (currentAbsTimeOff - currentAbsTimeOn) : getQuantizationGridStep();

    // Calculate movement
    uint32_t gridStep = getQuantizationGridStep();
    int32_t movement = e.dir * e.speed * gridStep;

    // Calculate new absolute time
    int64_t newAbsTimeOn = (int64_t)currentAbsTimeOn + movement;

    // Calculate viewport boundaries
    const uint32_t viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
    const uint32_t viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);

    // Constrain to viewport but allow movement across chunks
    if (newAbsTimeOn < (int64_t)viewStartMs) newAbsTimeOn = viewStartMs;
    if (newAbsTimeOn > (int64_t)(viewEndMs - noteLength)) newAbsTimeOn = viewEndMs - noteLength;

    uint32_t finalAbsTimeOn = (uint32_t)newAbsTimeOn;
    uint32_t finalAbsTimeOff = finalAbsTimeOn + noteLength;

    if (finalAbsTimeOn != currentAbsTimeOn) {
      // Use helper for clean region creation
      addNoteUpdateRegion(currentAbsTimeOn, currentAbsTimeOff,
        editingNote.editingNoteOn.note_in, true);
      addNoteUpdateRegion(finalAbsTimeOn, finalAbsTimeOff,
        editingNote.editingNoteOn.note_in, false);

      // Update editing note position
      updateEditingNotePosition(finalAbsTimeOn, finalAbsTimeOff);

      headerFlags |= (1 << EDIT_NOTEON) | (1 << EDIT_NOTEOFF);
      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }
  //pattern mode
  else if (selectedNote > -1) {
    // FIXED PATTERN MODE NOTE-ON EDITING
    LiveSequencer::MidiEvent& on = notePairs.at(selectedNote).noteOn;
    LiveSequencer::MidiEvent& off = notePairs.at(selectedNote).noteOff;

    uint32_t oldNoteOnTime = getEventTime(on);
    uint32_t noteOffTime = getEventTime(off);
    uint32_t noteLength = (noteOffTime > oldNoteOnTime) ? (noteOffTime - oldNoteOnTime) : 1;

    uint32_t gridStep = getQuantizationGridStep();
    const uint32_t minTime = 0;
    uint32_t viewEndMs = uint32_t(view.startBar + view.numBars) * uint32_t(data.patternLengthMs);
    uint32_t maxTimeFromScreen = (viewEndMs > noteLength) ? (viewEndMs - noteLength) : 0;
    uint32_t maxTime = maxTimeFromScreen;

    int32_t gridSteps = roundf(float(oldNoteOnTime) / float(gridStep));
    gridSteps = constrain(gridSteps + e.dir * e.speed,
      roundf(float(minTime) / float(gridStep)),
      roundf(float(maxTime) / float(gridStep)));
    uint32_t newTime = gridSteps * gridStep;

    if (newTime != oldNoteOnTime) {
      // Store old position for redraw region
      uint32_t oldTimeOff = getEventTime(off);

      // Update the note times
      setEventTime(on, newTime);
      uint32_t newNoteOffTime = newTime + noteLength;
      setEventTime(off, newNoteOffTime);

      // Calculate X range covering both old and new positions
      uint16_t fromX = std::min(getNoteCoord(oldNoteOnTime), getNoteCoord(newTime));
      uint16_t toX = std::max(getNoteCoord(oldTimeOff), getNoteCoord(newNoteOffTime));
      fromX = (fromX < 5 + ROLL_WIDTH) ? ROLL_WIDTH : (fromX - 5);
      toX = (toX > DISPLAY_WIDTH - 5) ? DISPLAY_WIDTH : (toX + 5);

      // Create region with empty notes for complete redraw
      UpdateRange updateRange;
      updateRange.fromX = fromX;
      updateRange.toX = toX;
      updateRange.notes = {}; // EMPTY = redraw everything
      drawRegionsX.push_back(updateRange);

      headerFlags |= (1 << EDIT_NOTEON) | (1 << EDIT_NOTEOFF);
      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditNoteOff(EncoderEvents e, uint16_t headerFlags) {

  if (data.activeTrack == SONG_MODE_NOTES_TRACK && selectedNote > -1) {
    if (!editingNote.active) {
      startEditingNote(selectedNote);
    }

    // Use the individual note events
    uint32_t currentAbsTimeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
    uint32_t currentAbsTimeOff = getAbsoluteEventTime(editingNote.editingNoteOff);

    uint32_t gridStep = getQuantizationGridStep();
    int64_t movement64 = (int64_t)e.dir * (int64_t)e.speed * (int64_t)gridStep;

    // Calculate viewport boundaries
    const uint32_t viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
    const uint32_t viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);

    int64_t newAbsTimeOff = (int64_t)currentAbsTimeOff + movement64;
    int64_t minOff = (int64_t)currentAbsTimeOn + (int64_t)gridStep;
    int64_t maxOff = (int64_t)viewEndMs;

    if (newAbsTimeOff < minOff) newAbsTimeOff = minOff;
    if (newAbsTimeOff > maxOff) newAbsTimeOff = maxOff;

    uint32_t finalAbsTimeOff = (uint32_t)newAbsTimeOff;

    if (finalAbsTimeOff != currentAbsTimeOff) {
      // MANUAL REDRAW: The note position changes but the start X stays the same
      uint16_t x = getNoteCoord(currentAbsTimeOn);
      uint16_t oldW = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);
      uint16_t newW = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

      // Use the maximum width to ensure we clear the entire area
      uint16_t maxW = std::max(oldW, newW);

      UpdateRange updateRegion;
      updateRegion.fromX = (x < 5) ? ROLL_WIDTH : std::max(ROLL_WIDTH, (uint16_t)(x - 5));
      updateRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + maxW + 5));
      updateRegion.notes = {}; // Empty set = redraw all notes in this X range
      drawRegionsX.push_back(updateRegion);

      // Update editing note position
      updateEditingNotePosition(currentAbsTimeOn, finalAbsTimeOff);

      headerFlags |= (1 << EDIT_NOTEOFF);
      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }

  //pattern mode
  else if (selectedNote > -1) {
    // FIXED PATTERN MODE NOTE-OFF EDITING
    LiveSequencer::MidiEvent& on = notePairs.at(selectedNote).noteOn;
    LiveSequencer::MidiEvent& off = notePairs.at(selectedNote).noteOff;

    uint32_t noteOnTime = getEventTime(on);
    uint32_t oldNoteOffTime = getEventTime(off);

    uint32_t gridStep = getQuantizationGridStep();
    const uint32_t minTime = noteOnTime + gridStep;
    uint32_t viewEndMs = uint32_t(view.startBar + view.numBars) * uint32_t(data.patternLengthMs);
    uint32_t maxTime = viewEndMs;

    int32_t gridSteps = roundf(float(oldNoteOffTime) / float(gridStep));
    gridSteps = constrain(gridSteps + e.dir * e.speed,
      roundf(float(minTime) / float(gridStep)),
      roundf(float(maxTime) / float(gridStep)));
    uint32_t newTime = gridSteps * gridStep;

    if (newTime != oldNoteOffTime) {
      // Update the note time
      setEventTime(off, newTime);

      // Calculate X range covering both old and new positions
      uint16_t fromX = std::min(getNoteCoord(noteOnTime), getNoteCoord(noteOnTime));
      uint16_t toX = std::max(getNoteCoord(oldNoteOffTime), getNoteCoord(newTime));
      fromX = (fromX < 5 + ROLL_WIDTH) ? ROLL_WIDTH : (fromX - 5);
      toX = (toX > DISPLAY_WIDTH - 5) ? DISPLAY_WIDTH : (toX + 5);

      // Create region with empty notes for complete redraw
      UpdateRange updateRange;
      updateRange.fromX = fromX;
      updateRange.toX = toX;
      updateRange.notes = {}; // EMPTY = redraw everything
      drawRegionsX.push_back(updateRange);

      headerFlags |= (1 << EDIT_NOTEOFF);
      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }
  return headerFlags;
}

FLASHMEM void UI_LiveSeq_PianoRoll::addSelectionUpdateRegion(uint32_t timeOn, uint32_t timeOff, uint8_t notePitch) {
  uint16_t x = getNoteCoord(timeOn);

  // Calculate width
  uint32_t lenMs = (timeOff > timeOn) ? (timeOff - timeOn) : 1;
  uint16_t w = std::max(1, int(roundf(lenMs * view.msToPix)));

  // Region: Specific note (for selection highlight) - NO vertical bar region
  UpdateRange noteRegion;
  noteRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(x - 3));
  noteRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + w + 3));
  noteRegion.notes = { notePitch };
  drawRegionsX.push_back(noteRegion);

  drawFlags |= DRAW_BACKGROUND;
}

FLASHMEM void UI_LiveSeq_PianoRoll::startEditingNote(int16_t noteIndex) {
  if (noteIndex < 0 || noteIndex >= static_cast<int16_t>(notePairs.size())) {
    return;
  }

  // Store which note we're editing
  editingNote.originalNoteIndex = noteIndex;
  editingNote.originalNoteOn = notePairs[noteIndex].noteOn;
  editingNote.originalNoteOff = notePairs[noteIndex].noteOff;
  editingNote.editingNoteOn = editingNote.originalNoteOn;
  editingNote.editingNoteOff = editingNote.originalNoteOff;

  // Store the original event locations
  if (noteIndex < static_cast<int16_t>(eventLocations.size())) {
    SongEventLocation& loc = eventLocations[noteIndex];
    editingNote.originalSongPatternOn = loc.noteOnPattern;
    editingNote.originalNoteOnIndex = loc.noteOnIndex;
    editingNote.originalSongPatternOff = loc.noteOffPattern;
    editingNote.originalNoteOffIndex = loc.noteOffIndex;

    // Initialize target chunks to original chunks
    editingNote.targetSongPatternOn = loc.noteOnPattern;
    editingNote.targetSongPatternOff = loc.noteOffPattern;
  }

  // Mark the note as invalid in data (same as delete)
  if (data.songEvents.count(editingNote.originalSongPatternOn) &&
    editingNote.originalNoteOnIndex < data.songEvents[editingNote.originalSongPatternOn].size()) {
    data.songEvents[editingNote.originalSongPatternOn][editingNote.originalNoteOnIndex].event = midi::InvalidType;
  }

  if (data.songEvents.count(editingNote.originalSongPatternOff) &&
    editingNote.originalNoteOffIndex < data.songEvents[editingNote.originalSongPatternOff].size()) {
    data.songEvents[editingNote.originalSongPatternOff][editingNote.originalNoteOffIndex].event = midi::InvalidType;
  }

  editingNote.active = true;

  // Create update region for where the note currently is
  uint32_t currentAbsTimeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
  uint16_t currentX = getNoteCoord(currentAbsTimeOn);
  uint16_t currentW = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

  // Create a region with EMPTY notes to redraw everything in this area
  UpdateRange fullRegion;
  fullRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(currentX - 5));
  fullRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(currentX + currentW + 5));
  fullRegion.notes = {}; // EMPTY = redraw ALL notes in this X range
  drawRegionsX.push_back(fullRegion);

  drawFlags |= DRAW_VERTICAL_BARS | DRAW_NOTES;
}

FLASHMEM void UI_LiveSeq_PianoRoll::updateEditingNotePosition(uint32_t newAbsTimeOn, uint32_t newAbsTimeOff) {
  if (!editingNote.active) return;

  const uint32_t chunkLengthMs = data.numberOfBars * data.patternLengthMs;

  // Calculate target chunks
  editingNote.targetSongPatternOn = newAbsTimeOn / chunkLengthMs;
  uint32_t timeInChunkOn = newAbsTimeOn % chunkLengthMs;

  editingNote.targetSongPatternOff = newAbsTimeOff / chunkLengthMs;
  uint32_t timeInChunkOff = newAbsTimeOff % chunkLengthMs;

  // Update pattern-relative times
  editingNote.editingNoteOn.patternNumber = (timeInChunkOn / data.patternLengthMs) % data.numberOfBars;
  editingNote.editingNoteOn.patternMs = timeInChunkOn % data.patternLengthMs;

  editingNote.editingNoteOff.patternNumber = (timeInChunkOff / data.patternLengthMs) % data.numberOfBars;
  editingNote.editingNoteOff.patternMs = timeInChunkOff % data.patternLengthMs;
}

FLASHMEM void UI_LiveSeq_PianoRoll::updateEditingNotePitch(uint8_t newPitch) {
  if (!editingNote.active) return;

  editingNote.editingNoteOn.note_in = newPitch;
  editingNote.editingNoteOff.note_in = newPitch;
}

FLASHMEM void UI_LiveSeq_PianoRoll::commitEditingNote() {

  if (!editingNote.active) return;

  // First, restore the original events to valid state (undo the soft delete)
  if (data.songEvents.count(editingNote.originalSongPatternOn) &&
    editingNote.originalNoteOnIndex < data.songEvents[editingNote.originalSongPatternOn].size()) {
    data.songEvents[editingNote.originalSongPatternOn][editingNote.originalNoteOnIndex].event = midi::NoteOn;
  }

  if (data.songEvents.count(editingNote.originalSongPatternOff) &&
    editingNote.originalNoteOffIndex < data.songEvents[editingNote.originalSongPatternOff].size()) {
    data.songEvents[editingNote.originalSongPatternOff][editingNote.originalNoteOffIndex].event = midi::NoteOff;
  }

  // Remove original note-on
  if (data.songEvents.count(editingNote.originalSongPatternOn)) {
    auto& events = data.songEvents[editingNote.originalSongPatternOn];
    for (auto it = events.begin(); it != events.end(); ) {
      if (it->track == editingNote.originalNoteOn.track &&
        it->layer == editingNote.originalNoteOn.layer &&
        it->note_in == editingNote.originalNoteOn.note_in &&
        it->event == midi::NoteOn &&  // Make sure it's a note-on
        it->patternNumber == editingNote.originalNoteOn.patternNumber &&
        it->patternMs == editingNote.originalNoteOn.patternMs &&
        it->note_in_velocity == editingNote.originalNoteOn.note_in_velocity) {
        it = events.erase(it);
        break;
      }
      else {
        ++it;
      }
    }
  }

  // Remove original note-off
  if (data.songEvents.count(editingNote.originalSongPatternOff)) {
    auto& events = data.songEvents[editingNote.originalSongPatternOff];
    for (auto it = events.begin(); it != events.end(); ) {
      if (it->track == editingNote.originalNoteOff.track &&
        it->layer == editingNote.originalNoteOff.layer &&
        it->note_in == editingNote.originalNoteOff.note_in &&
        it->event == midi::NoteOff &&  // Make sure it's a note-off
        it->patternNumber == editingNote.originalNoteOff.patternNumber &&
        it->patternMs == editingNote.originalNoteOff.patternMs &&
        it->note_in_velocity == editingNote.originalNoteOff.note_in_velocity) {
        it = events.erase(it);
        break;
      }
      else {
        ++it;
      }
    }
  }

  // Calculate target chunks for the edited note
  const uint32_t chunkLengthMs = data.numberOfBars * data.patternLengthMs;
  uint32_t absTimeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
  uint32_t absTimeOff = getAbsoluteEventTime(editingNote.editingNoteOff);

  uint16_t targetChunkOn = absTimeOn / chunkLengthMs;
  uint16_t targetChunkOff = absTimeOff / chunkLengthMs;

  // Ensure target chunks exist
  if (data.songEvents.find(targetChunkOn) == data.songEvents.end()) {
    data.songEvents[targetChunkOn] = {};
  }
  if (data.songEvents.find(targetChunkOff) == data.songEvents.end()) {
    data.songEvents[targetChunkOff] = {};
  }

  // Add the edited events
  data.songEvents[targetChunkOn].push_back(editingNote.editingNoteOn);
  data.songEvents[targetChunkOff].push_back(editingNote.editingNoteOff);

  editingNote.active = false;

  // Create update regions for OLD and NEW positions
  // OLD position: where the note used to be
  uint32_t oldAbsTimeOn = getAbsoluteEventTime(editingNote.originalNoteOn);
  uint16_t oldX = getNoteCoord(oldAbsTimeOn);
  uint16_t oldW = calculateNoteWidth(editingNote.originalNoteOn, editingNote.originalNoteOff);

  // NEW position: where the note is now
  uint16_t newX = getNoteCoord(absTimeOn);
  uint16_t newW = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

  // Create a SINGLE region that covers BOTH old and new positions with EMPTY notes
  // This ensures ALL notes in this area get redrawn
  UpdateRange fullRegion;
  fullRegion.fromX = std::min(oldX, newX) - 5;
  fullRegion.toX = std::max(oldX + oldW, newX + newW) + 5;
  fullRegion.notes = {}; // EMPTY = redraw ALL notes in this X range
  drawRegionsX.push_back(fullRegion);

  // Also need to redraw vertical bars
  drawFlags |= DRAW_VERTICAL_BARS | DRAW_NOTES;

  // Force complete reload
  reloadNotes(true);

  // Find the exact note we just committed
  selectedNote = -1;
  for (size_t i = 0; i < notePairs.size(); ++i) {
    // Compare all the properties of the note-on event
    if (notePairs[i].noteOn.track == editingNote.editingNoteOn.track &&
      notePairs[i].noteOn.layer == editingNote.editingNoteOn.layer &&
      notePairs[i].noteOn.note_in == editingNote.editingNoteOn.note_in &&
      notePairs[i].noteOn.note_in_velocity == editingNote.editingNoteOn.note_in_velocity &&
      notePairs[i].noteOn.patternNumber == editingNote.editingNoteOn.patternNumber &&
      notePairs[i].noteOn.patternMs == editingNote.editingNoteOn.patternMs &&
      notePairs[i].noteOn.event == editingNote.editingNoteOn.event) {

      selectedNote = i;
      break;
    }
  }

  liveSeq.requestSortEvents();
}

FLASHMEM void UI_LiveSeq_PianoRoll::cancelEditingNote() {
  if (!editingNote.active) return;

  // Create update region for where the editing note currently is
  uint32_t currentAbsTimeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
  uint16_t currentX = getNoteCoord(currentAbsTimeOn);
  uint16_t currentW = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

  // Create a region with EMPTY notes to redraw everything in this area
  UpdateRange fullRegion;
  fullRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(currentX - 5));
  fullRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(currentX + currentW + 5));
  fullRegion.notes = {}; // EMPTY = redraw ALL notes in this X range
  drawRegionsX.push_back(fullRegion);

  editingNote.active = false;

  // For cancel, we should be able to keep the same index since we're restoring the original
  // Just make sure the note is valid again
  if (editingNote.originalNoteIndex >= 0 &&
    editingNote.originalNoteIndex < static_cast<int16_t>(notePairs.size())) {
    notePairs[editingNote.originalNoteIndex].noteOn.event = midi::NoteOn;
    notePairs[editingNote.originalNoteIndex].noteOff.event = midi::NoteOff;
    selectedNote = editingNote.originalNoteIndex;
  }

  // Set flags for redraw
  drawFlags |= DRAW_VERTICAL_BARS | DRAW_NOTES;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::calculateNoteWidth(const LiveSequencer::MidiEvent& noteOn, const LiveSequencer::MidiEvent& noteOff) const {
  uint32_t timeOn, timeOff;

  if (data.isSongMode && !data.useGridSongMode) {
    timeOn = getAbsoluteEventTime(noteOn);
    timeOff = getAbsoluteEventTime(noteOff);

    if (timeOff <= timeOn) {
      timeOff = timeOn + data.patternLengthMs / 16;
    }
  }
  else {
    timeOn = getEventTime(noteOn);
    timeOff = getEventTime(noteOff);
  }

  const uint32_t lenMs = (timeOff > timeOn) ? (timeOff - timeOn) : 1;
  return std::max(1, int(roundf(lenMs * view.msToPix)));
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawEditingNote() {
  if (!editingNote.active) return;

  // Calculate position and size
  uint32_t timeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
  uint16_t x = getNoteCoord(timeOn);
  uint16_t w = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

  // Calculate Y position
  if (editingNote.editingNoteOn.note_in < view.lowestNote || editingNote.editingNoteOn.note_in >= view.highestNote) {
    return;
  }

  const uint8_t noteIndex = view.highestNote - editingNote.editingNoteOn.note_in - 1;
  const uint16_t y = round(noteIndex * view.noteHeight);
  const uint8_t h = view.noteHeight;

  // Ensure note is within content area
  if (y < 0 || y >= CONTENT_HEIGHT || (y + h) > CONTENT_HEIGHT) {
    return;
  }

  // Draw the editing note in RED - always draw regardless of regions
  display.console = true;
  if (w > 1) {
    display.fillRect(x, y, w, h, RED);
  }
  else {
    display.drawLine(x, y, x, y + h - 1, RED);
  }
}

FLASHMEM size_t findEventIndex(const std::vector<LiveSequencer::MidiEvent>& events, const LiveSequencer::MidiEvent& target) {
  for (size_t i = 0; i < events.size(); i++) {
    const auto& event = events[i];
    if (event.track == target.track &&
      event.layer == target.layer &&
      event.note_in == target.note_in &&
      event.event == target.event &&
      event.patternNumber == target.patternNumber &&
      event.patternMs == target.patternMs &&
      event.note_in_velocity == target.note_in_velocity) {
      return i;
    }
  }
  return SIZE_MAX;
}


FLASHMEM uint32_t UI_LiveSeq_PianoRoll::getMaxSongTime() const {
  if (!data.isSongMode || data.useGridSongMode) {
    return data.numberOfBars * data.patternLengthMs;
  }

  // Find the highest chunk index in use
  uint16_t maxChunk = 0;
  for (const auto& patternEntry : data.songEvents) {
    if (patternEntry.first > maxChunk) {
      maxChunk = patternEntry.first;
    }
  }

  // Return time at the end of the highest chunk
  return (maxChunk + 1) * data.numberOfBars * data.patternLengthMs;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditVelocity(EncoderEvents e, uint16_t headerFlags) {
  if (selectedNote > -1) {
    if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
      // SONG MODE: Use editing note system like handleEditNoteOn()
      if (!editingNote.active) {
        startEditingNote(selectedNote);
      }

      int8_t velo = editingNote.editingNoteOn.note_in_velocity;
      if (hasConstrainedChanged(velo, e.dir * e.speed, int8_t(0), int8_t(127))) {
        editingNote.editingNoteOn.note_in_velocity = velo;
        editingNote.editingNoteOff.note_in_velocity = velo; // Note-off should match

        // Force redraw of the editing note area
        uint32_t timeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
        uint16_t x = getNoteCoord(timeOn);
        uint16_t w = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

        UpdateRange updateRegion;
        updateRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(x - 3));
        updateRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + w + 3));
        updateRegion.notes = {}; // Empty = redraw all notes in this X range
        drawRegionsX.push_back(updateRegion);

        headerFlags |= (1 << EDIT_VELOCITY);
      }
    }
    else {
      // PATTERN MODE: Leave unchanged - it's working correctly
      LiveSequencer::MidiEvent& event = notePairs.at(selectedNote).noteOn;
      int8_t velo = notePairs.at(selectedNote).noteOn.note_in_velocity;
      if (hasConstrainedChanged(velo, e.dir * e.speed, int8_t(0), int8_t(127))) {
        event.note_in_velocity = velo;
        if (data.isSongMode && !data.useGridSongMode) {
          updateSongEvent(event);
          reloadNotes();
        }
        headerFlags |= (1 << EDIT_VELOCITY);
      }
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleEditNote(EncoderEvents e, uint16_t headerFlags) {
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    if (selectedMuteEvent > -1 && !muteAutomationEvents.empty()) {
      LiveSequencer::MidiEvent& eventCopy = muteAutomationEvents.at(selectedMuteEvent);

      uint16_t songPattern = getSongPatternForEvent(eventCopy);
      uint32_t absoluteTime = (songPattern * data.numberOfBars * data.patternLengthMs) +
        (eventCopy.patternNumber * data.patternLengthMs) + eventCopy.patternMs;
      UpdateRange region;
      region.fromX = getNoteCoord(absoluteTime) - 5;
      region.toX = getNoteCoord(absoluteTime) + 5;
      drawRegionsX.push_back(region);

      LiveSequencer::MidiEvent* realEvent = nullptr;
      for (auto& patternEntry : data.songEvents) {
        for (auto& ev : patternEntry.second) {
          if (ev.track == eventCopy.track &&
            ev.layer == eventCopy.layer &&
            ev.note_in == eventCopy.note_in &&
            ev.event == eventCopy.event &&
            ev.patternMs == eventCopy.patternMs &&
            ev.note_in_velocity == eventCopy.note_in_velocity) {
            realEvent = &ev;
            break;
          }
        }
        if (realEvent) break;
      }

      if (e.dir != 0) {
        uint8_t newMuteState = (eventCopy.note_in == LiveSequencer::TYPE_MUTE_ON) ?
          LiveSequencer::TYPE_MUTE_OFF : LiveSequencer::TYPE_MUTE_ON;

        if (realEvent) {
          realEvent->note_in = newMuteState;
          eventCopy.note_in = newMuteState;
        }
        else {
          eventCopy.note_in = newMuteState;
          updateSongEvent(eventCopy);
        }

        drawRegionsX.push_back(region);
        headerFlags |= (1 << EDIT_NOTE);
        drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_MUTE_AUTOMATION;
      }
    }
  }

  else if (data.activeTrack == SONG_MODE_NOTES_TRACK && selectedNote > -1) {
    if (!editingNote.active) {
      startEditingNote(selectedNote);
    }

    uint8_t oldPitch = editingNote.editingNoteOn.note_in;
    uint8_t newPitch = constrain(
      oldPitch + (e.dir * e.speed),
      view.lowestNote,
      view.highestNote - 1
    );

    if (newPitch != oldPitch) {
      // Get position info
      uint32_t timeOn = getAbsoluteEventTime(editingNote.editingNoteOn);
      uint16_t x = getNoteCoord(timeOn);
      uint16_t w = calculateNoteWidth(editingNote.editingNoteOn, editingNote.editingNoteOff);

      // Create region for OLD pitch - ONLY that specific note
      UpdateRange oldPitchRegion;
      oldPitchRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(x - 3));
      oldPitchRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + w + 3));
      oldPitchRegion.notes = { oldPitch }; // ONLY old pitch
      drawRegionsX.push_back(oldPitchRegion);

      // Update editing note pitch
      updateEditingNotePitch(newPitch);

      // Create region for NEW pitch - ONLY that specific note
      UpdateRange newPitchRegion;
      newPitchRegion.fromX = std::max(ROLL_WIDTH, (uint16_t)(x - 3));
      newPitchRegion.toX = std::min((uint16_t)DISPLAY_WIDTH, (uint16_t)(x + w + 3));
      newPitchRegion.notes = { newPitch }; // ONLY new pitch
      drawRegionsX.push_back(newPitchRegion);

      // Also redraw vertical bars in the area
      UpdateRange barRegion;
      barRegion.fromX = std::min(oldPitchRegion.fromX, newPitchRegion.fromX);
      barRegion.toX = std::max(oldPitchRegion.toX, newPitchRegion.toX);
      barRegion.notes = {}; // Empty = redraw bars
      drawRegionsX.push_back(barRegion);

      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
      headerFlags |= (1 << EDIT_NOTE);
    }
  }
  //pattern mode  
  else if (selectedNote > -1) {
    LiveSequencer::NotePair& thisNote = notePairs.at(selectedNote);
    int8_t note = thisNote.noteOn.note_in;

    if (hasConstrainedChanged(note, e.dir, int8_t(view.lowestNote), int8_t(view.highestNote))) {
      // Store the current time position
      uint32_t timeOn = getEventTime(thisNote.noteOn);
      uint32_t timeOff = getEventTime(thisNote.noteOff);

      // Update the note pitch
      thisNote.noteOn.note_in = note;
      thisNote.noteOff.note_in = note;

      // Use helper for full update region
      addFullUpdateRegion(timeOn, timeOff, timeOn, timeOff);

      headerFlags |= (1 << EDIT_NOTE);
      drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    }
  }
  return headerFlags;
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::handleStepModeSettingActivated(EncoderEvents e) {
  uint16_t headerFlags = 0;

  switch (stepMode) {
  case STEP_TARGET_TRACK:
    // Only used in song mode
    if (data.isSongMode && !data.useGridSongMode && data.activeTrack == SONG_MODE_NOTES_TRACK) {
      if (hasConstrainedChanged(data.stepRecordTargetTrack, e.dir,
        uint8_t(0), uint8_t(11))) { // 0-11 for tracks 1-12
        headerFlags |= (1 << STEP_TARGET_TRACK);
      }
    }
    break;

  case STEP_RECORD:
    if (hasConstrainedChanged(stepCursorPositionIndex, e.dir * e.speed,
      uint16_t(0), uint16_t(view.numBars * stepRecordSteps - 1))) {
      stepCursorPositionPrev = stepCursorPosition;
      stepCursorPosition = ROLL_WIDTH + round(stepCursorPositionIndex * stepRecordStepSizeMs * view.msToPix);
      queueDrawNote({ stepCursorPositionPrev, stepCursorPositionPrev, {} });
      queueDrawNote({ stepCursorPosition, stepCursorPosition, {} });
      drawFlags |= DRAW_STEP_CURSOR | DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
      headerFlags |= (1 << STEP_RECORD);
    }
    break;

  case STEP_SIZE:
    if (hasConstrainedChanged(stepRecordSteps, e.dir, uint8_t(1), uint8_t(32))) {
      stepRecordStepSizeMs = data.patternLengthMs / float(stepRecordSteps);
      stepCursorPositionIndex = 0;
      stepCursorPosition = ROLL_WIDTH;
      drawFlags |= DRAW_STEP_CURSOR;
      headerFlags |= (1 << STEP_SIZE);
    }
    break;

  case STEP_LAYER:
    if (hasConstrainedChanged(stepRecordLayer, e.dir,
      int8_t(0), int8_t(LiveSequencer::LIVESEQUENCER_NUM_LAYERS - 1))) {
      headerFlags |= (1 << STEP_LAYER);
    }
    break;
  }

  return headerFlags;
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleViewModeNormal(EncoderEvents e, uint8_t& oldMode, uint8_t& newMode) {
  oldMode = viewMode;
  if (data.activeTrack == SONG_MODE_MUTE_TRACK)
    newMode = constrain(viewMode + e.dir, 0, VIEW_NUM - 3); // no y scrolling
  else
    newMode = constrain(viewMode + e.dir, 0, VIEW_NUM - 1);
  viewMode = newMode;
  buttonView->drawNow();
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleEditModeNormal(EncoderEvents e, uint8_t& oldMode, uint8_t& newMode) {
  oldMode = editMode;
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    const uint8_t muteEditModes[] = { EDIT_SELECT_NOTE, EDIT_NOTEON, EDIT_NOTE };
    const uint8_t numMuteModes = 3;

    uint8_t currentPos = 0;
    for (uint8_t i = 0; i < numMuteModes; i++) {
      if (muteEditModes[i] == editMode) {
        currentPos = i;
        break;
      }
    }

    if (currentPos == 0 && muteEditModes[0] != editMode) {
      currentPos = 0;
      editMode = muteEditModes[0];
    }

    int8_t newPos = currentPos + e.dir;
    if (newPos < 0) newPos = numMuteModes - 1;
    else if (newPos >= numMuteModes) newPos = 0;

    newMode = muteEditModes[newPos];
  }
  else {
    newMode = constrain(editMode + e.dir, 0, EDIT_NUM - 1);
  }
  editMode = newMode;
  buttonEdit->drawNow();
}

FLASHMEM void UI_LiveSeq_PianoRoll::handleStepModeNormal(EncoderEvents e, uint8_t& oldMode, uint8_t& newMode) {

  uint8_t oldStepMode = stepMode;
  if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
    return;
  }
  if (data.isSongMode && !data.useGridSongMode && data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Song mode: cycle through all step modes including target track
    stepMode = constrain(stepMode + e.dir, 0, STEP_NUM - 1);
  }
  else {
    // Pattern mode: skip STEP_TARGET_TRACK (start from STEP_RECORD)
    int8_t newStepMode = stepMode + e.dir;
    if (newStepMode < STEP_RECORD) {
      newStepMode = STEP_NUM - 1;
    }
    else if (newStepMode >= STEP_NUM) {
      newStepMode = STEP_RECORD;
    }
    stepMode = newStepMode;
  }

  if ((data.isRunning == false) && (stepMode == STEP_RECORD)) {
    drawFlags |= DRAW_STEP_CURSOR;
  }
  queueDrawNote({ stepCursorPosition, stepCursorPosition, {} });
  buttonStep->drawNow();

  if (oldStepMode != stepMode) {
    drawHeader(HEADER_DRAW_ALL);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::setStartOctave(int8_t num) {
  if (num == view.startOctave) return; // No change

  view.startOctave = num;
  view.lowestNote = 24 + view.startOctave * 12;
  view.highestNote = (view.lowestNote + view.numNotes);
  reloadNotes();
  drawAreaIndicator();
  drawFlags |= DRAW_PIANOROLL; // Force piano roll redraw when start octave changes
}

FLASHMEM void UI_LiveSeq_PianoRoll::setNumOctaves(int8_t num) {
  view.numOctaves = num;
  view.numNotes = view.numOctaves * 12;
  view.noteHeight = CONTENT_HEIGHT / float(view.numNotes);
  view.lowestNote = 24 + view.startOctave * 12;
  view.highestNote = (view.lowestNote + view.numNotes);
  reloadNotes();
  drawAreaIndicator();
  drawFlags |= DRAW_PIANOROLL; // Force piano roll redraw when octaves change
}

FLASHMEM void UI_LiveSeq_PianoRoll::setEventTime(LiveSequencer::MidiEvent& e, uint32_t time) {
  // Basic conversion - song pattern movement is handled separately
  uint32_t absoluteBar = time / data.patternLengthMs;
  e.patternNumber = absoluteBar % data.numberOfBars;
  e.patternMs = time % data.patternLengthMs;
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawHeader(uint16_t modeFlags) {
  display.setTextSize(1);
  char s[16] = { "\0" };

  if (modeFlags == HEADER_DRAW_ALL) {
    display.console = true;
    display.fillRect(132, CONTENT_HEIGHT + 4, DISPLAY_WIDTH, HEADER_HEIGHT - 6, HEADER_BG_COLOR);
  }

  switch (mode) {
  case MODE_VIEW:
    if (modeFlags & (1 << VIEW_ZOOM_X)) {
      drawHeaderSetting(GRID.X[3], "ZOOM X", "", (viewMode == VIEW_ZOOM_X), false);
    }
    if (modeFlags & (1 << VIEW_SCROLL_X)) {
      drawHeaderSetting(GRID.X[4], "SCROLL X", "", (viewMode == VIEW_SCROLL_X), false);
    }
    if (data.activeTrack != SONG_MODE_MUTE_TRACK) {
      if (modeFlags & (1 << VIEW_ZOOM_Y)) {
        drawHeaderSetting(GRID.X[3], "", "ZOOM Y ", false, (viewMode == VIEW_ZOOM_Y));
      }
      if (modeFlags & (1 << VIEW_SCROLL_Y)) {
        drawHeaderSetting(GRID.X[4], "", "SCROLL Y", false, (viewMode == VIEW_SCROLL_Y));
      }
    }
    break;

  case MODE_EDIT:
    if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
      // Mute automation header - only show relevant parameters
      if (modeFlags & (1 << EDIT_SELECT_NOTE)) {
        if (selectedMuteEvent > -1 && selectedMuteEvent < static_cast<int16_t>(muteAutomationEvents.size())) {
          sprintf(s, "%03i/%03i", selectedMuteEvent + 1, muteAutomationEvents.size());
        }
        else {
          sprintf(s, "---/%03i", muteAutomationEvents.size());
        }
        drawHeaderSetting(GRID.X[3], "SELECT", s, false, (editMode == EDIT_SELECT_NOTE));
      }
      if (modeFlags & (1 << EDIT_NOTEON)) {
        if (selectedMuteEvent > -1 && selectedMuteEvent < static_cast<int16_t>(muteAutomationEvents.size())) {
          auto& event = muteAutomationEvents[selectedMuteEvent];
          // Use the EXACT same pattern as song mode notes
          uint32_t patternRelativeTime = getPatternRelativeTime(event);
          sprintf(s, "%lu.%04lu",
            patternRelativeTime / data.patternLengthMs,
            patternRelativeTime % data.patternLengthMs);
        }
        else {
          sprintf(s, "-.----");
        }
        drawHeaderSetting(GRID.X[4], "POS", s, false, (editMode == EDIT_NOTEON));
      }
      if (modeFlags & (1 << EDIT_NOTE)) {
        if (selectedMuteEvent > -1 && selectedMuteEvent < static_cast<int16_t>(muteAutomationEvents.size())) {
          auto& event = muteAutomationEvents[selectedMuteEvent];
          sprintf(s, "%s", event.note_in == LiveSequencer::TYPE_MUTE_ON ? "ON " : "OFF");
        }
        else {
          sprintf(s, "---");
        }
        drawHeaderSetting(GRID.X[5], "MUTE", s, false, (editMode == EDIT_NOTE));
      }
    }
    else {
      // Pattern and song notes header
      if (modeFlags & (1 << EDIT_SELECT_NOTE)) {
        // Count only notes in the selected layer
        int16_t layerNoteIndex = 0;
        int16_t totalLayerNotes = 0;

        for (size_t i = 0; i < notePairs.size(); i++) {
          if (notePairs[i].noteOn.layer == selectedLayer) {
            totalLayerNotes++;
            if (selectedNote > -1 && i < static_cast<size_t>(selectedNote)) {
              layerNoteIndex++;
            }
          }
        }

        if (selectedNote > -1) {
          layerNoteIndex++; // Current selection is also counted
          sprintf(s, "%03i/%03i", layerNoteIndex, totalLayerNotes);
        }
        else {
          sprintf(s, "---/%03i", totalLayerNotes);
        }
        drawHeaderSetting(GRID.X[3], "SELECT", s, false, (editMode == EDIT_SELECT_NOTE));
      }

      if (modeFlags & (1 << EDIT_LAYER)) {
        sprintf(s, "%i", selectedLayer + 1);
        drawHeaderSetting(GRID.X[2] + 25, "LAYR", s, false, (editMode == EDIT_LAYER));
      }

      if (modeFlags & (1 << EDIT_NOTEON)) {
        if (selectedNote > -1) {
          if (data.isSongMode && !data.useGridSongMode && editingNote.active) {
            // SONG MODE: Use editing note data during editing
            uint32_t patternRelativeTime = getPatternRelativeTime(editingNote.editingNoteOn);
            sprintf(s, "/ %lu.%04lu",
              patternRelativeTime / data.patternLengthMs,
              patternRelativeTime % data.patternLengthMs);
          }
          else {
            // PATTERN MODE or non-editing song mode: Use notePairs data
            LiveSequencer::NotePair& p = notePairs.at(selectedNote);
            if (data.isSongMode && !data.useGridSongMode) {
              uint32_t patternRelativeTime = getPatternRelativeTime(p.noteOn);
              sprintf(s, "/ %lu.%04lu",
                patternRelativeTime / data.patternLengthMs,
                patternRelativeTime % data.patternLengthMs);
            }
            else {
              sprintf(s, "/ %i.%04i", p.noteOn.patternNumber, p.noteOn.patternMs);
            }
          }
        }
        else {
          sprintf(s, "/ --.----");
        }
        drawHeaderSetting(GRID.X[4], s, "", (editMode == EDIT_NOTEON), false);
      }

      if (modeFlags & (1 << EDIT_NOTEOFF)) {
        if (selectedNote > -1) {
          if (data.isSongMode && !data.useGridSongMode && editingNote.active) {
            // SONG MODE: Use editing note data during editing
            uint32_t patternRelativeTime = getPatternRelativeTime(editingNote.editingNoteOff);
            sprintf(s, "\\ %lu.%04lu",
              patternRelativeTime / data.patternLengthMs,
              patternRelativeTime % data.patternLengthMs);
          }
          else {
            // PATTERN MODE or non-editing song mode: Use notePairs data
            LiveSequencer::NotePair& p = notePairs.at(selectedNote);
            if (data.isSongMode && !data.useGridSongMode) {
              uint32_t patternRelativeTime = getPatternRelativeTime(p.noteOff);
              sprintf(s, "\\ %lu.%04lu",
                patternRelativeTime / data.patternLengthMs,
                patternRelativeTime % data.patternLengthMs);
            }
            else {
              sprintf(s, "\\ %i.%04i", p.noteOff.patternNumber, p.noteOff.patternMs);
            }
          }
        }
        else {
          sprintf(s, "\\ --.----");
        }
        drawHeaderSetting(GRID.X[4], "", s, false, (editMode == EDIT_NOTEOFF));
      }

      if (modeFlags & (1 << EDIT_VELOCITY)) {
        if (selectedNote > -1) {
          if (data.isSongMode && !data.useGridSongMode && editingNote.active) {
            // SONG MODE: Use editing note data during editing
            sprintf(s, "%03i", editingNote.editingNoteOn.note_in_velocity);
          }
          else {
            // PATTERN MODE or non-editing song mode: Use notePairs data
            LiveSequencer::NotePair& p = notePairs.at(selectedNote);
            sprintf(s, "%03i", p.noteOn.note_in_velocity);
          }
        }
        else {
          sprintf(s, "---");
        }
        drawHeaderSetting(GRID.X[5], "VEL", s, false, (editMode == EDIT_VELOCITY));
      }

      if (modeFlags & (1 << EDIT_NOTE)) {
        if (selectedNote > -1) {
          if (data.isSongMode && !data.useGridSongMode && editingNote.active) {
            // SONG MODE: Use editing note data during editing
            std::string noteString = getNoteString(editingNote.editingNoteOn.note_in);
            sprintf(s, "%-3s", noteString.c_str());
          }
          else {
            // PATTERN MODE or non-editing song mode: Use notePairs data
            LiveSequencer::NotePair& p = notePairs.at(selectedNote);
            std::string noteString = getNoteString(p.noteOn.note_in);
            sprintf(s, "%-3s", noteString.c_str());
          }
        }
        else {
          sprintf(s, "---");
        }
        drawHeaderSetting(GRID.X[5] + 4 * CHAR_width_small, "NOTE", s, false, (editMode == EDIT_NOTE));
      }
    }

    break;

  case MODE_STEP:
    if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
      break;
    }
    // Show target track field ONLY in song mode
    if (data.isSongMode && !data.useGridSongMode && data.activeTrack == SONG_MODE_NOTES_TRACK) {
      if (modeFlags & (1 << STEP_TARGET_TRACK)) {
        sprintf(s, "TRACK %02i", data.stepRecordTargetTrack + 1); // Display as 1-12
        drawHeaderSetting(GRID.X[2], "PLAY BY", s, false, (stepMode == STEP_TARGET_TRACK));
      }
    }

    // Always show all step modes, highlight the active one (like view/edit modes)
    if (modeFlags & (1 << STEP_RECORD)) {
      const uint8_t pattern = (stepCursorPositionIndex / stepRecordSteps);
      sprintf(s, "P%i %02i/%02i", pattern, stepCursorPositionIndex % stepRecordSteps, stepRecordSteps);
      drawHeaderSetting(GRID.X[3], "REC/ADV", s, false, (stepMode == STEP_RECORD));
    }
    if (modeFlags & (1 << STEP_SIZE)) {
      sprintf(s, "1/%02i", stepRecordSteps);
      drawHeaderSetting(GRID.X[4], "STEPSIZE", s, false, (stepMode == STEP_SIZE));
    }
    if (modeFlags & (1 << STEP_LAYER)) {
      sprintf(s, "%i", stepRecordLayer + 1);
      drawHeaderSetting(GRID.X[5], "TO LAYER", s, false, (stepMode == STEP_LAYER));
    }
    break;
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawHeaderSetting(uint16_t x, const char* topText, const char* bottomText, bool topHighlighted, bool bottomHighlighted) {
  if (topText != 0) {
    setIsHighlighted(topHighlighted);
    display.setCursor(x, CONTENT_HEIGHT + 4);
    display.print(topText);
  }
  if (bottomText != 0) {
    setIsHighlighted(bottomHighlighted);
    display.setCursor(x, CONTENT_HEIGHT + 6 + CHAR_height_small);
    display.print(bottomText);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::setIsHighlighted(bool isHighlighted) {
  const uint16_t textColor = isHighlighted ? (isSettingActivated ? COLOR_SYSTEXT : HEADER_BG_COLOR) : COLOR_SYSTEXT;
  const uint16_t bgColor = isHighlighted ? (isSettingActivated ? RED : COLOR_SYSTEXT) : HEADER_BG_COLOR;
  display.setTextColor(textColor, bgColor);
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawPianoRoll() {
  const uint16_t numWhiteKeys = view.numOctaves * 7;
  const float keyHeight = CONTENT_HEIGHT / float(numWhiteKeys);

  display.console = true;
  display.fillRect(0, 0, ROLL_WIDTH, CONTENT_HEIGHT, COLOR_SYSTEXT);
  display.setTextColor(COLOR_BACKGROUND, COLOR_SYSTEXT);
  display.setTextSize(1);

  // Draw white key separators - these scale with zoom Y
  for (uint8_t i = 1; i < numWhiteKeys; i++) {
    const uint16_t y = round(i * keyHeight);
    display.console = true;
    display.drawLine(0, y, ROLL_WIDTH, y, COLOR_BACKGROUND);
  }

  // Draw black keys - these scale with zoom Y
  for (uint8_t note = view.lowestNote; note < view.highestNote; note++) {
    const uint8_t noteIndex = view.highestNote - note - 1;
    const uint16_t y = ceil(noteIndex * view.noteHeight); // ceil looks best rounded
    switch (note % 12) {
    case 1:  // C#
    case 3:  // D#
    case 6:  // F#
    case 8:  // G#
    case 10: // A#
      display.console = true;
      display.fillRect(0, y, 12, view.noteHeight, COLOR_BACKGROUND);
      break;
    }
  }
}

FLASHMEM std::string UI_LiveSeq_PianoRoll::getNoteString(uint8_t note) {
  std::string result;
  static constexpr char notes[12][3] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  static constexpr char octs[10][2] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
  const uint8_t octave = (note - 24) / 12;
  result.append(notes[note % 12]);
  result.append(octs[octave]);
  return result;
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawVerticalBars(void) {
  display.console = true;

  // Find the combined X range that needs bar updates
  uint16_t minX = DISPLAY_WIDTH;
  uint16_t maxX = 0;
  bool hasRegions = false;

  for (auto& u : drawRegionsX) {
    if (u.notes.empty() || (drawFlags & DRAW_VERTICAL_BARS)) {
      minX = std::min(minX, u.fromX);
      maxX = std::max(maxX, u.toX);
      hasRegions = true;
    }
  }

  if (!hasRegions) return;

  // Clamp to display bounds
  minX = std::max(minX, ROLL_WIDTH);
  maxX = std::min(maxX, (uint16_t)DISPLAY_WIDTH);

  static constexpr uint8_t substeps = 4;
  const float stepWidth = CONTENT_WIDTH / float(view.numBars * substeps);

  // Draw VERTICAL grid lines
  for (uint8_t step = 0; step <= view.numBars * substeps; step++) {
    const uint16_t x = ROLL_WIDTH + round(step * stepWidth);
    if (x >= minX && x <= maxX) {
      uint16_t color = (step % substeps == 0) ? GREY2 : GREY3;
      display.drawLine(x, 0, x, CONTENT_HEIGHT, color);
    }
  }

  // Draw HORIZONTAL grid lines (octave boundaries)
  for (uint8_t note = view.lowestNote; note <= view.highestNote; note++) {
    if (note % 12 == 0) {
      const uint8_t noteIndex = view.highestNote - note;
      const uint16_t y = round(noteIndex * view.noteHeight);
      display.drawLine(minX, y, maxX, y, GREY2);
    }
  }
}

FLASHMEM uint16_t UI_LiveSeq_PianoRoll::getNoteCoord(uint32_t time) const {
  uint32_t viewStartMs, viewEndMs;

  if (data.isSongMode && !data.useGridSongMode) {
    // Song mode: use 1:1 mapping (no 4:1 compression)
    viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
    viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);
  }
  else {
    // Pattern mode
    viewStartMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);
    viewEndMs = viewStartMs + uint32_t(view.numBars) * uint32_t(data.patternLengthMs);
  }

  // Handle times before viewport
  if (time < viewStartMs) {
    return ROLL_WIDTH;
  }

  // Handle times after viewport  
  if (time >= viewEndMs) {  // Changed from > to >= to prevent drawing at DISPLAY_WIDTH
    return DISPLAY_WIDTH - 1;
  }

  // Calculate position within viewport
  float ratio = float(time - viewStartMs) / float(viewEndMs - viewStartMs);
  uint16_t px = ROLL_WIDTH + roundf(ratio * CONTENT_WIDTH);

  // Clamp to display bounds (ensure we don't draw at DISPLAY_WIDTH)
  if (px < ROLL_WIDTH) px = ROLL_WIDTH;
  if (px >= DISPLAY_WIDTH) px = DISPLAY_WIDTH - 1;  // Changed from > to >=

  return px;
}

FLASHMEM void UI_LiveSeq_PianoRoll::onEventsChanged(void) {
  if (isVisible) {
    reloadNotes();

    // Re-locate selected note as it may have changed order (index)
    if (selectedNote > -1 && selectedNote < static_cast<int16_t>(notePairs.size())) {
      for (int16_t i = 0; i < int16_t(notePairs.size()); i++) {
        if (memcmp(&notePairs.at(i).noteOn, &currentNote, sizeof(currentNote)) == 0) {
          selectedNote = i;
          break;
        }
      }
    }
  }
}

FLASHMEM uint32_t UI_LiveSeq_PianoRoll::getRelativeNoteTime(uint32_t time) const {
  if (data.isSongMode && !data.useGridSongMode) {
    // Song mode: return time relative to viewport start
    // DO NOT clamp to viewport end - we need full time for length calculations
    const uint32_t startMs = uint32_t(view.startBar) * uint32_t(data.patternLengthMs);

    if (time < startMs) {
      return 0;
    }

    return time - startMs;
  }
  else {
    // Pattern mode: original logic with clamping
    const int32_t startMs = int32_t(view.startBar) * int32_t(data.patternLengthMs);
    const int32_t endMs = (int32_t(view.startBar) + int32_t(view.numBars)) * int32_t(data.patternLengthMs);

    int32_t timeRel = int32_t(time) - startMs;
    if (timeRel < 0) timeRel = 0;
    if (timeRel > endMs) timeRel = endMs;

    return uint32_t(timeRel);
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawNotes(void) {
  // Always determine active layer for both song mode and pattern mode
  int8_t activeLayer = selectedLayer;

  // Early return if there are no notes to draw
  if (notePairs.empty() && !editingNote.active) {
    return;
  }

  // Draw all regular notes from notePairs (skip invalid ones)
  for (size_t i = 0; i < notePairs.size(); ++i) {
    // Skip invalid notes (the one being edited)
    if (notePairs[i].noteOn.event == midi::InvalidType) {
      continue;
    }

    // Check if this note should be drawn based on current draw regions
    bool shouldDraw = true;
    if (!drawRegionsX.empty()) {
      shouldDraw = false;
      for (const auto& region : drawRegionsX) {
        if (region.notes.empty() || region.notes.count(notePairs[i].noteOn.note_in)) {
          shouldDraw = true;
          break;
        }
      }
    }

    if (shouldDraw) {
      bool isSelected = (selectedNote == static_cast<int16_t>(i));
      bool drawInGrey = (data.activeTrack == SONG_MODE_NOTES_TRACK || data.activeTrack < SONG_MODE_NOTES_TRACK) &&
        (notePairs[i].noteOn.layer != activeLayer);
      drawNote(notePairs[i], isSelected, drawInGrey);
    }
  }

  // Always draw the editing note on top (in red) if active
  // This is drawn separately and is NOT affected by drawRegionsX
  if (editingNote.active) {
    drawEditingNote();
  }
}

FLASHMEM void UI_LiveSeq_PianoRoll::drawNoteAtPosition(LiveSequencer::NotePair note, bool isSelected, uint16_t x, uint16_t w, bool drawInGrey) const {
  // Skip notes outside the visible octave range
  if (note.noteOn.note_in < view.lowestNote || note.noteOn.note_in >= view.highestNote) {
    return;
  }

  // Calculate Y position - use the same calculation as old file
  const uint8_t noteIndex = view.highestNote - note.noteOn.note_in - 1;
  const uint16_t y = round(noteIndex * view.noteHeight);
  const uint8_t h = view.noteHeight;

  // Ensure note is fully within content area
  if (y < 0 || y >= CONTENT_HEIGHT) {
    return;
  }

  // Ensure note doesn't overflow into header area
  if (y + h > CONTENT_HEIGHT) {
    return;
  }

  uint16_t color;

  bool isEditingParameter = (editMode == EDIT_NOTEON || editMode == EDIT_NOTEOFF ||
    editMode == EDIT_VELOCITY || editMode == EDIT_NOTE);

  if (mode == MODE_EDIT && isSelected && isSettingActivated && isEditingParameter) {
    color = RED;
  }
  else if (drawInGrey) {
    color = GREY2; // Draw non-active layer notes in grey
  }
  else if (isSelected) {
    color = COLOR_SYSTEXT;
  }
  else {
    color = layerColors.at(note.noteOn.layer);
  }

  display.console = true;

  if (w > 1) {
    if (note.isMuted) {
      display.drawRect(x, y, w, h, color);
    }
    else {
      display.fillRect(x, y, w, h, color);
    }
  }
  else {
    display.drawLine(x, y, x, y + h - 1, color);
  }
}

FLASHMEM const uint32_t UI_LiveSeq_PianoRoll::getEventTime(LiveSequencer::MidiEvent e) const {
  return e.patternNumber * data.patternLengthMs + e.patternMs;
}

FLASHMEM bool UI_LiveSeq_PianoRoll::setMode(uint8_t newMode) {
  // Prevent entering step mode in mute automation editor
  if (newMode == MODE_STEP && data.activeTrack == SONG_MODE_MUTE_TRACK) {
    return false;
  }
  const uint8_t oldMode = mode;
  const bool modeChanged = oldMode != newMode;
  if (modeChanged) {
    isSettingActivated = false;
    mode = newMode;

    // Redraw the combined instrument/quantization button when mode changes
    if (buttonInstQuant) {
      buttonInstQuant->drawNow();
    }


// Clear undo state when switching modes (except if going to same track/layer)
    if (oldMode == MODE_EDIT || newMode == MODE_EDIT) {
      // Keep undo data when staying in edit mode, but clear when leaving edit mode
      if (oldMode == MODE_EDIT && newMode != MODE_EDIT) {
        clearUndoState();
      }
    }

    // leaving old mode actions
    switch (oldMode) {
    case MODE_VIEW:
      buttonView->drawNow();
      break;

    case MODE_EDIT:
      editMode = EDIT_LAYER;
      buttonEdit->drawNow();
      if (selectedNote > -1) {
        queueDrawNote(notePairs.at(selectedNote));
        drawFlags |= DRAW_NOTES;
        liveSeq.requestSortEvents();
        onEventsChanged();
      }

      if (data.activeTrack == SONG_MODE_MUTE_TRACK && selectedMuteEvent > -1) {
        liveSeq.requestSortEvents();
        reloadNotes();
      }
      break;

    case MODE_STEP:
      stepMode = STEP_RECORD;
      buttonStep->drawNow();
      queueDrawNote({ stepCursorPosition, stepCursorPosition, {} });
      stepCursorPositionIndex = 0;
      stepCursorPosition = ROLL_WIDTH;
      stepCursorPositionPrev = ROLL_WIDTH;
      drawFlags |= DRAW_CONTENT_AREA;
      break;
    }

    // entering new mode actions
    switch (newMode) {
    case MODE_VIEW:
      break;

    case MODE_EDIT:
      if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
        display.console = true;
        display.fillRect(GRID.X[2], 183, 23, 20, COLOR_BACKGROUND);
      }

      if ((selectedNote == -1) && notePairs.size()) {
        int16_t leftmostNote = -1;
        uint16_t leftmostX = DISPLAY_WIDTH;

        for (size_t i = 0; i < notePairs.size(); i++) {
          auto& note = notePairs[i];
          if (note.noteOn.layer != selectedLayer) {
            continue;
          }

          uint32_t timeOn;
          if (data.isSongMode && !data.useGridSongMode) {
            uint16_t absoluteBarOn = getActualBarForNote(note);
            timeOn = absoluteBarOn * data.patternLengthMs + note.noteOn.patternMs;
          }
          else {
            timeOn = getEventTime(note.noteOn);
          }

          const uint16_t x = getNoteCoord(timeOn);
          if (x >= ROLL_WIDTH && x < DISPLAY_WIDTH && x < leftmostX) {
            leftmostNote = i;
            leftmostX = x;
          }
        }

        if (leftmostNote != -1) {
          selectedNote = leftmostNote;
        }
        else {
          for (size_t i = 0; i < notePairs.size(); i++) {
            if (notePairs[i].noteOn.layer == selectedLayer) {
              selectedNote = i;
              break;
            }
          }
        }

        if (selectedNote > -1) {
          queueDrawNote(notePairs.at(selectedNote));
          drawFlags |= DRAW_NOTES;
        }
      }
      break;

    case MODE_STEP:
      stepRecordStepSizeMs = data.patternLengthMs / float(stepRecordSteps);
      stepCursorPositionIndex = 0;
      stepCursorPosition = ROLL_WIDTH;
      drawFlags |= DRAW_STEP_CURSOR;
      break;
    }

    drawHeader(HEADER_DRAW_ALL);
  }
  return modeChanged;
}

FLASHMEM void UI_LiveSeq_PianoRoll::deleteSelectedNote(void) {
  // For mute automation track

  if (data.activeTrack == SONG_MODE_MUTE_TRACK && selectedMuteEvent > -1 &&
    selectedMuteEvent < static_cast<int16_t>(muteAutomationEvents.size())) {

    LiveSequencer::MidiEvent eventToRemove = muteAutomationEvents.at(selectedMuteEvent);

    for (auto& patternEntry : data.songEvents) {
      auto& events = patternEntry.second;
      for (auto it = events.begin(); it != events.end(); ) {
        if (it->track == eventToRemove.track &&
          it->layer == eventToRemove.layer &&
          it->patternNumber == eventToRemove.patternNumber &&
          it->patternMs == eventToRemove.patternMs &&
          it->note_in == eventToRemove.note_in &&
          it->event == eventToRemove.event) {
          it = events.erase(it);
        }
        else {
          ++it;
        }
      }
    }

    muteAutomationEvents.erase(muteAutomationEvents.begin() + selectedMuteEvent);

    if (muteAutomationEvents.empty()) {
      selectedMuteEvent = -1;
    }
    else {
      selectedMuteEvent = std::min(selectedMuteEvent, static_cast<int16_t>(muteAutomationEvents.size() - 1));
    }

    drawFlags |= DRAW_CONTENT_FULL | DRAW_MUTE_AUTOMATION;
    drawHeader(HEADER_DRAW_ALL);
    return;
  }

  // For note deletion
  if (selectedNote < 0 || selectedNote >= static_cast<int16_t>(notePairs.size())) {
    return;
  }

  // Get the event info
  //LiveSequencer::MidiEvent noteOnInfo = notePairs.at(selectedNote).noteOn;
  //LiveSequencer::MidiEvent noteOffInfo = notePairs.at(selectedNote).noteOff;

  if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
    // Song mode - mark as invalid (soft delete)
    // The references in notePairs point to actual events in data.songEvents
    notePairs.at(selectedNote).noteOn.event = midi::InvalidType;
    notePairs.at(selectedNote).noteOff.event = midi::InvalidType;
  }
  else {
    // Pattern mode - mark as invalid (soft delete)
    // This works because notePairs contains references to actual events in data.eventsList
    // When reloadNotes() is called, getNotePairsFromTrack() will skip InvalidType events
    notePairs.at(selectedNote).noteOn.event = midi::InvalidType;
    notePairs.at(selectedNote).noteOff.event = midi::InvalidType;
  }

  // Store previous selection
  int16_t prevSelectedNote = selectedNote;

  // Reload notes
  reloadNotes();

  // Update selection
  if (notePairs.empty()) {
    selectedNote = -1;
  }
  else {
    selectedNote = std::min(prevSelectedNote, static_cast<int16_t>(notePairs.size() - 1));
  }

  // Reset setting activation
  isSettingActivated = false;
  buttonEdit->drawNow();

  drawFlags |= DRAW_CONTENT_FULL | DRAW_PIANOROLL;
  drawHeader(HEADER_DRAW_ALL);
}

FLASHMEM uint8_t UI_LiveSeq_PianoRoll::getMaxTrackForCurrentMode() const {
  return MAX_TRACKS - 1; // 0-13 for all tracks
}

FLASHMEM void UI_LiveSeq_PianoRoll::initGUI(void) {
  const uint16_t y = CONTENT_HEIGHT + HEADER_HEIGHT + LINE_HEIGHT;

buttonPlay = new TouchButton(GRID.X[0], y,
    [this](auto* b) {
      // Check if we have undo data available
      if (quantizationUndoState.hasUndoData && !data.isRunning && 
          data.activeTrack != SONG_MODE_MUTE_TRACK) {
        // Show UNDO text when undo is available (only for regular/song tracks)
        b->draw("UNDO", "QUANT", TouchButton::BUTTON_RED);
      } else {
        // Normal play/stop button
        const TouchButton::Color playColor = data.isRunning ? TouchButton::BUTTON_RED : TouchButton::BUTTON_ACTIVE;
        b->draw((data.isRunning ? "STOP" : "START"), "", playColor);
      }
    },
    [this](auto* b) {
      // Check if we should perform undo (only for regular/song tracks)
      if (quantizationUndoState.hasUndoData && !data.isRunning && 
          data.activeTrack != SONG_MODE_MUTE_TRACK) {
        // Apply quantization undo
        applyQuantizationUndo();
        return;
      }
      
      // Normal play/stop functionality
      const bool isRunning = data.isRunning;
      if (isRunning) {
        liveSeq.stop();
      }
      else {
        liveSeq.start();
      }
      if (mode == MODE_STEP && stepMode == STEP_RECORD) {
        drawFlags |= isRunning ? DRAW_STEP_CURSOR : DRAW_CONTENT_AREA;
        ui_pianoroll->queueDrawNote({ stepCursorPosition, stepCursorPosition, {} });
      }
      b->drawNow();
    });
  buttons.push_back(buttonPlay);

  // Track button
  buttons.push_back(new TouchButton(GRID.X[1], y,
    [this](auto* b) {
      char temp_char[4];
      if (data.activeTrack >= SONG_MODE_NOTES_TRACK) {
        if (data.activeTrack == SONG_MODE_NOTES_TRACK) {
          b->draw("SONG", "NOTES", TouchButton::BUTTON_ACTIVE);
        }
        else {
          b->draw("SONG", "MUTES", TouchButton::BUTTON_ACTIVE);
        }
      }
      else {
        itoa(data.activeTrack + 1, temp_char, 10);
        b->draw("TRACK", temp_char, TouchButton::BUTTON_ACTIVE);
      }
    }, [this](auto* b) {
      uint8_t newTrack = data.activeTrack + 1;
      if (newTrack >= MAX_TRACKS) {
        newTrack = 0;
      }

      // CANCEL ANY ACTIVE EDITING WHEN SWITCHING TRACKS
      if (editingNote.active) {
        cancelEditingNote();
      }
      
      // CLEAR UNDO STATE WHEN SWITCHING TRACKS
      clearUndoState();

      // ALWAYS SET TO VIEW MODE WHEN SWITCHING TRACKS
      setMode(MODE_VIEW);

      isSettingActivated = false;
      selectedLayer = 0; // Reset to layer 0 when switching tracks

      display.console = true;
      display.fillRect(ROLL_WIDTH, 0, CONTENT_WIDTH, CONTENT_HEIGHT, COLOR_BACKGROUND);

      viewMode = VIEW_ZOOM_X;
      editMode = EDIT_SELECT_NOTE;

      data.activeTrack = newTrack;

      // Redraw the instrument/quant button if we're in view mode
      if ((mode == MODE_VIEW || data.activeTrack == SONG_MODE_MUTE_TRACK) && buttonInstQuant) {
        buttonInstQuant->drawNow();
      }

      if (newTrack >= SONG_MODE_NOTES_TRACK) {
        //data.isSongMode = true;
        uint16_t maxBars = getMaxBarsForCurrentMode();
        view.startBar = 0;
        view.numBars = maxBars;
        view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);
      }
      else {
        //data.isSongMode = false;
        view.startBar = 0;
        // ALWAYS DEFAULT TO 4 BARS FOR PATTERN TRACKS
        view.numBars = 4;
        view.msToPix = CONTENT_WIDTH / float(view.numBars * data.patternLengthMs);
      }

      view.startOctave = 1;
      view.numOctaves = 5;
      view.numNotes = view.numOctaves * 12;
      view.noteHeight = CONTENT_HEIGHT / float(view.numNotes);
      view.lowestNote = 24 + view.startOctave * 12;
      view.highestNote = (view.lowestNote + view.numNotes);

      if (mode == MODE_EDIT) {
        isSettingActivated = false;
        editMode = EDIT_LAYER;
        selectedMuteEvent = -1;
      }

      notePairs.clear();
      muteAutomationEvents.clear();
      selectedNote = -1;
      selectedMuteEvent = -1;

      ui_pianoroll->reloadNotes(true);

      if (data.activeTrack == SONG_MODE_MUTE_TRACK && muteAutomationEvents.size() > 0) {
        selectedMuteEvent = 0;
      }
      else if (notePairs.size() > 0) {
        selectedNote = 0;
      }

      drawFlags |= DRAW_CONTENT_FULL;
      if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
        drawFlags |= DRAW_MUTE_AUTOMATION;
      }
      else {
        drawFlags |= DRAW_PIANOROLL;
      }

      buttonEdit->drawNow();
      buttonStep->drawNow();
      ui_pianoroll->drawHeader(HEADER_DRAW_ALL);
      ui_pianoroll->drawLayersIndicator();
      ui_pianoroll->drawAreaIndicator();
      liveSeq.setActiveTrack(data.activeTrack);
      b->drawNow();
      }));

  // Combined Instrument/Quantization button at GRID.X[2]
  buttonInstQuant = new TouchButton(GRID.X[2], y,
    [this](auto* b) {
      // This draw handler will only be called when we explicitly call drawNow()
      if (mode == MODE_VIEW) {
        // Draw instrument button in VIEW mode
        if (data.activeTrack == SONG_MODE_MUTE_TRACK || data.activeTrack == SONG_MODE_NOTES_TRACK) {
          b->draw("", "", TouchButton::BUTTON_INACTIVE);
        }
        else {
          const uint8_t device = data.trackSettings[data.activeTrack].device;
          const uint8_t instrument = data.trackSettings[data.activeTrack].instrument;
          char name[10];
          char sub[10];
          liveSeq.getInstrumentName(device, instrument, name, sub);
          b->draw(name, sub, TouchButton::BUTTON_ACTIVE);
        }
      }
      else {
        // Draw quantization button in EDIT/STEP modes
        if (data.activeTrack != SONG_MODE_MUTE_TRACK)
        {
          const char* quantText = "";
          const char* subText = "QUANT";

          switch (quantizeLevel) {
          case QUANTIZE_OFF: quantText = "OFF"; break;
          case QUANTIZE_64: quantText = "1/64"; break;
          case QUANTIZE_32: quantText = "1/32"; break;
          case QUANTIZE_16T: quantText = "1/16T"; break;
          case QUANTIZE_16: quantText = "1/16"; break;
          case QUANTIZE_8T: quantText = "1/8T"; break;
          case QUANTIZE_8: quantText = "1/8"; break;
          case QUANTIZE_4: quantText = "1/4"; break;
          }
          b->draw(quantText, subText, TouchButton::BUTTON_ACTIVE);
        }
        else
          b->draw(" ", " ", TouchButton::BUTTON_INACTIVE);
      }
    },
    [this](auto* b) { // clickedHandler (short press)
      if (mode == MODE_VIEW) {
        // Instrument button click - only in VIEW mode
        if (data.activeTrack != SONG_MODE_MUTE_TRACK && data.activeTrack != SONG_MODE_NOTES_TRACK) {
          if (data.tracks[data.activeTrack].screenSetupFn != nullptr) {
            SetupFn f = (SetupFn)data.tracks[data.activeTrack].screenSetupFn;
            f(0);
          }
          UI_LiveSequencer::openScreen(data.tracks[data.activeTrack].screen);
        }
      }
      else {
        // Quantization button click - only in EDIT/STEP modes
        uint8_t oldLevel = quantizeLevel;
        quantizeLevel = (quantizeLevel + 1) % QUANTIZE_NUM;
        b->drawNow(); // Explicitly redraw after change
        
        // Clear undo state when quantization level changes (if we have undo data)
        if (quantizationUndoState.hasUndoData && oldLevel != quantizeLevel) {
          clearUndoState();
          if (buttonPlay) {
            buttonPlay->drawNow();
          }
        }

        if (mode == MODE_EDIT && (selectedNote > -1 || selectedMuteEvent > -1)) {
          drawHeader(HEADER_DRAW_ALL);
        }
      }
    },
    [this](auto* b) { // ADDED: Long press handler to quantize all notes
      quantizeAllNotesInCurrentTrack();
    });
  buttons.push_back(buttonInstQuant);

  buttonView = new TouchButton(GRID.X[3], y,
    [this](auto* b) {
      b->draw("VIEW", "MODE", (mode == MODE_VIEW) ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_ACTIVE);
    }, [this](auto* b) {
      ui_pianoroll->setMode(MODE_VIEW);
      b->drawNow();
      });
  buttons.push_back(buttonView);

  buttonEdit = new TouchButton(GRID.X[4], y,
    [this](auto* b) {
      std::string txt = "EDIT";
      std::string sub = "MODE";
      TouchButton::Color color = (mode == MODE_EDIT) ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_ACTIVE;

      if (mode == MODE_EDIT && isSettingActivated) {
        if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
          txt = "MUTE";
          sub = "DELETE";
          color = TouchButton::BUTTON_RED;
        }
        else if (editMode == EDIT_SELECT_NOTE) {
          txt = "NOTE";
          sub = "DELETE";
          color = TouchButton::BUTTON_RED;
        }
      }

      b->draw(txt.c_str(), sub.c_str(), color);
    },
    [this](auto* b) {
      ui_pianoroll->setMode(MODE_EDIT);

      if (isSettingActivated) {
        if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
          ui_pianoroll->deleteSelectedNote();
        }
        else if (editMode == EDIT_SELECT_NOTE) {
          ui_pianoroll->deleteSelectedNote();
        }
      }
      b->drawNow();
    });

  buttons.push_back(buttonEdit);

  buttonStep = new TouchButton(GRID.X[5], y,
    [this](auto* b) {
      // Hide step recorder in mute automation mode
      if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
        b->draw(" ", " ", TouchButton::BUTTON_INACTIVE);
        return;
      }

      TouchButton::Color color = (mode == MODE_STEP) ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_ACTIVE;
      if (data.isRunning) {
        color = TouchButton::BUTTON_NORMAL;
      }
      b->draw("STEP", "RECORD", color);
    },
    [this](auto* b) {
      // Disable step recorder in mute automation mode
      if (data.activeTrack == SONG_MODE_MUTE_TRACK) {
        return;
      }

      if (data.isRunning == false) {
        const bool modeChanged = ui_pianoroll->setMode(MODE_STEP);
        if ((modeChanged == false) && (stepMode == STEP_RECORD)) {
          ui_pianoroll->step(+1);
        }
        b->drawNow();
      }
    });
  buttons.push_back(buttonStep);
}

FLASHMEM bool UI_LiveSeq_PianoRoll::step(int8_t diff) {
  const bool stepCursorChanged = hasConstrainedChanged(stepCursorPositionIndex, diff, uint16_t(0), uint16_t(view.numBars * stepRecordSteps));

  if (stepCursorChanged) {
    if (stepCursorPositionIndex == view.numBars * stepRecordSteps) {
      stepCursorPositionIndex = 0;
    }

    // Store previous position for clearing
    stepCursorPositionPrev = stepCursorPosition;

    // Calculate new position
    stepCursorPosition = ROLL_WIDTH + round(stepCursorPositionIndex * stepRecordStepSizeMs * view.msToPix);

    // Queue redraw for BOTH old and new cursor positions
    queueDrawNote({ stepCursorPositionPrev, stepCursorPositionPrev, {} });
    queueDrawNote({ stepCursorPosition, stepCursorPosition, {} });

    drawFlags |= DRAW_STEP_CURSOR | DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;
    drawHeader(1 << STEP_RECORD);
  }
  return stepCursorChanged;
}

FLASHMEM void UI_LiveSeq_PianoRoll::quantizeSelectedNote(void) {
  if (quantizeLevel == QUANTIZE_OFF) return;

  // Calculate grid size based on quantization level
  uint32_t gridSize = 0;
  switch (quantizeLevel) {
  case QUANTIZE_64: gridSize = data.patternLengthMs / 64; break;
  case QUANTIZE_32: gridSize = data.patternLengthMs / 32; break;
  case QUANTIZE_16: gridSize = data.patternLengthMs / 16; break;
  case QUANTIZE_8: gridSize = data.patternLengthMs / 8; break;
  case QUANTIZE_8T: gridSize = data.patternLengthMs / 12; break;   // 8th triplets
  case QUANTIZE_16T: gridSize = data.patternLengthMs / 24; break; // 16th triplets
  default: return;
  }

  // Calculate minimum note length (1/16 note)
  uint32_t minNoteLength = data.patternLengthMs / 16;

  // Handle note quantization
  if (selectedNote > -1 && selectedNote < static_cast<int16_t>(notePairs.size())) {
    LiveSequencer::NotePair& note = notePairs.at(selectedNote);

    // Store original positions
    uint32_t oldTimeOn = getEventTime(note.noteOn);
    uint32_t oldTimeOff = getEventTime(note.noteOff);
    uint32_t noteLength = (oldTimeOff > oldTimeOn) ? (oldTimeOff - oldTimeOn) : minNoteLength;

    // Ensure minimum note length
    if (noteLength < minNoteLength) {
      noteLength = minNoteLength;
    }

    // Quantize note on time only
    uint32_t quantizedTimeOn = (oldTimeOn / gridSize) * gridSize;
    uint32_t remainder = oldTimeOn % gridSize;

    if (remainder > gridSize / 2) {
      quantizedTimeOn += gridSize;
    }

    // Calculate new note off time = quantized start + original length
    uint32_t quantizedTimeOff = quantizedTimeOn + noteLength;

    // Ensure note-off is at least gridSize after note-on
    if (quantizedTimeOff <= quantizedTimeOn + gridSize) {
      quantizedTimeOff = quantizedTimeOn + gridSize;
    }

    // Update the note times
    setEventTime(note.noteOn, quantizedTimeOn);
    setEventTime(note.noteOff, quantizedTimeOff);

    // Queue redraw
    queueDrawNote(note);
    drawFlags |= DRAW_BACKGROUND | DRAW_VERTICAL_BARS | DRAW_NOTES;

    // Update header
    drawHeader((1 << EDIT_NOTEON) | (1 << EDIT_NOTEOFF));
  }

  // Handle mute automation event quantization (unchanged)
  if (selectedMuteEvent > -1 && selectedMuteEvent < static_cast<int16_t>(muteAutomationEvents.size())) {
    LiveSequencer::MidiEvent& event = muteAutomationEvents.at(selectedMuteEvent);

    // Find canonical event in data.songEvents
    LiveSequencer::MidiEvent* realEvent = nullptr;
    uint16_t songPatternIndex = 0;
    for (auto& patternEntry : data.songEvents) {
      for (auto& ev : patternEntry.second) {
        if (ev.track == event.track &&
          ev.layer == event.layer &&
          ev.note_in == event.note_in &&
          ev.event == event.event &&
          ev.patternMs == event.patternMs &&
          ev.note_in_velocity == event.note_in_velocity) {
          realEvent = &ev;
          songPatternIndex = patternEntry.first;
          break;
        }
      }
      if (realEvent) break;
    }

    if (!realEvent) {
      songPatternIndex = getSongPatternForEvent(event);
    }

    // Get current time and quantize
    uint32_t currentTime = getPatternRelativeTime(event);
    uint32_t quantizedTime = (currentTime / gridSize) * gridSize;

    if (quantizedTime != currentTime) {
      // Update time
      setPatternRelativeTime(event, quantizedTime, songPatternIndex);

      // Update canonical event if found
      if (realEvent) {
        realEvent->patternNumber = event.patternNumber;
        realEvent->patternMs = event.patternMs;
      }
      else {
        updateSongEvent(event);
      }
    }
  }
}
