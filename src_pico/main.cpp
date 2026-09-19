// AZ-3 - Firmware du Pico : PANNEAU DE CONTROLE COMPLET.
//
// --------------------------------------------------------------------
// ROLE DANS L'AZ-3
// --------------------------------------------------------------------
// Trois cerveaux, trois roles nets :
//   ESP32-S3   -> ecran 480x480, Wi-Fi, carte SD, projets, emulateur GB
//   Teensy 4.1 -> moteurs audio, sequenceur, DAC PCM5102A, rack AZ-BUS
//   Pico       -> TOUTE la facade : matrice, encodeurs, boutons, LED
//
// Le Teensy ne garde aucune commande : ses 17 broches de croix / A-B-C-D /
// encodeurs sont liberees pour le rack. Ce firmware est donc le seul organe
// de commande de la machine.
//
// --------------------------------------------------------------------
// LE CHOIX QUI A EVITE DE REECRIRE L'INTERFACE
// --------------------------------------------------------------------
// La croix directionnelle et les boutons A/B/C/D n'existent plus en
// materiel. Leurs messages NAV: et BTN:, eux, continuent d'exister : ce
// firmware les SYNTHETISE depuis les encodeurs et les pads.
//
// L'UI de l'ESP32 (4680 lignes) consomme deja ce vocabulaire pour naviguer
// dans les menus, deplacer le curseur du tracker et piloter l'emulateur. En
// gardant le protocole identique, elle n'a eu besoin d'AUCUNE modification.
//
// --------------------------------------------------------------------
// HISTORIQUE
// --------------------------------------------------------------------
// Ce Pico avait ete tente puis abandonne le 2026-09-14 : le mux LED n'avait
// jamais allume une seule LED. Cause identifiee (broche EN en l'air) mais
// correction jamais reverifiee. L'AZ-3 le reprend en changeant la reponse au
// probleme de fond : un CD74HC4067 n'est pas un driver de LED, il ne laisse
// passer qu'un canal a la fois. Les LED passent donc sur un IS31FL3731 en
// I2C, qui balaye en materiel a courant constant ; le mux est recycle pour
// ce qu'il fait bien -- lire des boutons.
//
// Brochage   : docs/AZ3_CABLAGE_PANNEAU.md
// Conception : docs/AZ3_PANNEAU_PICO.md

#include <Arduino.h>
#include <AZ2_Protocol.h>

#include "az3_led_driver.h"
#include "az3_panel_config.h"
#include "az3_panel_io.h"

