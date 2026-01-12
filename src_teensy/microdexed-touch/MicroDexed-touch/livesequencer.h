#ifndef LIVESEQUENCER_H
#define LIVESEQUENCER_H

#include <string>
#include <vector>
#include <list>
#include "MIDI.h"
#include "LCDMenuLib2_typedef.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "UI.h"



typedef void (*SetupFn)(void*);
class UI_LiveSequencer;

class LiveSequencer {

public:
  static constexpr uint8_t LIVESEQUENCER_TRACKS_PER_SCREEN = 6;
  static constexpr uint8_t LIVESEQUENCER_NUM_TRACKS = 2 * LIVESEQUENCER_TRACKS_PER_SCREEN;
  static constexpr uint8_t LIVESEQUENCER_NUM_LAYERS = 4;

  bool isTrackLayerEmpty(uint8_t track, uint8_t layer);


  enum EventSource : uint8_t {
    EVENT_SONG = 0,
    EVENT_PATTERN = 1,
  };

  enum LayerMode {
    LAYER_MUTE = 0,
    LAYER_MERGE,
    LAYER_DELETE,
    LAYER_CLEAR_CC,
    LAYER_CLEAR_PB,
    LAYER_MODE_NUM
  };

  enum Device {
    DEVICE_INTERNAL = 0,
    DEVICE_MIDI_USB = 1,
    DEVICE_MIDI_DIN = 2,
    DEVICE_MIDI_INT = 3
  };

  // !!! never change existing numbers, used as IDs in saved performances !!!
  enum InternalInstrument {
    INSTR_DRUM = 0,
    INSTR_DX1 = 1,
    INSTR_DX2 = 2,
    INSTR_EP = 3,
    INSTR_MS1 = 4,
    INSTR_MS2 = 5,
    INSTR_BRD = 6,
    INSTR_MSP1 = 7,
    INSTR_MSP2 = 8,
    INSTR_SLC = 9,
    // The gap here is because in the other sequencer, all instruments are managed as the same thing, even the external 16+16+16=48 MIDI Channels (+ a safety gap)
    INSTR_DX3 = 70,
    INSTR_DX4 = 71,
    INSTR_GRA = 72,
    INSTR_MAX = 13
  };

  struct MidiEvent {
    EventSource source;
    uint16_t patternMs;
    uint8_t patternNumber;
    uint8_t track;
    uint8_t layer;
    midi::MidiType event;
    uint8_t note_in;          // For CC: controller number, for Pitch Bend: LSB
    uint8_t note_in_velocity; // For CC: value, for Pitch Bend: MSB
  };

  struct Track {
    midi::Channel channel;
    char name[10];
    uint8_t layerMutes;
    LCDML_FuncPtr_pu8 screen;
    SetupFn screenSetupFn;
    std::unordered_multiset<uint8_t> activeNotes[LIVESEQUENCER_NUM_LAYERS];
    std::pair<bool, uint8_t> pitchBend[LIVESEQUENCER_NUM_LAYERS];
  };

  struct TrackSettings {
    uint8_t device;
    uint8_t instrument;
    uint8_t layerCount;
    uint8_t quantizeDenom;
    uint8_t quantizeGrid; // mapped grid value (0/1 disables quantization)
    uint8_t velocityLevel; // 0: original, 1 - 10: 10-100%
    uint8_t songStartLayerMutes;
  };


  /**
   * Automation types for Live Sequencer
   * General-purpose high CC number from 102..119 range should be safe.
   * WARNING: Breaking change with old performances that had
   * 0 and 1 as mute on/off.
   */
  enum AutomationType : uint8_t {
    TYPE_MUTE_ON = 102,
    TYPE_MUTE_OFF = 103
  };

  struct FillNotes {
    uint8_t number;
    uint8_t offset;
    uint8_t velocityLevel;
  };

  enum ArpMode : uint8_t {
    ARP_UP = 0,
    ARP_DOWN,
    ARP_UPDOWN,
    ARP_UPDOWN_P,
    ARP_DOWNUP,
    ARP_DOWNUP_P,
    ARP_RANDOM,
    ARP_CHORD,
    ARP_MODENUM
  };

  struct ArpNote {
    uint16_t offDelay;
    uint8_t track;
    std::vector<uint8_t> notes;
  };

