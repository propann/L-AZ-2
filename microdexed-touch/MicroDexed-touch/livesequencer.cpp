#include "livesequencer.h"
#include "config.h"
#include "sequencer.h"
#include <algorithm>
#include <cstring>
#include "TeensyTimerTool.h"
#include "ui_livesequencer.h"
#include "ui_liveseq_pianoroll.h"
#include <map>


#ifdef GRANULAR
#include "granular.h"
extern granular_params_t granular_params;
#endif

extern sequencer_t seq;
extern uint8_t drum_midi_channel;
extern uint8_t slices_midi_channel;
extern config_t configuration;
extern microsynth_t microsynth[NUM_MICROSYNTH];
extern multisample_s msp[NUM_MULTISAMPLES];
extern braids_t braids_osc;
extern uint8_t microsynth_selected_instance;
extern uint8_t selected_instance_id; // dexed
bool openendFromLiveSequencer;

extern void handleNoteOnInput(byte, byte, byte, byte);
extern void handleNoteOn(byte, byte, byte, byte);
extern void handleNoteOff(byte, byte, byte, byte);
extern void handleAfterTouch(byte, byte);
extern void midiPitchBend(int pitch, uint8_t channel, uint8_t device);
extern int toPitchBend(uint8_t lsb, uint8_t msb);
extern void midiControlChange(uint8_t control, uint8_t value, uint8_t channel, uint8_t device);
extern void handleStart();
extern void handleStop();

using namespace TeensyTimerTool;

PeriodicTimer tickTimer(TMR1);   // only 16bit needed
OneShotTimer arpTimer(TCK);      // one tick timer of 20
OneShotTimer liveTimer(TCK);     // one tick timer of 20
PeriodicTimer countInTimer(TCK); // one tick timer of 20

UI_LiveSequencer* ui_liveSeq;
UI_LiveSeq_PianoRoll* ui_pianoRoll;

FLASHMEM LiveSequencer::LiveSequencer() {
  ui_liveSeq = new UI_LiveSequencer(*this, data);
  ui_pianoRoll = new UI_LiveSeq_PianoRoll(*this, data);

  // Initialize grid song mode data with PSRAM allocation
  data.useGridSongMode = false;
  data.currentGridStep = 0;
  data.selectedGridStep = 0;
  data.selectedGridTrack = 0;
  data.gridPlaybackStartStep = 0;
  data.gridPlaybackEndStep = LiveSequencer::LiveSeqData::GRIDSONGLEN - 1;
  data.gridLoopEnabled = true;

  // Allocate gridSongSteps in PSRAM
#ifdef PSRAM
  data.gridSongSteps = static_cast<uint8_t*>(extmem_malloc(data.GRIDSONGLEN * LIVESEQUENCER_NUM_TRACKS * LIVESEQUENCER_NUM_LAYERS));
  if (data.gridSongSteps) {
    memset(data.gridSongSteps, 0, data.GRIDSONGLEN * LIVESEQUENCER_NUM_TRACKS * LIVESEQUENCER_NUM_LAYERS);
  }
  else {
    // Fallback to internal RAM if PSRAM allocation fails
    data.gridSongSteps = new uint8_t[data.GRIDSONGLEN * LIVESEQUENCER_NUM_TRACKS * LIVESEQUENCER_NUM_LAYERS]();
  }
#else
  data.gridSongSteps = new uint8_t[data.GRIDSONGLEN * LIVESEQUENCER_NUM_TRACKS * LIVESEQUENCER_NUM_LAYERS]();
#endif

  updateTrackChannels(true);
  data.processMidiIn = false;
}


// Add destructor to free memory
// FLASHMEM LiveSequencer::~LiveSequencer() {
//     if (data.gridSongSteps) {
// #ifdef PSRAM
//         extmem_free(data.gridSongSteps);
// #else
//         delete[] data.gridSongSteps;
// #endif
//         data.gridSongSteps = nullptr;
//     }
// }


FLASHMEM void UI_func_livesequencer(uint8_t param) {
  ui_liveSeq->processLCDM();
}

FLASHMEM void handle_touchscreen_live_sequencer(void) {
  ui_liveSeq->handleTouchscreen();
}

FLASHMEM LiveSequencer::LiveSeqData* LiveSequencer::getData(void) {
  return &data;
}

FLASHMEM const std::string LiveSequencer::getEventName(midi::MidiType event) {
  switch (event) {
  case midi::NoteOn:
    return "NoteOn";
  case midi::NoteOff:
    return "NoteOff";
  case midi::ControlChange:
    return "Automation";
  case midi::PitchBend:
    return "PitchBend";
  case midi::InvalidType:
    return "Invalid";
  default:
    return "None";
  }
}

FLASHMEM const std::string LiveSequencer::getEventSource(EventSource source) {
  switch (source) {
  case EVENT_PATTERN:
    return "PATT";
  case EVENT_SONG:
    return "SONG";
  default:
    return "NONE";
  }
}

FLASHMEM void LiveSequencer::start(void) {
  DBG_LOG(printf("start liveseq\n"));
  handleStart();
}

FLASHMEM void LiveSequencer::stop(void) {
  DBG_LOG(printf("stop liveseq\n"));
  handleStop();
}

FLASHMEM void LiveSequencer::onStopped(void) {
  data.isStarting = false;
  data.isRunning = false;
  DBG_LOG(printf("onStopped\n"));

  if (data.isSongMode) {
    onSongStopped();
  }

  addPendingNotes();
  playIterator = data.eventsList.end();
  allNotesOff();
  resetControllers();
  ui_liveSeq->onStopped();
  activeArpKeys.clear();
  data.arpSettings.arpNotes.clear();
}

FLASHMEM void LiveSequencer::onStarted(void) {
  data.isStarting = true;
  data.recordedToSong = false;
  data.eventsList.erase(std::remove_if(data.eventsList.begin(), data.eventsList.end(), [](MidiEvent& e) { return e.source == EventSource::EVENT_SONG; }), data.eventsList.end());
  playIterator = data.eventsList.end();
}

FLASHMEM void LiveSequencer::allNotesOff(void) {
  for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
    allTrackNotesOff(track);
  }
}

FLASHMEM void LiveSequencer::allTrackNotesOff(const uint8_t track) {
  for (uint8_t layer = 0; layer < data.trackSettings[track].layerCount; layer++) {
    allLayerNotesOff(track, layer);
  }
}

FLASHMEM void LiveSequencer::resetTrackControllers(uint8_t track) {
  const uint8_t device = data.trackSettings[track].device;
  const midi::Channel channel = data.tracks[track].channel;

  //  Reset pitch bend to center
  midiPitchBend(0, channel, device);

  // Collect unique CCs for the current track
  std::unordered_set<uint8_t> uniqueCCs;
  for (const auto& event : data.eventsList) {
    if (event.track == track && event.event == midi::ControlChange) {
      // Note: maybe we should also check if event.source == EVENT_PATTERN
      uniqueCCs.insert(event.note_in);
    }
  }

  // Reset collected CCs to default value. In future add default values for specific CCs like #7 in sys config.
  for (const auto& cc : uniqueCCs) {
    // WARNING: this may affect internal automation. Needs more testing!
    midiControlChange(cc, 0, channel, device);
  }

  DBG_LOG(printf("Track %i: Reset pitch bend and control changes\n", track));
}

FLASHMEM void LiveSequencer::resetControllers() {
  for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
    resetTrackControllers(track);
  }
}

FLASHMEM uint8_t LiveSequencer::mapQuantizeDenom(uint8_t denom) {
  switch (denom) {
    case 1:  return 0;   // NONE
    case 4:  return 4;   // 1/4
    case 8:  return 8;   // 1/8
    case 12: return 12;  // 1/8T
    case 16: return 16;  // 1/16
    case 24: return 24;  // 1/16T
    case 32: return 32;  // 1/32
    case 64: return 64;  // 1/64
    default: return 0;   // fallback: no quantize
  }
}

FLASHMEM void LiveSequencer::allLayerNotesOff(const uint8_t track, const uint8_t layer) {
  for (auto note : data.tracks[track].activeNotes[layer]) {
    handleNoteOff(data.tracks[track].channel, note, 0, data.trackSettings[track].device);
  }
  data.tracks[track].activeNotes[layer].clear();
}

FLASHMEM void LiveSequencer::printEvent(int i, MidiEvent e) {
  DBG_LOG(printf("[%i]: %i:%04i {%s} (%i):%i, %s, %i, %i\n", i, e.patternNumber, e.patternMs, getEventSource(e.source).c_str(), e.track, e.layer, getEventName(e.event).c_str(), e.note_in, e.note_in_velocity));
}

FLASHMEM bool LiveSequencer::timeQuantization(MidiEvent& e, uint8_t denom) {
  bool overflow = false; // overflow if event rounded to start of next pattern

  if (denom > 1) {
    const uint16_t quantizeMs = data.patternLengthMs / denom;
    
    const uint16_t halfStep = quantizeMs / 2;
    uint8_t resultNumber = e.patternNumber;
    uint16_t resultMs = e.patternMs;
    resultMs = ((e.patternMs + halfStep) / quantizeMs) * quantizeMs;
    
    if (resultMs == data.patternLengthMs) {
      resultMs = 0;
      if (++resultNumber == data.numberOfBars) {
        resultNumber = 0;
        overflow = true;
      }
    }
    e.patternNumber = resultNumber;
    e.patternMs = resultMs;
  }
  return overflow;
}