namespace {

using namespace az3;

// =====================================================================
// ETAT
// =====================================================================

// --- Pads ---
bool padState[az2::kPadCount] = {};
bool padLastRaw[az2::kPadCount] = {};
uint32_t padDebounceMs[az2::kPadCount] = {};
uint32_t padDownSinceMs[az2::kPadCount] = {};
bool padHoldSent[az2::kPadCount] = {};

// Masque couleur "musical" par pad (bit0 rouge, bit1 vert, bit2 bleu), pilote
// par le Teensy. Reste affiche tant que le pad n'est pas physiquement presse.
uint8_t padColorMask[az2::kPadCount] = {};

// --- Encodeurs ---
// Table de transition de quadrature ("full step", tolerante aux rebonds).
constexpr int8_t kQuadratureTable[16] = {
    0, -1, 1,  0,
    1, 0,  0,  -1,
    -1, 0, 0,  1,
    0, 1,  -1, 0,
};

struct EncoderState {
  uint8_t quadrature = 0;      // 2 derniers etats A/B empiles
  int32_t accum = 0;           // transitions depuis le dernier cran
  int16_t pendingDetents = 0;  // crans franchis, pas encore traduits
  int32_t totalDetents = 0;    // compteur cumule, pour ENCTEST
  uint8_t potValue = 0;        // role Pot : valeur absolue 0-127
  uint8_t potLastSent = 255;   // 255 = jamais envoye
  uint32_t potLastSentMs = 0;
};
EncoderState encoders[kEncoderCount];

// --- Boutons du mux ---
bool muxState[kMuxChannelCount] = {};
bool muxLastRaw[kMuxChannelCount] = {};
uint32_t muxDebounceMs[kMuxChannelCount] = {};

// --- Modes ---
// Mode manette : les pads deviennent une croix + boutons GB (voir
// kGamepadMap). Un encodeur ne peut pas MAINTENIR une direction, les pads si.
bool gamepadMode = false;
bool shiftHeld = false;
bool recording = false;

uint32_t lastHeartbeatMs = 0;
String teensyLine;

// =====================================================================
// EMISSION
// =====================================================================
// Tout part en double : USB (diagnostic au moniteur serie, utilisable sans
// Teensy branche) et UART (le vrai destinataire).

void send(const char *message) {
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

void sendNav(const char *direction, bool pressed) {
  az2::printNav(Serial, direction, pressed);
  az2::printNav(Serial1, direction, pressed);
}

// Impulsion de croix synthetique : un cran d'encodeur = un appui suivi d'un
// relachement immediat. L'ESP32 agit sur :DOWN et ne fait que memoriser
// l'etat sur :UP, donc une impulsion suffit pour la navigation.
void sendNavPulse(const char *direction) {
  sendNav(direction, true);
  sendNav(direction, false);
}

void sendBtn(char letter, bool pressed) {
  az2::printBtn(Serial, letter, pressed);
  az2::printBtn(Serial1, letter, pressed);
}

void sendEnc(uint8_t index, bool pressed) {
  az2::printEnc(Serial, index, pressed);
  az2::printEnc(Serial1, index, pressed);
}

void sendPot(uint8_t index, uint8_t value) {
  az2::printPot(Serial, index, value);
  az2::printPot(Serial1, index, value);
}

void sendMacro(uint8_t index, int32_t delta) {
  az2::printMacro(Serial, index, delta);
  az2::printMacro(Serial1, index, delta);
}

// =====================================================================
// PADS
// =====================================================================

// Un pad physiquement presse s'allume tout de suite en blanc, meme sans
// Teensy branche -- ca prime sur l'etat musical, qui ne reste visible que
// quand le pad n'est pas presse. C'est le test le plus rapide du cablage.
uint8_t effectiveColorMask(uint8_t pad) {
  return padState[pad] ? 0b111 : padColorMask[pad];
}

void refreshLedFrame() {
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    led::setPadColor(pad, effectiveColorMask(pad), kLedBrightness);
  }
}

// Traduit un appui de pad selon le mode courant.
void emitPadEvent(uint8_t pad, bool pressed) {
  if (!gamepadMode) {
    sendPadEvent(pad, pressed);
    return;
  }

  // Mode manette : le pad prend la place d'une commande Game Boy. Le mapping
  // colle a ce que l'ESP32 attend deja, aucune modification cote ecran.
  const GamepadBinding &binding = kGamepadMap[pad];
  switch (binding.action) {
    case GamepadAction::Nav:
      sendNav(binding.navDirection, pressed);
      break;
    case GamepadAction::Btn:
      sendBtn(binding.letter, pressed);
      break;
    case GamepadAction::Enc:
      sendEnc(binding.encIndex, pressed);
      break;
    case GamepadAction::None:
      break;  // pad sans role en mode manette : silencieux
  }
}

void scanPads() {
  const uint32_t now = millis();

  for (uint8_t col = 0; col < 4; ++col) {
    io::driveColumn(col);
    bool rows[4];
    io::readColumnRows(rows);

    for (uint8_t row = 0; row < 4; ++row) {
      const uint8_t pad = az2::padId(row, col);
      const bool raw = rows[row];

      if (raw != padLastRaw[pad]) {
        padLastRaw[pad] = raw;
        padDebounceMs[pad] = now;
      }

      if ((now - padDebounceMs[pad]) >= kPadDebounceMs && raw != padState[pad]) {
        padState[pad] = raw;
        emitPadEvent(pad, raw);
        if (raw) {
          padDownSinceMs[pad] = now;
          padHoldSent[pad] = false;
        }
      }

      // HOLD n'a pas de sens en mode manette : tenir une direction est
      // justement le comportement normal.
      if (!gamepadMode && padState[pad] && !padHoldSent[pad] &&
          (now - padDownSinceMs[pad]) >= kPadHoldMs) {
        padHoldSent[pad] = true;
        sendPadHold(pad);
      }
    }
  }

  io::releaseAllColumns();
}

// =====================================================================
// ENCODEURS
// =====================================================================

// Echantillonnage de la quadrature. Ne fait QUE de l'arithmetique, aucune
// emission serie : il peut donc etre appele n'importe ou sans allonger un
// traitement en cours. Voir loop() pour le placement.
void pollEncoders() {
  const uint16_t lines = io::readEncoderLines();

  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    EncoderState &st = encoders[i];
    const uint8_t current = static_cast<uint8_t>((lines >> (i * 2)) & 0x03);
    st.quadrature = static_cast<uint8_t>(((st.quadrature << 2) | current) & 0x0F);
    st.accum += kQuadratureTable[st.quadrature];

    while (st.accum >= kQuadratureStepsPerDetent) {
      st.accum -= kQuadratureStepsPerDetent;
      ++st.pendingDetents;
      ++st.totalDetents;
    }
    while (st.accum <= -kQuadratureStepsPerDetent) {
      st.accum += kQuadratureStepsPerDetent;
      --st.pendingDetents;
      --st.totalDetents;
    }
  }
}

