#include "ui_grid_song_editor.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <Arduino.h>

#include "ILI9341_t3n.h"
#include "touch.h"
#include "sequencer.h"

extern ts_t ts;
extern int numTouchPoints;
extern bool remote_active;
extern sequencer_t seq;

FLASHMEM GridSongEditorUI::GridSongEditorUI(LiveSequencer& sequencer, LiveSequencer::LiveSeqData& d, ILI9341_t3n& disp)
  : liveSeq(sequencer), data(d), display(disp) {
  initButtons();
}

FLASHMEM void GridSongEditorUI::setCallbacks(const Callbacks& cb) {
  callbacks = cb;
}

FLASHMEM void GridSongEditorUI::enter() {
  if (active) {
    return;
  }
  active = true;
  needsRedraw = true;
  data.redraw_grid_header = true;
  data.redraw_grid_buttons = true;
  controlBank = ControlBank::Primary;
  cancelClearBlockConfirmation();
  cancelClearRowConfirmation();
  cancelFromPatternConfirmation();
  cancelToPatternConfirmation();
  display.fillScreen(COLOR_BACKGROUND);
}

FLASHMEM void GridSongEditorUI::exit() {
  if (!active) {
    return;
  }
  active = false;
  needsRedraw = true;
  data.redraw_grid_header = true;
  data.redraw_grid_buttons = true;
  controlBank = ControlBank::Primary;
  cancelClearBlockConfirmation();
  cancelClearRowConfirmation();
  cancelFromPatternConfirmation();
  cancelToPatternConfirmation();
}

FLASHMEM bool GridSongEditorUI::isActive() const {
  return active;
}

FLASHMEM void GridSongEditorUI::tick() {
  if (!active) {
    return;
  }

  updateClearBlockConfirmation();
  updateClearRowConfirmation();
  updateFromPatternConfirmation();
  updateToPatternConfirmation();

  if (needsRedraw) {
    draw();
  }
}

FLASHMEM void GridSongEditorUI::handleEncoder(const EncoderEvents& events) {
  if (!active) {
    return;
  }

  if (events.down) {
    if (data.gridStepOffset < data.GRIDSONGLEN - GRID_SONG_ROWS) {
      ++data.gridStepOffset;
      needsRedraw = true;
    }
  }
  else if (events.up) {
    if (data.gridStepOffset > 0) {
      --data.gridStepOffset;
      needsRedraw = true;
    }
  }
}

FLASHMEM void GridSongEditorUI::handleTouch() {
  if (!active) {
    return;
  }

  for (uint8_t layer = 0; layer < 4; layer++) {
    if (TouchButton::isPressed(GRID.X[layer], GRID.Y[5])) {
      if (controlBank == ControlBank::Primary) {
        toggleLayer(layer);
      }
      else {
        const uint8_t funcIndex = layer + 4;
        if (isSecondaryFunctionEnabled(funcIndex)) {
          handleSecondaryButton(funcIndex);
        }
      }
      return;
    }
  }

  if (TouchButton::isPressed(GRID.X[4], GRID.Y[5])) {
    toggleControlBank();
    return;
  }

  if (TouchButton::isPressed(GRID.X[5], GRID.Y[5])) {
    processPlayButton();
    return;
  }

  processControlButtons();
  processGridTouch();
}

void GridSongEditorUI::requestRedraw() {
  needsRedraw = true;
}

void GridSongEditorUI::handleStepChanged() {
  if (active) {
    needsRedraw = true;
  }
}

FLASHMEM void GridSongEditorUI::handlePlaybackEnded() {
  const bool playingNow = data.isRunning || data.isStarting || seq.running;
  if (playingNow) {
    data.seq_started_from_pattern_mode = false;
    liveSeq.stop();
  }
  needsRedraw = true;
  data.redraw_grid_buttons = true;
  data.redraw_grid_header = true;
}

