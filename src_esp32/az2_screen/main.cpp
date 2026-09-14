// AZ-2 - Ecran + intro + menu de test NAVIGABLE. Carte reelle identifiee
// via l'etiquette sur le flex de l'ecran (2026-09-13): VIEWE
// UEDX48480040E-WB-V1.3, driver GC9503V (pas le generique
// "ESP32-4848S040C_I/ST7701" suppose au depart). Voir le depot officiel :
// https://github.com/VIEWESMART/UEDX48480040ESP32-4inch-Touch-Display
//
// Ce firmware:
// 1. Intro animee "pixel art" (nom de la machine, palette synthwave).
// 2. Menu de test avec de vraies pages (tap = on rentre dedans, "< " en
//    haut a gauche = retour) :
//    - PADS & LEDS : visualise en direct l'etat LED:NN:ON/OFF relaye par
//      le Teensy (donc pilote par les pads du Pico). Pas de tap ici, page
//      de VERIFICATION seulement.
//    - ENCODEURS : idem, affiche les MACRO:n:+/-N relayes par le Teensy.
//    - AUDIO : grille de 16 pads tactiles qui envoient PAD:NN:DOWN/UP
//      directement au Teensy depuis l'ecran -- pour faire jouer le Teensy
//      sans attendre que le Pico soit cable.
//    - LIENS SERIE : dernieres lignes recues du Teensy (journal).
//    - A PROPOS : infos statiques (roles, liens).
//
// 3. Lien UART vers le Teensy (GPIO19/20, les 2 seules broches vraiment
//    libres sur cette carte -- voir AZ2_CABLAGE_MASTER.md section 2).
//
// 4. Tactile : FT6336U (compatible FT5x06), PAS un GT911 -- verifie sur le
//    depot officiel VIEWE. Registre standard (I2C 0x38, TD_STATUS a 0x02).
//    SDA=40, SCL=41. Rotation ecran = 180 donc (x,y) brut -> (479-x,479-y).

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <AZ2_Protocol.h>
#include <Wire.h>
#include <math.h>

namespace {

constexpr int kPinBacklight = 38;
constexpr int16_t kScreenSize = 480;

constexpr int kTeensyTxPin = 19;
constexpr int kTeensyRxPin = 20;

constexpr int kTouchSdaPin = 40;
constexpr int kTouchSclPin = 41;
constexpr uint8_t kTouchI2cAddr = 0x38;

Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC (inutilise, 3-wire) */, 39 /* CS */, 48 /* SCK */,
    47 /* SDA */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbPanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 12000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 0 /* bounce_buffer_size_px */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    kScreenSize, kScreenSize, rgbPanel, 2 /* rotation: ecran tete-en-bas */,
    true, bus, GFX_NOT_DEFINED, gc9503v_type1_init_operations, sizeof(gc9503v_type1_init_operations));

const uint16_t kPalette[] = {
    RGB565(255, 60, 172), RGB565(70, 220, 255), RGB565(255, 170, 50),
    RGB565(170, 90, 255), RGB565(255, 225, 80),
};
constexpr uint8_t kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);
constexpr uint16_t kDim = RGB565(140, 140, 150);