void applyDetent(uint8_t index, int32_t direction) {
  const EncoderConfig &cfg = kEncoders[index];
  EncoderState &st = encoders[index];

  switch (cfg.role) {
    case EncoderRole::NavVertical:
      sendNavPulse(direction > 0 ? "DOWN" : "UP");
      break;

    case EncoderRole::NavHorizontal:
      sendNavPulse(direction > 0 ? "RIGHT" : "LEFT");
      break;

    case EncoderRole::Pot: {
      const int32_t next =
          static_cast<int32_t>(st.potValue) + direction * kPotStepPerDetent;
      st.potValue = static_cast<uint8_t>(constrain(next, 0, 127));
      break;  // emission differee, voir flushPot()
    }

    case EncoderRole::Macro:
      sendMacro(cfg.roleIndex, direction);
      break;
  }
}

// Le role Pot emet une valeur ABSOLUE, limitee en debit -- separe de
// applyDetent() pour n'envoyer qu'une fois la rotation calmee.
void flushPot(uint8_t index, uint32_t now) {
  const EncoderConfig &cfg = kEncoders[index];
  EncoderState &st = encoders[index];

  if (cfg.role != EncoderRole::Pot || st.potValue == st.potLastSent) {
    return;
  }
  if (now - st.potLastSentMs < kPotMinIntervalMs) {
    return;
  }

  st.potLastSent = st.potValue;
  st.potLastSentMs = now;
  sendPot(cfg.roleIndex, st.potValue);
}

void drainEncoders(uint32_t now) {
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    int16_t detents = encoders[i].pendingDetents;
    encoders[i].pendingDetents = 0;
    for (; detents > 0; --detents) {
      applyDetent(i, 1);
    }
    for (; detents < 0; ++detents) {
      applyDetent(i, -1);
    }
    flushPot(i, now);
  }
}

// =====================================================================
// BOUTONS (via le mux)
// =====================================================================

void onButtonEdge(uint8_t channel, bool pressed) {
  // Clic d'encodeur : remplace le bouton A/B/C/D disparu, ou evenement brut.
  if (channel < kEncoderCount) {
    const char letter = kEncoders[channel].buttonLetter;
    if (letter != '\0') {
      sendBtn(letter, pressed);
    } else {
      sendEnc(channel, pressed);
    }
    return;
  }

  switch (channel) {
    case kBtnMuxChannelPlay:
      if (pressed) {
        send(az2::kPlay);
      }
      break;
    case kBtnMuxChannelStop:
      if (pressed) {
        send(az2::kStop);
      }
      break;
    case kBtnMuxChannelRec:
      // Le Teensy attend REC:START puis REC:STOP (voir handleRecCommand()) :
      // un seul bouton bascule entre les deux.
      if (pressed) {
        recording = !recording;
        send(recording ? "REC:START" : "REC:STOP");
      }
      break;
    case kBtnMuxChannelShift:
      shiftHeld = pressed;
      break;
    default:
      break;  // canal libre : cable plus tard
  }
}

void scanButtons(uint32_t now) {
  for (uint8_t channel = 0; channel < kMuxChannelCount; ++channel) {
    const bool raw = io::readButtonChannel(channel);

    if (raw != muxLastRaw[channel]) {
      muxLastRaw[channel] = raw;
      muxDebounceMs[channel] = now;
    }
    if ((now - muxDebounceMs[channel]) >= kButtonDebounceMs &&
        raw != muxState[channel]) {
      muxState[channel] = raw;
      onButtonEdge(channel, raw);
    }
  }
}