FLASHMEM void GridSongEditorUI::initButtons() {
  gridControlButtons.reserve(6);
  gridControlButtons.push_back(new TouchButton(GRID.X[0], GRID.Y[4],
    [this](auto* b) {
      if (controlBank == ControlBank::Secondary) {
        drawSecondaryControlButton(b, 0);
        return;
      }

      const uint8_t layerLimit = selectedTrackLayerLimit();
      const bool hasLayers = layerLimit > 0;
      const bool hasContent = hasLayers && selectedCellHasContent();

      const char* topLabel = hasContent ? "CLEAR" : "FILL";
      TouchButton::Color color = TouchButton::BUTTON_INACTIVE;
      if (hasLayers) {
        color = hasContent ? TouchButton::BUTTON_RED : TouchButton::BUTTON_ACTIVE;
      }

      b->draw(topLabel, "CELL", color);
    },
    [this](auto*) {
      if (controlBank == ControlBank::Secondary) {
        if (isSecondaryFunctionEnabled(0)) {
          handleSecondaryButton(0);
        }
        return;
      }

      const uint8_t layerLimit = selectedTrackLayerLimit();
      if (layerLimit == 0) {
        return;
      }

      if (selectedCellHasContent()) {
        clearGridCell();
      }
      else {
        fillGridCell();
      }
    }));

  gridControlButtons.push_back(new TouchButton(GRID.X[1], GRID.Y[4],
    [this](auto* b) {
      if (controlBank == ControlBank::Secondary) {
        drawSecondaryControlButton(b, 1);
        return;
      }

      const bool hasLayers = selectedTrackLayerLimit() > 0;
      if (hasLayers && data.selectedGridStep > 0) {
        b->draw("FROM", "ABOVE", TouchButton::BUTTON_ACTIVE);
      }
      else {
        b->draw("FROM", "ABOVE", TouchButton::BUTTON_INACTIVE);
      }
    },
    [this](auto*) {
      if (controlBank == ControlBank::Secondary) {
        if (isSecondaryFunctionEnabled(1)) {
          handleSecondaryButton(1);
        }
        return;
      }

      if (selectedTrackLayerLimit() > 0 && data.selectedGridStep > 0) {
        copyFromAbove();
      }
    }));

  gridControlButtons.push_back(new TouchButton(GRID.X[2], GRID.Y[4],
    [this](auto* b) {
      if (controlBank == ControlBank::Secondary) {
        drawSecondaryControlButton(b, 2);
        return;
      }

      const bool enabled = liveSeq.canInsertGridRow(data.selectedGridStep);
      b->draw("INSERT", "ROW", enabled ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_INACTIVE);
    },
    [this](auto*) {
      if (controlBank == ControlBank::Secondary) {
        if (isSecondaryFunctionEnabled(2)) {
          handleSecondaryButton(2);
        }
        return;
      }
      insertGridRow();
    }));

  gridControlButtons.push_back(new TouchButton(GRID.X[3], GRID.Y[4],
    [this](auto* b) {
      if (controlBank == ControlBank::Secondary) {
        drawSecondaryControlButton(b, 3);
        return;
      }

      const bool enabled = liveSeq.canRemoveGridRow(data.selectedGridStep);
      b->draw("REMOVE", "ROW", enabled ? TouchButton::BUTTON_RED : TouchButton::BUTTON_INACTIVE);
    },
    [this](auto*) {
      if (controlBank == ControlBank::Secondary) {
        if (isSecondaryFunctionEnabled(3)) {
          handleSecondaryButton(3);
        }
        return;
      }
      removeGridRow();
    }));

  gridControlButtons.push_back(new TouchButton(GRID.X[4], GRID.Y[4],
    [this](auto* b) {
      char startLabel[5];
      snprintf(startLabel, sizeof(startLabel), "%02u", static_cast<unsigned>(data.gridPlaybackStartStep) + 1);
      b->draw("START", startLabel, TouchButton::BUTTON_ACTIVE);
    },
    [this](auto*) {
      if (data.selectedGridStep < LiveSequencer::LiveSeqData::GRIDSONGLEN) {
        data.gridPlaybackStartStep = data.selectedGridStep;
        if (data.gridPlaybackStartStep > data.gridPlaybackEndStep) {
          data.gridPlaybackEndStep = data.gridPlaybackStartStep;
        }
        data.redraw_grid_buttons = true;
        needsRedraw = true;
      }
    }));

  gridControlButtons.push_back(new TouchButton(GRID.X[5], GRID.Y[4],
    [this](auto* b) {
      char endLabel[5];
      snprintf(endLabel, sizeof(endLabel), "%02u", static_cast<unsigned>(data.gridPlaybackEndStep) + 1);
      const char* topLabel = data.gridLoopEnabled ? "LOOP" : "END";
      const bool emphasize = data.gridLoopEnabled;
      b->draw(topLabel, endLabel, emphasize ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_ACTIVE);
    },
    [this](auto*) {
      if (data.selectedGridStep < LiveSequencer::LiveSeqData::GRIDSONGLEN) {
        if (data.gridPlaybackEndStep != data.selectedGridStep) {
          data.gridPlaybackEndStep = data.selectedGridStep;
          if (data.gridPlaybackEndStep < data.gridPlaybackStartStep) {
            data.gridPlaybackStartStep = data.gridPlaybackEndStep;
          }
        }
        else {
          data.gridLoopEnabled = !data.gridLoopEnabled;
        }
        data.redraw_grid_buttons = true;
        needsRedraw = true;
      }
    }));
}

