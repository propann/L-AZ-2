// Environnement ui_esp : bring-up isole pour la matrice boutons+LED
// SparkFun 4x4 (via mux CD74HC4067) et les encodeurs, sur un
// ESP32-S3-DevKitC-1 nu. Ce hardware (matrice+Pico) a ete ABANDONNE le
// 2026-09-14 au profit de croix+4 boutons+3 encodeurs directs sur le
// Teensy (voir AZ2_CABLAGE_MASTER.md) -- ce fichier n'est plus dans
// default_envs (voir platformio.ini) et n'est PAS le firmware ESP32
// shippe (c'est src_esp32/az2_screen/main.cpp). Garde uniquement comme
// reference/tests isoles pour ce mux, pas comme code actif.
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <AZ2_Protocol.h>

namespace {

// Cablage v0 pour un ESP32-S3-DevKitC-1 nu (bring-up avant integration sur le
// module ecran ESP32-4848S040C_I : ce module-la n'a quasiment plus de GPIO
// libre une fois l'ecran/tactile/SD branches, a reverifier au moment venu).
// Voir docs/AZ2_CABLAGE_BASE.md.
constexpr int kTeensyTxPin = 17;  // ESP32 TX -> Teensy RX1 (pin 0)
constexpr int kTeensyRxPin = 18;  // ESP32 RX <- Teensy TX1 (pin 1)

// Mux boutons et mux LED : deux CD74HC4067 (16 voies) dont les 4 lignes
// d'adresse S0-S3 sont cablees en parallele (memes GPIO) puisque bouton et
// LED d'un meme pad_id sont toujours adresses ensemble dans scanPads().
// Seuls les signaux SIG different (un en entree, un en sortie).
constexpr int kButtonMuxS0 = 4;
constexpr int kButtonMuxS1 = 5;
constexpr int kButtonMuxS2 = 6;
constexpr int kButtonMuxS3 = 7;
constexpr int kButtonMuxSignal = 8;  // SIG du mux boutons -> entree ESP32

constexpr int kLedMuxS0 = kButtonMuxS0;
constexpr int kLedMuxS1 = kButtonMuxS1;
constexpr int kLedMuxS2 = kButtonMuxS2;
constexpr int kLedMuxS3 = kButtonMuxS3;
constexpr int kLedMuxSignal = 9;  // ESP32 -> SIG du mux LED (une LED a la fois, v0 mono)

constexpr int kSdSckPin = -1;
constexpr int kSdMisoPin = -1;
constexpr int kSdMosiPin = -1;
constexpr int kSdCsPin = -1;

// Encodeurs rotatifs 1-4 (sous l'ecran, voir AZ2_ECRAN_FACADE.md). EC11
// classiques: 2 voies quadrature A/B + bouton poussoir integre, tout en
// INPUT_PULLUP (commun cote GND). Cablage v0 sur ESP32-S3-DevKitC-1 nu.
constexpr int kEnc1APin = 10, kEnc1BPin = 11, kEnc1BtnPin = 38;
constexpr int kEnc2APin = 12, kEnc2BPin = 13, kEnc2BtnPin = 39;
constexpr int kEnc3APin = 14, kEnc3BPin = 15, kEnc3BtnPin = 40;
constexpr int kEnc4APin = 16, kEnc4BPin = 21, kEnc4BtnPin = 41;

bool padState[az2::kPadCount] = {};
bool lastRawPadState[az2::kPadCount] = {};
uint32_t lastDebounceMs[az2::kPadCount] = {};
uint32_t padDownSinceMs[az2::kPadCount] = {};
bool padHoldSent[az2::kPadCount] = {};
uint32_t lastHeartbeatMs = 0;
bool sdMounted = false;
String teensyLine;

constexpr uint32_t kPadHoldMs = 600;
constexpr uint32_t kButtonDebounceMs = 15;

// Table de transition quadrature (methode "full step" tolerante aux
// rebonds/sauts) : indexee par (etat precedent << 2 | etat courant) sur 2
// bits (A,B). Renvoie -1, 0 ou +1. Technique standard domaine public pour
// decoder un encodeur EC11 par polling.
constexpr int8_t kQuadratureTable[16] = {
    0, -1, 1,  0,
    1, 0,  0,  -1,
    -1, 0, 0,  1,
    0, 1,  -1, 0,
};

struct QuadEncoder {
  QuadEncoder(int a, int b, int btn) : pinA(a), pinB(b), pinBtn(btn) {}