FLASHMEM void LiveSequencer::printEvents() {
  DBG_LOG(printf("--- %i pattern events (%i bytes with one be %i bytes)\n", data.eventsList.size(), data.eventsList.size() * sizeof(MidiEvent), sizeof(MidiEvent)));
  uint i = 0;
  for (auto& e : data.eventsList) {
    printEvent(i++, e);
  }
  DBG_LOG(printf("--- song events on %i layers until pattern %i\n", data.songLayerCount, data.lastSongEventPattern));
  for (int i = 0; i <= data.lastSongEventPattern; i++) {
    for (auto& e : data.songEvents[i]) {
      printEvent(i++, e);
    }
  }
}

FLASHMEM void LiveSequencer::onArpSourceChanged(void) {
  // for now the best way to avoid pending notes / arp keys 
  if (data.isRunning && data.arpSettings.enabled) {
    stop();
    start();
  }
}

FLASHMEM void LiveSequencer::refreshSongLength(void) {
  uint8_t lastSongPattern = 0;
  for (auto& e : data.songEvents) {
    // remove all invalidated song events and repopulate song length
    e.second.erase(std::remove_if(e.second.begin(), e.second.end(), [](MidiEvent& e) { return e.event == midi::InvalidType; }), e.second.end());

    if (e.second.size()) {
      if (e.first > lastSongPattern) {
        lastSongPattern = e.first;
      }
    }
  }
  data.lastSongEventPattern = lastSongPattern;
  DBG_LOG(printf("last song pattern: %i\n", lastSongPattern));
}

FLASHMEM void LiveSequencer::onSongStopped(void) {
  // Reset grid step when song stops
  if (data.useGridSongMode) {
    data.currentGridStep = data.gridPlaybackStartStep;
    // data.redraw_grid_header = true;
    ui_liveSeq->requestGridEditorRedraw();
  }
  else
    if (data.recordedToSong) {
      data.songLayerCount++;
      ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawSongLayers);
      data.recordedToSong = false;
    }

  refreshSongLength();
  applySongStartLayerMutes();
}

FLASHMEM void LiveSequencer::startCountIn(void) {
  handleNoteOn(drum_midi_channel, 82, 127, DEVICE_INTERNAL); // casta
  data.remainingCountIns = 4;
  const int intervalMS = data.patternLengthMs / 4;

  countInTimer.begin([this, intervalMS] {
    if (data.remainingCountIns > 1) {
      handleNoteOn(drum_midi_channel, 73, 127, DEVICE_INTERNAL); // rim
      ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawTopButtons);
    }
    else {
      countInTimer.stop();
      handleStart();
    }
    data.remainingCountIns--;
    }, intervalMS * 1000);
}

FLASHMEM void LiveSequencer::applySongStartLayerMutes(void) {
  if (data.songLayerCount > 0) {
    for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
      data.tracks[track].layerMutes = data.trackSettings[track].songStartLayerMutes;
    }
    ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawLayerButtons);
  }
}

// FLASHMEM void LiveSequencer::handleMidiEvent(uint8_t inChannel, midi::MidiType event, uint8_t note, uint8_t velocity) {
//   if (data.processMidiIn) {
//     DBG_LOG(printf("got midi note %i\n", note));
//     if (data.instrumentChannels.count(inChannel) == 0) {
//       ui_pianoRoll->handleKeyChanged(note, event, velocity);
//       // velocity adjustment for keyboard events (live playing or recording)
//       const uint8_t velocitySetting = data.trackSettings[data.activeTrack].velocityLevel;
//       const uint8_t velocityActive = (velocitySetting == 0) ? velocity : velocitySetting * 1.27f; // 100% * 1.27 = 127

//       if ((data.isRecording && data.isRunning) || data.remainingCountIns > 0) {
//         const EventSource source = data.isSongMode ? EVENT_SONG : EVENT_PATTERN;
//         MidiEvent newEvent = { source, uint16_t(data.patternTimer), data.currentPattern, data.activeTrack, data.trackSettings[data.activeTrack].layerCount, event, note, velocityActive };

//         if (data.isSongMode) {
//           if (data.songLayerCount < LIVESEQUENCER_NUM_LAYERS) {
//             // in song mode, simply add event, no rounding and checking needed
//             newEvent.layer = data.songLayerCount;
//             uint8_t patternCount = data.songPatternCount;
//             if (newEvent.event == midi::NoteOn) {
//               if (timeQuantization(newEvent, data.trackSettings[data.activeTrack].quantizeDenom)) {
//                 patternCount++; // event rounded up to start of next song pattern
//               }
//             }
//             data.recordedToSong = true;
//             data.songEvents[patternCount].emplace_back(newEvent);
//           }
//         }
//         else {
//           if (data.trackSettings[data.activeTrack].layerCount < LIVESEQUENCER_NUM_LAYERS) {
//             switch (newEvent.event) {
//             default:
//               // ignore all other types
//               break;

//               // TODO: record aftertouch

//             case midi::ControlChange:
//               // Record MIDI CC
//               newEvent.note_in = note;              // Controller number
//               newEvent.note_in_velocity = velocity; // Controller value
//               data.pendingEvents.emplace_back(newEvent);
//               break;

//             case midi::PitchBend:
//               // Record Pitch Bend
//               newEvent.note_in = note;              // LSB
//               newEvent.note_in_velocity = velocity; // MSB
//               data.pendingEvents.emplace_back(newEvent);
//               break;

//             case midi::NoteOn:
//               static constexpr int ROUND_UP_MS = 100;
//               // round up events just at end probably meant to be played at start
//               if (newEvent.patternNumber == (data.numberOfBars - 1) && newEvent.patternMs > (data.patternLengthMs - ROUND_UP_MS)) {
//                 newEvent.patternNumber = 0;
//                 newEvent.patternMs = 0;
//               }
//               data.notesOn.insert(std::pair<uint8_t, MidiEvent>(note, newEvent));
//               break;

//             case midi::NoteOff:
//               // check if it has a corresponding NoteOn
//               const auto on = data.notesOn.find(note);
//               if (on != data.notesOn.end()) {
//                 timeQuantization(on->second, data.trackSettings[data.activeTrack].quantizeDenom);
//                 // if so, insert NoteOn and this NoteOff to pending
//                 data.pendingEvents.emplace_back(on->second);
//                 data.pendingEvents.emplace_back(newEvent);
//                 data.notesOn.erase(on);
//               }
//               break;
//             }
//           }
//         }
//       }

//       // forward incoming midi event to correct channel
//       // ignore events directly mapped to an instrument
//       const bool arpSamplesKeyboard = data.isRunning && (data.arpSettings.enabled) && (data.arpSettings.source == 0);
//       if (arpSamplesKeyboard) {
//         switch (event) {
//         case midi::NoteOn:
//           activeArpKeys.insert(note);
//           data.arpSettings.keysChanged = true;
//           break;

//         case midi::NoteOff:
//           activeArpKeys.erase(note);
//           data.arpSettings.keysChanged = true;
//           break;

//         default:
//           break;
//         }
//       }
//       else {
//         midi::Channel ch = data.tracks[data.activeTrack].channel;
//         uint8_t device = data.trackSettings[data.activeTrack].device;
//         switch (event) {
//         case midi::ControlChange:
//           midiControlChange(note, velocity, ch, device); // note is controller number, velocity is value
//           break;

//         case midi::PitchBend:
//           midiPitchBend(toPitchBend(note, velocity), ch, device); // note is LSB, velocity is MSB
//           break;

//         case midi::AfterTouchChannel:
//           handleAfterTouch(ch, note);
//           break;

//         case midi::NoteOn:
//           pressedKeys.insert(note);
//           DBG_LOG(printf("noteOn on ch%i of device %i\n", ch, device));

//           if (data.activeTrack == SONG_MODE_NOTES_TRACK && openendFromLiveSequencer) {
//             ch = data.tracks[data.stepRecordTargetTrack].channel;
//             device = data.trackSettings[data.stepRecordTargetTrack].device;
//           }

//           handleNoteOnInput(ch, note, velocityActive, device);

//           if (data.lastPlayedNote != note) {
//             data.lastPlayedNote = note;
//             ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawLastPlayedNote);
//           }
//           break;

//         case midi::NoteOff:
//           if (pressedKeys.count(note) > 0) {
//             DBG_LOG(printf("noteOff on ch%i of device %i\n", ch, device));
//             pressedKeys.erase(note);

//             if (data.activeTrack == SONG_MODE_NOTES_TRACK && openendFromLiveSequencer) {
//               ch = data.tracks[data.stepRecordTargetTrack].channel;
//               device = data.trackSettings[data.stepRecordTargetTrack].device;
//             }

//             handleNoteOff(ch, note, velocityActive, device);
//           }
//           break;

//         default:
//           break;
//         }
//       }
//     }
//     else {
//       ui_liveSeq->showDirectMappingWarning(inChannel);
//       DBG_LOG(printf("LiveSeq: drop event as directly assigned to an instrument\n"));
//     }
//   }
//   else {
//     DBG_LOG(printf("LiveSeq: drop event as not active\n"));
//   }
// }