FLASHMEM void GridSongEditorUI::draw() {
  if (!active || !needsRedraw) {
    return;
  }

  if (!seq.running) {
    data.seq_started_from_pattern_mode = false;
  }

  if (data.redraw_grid_header) {
    display.fillScreen(COLOR_BACKGROUND);
  }

  if (data.redraw_grid_header) {
    display.setTextSize(1);
    display.setTextColor(COLOR_SYSTEXT);
    for (uint8_t track = 0; track < GRID_SONG_COLS; track++) {
      const uint16_t x = 16 + (track * (GRID_SONG_CELL_WIDTH));
      display.setCursor(x + (GRID_SONG_CELL_WIDTH / 2) - 3, gridStartY - 15);
      display.printf("T%i", track + 1);
    }
    data.redraw_grid_header = false;
  }

  //display.drawRect(gridStartX, gridStartY, GRID_SONG_CELL_WIDTH * GRID_SONG_COLS + 1,  GRID_SONG_CELL_HEIGHT * GRID_SONG_ROWS + 1, GREY3);
  //   for (uint8_t track = 1; track < GRID_SONG_COLS; track++) {
  //     const uint16_t x = gridStartX + (track * GRID_SONG_CELL_WIDTH);
  //     display.drawLine(x, gridStartY, x, gridStartY + (GRID_SONG_CELL_HEIGHT * GRID_SONG_ROWS), GREY3);
  //   }
  //   for (uint8_t step = 1; step < GRID_SONG_ROWS; step++) {
  //     const uint16_t y = gridStartY + (step * GRID_SONG_CELL_HEIGHT);
  //     display.drawLine(gridStartX, y, (GRID_SONG_CELL_WIDTH * (GRID_SONG_COLS + 1)), y, GREY3);
  //   }

  for (uint8_t track = 0; track < GRID_SONG_COLS; track++) {
    for (uint8_t step = 0; step < GRID_SONG_ROWS; step++) {
      const uint16_t y = gridStartY + (step * GRID_SONG_CELL_HEIGHT);
      const uint16_t x = gridStartX + (track * GRID_SONG_CELL_WIDTH);
      checkWebRemote();
      display.drawRect(x, y, GRID_SONG_CELL_WIDTH + 1, GRID_SONG_CELL_HEIGHT + 1, GREY3);
    }
  }

  for (uint8_t step = 0; step < GRID_SONG_ROWS; step++) {
    const uint8_t actualStep = data.gridStepOffset + step;
    if (actualStep >= data.GRIDSONGLEN) {
      break;
    }

    const bool isCurrentStep = (actualStep == data.currentGridStep) && data.isRunning;
    const bool isSelectedStep = (actualStep == data.selectedGridStep);
    const bool isEndStep = (actualStep == data.gridPlaybackEndStep);

    display.setCursor(1, gridStartY + (step * (GRID_SONG_CELL_HEIGHT)) + GRID_SONG_CELL_HEIGHT / 2 - 4);
    uint16_t stepColor = COLOR_SYSTEXT;
    if (isCurrentStep) {
      stepColor = RED;
    }
    else if (isEndStep) {
      stepColor = YELLOW;
    }
    else if (actualStep == data.gridPlaybackStartStep) {
      stepColor = GREEN;
    }
    display.setTextColor(stepColor, COLOR_BACKGROUND);
    display.printf("%02i", actualStep + 1);

    for (uint8_t track = 0; track < GRID_SONG_COLS; track++) {
      const uint16_t x = gridStartX + (track * (GRID_SONG_CELL_WIDTH));
      const uint16_t y = gridStartY + (step * (GRID_SONG_CELL_HEIGHT));

      if (isSelectedStep && (track == data.selectedGridTrack)) {
        checkWebRemote();
        display.drawRect(x, y, GRID_SONG_CELL_WIDTH + 1, GRID_SONG_CELL_HEIGHT + 1, RED);
      }

      if (isCurrentStep) {
        bool hasActiveLayers = false;
        for (uint8_t layer = 0; layer < 4; layer++) {
          if (data.gridStep(actualStep, track, layer) && !liveSeq.isTrackLayerEmpty(track, layer)) {
            hasActiveLayers = true;
            break;
          }
        }
        if (hasActiveLayers) {
          checkWebRemote();
          display.drawRect(x, y, GRID_SONG_CELL_WIDTH + 1, GRID_SONG_CELL_HEIGHT + 1, GREEN);
        }
      }

      display.setTextSize(1);
      uint16_t color;
      const uint8_t layerSpacing = GRID_SONG_CELL_WIDTH / 4;
      const uint8_t layerOffset = 1;

      for (uint8_t layer = 0; layer < 2; layer++) {
        const uint16_t layerX = x + layerOffset + (layer * layerSpacing) + 6;
        const uint16_t layerY = y + 3;

        color = data.gridStep(actualStep, track, layer) ? COLOR_SYSTEXT : GREY3;
        display.setTextColor(color, COLOR_BACKGROUND);
        display.setCursor(layerX, layerY);
        if (!liveSeq.isTrackLayerEmpty(track, layer)) {
          display.printf("%i", layer + 1);
        }
      }

      for (uint8_t layer = 2; layer < 4; layer++) {
        const uint16_t layerX = x + layerOffset + ((layer - 2) * layerSpacing) + 6;
        const uint16_t layerY = y + GRID_SONG_CELL_HEIGHT / 2 + 3;
        color = data.gridStep(actualStep, track, layer) ? COLOR_SYSTEXT : GREY3;
        display.setTextColor(color, COLOR_BACKGROUND);
        display.setCursor(layerX, layerY);
        if (!liveSeq.isTrackLayerEmpty(track, layer)) {
          display.printf("%i", layer + 1);
        }
      }
    }
  }

  if (data.redraw_grid_buttons) {
    for (auto* button : gridControlButtons) {
      button->drawNow();
    }

    for (uint8_t layer = 0; layer < 4; layer++) {
      const uint16_t x = GRID.X[layer];
      const uint16_t y = GRID.Y[5];

      if (controlBank == ControlBank::Secondary) {
        const uint8_t funcIndex = layer + 4;
        drawSecondaryBottomButton(x, y, funcIndex);
        continue;
      }

      const bool layerAvailable = !liveSeq.isTrackLayerEmpty(data.selectedGridTrack, layer);
      if (!layerAvailable) {
        TouchButton::drawButton(x, y, "LAYER", std::to_string(layer + 1).c_str(), TouchButton::BUTTON_INACTIVE);
      }
      else {
        const bool isActiveLayer = layerIsActive(layer);
        TouchButton::drawButton(x, y, "LAYER", std::to_string(layer + 1).c_str(),
          isActiveLayer ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_NORMAL);
      }
    }

    const bool secondaryBankActive = (controlBank == ControlBank::Secondary);
    TouchButton::drawButton(GRID.X[4], GRID.Y[5], "SHIFT", "",
      secondaryBankActive ? TouchButton::BUTTON_HIGHLIGHTED : TouchButton::BUTTON_NORMAL);
    const bool playingNow = data.isRunning || data.isStarting || seq.running;
    TouchButton::drawButton(GRID.X[5], GRID.Y[5], "PLAY", playingNow ? "STOP" : "START",
      playingNow ? TouchButton::BUTTON_RED : TouchButton::BUTTON_NORMAL);
    data.redraw_grid_buttons = false;
  }

  needsRedraw = false;
}

