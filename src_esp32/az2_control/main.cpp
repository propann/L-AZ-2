#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <AZ2_Protocol.h>

namespace {

// TODO: replace -1 values when the exact multiplexers and GPIO map are fixed.
constexpr int kTeensyRxPin = -1;
constexpr int kTeensyTxPin = -1;

constexpr int kButtonMuxS0 = -1;
constexpr int kButtonMuxS1 = -1;
constexpr int kButtonMuxS2 = -1;
constexpr int kButtonMuxS3 = -1;
constexpr int kButtonMuxSignal = -1;

constexpr int kLedMuxS0 = -1;
constexpr int kLedMuxS1 = -1;
constexpr int kLedMuxS2 = -1;
constexpr int kLedMuxS3 = -1;
constexpr int kLedMuxSignal = -1;

constexpr int kSdSckPin = -1;
constexpr int kSdMisoPin = -1;
constexpr int kSdMosiPin = -1;
constexpr int kSdCsPin = -1;

bool padState[az2::kPadCount] = {};
bool lastRawPadState[az2::kPadCount] = {};
uint32_t lastDebounceMs[az2::kPadCount] = {};
uint32_t lastHeartbeatMs = 0;
bool sdMounted = false;
String teensyLine;

bool validPin(int pin) {
  return pin >= 0;
}

bool uartToTeensyConfigured() {
  return validPin(kTeensyRxPin) && validPin(kTeensyTxPin);
}

bool buttonMuxConfigured() {
  return validPin(kButtonMuxS0) && validPin(kButtonMuxS1) && validPin(kButtonMuxS2) &&
         validPin(kButtonMuxS3) && validPin(kButtonMuxSignal);
}

bool ledMuxConfigured() {
  return validPin(kLedMuxS0) && validPin(kLedMuxS1) && validPin(kLedMuxS2) &&
         validPin(kLedMuxS3) && validPin(kLedMuxSignal);
}

bool sdSpiConfigured() {
  return validPin(kSdSckPin) && validPin(kSdMisoPin) && validPin(kSdMosiPin) && validPin(kSdCsPin);
}

void writeMuxAddress(int s0, int s1, int s2, int s3, uint8_t channel) {
  digitalWrite(s0, bitRead(channel, 0));
  digitalWrite(s1, bitRead(channel, 1));
  digitalWrite(s2, bitRead(channel, 2));
  digitalWrite(s3, bitRead(channel, 3));
}

void sendToTeensy(const char *message) {
  Serial.println(message);
  if (uartToTeensyConfigured()) {
    Serial1.println(message);
  }
}

void sendPadToTeensy(uint8_t pad, bool pressed) {
  az2::printPadEvent(Serial, pad, pressed);
  if (uartToTeensyConfigured()) {
    az2::printPadEvent(Serial1, pad, pressed);
  }
}

bool readPadRaw(uint8_t pad) {
  if (!buttonMuxConfigured()) {
    return false;
  }

  writeMuxAddress(kButtonMuxS0, kButtonMuxS1, kButtonMuxS2, kButtonMuxS3, pad);
  delayMicroseconds(5);
  return digitalRead(kButtonMuxSignal) == LOW;
}

void setPadLed(uint8_t pad, bool enabled) {
  if (!ledMuxConfigured()) {
    return;
  }

  writeMuxAddress(kLedMuxS0, kLedMuxS1, kLedMuxS2, kLedMuxS3, pad);
  digitalWrite(kLedMuxSignal, enabled ? HIGH : LOW);
}

void setupMuxPins() {
  if (buttonMuxConfigured()) {
    pinMode(kButtonMuxS0, OUTPUT);
    pinMode(kButtonMuxS1, OUTPUT);
    pinMode(kButtonMuxS2, OUTPUT);
    pinMode(kButtonMuxS3, OUTPUT);
    pinMode(kButtonMuxSignal, INPUT_PULLUP);
  }

  if (ledMuxConfigured()) {
    pinMode(kLedMuxS0, OUTPUT);
    pinMode(kLedMuxS1, OUTPUT);
    pinMode(kLedMuxS2, OUTPUT);
    pinMode(kLedMuxS3, OUTPUT);
    pinMode(kLedMuxSignal, OUTPUT);
  }
}

void setupWifiIdle() {
  WiFi.mode(WIFI_OFF);
  Serial.println("WIFI:OFF:BOOT");
}

void setupSdCard() {
  if (!sdSpiConfigured()) {
    Serial.println("SD:SKIP:PINS_NOT_SET");
    return;
  }

  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  sdMounted = SD.begin(kSdCsPin, SPI);
  Serial.println(sdMounted ? "SD:READY" : "SD:ERROR:MOUNT_FAILED");

  if (sdMounted && !SD.exists("/az2")) {
    SD.mkdir("/az2");
  }
}

void printBootInfo() {
  Serial.println("AZ2:ROLE:ESP32_CONTROL");
  Serial.println("AZ2:FEATURE:UI_480X480_ST7701");
  Serial.println("AZ2:FEATURE:SPARKFUN_4X4_MATRIX");
  Serial.println("AZ2:FEATURE:WIFI_READY_WHEN_CONFIGURED");
  Serial.println("AZ2:FEATURE:SD_READY_WHEN_PINS_SET");
  Serial.println("AZ2:FEATURE:RETRO_GO_RESEARCH_MODE");
}

void scanPads() {
  constexpr uint32_t kDebounceMs = 12;
  const uint32_t now = millis();

  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    const bool raw = readPadRaw(pad);

    if (raw != lastRawPadState[pad]) {
      lastRawPadState[pad] = raw;
      lastDebounceMs[pad] = now;
    }

    if ((now - lastDebounceMs[pad]) < kDebounceMs) {
      continue;
    }

    if (raw != padState[pad]) {
      padState[pad] = raw;
      setPadLed(pad, raw);
      sendPadToTeensy(pad, raw);
    }
  }
}

void heartbeat() {
  const uint32_t now = millis();
  if (now - lastHeartbeatMs < 1000) {
    return;
  }

  lastHeartbeatMs = now;
  az2::printStatus(Serial, "ESP32_CONTROL", az2::kStatusReady);
}

void handleTeensyLine(const String &line) {
  Serial.print("TEENSY:");
  Serial.println(line);
}

void readTeensyStatus() {
  if (!uartToTeensyConfigured()) {
    return;
  }

  while (Serial1.available() > 0) {
    const char c = static_cast<char>(Serial1.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      teensyLine.trim();
      if (teensyLine.length() > 0) {
        handleTeensyLine(teensyLine);
      }
      teensyLine = "";
      continue;
    }

    if (teensyLine.length() < 96) {
      teensyLine += c;
    }
  }
}

} // namespace

void setup() {
  Serial.begin(az2::kControlBaud);
  delay(300);

  printBootInfo();
  setupWifiIdle();
  setupSdCard();

  if (uartToTeensyConfigured()) {
    Serial1.begin(az2::kControlBaud, SERIAL_8N1, kTeensyRxPin, kTeensyTxPin);
  }

  setupMuxPins();
  sendToTeensy(az2::kHelloControl);
}

void loop() {
  scanPads();
  readTeensyStatus();
  heartbeat();
}