// =====================================================================
// DIAGNOSTICS
// =====================================================================
// Tous pilotables au moniteur serie USB, SANS Teensy branche. C'est
// deliberement fourni : la panne LED de 2026-09 a coute le projet faute de
// pouvoir isoler le bloc fautif.

void cmdMuxTest() {
  Serial.print("MUXTEST:");
  for (uint8_t channel = 0; channel < kMuxChannelCount; ++channel) {
    Serial.print(io::readButtonChannel(channel) ? '1' : '0');
  }
  Serial.println();
  Serial.print("MUXTEST:1=contact ferme. Canaux 0-");
  Serial.print(kEncoderCount - 1);
  Serial.println(" = clics encodeurs, 8=PLAY 9=STOP 10=REC 11=SHIFT.");
  Serial.println("MUXTEST:si les 16 restent figes -> broche EN du mux pas au GND.");
}

void cmdEncTest() {
  const uint16_t lines = io::readEncoderLines();
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    Serial.print("ENCTEST:");
    Serial.print(i);
    Serial.print(":A=");
    Serial.print((lines >> (i * 2)) & 1);
    Serial.print(":B=");
    Serial.print((lines >> (i * 2 + 1)) & 1);
    Serial.print(":crans=");
    Serial.println(encoders[i].totalDetents);
  }
}

void cmdPadTest() {
  Serial.println("PADTEST:etat de la matrice, ligne par ligne");
  for (uint8_t row = 0; row < 4; ++row) {
    Serial.print("PADTEST:ROW_");
    Serial.print(row);
    Serial.print(':');
    for (uint8_t col = 0; col < 4; ++col) {
      Serial.print(padState[az2::padId(row, col)] ? '1' : '0');
    }
    Serial.println();
  }
}

// Allume les 48 LED une par une, ~0,3 s chacune, en annoncant laquelle. A la
// difference du POV logiciel d'avant, le driver maintient la LED allumee tout
// seul : ce qu'on voit est stable et verifiable au multimetre.
bool ledTestActive = false;
uint8_t ledTestIndex = 0;
uint32_t ledTestLastMs = 0;
constexpr uint32_t kLedTestHoldMs = 300;

void cmdLedTest(bool start) {
  ledTestActive = start;
  ledTestIndex = 0;
  ledTestLastMs = 0;
  led::clear();
  Serial.println(start ? "LEDTEST:START -- 48 LED une par une"
                       : "LEDTEST:STOP");
}

void ledTestStep(uint32_t now) {
  if (now - ledTestLastMs < kLedTestHoldMs) {
    return;
  }
  ledTestLastMs = now;

  led::clear();
  const uint8_t color = static_cast<uint8_t>(ledTestIndex / 16);
  const uint8_t pad = static_cast<uint8_t>(ledTestIndex % 16);
  static const char *const kColorNames[3] = {"ROUGE", "VERT", "BLEU"};

  led::setPadColor(pad, static_cast<uint8_t>(1u << color), kLedBrightness);

  Serial.print("LEDTEST:pad=");
  if (pad < 10) {
    Serial.print('0');
  }
  Serial.print(pad);
  Serial.print(":couleur=");
  Serial.println(kColorNames[color]);

  ledTestIndex = static_cast<uint8_t>((ledTestIndex + 1) % 48);
}

void printIdentity() {
  Serial.println("AZ2:ROLE:PICO_KEYPAD");
  Serial.println("AZ2:FEATURE:SPARKFUN_4X4_MATRIX");
  Serial.println("AZ2:FEATURE:LED_IS31FL3731_I2C");
  Serial.println("AZ2:FEATURE:INPUT_MUX_CD74HC4067");
  Serial.print("AZ2:FEATURE:ENCODERS_");
  Serial.println(kEncoderCount);
  Serial.println("AZ2:FEATURE:NAV_FROM_ENCODERS");
  Serial.println("AZ2:FEATURE:GAMEPAD_ON_PADS");
}

void cmdSelfTest() {
  Serial.println("SELFTEST:debut");
  printIdentity();
  led::scanBus(Serial);
  cmdMuxTest();
  cmdEncTest();
  cmdPadTest();
  Serial.println("SELFTEST:fin -- LEDTEST pour verifier les LED une par une");
}