FLASHMEM void GridSongEditorUI::exitToLiveSequencer() {
  exit();
  data.isSongMode = false;
  data.redraw_grid_header = true;
  data.redraw_grid_buttons = true;
  display.fillScreen(COLOR_BACKGROUND);

  if (!seq.running) {
    data.seq_started_from_pattern_mode = true;
  }

  if (callbacks.onExitRequested) {
    callbacks.onExitRequested();
  }
}

FLASHMEM void GridSongEditorUI::processPlayButton() {
  const bool playingNow = data.isRunning || data.isStarting || seq.running;
  auto stopPlayback = [&]() {
    data.seq_started_from_pattern_mode = false;
    liveSeq.stop();
    };

  if (playingNow) {
    stopPlayback();
  }
  else {
    data.seq_started_from_pattern_mode = false;
    if (data.gridPlaybackStartStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
      data.gridPlaybackStartStep = LiveSequencer::LiveSeqData::GRIDSONGLEN - 1;
    }
    data.currentGridStep = data.gridPlaybackStartStep;
    if (data.gridPlaybackEndStep < data.gridPlaybackStartStep) {
      data.gridPlaybackEndStep = data.gridPlaybackStartStep;
      data.redraw_grid_buttons = true;
    }
    data.redraw_grid_header = true;
    liveSeq.start();
  }
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::processGridTouch() {
  if (numTouchPoints == 0) {
    return;
  }

  const uint16_t touchX = ts.p.x;
  const uint8_t touchY = ts.p.y;

  for (uint8_t step = 0; step < GRID_SONG_ROWS; step++) {
    const uint8_t actualStep = data.gridStepOffset + step;
    if (actualStep >= data.GRIDSONGLEN) {
      break;
    }

    for (uint8_t track = 0; track < GRID_SONG_COLS; track++) {
      const uint16_t x = gridStartX + (track * GRID_SONG_CELL_WIDTH);
      const uint16_t y = gridStartY + (step * GRID_SONG_CELL_HEIGHT);

      if (touchX >= x && touchX <= x + GRID_SONG_CELL_WIDTH &&
        touchY >= y && touchY <= y + GRID_SONG_CELL_HEIGHT) {
        data.selectedGridStep = actualStep;
        data.selectedGridTrack = track;
        needsRedraw = true;
        data.redraw_grid_buttons = true;
        return;
      }
    }
  }
}

FLASHMEM void GridSongEditorUI::processControlButtons() {
  for (auto* button : gridControlButtons) {
    button->processPressed();
  }
}

FLASHMEM void GridSongEditorUI::toggleControlBank() {
  const ControlBank next = (controlBank == ControlBank::Primary) ? ControlBank::Secondary : ControlBank::Primary;
  setControlBank(next);
}

FLASHMEM void GridSongEditorUI::setControlBank(ControlBank bank) {
  if (controlBank == bank) {
    return;
  }
  controlBank = bank;
  cancelClearBlockConfirmation();
  cancelClearRowConfirmation();
  cancelFromPatternConfirmation();
  cancelToPatternConfirmation();
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::handleSecondaryButton(uint8_t index) {
  if (index != 1) {
    cancelClearBlockConfirmation();
  }
  if (index != 2) {
    cancelClearRowConfirmation();
  }
  if (index != 4) {
    cancelFromPatternConfirmation();
  }
  if (index != 5) {
    cancelToPatternConfirmation();
  }

  switch (index) {
  case 0:
    copySelectedBlock();
    break;
  case 1:
    processConfirmableAction(canClearBlock(), clearBlockConfirmPending,
      &GridSongEditorUI::armClearBlockConfirmation, &GridSongEditorUI::clearSelectedBlock);
    break;
  case 2:
    processConfirmableAction(canClearRow(), clearRowConfirmPending,
      &GridSongEditorUI::armClearRowConfirmation, &GridSongEditorUI::clearSelectedRow);
    break;
  case 4:
    processConfirmableAction(canCopyFromPattern(), fromPatternConfirmPending,
      &GridSongEditorUI::armFromPatternConfirmation, &GridSongEditorUI::copyRowFromPattern);
    break;
  case 5:
    processConfirmableAction(canCopyToPattern(), toPatternConfirmPending,
      &GridSongEditorUI::armToPatternConfirmation, &GridSongEditorUI::copyRowToPattern);
    break;
  default:
    break;
  }
}

FLASHMEM void GridSongEditorUI::drawSecondaryControlButton(TouchButton* button, uint8_t index) {
  const bool enabled = isSecondaryFunctionEnabled(index);
  if (index == 0) {
    button->draw("COPY", "BLOCK", enabled ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_INACTIVE);
    return;
  }

  if (index == 1) {
    if (!enabled && clearBlockConfirmPending) {
      cancelClearBlockConfirmation();
    }
    const bool confirming = clearBlockConfirmPending && enabled;
    drawConfirmableButton(button, "CLEAR", "BLOCK", enabled, confirming);
    return;
  }

  if (index == 2) {
    if (!enabled && clearRowConfirmPending) {
      cancelClearRowConfirmation();
    }
    const bool confirming = clearRowConfirmPending && enabled;
    drawConfirmableButton(button, "CLEAR", "ROW", enabled, confirming);
    return;
  }

  char subLabel[4];
  snprintf(subLabel, sizeof(subLabel), "%u", static_cast<unsigned>(index + 1));
  button->draw("FUNC", subLabel, enabled ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_INACTIVE);
}

FLASHMEM void GridSongEditorUI::drawSecondaryBottomButton(uint16_t x, uint16_t y, uint8_t index) {
  if (index == 4) {
    const bool enabled = isSecondaryFunctionEnabled(index);
    if (!enabled && fromPatternConfirmPending) {
      cancelFromPatternConfirmation();
    }
    const bool confirming = fromPatternConfirmPending && enabled;
    drawConfirmableBottomButton(x, y, "FROM", "PAT", enabled, confirming);
    return;
  }

  if (index == 5) {
    const bool enabled = isSecondaryFunctionEnabled(index);
    if (!enabled && toPatternConfirmPending) {
      cancelToPatternConfirmation();
    }
    const bool confirming = toPatternConfirmPending && enabled;
    drawConfirmableBottomButton(x, y, "TO", "PAT", enabled, confirming);
    return;
  }

  char subLabel[4];
  snprintf(subLabel, sizeof(subLabel), "%u", static_cast<unsigned>(index + 1));
  const bool enabled = isSecondaryFunctionEnabled(index);
  TouchButton::drawButton(x, y, "FUNC", subLabel, enabled ? TouchButton::BUTTON_ACTIVE : TouchButton::BUTTON_INACTIVE);
}

FLASHMEM bool GridSongEditorUI::isSecondaryFunctionEnabled(uint8_t index) const {
  switch (index) {
  case 0:
    return canCopyBlock();
  case 1:
    return canClearBlock();
  case 2:
    return canClearRow();
  case 4:
    return canCopyFromPattern();
  case 5:
    return canCopyToPattern();
  default:
    return false;
  }
}

FLASHMEM bool GridSongEditorUI::canCopyBlock() const {
  if (!playbackBlockIsValid()) {
    return false;
  }

  const uint8_t srcStart = data.gridPlaybackStartStep;
  const uint8_t srcEnd = data.gridPlaybackEndStep;
  const uint8_t blockLength = static_cast<uint8_t>(srcEnd - srcStart + 1);
  if (blockLength == 0) {
    return false;
  }

  const uint8_t destStart = data.selectedGridStep;
  if (destStart >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  if (destStart >= srcStart && destStart <= srcEnd) {
    return false;
  }

  const uint16_t destEndExclusive = static_cast<uint16_t>(destStart) + blockLength;
  if (destEndExclusive > LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  return destinationRangeIsEmpty(destStart, blockLength);
}

FLASHMEM bool GridSongEditorUI::canClearBlock() const {
  return playbackBlockIsValid() && blockHasContent();
}

FLASHMEM bool GridSongEditorUI::canClearRow() const {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }
  return rowHasContent(data.selectedGridStep);
}

FLASHMEM bool GridSongEditorUI::canCopyFromPattern() const {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
    const uint8_t layerCount = (track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS) ? data.trackSettings[track].layerCount : 0;
    const uint8_t layerLimit = std::min<uint8_t>(layerCount, LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
    const uint8_t patternLayerMutes = (track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS) ? data.tracks[track].layerMutes : 0;

    for (uint8_t layer = 0; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; ++layer) {
      const bool patternEnabled = (layer < layerLimit) && ((patternLayerMutes & (1 << layer)) == 0);
      const bool rowEnabled = data.gridStep(data.selectedGridStep, track, layer);
      if (patternEnabled != rowEnabled) {
        return true;
      }
    }
  }

  return false;
}

FLASHMEM bool GridSongEditorUI::canCopyToPattern() const {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  const uint8_t trackLimit = std::min<uint8_t>(GRID_SONG_COLS, LiveSequencer::LIVESEQUENCER_NUM_TRACKS);
  for (uint8_t track = 0; track < trackLimit; ++track) {
    const uint8_t layerCount = data.trackSettings[track].layerCount;
    const uint8_t layerLimit = std::min<uint8_t>(layerCount, LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
    if (layerLimit == 0) {
      continue;
    }

    const uint8_t patternLayerMutes = data.tracks[track].layerMutes;
    for (uint8_t layer = 0; layer < layerLimit; ++layer) {
      const bool rowEnabled = data.gridStep(data.selectedGridStep, track, layer);
      const bool patternEnabled = ((patternLayerMutes & (1 << layer)) == 0);
      if (rowEnabled != patternEnabled) {
        return true;
      }
    }
  }

  return false;
}
FLASHMEM bool GridSongEditorUI::playbackBlockIsValid() const {
  if (data.gridPlaybackStartStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN ||
    data.gridPlaybackEndStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }
  return data.gridPlaybackStartStep <= data.gridPlaybackEndStep;
}

FLASHMEM bool GridSongEditorUI::blockHasContent() const {
  if (!playbackBlockIsValid()) {
    return false;
  }

  for (uint8_t step = data.gridPlaybackStartStep; step <= data.gridPlaybackEndStep; ++step) {
    if (rowHasContent(step)) {
      return true;
    }
  }

  return false;
}

FLASHMEM bool GridSongEditorUI::rowHasContent(uint8_t step) const {
  for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
    for (uint8_t layer = 0; layer < 4; ++layer) {
      if (data.gridStep(step, track, layer)) {
        return true;
      }
    }
  }
  return false;
}

FLASHMEM bool GridSongEditorUI::destinationRangeIsEmpty(uint8_t destStart, uint8_t blockLength) const {
  const uint16_t destEndExclusive = static_cast<uint16_t>(destStart) + blockLength;
  if (destEndExclusive > LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    return false;
  }

  for (uint8_t offset = 0; offset < blockLength; ++offset) {
    const uint8_t step = destStart + offset;
    for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
      for (uint8_t layer = 0; layer < 4; ++layer) {
        if (data.gridStep(step, track, layer)) {
          return false;
        }
      }
    }
  }

  return true;
}

FLASHMEM void GridSongEditorUI::copySelectedBlock() {
  if (!canCopyBlock()) {
    return;
  }

  cancelClearBlockConfirmation();

  const uint8_t srcStart = data.gridPlaybackStartStep;
  const uint8_t srcEnd = data.gridPlaybackEndStep;
  const uint8_t blockLength = static_cast<uint8_t>(srcEnd - srcStart + 1);
  const uint8_t destStart = data.selectedGridStep;

  for (uint8_t offset = 0; offset < blockLength; ++offset) {
    const uint8_t srcStep = srcStart + offset;
    const uint8_t destStep = destStart + offset;
    for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
      for (uint8_t layer = 0; layer < 4; ++layer) {
        data.gridStep(destStep, track, layer) = data.gridStep(srcStep, track, layer);
      }
    }
  }

  needsRedraw = true;
  data.redraw_grid_buttons = true;
  data.redraw_grid_header = true;
}

FLASHMEM void GridSongEditorUI::clearSelectedBlock() {
  if (!playbackBlockIsValid()) {
    cancelClearBlockConfirmation();
    return;
  }

  for (uint8_t step = data.gridPlaybackStartStep; step <= data.gridPlaybackEndStep; ++step) {
    for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
      for (uint8_t layer = 0; layer < 4; ++layer) {
        data.gridStep(step, track, layer) = false;
      }
    }
  }

  cancelClearBlockConfirmation();
  needsRedraw = true;
  data.redraw_grid_buttons = true;
  data.redraw_grid_header = true;
}

