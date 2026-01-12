#include "ui_livesequencer.h"
#include "LCDMenuLib2.h"
#include "ILI9341_t3n.h"
#include "sequencer.h"
#include "livesequencer.h"
#include "touch.h"
#include "ui_liveseq_pianoroll.h"
#include "virtualkeyboard.h"
#include <algorithm>


extern sequencer_t seq;
extern LCDMenuLib2 LCDML;
extern ILI9341_t3n display;
extern ts_t ts;
extern bool remote_active;
bool openingScreen = false;

static constexpr int DRAW_INTERVAL = 25; //ms

#define SCREEN_TRACK_INDEX(t) (t % LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN)

FLASHMEM UI_LiveSequencer::UI_LiveSequencer(LiveSequencer& sequencer, LiveSequencer::LiveSeqData& d)
  : instance(this), liveSeq(sequencer), data(d), gridEditor(sequencer, d, display) {
  GridSongEditorUI::Callbacks callbacks{
    [this]() {
      currentPage = PAGE_PATTERN;
      resetProgressBars();
      drawUpdates(drawTime);
      redrawScreen();
    }
  };
  gridEditor.setCallbacks(callbacks);
}

FLASHMEM void UI_LiveSequencer::checkApplyTrackInstrument(void) {
  const LiveSequencer::TrackSettings& trackSettings = data.trackSettings[data.activeTrack];

  uint8_t newInstrument = selectedTrackSetup.instrument;

  const bool hasChanged = (trackSettings.device != selectedTrackSetup.device) ||
    (trackSettings.instrument != newInstrument);

  if (hasChanged) {
    liveSeq.changeTrackInstrument(data.activeTrack, selectedTrackSetup.device, newInstrument);
    drawUpdates(drawTrackButtons);
  }
}

FLASHMEM void UI_LiveSequencer::drawTrackSubtext(uint8_t track) {
  display.fillRect(GRID.X[SCREEN_TRACK_INDEX(track)], GRID.Y[1] + TouchButton::BUTTON_SIZE_Y + 3, TouchButton::BUTTON_SIZE_X, CHAR_height_small, COLOR_BACKGROUND);
  display.setTextSize(1);
  display.setTextColor((track == data.activeTrack) ? GREY1 : GREY2);
  display.setCursor(GRID.X[SCREEN_TRACK_INDEX(track)], GRID.Y[1] + TouchButton::BUTTON_SIZE_Y + 3);

  const uint8_t denom = data.trackSettings[track].quantizeDenom;

  if (denom == 1) {
    display.print("Q-");
  }
  else if (denom == 64) {
    display.print("Q64");
  }
  else if (denom == 32) {
    display.print("Q32");
  }
  else if (denom == 24) {
    display.print("Q16T");
  }
  else if (denom == 16) {
    display.print("Q16");
  }
  else if (denom == 12) {
    display.print("Q8T");
  }
  else if (denom == 8) {
    display.print("Q8");
  }
  else if (denom == 4) {
    display.print("Q4");
  }
  else {
    display.printf("Q%i", denom);
  }

  const uint8_t velocity = data.trackSettings[track].velocityLevel;
  const uint8_t digits = (velocity == 0) ? 3 : floor(log10(velocity)) + 2;
  display.setCursor(GRID.X[SCREEN_TRACK_INDEX(track)] + TouchButton::BUTTON_SIZE_X - CHAR_width_small * digits, GRID.Y[1] + TouchButton::BUTTON_SIZE_Y + 3);

  if (velocity == 0) {
    display.print("KEY");
  }
  else {
    display.printf("%i%%", velocity);
  }
}


FLASHMEM void metronome_text_pos()
{
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setCursor(CHAR_width_small * 18, CHAR_height_small * 27);
}