// Assombrit une couleur RGB565 (garde la teinte, baisse la luminosite) --
// sert aux bandes verticales de mesure du sequenceur, voir seqBandColor().
uint16_t dimColor(uint16_t c, uint8_t shift) {
  const uint16_t r = static_cast<uint16_t>(((c >> 11) & 0x1F) >> shift);
  const uint16_t g = static_cast<uint16_t>(((c >> 5) & 0x3F) >> shift);
  const uint16_t b = static_cast<uint16_t>((c & 0x1F) >> shift);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
constexpr uint16_t kFaint = RGB565(90, 90, 110);

// ---------------------------------------------------------------------
// Tactile FT6336U
// ---------------------------------------------------------------------
// Diagnostic limite a 1 message/seconde pour ne pas noyer le port serie.
void reportTouchI2cError(const char *what) {
  static uint32_t lastReportMs = 0;
  const uint32_t now = millis();
  if (now - lastReportMs < 1000) {
    return;
  }
  lastReportMs = now;
  Serial.print("TOUCH:I2C_ERROR:");
  Serial.println(what);
}

struct TouchPoint {
  bool active = false;
  int16_t x = 0;
  int16_t y = 0;
};

// Lit jusqu'a 2 points de contact simultanes (registres FT5x06 standard,
// famille FT6336U incluse) : 0x02 = nombre de points, point 1 a partir de
// 0x03, point 2 a partir de 0x09 (meme mise en page, 6 octets d'ecart).
// Une seule transaction I2C pour les deux, plus rapide qu'un point a la
// fois. Coordonnees deja converties dans le repere ecran (rotation 180).
uint8_t readTouches(TouchPoint points[2]) {
  points[0].active = false;
  points[1].active = false;

  Wire.beginTransmission(kTouchI2cAddr);
  Wire.write(0x02);
  const uint8_t txResult = Wire.endTransmission(false);
  if (txResult != 0) {
    reportTouchI2cError(txResult == 2 ? "NAK_ADDR" : txResult == 3 ? "NAK_DATA" : "OTHER");
    return 0;
  }

  constexpr uint8_t kReadLen = 11;  // registres 0x02 a 0x0C inclus
  if (Wire.requestFrom(kTouchI2cAddr, kReadLen) != kReadLen) {
    reportTouchI2cError("SHORT_READ");
    return 0;
  }

  uint8_t buf[kReadLen];
  for (uint8_t i = 0; i < kReadLen; ++i) {
    buf[i] = Wire.read();
  }

  const uint8_t touchCount = buf[0] & 0x0F;

  // Le FT6336U ne rapporte jamais plus de 2 points (registre TD_STATUS,
  // 0-2 valides) -- un bus I2C bruite peut renvoyer des octets a 0xFF
  // (flottant/erreur silencieuse, pas forcement signalee par endTransmission
  // ni requestFrom) qui, une fois masques par & 0x0F, ressemblent a un
  // touchCount valide (ex: 0xFF -> 15). Observe reellement le 2026-09-14 :
  // rafale de faux "TOUCH:DOWN" avec x/y a -3616 (= (kScreenSize-1) - 4095,
  // 4095 = 0x0FFF = les 2 registres de coordonnee a 0xFF). On rejette donc
  // tout touchCount hors 0-2 ET toute coordonnee hors ecran.
  if (touchCount > 2) {
    reportTouchI2cError("BAD_COUNT");
    return 0;
  }

  if (touchCount >= 1) {
    const int16_t rawX = static_cast<int16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
    const int16_t rawY = static_cast<int16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
    const int16_t x = static_cast<int16_t>((kScreenSize - 1) - rawX);
    const int16_t y = static_cast<int16_t>((kScreenSize - 1) - rawY);
    if (x >= 0 && x < kScreenSize && y >= 0 && y < kScreenSize) {
      points[0].active = true;
      points[0].x = x;
      points[0].y = y;
    } else {
      reportTouchI2cError("BAD_COORD");
    }
  }

  if (touchCount >= 2) {
    const int16_t rawX = static_cast<int16_t>(((buf[7] & 0x0F) << 8) | buf[8]);
    const int16_t rawY = static_cast<int16_t>(((buf[9] & 0x0F) << 8) | buf[10]);
    const int16_t x = static_cast<int16_t>((kScreenSize - 1) - rawX);
    const int16_t y = static_cast<int16_t>((kScreenSize - 1) - rawY);
    if (x >= 0 && x < kScreenSize && y >= 0 && y < kScreenSize) {
      points[1].active = true;
      points[1].x = x;
      points[1].y = y;
    } else {
      reportTouchI2cError("BAD_COORD");
    }
  }

  return touchCount;
}

bool inBox(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

// ---------------------------------------------------------------------
// Etat partage entre les pages / le lien Teensy
// ---------------------------------------------------------------------
enum class Screen : uint8_t { Menu, PadsLeds, Encoders, Audio, Sequencer, Engines, Retro, Links, About };
Screen currentScreen = Screen::Menu;

bool ledState[az2::kPadCount] = {};
int32_t macroValue[4] = {};
bool teensyLinked = false;

constexpr uint8_t kLogLines = 8;
String logBuf[kLogLines];
uint8_t logCount = 0;

void pushLog(const String &line) {
  for (uint8_t i = kLogLines - 1; i > 0; --i) {
    logBuf[i] = logBuf[i - 1];
  }
  logBuf[0] = line;
  if (logCount < kLogLines) {
    ++logCount;
  }
}

void sendToTeensy(const String &message) {
  Serial.println(message);
  Serial1.println(message);
}

// ---------------------------------------------------------------------
// Chrome commun : en-tete avec fleche retour + titre, bas d'ecran = etat
// du lien Teensy.
// ---------------------------------------------------------------------
constexpr int16_t kMargin = 24;
constexpr int16_t kStatusY = kScreenSize - 30;

void drawLinkStatus() {
  gfx->fillRect(kMargin, kStatusY, kScreenSize - 2 * kMargin, 22, RGB565_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(teensyLinked ? RGB565(90, 220, 120) : kDim);
  gfx->setCursor(kMargin, kStatusY + 4);
  gfx->print(teensyLinked ? "TEENSY: relie" : "TEENSY: en attente...");
}

void drawSubHeader(const char *title, uint16_t accent) {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(accent);
  gfx->setTextSize(2);
  gfx->setCursor(kMargin, 28);
  gfx->print("< ");
  gfx->print(title);
  gfx->drawFastHLine(kMargin, 60, kScreenSize - 2 * kMargin, kFaint);
  drawLinkStatus();
}

bool hitBack(int16_t x, int16_t y) {
  return inBox(x, y, 0, 0, 90, 50);
}

// ---------------------------------------------------------------------
// Page Menu
// ---------------------------------------------------------------------
struct MenuItem {
  const char *label;
  const char *hint;
  Screen target;
};

constexpr MenuItem kMenuItems[] = {
    {"SEQUENCEUR", "programmer les 16 pas", Screen::Sequencer},
    {"MOTEURS", "moteur + patch par piste", Screen::Engines},
    {"PADS & LEDS", "verifier la matrice + Pico", Screen::PadsLeds},
    {"ENCODEURS", "verifier les 4 rotatifs", Screen::Encoders},
    {"AUDIO", "jouer le Teensy depuis l'ecran", Screen::Audio},
    {"JEUX", "NES (en construction, voir doc)", Screen::Retro},
    {"LIENS SERIE", "journal ESP32 / Teensy", Screen::Links},
    {"A PROPOS", "version, roles, build", Screen::About},
};
constexpr uint8_t kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

constexpr int16_t kMenuLeft = 48;
constexpr int16_t kMenuTop = 130;
// 39 (au lieu de 42) depuis l'ajout de JEUX -- 8 entrees doivent tenir
// avant la barre d'etat en bas d'ecran (kStatusY).
constexpr int16_t kMenuRowH = 39;
constexpr int16_t kMenuWidth = kScreenSize - 2 * kMenuLeft;

void drawMenuRow(uint8_t index) {
  const int16_t y = kMenuTop + index * kMenuRowH;
  const uint16_t accent = kPalette[index % kPaletteCount];
  gfx->drawRect(kMenuLeft, y, kMenuWidth, kMenuRowH - 8, accent);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(kMenuLeft + 14, y + 3);
  gfx->print(kMenuItems[index].label);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMenuLeft + 14, y + 22);
  gfx->print(kMenuItems[index].hint);
}

void drawMenu() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(kMenuLeft, 48);
  gfx->print("AZ-2");
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMenuLeft + 90, 60);
  gfx->print("MENU DE TEST");
  gfx->drawFastHLine(kMenuLeft, 96, kMenuWidth, kFaint);
  for (uint8_t i = 0; i < kMenuItemCount; ++i) {
    drawMenuRow(i);
  }
  drawLinkStatus();
}