FLASHMEM void LiveSequencer::handleMidiEvent(uint8_t inChannel, midi::MidiType event, uint8_t note, uint8_t velocity) {
  if (data.processMidiIn) {
    DBG_LOG(printf("got midi note %i\n", note));
    
    // REMOVED: Channel filtering - now all MIDI input is processed
    // if (data.instrumentChannels.count(inChannel) == 0) {
    
    ui_pianoRoll->handleKeyChanged(note, event, velocity);
    // velocity adjustment for keyboard events (live playing or recording)
    const uint8_t velocitySetting = data.trackSettings[data.activeTrack].velocityLevel;
    const uint8_t velocityActive = (velocitySetting == 0) ? velocity : velocitySetting * 1.27f; // 100% * 1.27 = 127

    if ((data.isRecording && data.isRunning) || data.remainingCountIns > 0) {
      const EventSource source = data.isSongMode ? EVENT_SONG : EVENT_PATTERN;
      MidiEvent newEvent = { source, uint16_t(data.patternTimer), data.currentPattern, data.activeTrack, data.trackSettings[data.activeTrack].layerCount, event, note, velocityActive };

      if (data.isSongMode) {
        if (data.songLayerCount < LIVESEQUENCER_NUM_LAYERS) {
          // in song mode, simply add event, no rounding and checking needed
          newEvent.layer = data.songLayerCount;
          uint8_t patternCount = data.songPatternCount;
          if (newEvent.event == midi::NoteOn) {
            if (timeQuantization(newEvent, data.trackSettings[data.activeTrack].quantizeGrid)) {
              patternCount++; // event rounded up to start of next song pattern
            }
          }
          data.recordedToSong = true;
          data.songEvents[patternCount].emplace_back(newEvent);
        }
      }
      else {
        if (data.trackSettings[data.activeTrack].layerCount < LIVESEQUENCER_NUM_LAYERS) {
          switch (newEvent.event) {
          default:
            // ignore all other types
            break;

            // TODO: record aftertouch

          case midi::ControlChange:
            // Record MIDI CC
            newEvent.note_in = note;              // Controller number
            newEvent.note_in_velocity = velocity; // Controller value
            data.pendingEvents.emplace_back(newEvent);
            break;

          case midi::PitchBend:
            // Record Pitch Bend
            newEvent.note_in = note;              // LSB
            newEvent.note_in_velocity = velocity; // MSB
            data.pendingEvents.emplace_back(newEvent);
            break;

          case midi::NoteOn:
            static constexpr int ROUND_UP_MS = 100;
            // round up events just at end probably meant to be played at start
            if (newEvent.patternNumber == (data.numberOfBars - 1) && newEvent.patternMs > (data.patternLengthMs - ROUND_UP_MS)) {
              newEvent.patternNumber = 0;
              newEvent.patternMs = 0;
            }
            data.notesOn.insert(std::pair<uint8_t, MidiEvent>(note, newEvent));
            break;

          case midi::NoteOff:
            // check if it has a corresponding NoteOn
            const auto on = data.notesOn.find(note);
            if (on != data.notesOn.end()) {
              timeQuantization(on->second, data.trackSettings[data.activeTrack].quantizeGrid);
              // if so, insert NoteOn and this NoteOff to pending
              data.pendingEvents.emplace_back(on->second);
              data.pendingEvents.emplace_back(newEvent);
              data.notesOn.erase(on);
            }
            break;
          }
        }
      }
    }

    // forward incoming midi event to correct channel
    // ALWAYS route to currently active track, regardless of input channel
    const bool arpSamplesKeyboard = data.isRunning && (data.arpSettings.enabled) && (data.arpSettings.source == 0);
    if (arpSamplesKeyboard) {
      switch (event) {
      case midi::NoteOn:
        activeArpKeys.insert(note);
        data.arpSettings.keysChanged = true;
        break;

      case midi::NoteOff:
        activeArpKeys.erase(note);
        data.arpSettings.keysChanged = true;
        break;

      default:
        break;
      }
    }
    else {
      midi::Channel ch = data.tracks[data.activeTrack].channel;
      uint8_t device = data.trackSettings[data.activeTrack].device;
      switch (event) {
      case midi::ControlChange:
        midiControlChange(note, velocity, ch, device); // note is controller number, velocity is value
        break;

      case midi::PitchBend:
        midiPitchBend(toPitchBend(note, velocity), ch, device); // note is LSB, velocity is MSB
        break;

      case midi::AfterTouchChannel:
        handleAfterTouch(ch, note);
        break;

      case midi::NoteOn:
        pressedKeys.insert(note);
        DBG_LOG(printf("noteOn on ch%i of device %i\n", ch, device));

        if (data.activeTrack == SONG_MODE_NOTES_TRACK && openendFromLiveSequencer) {
          ch = data.tracks[data.stepRecordTargetTrack].channel;
          device = data.trackSettings[data.stepRecordTargetTrack].device;
        }

        handleNoteOnInput(ch, note, velocityActive, device);

        if (data.lastPlayedNote != note) {
          data.lastPlayedNote = note;
          ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawLastPlayedNote);
        }
        break;

      case midi::NoteOff:
        if (pressedKeys.count(note) > 0) {
          DBG_LOG(printf("noteOff on ch%i of device %i\n", ch, device));
          pressedKeys.erase(note);

          if (data.activeTrack == SONG_MODE_NOTES_TRACK && openendFromLiveSequencer) {
            ch = data.tracks[data.stepRecordTargetTrack].channel;
            device = data.trackSettings[data.stepRecordTargetTrack].device;
          }

          handleNoteOff(ch, note, velocityActive, device);
        }
        break;

      default:
        break;
      }
    }
    // REMOVED: The else block that showed the direct mapping warning
    // } else {
    //   ui_liveSeq->showDirectMappingWarning(inChannel);
    //   DBG_LOG(printf("LiveSeq: drop event as directly assigned to an instrument\n"));
    // }
  }
  else {
    DBG_LOG(printf("LiveSeq: drop event as not active\n"));
  }
}

FLASHMEM void LiveSequencer::fillTrackLayer(void) {
  if (data.trackSettings[data.activeTrack].layerCount < LIVESEQUENCER_NUM_LAYERS) {
    const float msIncrement = data.patternLengthMs / float(data.fillNotes.number);
    const uint8_t msOffset = round(data.fillNotes.offset * msIncrement / 8.0f);
    const uint16_t noteLength = round(msIncrement / 2.0f); // ...
    const uint8_t velocity = uint8_t(data.fillNotes.velocityLevel * 1.27F);
    for (uint8_t bar = 0; bar < data.numberOfBars; bar++) {
      for (uint16_t note = 0; note < data.fillNotes.number; note++) {
        const uint16_t noteOnTime = round(note * msIncrement) + msOffset;
        const uint16_t noteOffTime = noteOnTime + noteLength;
        data.pendingEvents.emplace_back(MidiEvent{ EVENT_PATTERN, noteOnTime, bar, data.activeTrack, data.trackSettings[data.activeTrack].layerCount, midi::NoteOn, data.lastPlayedNote, velocity });
        data.pendingEvents.emplace_back(MidiEvent{ EVENT_PATTERN, noteOffTime, bar, data.activeTrack, data.trackSettings[data.activeTrack].layerCount, midi::NoteOff, data.lastPlayedNote, 0 });
      }
    }
    if (data.isRunning == false) {
      addPendingNotes();
    }
  }
}

FLASHMEM void LiveSequencer::changeNumberOfBars(uint8_t num) {
  if (num != data.numberOfBars) {
    data.numberOfBars = num;
    deleteLiveSequencerData();
  }
}

FLASHMEM void LiveSequencer::deleteLiveSequencerData(void) {
  allNotesOff();
  if (data.isRunning) {
    stop(); // stop so we can delete invalidated events
  }
  data.pendingEvents.clear();
  for (auto& e : data.eventsList) {
    if (e.source == EVENT_PATTERN) {
      e.event = midi::InvalidType; // mark as invalid
    }
  }

  deleteAllSongEvents();
  for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
    data.trackSettings[track].layerCount = 0;
    data.tracks[track].layerMutes = 0;
  }
  ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawLayerButtons);

  data.eventsList.clear();
  init();
}

FLASHMEM void LiveSequencer::deleteAllSongEvents(void) {
  allNotesOff();
  data.songEvents.clear();
  data.lastSongEventPattern = 0;
  data.songLayerCount = 0;

  for (auto& e : data.eventsList) {
    if (e.source == EVENT_SONG) {
      e.event = midi::InvalidType; // mark as invalid
    }
  }
  for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
    data.trackSettings[track].songStartLayerMutes = 0;
  }
  ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawSongLayers);
}

FLASHMEM void LiveSequencer::songLayerAction(uint8_t layer, LayerMode action) {
  if ((layer == 0) && (action == LayerMode::LAYER_MERGE)) {
    return; // avoid merge up top layer
  }
  for (auto& e : data.songEvents) {
    for (auto& a : e.second) {
      performLayerAction(action, a, layer);
    }
  }
  refreshSongLength();
  data.songLayerCount--;
  ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawSongLayers);
}

