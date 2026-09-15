// AZ-2 - Firmware Pico (RP2040) : clavier 4x4, LEDs RGB (via mux), encodeurs.
//
// Pourquoi un 3e cerveau: le module ecran ESP32-4848S040C_I n'a presque plus
// de GPIO libre une fois ecran+tactile+SD comptes (voir
// docs/AZ2_ECRAN_FACADE.md). Le Pico (26 GPIO utilisables) prend donc en
// charge tout le clavier physique et l'envoie directement au Teensy en UART.
//
// Cablage matrice boutons (voir docs/AZ2_CABLAGE_PICO.md pour le detail
// complet) : la carte SparkFun 4x4 est une VRAIE matrice ligne/colonne (pas
// 16 boutons independants) -- 4 colonnes pilotees + 4 lignes lues, en
// direct sur le Pico (assez de GPIO, pas besoin de mux pour les boutons).
//   - Appuyer sur un bouton relie sa ligne a sa colonne (contact).
//   - Scan: on met une colonne a LOW (les autres en Hi-Z), on lit les 4
//     lignes (INPUT_PULLUP) ; une ligne a LOW = bouton (ligne,colonne) appuye.
//
// Cablage LED RGB (notice SparkFun officielle -
// https://learn.sparkfun.com/tutorials/button-pad-hookup-guide/all) : "les
// LED sont montees comme trois matrices 4x4 superposees, une par couleur" --
// 4 colonnes cathodes COMMUNES (les memes que les boutons) + 12 lignes
// d'anode (4 par couleur: rouge, vert, bleu). Avec un multiplexeur 16 voies
// (CD74HC4067) sur les 12 lignes d'anode (canaux 0-11, 4 lignes select
// S0-S3 + 1 signal), on adresse "couleur*4+ligne" au lieu de cabler 12 fils
// directs. Attention: la notice SparkFun signale une erreur de reperage
// Vert/Bleu inversee sur certains lots -- si les couleurs sont echangees a
// l'usage, c'est probablement ca.

#include <Arduino.h>
#include <AZ2_Protocol.h>