int8_t hitTestMenuRow(int16_t x, int16_t y) {
  if (x < kMenuLeft || x > kMenuLeft + kMenuWidth) {
    return -1;
  }
  for (uint8_t i = 0; i < kMenuItemCount; ++i) {
    const int16_t rowTop = kMenuTop + i * kMenuRowH;
    if (y >= rowTop && y < rowTop + (kMenuRowH - 8)) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------
// Page PADS & LEDS -- visualisation seule (pilotee par le Pico via relais
// Teensy : LED:NN:ON/OFF)
// ---------------------------------------------------------------------
constexpr int16_t kGridLeft = 60;
constexpr int16_t kGridTop = 90;
constexpr int16_t kGridCell = 80;
constexpr int16_t kGridGap = 10;

void padCellRect(uint8_t pad, int16_t &x, int16_t &y) {
  const uint8_t row = pad / 4;
  const uint8_t col = pad % 4;
  x = static_cast<int16_t>(kGridLeft + col * (kGridCell + kGridGap));
  y = static_cast<int16_t>(kGridTop + row * (kGridCell + kGridGap));
}

void drawPadCell(uint8_t pad, bool lit) {
  int16_t x, y;
  padCellRect(pad, x, y);
  gfx->fillRect(x, y, kGridCell, kGridCell, lit ? kPalette[pad % kPaletteCount] : RGB565_BLACK);
  gfx->drawRect(x, y, kGridCell, kGridCell, kFaint);
  gfx->setTextSize(1);
  gfx->setTextColor(lit ? RGB565_BLACK : kDim);
  gfx->setCursor(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + kGridCell - 16));
  if (pad < 10) gfx->print('0');
  gfx->print(pad);
}

void drawPadsLedsPage() {
  drawSubHeader("PADS & LEDS", kPalette[0]);
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    drawPadCell(pad, ledState[pad]);
  }
}

// ---------------------------------------------------------------------
// Page ENCODEURS -- visualisation seule (MACRO:n:+/-N relaye par le Teensy)
// Jauge horizontale centree sur zero: verte/couleur palette vers la droite
// (valeur positive), rouge vers la gauche (negative) -- longueur = valeur,
// couleur = sens, comme demande.
// ---------------------------------------------------------------------
constexpr int16_t kEncRowH = 64;
constexpr int16_t kEncRowPitch = 76;
constexpr int32_t kGaugeRange = 24;  // plage affichee +/-24 (transpose max)

void drawEncoderGauge(uint8_t index, int16_t rowY) {
  const int16_t barX = static_cast<int16_t>(kMargin + 16);
  const int16_t barW = static_cast<int16_t>(kScreenSize - 2 * kMargin - 32);
  const int16_t barY = static_cast<int16_t>(rowY + 44);
  constexpr int16_t barH = 14;
  const int16_t midX = static_cast<int16_t>(barX + barW / 2);
  const int16_t halfW = static_cast<int16_t>(barW / 2 - 2);

  gfx->fillRect(barX, barY, barW, barH, RGB565_BLACK);
  gfx->drawRect(barX, barY, barW, barH, kFaint);
  gfx->drawFastVLine(midX, barY, barH, kFaint);

  const int32_t clamped = constrain(macroValue[index], -kGaugeRange, kGaugeRange);
  if (clamped == 0) {
    return;
  }
  const int16_t fillW = static_cast<int16_t>((labs(clamped) * halfW) / kGaugeRange);
  const uint16_t fillColor = clamped > 0 ? kPalette[index % kPaletteCount] : RGB565(240, 80, 80);
  const int16_t fillX = clamped > 0 ? static_cast<int16_t>(midX + 1)
                                     : static_cast<int16_t>(midX - fillW);
  gfx->fillRect(fillX, static_cast<int16_t>(barY + 1), fillW, static_cast<int16_t>(barH - 2), fillColor);
}

void drawEncoderRow(uint8_t index) {
  const int16_t y = static_cast<int16_t>(100 + index * kEncRowPitch);
  const bool active = macroValue[index] != 0;
  const uint16_t accent = kPalette[index % kPaletteCount];

  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kEncRowH, RGB565_BLACK);
  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, kEncRowH, active ? accent : kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(active ? accent : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 16), static_cast<int16_t>(y + 6));
  gfx->print("ENC ");
  gfx->print(index + 1);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kMargin + 16), static_cast<int16_t>(y + 28));
  if (index == 0) {
    gfx->print("transpose (Teensy) : ");
  } else {
    gfx->print("valeur : ");
  }
  gfx->print(macroValue[index]);

  drawEncoderGauge(index, y);
}

void drawEncodersPage() {
  drawSubHeader("ENCODEURS", kPalette[1]);
  for (uint8_t i = 0; i < 4; ++i) {
    drawEncoderRow(i);
  }
}

// ---------------------------------------------------------------------
// Page AUDIO -- grille tactile, envoie PAD:NN:DOWN/UP au Teensy
// 2 doigts geres en parallele (un pad tenu par slot tactile) -> accords.
// ---------------------------------------------------------------------
int8_t heldAudioPad[2] = {-1, -1};