FLASHMEM bool LiveSequencer::trackLayerAction(uint8_t track, uint8_t layer, LayerMode action, bool& clearLayer) {
  if ((layer == 0) && (action == LayerMode::LAYER_MERGE)) {
    return false; // avoid merge up top layer
  }

  clearLayer = (action == LayerMode::LAYER_MERGE) || (action == LayerMode::LAYER_DELETE);

  if (clearLayer == false) {
    // TODO: reset CC / PB for track?
    //resetTrackControllers(track);
    const midi::MidiType targetEvent = (action == LayerMode::LAYER_CLEAR_CC) ? midi::ControlChange : midi::PitchBend;
    for (auto& e : data.eventsList) {
      if ((e.track == track) && (e.layer == layer) && (e.event == targetEvent)) {
        e.event = midi::InvalidType; // mark as invalid
      }
    }
  }
  else {
    // play noteOff for active layer notes
    allLayerNotesOff(track, layer);

    for (auto& e : data.eventsList) {
      if (e.track == track) {
        performLayerAction(action, e, layer);
      }
    }

    // handle layer mutes. example with layer 2 deleted
    // old: 0010 1101
    // new: 0001 0101 -> lower layers stay same, higher layers shifted down by one
    const uint8_t bitmask = pow(2, layer) - 1;
    const uint8_t layerMutesLo = data.tracks[track].layerMutes & bitmask;         // 0010 1101 &  0000 0011 = 0000 0001
    const uint8_t layerMutesHi = (data.tracks[track].layerMutes >> 1) & ~bitmask; // 0001 0110 & ~0000 0011 = 0001 0100
    data.tracks[track].layerMutes = (layerMutesLo | layerMutesHi);                // 0000 0001 |  0001 0100 = 0001 0101
    data.trackSettings[track].layerCount--;
  }
  ui_liveSeq->drawTrackLayers(track);
  return true;
}

FLASHMEM void LiveSequencer::performLayerAction(LayerMode action, MidiEvent& e, uint8_t layer) {
  if (e.layer == layer) {
    switch (action) {
    case LayerMode::LAYER_MERGE:
      // layer 0 must not be shifted up
      if (e.layer > 0) {
        e.layer--;
      }
      break;

    case LayerMode::LAYER_DELETE:
      e.event = midi::InvalidType; // mark layer notes to delete later
      break;

    default:
      break;
    }
  }

  // both actions above shift upper layers one lower
  if (e.layer > layer) {
    e.layer--;
  }
}

FLASHMEM void LiveSequencer::loadNextEvent(int timeMs) {
  if (data.isRunning) {
    if (timeMs > 0) {
      //LOG.printf("trigger in %ims\n", timeMs);
      liveTimer.trigger(timeMs * 1000);
    }
    else {
      playNextEvent();
    }
  }
}

FLASHMEM void LiveSequencer::playNextEvent(void) {

  bool isMuted = false;
  if (playIterator->source == EVENT_PATTERN) {
    if (data.useGridSongMode && !data.seq_started_from_pattern_mode) {
      isMuted = !data.gridStep(data.currentGridStep, playIterator->track, playIterator->layer);
    }
    else {
      isMuted = (data.tracks[playIterator->track].layerMutes & (1 << playIterator->layer));
    }
  }


  if (isMuted) {
    // Just advance to next event without playing ANYTHING

    // Advance iterator and schedule next event
    if (++playIterator != data.eventsList.end()) {
      const unsigned long now = ((data.currentPattern * data.patternLengthMs) + data.patternTimer);
      int timeToNextEvent = timeToMs(playIterator->patternNumber, playIterator->patternMs) - now;
      loadNextEvent(timeToNextEvent);
    }
    return; // EXIT WITHOUT PROCESSING THE MUTED EVENT
  }

  // Only process events that are NOT muted
  const midi::Channel channel = data.tracks[playIterator->track].channel;
  const uint8_t device = data.trackSettings[playIterator->track].device;

  switch (playIterator->event) {
  case midi::PitchBend:
    midiPitchBend(toPitchBend(playIterator->note_in, playIterator->note_in_velocity), channel, device);
    data.tracks[playIterator->track].pitchBend[playIterator->layer] = { true, playIterator->note_in_velocity };
    break;

  case midi::ControlChange:
    midiControlChange(playIterator->note_in, playIterator->note_in_velocity, channel, device);
    switch (playIterator->note_in) {
    case AutomationType::TYPE_MUTE_ON:
    case AutomationType::TYPE_MUTE_OFF:
      const uint8_t muteLayer = playIterator->note_in_velocity;
#ifdef DEBUG
      DBG_LOG(printf("track %i layer %i %s\n", muteLayer, playIterator->track, playIterator->note_in == AutomationType::TYPE_MUTE_ON ? "MUTE" : "UNMUTE"));
#endif
      setLayerMuted(playIterator->track, muteLayer, playIterator->note_in == AutomationType::TYPE_MUTE_ON);
      break;
    }
    break;

  case midi::NoteOff:
    if (data.arpSettings.enabled && (data.arpSettings.source - 1) == playIterator->track) {
      activeArpKeys.erase(playIterator->note_in);
      data.arpSettings.keysChanged = true;
    }
    else {
      const auto it = data.tracks[playIterator->track].activeNotes[playIterator->layer].find(playIterator->note_in);
      if (it != data.tracks[playIterator->track].activeNotes[playIterator->layer].end()) {
        data.tracks[playIterator->track].activeNotes[playIterator->layer].erase(it);
      }
      handleNoteOff(channel, playIterator->note_in, playIterator->note_in_velocity, device);
    }
    break;

  case midi::NoteOn:
    if (data.arpSettings.enabled && (data.arpSettings.source - 1) == playIterator->track) {
      activeArpKeys.insert(playIterator->note_in);
      data.arpSettings.keysChanged = true;
    }
    else {
      data.tracks[playIterator->track].activeNotes[playIterator->layer].insert(playIterator->note_in);
      const uint8_t velocitySetting = data.trackSettings[playIterator->track].velocityLevel;
      const uint8_t velocityActive = (velocitySetting == 0) ? playIterator->note_in_velocity : velocitySetting * 1.27f;
      handleNoteOn(channel, playIterator->note_in, velocityActive, device);
    }
    break;

  default:
    break;
  }

  if (playIterator->source == EVENT_SONG) {
    playIterator->event = midi::InvalidType;
  }

  if (++playIterator != data.eventsList.end()) {
    const unsigned long now = ((data.currentPattern * data.patternLengthMs) + data.patternTimer);
    int timeToNextEvent = timeToMs(playIterator->patternNumber, playIterator->patternMs) - now;
    loadNextEvent(timeToNextEvent);
  }
}


inline uint32_t LiveSequencer::timeToMs(uint8_t patternNumber, uint16_t patternMs) const {
  return (patternNumber * data.patternLengthMs) + patternMs;
}

FLASHMEM void LiveSequencer::checkLoadNewArpNotes(void) {
  bool reloadArpNotes = false;

  if (data.arpSettings.latch) {
    reloadArpNotes = data.arpSettings.keysChanged && activeArpKeys.size();
  }
  else {
    reloadArpNotes = data.arpSettings.keysChanged; // TODO: maybe immediadely stop playing with no latch?
  }
  data.arpSettings.keysChanged = false;

  if (reloadArpNotes) {
    data.arpSettings.arpNotesIn.assign(activeArpKeys.begin(), activeArpKeys.end());
  }

  if (reloadArpNotes || data.arpSettings.arpSettingsChanged) {
    data.arpSettings.arpSettingsChanged = false;
    data.arpSettings.arpNotes.assign(data.arpSettings.arpNotesIn.begin(), data.arpSettings.arpNotesIn.end());
    const uint8_t numNotes = data.arpSettings.arpNotes.size();

    for (uint octave = 1; octave < data.arpSettings.octaves; octave++) {
      for (uint number = 0; number < numNotes; number++) {
        data.arpSettings.arpNotes.emplace_back(data.arpSettings.arpNotes[number] + (octave * 12));
      }
    }

    switch (data.arpSettings.mode) {
    case ArpMode::ARP_RANDOM:
      std::random_shuffle(data.arpSettings.arpNotes.begin(), data.arpSettings.arpNotes.end());
      break;
    case ArpMode::ARP_DOWN:
    case ArpMode::ARP_DOWNUP:
    case ArpMode::ARP_DOWNUP_P:
      std::sort(data.arpSettings.arpNotes.begin(), data.arpSettings.arpNotes.end(), std::greater<>());
      break;
    case ArpMode::ARP_UP:
    case ArpMode::ARP_UPDOWN:
    case ArpMode::ARP_UPDOWN_P:
      std::sort(data.arpSettings.arpNotes.begin(), data.arpSettings.arpNotes.end(), std::less<>());
      break;
    default:
      break;
    }

    data.arpSettings.arpIt = data.arpSettings.arpNotes.begin();
    data.arpSettings.notePlayCount = 0;
  }
}

FLASHMEM void LiveSequencer::setArpEnabled(bool enabled) {
  data.arpSettings.enabled = enabled;
  if (enabled && data.arpSettings.source != 0) {
    // finish active pattern notes on arp enable
    allTrackNotesOff(data.arpSettings.source - 1);
  }
}

FLASHMEM bool sortedArpNote(const LiveSequencer::ArpNote& n1, const LiveSequencer::ArpNote& n2) {
  return (n1.offDelay < n2.offDelay);
}