namespace {

// --- UART vers le Teensy (Serial3 cote Teensy, pins 14/15) ---
// Pins par defaut de Serial1 sur ce framework (UART0) : TX=GPIO0, RX=GPIO1.

// --- Matrice boutons/LED : colonnes pilotees, lignes lues ---
// COL_0..COL_3 = pads {0,4,8,12} / {1,5,9,13} / {2,6,10,14} / {3,7,11,15}
// (nommage AZ-2, voir AZ2_CABLAGE_BASE.md). Verifier ces 4 fils contre le
// guide SparkFun (colonnes = cathodes LED communes, pas de vrais GND).
constexpr int kColPins[4] = {2, 3, 4, 5};
// ROW_0..ROW_3 = pads {0,1,2,3} / {4,5,6,7} / {8,9,10,11} / {12,13,14,15}
constexpr int kRowPins[4] = {6, 7, 8, 9};

// --- Mux LED (CD74HC4067) : 4 lignes d'adresse + 1 signal, sur les 12
// lignes d'anode RGB (canal = couleur*4 + ligne, canaux 12-15 inutilises).
constexpr int kLedMuxS0 = 10, kLedMuxS1 = 11, kLedMuxS2 = 12, kLedMuxS3 = 13;
constexpr int kLedMuxSignal = 14;  // vers l'entree commune du mux (avec resistance serie)
constexpr uint8_t kColorCount = 3;  // 0=rouge, 1=vert, 2=bleu (voir errata SparkFun ci-dessus)

// --- Encodeurs rotatifs (EC11: A/B quadrature + bouton) ---
// Encodeur 3 retire (pas cable) : GPIO21/22/25 libres pour l'instant.
// GPIO23/24 a eviter si on en rajoute un (23 = mode alim SMPS, 24 =
// detection VBUS) -- reserves a la carte, pas de risque avec 21/22/25.
constexpr int kEnc1APin = 15, kEnc1BPin = 16, kEnc1BtnPin = 17;
constexpr int kEnc2APin = 18, kEnc2BPin = 19, kEnc2BtnPin = 20;
constexpr int kEnc4APin = 26, kEnc4BPin = 27, kEnc4BtnPin = 28;

constexpr uint32_t kPadHoldMs = 600;
constexpr uint32_t kButtonDebounceMs = 15;
constexpr uint32_t kPadDebounceMs = 12;
constexpr uint32_t kColumnSettleUs = 5;

bool padState[az2::kPadCount] = {};
bool lastRawPadState[az2::kPadCount] = {};
uint32_t lastDebounceMs[az2::kPadCount] = {};
uint32_t padDownSinceMs[az2::kPadCount] = {};
bool padHoldSent[az2::kPadCount] = {};
// Masque couleur "musical" par pad (bit0=rouge, bit1=vert, bit2=bleu),
// pilote par la confirmation du Teensy (LED:NN:ON/OFF) -- reste affiche
// tant que le pad n'est PAS physiquement presse (voir effectiveColorMask).
uint8_t teensyColorMask[az2::kPadCount] = {};
uint32_t lastHeartbeatMs = 0;
String teensyLine;

// Table de transition quadrature ("full step", tolerante aux rebonds).
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

// Encodeur 3 pas cable pour l'instant : pour le rebrancher plus tard,
// declarer kEnc3APin/BPin/BtnPin (voir commentaire plus haut, GPIO21/22/25
// libres) et rajouter une ligne ici + son label dans kEncoderLabels.
QuadEncoder encoders[] = {
    {kEnc1APin, kEnc1BPin, kEnc1BtnPin},
    {kEnc2APin, kEnc2BPin, kEnc2BtnPin},
    {kEnc4APin, kEnc4BPin, kEnc4BtnPin},
};
constexpr uint8_t kEncoderLabels[] = {1, 2, 4};  // garde le meme numero par encodeur physique
constexpr uint8_t kEncoderCount = sizeof(encoders) / sizeof(encoders[0]);

void sendToTeensy(const char *message) {
  Serial.println(message);
  Serial1.println(message);
}

void sendPadEvent(uint8_t pad, bool pressed) {
  az2::printPadEvent(Serial, pad, pressed);
  az2::printPadEvent(Serial1, pad, pressed);
}

void sendPadHold(uint8_t pad) {
  az2::printPadHold(Serial, pad);
  az2::printPadHold(Serial1, pad);
}

void sendMacro(uint8_t index, int32_t delta) {
  az2::printMacro(Serial, index, delta);
  az2::printMacro(Serial1, index, delta);
}

void releaseAllColumns() {
  for (int col : kColPins) {
    pinMode(col, INPUT);
  }
}

void writeLedMuxChannel(uint8_t channel) {
  digitalWrite(kLedMuxS0, bitRead(channel, 0));
  digitalWrite(kLedMuxS1, bitRead(channel, 1));
  digitalWrite(kLedMuxS2, bitRead(channel, 2));
  digitalWrite(kLedMuxS3, bitRead(channel, 3));
}

// Allume brievement (quelques dizaines de us) le canal mux d'une couleur/
// ligne donnee. Appele une fois par couleur active pendant la colonne en
// cours -- le balayage colonne x couleur x ligne donne l'effet "allume en
// continu" par persistance retinienne (POV), comme pour le mono v0.
void pulseLedChannel(uint8_t color, uint8_t row) {
  writeLedMuxChannel(static_cast<uint8_t>(color * 4 + row));
  digitalWrite(kLedMuxSignal, HIGH);
  delayMicroseconds(150);
  digitalWrite(kLedMuxSignal, LOW);
}

// Mode test hardware (voir AZ2_CABLAGE_BASE.md, "Tests de cablage v0") :
// un pad physiquement presse s'allume tout de suite en blanc, meme sans
// Teensy branche -- ca prime sur l'etat "musical" venant du Teensy, qui ne
// reste visible que quand le pad n'est pas presse (ex: futur indicateur de
// step de sequenceur).
uint8_t effectiveColorMask(uint8_t pad) {
  return padState[pad] ? 0b111 : teensyColorMask[pad];
}

// Boutons et LED de la matrice sont scannes en 2 PASSES SEPAREES (et non
// colonne par colonne en alternant les deux) : voir le rapport temps reel
// du 2026-09-14 -- avec l'ancien code, appuyer sur un pad de la colonne 3
// attendait que les colonnes 0-2 aient fini boutons+LED (les pulses LED
// coutent jusqu'a ~1.8ms/colonne si plusieurs sont allumees), soit du
// "temps mort" ajoute avant meme l'envoi de l'evenement au Teensy. En
// lisant TOUTES les colonnes (rapide, pas de delay()) avant de faire la
// phase LED (lente), un appui est detecte et envoye des le debut du tour
// de loop(), jamais retarde par l'affichage.

// Passe 1/2 : lecture pure des boutons de la colonne (rapide, aucun delay).
void scanColumnButtons(uint8_t col) {
  const uint32_t now = millis();

  releaseAllColumns();
  pinMode(kColPins[col], OUTPUT);
  digitalWrite(kColPins[col], LOW);
  delayMicroseconds(kColumnSettleUs);

  for (uint8_t row = 0; row < 4; ++row) {
    const uint8_t pad = az2::padId(row, col);
    const bool raw = digitalRead(kRowPins[row]) == LOW;

    if (raw != lastRawPadState[pad]) {
      lastRawPadState[pad] = raw;
      lastDebounceMs[pad] = now;
    }

    if ((now - lastDebounceMs[pad]) >= kPadDebounceMs && raw != padState[pad]) {
      padState[pad] = raw;
      sendPadEvent(pad, raw);
      if (raw) {
        padDownSinceMs[pad] = now;
        padHoldSent[pad] = false;
      }
    }

    if (padState[pad] && !padHoldSent[pad] && (now - padDownSinceMs[pad]) >= kPadHoldMs) {
      padHoldSent[pad] = true;
      sendPadHold(pad);
    }
  }
}

// Passe 2/2 : rafraichissement LED de la colonne (lent, delayMicroseconds
// par LED allumee) -- appelee APRES que les 4 colonnes aient deja ete
// scannees pour les boutons, donc jamais sur le chemin critique d'un appui.
void scanColumnLeds(uint8_t col) {
  releaseAllColumns();
  pinMode(kColPins[col], OUTPUT);
  digitalWrite(kColPins[col], LOW);
  delayMicroseconds(kColumnSettleUs);

  for (uint8_t color = 0; color < kColorCount; ++color) {
    for (uint8_t row = 0; row < 4; ++row) {
      const uint8_t pad = az2::padId(row, col);
      if (bitRead(effectiveColorMask(pad), color)) {
        pulseLedChannel(color, row);
      }
    }
  }
}

void scanMatrix() {
  for (uint8_t col = 0; col < 4; ++col) {
    scanColumnButtons(col);
  }
  for (uint8_t col = 0; col < 4; ++col) {
    scanColumnLeds(col);
  }
}

// ---------------------------------------------------------------------
// Mode diagnostic LEDTEST (demande le 2026-09-14, "etudier pourquoi on a
// pas les led") : suspend le scan normal et maintient UN SEUL canal du
// mux allume en continu (pas en pulse POV de 150us -- trop bref pour
// verifier au multimetre ou meme a l'oeil de facon fiable), colonne par
// colonne, canal par canal, avec le detail imprime en clair. Se pilote
// depuis le moniteur serie USB du Pico (pas besoin du Teensy) :
//   LEDTEST        -- demarre le defilement automatique (~0,7s/canal)
//   LEDTEST:ALL    -- pareil mais les 4 colonnes ensemble (jusqu'a 4 LED
//                     a la fois, pratique si une seule LED est HS)
//   LEDTEST:STOP   -- arrete, revient au scan normal
// ---------------------------------------------------------------------
bool ledTestActive = false;
bool ledTestAllColumns = false;  // mode LEDTEST:ALL -- voir plus bas
uint8_t ledTestColumn = 0;
uint8_t ledTestChannel = 0;
uint32_t ledTestLastChangeMs = 0;
constexpr uint32_t kLedTestHoldMs = 700;

void ledTestEnter(bool allColumns) {
  ledTestActive = true;
  ledTestAllColumns = allColumns;
  ledTestColumn = 0;
  ledTestChannel = 0;
  ledTestLastChangeMs = 0;  // force l'affichage immediat du premier canal
  releaseAllColumns();
  digitalWrite(kLedMuxSignal, LOW);
  if (allColumns) {
    Serial.println("LEDTEST:START (4 colonnes ensemble) -- jusqu'a 4 LED allumees a la fois");
  } else {
    Serial.println("LEDTEST:START -- une LED doit rester allumee en continu a chaque etape");
  }
}

void ledTestExit() {
  ledTestActive = false;
  digitalWrite(kLedMuxSignal, LOW);
  releaseAllColumns();
  Serial.println("LEDTEST:STOP");
}

void ledTestStep() {
  const uint32_t now = millis();
  if (now - ledTestLastChangeMs < kLedTestHoldMs) {
    return;
  }
  ledTestLastChangeMs = now;

  digitalWrite(kLedMuxSignal, LOW);
  releaseAllColumns();

  const uint8_t color = static_cast<uint8_t>(ledTestChannel / 4);
  const uint8_t row = static_cast<uint8_t>(ledTestChannel % 4);
  static const char *const kColorNames[3] = {"ROUGE", "VERT", "BLEU"};

  Serial.print("LEDTEST:ligne=");
  Serial.print(row);
  Serial.print(":couleur=");
  Serial.print(kColorNames[color]);

  if (ledTestAllColumns) {
    // Les 4 colonnes en meme temps : jusqu'a 4 LED (une par colonne, meme
    // ligne/couleur) allumees simultanement -- pour maximiser la chance
    // de voir quelque chose si une seule LED est peut-etre HS.
    Serial.println(":colonnes=0,1,2,3");
    for (int col : kColPins) {
      pinMode(col, OUTPUT);
      digitalWrite(col, LOW);
    }
  } else {
    const uint8_t pad = az2::padId(row, ledTestColumn);
    Serial.print(":pad=");
    if (pad < 10) {
      Serial.print('0');
    }
    Serial.print(pad);
    Serial.print(":col=");
    Serial.println(ledTestColumn);
    pinMode(kColPins[ledTestColumn], OUTPUT);
    digitalWrite(kColPins[ledTestColumn], LOW);
  }

  writeLedMuxChannel(ledTestChannel);
  digitalWrite(kLedMuxSignal, HIGH);

  ++ledTestChannel;
  if (ledTestChannel >= 12) {  // canaux 12-15 du mux inutilises (voir doc)
    ledTestChannel = 0;
    ledTestColumn = static_cast<uint8_t>((ledTestColumn + 1) % 4);
  }
}

void readUsbCommands() {
  static String line;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line.trim();
      if (line == "LEDTEST") {
        ledTestEnter(false);
      } else if (line == "LEDTEST:ALL") {
        ledTestEnter(true);
      } else if (line == "LEDTEST:STOP") {
        ledTestExit();
      }
      line = "";
      continue;
    }
    if (line.length() < 32) {
      line += c;
    }
  }
}