FLASHMEM void GridSongEditorUI::armClearBlockConfirmation() {
  clearBlockConfirmPending = true;
  clearBlockConfirmDeadlineMs = millis() + CONFIRM_TIMEOUT_MS;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::cancelClearBlockConfirmation() {
  if (!clearBlockConfirmPending) {
    return;
  }
  clearBlockConfirmPending = false;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::updateClearBlockConfirmation() {
  if (!clearBlockConfirmPending) {
    return;
  }

  if (controlBank != ControlBank::Secondary || !canClearBlock()) {
    cancelClearBlockConfirmation();
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - clearBlockConfirmDeadlineMs) >= 0) {
    cancelClearBlockConfirmation();
  }
}

FLASHMEM void GridSongEditorUI::clearSelectedRow() {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    cancelClearRowConfirmation();
    return;
  }

  for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
    for (uint8_t layer = 0; layer < 4; ++layer) {
      data.gridStep(data.selectedGridStep, track, layer) = false;
    }
  }

  cancelClearRowConfirmation();
  needsRedraw = true;
  data.redraw_grid_buttons = true;
  data.redraw_grid_header = true;
}

FLASHMEM void GridSongEditorUI::copyRowFromPattern() {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    cancelFromPatternConfirmation();
    return;
  }

  for (uint8_t track = 0; track < GRID_SONG_COLS; ++track) {
    const uint8_t layerCount = (track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS) ? data.trackSettings[track].layerCount : 0;
    const uint8_t layerLimit = std::min<uint8_t>(layerCount, LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
    const uint8_t patternLayerMutes = (track < LiveSequencer::LIVESEQUENCER_NUM_TRACKS) ? data.tracks[track].layerMutes : 0;

    for (uint8_t layer = 0; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; ++layer) {
      const bool enabled = (layer < layerLimit) && ((patternLayerMutes & (1 << layer)) == 0);
      data.gridStep(data.selectedGridStep, track, layer) = enabled;
    }
  }

  cancelFromPatternConfirmation();
  needsRedraw = true;
  data.redraw_grid_buttons = true;
  data.redraw_grid_header = true;
}