void drawAudioCell(uint8_t pad, bool pressed) {
  int16_t x, y;
  padCellRect(pad, x, y);
  gfx->fillRect(x, y, kGridCell, kGridCell, pressed ? kPalette[pad % kPaletteCount] : RGB565_BLACK);
  gfx->drawRect(x, y, kGridCell, kGridCell, kPalette[pad % kPaletteCount]);
  gfx->setTextSize(1);
  gfx->setTextColor(pressed ? RGB565_BLACK : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + kGridCell - 16));
  if (pad < 10) gfx->print('0');
  gfx->print(pad);
}

void drawAudioPage() {
  drawSubHeader("AUDIO - touche pour jouer (2 doigts OK)", kPalette[2]);
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    drawAudioCell(pad, false);
  }
  heldAudioPad[0] = -1;
  heldAudioPad[1] = -1;
}

int8_t hitTestAudioPad(int16_t x, int16_t y) {
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    int16_t px, py;
    padCellRect(pad, px, py);
    if (inBox(x, y, px, py, kGridCell, kGridCell)) {
      return static_cast<int8_t>(pad);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------
// Page SEQUENCEUR -- grille tactile 4 pistes x 16 pas, envoie STEP: au
// Teensy ; curseur de lecture synchronise sur les CLOCK: recus ; bouton
// PLAY/STOP qui reflete l'etat reel (STATUS:TEENSY_AUDIO:).
// 4 pistes / 16 pas : doit rester aligne avec kTrackCount/kStepCount cote
// Teensy (src_teensy/az2_audio/main.cpp).
// ---------------------------------------------------------------------
constexpr uint8_t kSeqTrackCount = 4;
constexpr uint8_t kSeqStepCount = 16;
constexpr int16_t kSeqGridLeft = 17;
constexpr int16_t kSeqGridTop = 90;
constexpr int16_t kSeqCellW = 26;
constexpr int16_t kSeqGapX = 2;
constexpr int16_t kSeqCellH = 48;
constexpr int16_t kSeqGapY = 8;
constexpr int16_t kSeqTransportW = 200;
constexpr int16_t kSeqTransportH = 50;
constexpr int16_t kSeqTransportX = (kScreenSize - kSeqTransportW) / 2;
constexpr int16_t kSeqTransportY = kSeqGridTop + kSeqTrackCount * (kSeqCellH + kSeqGapY) + 20;

// Tempo + division ("vrai sequenceur" demande le 2026-09-14 : "on est pas
// assez precis ... il faut les divisions le tempo") -- sous le transport,
// avant la barre d'etat (kStatusY). Pas d'etat local optimiste ici (au
// contraire de seqStepOn) : le Teensy relaie sa valeur confirmee (BPM:/
// DIV:) en quelques ms, l'ecran se contente d'afficher ce qui revient.
constexpr int16_t kSeqRightEdge = kSeqGridLeft + kSeqStepCount * (kSeqCellW + kSeqGapX) - kSeqGapX;  // 463
constexpr int16_t kSeqTempoY = kSeqTransportY + kSeqTransportH + 12;
constexpr int16_t kSeqTempoH = 26;
constexpr int16_t kSeqTempoBtnW = 50;
constexpr int16_t kSeqDivY = kSeqTempoY + kSeqTempoH + 4;
constexpr int16_t kSeqDivH = 20;

bool seqStepOn[kSeqTrackCount][kSeqStepCount] = {};
uint8_t seqCurrentStep = 0;
bool seqPlaying = false;
float seqBpm = 120.0f;
uint8_t seqStepsPerBeat = 4;

void seqCellRect(uint8_t track, uint8_t step, int16_t &x, int16_t &y) {
  x = static_cast<int16_t>(kSeqGridLeft + step * (kSeqCellW + kSeqGapX));
  y = static_cast<int16_t>(kSeqGridTop + track * (kSeqCellH + kSeqGapY));
}

// Bande de mesure : chaque groupe de seqStepsPerBeat pas (aligne sur la
// division reglable, voir DIV: / seqStepsPerBeat) prend une teinte
// assombrie differente, cyclique sur la palette -- repere visuel demande
// le 2026-09-14 ("une bande verticale a chaque mesure de couleurs").
uint16_t seqBandColor(uint8_t step) {
  const uint8_t group = static_cast<uint8_t>(step / seqStepsPerBeat);
  return dimColor(kPalette[group % kPaletteCount], 3);
}

void drawSeqCell(uint8_t track, uint8_t step) {
  int16_t x, y;
  seqCellRect(track, step, x, y);
  const bool on = seqStepOn[track][step];
  const bool playhead = (step == seqCurrentStep);
  gfx->fillRect(x, y, kSeqCellW, kSeqCellH, on ? kPalette[track % kPaletteCount] : seqBandColor(step));
  gfx->drawRect(x, y, kSeqCellW, kSeqCellH, playhead ? RGB565_WHITE : kFaint);
}

// Remplit chaque groupe de mesure sur toute la hauteur de la grille (y
// compris les interstices entre cellules/lignes) -- appele avant
// drawSeqGrid() pour que les bandes soient continues, sans coutures ;
// drawSeqCell() reutilise la meme couleur (seqBandColor()) pour les
// cellules eteintes, donc un redessin ponctuel (echo STEP:, playhead)
// reste coherent sans refaire toute la bande.
void drawSeqBeatBands() {
  const int16_t bandW = static_cast<int16_t>(seqStepsPerBeat * (kSeqCellW + kSeqGapX));
  const int16_t bandH = static_cast<int16_t>(kSeqTrackCount * (kSeqCellH + kSeqGapY));
  const uint8_t groupCount = static_cast<uint8_t>((kSeqStepCount + seqStepsPerBeat - 1) / seqStepsPerBeat);
  for (uint8_t g = 0; g < groupCount; ++g) {
    const int16_t x = static_cast<int16_t>(kSeqGridLeft + g * bandW);
    gfx->fillRect(x, kSeqGridTop, bandW, bandH, dimColor(kPalette[g % kPaletteCount], 3));
  }
}

void drawSeqGrid() {
  drawSeqBeatBands();
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    for (uint8_t s = 0; s < kSeqStepCount; ++s) {
      drawSeqCell(t, s);
    }
  }
}

void drawSeqTransport() {
  gfx->fillRect(kSeqTransportX, kSeqTransportY, kSeqTransportW, kSeqTransportH,
                seqPlaying ? RGB565(80, 220, 120) : RGB565_BLACK);
  gfx->drawRect(kSeqTransportX, kSeqTransportY, kSeqTransportW, kSeqTransportH, RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(seqPlaying ? RGB565_BLACK : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kSeqTransportX + 55), static_cast<int16_t>(kSeqTransportY + 16));
  gfx->print(seqPlaying ? "STOP" : "PLAY");
}

void drawSeqTempo() {
  const int16_t plusX = static_cast<int16_t>(kSeqRightEdge - kSeqTempoBtnW);
  gfx->fillRect(kSeqGridLeft, kSeqTempoY, kSeqRightEdge - kSeqGridLeft, kSeqTempoH, RGB565_BLACK);
  gfx->drawRect(kSeqGridLeft, kSeqTempoY, kSeqTempoBtnW, kSeqTempoH, kFaint);
  gfx->drawRect(plusX, kSeqTempoY, kSeqTempoBtnW, kSeqTempoH, kFaint);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kSeqGridLeft + 16), static_cast<int16_t>(kSeqTempoY + 4));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 16), static_cast<int16_t>(kSeqTempoY + 4));
  gfx->print('+');
  char buf[16];
  snprintf(buf, sizeof(buf), "%d BPM", static_cast<int>(seqBpm + 0.5f));
  gfx->setCursor(static_cast<int16_t>(kSeqGridLeft + kSeqTempoBtnW + 70), static_cast<int16_t>(kSeqTempoY + 4));
  gfx->print(buf);
}