// A appeler quand le Teensy confirme l'etat d'un pad (message LED:NN:ON/OFF)
// pour que le clavier reflete l'etat musical, pas seulement l'appui brut.
// Le protocole AZ2 actuel ne transporte pas encore de couleur (juste
// ON/OFF) : ON allume les 3 couleurs (blanc), histoire d'exercer les 12
// canaux du mux des maintenant. A affiner (rouge=mute, vert=play...) une
// fois qu'un vrai champ couleur existera dans le protocole.
void setPadLedFromTeensy(uint8_t pad, bool on) {
  if (az2::validPad(pad)) {
    teensyColorMask[pad] = on ? 0b111 : 0b000;
  }
}

void updateEncoder(QuadEncoder &enc, uint8_t index, uint32_t now) {
  const uint8_t a = digitalRead(enc.pinA);
  const uint8_t b = digitalRead(enc.pinB);
  const uint8_t current = static_cast<uint8_t>((a << 1) | b);
  enc.state = static_cast<uint8_t>(((enc.state << 2) | current) & 0x0F);
  enc.accum += kQuadratureTable[enc.state];

  constexpr int32_t kStepsPerDetent = 4;
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
    Serial1.print("ENC:");
    Serial1.print(index);
    Serial1.println(enc.btnState ? ":BTN:DOWN" : ":BTN:UP");
  }
}

