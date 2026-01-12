#ifndef UI_GRID_SONG_EDITOR_H
#define UI_GRID_SONG_EDITOR_H

#include <functional>
#include <vector>
#include "touchbutton.h"
#include "livesequencer.h"

class ILI9341_t3n;
struct EncoderEvents;

class GridSongEditorUI {
public:
  enum class ControlBank : uint8_t {
    Primary = 0,
    Secondary
  };

  struct Callbacks {
    std::function<void()> onExitRequested;
  };

  GridSongEditorUI(LiveSequencer& sequencer, LiveSequencer::LiveSeqData& data, ILI9341_t3n& display);
  void setCallbacks(const Callbacks& cb);

  void enter();
  void exit();
  bool isActive() const;

  void tick();
  void handleEncoder(const EncoderEvents& events);
  void handleTouch();
  void requestRedraw();
  void handleStepChanged();
  void handlePlaybackEnded();
  void exitToLiveSequencer();

private:
  FLASHMEM void initButtons();
  FLASHMEM void draw();
  FLASHMEM void processPlayButton();
  FLASHMEM void processGridTouch();
  FLASHMEM void processControlButtons();
  FLASHMEM void toggleControlBank();
  FLASHMEM void setControlBank(ControlBank bank);
  FLASHMEM void handleSecondaryButton(uint8_t index);
  FLASHMEM void drawSecondaryControlButton(TouchButton* button, uint8_t index);
  FLASHMEM void drawSecondaryBottomButton(uint16_t x, uint16_t y, uint8_t index);
  FLASHMEM bool isSecondaryFunctionEnabled(uint8_t index) const;
  FLASHMEM bool canCopyBlock() const;
  FLASHMEM bool canClearBlock() const;
  FLASHMEM bool canClearRow() const;
  FLASHMEM bool canCopyFromPattern() const;
  FLASHMEM bool canCopyToPattern() const;
  FLASHMEM bool playbackBlockIsValid() const;
  FLASHMEM bool blockHasContent() const;
  FLASHMEM bool rowHasContent(uint8_t step) const;
  FLASHMEM bool destinationRangeIsEmpty(uint8_t destStart, uint8_t blockLength) const;
  FLASHMEM void copySelectedBlock();
  FLASHMEM void clearSelectedBlock();
  FLASHMEM void clearSelectedRow();
  FLASHMEM void copyRowFromPattern();
  FLASHMEM void copyRowToPattern();
  FLASHMEM void armClearBlockConfirmation();
  FLASHMEM void cancelClearBlockConfirmation();
  FLASHMEM void updateClearBlockConfirmation();
  FLASHMEM void armClearRowConfirmation();
  FLASHMEM void cancelClearRowConfirmation();
  FLASHMEM void updateClearRowConfirmation();
  FLASHMEM void armFromPatternConfirmation();
  FLASHMEM void cancelFromPatternConfirmation();
  FLASHMEM void updateFromPatternConfirmation();
  FLASHMEM void armToPatternConfirmation();
  FLASHMEM void cancelToPatternConfirmation();
  FLASHMEM void updateToPatternConfirmation();
  FLASHMEM void toggleLayer(uint8_t layer);
  FLASHMEM void clearGridCell();
  FLASHMEM void fillGridCell();
  FLASHMEM void copyFromAbove();
  FLASHMEM void insertGridRow();
  FLASHMEM void removeGridRow();
  FLASHMEM void slowRect(uint16_t x, uint8_t y, uint16_t color);
  FLASHMEM void checkWebRemote();
  FLASHMEM uint8_t selectedTrackLayerLimit() const;
  FLASHMEM bool selectedCellHasContent() const;
  FLASHMEM bool layerIsActive(uint8_t layer) const;

  struct ConfirmButtonView {
    const char* topLabel;
    const char* subLabel;
    TouchButton::Color color;
  };

  FLASHMEM ConfirmButtonView buildConfirmButtonView(const char* idleTop, const char* idleSub, bool enabled, bool confirming) const;
  FLASHMEM void drawConfirmableButton(TouchButton* button, const char* idleTop, const char* idleSub, bool enabled, bool confirming);
  FLASHMEM void drawConfirmableBottomButton(uint16_t x, uint16_t y, const char* idleTop, const char* idleSub, bool enabled, bool confirming);
  FLASHMEM bool processConfirmableAction(bool enabled, bool& pendingFlag, void (GridSongEditorUI::*armFn)(), void (GridSongEditorUI::*executeFn)());

  LiveSequencer& liveSeq;
  LiveSequencer::LiveSeqData& data;
  ILI9341_t3n& display;
  Callbacks callbacks{};

  bool active = false;
  bool needsRedraw = true;
  ControlBank controlBank = ControlBank::Primary;
  bool clearBlockConfirmPending = false;
  uint32_t clearBlockConfirmDeadlineMs = 0;
  bool clearRowConfirmPending = false;
  uint32_t clearRowConfirmDeadlineMs = 0;
  bool fromPatternConfirmPending = false;
  uint32_t fromPatternConfirmDeadlineMs = 0;
  bool toPatternConfirmPending = false;
  uint32_t toPatternConfirmDeadlineMs = 0;

  static constexpr uint8_t GRID_SONG_COLS = 12;
  static constexpr uint8_t GRID_SONG_ROWS = 6;
  static constexpr uint8_t GRID_SONG_CELL_WIDTH = (320 - (GRID_SONG_COLS - 1)) / GRID_SONG_COLS;
  static constexpr uint8_t GRID_SONG_CELL_HEIGHT = (154 - (GRID_SONG_ROWS - 1)) / GRID_SONG_ROWS;
  static constexpr uint8_t gridStartX = 19;
  static constexpr uint8_t gridStartY = 12;
  static constexpr uint32_t CONFIRM_TIMEOUT_MS = 2000;

  std::vector<TouchButton*> gridControlButtons;
};

#endif // UI_GRID_SONG_EDITOR_H