void drawSeqDivision() {
  gfx->fillRect(kSeqGridLeft, kSeqDivY, kSeqRightEdge - kSeqGridLeft, kSeqDivH, RGB565_BLACK);
  gfx->drawRect(kSeqGridLeft, kSeqDivY, kSeqRightEdge - kSeqGridLeft, kSeqDivH, kFaint);
  gfx->setTextSize(1);
  gfx->setTextColor(kPalette[1 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(kSeqGridLeft + 8), static_cast<int16_t>(kSeqDivY + 6));
  gfx->print("PAS: ");
  gfx->print(az2::divisionLabel(seqStepsPerBeat));
  gfx->print("  (toucher pour changer)");
}

void drawSequencerPage() {
  drawSubHeader("SEQUENCEUR", kPalette[0]);
  drawSeqGrid();
  drawSeqTransport();
  drawSeqTempo();
  drawSeqDivision();
}

bool hitTestSeqTransport(int16_t x, int16_t y) {
  return inBox(x, y, kSeqTransportX, kSeqTransportY, kSeqTransportW, kSeqTransportH);
}

// Renvoie -1 (aucun), 0 (tempo -), 1 (tempo +).
int8_t hitTestSeqTempo(int16_t x, int16_t y) {
  if (y < kSeqTempoY || y >= kSeqTempoY + kSeqTempoH) {
    return -1;
  }
  if (inBox(x, y, kSeqGridLeft, kSeqTempoY, kSeqTempoBtnW, kSeqTempoH)) {
    return 0;
  }
  const int16_t plusX = static_cast<int16_t>(kSeqRightEdge - kSeqTempoBtnW);
  if (inBox(x, y, plusX, kSeqTempoY, kSeqTempoBtnW, kSeqTempoH)) {
    return 1;
  }
  return -1;
}

bool hitTestSeqDivision(int16_t x, int16_t y) {
  return inBox(x, y, kSeqGridLeft, kSeqDivY, kSeqRightEdge - kSeqGridLeft, kSeqDivH);
}

bool hitTestSeqCell(int16_t x, int16_t y, uint8_t &track, uint8_t &step) {
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    for (uint8_t s = 0; s < kSeqStepCount; ++s) {
      int16_t cx, cy;
      seqCellRect(t, s, cx, cy);
      if (inBox(x, y, cx, cy, kSeqCellW, kSeqCellH)) {
        track = t;
        step = s;
        return true;
      }
    }
  }
  return false;
}

// ---------------------------------------------------------------------
// Page MOTEURS -- moteur + patch par piste, demandee le 2026-09-14 ("les
// moteurs audio ne sont pas selectionnables ni reglables ... faut faire
// un truc propre"). Une ligne par piste : toucher la moitie gauche
// change de moteur (ENGINE:), la moitie droite change de patch (PATCH:)
// -- le Teensy applique et renvoie confirmation, voir handleTeensyLine().
// ---------------------------------------------------------------------
uint8_t trackEngine[kSeqTrackCount] = {az2::kEngineDexed, az2::kEngineDexed, az2::kEngineEPiano, az2::kEngineBraids};
uint8_t trackPatch[kSeqTrackCount] = {0, 0, 0, 0};

constexpr int16_t kEngRowTop = 90;
constexpr int16_t kEngRowH = 70;
constexpr int16_t kEngRowGap = 10;
constexpr int16_t kEngLeft = kMargin;
constexpr int16_t kEngWidth = kScreenSize - 2 * kMargin;
constexpr int16_t kEngSplitX = kEngLeft + kEngWidth / 2;

