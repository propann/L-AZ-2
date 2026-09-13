#pragma once

#include <Arduino.h>

namespace az2 {

constexpr uint32_t kControlBaud = 230400;
constexpr uint8_t kPadCount = 16;
constexpr uint8_t kPadRows = 4;
constexpr uint8_t kPadCols = 4;

constexpr const char *kHelloControl = "HELLO:ESP32_CONTROL";
constexpr const char *kHelloAudio = "HELLO:TEENSY_AUDIO";
constexpr const char *kPlay = "PLAY";
constexpr const char *kStop = "STOP";
constexpr const char *kRecToggle = "REC:TOGGLE";
constexpr const char *kStatusReady = "READY";
constexpr const char *kStatusPlaying = "PLAYING";
constexpr const char *kStatusStopped = "STOPPED";

inline uint8_t padId(uint8_t row, uint8_t col) {
  return (row * kPadCols) + col;
}

inline bool validPad(uint8_t pad) {
  return pad < kPadCount;
}

inline void printPadEvent(Print &out, uint8_t pad, bool pressed, uint8_t velocity = 110) {
  if (!validPad(pad)) {
    return;
  }

  out.print("PAD:");
  if (pad < 10) {
    out.print('0');
  }
  out.print(pad);
  out.print(pressed ? ":DOWN:vel=" : ":UP");
  if (pressed) {
    out.print(velocity);
  }
  out.println();
}

inline void printLedEvent(Print &out, uint8_t pad, const char *state) {
  if (!validPad(pad)) {
    return;
  }

  out.print("LED:");
  if (pad < 10) {
    out.print('0');
  }
  out.print(pad);
  out.print(':');
  out.println(state);
}

inline void printStatus(Print &out, const char *role, const char *state) {
  out.print("STATUS:");
  out.print(role);
  out.print(':');
  out.println(state);
}

inline void printBpm(Print &out, float bpm) {
  out.print("BPM:");
  out.println(bpm, 2);
}

inline void printPattern(Print &out, uint8_t pattern) {
  out.print("PATTERN:");
  if (pattern < 10) {
    out.print('0');
  }
  out.println(pattern);
}

inline void printClock(Print &out, uint16_t bar, uint8_t step) {
  out.print("CLOCK:bar=");
  out.print(bar);
  out.print(":step=");
  out.println(step);
}

} // namespace az2