FLASHMEM void LiveSequencer::playNextArpNote(void) {
  const uint16_t nowMs = uint16_t(data.patternTimer);

  if (data.arpSettings.delayToNextArpOnMs == 0) {
    const uint8_t arpAmount = data.arpSettings.amount;
    const uint8_t loadPerBar = data.arpSettings.loadPerBar;
    const float arpIntervalMs = data.patternLengthMs / float(arpAmount);
    uint8_t arpIndex = (nowMs + (arpIntervalMs / 2)) / arpIntervalMs;
    if (((arpIndex * loadPerBar) % arpAmount) == 0) { // check if reload pressed keys
      checkLoadNewArpNotes();
    }

    if (data.arpSettings.arpNotes.empty()) {
      data.arpSettings.delayToNextArpOnMs = data.patternLengthMs / loadPerBar; // bypass loading timer until next pattern start
    }
    else {
      ArpNote newArp; // play a new note...
      newArp.track = (data.arpSettings.source == 0) ? data.activeTrack : (data.arpSettings.source - 1);

      if (data.arpSettings.mode != ArpMode::ARP_CHORD) {
        newArp.notes.emplace_back(*data.arpSettings.arpIt);

        if (data.arpSettings.arpNotes.size() > 1) {
          if (++data.arpSettings.notePlayCount > data.arpSettings.noteRepeat) {
            data.arpSettings.notePlayCount = 0;
            if (++data.arpSettings.arpIt == data.arpSettings.arpNotes.end()) {
              data.arpSettings.arpIt = data.arpSettings.arpNotes.begin();
              bool doubleEndNote = false;
              switch (data.arpSettings.mode) {
              case ArpMode::ARP_DOWNUP_P:
              case ArpMode::ARP_UPDOWN_P:
                doubleEndNote = true;
              case ArpMode::ARP_DOWNUP:
              case ArpMode::ARP_UPDOWN:
                std::reverse(data.arpSettings.arpNotes.begin(), data.arpSettings.arpNotes.end());
                if (doubleEndNote == false) {
                  data.arpSettings.arpIt++;
                }
                break;
              case ArpMode::ARP_RANDOM:
                std::random_shuffle(data.arpSettings.arpNotes.begin(), data.arpSettings.arpNotes.end());
                break;
              default:
                break;
              }
            }
          }
        }
      }
      else {
        for (uint8_t note : data.arpSettings.arpNotes) {
          newArp.notes.emplace_back(note);
        }
      }
      if (data.arpSettings.enabled) {
        playArp(midi::NoteOn, newArp);
        newArp.offDelay = (arpIntervalMs * data.arpSettings.length) / 100;
        activeArps.emplace_back(newArp);
      }

      // calc time to next noteOn with incremented
      uint16_t nextArpEventOnTimeMs = uint16_t(++arpIndex * arpIntervalMs);
      if (arpIndex & 0x01) {
        // swing: odd beats NoteOn is variable
        nextArpEventOnTimeMs += round(data.arpSettings.swing * arpIntervalMs / 20.0); // swing from -8 to +8;
      }
      data.arpSettings.delayToNextArpOnMs = (nextArpEventOnTimeMs - nowMs);
    }
  }
  else {
    // finish and erase elapsed note
    for (auto it = activeArps.begin(); it != activeArps.end();) {
      if (it->offDelay == 0) {
        playArp(midi::NoteOff, *it);
        activeArps.erase(it);
      }
      else {
        ++it;
      }
    }
  }
  uint16_t delayToNextTimerCall = data.arpSettings.delayToNextArpOnMs;

  if (activeArps.size() && (activeArps.front().offDelay < data.arpSettings.delayToNextArpOnMs)) {
    // next call will be a finishing note
    delayToNextTimerCall = activeArps.front().offDelay;
  }
  const uint16_t delayToNextPatternStart = uint16_t(data.patternLengthMs - nowMs);
  const bool nextIsPatternStart = (delayToNextTimerCall >= delayToNextPatternStart);

  if (nextIsPatternStart) {
    delayToNextTimerCall = delayToNextPatternStart;
  }

  for (auto& n : activeArps) {
    n.offDelay -= std::min(delayToNextTimerCall, n.offDelay);
  }
  data.arpSettings.delayToNextArpOnMs -= std::min(delayToNextTimerCall, data.arpSettings.delayToNextArpOnMs);
  if (nextIsPatternStart == false) {
    //DBG_LOG(printf("@%i:\ttrigger again in %ims\n", nowMs, delayToNextTimerCall));
    // delay all arp events by 1ms to make sure when in track source mode, quantized notes are already on when sampling
    arpTimer.trigger((delayToNextTimerCall + 1) * 1000);
  }
}

FLASHMEM void LiveSequencer::playArp(const midi::MidiType type, const ArpNote arp) {
  const midi::Channel channel = data.tracks[arp.track].channel;
  const midi::Channel device = data.trackSettings[arp.track].device;

  for (auto& n : arp.notes) {
    switch (type) {
    case midi::NoteOn:
      handleNoteOn(channel, n, data.arpSettings.velocityLevel * 1.27f, device);
      break;

    case midi::NoteOff:
      handleNoteOff(channel, n, 0, device);
      break;

    default:
      break;
    }
    // FIXME: fast arp recording crashes...
    /*const uint8_t layer = data.trackSettings[data.activeTrack].layerCount;
    if(data.isRecording && layer < LIVESEQUENCER_NUM_LAYERS) {
      const EventSource source = data.isSongMode ? EVENT_SONG : EVENT_PATTERN;
      const MidiEvent newEvent = { source, uint16_t(data.patternTimer), data.currentPattern, data.activeTrack, layer, type, n, data.arpSettings.velocity };
      data.pendingEvents.emplace_back(newEvent);
    }*/
  }
}

FLASHMEM void LiveSequencer::initOnce(void) {
  tickTimer.begin([] { TeensyTimerTool::tick(); }, 0.1ms);
  liveTimer.begin([this] { playNextEvent(); });
  arpTimer.begin([this] { playNextArpNote(); });
  ui_liveSeq->init();

  // reserve memory to avoid re-allocation until reserved is used
  data.pendingEvents.reserve(200);
#ifdef PSRAM
  data.eventsList.reserve(10000);
#else
  data.eventsList.reserve(1000);
#endif
}

FLASHMEM void LiveSequencer::init(void) {
  data.patternLengthMs = (4 * 1000 * 60) / seq.bpm; // for a 4/4 signature
  checkBpmChanged();
  updateTrackChannels();
  DBG_LOG(printf("init has %i events\n", data.eventsList.size()));
  //printEvents();

  data.performanceName = seq.name;
  refreshSongLength();
}

FLASHMEM void LiveSequencer::onGuiInit(void) {
  init();
  //AddMetronome(); //moved to a button in pattern tools since this creates the metronome just by entering liveseq while recording is running 
  //and the user probably does not know where the sound is coming from after leaving the liveseq page 
}

FLASHMEM void LiveSequencer::checkBpmChanged(void) {
  if (seq.bpm != data.currentBpm) {
    data.patternLengthMs = (4 * 1000 * 60) / seq.bpm; // for a 4/4 signature
    DBG_LOG(printf("bpm changed from %i to %i\n", data.currentBpm, seq.bpm));
    float resampleFactor = data.currentBpm / float(seq.bpm);
    data.currentBpm = seq.bpm;
    // resample pattern events - not lossless
    for (auto& e : data.eventsList) {
      e.patternMs = round(resampleFactor * e.patternMs);
    }
    // resample song events - not lossless
    for (auto& e : data.songEvents) {
      for (auto& a : e.second) {
        a.patternMs = round(resampleFactor * a.patternMs);
      }
    }
  }
}

FLASHMEM void LiveSequencer::addPendingNotes(bool incrementLayer) {
  // finish active notes at end of all bars
  for (auto it = data.notesOn.begin(); it != data.notesOn.end();) {
    if (timeToMs(it->second.patternNumber, it->second.patternMs) != 0) {
      data.pendingEvents.emplace_back(it->second);
      it->second.event = midi::NoteOff;
      it->second.note_in_velocity = 0;
      it->second.patternNumber = data.numberOfBars - 1;
      it->second.patternMs = data.patternLengthMs - 1;
      data.pendingEvents.emplace_back(it->second);
      data.notesOn.erase(it++);
    }
    else {
      ++it; // ignore notesOn at 0.0000, those were round up just before
    }
  }

  // then add all notes to events and sort
  if (data.pendingEvents.size()) {
    for (auto& e : data.pendingEvents) {
      data.eventsList.emplace_back(e);
    }
    data.pendingEvents.clear();
    sortEvents();
    const bool canIncrementLayer = (data.trackSettings[data.activeTrack].layerCount < LIVESEQUENCER_NUM_LAYERS);

    if (canIncrementLayer && incrementLayer) {
      setLayerMuted(data.activeTrack, data.trackSettings[data.activeTrack].layerCount, false); // new layer is unmuted
      data.trackSettings[data.activeTrack].layerCount++;
    }
    ui_liveSeq->drawUpdates(UI_LiveSequencer::GuiUpdates::drawLayerButtons | UI_LiveSequencer::GuiUpdates::drawTrackButtons);
  }
}