FLASHMEM void UI_LiveSequencer::init(void) {
  // TRACK BUTTONS
  for (uint8_t trackIndex = 0; trackIndex < LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN; trackIndex++) {
    trackButtons.push_back(new TouchButton(GRID.X[trackIndex], GRID.Y[1],
      [this, trackIndex](auto* b) { // drawHandler
        const bool isActiveTrack = ((trackOffset + trackIndex) == data.activeTrack);
        const TouchButton::Color color = isActiveTrack ? (data.isRecording ? TouchButton::BUTTON_RED : TouchButton::BUTTON_HIGHLIGHTED) : TouchButton::BUTTON_ACTIVE;
        char temp_char[4];
        b->draw(data.tracks[(trackOffset + trackIndex)].name, itoa((trackOffset + trackIndex) + 1, temp_char, 10), color);
        instance->drawTrackSubtext(trackOffset + trackIndex);
      },
      [this, trackIndex](auto* b) { // clickedHandler
        instance->onTrackButtonPressed(trackOffset + trackIndex);
      },
      [this, trackIndex](auto* b) { // longPressHandler
        openScreen(UI_func_liveseq_graphic, (trackOffset + trackIndex));
      }));
  }

  // MODE BUTTON with long press support
  modeButton = new TouchButton(GRID.X[5], GRID.Y[0],
    [this](auto* b) { // drawHandler
      if (data.isSongMode) {
        b->draw("SONG", data.useGridSongMode ? "GRID" : "LIVE", TouchButton::BUTTON_HIGHLIGHTED);
      }
      else {
        b->draw("PATT", "MODE", TouchButton::BUTTON_HIGHLIGHTED);
      }
    },
    [this](auto* b) { // clickedHandler (short press)
      const bool newIsSongMode = !data.isSongMode;
      currentPage = newIsSongMode ? PAGE_SONG : PAGE_PATTERN;

      if (data.useGridSongMode && !newIsSongMode) {
        gridEditor.exit();
      }

      if (isModeToolActive()) {
        currentTools = newIsSongMode ? TOOLS_SONG : TOOLS_PATTERN;
      }
      data.isSongMode = newIsSongMode;
      instance->redrawScreen();
    },
    [this](auto* b) { // longPressHandler
      if (data.isSongMode) {
        if (data.useGridSongMode) {
          gridEditor.enter();
        }
        else {
          data.stepRecordTargetTrack = data.activeTrack;
          // Open piano roll editor for linear song mode on active track
          openScreen(UI_func_liveseq_graphic, SONG_MODE_NOTES_TRACK);
        }
      }
    });

  // TOOL MENU
  buttonsToolSelect.push_back(new TouchButton(GRID.X[0], GRID.Y[2],
    [this](auto* b) { // drawHandler
      b->draw("TOOL", (data.isSongMode) ? "SNG" : "PAT", instance->isModeToolActive() ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    },
    [this](auto* b) { // clickedHandler
      instance->selectTools((data.isSongMode) ? TOOLS_SONG : TOOLS_PATTERN);
    }));

  buttonsToolSelect.push_back(new TouchButton(GRID.X[1], GRID.Y[2],
    [this](auto* b) { // drawHandler
      b->draw("TOOL", "ARP", currentTools == TOOLS_ARP ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    },
    [this](auto* b) { // clickedHandler
      instance->selectTools(TOOLS_ARP);
    }));

  buttonsToolSelect.push_back(new TouchButton(GRID.X[2], GRID.Y[2],
    [this](auto* b) { // drawHandler
      b->draw("TOOL", "SEQ", currentTools == TOOLS_SEQ ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    },
    [this](auto* b) { // clickedHandler
      instance->selectTools(TOOLS_SEQ);
    }));

  // Keyboard tool 
  buttonsToolSelect.push_back(new TouchButton(GRID.X[3], GRID.Y[2],
    [this](auto* b) { // drawHandler
      b->draw("TOOL", "KEY", currentTools == TOOLS_KEYBOARD ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    },
    [this](auto* b) { // clickedHandler
      instance->selectTools(TOOLS_KEYBOARD);
    }));


  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[0], GRID.Y[3],
    [](auto* b) { // drawHandler
      b->draw("TRACK", "SETUP", TouchButton::BUTTON_LABEL);
    }));

  selectedTrackSetup.label = new TouchButton(GRID.X[1], GRID.Y[3],
    [this](auto* b) { // drawHandler
      char temp_char[2];
      itoa(data.activeTrack + 1, temp_char, 10);
      b->draw("TRACK", temp_char, TouchButton::BUTTON_LABEL);
    });
  toolsPages[TOOLS_SEQ].push_back(selectedTrackSetup.label);

  // track instrument
  ValueButtonRange<uint8_t>* currentTrackInstrument = new ValueButtonRange<uint8_t>(&currentValue, GRID.X[3], GRID.Y[3], selectedInstrument, 0, 15, 1, data.trackSettings[data.activeTrack].instrument,
    [this](auto* b, auto* v) { // drawHandler
      char name[10];
      char sub[10];

      // Handle instrument selection based on device type
      if (selectedTrackSetup.device == LiveSequencer::DEVICE_INTERNAL) {
        // For internal devices: use instrumentToInstrumentID mapping
        selectedTrackSetup.instrument = instrumentToInstrumentID[selectedInstrument];
        const uint8_t maxInstrument = LiveSequencer::INSTR_MAX - 1;
        static_cast<EditableValueRange<uint8_t>*>(v)->changeRange(0, maxInstrument);
        liveSeq.getInstrumentName(selectedTrackSetup.device, selectedTrackSetup.instrument, name, sub);
      }
      else {
        // For external MIDI devices: 
        selectedTrackSetup.instrument = selectedInstrument;
        static_cast<EditableValueRange<uint8_t>*>(v)->changeRange(0, 15);
        liveSeq.getInstrumentName(selectedTrackSetup.device, selectedTrackSetup.instrument, name, sub);
      }

      b->draw(name, sub, TouchButton::BUTTON_ACTIVE);
      instance->checkApplyTrackInstrument();
    });
  toolsPages[TOOLS_SEQ].push_back(currentTrackInstrument);

  // track device
  toolsPages[TOOLS_SEQ].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[2], GRID.Y[3], selectedTrackSetup.device, LiveSequencer::DEVICE_INTERNAL, LiveSequencer::DEVICE_MIDI_INT, 1, data.trackSettings[data.activeTrack].device,
    [this, currentTrackInstrument](auto* b, auto* v) { // drawHandler
      char name[10];
      char sub[10];
      liveSeq.getDeviceName(v->getValue(), name, sub);
      b->draw(name, sub, TouchButton::BUTTON_ACTIVE);
      currentTrackInstrument->drawNow();
      instance->checkApplyTrackInstrument();
    }));

  // track quantize
  toolsPages[TOOLS_SEQ].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[4], GRID.Y[3],
    selectedTrackSetup.quantizeDenom, { 1, 64, 32, 24, 16, 12, 8, 4 }, data.trackSettings[data.activeTrack].quantizeDenom,
    [this](auto* b, auto* v) { // drawHandler
      char quantText[10];
      uint8_t value = v->getValue();

      if (value == 1) {
        strcpy(quantText, "NONE");
      }
      else if (value == 64) {
        strcpy(quantText, "1/64");
      }
      else if (value == 32) {
        strcpy(quantText, "1/32");
      }
      else if (value == 24) {
        strcpy(quantText, "1/16T");
      }
      else if (value == 16) {
        strcpy(quantText, "1/16");
      }
      else if (value == 12) {
        strcpy(quantText, "1/8T");
      }
      else if (value == 8) {
        strcpy(quantText, "1/8");
      }
      else if (value == 4) {
        strcpy(quantText, "1/4");
      }
      else {
        snprintf(quantText, sizeof(quantText), "%i", value);
      }

      b->draw("QUANT", quantText, (v->getValue() == 1) ? TouchButton::BUTTON_NORMAL : TouchButton::BUTTON_ACTIVE);
      if (data.trackSettings[data.activeTrack].quantizeDenom != selectedTrackSetup.quantizeDenom) {
        data.trackSettings[data.activeTrack].quantizeDenom = selectedTrackSetup.quantizeDenom;
        data.trackSettings[data.activeTrack].quantizeGrid = LiveSequencer::mapQuantizeDenom(selectedTrackSetup.quantizeDenom);
        instance->drawUpdates(drawActiveTrackSubLabel);
      }
    }));

  // track velocity
  toolsPages[TOOLS_SEQ].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[5], GRID.Y[3], selectedTrackSetup.velocity, 0, 100, 5, data.trackSettings[data.activeTrack].velocityLevel,
    [this](auto* b, auto* v) { // drawHandler
      b->draw("VELOCTY", (v->getValue() == 0) ? "KEY" : v->toString() + std::string("%"), (v->getValue() == 0) ? TouchButton::BUTTON_NORMAL : TouchButton::BUTTON_ACTIVE);
      if (data.trackSettings[data.activeTrack].velocityLevel != selectedTrackSetup.velocity) {
        data.trackSettings[data.activeTrack].velocityLevel = selectedTrackSetup.velocity;
        instance->drawUpdates(drawActiveTrackSubLabel);
      }
    }));

  // jump to other pages
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[0], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("JUMP", "PAGE", TouchButton::BUTTON_LABEL);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[1], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("FILE", "MANAGER", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      openScreen(UI_func_file_manager);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[2], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("MASTER", "EFFECTS", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      openScreen(UI_func_master_effects);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[3], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("SIDE", "CHAIN", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      openScreen(UI_func_sidechain);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[4], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("MULTI", "BAND", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      openScreen(UI_func_multiband_dynamics);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[5], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("SEQ", "SETTING", TouchButton::BUTTON_ACTIVE);
    },
    [](auto* b) { // clickedHandler
      // open sequencer settings
      openScreen(UI_func_seq_settings);
    }));

  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[0], GRID.Y[5],
    [](auto* b) { // drawHandler
      b->draw("ACTIONS", "", TouchButton::BUTTON_LABEL);
    }));

  TouchButton* confirmDelete = new TouchButton(GRID.X[2], GRID.Y[5],
    [this](auto* b) { // drawHandler
      if (deleteConfirming) {
        b->draw("DO IT", "!", TouchButton::BUTTON_RED);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    },
    [this](auto* b) { // clickedHandler
      // really delete
      if (deleteConfirming) {
        deleteConfirming = false;
        if (data.isSongMode) {
          liveSeq.deleteAllSongEvents();
        }
        else {
          liveSeq.deleteLiveSequencerData();
        }
        b->draw("DELETE", "OK", TouchButton::BUTTON_LABEL);
      }
    });
  toolsPages[TOOLS_SEQ].push_back(confirmDelete);
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[1], GRID.Y[5],
    [this](auto* b) { // drawHandler
      b->draw("DELETE", data.isSongMode ? "SONG" : "ALL", TouchButton::BUTTON_ACTIVE);
    },
    [this, confirmDelete](auto* b) { // clickedHandler
      deleteConfirming = !deleteConfirming;
      confirmDelete->drawNow();
    }));

  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[3], GRID.Y[5],
    [](auto* b) { // drawHandler
      b->draw("NAME", "PERF", TouchButton::BUTTON_ACTIVE);
    },
    [](auto* b) { // clickedHandler
      // name
      openScreen(UI_func_set_performance_name);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[4], GRID.Y[5],
    [](auto* b) { // drawHandler
      b->draw("LOAD", "PERF", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      // load
      openScreen(UI_func_load_performance, data.performanceID);
    }));
  toolsPages[TOOLS_SEQ].push_back(new TouchButton(GRID.X[5], GRID.Y[5],
    [](auto* b) { // drawHandler
      b->draw("SAVE", "PERF", TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* b) { // clickedHandler
      // save
      openScreen(UI_func_save_performance, data.performanceID);
    }));

  // PATTERN TOOLS
  TouchButton* applyPatternLength = new TouchButton(GRID.X[2], GRID.Y[3],
    [this](auto* b) { // drawHandler
      const bool isSame = (data.numberOfBars == numberOfBarsTemp);
      b->draw("APPLY", "NOW", isSame ? TouchButton::BUTTON_NORMAL : TouchButton::BUTTON_RED);
      display.setTextSize(1);
      display.setTextColor(isSame ? COLOR_SYSTEXT : RED, COLOR_BACKGROUND);
      display.setCursor(GRID.X[3] + 2, GRID.Y[3] + 5);
      display.print(F("CHANGING PATTERN LENGTH"));
      display.setCursor(GRID.X[3] + 2, GRID.Y[3] + 20);
      display.print(F("WILL DELETE ALL DATA!"));
    },
    [this](auto* b) { // clickedHandler
      if (data.numberOfBars != numberOfBarsTemp) {
        liveSeq.stop();
        liveSeq.changeNumberOfBars(numberOfBarsTemp);
        b->drawNow();
      }
    });
  toolsPages[TOOLS_PATTERN].push_back(applyPatternLength);
  toolsPages[TOOLS_PATTERN].push_back(new TouchButton(GRID.X[0], GRID.Y[3],
    [](auto* b) { // drawHandler
      b->draw("PATTERN", "LENGTH", TouchButton::BUTTON_LABEL);
    }));
  toolsPages[TOOLS_PATTERN].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[1], GRID.Y[3], numberOfBarsTemp, { 1, 2, 4, 8 }, 4,
    [applyPatternLength](auto* b, auto* v) { // drawHandler
      b->draw("LENGTH", v->toString(), TouchButton::BUTTON_ACTIVE);
      applyPatternLength->drawNow();
    }));

  // fill notes
  toolsPages[TOOLS_PATTERN].push_back(new TouchButton(GRID.X[0], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("FILL", "NOTES", TouchButton::BUTTON_LABEL);
    }));

  toolsPages[TOOLS_PATTERN].push_back(new TouchButton(GRID.X[0], GRID.Y[5],
    [](auto* b) { // drawHandler
      b->draw("COUNT", "IN", TouchButton::BUTTON_LABEL);
    }));

  toolsPages[TOOLS_PATTERN].push_back(new TouchButton(GRID.X[1], GRID.Y[5],
    [](auto* b) { // drawHandler

      b->draw("ADD", "METRONM", TouchButton::BUTTON_ACTIVE);
      metronome_text_pos();
      display.print(F("TOUCH TO ADD METRONOME"));
    },
    [this](auto* b) { // clickedHandler

      liveSeq.start();
      delay(50);
      liveSeq.stop();
      delay(20);
      metronome_text_pos();

      if (seq.running == false) {
        liveSeq.AddMetronome();
        display.print(F("METRONOME TRACK ADDED  "));
        delay(50);
      }
    }));

  lastNoteLabel = new TouchButton(GRID.X[1], GRID.Y[4],
    [this](auto* b) { // drawHandler
      char temp_char[4];
      b->draw("NOTE", itoa(data.lastPlayedNote, temp_char, 10), TouchButton::BUTTON_LABEL);
    });
  toolsPages[TOOLS_PATTERN].push_back(lastNoteLabel);

  toolsPages[TOOLS_PATTERN].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[2], GRID.Y[4], data.fillNotes.number, { 4, 6, 8, 12, 16, 24, 32 }, 16,
    [](auto* b, auto* v) { // drawHandler
      b->draw("NUMBER", v->toString(), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_PATTERN].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[3], GRID.Y[4], data.fillNotes.offset, 0, 7, 1, 0,
    [](auto* b, auto* v) { // drawHandler
      b->draw("OFFSET", v->toString(), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_PATTERN].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[4], GRID.Y[4], data.fillNotes.velocityLevel, 0, 100, 5, 100,
    [](auto* b, auto* v) { // drawHandler
      b->draw("VELOCTY", v->toString() + std::string("%"), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_PATTERN].push_back(new TouchButton(GRID.X[5], GRID.Y[4],
    [](auto* b) { // drawHandler
      b->draw("FILL", "NOW", TouchButton::BUTTON_RED);
    },
    [this](auto* b) { // clickedHandler
      liveSeq.fillTrackLayer();
    }));


  // SONG TOOLS

  // SONG TOOLS - with song mode selection at the top
  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[0], GRID.Y[3],
    [](auto* b) { // drawHandler
      b->draw("SELECT", "MODE", TouchButton::BUTTON_LABEL);
    }));

  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[1], GRID.Y[3],
    [this](auto* b) { // drawHandler
      // Only highlight if this is the currently selected mode
      b->draw("LIVE", "RECORD", !data.useGridSongMode ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    },
    [this](auto* b) { // clickedHandler
      data.useGridSongMode = false;
      // Redraw all buttons in this tool to update their states
      for (auto* btn : toolsPages[TOOLS_SONG]) {
        btn->drawNow();
      }
    }));

  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[2], GRID.Y[3],
    [this](auto* b) { // drawHandler
      // Only highlight if this is the currently selected mode
      b->draw("GRID", "EDITOR", data.useGridSongMode ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);

      display.setTextSize(1);
      display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
      uint8_t xpos = CHAR_width_small * 28;

      if (data.useGridSongMode)
      {

        // Clear the song layer buttons when entering grid editor
        for (int songLayer = 0; songLayer < 4; songLayer++) {
          TouchButton::clearButton(GRID.X[2 + songLayer], GRID.Y[5], COLOR_BACKGROUND);
        }

        display.setCursor(xpos, CHAR_height_small * 17 - 4);
        display.print(F("ARRANGE THE PATTERNS +"));
        display.setCursor(xpos, CHAR_height_small * 18 - 2);
        display.print(F("LAYERS  IN YOUR SONG  "));
        display.setCursor(xpos, CHAR_height_small * 19);
        display.print(F("WITH A GLOBAL TOUCH"));
        display.setCursor(xpos, CHAR_height_small * 20 + 3);
        display.print(F("GRID/MATRIX STRUCTURE"));  //phtodo
      }
      else
      {
        display.setCursor(xpos, CHAR_height_small * 17 - 4);
        display.print(F("RECORD MUTE AUTOMATION"));
        display.setCursor(xpos, CHAR_height_small * 18 - 2);
        display.print(F("IN REAL TIME. YOU ALSO"));
        display.setCursor(xpos, CHAR_height_small * 19);
        display.print(F("CAN RECORD 4 GLOBAL,"));
        display.setCursor(xpos, CHAR_height_small * 20 + 3);
        display.print(F("LINEAR MIDI TRACKS.  "));
      }
    },
    [this](auto* b) { // clickedHandler
      data.useGridSongMode = true;
      // Redraw all buttons in this tool to update their states
      for (auto* btn : toolsPages[TOOLS_SONG]) {

        btn->drawNow();
      }
    }));

  // Grid mode button - only visible when grid mode is active
  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[5], GRID.Y[5],
    [this](auto* b) { // drawHandler
      if (data.useGridSongMode) {
        b->draw("ENTER", "GRID", TouchButton::BUTTON_ACTIVE);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    },
    [this](auto* b) { // clickedHandler
      if (data.useGridSongMode) {
        gridEditor.enter();
      }
    }));

  // MUTE QUANT - only visible in live mode
  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[0], GRID.Y[4],
    [this](auto* b) { // drawHandler
      if (!data.useGridSongMode) {
        b->draw("MUTE", "QUANT", TouchButton::BUTTON_LABEL);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    }));

  toolsPages[TOOLS_SONG].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[1], GRID.Y[4],
    data.songMuteQuantizeDenom, { 1, 2, 4, 8 }, 1,
    [this](auto* b, auto* v) { // drawHandler
      if (!data.useGridSongMode) {
        b->draw("QUANT", (v->getValue() == 1) ? "NONE" : v->toString(), (v->getValue() == 1) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_HIGHLIGHTED);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    }));

  // SONG LAYERS - only visible in live mode
  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[0], GRID.Y[5],
    [this](auto* b) { // drawHandler
      if (!data.useGridSongMode) {
        b->draw("SONG", "LAYERS", TouchButton::BUTTON_LABEL);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    }));

  toolsPages[TOOLS_SONG].push_back(new TouchButton(GRID.X[1], GRID.Y[5],
    [this](auto* b) { // drawHandler
      if (!data.useGridSongMode) {
        std::string t1 = (data.songLayerCount == 0) ? "NO" : "LAYER";
        std::string t2 = (data.songLayerCount == 0) ? "LAYERS" : "ACTION";
        b->draw(t1, t2, (data.songLayerCount == 0) ? TouchButton::BUTTON_LABEL : TouchButton::BUTTON_ACTIVE);
        instance->drawUpdates(drawSongLayers);
      }
      else {
        b->clear(COLOR_BACKGROUND);
      }
    }, [this](auto* b) { // clickedHandler
      if (!data.useGridSongMode && data.songLayerCount > 0) {
        if (++songLayerMode == LiveSequencer::LayerMode::LAYER_MODE_NUM) {
          songLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
        }
        instance->drawUpdates(drawSongLayers);
      }
      }));

  // ARP TOOL
  toolsPages[TOOLS_ARP].push_back(new TouchButton(GRID.X[0], GRID.Y[3],
    [this](auto* b) { // drawHandler
      b->draw("ENABLE", (data.arpSettings.enabled == 0) ? "OFF" : "ON", (data.arpSettings.enabled == 0) ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_HIGHLIGHTED);
    }, [this](auto* b) { // clickedHandler
      liveSeq.setArpEnabled(!data.arpSettings.enabled);
      b->drawNow();
      }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[1], GRID.Y[3], data.arpSettings.amount, { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 }, 8,
    [](auto* b, auto* v) { // drawHandler
      b->draw("SPEED", v->toString(), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[2], GRID.Y[3], data.arpSettings.octaves, { 1, 2, 3, 4 }, 1,
    [](auto* b, auto* v) { // drawHandler
      b->draw("OCTAVES", v->toString(), TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* v) { // changedHandler
      data.arpSettings.arpSettingsChanged = true;
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[3], GRID.Y[3], (uint8_t&)data.arpSettings.mode, 0, uint8_t(LiveSequencer::ARP_MODENUM - 1), 1, uint8_t(LiveSequencer::ARP_UP),
    [](auto* b, auto* v) { // drawHandler
      b->draw("MODE", UI_LiveSequencer::getArpModeName(v->getValue()).c_str(), TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* v) { // changedHandler
      data.arpSettings.arpSettingsChanged = true;
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint16_t>(&currentValue, GRID.X[4], GRID.Y[3], data.arpSettings.length, 10, 420, 10, 150,
    [](auto* b, auto* v) { // drawHandler
      b->draw("LENGTH", v->toString(), v->getValue() == 0 ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<int8_t>(&currentValue, GRID.X[5], GRID.Y[3], data.arpSettings.swing, -8, 8, 1, 0,
    [](auto* b, auto* v) { // drawHandler
      b->draw("SWING", v->toString(), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[0], GRID.Y[4], data.arpSettings.source, 0, uint8_t(LiveSequencer::LIVESEQUENCER_NUM_TRACKS), 1, 0,
    [](auto* b, auto* v) { // drawHandler
      b->draw("SOURCE", (v->getValue() == 0) ? "KEY" : std::string("TK") + v->toString(), TouchButton::BUTTON_ACTIVE);
    },
    [this](auto* v) { // changedHandler
      liveSeq.onArpSourceChanged();
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[1], GRID.Y[4], data.arpSettings.velocityLevel, 0, 100, 5, 100,
    [](auto* b, auto* v) { // drawHandler
      b->draw("VELOCTY", v->toString() + std::string("%"), TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[2], GRID.Y[4], data.arpSettings.latch, 0, 1, 1, 1,
    [](auto* b, auto* v) { // drawHandler
      b->draw("LATCH", v->getValue() == 1 ? "ON" : "-", TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonVector<uint8_t>(&currentValue, GRID.X[3], GRID.Y[4], data.arpSettings.loadPerBar, { 1, 2, 4 }, 2,
    [](auto* b, auto* v) { // drawHandler
      b->draw("SAMPLE", std::string(v->toString()) + "x", TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[4], GRID.Y[4], data.arpSettings.noteRepeat, 0, 4, 1, 0,
    [](auto* b, auto* v) { // drawHandler
      b->draw("REPEAT", std::string(v->toString()) + "x", TouchButton::BUTTON_ACTIVE);
    }));
  toolsPages[TOOLS_ARP].push_back(new ValueButtonRange<uint8_t>(&currentValue, GRID.X[5], GRID.Y[4], data.arpSettings.freerun, 0, 1, 1, 0,
    [](auto* b, auto* v) { // drawHandler
      b->draw("FREERUN", v->getValue() == 1 ? "ON" : "-", TouchButton::BUTTON_ACTIVE);
    }));


  // Grid song editor UI handled by GridSongEditorUI
}

FLASHMEM void UI_LiveSequencer::selectTools(Tools tools) {
  if (currentTools != tools) {
    if (currentValue.button != nullptr) {
      currentValue.button->setSelected(false);
    }
    currentValue.valueBase = nullptr;

    // Handle keyboard visible changed
    const bool keyboardVisibleChanged = (currentTools == TOOLS_KEYBOARD) || (tools == TOOLS_KEYBOARD);
    if (keyboardVisibleChanged) {
      seq.cycle_touch_element = (tools == TOOLS_KEYBOARD);
    }

    currentTools = tools;
    clearBottomArea();
    drawUpdates(drawTools);

    // Force immediate redraw for the new tool
    drawGUI(guiUpdateFlags);
  }
}

FLASHMEM bool UI_LiveSequencer::isModeToolActive(void) {
  bool result = false;
  result |= (currentTools == TOOLS_PATTERN) && (data.isSongMode == false);
  result |= (currentTools == TOOLS_SONG) && (data.isSongMode == true);
  return result;
}

FLASHMEM void UI_LiveSequencer::resetProgressBars(void) {
  barPattern.currentPhase = 0;
  barPattern.drawnLength = 0;
  barTotal.currentPhase = 0;
  barTotal.drawnLength = 0;
}

FLASHMEM void UI_LiveSequencer::onStopped(void) {
  if (isVisible) {
    resetProgressBars();
    drawUpdates(drawActiveNotes | drawTime);
    drawGUI(guiUpdateFlags);
  }
}

bool disableDirectMIDIinput;

FLASHMEM void UI_LiveSequencer::processLCDM(void) {
  // ****** SETUP *********
  if (LCDML.FUNC_setup()) {
    registerTouchHandler(handle_touchscreen_live_sequencer);
    isVisible = true;
    data.processMidiIn = true;
    display.fillScreen(COLOR_BACKGROUND);
    numberOfBarsTemp = data.numberOfBars;
    liveSeq.onGuiInit();
    updateTrackChannelSetupButtons(data.activeTrack); // update GUI for the case a new perf was loaded
    disableDirectMIDIinput = true;

    // currentPage = PAGE_PATTERN;

    drawPerformanceName();

    // Reset grid editor state
    gridEditor.exit();
    gridEditor.requestRedraw();
    data.redraw_grid_header = true;
    data.redraw_grid_buttons = true;
    resetProgressBars();
    redrawScreen();

    // setup function
    LCDML.FUNC_setLoopInterval(DRAW_INTERVAL); // 40Hz gui refresh
  }
  // ****** LOOP *********
  if (LCDML.FUNC_loop()) {

    const EncoderEvents e = getEncoderEvents(ENC_R);
    const EncoderEvents e_left = getEncoderEvents(ENC_L);

    if (gridEditor.isActive() && e_left.pressed) {
      gridEditor.exitToLiveSequencer();
    }

    msCount += DRAW_INTERVAL;
    if (msCount >= 200) {
      drawUpdates(drawCpuLoad);
      msCount = 0;
    }
    if (gridEditor.isActive()) {
      gridEditor.handleEncoder(e);
    }
    else {
      if (e.down) {
        if (currentValue.valueBase != nullptr) {
          currentValue.valueBase->next();
        }
        else if ((trackOffset == 0) && LiveSequencer::LIVESEQUENCER_NUM_TRACKS > LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN) {
          trackOffset = LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN;
          drawUpdates(drawTrackButtons);
          if (isLayerViewActive) {
            clearBottomArea();
            drawUpdates(drawLayerButtons);
          }
        }
      }

      if (e.up) {
        if (currentValue.valueBase != nullptr) {
          currentValue.valueBase->previous();
        }
        else if (trackOffset == LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN) {
          trackOffset = 0;
          drawUpdates(drawTrackButtons);
          if (isLayerViewActive) {
            clearBottomArea();
            drawUpdates(drawLayerButtons);
          }
        }
      }
    }
    if (e.pressed) {
      if (currentValue.valueBase != nullptr) {
        currentValue.button->setSelected(false);
        currentValue.valueBase = nullptr;
      }
    }

    // Handle different screen states
    if (gridEditor.isActive()) {
      gridEditor.tick();
    }
    else {
      // Normal LiveSequencer operation
      drawUpdates(data.isRunning ? (drawActiveNotes | drawTime) : 0);
      if ((isLayerViewActive == false) && (currentTools == TOOLS_PATTERN) && (guiUpdateFlags & drawLastPlayedNote)) {
        lastNoteLabel->drawNow();
      }

      if (showingHowTo == false) {
        drawGUI(guiUpdateFlags);
      }
    }

  }

  // ****** STABLE END *********
  if (LCDML.FUNC_close()) {
    unregisterTouchHandler();
    data.processMidiIn = openingScreen;
    openingScreen = false;
    isVisible = false;
    data.isRecording = false;
    showingHowTo = false;

    gridEditor.exit();
    gridEditor.requestRedraw();
    data.redraw_grid_buttons = true;

    // disableDirectMIDIinput=false;


    display.fillScreen(COLOR_BACKGROUND);
  }
}

bool UI_LiveSequencer::isGridEditorActive() const {
  return gridEditor.isActive();
}

FLASHMEM void UI_LiveSequencer::notifyLeftEncoderShort() {
  if (gridEditor.isActive()) {
    gridEditor.exitToLiveSequencer();
  }
}

FLASHMEM void UI_LiveSequencer::onGridStepChanged(void) {
  gridEditor.handleStepChanged();
}

FLASHMEM void UI_LiveSequencer::onGridPlaybackEnded(void) {
  gridEditor.handlePlaybackEnded();
}

FLASHMEM void UI_LiveSequencer::drawUpdates(uint16_t flags) {
  guiUpdateFlags |= flags;
}

FLASHMEM void UI_LiveSequencer::clearBottomArea(void) {
  display.console = true;
  display.fillRect(0, GRID.Y[2], DISPLAY_WIDTH, DISPLAY_HEIGHT - GRID.Y[2], COLOR_BACKGROUND);


  // Reset grid editor state when clearing bottom area
  if (gridEditor.isActive()) {
    gridEditor.exit();
    data.isSongMode = false; // Return to pattern mode
    currentPage = PAGE_PATTERN;
  }
}

FLASHMEM void UI_LiveSequencer::redrawScreen(void) {
  drawUpdates(drawTopButtons | drawTrackButtons | drawTime);
  isLayerViewActive = (showingTools == false);
  if (isLayerViewActive) {
    drawUpdates(drawLayerButtons);
  }
  else {
    drawUpdates(drawTools);
  }
  clearBottomArea();
}

FLASHMEM void UI_LiveSequencer::openScreen(LCDML_FuncPtr_pu8 screen, uint8_t param) {
  // stay active in background and keep processing midi inputs for screens opened in LiveSequencer
  openingScreen = true;

  LCDML.FUNC_setGBAToLastFunc();
  disableDirectMIDIinput = true;
  LCDML.OTHER_jumpToFunc(screen, param);
}

FLASHMEM void UI_LiveSequencer::onTrackButtonPressed(uint8_t track) {
  if (track == data.activeTrack) {
    if (data.isRecording) {
      if (data.pendingEvents.size()) {
        data.pendingEvents.clear(); // clear pending
      }
      else {
        // track layer actions only for pattern mode
        if (currentPage == PAGE_PATTERN) {
          if (++trackLayerMode == LiveSequencer::LayerMode::LAYER_MODE_NUM) {
            trackLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
          }
          drawTrackLayers(track);
        }
      }
    }
    else {
      // open instrument settings
      if (data.tracks[track].screenSetupFn != nullptr) {
        SetupFn f = (SetupFn)data.tracks[track].screenSetupFn;
        f(0);
      }
      openScreen(data.tracks[track].screen);
    }
  }
  else {
    const uint8_t activeOld = data.activeTrack;
    liveSeq.setActiveTrack(track);
    trackButtons[SCREEN_TRACK_INDEX(activeOld)]->drawNow();
    trackButtons[SCREEN_TRACK_INDEX(track)]->drawNow();

    if (showingTools && (currentTools == TOOLS_KEYBOARD)) {
      const bool isDrumsActive = data.trackSettings[data.activeTrack].instrument == LiveSequencer::INSTR_DRUM;
      // only redraw if PADs / KEYs view needs redrawing
      if (ts.current_virtual_keyboard_display_mode != isDrumsActive) {
        drawKeyboard(VK_DRAW_KEYS);
      }
    }
    if (trackLayerMode != LiveSequencer::LayerMode::LAYER_MUTE) {
      drawTrackLayers(activeOld);
      trackLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
    }
    DBG_LOG(printf("active track now is %i\n", track + 1));
  }
}

FLASHMEM void UI_LiveSequencer::updateTrackChannelSetupButtons(uint8_t track) {
  selectedTrackSetup.device = data.trackSettings[track].device;
  selectedTrackSetup.instrument = data.trackSettings[track].instrument;
  selectedTrackSetup.quantizeDenom = data.trackSettings[track].quantizeDenom;
  selectedTrackSetup.velocity = data.trackSettings[track].velocityLevel;

  // Set selectedInstrument based on device type
  if (selectedTrackSetup.device == LiveSequencer::DEVICE_INTERNAL) {
    // For internal devices: reverse lookup selected instrument index
    for (size_t i = 0; i < instrumentToInstrumentID.size(); i++) {
      if (instrumentToInstrumentID[i] == selectedTrackSetup.instrument) {
        selectedInstrument = i;
        break;
      }
    }
  }
  else {
    // For external MIDI devices
    selectedInstrument = selectedTrackSetup.instrument;
  }

  // update currently selected track
  if ((isLayerViewActive == false) && (currentTools == TOOLS_SEQ)) {
    drawUpdates(drawTools);
  }
}

FLASHMEM void UI_LiveSequencer::drawKeyboard(uint8_t flags) {
  const bool isDrumsActive = data.trackSettings[data.activeTrack].instrument == LiveSequencer::INSTR_DRUM;
  ts.current_virtual_keyboard_display_mode = isDrumsActive ? 1 : 0;
  ts.virtual_keyboard_instrument = VK_LIVESEQUENCER;
  drawVirtualKeyboard(flags);
}

FLASHMEM void UI_LiveSequencer::handleTouchscreen(void) {
  if (showingHowTo) {
    if (TouchButton::isPressed(GRID.X[5], GRID.Y[5])) {
      openScreen(UI_func_midi_channels);
      data.processMidiIn = false; // do not process midi in background
    }
    return;
  }

  // Handle grid song editor touch
  if (gridEditor.isActive()) {
    gridEditor.handleTouch();
    return;
  }

  if (showingTools && (currentTools == TOOLS_KEYBOARD)) {
    handleTouchVirtualKeyboard();
  }

  const bool runningChanged = (runningHere != data.isRunning);
  runningHere = data.isRunning;

  const bool runningPressed = TouchButton::isPressed(GRID.X[0], GRID.Y[0]);
  if (runningPressed) {
    data.seq_started_from_pattern_mode = true;
    if (runningHere) {
      liveSeq.stop();
    }
    else {
      if (data.isRecording) {
        liveSeq.startCountIn(); // pressing start with record activated does a count in
        drawUpdates(drawTopButtons);
      }
      else {
        liveSeq.start();
      }
    }
  }

  if (runningPressed || runningChanged) {
    drawUpdates(drawTopButtons);
    if (trackLayerMode != LiveSequencer::LayerMode::LAYER_MUTE) {
      trackLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
      drawTrackLayers(data.activeTrack);
    }
  }

  const bool recPressed = TouchButton::isPressed(GRID.X[1], GRID.Y[0]);
  if (recPressed) {
    if (trackLayerMode != LiveSequencer::LayerMode::LAYER_MUTE) {
      trackLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
      drawTrackLayers(data.activeTrack);
    }
    data.isRecording = !data.isRecording;
    drawUpdates(drawTopButtons | drawTrackButtons);

    if (isLayerViewActive && data.isSongMode) {
      drawUpdates(drawLayerButtons);
    }
  }

  const bool toolsPressed = TouchButton::isPressed(GRID.X[4], GRID.Y[0]);
  if (toolsPressed) {
    if (gridEditor.isActive()) {
      gridEditor.exit();
    }
    // possible switch to song / pattern tools if mode changed
    currentTools = data.isSongMode && currentTools == TOOLS_PATTERN ? TOOLS_SONG : currentTools;
    currentTools = !data.isSongMode && currentTools == TOOLS_SONG ? TOOLS_PATTERN : currentTools;
    showingTools = !showingTools;
    if (showingTools == false) {
      if (currentValue.button != nullptr) {
        currentValue.button->setSelected(false);
      }
      currentValue.valueBase = nullptr;
    }
    redrawScreen();
  }

  // Process the mode button with long press support
  modeButton->processPressed();

  for (TouchButton* b : trackButtons) {
    b->processPressed();
  }

  if (isLayerViewActive) {
    for (uint8_t trackIndex = 0; trackIndex < LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN; trackIndex++) {
      const uint8_t track = trackOffset + trackIndex;
      for (uint8_t layer = 0; layer < data.trackSettings[track].layerCount; layer++) {
        const bool pressed = TouchButton::isPressed(GRID.X[trackIndex], GRID.Y[2 + layer]);
        if (pressed) {
          if (trackLayerMode != LiveSequencer::LayerMode::LAYER_MUTE) {
            if (data.isRecording && (track == data.activeTrack)) {
              bool layerCleared = false;
              const bool success = liveSeq.trackLayerAction(track, layer, LiveSequencer::LayerMode(trackLayerMode), layerCleared);
              if (success) {
                if (layerCleared) {
                  // one less layer now, clear last layer button
                  TouchButton::clearButton(GRID.X[SCREEN_TRACK_INDEX(trackIndex)], GRID.Y[2 + data.trackSettings[track].layerCount], COLOR_BACKGROUND);
                }
                trackLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
              }
            }
          }
          else {
            const bool isMutedOld = data.tracks[track].layerMutes & (1 << layer);
            const bool recordMuteToSong = data.isSongMode && data.isRecording && data.isRunning;
            liveSeq.setLayerMuted(track, layer, !isMutedOld, recordMuteToSong);
          }
        }
      }
    }
  }
  else {
    // process TOOLS MENU if tools view active
    for (auto* b : buttonsToolSelect) {
      b->processPressed();
    }
    for (TouchButton* b : toolsPages[currentTools]) {
      b->processPressed();
    }

    if (currentTools == TOOLS_SONG) {  //phtodo
      if (songLayerMode != LiveSequencer::LayerMode::LAYER_MUTE && !data.useGridSongMode) { // song layers can not be muted
        for (uint8_t songLayer = 0; songLayer < data.songLayerCount; songLayer++) {
          if (TouchButton::isPressed(GRID.X[2 + songLayer], GRID.Y[5])) {
            liveSeq.songLayerAction(songLayer, LiveSequencer::LayerMode(songLayerMode));
            songLayerMode = LiveSequencer::LayerMode::LAYER_MUTE;
            TouchButton::clearButton(GRID.X[2 + data.songLayerCount], GRID.Y[5], COLOR_BACKGROUND);
            drawUpdates(drawSongLayers);
            break;
          }
        }
      }
    }
  }
}

FLASHMEM void UI_LiveSequencer::drawBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  // this way it looks good on console and on MDT...
  for (uint8_t yloc = y; yloc < (y + h); yloc++) {
    display.drawLine(x, yloc, x + w, yloc, color);
  }
}

FLASHMEM void UI_LiveSequencer::processBar(const float progress, const uint16_t y, ProgressBar& bar, const uint16_t color) {
  const uint8_t totalBarWidth = progress * BAR_WIDTH;
  int16_t drawWidth = totalBarWidth - bar.drawnLength;

  if (drawWidth < 0) { // bar ended
    drawBar(GRID.X[2] + bar.drawnLength, y, BAR_WIDTH - bar.drawnLength, BAR_HEIGHT, bar.currentPhase ? color : GREY2);
    bar.currentPhase = !bar.currentPhase;
    bar.drawnLength = 0;
    drawWidth = totalBarWidth;
  }

  if (drawWidth > 0) {
    drawBar(GRID.X[2] + bar.drawnLength, y, drawWidth, BAR_HEIGHT, bar.currentPhase ? color : GREY2);
    bar.drawnLength = totalBarWidth;
  }
}

FLASHMEM void UI_LiveSequencer::drawPerformanceName(void) {
  if (remote_active) {
    display.console = true;
  }

  const uint16_t textX = GRID.X[2];
  const uint16_t textY = GRID.Y[0] + 6;
  const uint16_t textWidth = (2 * TouchButton::BUTTON_SIZE_X) + TouchButton::BUTTON_SPACING;
  const uint16_t textHeight = CHAR_height_small + 2;

  display.fillRect(textX, textY, textWidth, textHeight, COLOR_BACKGROUND);
  display.setCursor(textX, textY);
  display.setTextSize(1);
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.print(data.performanceName.c_str());
}

FLASHMEM void UI_LiveSequencer::drawGUI(uint16_t& guiFlags) {
  if (remote_active) {
    display.console = true;
  }

  if (guiFlags & drawCpuLoad && !gridEditor.isActive()) {
    static constexpr float size = 2 * TouchButton::BUTTON_SIZE_X + TouchButton::BUTTON_SPACING;
    uint16_t color = GREEN;
    const uint8_t cpuLoad = AudioProcessorUsage();
    const uint16_t barWidth = cpuLoad * size / float(100);
    if (cpuLoad > 90) {
      color = RED;
    }
    else if (cpuLoad > 75) {
      color = COLOR_ARP;
    }
    if (remote_active) {
      display.console = true;
    }
    display.drawLine(GRID.X[2], 0, GRID.X[2] + barWidth, 0, color);
    if (remote_active) {
      display.console = true;
    }
    display.drawLine(GRID.X[2] + barWidth, 0, GRID.X[4] - TouchButton::BUTTON_SPACING, 0, GREY2);
  }

  if (guiFlags & drawTopButtons && !gridEditor.isActive()) {
    TouchButton::Color playColor = runningHere ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL;
    if (data.remainingCountIns > 0) {
      playColor = (data.remainingCountIns) % 2 ? TouchButton::BUTTON_NORMAL : TouchButton::BUTTON_HIGHLIGHTED;
    }
    TouchButton::drawButton(GRID.X[0], GRID.Y[0], (runningHere ? "STOP" : "START"), "", playColor);
    TouchButton::drawButton(GRID.X[1], GRID.Y[0], "REC", "", data.isRecording ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
    TouchButton::drawButton(GRID.X[4], GRID.Y[0], isLayerViewActive ? "LAYERS" : "TOOLS", "VIEW", TouchButton::BUTTON_NORMAL);

    // Use the mode button instead of manual drawing
    modeButton->drawNow();
    drawPerformanceName();
  }

  if (guiFlags & drawActiveTrackSubLabel && !gridEditor.isActive()) {
    drawTrackSubtext(data.activeTrack);
  }

  // print time
  if (guiFlags & drawTime && !gridEditor.isActive()) {
    uint16_t timeMs = data.isRunning ? uint16_t(data.patternTimer) : data.patternLengthMs;
    if (data.isRunning == false) {
      data.songPatternCount = data.lastSongEventPattern; // show song length
      data.currentPattern = data.numberOfBars - 1; // show num bars
    }

    const float progressPattern = timeMs / float(data.patternLengthMs);
    const float progressTotal = (progressPattern + data.currentPattern) / float(data.numberOfBars);

    processBar(progressPattern, 17, barPattern, GREEN);
    processBar(progressTotal, 22, barTotal, RED);

    uint16_t patCount = data.isRunning ? data.currentPattern : 0;
    display.setCursor(GRID.X[2], 28);
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
    if (data.isSongMode) {
      const uint32_t songMs = (data.songPatternCount * data.numberOfBars + data.currentPattern) * data.patternLengthMs + timeMs;
      const uint32_t minutes = songMs / 60000;
      const uint32_t seconds = (songMs % 60000) / 1000;
      const uint32_t millis = songMs % 1000;
      display.printf("S %i:%02i.%03i", minutes, seconds, millis);
    }
    else {
      display.printf("P %i.%04i   ", patCount, timeMs);
    }
    display.setCursor(200, 28);
    display.printf("%02i", data.isSongMode ? data.songPatternCount : data.currentPattern);
  }

  const bool doBlink = data.notesOn.size() || data.pendingEvents.size();
  if (doBlink && !gridEditor.isActive()) {
    if (guiCounter-- == 0) {
      guiCounter = 8;
      TouchButton::Color trackButtonRecColor = blinkPhase ? TouchButton::BUTTON_RED : TouchButton::BUTTON_HIGHLIGHTED;
      blinkPhase = !blinkPhase;
      char temp_char[4];
      trackButtons[SCREEN_TRACK_INDEX(data.activeTrack)]->draw(data.tracks[data.activeTrack].name, itoa(data.activeTrack + 1, temp_char, 10), trackButtonRecColor);
    }
  }
  else {
    guiCounter = 0;
    blinkPhase = 0;
  }

  if (guiUpdateFlags & drawTrackButtons && !gridEditor.isActive()) {
    DBG_LOG(printf("draw tracks\n"));
    for (TouchButton* b : trackButtons) {
      b->drawNow();
    }
  }

  if (isLayerViewActive && !gridEditor.isActive()) {
    const bool isSongRec = (data.isSongMode && data.isRecording);
    const bool drawAllLayers = guiFlags & drawLayerButtons;

    for (uint8_t trackIndex = 0; trackIndex < LiveSequencer::LIVESEQUENCER_TRACKS_PER_SCREEN; trackIndex++) {
      const uint8_t track = trackOffset + trackIndex;
      const bool layerEditActive = !data.isSongMode && (data.activeTrack == track) && (trackLayerMode != LiveSequencer::LayerMode::LAYER_MUTE);

      // layer button
      for (uint8_t layer = 0; layer < data.trackSettings[track].layerCount; layer++) {
        const bool isMuted = data.tracks[track].layerMutes & (1 << layer);
        TouchButton::Color color = (isMuted ? TouchButton::BUTTON_NORMAL : (isSongRec ? TouchButton::BUTTON_RED : TouchButton::BUTTON_ACTIVE));
        if (layerEditActive) {
          // adapt button background if in layer edit mode
          handleLayerEditButtonColor(trackLayerMode, color);
        }

        const bool drawThisLayer = (layerUpdates[trackIndex] & (1 << layer)); // only used for song automation mute toggles
        if (drawAllLayers || drawThisLayer) {
          layerUpdates[trackIndex] &= ~(1 << layer);
          drawLayerButton(data.isSongMode, trackLayerMode, layer, layerEditActive, color, GRID.X[trackIndex], GRID.Y[2 + layer]);
        }
        if (guiFlags & drawActiveNotes) {
          // always draw pb / notes when layers visible and running
          if (data.tracks[track].pitchBend[layer].first) { // changed flag
            data.tracks[track].pitchBend[layer].first = false;
            const uint8_t pbValue = data.tracks[track].pitchBend[layer].second;
            const uint16_t xStart = GRID.X[trackIndex];
            const uint16_t yStart = GRID.Y[2 + layer] + TouchButton::BUTTON_SIZE_Y / 2;
            const int8_t thisYFill = -int8_t((pbValue - 64) / 4);
            display.console = true;
            display.fillRect(xStart, GRID.Y[2 + layer], 3, TouchButton::BUTTON_SIZE_Y, TouchButton::getColors(color).bg); // clear whole bar
            display.fillRect(xStart, std::min(yStart, uint16_t(yStart + thisYFill)), 3, std::abs(thisYFill), COLOR_PITCHSMP);
          }

          const uint16_t barHeight = 6 * data.tracks[track].activeNotes[layer].size();
          const uint16_t xStart = GRID.X[trackIndex] + TouchButton::BUTTON_SIZE_X - 3;
          const uint16_t yStart = GRID.Y[2 + layer];
          const uint16_t yFill = std::min(barHeight, TouchButton::BUTTON_SIZE_Y);
          display.console = true;
          display.fillRect(xStart, yStart, 3, TouchButton::BUTTON_SIZE_Y - yFill, TouchButton::getColors(color).bg);
          display.fillRect(xStart, yStart + (TouchButton::BUTTON_SIZE_Y - yFill), 3, yFill, COLOR_SYSTEXT);
        }
      }
    }
  }
  else {
    if (guiFlags & drawTools && !gridEditor.isActive()) {
      for (auto* b : buttonsToolSelect) {
        b->drawNow();
      }

      display.console = true;
      display.fillRect(0, GRID.Y[2] + TouchButton::BUTTON_SIZE_Y, DISPLAY_WIDTH, 4, MIDDLEGREEN);

      refreshToolsElements(static_cast<Tools>(currentTools));

      for (TouchButton* b : toolsPages[currentTools]) {
        b->drawNow();
      }
    }
    if (guiFlags & drawSongLayers && !gridEditor.isActive()) {
      TouchButton::Color color = TouchButton::BUTTON_ACTIVE;
      handleLayerEditButtonColor(songLayerMode, color);
      for (int songLayer = 0; songLayer < data.songLayerCount; songLayer++) {
        drawLayerButton(data.isSongMode, songLayerMode, songLayer, true, color, GRID.X[2 + songLayer], GRID.Y[5]);
      }
    }
  }
  guiFlags = 0;
}

FLASHMEM void UI_LiveSequencer::drawSingleLayer(uint8_t track, uint8_t layer) {
  layerUpdates[SCREEN_TRACK_INDEX(track)] |= (1 << layer);
}

FLASHMEM void UI_LiveSequencer::drawTrackLayers(uint8_t track) {
  layerUpdates[SCREEN_TRACK_INDEX(track)] = 0xFF;
}

FLASHMEM void UI_LiveSequencer::refreshToolsElements(Tools tools) {
  // refresh contents of current tools elements
  switch (tools) {
  case TOOLS_PATTERN:
    lastNoteLabel->drawNow();
    break;

  case TOOLS_KEYBOARD:
    drawKeyboard(VK_DRAW_ALL);
    break;

  default:
    break;
  }
}

FLASHMEM std::string UI_LiveSequencer::getArpModeName(uint8_t mode) {
  switch (mode) {
  case LiveSequencer::ArpMode::ARP_CHORD:
    return "CHORD";
  case LiveSequencer::ArpMode::ARP_DOWN:
    return "DN";
  case LiveSequencer::ArpMode::ARP_DOWNUP:
    return "DNUP";
  case LiveSequencer::ArpMode::ARP_DOWNUP_P:
    return "DNUP+";
  case LiveSequencer::ArpMode::ARP_RANDOM:
    return "RAND";
  case LiveSequencer::ArpMode::ARP_UP:
    return "UP";
  case LiveSequencer::ArpMode::ARP_UPDOWN:
    return "UPDN";
  case LiveSequencer::ArpMode::ARP_UPDOWN_P:
    return "UPDN+";
  default:
    return "NONE";
  }
}

FLASHMEM void UI_LiveSequencer::drawLayerButton(const bool horizontal, uint8_t layerMode, int layer, const bool layerEditActive, TouchButton::Color color, uint16_t x, uint16_t y) {
  if (currentTools != TOOLS_KEYBOARD)
  {
    char temp_char[4];
    std::string label = "LAYER";
    std::string labelSub = itoa(layer + 1, temp_char, 10);
    if (layerEditActive) {
      switch (layerMode) {
      case LiveSequencer::LayerMode::LAYER_MERGE:
        if (layer > 0) {
          label = "MERGE";
          labelSub = horizontal ? "<" : "^";
        }
        break;
      case LiveSequencer::LayerMode::LAYER_DELETE:
        label = "DELETE";
        labelSub = "x";
        break;

      case LiveSequencer::LayerMode::LAYER_CLEAR_CC:
        label = "CLEAR";
        labelSub = "CC";
        break;

      case LiveSequencer::LayerMode::LAYER_CLEAR_PB:
        label = "CLEAR";
        labelSub = "PB";
        break;
      }
    }
    TouchButton::drawButton(x, y, label.c_str(), labelSub.c_str(), color);
  }
}

FLASHMEM void UI_LiveSequencer::handleLayerEditButtonColor(uint8_t layerMode, TouchButton::Color& color) {
  switch (layerMode) {
  case LiveSequencer::LayerMode::LAYER_MERGE:
    color = TouchButton::BUTTON_HIGHLIGHTED;
    break;

  case LiveSequencer::LayerMode::LAYER_DELETE:
    color = TouchButton::BUTTON_RED;
    break;

  case LiveSequencer::LayerMode::LAYER_CLEAR_CC:
    color = TouchButton::BUTTON_PINK;
    break;

  case LiveSequencer::LayerMode::LAYER_CLEAR_PB:
    color = TouchButton::BUTTON_BLUE;
    break;
  }
}