void readUsbCommands() {
  static String line;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c != '\n') {
      if (line.length() < 32) {
        line += c;
      }
      continue;
    }

    line.trim();
    if (line == "SELFTEST") {
      cmdSelfTest();
    } else if (line == "MUXTEST") {
      cmdMuxTest();
    } else if (line == "ENCTEST") {
      cmdEncTest();
    } else if (line == "PADTEST") {
      cmdPadTest();
    } else if (line == "LEDSCAN") {
      led::scanBus(Serial);
    } else if (line == "LEDTEST") {
      cmdLedTest(true);
    } else if (line == "LEDTEST:STOP") {
      cmdLedTest(false);
    } else if (line == "VERSION") {
      printIdentity();
    } else if (line.length() > 0) {
      Serial.println("?:commandes: SELFTEST MUXTEST ENCTEST PADTEST LEDSCAN LEDTEST VERSION");
    }
    line = "";
  }
}

// =====================================================================
// RECEPTION TEENSY
// =====================================================================

void handleTeensyLine(const String &line) {
  Serial.print("TEENSY:");
  Serial.println(line);

  // LED:NN:ON / LED:NN:OFF -- forme historique, toujours acceptee : ON allume
  // les trois couleurs (blanc).
  // LED:NN:C:<0-7> -- forme etendue, un bit par couleur (1 rouge, 2 vert,
  // 4 bleu). Retro-compatible : le Teensy actuel n'emet que la premiere.
  if (line.startsWith("LED:") && line.length() >= 7) {
    const uint8_t pad = static_cast<uint8_t>(line.substring(4, 6).toInt());
    if (!az2::validPad(pad)) {
      return;
    }
    const int colorMarker = line.indexOf(":C:");
    if (colorMarker > 0) {
      padColorMask[pad] =
          static_cast<uint8_t>(line.substring(colorMarker + 3).toInt() & 0b111);
    } else {
      padColorMask[pad] = line.endsWith("ON") ? 0b111 : 0b000;
    }
    return;
  }

  // PANEL:MODE:GAME / PANEL:MODE:MUSIC -- bascule le role des pads. Envoye
  // par l'ecran quand une partie demarre ou s'arrete.
  if (line.startsWith("PANEL:MODE:")) {
    const bool game = line.endsWith("GAME");
    if (game != gamepadMode) {
      gamepadMode = game;
      // Relacher tout ce qui etait enfonce : sinon une direction resterait
      // tenue cote ESP32 apres la bascule.
      for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
        padState[pad] = false;
        padLastRaw[pad] = false;
      }
    }
    Serial.print("PANEL:MODE:");
    Serial.println(gamepadMode ? "GAME" : "MUSIC");
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

// LED embarquee : test visuel "il tourne" sans avoir besoin d'un ordinateur.
void heartbeat(uint32_t now) {
  digitalWrite(kOnboardLedPin, (now / 250) % 2);

  if (now - lastHeartbeatMs < 1000) {
    return;
  }
  lastHeartbeatMs = now;
  az2::printStatus(Serial, "PICO_KEYPAD", az2::kStatusReady);
}

}  // namespace

void setup() {
  Serial.begin(az2::kControlBaud);
  Serial1.begin(az2::kControlBaud);  // UART0 par defaut : TX=GPIO0, RX=GPIO1
  delay(300);

  io::begin();
  led::begin();

  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    if (kEncoders[i].role == EncoderRole::Pot) {
      encoders[i].potValue = kEncoders[i].potInitial;
    }
  }

  printIdentity();
  led::scanBus(Serial);
  send(az2::kHelloKeypad);
}

void loop() {
  const uint32_t now = millis();

  readUsbCommands();

  // La quadrature est echantillonnee plusieurs fois par tour, encadrant les
  // traitements longs : le scan des pads (4 colonnes x 4 lignes, chacune avec
  // son temps d'etablissement de mux) prend ~450 us, celui des boutons ~130 us.
  // Sonder entre chaque bloc borne l'intervalle bien en dessous de la
  // milliseconde qui separe deux transitions, meme sur une rotation rapide.
  pollEncoders();
  scanPads();
  pollEncoders();
  scanButtons(now);
  pollEncoders();

  // L'emission serie est faite ICI, jamais au milieu d'un scan.
  drainEncoders(now);

  if (ledTestActive) {
    ledTestStep(now);
  } else {
    refreshLedFrame();
  }
  // Aucune contrainte temps reel : le balayage est fait par le driver, on ne
  // lui envoie une trame que quand l'image a change.
  led::flush(now);

  readTeensyStatus();
  heartbeat(now);
}