FLASHMEM void GridSongEditorUI::copyRowToPattern() {
  if (data.selectedGridStep >= LiveSequencer::LiveSeqData::GRIDSONGLEN) {
    cancelToPatternConfirmation();
    return;
  }

  bool patternChanged = false;
  const uint8_t trackLimit = std::min<uint8_t>(GRID_SONG_COLS, LiveSequencer::LIVESEQUENCER_NUM_TRACKS);
  for (uint8_t track = 0; track < trackLimit; ++track) {
    const uint8_t layerCount = data.trackSettings[track].layerCount;
    const uint8_t layerLimit = std::min<uint8_t>(layerCount, LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
    if (layerLimit == 0) {
      continue;
    }

    uint8_t updatedMask = data.tracks[track].layerMutes;
    for (uint8_t layer = 0; layer < layerLimit; ++layer) {
      const bool rowEnabled = data.gridStep(data.selectedGridStep, track, layer);
      if (rowEnabled) {
        updatedMask &= ~(1 << layer);
      }
      else {
        updatedMask |= (1 << layer);
      }
    }

    for (uint8_t layer = layerLimit; layer < LiveSequencer::LIVESEQUENCER_NUM_LAYERS; ++layer) {
      updatedMask |= (1 << layer);
    }

    if (updatedMask != data.tracks[track].layerMutes) {
      data.tracks[track].layerMutes = updatedMask;
      patternChanged = true;
    }
  }

  cancelToPatternConfirmation();
  if (patternChanged) {
    needsRedraw = true;
    data.redraw_grid_buttons = true;
    data.redraw_grid_header = true;
  }
}

FLASHMEM void GridSongEditorUI::armClearRowConfirmation() {
  clearRowConfirmPending = true;
  clearRowConfirmDeadlineMs = millis() + CONFIRM_TIMEOUT_MS;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::cancelClearRowConfirmation() {
  if (!clearRowConfirmPending) {
    return;
  }
  clearRowConfirmPending = false;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::updateClearRowConfirmation() {
  if (!clearRowConfirmPending) {
    return;
  }

  if (controlBank != ControlBank::Secondary || !canClearRow()) {
    cancelClearRowConfirmation();
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - clearRowConfirmDeadlineMs) >= 0) {
    cancelClearRowConfirmation();
  }
}

FLASHMEM void GridSongEditorUI::armFromPatternConfirmation() {
  fromPatternConfirmPending = true;
  fromPatternConfirmDeadlineMs = millis() + CONFIRM_TIMEOUT_MS;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::cancelFromPatternConfirmation() {
  if (!fromPatternConfirmPending) {
    return;
  }
  fromPatternConfirmPending = false;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::updateFromPatternConfirmation() {
  if (!fromPatternConfirmPending) {
    return;
  }

  if (controlBank != ControlBank::Secondary || !canCopyFromPattern()) {
    cancelFromPatternConfirmation();
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - fromPatternConfirmDeadlineMs) >= 0) {
    cancelFromPatternConfirmation();
  }
}

FLASHMEM void GridSongEditorUI::armToPatternConfirmation() {
  toPatternConfirmPending = true;
  toPatternConfirmDeadlineMs = millis() + CONFIRM_TIMEOUT_MS;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::cancelToPatternConfirmation() {
  if (!toPatternConfirmPending) {
    return;
  }
  toPatternConfirmPending = false;
  data.redraw_grid_buttons = true;
  needsRedraw = true;
}

FLASHMEM void GridSongEditorUI::updateToPatternConfirmation() {
  if (!toPatternConfirmPending) {
    return;
  }

  if (controlBank != ControlBank::Secondary || !canCopyToPattern()) {
    cancelToPatternConfirmation();
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - toPatternConfirmDeadlineMs) >= 0) {
    cancelToPatternConfirmation();
  }
}

FLASHMEM void GridSongEditorUI::toggleLayer(uint8_t layer) {
  data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer) =
    !data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer);
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::clearGridCell() {
  for (uint8_t layer = 0; layer < 4; layer++) {
    data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer) = false;
  }
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::fillGridCell() {
  for (uint8_t layer = 0; layer < 4; layer++) {
    data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer) = true;
  }
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::copyFromAbove() {
  if (data.selectedGridStep == 0) {
    return;
  }

  for (uint8_t layer = 0; layer < 4; layer++) {
    data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer) =
      data.gridStep(data.selectedGridStep - 1, data.selectedGridTrack, layer);
  }
  needsRedraw = true;
  data.redraw_grid_buttons = true;
}

FLASHMEM void GridSongEditorUI::insertGridRow() {
  if (liveSeq.insertGridRow(data.selectedGridStep)) {
    needsRedraw = true;
    data.redraw_grid_buttons = true;
  }
}

FLASHMEM void GridSongEditorUI::removeGridRow() {
  if (liveSeq.removeGridRow(data.selectedGridStep)) {
    needsRedraw = true;
    data.redraw_grid_buttons = true;
  }
}

FLASHMEM void GridSongEditorUI::checkWebRemote() {
  if (remote_active) {
    display.console = true;
  }
}

FLASHMEM uint8_t GridSongEditorUI::selectedTrackLayerLimit() const {
  const uint8_t layerCount = data.trackSettings[data.selectedGridTrack].layerCount;
  return std::min<uint8_t>(layerCount, LiveSequencer::LIVESEQUENCER_NUM_LAYERS);
}

FLASHMEM bool GridSongEditorUI::selectedCellHasContent() const {
  const uint8_t layerLimit = selectedTrackLayerLimit();
  for (uint8_t layer = 0; layer < layerLimit; ++layer) {
    if (data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer)) {
      return true;
    }
  }
  return false;
}