  int pinA;
  int pinB;
  int pinBtn;
  uint8_t state = 0;
  int32_t accum = 0;
  bool btnState = false;
  bool btnLastRaw = false;
  uint32_t btnLastChangeMs = 0;
};

QuadEncoder encoders[4] = {
    {kEnc1APin, kEnc1BPin, kEnc1BtnPin},
    {kEnc2APin, kEnc2BPin, kEnc2BtnPin},
    {kEnc3APin, kEnc3BPin, kEnc3BtnPin},
    {kEnc4APin, kEnc4BPin, kEnc4BtnPin},
};

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

void setupEncoderPins() {
  for (const QuadEncoder &enc : encoders) {
    pinMode(enc.pinA, INPUT_PULLUP);
    pinMode(enc.pinB, INPUT_PULLUP);
    pinMode(enc.pinBtn, INPUT_PULLUP);
  }
}

void sendMacro(uint8_t index, int32_t delta) {
  az2::printMacro(Serial, index, delta);
  if (uartToTeensyConfigured()) {
    az2::printMacro(Serial1, index, delta);
  }
}

void updateEncoder(QuadEncoder &enc, uint8_t index, uint32_t now) {
  const uint8_t a = digitalRead(enc.pinA);
  const uint8_t b = digitalRead(enc.pinB);
  const uint8_t current = static_cast<uint8_t>((a << 1) | b);
  enc.state = static_cast<uint8_t>(((enc.state << 2) | current) & 0x0F);
  enc.accum += kQuadratureTable[enc.state];

  constexpr int32_t kStepsPerDetent = 4;  // EC11 typique : 4 transitions / cran
  while (enc.accum >= kStepsPerDetent) {
    enc.accum -= kStepsPerDetent;
    sendMacro(index, 1);
  }
  while (enc.accum <= -kStepsPerDetent) {
    enc.accum += kStepsPerDetent;
    sendMacro(index, -1);
  }

  const bool rawBtn = digitalRead(enc.pinBtn) == LOW;
  if (rawBtn != enc.btnLastRaw) {
    enc.btnLastRaw = rawBtn;
    enc.btnLastChangeMs = now;
  }

  if ((now - enc.btnLastChangeMs) < kButtonDebounceMs) {
    return;
  }

  if (rawBtn != enc.btnState) {
    enc.btnState = rawBtn;
    Serial.print("ENC:");
    Serial.print(index);
    Serial.println(enc.btnState ? ":BTN:DOWN" : ":BTN:UP");
  }
}

void scanEncoders() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 4; ++i) {
    updateEncoder(encoders[i], static_cast<uint8_t>(i + 1), now);
  }
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
  Serial.println("AZ2:FEATURE:ENCODERS_4X");
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
      if (raw) {
        padDownSinceMs[pad] = now;
        padHoldSent[pad] = false;
      }
    }

    if (padState[pad] && !padHoldSent[pad] && (now - padDownSinceMs[pad]) >= kPadHoldMs) {
      padHoldSent[pad] = true;
      az2::printPadHold(Serial, pad);
      if (uartToTeensyConfigured()) {
        az2::printPadHold(Serial1, pad);
      }
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
  setupEncoderPins();
  sendToTeensy(az2::kHelloControl);
}

void loop() {
  scanPads();
  scanEncoders();
  readTeensyStatus();
  heartbeat();
}