  struct ArpSettings {
    uint16_t delayToNextArpOnMs;
    uint8_t enabled;
    uint8_t amount; // 1, 2, ... per bar
    uint8_t octaves;
    uint8_t source;
    ArpMode mode;
    uint8_t loadPerBar;
    uint8_t noteRepeat;
    uint8_t velocityLevel;
    uint8_t notePlayCount;
    uint16_t length; // >100% pulse width possible
    int8_t swing;
    uint8_t latch; // keep notes or drop them
    uint8_t freerun; // do not restart arp on pattern start
    bool keysChanged;
    bool arpSettingsChanged;
    std::vector<uint8_t> arpNotesIn;
    std::vector<uint8_t> arpNotes;
    std::vector<uint8_t>::iterator arpIt;
  };

#ifdef PSRAM
  template<typename T>
  class Alloc {
  public:
    using value_type = T;

    Alloc() = default;

    template<typename U>
    constexpr Alloc(const Alloc<U>&) noexcept {}

    T* allocate(std::size_t n) {
      DBG_LOG(printf("alloc %i!\n", n));
      return static_cast<T*>(extmem_malloc(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
      extmem_free((uint8_t*)p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
      new(p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) noexcept {
      p->~U();
    }

    friend bool operator==(const Alloc&, const Alloc&) { return true; }
    friend bool operator!=(const Alloc&, const Alloc&) { return false; }
  };

  typedef std::vector<MidiEvent, Alloc<MidiEvent>> EventVector;
#else
  typedef std::vector<MidiEvent> EventVector;
#endif

  struct LiveSeqData {
    // non - volatile
    TrackSettings trackSettings[LIVESEQUENCER_NUM_TRACKS];

    EventVector eventsList;
    std::unordered_map<uint8_t, EventVector> songEvents;
    uint8_t numberOfBars = 4;

    // volatile
    bool processMidiIn;
    std::string performanceName;
    std::unordered_set<uint8_t> instrumentChannels;
    Track tracks[LIVESEQUENCER_NUM_TRACKS];
    ArpSettings arpSettings;
    uint8_t lastSongEventPattern; // because using unordered map above we need to know last index to be able to know song length (eg. for song loop)
    uint8_t currentPattern = 0;
    FillNotes fillNotes = { 4, 0 }; // user default
    unsigned long patternLengthMs;
    uint8_t activeTrack = 0;
    elapsedMillis patternTimer;
    std::unordered_map<uint8_t, MidiEvent> notesOn;
    EventVector pendingEvents;
    uint8_t songPatternCount = 0;
    uint8_t songLayerCount = 0;
    uint8_t lastPlayedNote = 0;
    uint8_t remainingCountIns = 0;

    bool isStarting = false;
    bool isRunning = false;
    bool isRecording = false;

    bool isSongMode = false;
    bool recordedToSong = false;
    int currentBpm = 90;
    uint8_t performanceID = 0;
    uint8_t songMuteQuantizeDenom = 1;

    uint8_t stepRecordTargetTrack;  //select target instrument for step recorder in song mode

    // Grid-based song mode
#ifdef PSRAM
    // Extra song slots for PSRAM builds allow deeper arrangements in the grid editor
    static const uint8_t GRIDSONGLEN = 64;
#else
    static const uint8_t GRIDSONGLEN = 32;
#endif
  bool useGridSongMode = false;  // false = live recording mode, true = grid mode
  bool seq_started_from_pattern_mode = false;
  //uint8_t  gridSongSteps[GRIDSONGLEN][LIVESEQUENCER_NUM_TRACKS][LIVESEQUENCER_NUM_LAYERS]; // GRIDSONGLEN song steps, 12 tracks, 4 layers
  uint8_t* gridSongSteps = nullptr;  // Pointer to PSRAM allocation
  uint8_t gridPlaybackStartStep = 0;           // first grid row to play (0-indexed)
  uint8_t gridPlaybackEndStep = GRIDSONGLEN - 1; // last grid row to play (0-indexed)
  bool gridLoopEnabled = false;
    uint8_t currentGridStep = 0;
    uint8_t gridStepOffset = 0;  // for scrolling through steps
    uint8_t selectedGridStep = 0;
    uint8_t selectedGridTrack = 0;
    bool redraw_grid_header;
    bool redraw_grid_buttons;

    uint8_t& gridStep(uint8_t step, uint8_t track, uint8_t layer) {
      return gridSongSteps[step * (LiveSequencer::LIVESEQUENCER_NUM_TRACKS * LiveSequencer::LIVESEQUENCER_NUM_LAYERS) +
        track * LiveSequencer::LIVESEQUENCER_NUM_LAYERS + layer];
    }

    const uint8_t& gridStep(uint8_t step, uint8_t track, uint8_t layer) const {
      return gridSongSteps[step * (LiveSequencer::LIVESEQUENCER_NUM_TRACKS * LiveSequencer::LIVESEQUENCER_NUM_LAYERS) +
        track * LiveSequencer::LIVESEQUENCER_NUM_LAYERS + layer];
    }

  };

  LiveSequencer();
  void initOnce(void);
  LiveSequencer::LiveSeqData* getData(void);
  void songLayerAction(uint8_t layer, LayerMode action);
  bool trackLayerAction(uint8_t track, uint8_t layer, LayerMode action, bool& clearLayer);
  void handleMidiEvent(uint8_t inChannel, midi::MidiType event, uint8_t note, uint8_t velocity);
  void handlePatternBegin(void);
  void start(void);
  void stop(void);
  void onStarted(void);
  void onStopped(void);
  void init(void);
  void onGuiInit(void);
  void onArpSourceChanged(void);
  void setLayerMuted(uint8_t track, uint8_t layer, bool isMuted, bool recordToSong = false);
  void changeNumberOfBars(uint8_t num);
  void deleteAllSongEvents(void);
  void resetGridEditorDefaults(void);
  void sanitizeGridSongSteps();
  void fillTrackLayer();
  void deleteLiveSequencerData(void);
  void setArpEnabled(bool enabled);
  uint32_t timeToMs(uint8_t patternNumber, uint16_t patternMs) const;
  void startCountIn(void);
  void getDeviceName(uint8_t device, char* name, char* sub) const;
  void getInstrumentName(uint8_t device, uint8_t instrument, char* name, char* sub) const;
  void changeTrackInstrument(uint8_t track, uint8_t newDevice, uint8_t newInstrumentOrChannel);
  void loadOldTrackInstruments(void); // load track instruments from normal sequencer
  void requestSortEvents(void);
  void setActiveTrack(uint8_t track);
  bool insertGridRow(uint8_t step);
  bool removeGridRow(uint8_t step);
  bool canInsertGridRow(uint8_t step) const;
  bool canRemoveGridRow(uint8_t step) const;

  struct NotePair {
    MidiEvent& noteOn;
    MidiEvent& noteOff;
    bool isMuted;
  };

  void addNotePair(MidiEvent noteOn, MidiEvent noteOff);
  std::vector<NotePair> getNotePairsFromTrack(uint8_t track, uint8_t& lowestNote, uint8_t& highestNote, uint8_t patternFrom = 0, uint8_t patternTo = 0xFF);

  static void printNotePairs(std::vector<NotePair> notePairs);
  static void printEvent(int i, MidiEvent e);
  static const std::string getEventName(midi::MidiType event);
  static const std::string getEventSource(EventSource source);
  static uint8_t mapQuantizeDenom(uint8_t denom);
  void checkBpmChanged(void);
  void AddMetronome(void);

private:
  static constexpr size_t GRID_ROW_STRIDE = LIVESEQUENCER_NUM_TRACKS * LIVESEQUENCER_NUM_LAYERS;
  LiveSeqData data;
  std::set<uint8_t> activeArpKeys;
  std::set<uint8_t> pressedKeys;
  std::vector<ArpNote> activeArps;
  EventVector::iterator playIterator;
  bool sortRequested = false;

  void checkLoadNewArpNotes(void);
  void onSongStopped(void);
  void updateTrackChannels(bool initial = false);
  void updateInstrumentChannels(void);
  void addPendingNotes(bool incrementLayer = true);
  void refreshSongLength(void);
  void applySongStartLayerMutes(void);
  void printEvents();
  void loadNextEvent(int timeMs);
  void allTrackNotesOff(const uint8_t track);
  void allLayerNotesOff(const uint8_t track, const uint8_t layer);
  void allNotesOff(void);
  void resetTrackControllers(uint8_t track);
  void resetControllers();
  void playNextEvent(void);
  void playNextArpNote(void);
  void playArp(const midi::MidiType type, const ArpNote arp);
  bool timeQuantization(MidiEvent& e, uint8_t denom);

  void performLayerAction(LayerMode action, MidiEvent& e, uint8_t layer);
  void sortEvents(void);
  bool isGridRowEmpty(uint8_t step) const;
  int lastOccupiedGridRow() const;
  void adjustGridMarkersAfterInsert(uint8_t step, bool shiftedContent);
  void adjustGridMarkersAfterRemove(uint8_t step);
  void clampGridMarkers();

  static bool sortMidiEvent(MidiEvent& a, MidiEvent& b) {
    // + a.source is a hack to sort song events before pattern events if the have same time
    return ((a.patternNumber * 5000) + a.patternMs + a.source) < ((b.patternNumber * 5000) + b.patternMs + b.source); // FIXME: patternLengthMs
  }

};

void UI_func_livesequencer(uint8_t param);
void handle_touchscreen_live_sequencer(void);


#endif // LIVESEQUENCER_H