FLASHMEM bool GridSongEditorUI::layerIsActive(uint8_t layer) const {
  if (layer >= selectedTrackLayerLimit()) {
    return false;
  }
  return data.gridStep(data.selectedGridStep, data.selectedGridTrack, layer);
}

FLASHMEM GridSongEditorUI::ConfirmButtonView GridSongEditorUI::buildConfirmButtonView(const char* idleTop, const char* idleSub,
  bool enabled, bool confirming) const {
  ConfirmButtonView view{ idleTop, idleSub, TouchButton::BUTTON_INACTIVE };
  if (!enabled) {
    return view;
  }

  view.topLabel = confirming ? "CONFIRM" : idleTop;
  view.subLabel = confirming ? "?" : idleSub;
  view.color = confirming ? TouchButton::BUTTON_RED : TouchButton::BUTTON_ACTIVE;
  return view;
}

FLASHMEM void GridSongEditorUI::drawConfirmableButton(TouchButton* button, const char* idleTop, const char* idleSub, bool enabled,
  bool confirming) {
  const auto view = buildConfirmButtonView(idleTop, idleSub, enabled, confirming);
  button->draw(view.topLabel, view.subLabel, view.color);
}

FLASHMEM void GridSongEditorUI::drawConfirmableBottomButton(uint16_t x, uint16_t y, const char* idleTop, const char* idleSub,
  bool enabled, bool confirming) {
  const auto view = buildConfirmButtonView(idleTop, idleSub, enabled, confirming);
  TouchButton::drawButton(x, y, view.topLabel, view.subLabel, view.color);
}

FLASHMEM bool GridSongEditorUI::processConfirmableAction(bool enabled, bool& pendingFlag,
  void (GridSongEditorUI::* armFn)(), void (GridSongEditorUI::* executeFn)()) {
  if (!enabled) {
    return false;
  }

  if (!pendingFlag) {
    (this->*armFn)();
  }
  else {
    (this->*executeFn)();
  }
  return true;
}