FLASHMEM void LiveSequencer::sortEvents(void) {
  std::sort(data.eventsList.begin(), data.eventsList.end(), sortMidiEvent);
  ui_pianoRoll->onEventsChanged();
}

FLASHMEM void LiveSequencer::requestSortEvents(void) {
  if (data.isRunning) {
    sortRequested = true;
  }
  else {
    sortEvents();
  }
}

FLASHMEM void LiveSequencer::handlePatternBegin(void) {
  data.patternTimer = 0;

  if (data.isStarting) {

    data.isStarting = false;
    data.isRunning = true;
    data.currentPattern = 0;
    data.songPatternCount = 0;

    activeArps.clear();
    data.arpSettings.notePlayCount = 0;
    data.arpSettings.delayToNextArpOnMs = 0;
    if (data.useGridSongMode) {
      ui_liveSeq->onGridStepChanged();
    }
    if (data.isSongMode && data.isRecording && !data.useGridSongMode) {
      // save current song start layer mutes
      for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; track++) {
        data.trackSettings[track].songStartLayerMutes = data.tracks[track].layerMutes;
      }
    }
  }
  else {
    // Advance pattern counter
    if ((data.currentPattern + 1) == data.numberOfBars) {
      data.currentPattern = 0;

      if (data.useGridSongMode && !data.seq_started_from_pattern_mode) {
        const bool atEnd = data.currentGridStep >= data.gridPlaybackEndStep;
        if (atEnd && !data.gridLoopEnabled) {
          ui_liveSeq->onGridPlaybackEnded();
          return;
        }

        if (atEnd && data.gridLoopEnabled) {
          data.currentGridStep = data.gridPlaybackStartStep;
        }
        else {
          data.currentGridStep++;
        }

        ui_liveSeq->onGridStepChanged();

        // Trigger UI redraw 
        ui_liveSeq->requestGridEditorRedraw();
      }
      else  if (data.isSongMode && !data.useGridSongMode) {
        // Traditional song mode
        if ((data.isRecording == false) && (data.songPatternCount == data.lastSongEventPattern)) {
          data.songPatternCount = 0;
        }
        else {
          data.songPatternCount++;
        }
      }

    }
    else {
      data.currentPattern++;
    }
  }

  if (data.currentPattern == 0) {
    // first finish possibly not played events at end of previous pattern
    while (playIterator != data.eventsList.end()) {
#ifdef DEBUG
      DBG_LOG(printf("about to finish event: "));
      printEvent(0, *playIterator);
#endif
      playNextEvent();
    }

    // then remove all invalidated notes
    data.eventsList.erase(std::remove_if(data.eventsList.begin(), data.eventsList.end(), [](MidiEvent& e) { return e.event == midi::InvalidType; }), data.eventsList.end());
    ui_pianoRoll->onEventsChanged();

    // for song mode, add song events for this pattern
    if (data.isSongMode) {
      if (!data.useGridSongMode) {
        // Traditional song mode: load events from songEvents
        for (auto& e : data.songEvents[data.songPatternCount]) {
          data.eventsList.emplace_back(e);
        }
        sortEvents();
        if (data.songPatternCount == 0) {
          // load previously saved song start layer mutes
          applySongStartLayerMutes();
        }
      }
      else {
        // Grid song mode: pattern events should already be there, just sort them
        // The grid filtering will happen in playNextEvent()
        sortEvents();

#ifdef DEBUG
        DBG_LOG(printf("Grid mode: Starting pattern cycle for grid step %i with %i events\n",
          data.currentGridStep + 1, data.eventsList.size()));
#endif
      }
    }
    else {
      // insert pending events and sort
      addPendingNotes();

      // sort events was requested externally (on editing noteOn time)
      if (sortRequested) {
        sortRequested = false;
        sortEvents();
      }
    }

    // finally load for first event
    if (data.eventsList.size() > 0) {
      //printEvents();
      playIterator = data.eventsList.begin();
      loadNextEvent(timeToMs(playIterator->patternNumber, playIterator->patternMs));
    }
  }

  // restart arp on pattern start
  if (data.arpSettings.enabled) {
    if (data.arpSettings.arpNotes.size() && data.arpSettings.freerun == false) {
      data.arpSettings.arpSettingsChanged = true; // force reload
    }
    playNextArpNote();
  }
}

FLASHMEM void selectDexed0() {
  selected_instance_id = 0;
}

FLASHMEM void selectDexed1() {
  selected_instance_id = 1;
}

FLASHMEM void selectDexed2() {
  selected_instance_id = 2;
}

FLASHMEM void selectDexed3() {
  selected_instance_id = 3;
}

FLASHMEM void selectMs0() {
  microsynth_selected_instance = 0;
}

FLASHMEM void selectMs1() {
  microsynth_selected_instance = 1;
}

FLASHMEM void selectMsp0() {
  seq.active_multisample = 0;
}

FLASHMEM void selectMsp1() {
  seq.active_multisample = 1;
}

FLASHMEM void LiveSequencer::setLayerMuted(uint8_t track, uint8_t layer, bool isMuted, bool recordToSong) {
  if (isMuted) {
    data.tracks[track].layerMutes |= (1 << layer);
    allLayerNotesOff(track, layer);
  }
  else {
    data.tracks[track].layerMutes &= ~(1 << layer);
  }
  ui_liveSeq->drawSingleLayer(track, layer);

  if (recordToSong) {
    if (data.songLayerCount < LIVESEQUENCER_NUM_LAYERS) {
      data.recordedToSong = true;
      const AutomationType type = isMuted ? AutomationType::TYPE_MUTE_ON : AutomationType::TYPE_MUTE_OFF;
      MidiEvent e = { EVENT_SONG, uint16_t(data.patternTimer), data.currentPattern, track, data.songLayerCount, midi::MidiType::ControlChange, type, layer };
      uint8_t patternCount = data.songPatternCount;
      if (timeQuantization(e, mapQuantizeDenom(data.songMuteQuantizeDenom))) {
        patternCount++; // event rounded up to start of next song pattern
      }
      data.songEvents[patternCount].emplace_back(e);
      DBG_LOG(printf("record muted %i at %i of song pattern count %i\n", isMuted, timeToMs(data.currentPattern, data.patternTimer), data.songPatternCount));
    }
  }
}

FLASHMEM void LiveSequencer::setActiveTrack(uint8_t track) {
  for (uint8_t key : pressedKeys) { // noteOff all pressed keys before switching active track
    handleMidiEvent(0xFF, midi::NoteOff, key, 127);
  }
  data.pendingEvents.clear(); // avoid wrong track events / layer issue
  data.activeTrack = track;
  ui_liveSeq->updateTrackChannelSetupButtons(track);
}

FLASHMEM void LiveSequencer::AddMetronome(void) {
  // always assure we have a drum track with some tempo to begin

  if (data.eventsList.empty() && !seq.running) {
    const uint8_t activeTrack = data.activeTrack;

    uint8_t fillNumOld = data.fillNotes.number;
    uint8_t fillOffOld = data.fillNotes.offset;

    for (uint8_t i = 0; i < LiveSequencer::LIVESEQUENCER_NUM_TRACKS; i++) {
      if (data.tracks[i].screen == UI_func_drums) {
        data.activeTrack = i;
        data.fillNotes.number = 8;
        data.fillNotes.offset = 0;
        data.lastPlayedNote = 78; // hats
        fillTrackLayer();
        data.fillNotes.number = 1;
        data.fillNotes.offset = 0;
        data.lastPlayedNote = 48; // kick
        fillTrackLayer();
        if (data.isRunning == false) {
          // only merge if not added to pending in fillTrackLayer above
          bool clearLayer = false;
          trackLayerAction(i, 1, LayerMode::LAYER_MERGE, clearLayer);
        }
        // reset fillNotes to user values
        data.fillNotes.number = fillNumOld;
        data.fillNotes.offset = fillOffOld;
        data.activeTrack = activeTrack;
        return;
      }
    }
  }
}

FLASHMEM void LiveSequencer::changeTrackInstrument(uint8_t track, uint8_t newDevice, uint8_t newInstrumentOrChannel) {
  DBG_LOG(printf("change track %i to device %i and instrument/channel %i\n", track, newDevice, newInstrumentOrChannel));
  allTrackNotesOff(track);
  data.trackSettings[track].device = newDevice;

  // if (data.trackSettings[track].device == LiveSequencer::DEVICE_INTERNAL)
  // {
  data.trackSettings[track].instrument = newInstrumentOrChannel;
  //}
  // else
  // { // For external MIDI, convert 0-15 to 1-16 for MIDI channel
  //   data.trackSettings[track].instrument = newInstrumentOrChannel + 1;
  // }

  updateTrackChannels();
}

FLASHMEM void LiveSequencer::loadOldTrackInstruments(void) {
  const uint8_t loadTrackNumber = std::min(uint8_t(NUM_SEQ_TRACKS), LIVESEQUENCER_NUM_TRACKS);
  for (uint8_t i = 0; i < loadTrackNumber; i++) {
    data.trackSettings[i].device = DEVICE_INTERNAL;

    switch (seq.track_type[i]) {
    case 0:
      data.trackSettings[i].instrument = INSTR_DRUM;
      break;

    case 1:
      // dexed instance 0+1, 2 = epiano , 3+4 = MicroSynth, 5 = Braids
      switch (seq.instrument[i]) {
      case 0:
        data.trackSettings[i].instrument = INSTR_DX1;
        break;

      case 1:
        data.trackSettings[i].instrument = INSTR_DX2;
        break;

      case 2:
        data.trackSettings[i].instrument = INSTR_EP;
        break;

      case 3:
        data.trackSettings[i].instrument = INSTR_MS1;
        break;

      case 4:
        data.trackSettings[i].instrument = INSTR_MS2;
        break;

      case 5:
        data.trackSettings[i].instrument = INSTR_BRD;
        break;

      default:
        // new mappings not backwards compatible
        break;
      }
      break;
    }
  }
}