void scanEncoders() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    updateEncoder(encoders[i], kEncoderLabels[i], now);
  }
}

void setupMatrixPins() {
  for (int row : kRowPins) {
    pinMode(row, INPUT_PULLUP);
  }
  releaseAllColumns();

  pinMode(kLedMuxS0, OUTPUT);
  pinMode(kLedMuxS1, OUTPUT);
  pinMode(kLedMuxS2, OUTPUT);
  pinMode(kLedMuxS3, OUTPUT);
  pinMode(kLedMuxSignal, OUTPUT);
  digitalWrite(kLedMuxSignal, LOW);
}

void setupEncoderPins() {
  for (const QuadEncoder &enc : encoders) {
    pinMode(enc.pinA, INPUT_PULLUP);
    pinMode(enc.pinB, INPUT_PULLUP);
    pinMode(enc.pinBtn, INPUT_PULLUP);
  }
}

void printBootInfo() {
  Serial.println("AZ2:ROLE:PICO_KEYPAD");
  Serial.println("AZ2:FEATURE:SPARKFUN_4X4_MATRIX");
  Serial.println("AZ2:FEATURE:LED_RGB_MUX");
  Serial.println("AZ2:FEATURE:ENCODERS_4X");
}

// LED embarquee de la Pico (GPIO25, libre depuis le retrait de
// l'encodeur 3) : clignote pour un test visuel "il tourne" sans avoir
// besoin du port serie -- allumee = vivant, clignote = boucle active.
constexpr int kOnboardLedPin = 25;