void engRowRect(uint8_t track, int16_t &y) {
  y = static_cast<int16_t>(kEngRowTop + track * (kEngRowH + kEngRowGap));
}

void drawEngRow(uint8_t track) {
  int16_t y;
  engRowRect(track, y);
  const uint16_t accent = kPalette[track % kPaletteCount];

  gfx->fillRect(kEngLeft, y, kEngWidth, kEngRowH - kEngRowGap, RGB565_BLACK);
  gfx->drawRect(kEngLeft, y, kEngWidth, kEngRowH - kEngRowGap, accent);
  gfx->drawFastVLine(kEngSplitX, y, kEngRowH - kEngRowGap, kFaint);

  char title[12];
  snprintf(title, sizeof(title), "PISTE %d", track);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kEngLeft + 8), static_cast<int16_t>(y + 6));
  gfx->print(title);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kEngLeft + 8), static_cast<int16_t>(y + 22));
  gfx->print(az2::engineName(trackEngine[track]));

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kEngSplitX + 8), static_cast<int16_t>(y + 6));
  gfx->print("PATCH");
  gfx->setTextSize(2);
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(kEngSplitX + 8), static_cast<int16_t>(y + 22));
  gfx->print(az2::enginePatchName(trackEngine[track], trackPatch[track]));
}

void drawEnginesPage() {
  drawSubHeader("MOTEURS", kPalette[2]);
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    drawEngRow(t);
  }
}

// Renvoie -1 (aucun), sinon la piste touchee ; isPatchSide indique quelle
// moitie de la ligne (moteur = gauche, patch = droite).
int8_t hitTestEngRow(int16_t x, int16_t y, bool &isPatchSide) {
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    int16_t rowY;
    engRowRect(t, rowY);
    if (inBox(x, y, kEngLeft, rowY, kEngWidth, kEngRowH - kEngRowGap)) {
      isPatchSide = (x >= kEngSplitX);
      return static_cast<int8_t>(t);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------
// Page LIENS SERIE -- journal des dernieres lignes Teensy
// ---------------------------------------------------------------------
void drawLinksPage() {
  drawSubHeader("LIENS SERIE", kPalette[3]);
  gfx->setTextSize(1);
  for (uint8_t i = 0; i < logCount; ++i) {
    gfx->setTextColor(i == 0 ? RGB565_WHITE : kDim);
    gfx->setCursor(kMargin, static_cast<int16_t>(90 + i * 20));
    gfx->print(logBuf[i]);
  }
}

// ---------------------------------------------------------------------
// Page JEUX -- mode emulation NES, demande le 2026-09-14. PAS ENCORE
// FONCTIONNELLE : simple page d'attente honnete tant que le portage n'est
// pas fait (voir docs/AZ2_EMULATION_JEUX.md pour la decision technique
// complete -- Retro-Go ecarte car ESP-IDF + ecrans SPI seulement,
// incompatible avec notre ecran RGB parallele sans double demarrage ;
// Anemoia-ESP32 retenu a la place, portable en simple page ici via son
// acces framebuffer brut, mais pas encore vendore/adapte -- il manque
// aussi une ROM legale et une carte SD pour la charger).
// ---------------------------------------------------------------------
void drawRetroPage() {
  drawSubHeader("JEUX", kPalette[2]);
  const char *lines[] = {
      "Emulateur Game Boy / GBC en preparation.",
      "",
      "Moteur retenu : Walnut-CGB",
      "(callbacks purs, licence MIT, deja",
      " demontre sur ESP32-S3 -- pas de",
      " double demarrage requis).",
      "",
      "GBA ecarte du v0 : ~20fps mesures",
      "sur ESP32-S3, pas fluide.",
      "",
      "Reste a faire : vendorer le coeur,",
      "brancher lcd_draw_line() sur",
      "Arduino_GFX, mapper les entrees,",
      "trouver une ROM legale.",
      "",
      "Detail : docs/AZ2_EMULATION_JEUX.md",
  };
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  for (uint8_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
    gfx->setCursor(kMargin, static_cast<int16_t>(90 + i * 20));
    gfx->print(lines[i]);
  }
}

// ---------------------------------------------------------------------
// Page A PROPOS
// ---------------------------------------------------------------------
void drawAboutPage() {
  drawSubHeader("A PROPOS", kPalette[4]);
  const char *lines[] = {
      "AZ-2 groovebox",
      "Ecran: VIEWE UEDX48480040E-WB (GC9503V)",
      "Tactile: FT6336U",
      "Audio: Teensy 4.1 + Synth_Dexed + PCM5102A",
      "Clavier: Pico (matrice 4x4 + mux LED RGB + 4 enc.)",
      "Build: screen_esp, 2026-09-13",
  };
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  for (uint8_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
    gfx->setCursor(kMargin, static_cast<int16_t>(90 + i * 22));
    gfx->print(lines[i]);
  }
}

void drawScreen(Screen s) {
  switch (s) {
    case Screen::Menu: drawMenu(); return;
    case Screen::PadsLeds: drawPadsLedsPage(); return;
    case Screen::Encoders: drawEncodersPage(); return;
    case Screen::Audio: drawAudioPage(); return;
    case Screen::Sequencer: drawSequencerPage(); return;
    case Screen::Engines: drawEnginesPage(); return;
    case Screen::Retro: drawRetroPage(); return;
    case Screen::Links: drawLinksPage(); return;
    case Screen::About: drawAboutPage(); return;
  }
}

void goTo(Screen s) {
  currentScreen = s;
  drawScreen(s);
}

// ---------------------------------------------------------------------
// Reception Teensy : met a jour l'etat partage + le journal + la page
// courante si elle affiche la donnee concernee.
// ---------------------------------------------------------------------
String teensyLine;

void handleTeensyLine(const String &line) {
  Serial.print("TEENSY:");
  Serial.println(line);
  teensyLinked = true;
  pushLog(line);

  if (line.startsWith("LED:") && line.length() >= 9) {
    const uint8_t pad = static_cast<uint8_t>(line.substring(4, 6).toInt());
    const bool on = line.endsWith("ON");
    if (az2::validPad(pad)) {
      ledState[pad] = on;
      if (currentScreen == Screen::PadsLeds) {
        drawPadCell(pad, on);
      }
    }
  } else if (line.startsWith("MACRO:")) {
    const int firstColon = line.indexOf(':');
    const int secondColon = line.indexOf(':', firstColon + 1);
    if (firstColon >= 0 && secondColon >= 0) {
      const uint8_t index = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
      const int32_t delta = line.substring(secondColon + 1).toInt();
      if (index >= 1 && index <= 4) {
        macroValue[index - 1] += delta;
        if (currentScreen == Screen::Encoders) {
          drawEncoderRow(index - 1);
        }
      }
    }
  } else if (line.startsWith("STEP:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const bool on = line.substring(i3 + 1).toInt() != 0;
      if (track < kSeqTrackCount && step < kSeqStepCount) {
        seqStepOn[track][step] = on;
        if (currentScreen == Screen::Sequencer) {
          drawSeqCell(track, step);
        }
      }
    }
  } else if (line.startsWith("CLOCK:")) {
    const int idx = line.indexOf("step=");
    if (idx >= 0) {
      const uint8_t newStep = static_cast<uint8_t>(line.substring(idx + 5).toInt());
      if (newStep < kSeqStepCount && newStep != seqCurrentStep) {
        const uint8_t oldStep = seqCurrentStep;
        seqCurrentStep = newStep;
        if (currentScreen == Screen::Sequencer) {
          for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
            drawSeqCell(t, oldStep);
            drawSeqCell(t, seqCurrentStep);
          }
        }
      }
    }
  } else if (line.startsWith("STATUS:TEENSY_AUDIO:")) {
    const bool nowPlaying = line.endsWith("PLAYING");
    if (nowPlaying != seqPlaying) {
      seqPlaying = nowPlaying;
      if (currentScreen == Screen::Sequencer) {
        drawSeqTransport();
      }
    }
  } else if (line.startsWith("BPM:")) {
    const float value = line.substring(4).toFloat();
    if (value > 0.0f) {
      seqBpm = value;
      if (currentScreen == Screen::Sequencer) {
        drawSeqTempo();
      }
    }
  } else if (line.startsWith("DIV:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(4).toInt());
    if (value > 0) {
      seqStepsPerBeat = value;
      if (currentScreen == Screen::Sequencer) {
        drawSeqDivision();
        drawSeqGrid();  // les bandes de mesure suivent le regroupement (voir drawSeqBeatBands())
      }
    }
  } else if (line.startsWith("ENGINE:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t engine = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (track < kSeqTrackCount && engine < az2::kEngineCount) {
        trackEngine[track] = engine;
        if (currentScreen == Screen::Engines) {
          drawEngRow(track);
        }
      }
    }
  } else if (line.startsWith("PATCH:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t patch = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (track < kSeqTrackCount) {
        trackPatch[track] = patch;
        if (currentScreen == Screen::Engines) {
          drawEngRow(track);
        }
      }
    }
  }

  if (currentScreen == Screen::Links) {
    drawLinksPage();
  }
  if (currentScreen != Screen::PadsLeds && currentScreen != Screen::Encoders &&
      currentScreen != Screen::Links) {
    drawLinkStatus();
  }
}