FLASHMEM void LiveSequencer::updateTrackChannels(bool initial) {
  updateInstrumentChannels();
  char temp_char[10];

  for (uint8_t i = 0; i < LIVESEQUENCER_NUM_TRACKS; i++) {
    const uint8_t device = data.trackSettings[i].device;
    const uint8_t instrument = data.trackSettings[i].instrument;

    data.tracks[i].screen = nullptr;
    data.tracks[i].screenSetupFn = nullptr;

    getInstrumentName(device, instrument, temp_char, data.tracks[i].name);

    if (initial) {
      data.trackSettings[i].quantizeDenom = 1; // default: no quantization
    }
    switch (device) {
    case DEVICE_INTERNAL:
      switch (instrument) {
      case INSTR_DRUM:
        data.tracks[i].channel = static_cast<midi::Channel>(drum_midi_channel);
        data.tracks[i].screen = UI_func_drums;
        if (initial) {
          data.trackSettings[i].quantizeDenom = 16; // default: drum quantize to 1/16
        }
        break;

      case INSTR_DX1:
      case INSTR_DX2:
        data.tracks[i].channel = static_cast<midi::Channel>(configuration.dexed[instrument - 1].midi_channel);
        data.tracks[i].screen = UI_func_voice_select;
        data.tracks[i].screenSetupFn = (instrument == INSTR_DX1) ? (SetupFn)selectDexed0 : (SetupFn)selectDexed1;
        break;

#if (NUM_DEXED>2)
      case INSTR_DX3:
        data.tracks[i].channel = static_cast<midi::Channel>(configuration.dexed[2].midi_channel);
        data.tracks[i].screen = UI_func_voice_select;
        data.tracks[i].screenSetupFn = (SetupFn)selectDexed2;
        break;

      case INSTR_DX4:
        data.tracks[i].channel = static_cast<midi::Channel>(configuration.dexed[3].midi_channel);
        data.tracks[i].screen = UI_func_voice_select;
        data.tracks[i].screenSetupFn = (SetupFn)selectDexed3;
        break;
#endif

#ifdef GRANULAR
      case INSTR_GRA:
        data.tracks[i].channel = static_cast<midi::Channel>(granular_params.midi_channel);
        data.tracks[i].screen = UI_func_granular;
        break;
#endif
      case INSTR_EP:
        data.tracks[i].channel = static_cast<midi::Channel>(configuration.epiano.midi_channel);
        data.tracks[i].screen = UI_func_epiano;
        break;

      case INSTR_MS1:
      case INSTR_MS2:
        data.tracks[i].channel = microsynth[instrument - INSTR_MS1].midi_channel;
        data.tracks[i].screen = UI_func_microsynth;
        data.tracks[i].screenSetupFn = (instrument == INSTR_MS1) ? (SetupFn)selectMs0 : (SetupFn)selectMs1;
        break;

      case INSTR_BRD:
        data.tracks[i].channel = braids_osc.midi_channel;
        data.tracks[i].screen = UI_func_braids;
        break;

      case INSTR_SLC:
        data.tracks[i].channel = slices_midi_channel;
        data.tracks[i].screen = UI_func_slice_editor;
        break;

      case INSTR_MSP1:
      case INSTR_MSP2:
        data.tracks[i].channel = msp[instrument - INSTR_MSP1].midi_channel;
        data.tracks[i].screen = UI_func_MultiSamplePlay;
        data.tracks[i].screenSetupFn = (instrument == INSTR_MSP1) ? (SetupFn)selectMsp0 : (SetupFn)selectMsp1;
        break;

      default:
        data.tracks[i].channel = 99; // probably unused
        break;
      }
      break;

    case DEVICE_MIDI_USB:
    case DEVICE_MIDI_DIN:
    case DEVICE_MIDI_INT:
      data.tracks[i].channel = static_cast<midi::Channel>((instrument % 16) + 1);
      break;

    default:
      data.tracks[i].channel = 99; // probably unused
      break;
    }

      data.trackSettings[i].quantizeGrid = mapQuantizeDenom(data.trackSettings[i].quantizeDenom);
  }
}

FLASHMEM void LiveSequencer::getDeviceName(uint8_t device, char* name, char* sub) const {
  switch (device) {
  case DEVICE_INTERNAL:
    sprintf(name, "DEVICE");
    sprintf(sub, "MDT");
    break;

  case DEVICE_MIDI_USB:
    sprintf(name, "MIDI");
    sprintf(sub, "USB");
    break;

  case DEVICE_MIDI_DIN:
    sprintf(name, "MIDI");
    sprintf(sub, "DIN");
    break;

  case DEVICE_MIDI_INT:
    sprintf(name, "MIDI");
    sprintf(sub, "INT");
    break;

  default:
    sprintf(name, "NONE");
    sprintf(sub, " ");
    break;
  }
}

FLASHMEM void LiveSequencer::getInstrumentName(uint8_t device, uint8_t instrument, char* name, char* sub) const {
  switch (device) {
  case DEVICE_INTERNAL:
    sprintf(name, "INSTR");

    switch (instrument) {
    case INSTR_DRUM:
      sprintf(sub, "DRM");
      break;

    case INSTR_DX1:
    case INSTR_DX2:
      sprintf(sub, "DX%i", instrument);
      break;

    case INSTR_DX3:
    case INSTR_DX4:
      sprintf(sub, "DX%i", instrument - INSTR_DX3 + 3);
      break;

    case INSTR_GRA:
      sprintf(sub, "GRA");
      break;

    case INSTR_EP:
      sprintf(sub, "EP");
      break;

    case INSTR_MS1:
    case INSTR_MS2:
      sprintf(sub, "MS%i", instrument - INSTR_MS1 + 1);
      break;

    case INSTR_BRD:
      sprintf(sub, "BRD");
      break;

    case INSTR_MSP1:
    case INSTR_MSP2:
      sprintf(sub, "SP%i", instrument - INSTR_MSP1 + 1);
      break;

    case INSTR_SLC:
      sprintf(sub, "SLC");
      break;

    default:
      sprintf(sub, "-");
      break;
    }
    break;

  case DEVICE_MIDI_USB:
    sprintf(name, "CHANNEL");
    sprintf(sub, "USB %02i", instrument + 1);
    break;

  case DEVICE_MIDI_DIN:
    sprintf(name, "CHANNEL");
    sprintf(sub, "DIN %02i", instrument + 1);
    break;

  case DEVICE_MIDI_INT:
    sprintf(name, "CHANNEL");
    sprintf(sub, "INT %02i", instrument + 1);
    break;

  default:
    sprintf(name, "NONE");
    sprintf(sub, " ");
    break;
  }
}

FLASHMEM void LiveSequencer::updateInstrumentChannels(void) {
  data.instrumentChannels.clear();
  data.instrumentChannels.insert(drum_midi_channel);
  for (int i = 0; i < NUM_DEXED; i++) {
    data.instrumentChannels.insert(configuration.dexed[i].midi_channel);
  }
#ifdef GRANULAR
  data.instrumentChannels.insert(granular_params.midi_channel);
#endif
  data.instrumentChannels.insert(configuration.epiano.midi_channel);
  for (int i = 0; i < NUM_MICROSYNTH; i++) {
    data.instrumentChannels.insert(microsynth[i].midi_channel);
  }
  for (int i = 0; i < NUM_MULTISAMPLES; i++) {
    data.instrumentChannels.insert(msp[i].midi_channel);
  }
  data.instrumentChannels.insert(braids_osc.midi_channel);
}

FLASHMEM void LiveSequencer::addNotePair(MidiEvent noteOn, MidiEvent noteOff) {
  DBG_LOG(printf("adding note pair:\n"));
  printEvent(1, noteOn);
  printEvent(2, noteOff);
  data.pendingEvents.emplace_back(noteOn);
  data.pendingEvents.emplace_back(noteOff);
  if (data.isRunning == false) {
    addPendingNotes(false); // do not increment layer count
  }
}

