#include <Arduino.h>
#include <AZ2_Protocol.h>

namespace {
// Configuration actuelle du câblage : SWITCH1-4 sur GPIO2-5,
// SW-GND1-4 sur GPIO6-9. Les diodes du PCB imposent ce sens de scan.
constexpr uint8_t kColumnPins[4] = {6, 7, 8, 9};
constexpr uint8_t kRowPins[4] = {2, 3, 4, 5};
// LED matrix: four LED cathode columns, then RED/GREEN/BLUE rows.
// GP23/GP24 are not exposed on the Pico, so the last blue row uses GP28.
constexpr uint8_t kLedColumnPins[4] = {10, 11, 12, 13};
constexpr uint8_t kLedRedPins[4] = {14, 15, 16, 17};
constexpr uint8_t kLedGreenPins[4] = {18, 19, 20, 21};
constexpr uint8_t kLedBluePins[4] = {22, 26, 27, 28};
constexpr uint32_t kDebounceMs = 12;
constexpr uint32_t kUartBaud = 230400;
constexpr uint8_t kLedPin = LED_BUILTIN;
bool rawState[16] = {};
bool stableState[16] = {};
uint32_t changedAt[16] = {};

void releaseColumns() {
  for (uint8_t pin : kColumnPins) pinMode(pin, INPUT);
}

void scanMatrix() {
  const uint32_t now = millis();
  for (uint8_t col = 0; col < 4; ++col) {
    releaseColumns();
    pinMode(kColumnPins[col], OUTPUT);
    digitalWrite(kColumnPins[col], LOW);
    delayMicroseconds(5);
    for (uint8_t row = 0; row < 4; ++row) {
      const uint8_t pad = az2::padId(row, col);
      const bool pressed = digitalRead(kRowPins[row]) == LOW;
      if (pressed != rawState[pad]) {
        rawState[pad] = pressed;
        changedAt[pad] = now;
      }
      if (pressed != stableState[pad] && now - changedAt[pad] >= kDebounceMs) {
        stableState[pad] = pressed;
        bool anyPressed = false;
        for (bool state : stableState) anyPressed |= state;
        digitalWrite(kLedPin, anyPressed ? HIGH : LOW);
        az2::printPadEvent(Serial, pad, pressed);
        az2::printPadEvent(Serial1, pad, pressed);
      }
    }
  }
  releaseColumns();
}

void allLedsOff() {
  for (uint8_t pin : kLedColumnPins) {
    pinMode(pin, INPUT);
  }
  for (uint8_t pin : kLedRedPins) digitalWrite(pin, LOW);
  for (uint8_t pin : kLedGreenPins) digitalWrite(pin, LOW);
  for (uint8_t pin : kLedBluePins) digitalWrite(pin, LOW);
}

void scanLeds() {
  static uint8_t column = 0;
  allLedsOff();
  for (uint8_t pin : kLedRedPins) pinMode(pin, OUTPUT);
  pinMode(kLedColumnPins[column], OUTPUT);
  digitalWrite(kLedColumnPins[column], LOW);
  for (uint8_t row = 0; row < 4; ++row) {
    const uint8_t pad = az2::padId(row, column);
    digitalWrite(kLedRedPins[row], stableState[pad] ? HIGH : LOW);
  }
  column = (column + 1) & 0x03;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial1.begin(kUartBaud);
  uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 2000) {}
  for (uint8_t pin : kRowPins) pinMode(pin, INPUT_PULLUP);
  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, LOW);
  for (uint8_t pin : kLedRedPins) pinMode(pin, OUTPUT);
  for (uint8_t pin : kLedGreenPins) pinMode(pin, OUTPUT);
  for (uint8_t pin : kLedBluePins) pinMode(pin, OUTPUT);
  allLedsOff();
  releaseColumns();
  Serial.println("AZ2:ROLE:PICO_KEYPAD");
  Serial.println("AZ2:FEATURE:SPARKFUN_4X4_MATRIX");
  Serial.println("AZ2:PHASE:MATRIX_ONLY");
  Serial.println("AZ2:WIRING:ROW=GP2,3,4,5 COL=GP6,7,8,9");
  Serial.println("AZ2:UART:GPIO0_TX GPIO1_RX BAUD=230400");
  Serial.println("AZ2:INFO:ENCODERS_DISABLED");
  Serial1.println("HELLO:PICO_KEYPAD");
}

void loop() {
  scanMatrix();
  scanLeds();
}