void readTeensyStatus() {
  while (Serial1.available() > 0) {
    const char c = static_cast<char>(Serial1.read());
    if (c == '\r') continue;
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

// ---------------------------------------------------------------------
// Intro
// ---------------------------------------------------------------------
struct Star {
  int16_t x, y;
  uint8_t phase;
};
Star stars[36];

void seedStars() {
  randomSeed(micros());
  for (Star &s : stars) {
    s.x = static_cast<int16_t>(random(kScreenSize));
    s.y = static_cast<int16_t>(random(kScreenSize));
    s.phase = static_cast<uint8_t>(random(0, 255));
  }
}

void drawStars(uint32_t nowMs) {
  for (const Star &s : stars) {
    const bool bright = ((nowMs / 300) + s.phase) % 2 == 0;
    gfx->drawPixel(s.x, s.y, bright ? RGB565(90, 90, 110) : RGB565(35, 35, 50));
  }
}

void runIntro() {
  gfx->fillScreen(RGB565_BLACK);
  seedStars();
  drawStars(0);

  static const char kName[] = "AZ-2";
  constexpr int16_t kCharW = 36;
  constexpr int16_t kStartX = (kScreenSize - kCharW * 4) / 2;
  constexpr int16_t kNameY = 170;

  gfx->setTextSize(8);
  for (uint8_t i = 0; kName[i] != '\0'; ++i) {
    drawStars(millis());
    gfx->setTextColor(kPalette[i % kPaletteCount]);
    gfx->setCursor(kStartX + i * kCharW, kNameY);
    gfx->print(kName[i]);
    delay(220);
  }

  delay(200);
  gfx->drawFastHLine(kStartX, kNameY + 76, kCharW * 4, RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565(150, 150, 170));
  gfx->setCursor(kStartX + 6, kNameY + 96);
  gfx->print("GROOVEBOX");
  delay(900);
}

}  // namespace

void setup() {
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:ROLE:ESP32_SCREEN_TEST");

  pinMode(kPinBacklight, OUTPUT);
  digitalWrite(kPinBacklight, HIGH);

  if (!gfx->begin()) {
    Serial.println("DISPLAY:ERROR:BEGIN_FAILED");
    return;
  }

  Serial.println("DISPLAY:READY");
  runIntro();
  goTo(Screen::Menu);

  Serial1.begin(az2::kControlBaud, SERIAL_8N1, kTeensyRxPin, kTeensyTxPin);
  sendToTeensy(az2::kHelloControl);

  Wire.begin(kTouchSdaPin, kTouchSclPin);
  Wire.setClock(400000);  // I2C fast mode: tactile plus reactif
  Serial.println("TOUCH:FT6336U:READY");
}

void handleTouchDown(uint8_t slot, int16_t x, int16_t y) {
  Serial.print("TOUCH:DOWN:slot=");
  Serial.print(slot);
  Serial.print(":x=");
  Serial.print(x);
  Serial.print(":y=");
  Serial.println(y);

  if (currentScreen != Screen::Menu && hitBack(x, y)) {
    goTo(Screen::Menu);
  } else if (currentScreen == Screen::Menu) {
    const int8_t hit = hitTestMenuRow(x, y);
    if (hit >= 0) {
      goTo(kMenuItems[hit].target);
    }
  } else if (currentScreen == Screen::Audio) {
    const int8_t pad = hitTestAudioPad(x, y);
    if (pad >= 0 && pad != heldAudioPad[0] && pad != heldAudioPad[1]) {
      heldAudioPad[slot] = pad;
      drawAudioCell(static_cast<uint8_t>(pad), true);
      char msg[24];
      snprintf(msg, sizeof(msg), "PAD:%02d:DOWN:vel=100", pad);
      sendToTeensy(msg);
    }
  } else if (currentScreen == Screen::Sequencer) {
    const int8_t tempoHit = hitTestSeqTempo(x, y);
    uint8_t track, step;
    if (hitTestSeqTransport(x, y)) {
      sendToTeensy(seqPlaying ? az2::kStop : az2::kPlay);
    } else if (tempoHit >= 0) {
      // +/- 5 BPM par toucher, borne comme cote Teensy (30-300) -- pas
      // d'affichage optimiste, on attend l'echo BPM: confirme (voir
      // handleTeensyLine()).
      const int newBpm = constrain(static_cast<int>(seqBpm + 0.5f) + (tempoHit == 0 ? -5 : 5), 30, 300);
      char msg[16];
      snprintf(msg, sizeof(msg), "BPM:%d", newBpm);
      sendToTeensy(msg);
    } else if (hitTestSeqDivision(x, y)) {
      // Cycle vers la division suivante de la table partagee (voir
      // AZ2_Protocol.h: kDivisionOptions) -- meme logique, pas d'echo
      // optimiste, on attend la confirmation DIV: du Teensy.
      uint8_t nextIdx = 0;
      for (uint8_t i = 0; i < az2::kDivisionOptionCount; ++i) {
        if (az2::kDivisionOptions[i].stepsPerBeat == seqStepsPerBeat) {
          nextIdx = static_cast<uint8_t>((i + 1) % az2::kDivisionOptionCount);
          break;
        }
      }
      char msg[16];
      snprintf(msg, sizeof(msg), "DIV:%d", az2::kDivisionOptions[nextIdx].stepsPerBeat);
      sendToTeensy(msg);
    } else if (hitTestSeqCell(x, y, track, step)) {
      const bool newState = !seqStepOn[track][step];
      seqStepOn[track][step] = newState;  // optimiste ; re-synchronise par l'echo STEP: du Teensy
      drawSeqCell(track, step);
      char msg[20];
      snprintf(msg, sizeof(msg), "STEP:%d:%d:%d", track, step, newState ? 1 : 0);
      sendToTeensy(msg);
    }
  } else if (currentScreen == Screen::Engines) {
    bool isPatchSide = false;
    const int8_t track = hitTestEngRow(x, y, isPatchSide);
    if (track >= 0) {
      char msg[16];
      if (!isPatchSide) {
        const uint8_t nextEngine = static_cast<uint8_t>((trackEngine[track] + 1) % az2::kEngineCount);
        snprintf(msg, sizeof(msg), "ENGINE:%d:%d", track, nextEngine);
      } else {
        const uint8_t count = az2::enginePatchCount(trackEngine[track]);
        const uint8_t nextPatch = static_cast<uint8_t>((trackPatch[track] + 1) % count);
        snprintf(msg, sizeof(msg), "PATCH:%d:%d", track, nextPatch);
      }
      sendToTeensy(msg);
    }
  }
}

void handleTouchUp(uint8_t slot) {
  if (currentScreen == Screen::Audio && heldAudioPad[slot] >= 0) {
    drawAudioCell(static_cast<uint8_t>(heldAudioPad[slot]), false);
    char msg[16];
    snprintf(msg, sizeof(msg), "PAD:%02d:UP", heldAudioPad[slot]);
    sendToTeensy(msg);
  }
  heldAudioPad[slot] = -1;
}

void loop() {
  static uint32_t lastHeartbeatMs = 0;
  static bool wasActive[2] = {false, false};
  const uint32_t now = millis();

  readTeensyStatus();

  TouchPoint touches[2];
  readTouches(touches);

  for (uint8_t slot = 0; slot < 2; ++slot) {
    const bool active = touches[slot].active;
    if (active && !wasActive[slot]) {
      handleTouchDown(slot, touches[slot].x, touches[slot].y);
    } else if (!active && wasActive[slot]) {
      handleTouchUp(slot);
    }
    wasActive[slot] = active;
  }

  if (now - lastHeartbeatMs >= 1000) {
    lastHeartbeatMs = now;
    Serial.println("DISPLAY:ALIVE:TICK");
  }
}