FLASHMEM std::vector<LiveSequencer::NotePair> LiveSequencer::getNotePairsFromTrack(uint8_t track, uint8_t& lowestNote, uint8_t& highestNote, uint8_t patternFrom, uint8_t patternTo) {
  std::vector<LiveSequencer::NotePair> result;
  uint8_t lowestFound = 0xFF;
  uint8_t highestFound = 0x00;

  for (EventVector::iterator itOn = data.eventsList.begin(); itOn != data.eventsList.end(); itOn++) {
    if (itOn->track == track && itOn->event == midi::NoteOn) {
      for (EventVector::iterator itOff = itOn; itOff != data.eventsList.end(); itOff++) {
        const bool isWithinY = (itOn->note_in >= lowestNote) && (itOn->note_in <= highestNote);
        if (isWithinY) {
          const bool sameNote = itOff->note_in == itOn->note_in;
          const bool sameTrack = itOff->track == itOn->track;
          const bool sameLayer = itOff->layer == itOn->layer;
          const bool isNoteOff = itOff->event == midi::NoteOff;

          if (sameTrack && sameLayer && sameNote && isNoteOff) {
            const bool isInside = (patternTo >= itOn->patternNumber) && (patternFrom <= itOff->patternNumber);
            if (isInside) {
              NotePair p = {
                .noteOn = *itOn,
                .noteOff = *itOff,
                .isMuted = (data.tracks[track].layerMutes & (1 << itOn->layer)) > 0
              };
              highestFound = std::max(highestFound, itOn->note_in);
              lowestFound = std::min(lowestFound, itOn->note_in);
              result.emplace_back(p);
            }
            break;
          }
        }
      }
    }
  }
  lowestNote = lowestFound;
  highestNote = highestFound;
  return result;
}

FLASHMEM void LiveSequencer::printNotePairs(std::vector<NotePair> notePairs) {
  int pairNum = 0;
  DBG_LOG(printf("\nON-OFF Pair:\n"));

  for (LiveSequencer::NotePair p : notePairs) {
    LiveSequencer::printEvent(pairNum, p.noteOn);
    LiveSequencer::printEvent(pairNum, p.noteOff);
    pairNum++;
  }
}

FLASHMEM bool LiveSequencer::canInsertGridRow(uint8_t step) const {
  if (data.gridSongSteps == nullptr) {
    return false;
  }
  if (step >= LiveSeqData::GRIDSONGLEN) {
    return false;
  }
  const int lastOccupied = lastOccupiedGridRow();
  if (lastOccupied < 0) {
    return false;
  }
  if (lastOccupied < static_cast<int>(step)) {
    return false;
  }
  return lastOccupied < (static_cast<int>(LiveSeqData::GRIDSONGLEN) - 1);
}

FLASHMEM bool LiveSequencer::canRemoveGridRow(uint8_t step) const {
  if (data.gridSongSteps == nullptr) {
    return false;
  }
  if (step >= LiveSeqData::GRIDSONGLEN) {
    return false;
  }
  if (!isGridRowEmpty(step)) {
    return false;
  }

  const int lastOccupied = lastOccupiedGridRow();
  if (lastOccupied <= static_cast<int>(step)) {
    return false;
  }

  return true;
}

FLASHMEM bool LiveSequencer::insertGridRow(uint8_t step) {
  if (!data.gridSongSteps || step >= LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  const int lastOccupied = lastOccupiedGridRow();
  if (lastOccupied >= (static_cast<int>(LiveSeqData::GRIDSONGLEN) - 1)) {
    return false;
  }

  const size_t stride = GRID_ROW_STRIDE;
  uint8_t* const rowPtr = data.gridSongSteps + static_cast<size_t>(step) * stride;
  const bool shiftedContent = lastOccupied >= static_cast<int>(step);

  if (shiftedContent) {
    const size_t rowsToMove = static_cast<size_t>(lastOccupied - static_cast<int>(step) + 1);
    memmove(rowPtr + stride, rowPtr, rowsToMove * stride);
  }

  memset(rowPtr, 0, stride);

  adjustGridMarkersAfterInsert(step, shiftedContent);
  clampGridMarkers();
  return true;
}

FLASHMEM bool LiveSequencer::removeGridRow(uint8_t step) {
  if (!data.gridSongSteps || step >= LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  if (!isGridRowEmpty(step)) {
    return false;
  }

  const int lastOccupied = lastOccupiedGridRow();
  const size_t stride = GRID_ROW_STRIDE;
  uint8_t* const dest = data.gridSongSteps + static_cast<size_t>(step) * stride;

  if (lastOccupied > static_cast<int>(step)) {
    const size_t rowsToMove = static_cast<size_t>(lastOccupied - static_cast<int>(step));
    memmove(dest, dest + stride, rowsToMove * stride);
    uint8_t* const zeroRow = dest + rowsToMove * stride;
    memset(zeroRow, 0, stride);
  }
  else {
    memset(dest, 0, stride);
  }

  adjustGridMarkersAfterRemove(step);
  clampGridMarkers();
  return true;
}

FLASHMEM bool LiveSequencer::isGridRowEmpty(uint8_t step) const {
  if (!data.gridSongSteps || step >= LiveSeqData::GRIDSONGLEN) {
    return true;
  }

  const uint8_t* const rowPtr = data.gridSongSteps + static_cast<size_t>(step) * GRID_ROW_STRIDE;
  for (size_t i = 0; i < GRID_ROW_STRIDE; ++i) {
    if (rowPtr[i] != 0) {
      return false;
    }
  }
  return true;
}

FLASHMEM int LiveSequencer::lastOccupiedGridRow() const {
  if (!data.gridSongSteps) {
    return -1;
  }

  for (int step = static_cast<int>(LiveSeqData::GRIDSONGLEN) - 1; step >= 0; --step) {
    if (!isGridRowEmpty(static_cast<uint8_t>(step))) {
      return step;
    }
  }
  return -1;
}

FLASHMEM void LiveSequencer::adjustGridMarkersAfterInsert(uint8_t step, bool shiftedContent) {
  if (!shiftedContent) {
    return;
  }

  auto incrementIfNeeded = [&](uint8_t& marker) {
    if (marker >= step && marker < (LiveSeqData::GRIDSONGLEN - 1)) {
      marker++;
    }
  };

  incrementIfNeeded(data.currentGridStep);

  // Keep the viewport anchored only when we inserted above it; inserting at the first
  // visible row should reveal the new empty row instead of scrolling past it.
  if (data.gridStepOffset > step && data.gridStepOffset < (LiveSeqData::GRIDSONGLEN - 1)) {
    data.gridStepOffset++;
  }
}

FLASHMEM void LiveSequencer::adjustGridMarkersAfterRemove(uint8_t step) {
  auto decrementIfNeeded = [&](uint8_t& marker) {
    if (marker > step) {
      marker--;
    }
  };

  decrementIfNeeded(data.currentGridStep);
  if (data.gridStepOffset > 0 && data.gridStepOffset > step) {
    data.gridStepOffset--;
  }
}

FLASHMEM void LiveSequencer::clampGridMarkers() {
  auto clampMarker = [&](uint8_t& marker) {
    if (marker >= LiveSeqData::GRIDSONGLEN) {
      marker = LiveSeqData::GRIDSONGLEN - 1;
    }
  };

  clampMarker(data.gridPlaybackStartStep);
  clampMarker(data.gridPlaybackEndStep);
  clampMarker(data.currentGridStep);
  clampMarker(data.gridStepOffset);
  clampMarker(data.selectedGridStep);

  if (data.gridPlaybackEndStep < data.gridPlaybackStartStep) {
    data.gridPlaybackStartStep = data.gridPlaybackEndStep;
  }
}

FLASHMEM void LiveSequencer::resetGridEditorDefaults(void) {
  data.gridPlaybackStartStep = 0;
  data.gridPlaybackEndStep = LiveSeqData::GRIDSONGLEN - 1;
  data.gridLoopEnabled = true;
  data.gridStepOffset = 0;
  data.currentGridStep = data.gridPlaybackStartStep;
  data.selectedGridStep = data.gridPlaybackStartStep;
  data.selectedGridTrack = 0;
  clampGridMarkers();
}

FLASHMEM void LiveSequencer::sanitizeGridSongSteps() {
	if (!data.gridSongSteps) {
		return;
	}

	const size_t stride = GRID_ROW_STRIDE;
	const size_t trackStride = LIVESEQUENCER_NUM_LAYERS;

	for (uint8_t track = 0; track < LIVESEQUENCER_NUM_TRACKS; ++track) {
		uint8_t validLayers = data.trackSettings[track].layerCount;
		if (validLayers > LIVESEQUENCER_NUM_LAYERS) {
			validLayers = LIVESEQUENCER_NUM_LAYERS;
		}

		if (validLayers >= LIVESEQUENCER_NUM_LAYERS) {
			continue;
		}

		for (uint8_t step = 0; step < LiveSeqData::GRIDSONGLEN; ++step) {
			uint8_t* const rowTrack = data.gridSongSteps + static_cast<size_t>(step) * stride + track * trackStride;
			for (uint8_t layer = validLayers; layer < LIVESEQUENCER_NUM_LAYERS; ++layer) {
				rowTrack[layer] = 0;
			}
		}
	}
}

FLASHMEM bool LiveSequencer::isTrackLayerEmpty(uint8_t track, uint8_t layer) {
  // Check if the track and layer are valid
  if (track >= LIVESEQUENCER_NUM_TRACKS || layer >= LIVESEQUENCER_NUM_LAYERS) {
    return true; // Invalid track or layer, consider it empty
  }

  // Check pattern events for this track and layer
  for (const auto& event : data.eventsList) {
    if (event.track == track &&
      event.layer == layer &&
      event.event != midi::InvalidType) {
      return false; // Found at least one valid event in this track-layer
    }
  }

  // Also check if there are any active notes in this track-layer (for real-time playing)
  if (!data.tracks[track].activeNotes[layer].empty()) {
    return false;
  }

  return true; // No events found for this track-layer combination
}