void heartbeat() {
  const uint32_t now = millis();

  // Clignote 2x/seconde, independamment du heartbeat serie (1x/seconde).
  digitalWrite(kOnboardLedPin, (now / 250) % 2);

  if (now - lastHeartbeatMs < 1000) {
    return;
  }

  lastHeartbeatMs = now;
  az2::printStatus(Serial, "PICO_KEYPAD", az2::kStatusReady);
}

void handleTeensyLine(const String &line) {
  Serial.print("TEENSY:");
  Serial.println(line);

  if (line.startsWith("LED:") && line.length() >= 9) {
    const uint8_t pad = static_cast<uint8_t>(line.substring(4, 6).toInt());
    const bool on = line.endsWith("ON");
    setPadLedFromTeensy(pad, on);
  }
}

void readTeensyStatus() {
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

}  // namespace

void setup() {
  Serial.begin(az2::kControlBaud);
  Serial1.begin(az2::kControlBaud);  // UART0 par defaut: TX=GPIO0, RX=GPIO1
  pinMode(kOnboardLedPin, OUTPUT);
  delay(300);

  printBootInfo();
  setupMatrixPins();
  setupEncoderPins();
  sendToTeensy(az2::kHelloKeypad);
}

void loop() {
  readUsbCommands();
  if (ledTestActive) {
    ledTestStep();
  } else {
    scanMatrix();
    scanEncoders();
  }
  readTeensyStatus();
  heartbeat();
}
