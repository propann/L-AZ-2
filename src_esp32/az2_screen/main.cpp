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
//    - CONTROLES : visualise en direct la croix + 4 boutons + 3 potards
//      cables directement sur le Teensy (NAV:/BTN:/POT:, remplace le
//      Pico/mux LED abandonnes le 2026-09-15, voir AZ2_CABLAGE_MASTER.md).
//      Pas de tap ici, page de VERIFICATION seulement.
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
#include <SPI.h>
#include <SD.h>
#include "gb_emulator.h"

namespace {

constexpr int kPinBacklight = 38;
constexpr int16_t kScreenSize = 480;

constexpr int kTeensyTxPin = 19;
constexpr int kTeensyRxPin = 20;

constexpr int kTouchSdaPin = 40;
constexpr int kTouchSclPin = 41;
constexpr uint8_t kTouchI2cAddr = 0x38;

// Carte SD (ROMs de jeux, voir AZ2_EMULATION_JEUX.md) -- broches
// verifiees sur le depot officiel VIEWE (meme source que l'ecran).
// ATTENTION : SD-CS (IO47) est le MEME GPIO que le bus SPI 3 fils de
// commande de l'ecran (SPI-SDA) -- pas un vrai conflit dans la pratique
// car l'ecran n'utilise ce bus QUE pendant gfx->begin() (init du
// GC9503V), jamais apres (le rendu passe ensuite par le panneau RGB
// parallele). Init SD faite APRES gfx->begin() dans setup(), jamais en
// meme temps qu'une commande ecran.
constexpr int kSdCsPin = 47;
constexpr int kSdClkPin = 45;
constexpr int kSdMisoPin = 46;
constexpr int kSdMosiPin = 42;

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
enum class Screen : uint8_t { Menu, Controls, Audio, Sequencer, Engines, Retro, Config, Links, About };
Screen currentScreen = Screen::Menu;

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
// Page Menu -- reorganisee en 4 cadres (MUSIQUE/JEUX/CONFIG/DOC) le
// 2026-09-15 ("on est trop charge, on fait 4 cadre reglage avec tout ce
// qui est config musique jeux doc") : la liste plate de 8 entrees
// devenait dense a l'ecran et a la croix. Deux niveaux : une grille de
// 4 cartes (categories), puis la sous-liste habituelle (meme style que
// l'ancien menu plat) une fois une categorie choisie.
// ---------------------------------------------------------------------
enum class MenuCat : uint8_t { Musique, Jeux, Config, Doc };
constexpr uint8_t kMenuCatCount = 4;

struct CategoryInfo {
  const char *label;
  const char *hint;
};

constexpr CategoryInfo kCategories[kMenuCatCount] = {
    {"MUSIQUE", "sequenceur, moteurs, audio"},
    {"JEUX", "emulateur Game Boy / GBC"},
    {"CONFIG", "reglages, croix/boutons"},
    {"DOC", "journal serie, a propos"},
};

struct MenuItem {
  const char *label;
  const char *hint;
  Screen target;
  MenuCat category;
};

constexpr MenuItem kMenuItems[] = {
    {"SEQUENCEUR", "programmer les 16 pas", Screen::Sequencer, MenuCat::Musique},
    {"MOTEURS", "moteur + patch par piste", Screen::Engines, MenuCat::Musique},
    {"AUDIO", "jouer le Teensy depuis l'ecran", Screen::Audio, MenuCat::Musique},
    {"JEUX", "Game Boy / GBC (ROM sur carte SD)", Screen::Retro, MenuCat::Jeux},
    {"CONFIGURATION", "ecran de veille, reglages", Screen::Config, MenuCat::Config},
    {"CONTROLES", "croix + boutons + potards (Teensy)", Screen::Controls, MenuCat::Config},
    {"LIENS SERIE", "journal ESP32 / Teensy", Screen::Links, MenuCat::Doc},
    {"A PROPOS", "version, roles, build", Screen::About, MenuCat::Doc},
};
constexpr uint8_t kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

// Remplit out[] (capacite >= kMenuItemCount) avec les index (dans
// kMenuItems) des entrees de la categorie cat, renvoie le compte.
uint8_t categoryItems(MenuCat cat, uint8_t *out) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < kMenuItemCount; ++i) {
    if (kMenuItems[i].category == cat) {
      out[count++] = i;
    }
  }
  return count;
}

// -1 = grille des 4 categories (accueil du menu), 0-3 = sous-liste de
// la categorie choisie. Remis a -1 a chaque entree sur Screen::Menu
// depuis un autre ecran (voir goTo()) -- toujours revenir a l'accueil.
int8_t menuCategory = -1;
// Ligne/carte survolee par la croix (voir NAV: dans handleTeensyLine())
// -- demande 2026-09-15 ("il faut que ca serve dans les menus") : la
// croix + le bouton A pilotent le menu, pas seulement le tactile.
int8_t menuSelected = 0;

constexpr int16_t kMenuLeft = 48;
constexpr int16_t kMenuTop = 130;
// 34 (au lieu de 39) depuis l'ajout de CONFIGURATION -- une sous-liste
// (3 entrees max par categorie) doit tenir avant la barre d'etat.
constexpr int16_t kMenuRowH = 34;
constexpr int16_t kMenuWidth = kScreenSize - 2 * kMenuLeft;

// Grille 2x2 des 4 cartes de categorie.
constexpr int16_t kCatTop = 110;
constexpr int16_t kCatGap = 16;
constexpr int16_t kCatW = (kScreenSize - 2 * kMargin - kCatGap) / 2;
constexpr int16_t kCatH = (kStatusY - kCatTop - kCatGap - 24) / 2;

void catRect(uint8_t index, int16_t &x, int16_t &y) {
  x = static_cast<int16_t>(kMargin + (index % 2) * (kCatW + kCatGap));
  y = static_cast<int16_t>(kCatTop + (index / 2) * (kCatH + kCatGap));
}

void drawCategoryCard(uint8_t index) {
  int16_t x, y;
  catRect(index, x, y);
  const uint16_t accent = kPalette[index % kPaletteCount];
  const bool selected = (index == menuSelected);
  gfx->fillRect(x, y, kCatW, kCatH, selected ? accent : RGB565_BLACK);
  gfx->drawRect(x, y, kCatW, kCatH, accent);
  gfx->setTextSize(3);
  gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + 14), static_cast<int16_t>(y + kCatH / 2 - 24));
  gfx->print(kCategories[index].label);
  gfx->setTextSize(1);
  gfx->setTextColor(selected ? RGB565_BLACK : kDim);
  gfx->setCursor(static_cast<int16_t>(x + 14), static_cast<int16_t>(y + kCatH / 2 + 6));
  gfx->print(kCategories[index].hint);
}

void drawMenuSubRow(uint8_t rowIndex, uint8_t itemIndex) {
  const int16_t y = static_cast<int16_t>(kMenuTop + rowIndex * kMenuRowH);
  const uint16_t accent = kPalette[itemIndex % kPaletteCount];
  const bool selected = (rowIndex == menuSelected);
  gfx->fillRect(kMenuLeft, y, kMenuWidth, kMenuRowH - 6, selected ? accent : RGB565_BLACK);
  gfx->drawRect(kMenuLeft, y, kMenuWidth, kMenuRowH - 6, accent);
  gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kMenuLeft + 14), static_cast<int16_t>(y + 2));
  gfx->print(kMenuItems[itemIndex].label);
  gfx->setTextSize(1);
  gfx->setTextColor(selected ? RGB565_BLACK : kDim);
  gfx->setCursor(static_cast<int16_t>(kMenuLeft + 14), static_cast<int16_t>(y + 20));
  gfx->print(kMenuItems[itemIndex].hint);
}

void drawMenu() {
  if (menuCategory < 0) {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(3);
    gfx->setCursor(kMargin, 48);
    gfx->print("AZ-2");
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(static_cast<int16_t>(kMargin + 90), 60);
    gfx->print("CHOISIS UNE SECTION");
    gfx->drawFastHLine(kMargin, 96, kScreenSize - 2 * kMargin, kFaint);
    for (uint8_t i = 0; i < kMenuCatCount; ++i) {
      drawCategoryCard(i);
    }
    drawLinkStatus();
  } else {
    drawSubHeader(kCategories[menuCategory].label, kPalette[menuCategory % kPaletteCount]);
    uint8_t items[kMenuItemCount];
    const uint8_t count = categoryItems(static_cast<MenuCat>(menuCategory), items);
    for (uint8_t i = 0; i < count; ++i) {
      drawMenuSubRow(i, items[i]);
    }
  }
}

int8_t hitTestCategoryCard(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < kMenuCatCount; ++i) {
    int16_t cx, cy;
    catRect(i, cx, cy);
    if (x >= cx && x < cx + kCatW && y >= cy && y < cy + kCatH) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

int8_t hitTestMenuSubRow(int16_t x, int16_t y, uint8_t count) {
  if (x < kMenuLeft || x > kMenuLeft + kMenuWidth) {
    return -1;
  }
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t rowTop = kMenuTop + i * kMenuRowH;
    if (y >= rowTop && y < rowTop + (kMenuRowH - 6)) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

// kGridLeft/kGridTop/kGridCell/kGridGap/padCellRect() : grille 4x4
// partagee avec la page AUDIO plus bas (touche = jouer le Teensy) --
// gardee ici meme si la page PADS & LEDS d'origine (qui les a introduits)
// a ete remplacee par CONTROLES ci-dessous.
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

// ---------------------------------------------------------------------
// Page CONTROLES -- visualisation seule de la croix + 4 boutons + 3
// potards cables DIRECTEMENT sur le Teensy (NAV:/BTN:/POT:, voir
// AZ2_Protocol.h) -- remplace PADS & LEDS le 2026-09-15, suite a
// l'abandon du Pico/mux LED (voir AZ2_CABLAGE_MASTER.md). Permet de
// verifier chaque switch/potard un par un pendant le cablage.
// ---------------------------------------------------------------------
bool navState[4] = {};  // 0=HAUT 1=BAS 2=GAUCHE 3=DROITE
bool btnState[4] = {};  // 0=A 1=B 2=C 3=D
uint8_t potValue[3] = {};
// Bouton poussoir integre a chaque encodeur rotatif (ENC:0-2, voir
// AZ2_Protocol.h) -- ajoute le 2026-09-15, absent du premier cablage
// (potards simples n'avaient pas de bouton).
bool encSwState[3] = {};

constexpr int16_t kNavBox = 56;
constexpr int16_t kNavGap = 4;
constexpr int16_t kNavCenterX = 130;
constexpr int16_t kNavCenterY = 220;

void navRect(uint8_t dir, int16_t &x, int16_t &y) {
  switch (dir) {
    case 0: x = kNavCenterX - kNavBox / 2; y = kNavCenterY - kNavBox - kNavGap; break;
    case 1: x = kNavCenterX - kNavBox / 2; y = kNavCenterY + kNavGap; break;
    case 2: x = kNavCenterX - kNavBox - kNavGap; y = kNavCenterY - kNavBox / 2; break;
    default: x = kNavCenterX + kNavGap; y = kNavCenterY - kNavBox / 2; break;
  }
}

void drawNavBox(uint8_t dir) {
  int16_t x, y;
  navRect(dir, x, y);
  static const char *const kLabels[4] = {"H", "B", "G", "D"};
  gfx->fillRect(x, y, kNavBox, kNavBox, navState[dir] ? kPalette[dir % kPaletteCount] : RGB565_BLACK);
  gfx->drawRect(x, y, kNavBox, kNavBox, kFaint);
  gfx->setTextSize(3);
  gfx->setTextColor(navState[dir] ? RGB565_BLACK : kDim);
  gfx->setCursor(static_cast<int16_t>(x + kNavBox / 2 - 9), static_cast<int16_t>(y + kNavBox / 2 - 12));
  gfx->print(kLabels[dir]);
}

constexpr int16_t kBtnBox = 60;
constexpr int16_t kBtnGap = 12;
constexpr int16_t kBtnLeft = 300;
constexpr int16_t kBtnTop = 160;

void btnRect(uint8_t index, int16_t &x, int16_t &y) {
  x = static_cast<int16_t>(kBtnLeft + (index % 2) * (kBtnBox + kBtnGap));
  y = static_cast<int16_t>(kBtnTop + (index / 2) * (kBtnBox + kBtnGap));
}

void drawBtnBox(uint8_t index) {
  int16_t x, y;
  btnRect(index, x, y);
  static const char kLabels[4] = {'A', 'B', 'C', 'D'};
  gfx->fillRect(x, y, kBtnBox, kBtnBox, btnState[index] ? kPalette[(index + 1) % kPaletteCount] : RGB565_BLACK);
  gfx->drawRect(x, y, kBtnBox, kBtnBox, kFaint);
  gfx->setTextSize(3);
  gfx->setTextColor(btnState[index] ? RGB565_BLACK : kDim);
  gfx->setCursor(static_cast<int16_t>(x + kBtnBox / 2 - 9), static_cast<int16_t>(y + kBtnBox / 2 - 12));
  gfx->print(kLabels[index]);
}

constexpr int16_t kPotBarX = kMargin;
constexpr int16_t kPotBarW = kScreenSize - 2 * kMargin;
constexpr int16_t kPotBarH = 26;
constexpr int16_t kPotBarTop = 340;
constexpr int16_t kPotBarGap = 30;

// Bouton poussoir integre a l'encodeur (ENC:0-2, voir AZ2_Protocol.h) --
// pas de fonction musicale assignee, le temoin visuel EST le slider
// entier qui s'allume au complet pendant l'appui (demande 2026-09-15,
// "il faut que ca allume le slider complet"), plutot qu'un petit
// indicateur separe difficile a voir. encSwState[index] est lu
// directement par drawPotBar() ci-dessous ; appeler drawPotBar() suffit
// pour rafraichir l'affichage du clic, pas de fonction dediee.
void drawPotBar(uint8_t index) {
  const int16_t y = static_cast<int16_t>(kPotBarTop + index * (kPotBarH + kPotBarGap));
  static const char *const kLabels[3] = {"POT 1 - VOLUME", "POT 2 - REVERB", "POT 3 - DELAY"};

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(kPotBarX, static_cast<int16_t>(y - 14));
  gfx->print(kLabels[index]);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", potValue[index]);
  gfx->setCursor(static_cast<int16_t>(kPotBarX + kPotBarW - 28), static_cast<int16_t>(y - 14));
  gfx->print(buf);

  gfx->fillRect(kPotBarX, y, kPotBarW, kPotBarH, RGB565_BLACK);
  gfx->drawRect(kPotBarX, y, kPotBarW, kPotBarH, kFaint);
  // Bouton presse : slider allume a 100% quelle que soit la position
  // reelle du potard (temoin visuel du clic). Sinon, remplissage
  // proportionnel normal a la valeur.
  const int16_t fillW = encSwState[index]
                             ? static_cast<int16_t>(kPotBarW - 4)
                             : static_cast<int16_t>(static_cast<float>(potValue[index]) / 127.0f * (kPotBarW - 4));
  if (fillW > 0) {
    gfx->fillRect(static_cast<int16_t>(kPotBarX + 2), static_cast<int16_t>(y + 2), fillW,
                  static_cast<int16_t>(kPotBarH - 4), kPalette[index % kPaletteCount]);
  }
}

void drawControlsPage() {
  drawSubHeader("CONTROLES", kPalette[0]);
  for (uint8_t i = 0; i < 4; ++i) {
    drawNavBox(i);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    drawBtnBox(i);
  }
  for (uint8_t i = 0; i < 3; ++i) {
    drawPotBar(i);
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
// Note par pas (voir NOTE: dans AZ2_Protocol.h -- porte de MicroDexed-touch,
// demande le 2026-09-15 "prend le sequenceur du dexed touch"). Meme
// defauts que seedDefaultNotes() cote Teensy tant que le NOTE: echo n'est
// pas arrive.
uint8_t seqStepNote[kSeqTrackCount][kSeqStepCount];
uint8_t seqCurrentStep = 0;
bool seqPlaying = false;
float seqBpm = 120.0f;
uint8_t seqStepsPerBeat = 4;

// Dernier pas touche : la croix du Teensy (NAV:UP/DOWN, voir
// handleTeensyLine()) transpose la note de CE pas -- pas besoin d'une
// interface piano-roll complete, on reutilise le clavier physique qu'on
// vient de cabler.
int8_t selectedSeqTrack = -1;
int8_t selectedSeqStep = -1;

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
  const bool selected = (track == selectedSeqTrack && step == selectedSeqStep);
  gfx->fillRect(x, y, kSeqCellW, kSeqCellH, on ? kPalette[track % kPaletteCount] : seqBandColor(step));

  uint16_t borderColor = kFaint;
  if (playhead) {
    borderColor = RGB565_WHITE;
  } else if (selected) {
    borderColor = kPalette[4 % kPaletteCount];  // accent distinct pour "pas selectionne" (croix transpose sa note)
  }
  gfx->drawRect(x, y, kSeqCellW, kSeqCellH, borderColor);

  if (on) {
    // Petit repere de hauteur = note du pas (voir seqStepNote[], NOTE:
    // dans AZ2_Protocol.h) -- plus haut dans la case = note plus aigue.
    // Plage d'affichage 36-84 (3 octaves autour du C4), bornee au-dela.
    constexpr uint8_t kNoteVisualMin = 36;
    constexpr uint8_t kNoteVisualMax = 84;
    const uint8_t note = constrain(seqStepNote[track][step], kNoteVisualMin, kNoteVisualMax);
    const float ratio = static_cast<float>(note - kNoteVisualMin) / static_cast<float>(kNoteVisualMax - kNoteVisualMin);
    const int16_t notchY = static_cast<int16_t>(y + kSeqCellH - 3 - ratio * (kSeqCellH - 6));
    gfx->drawFastHLine(static_cast<int16_t>(x + 2), notchY, static_cast<int16_t>(kSeqCellW - 4), RGB565_WHITE);
  }
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
// Page JEUX -- emulation Game Boy / Game Boy Color (Walnut-CGB, voir
// gb_emulator.h/.cpp et docs/AZ2_EMULATION_JEUX.md). GBA ecarte du v0
// (~20fps mesures sur ESP32-S3 avec les coeurs existants, pas fluide).
// Liste des ROM /games/*.gb(c) rescannee en entrant sur la page (voir
// goTo()) -- toucher une ligne la charge et demarre le jeu ; ROM
// dechargee en quittant la page. PAS DE SON pour l'instant (voir
// gb_emulator.cpp).
// ---------------------------------------------------------------------
char gbRomNames[kGbMaxRoms][kGbRomNameLen];
uint8_t gbRomCount = 0;

constexpr int16_t kRomRowTop = 90;
constexpr int16_t kRomRowH = 40;

void romRowRect(uint8_t index, int16_t &y) {
  y = static_cast<int16_t>(kRomRowTop + index * kRomRowH);
}

void drawRomRow(uint8_t index) {
  int16_t y;
  romRowRect(index, y);
  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6, RGB565_BLACK);
  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6, kPalette[index % kPaletteCount]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 10), static_cast<int16_t>(y + 6));
  gfx->print(gbRomNames[index]);
}

int8_t hitTestRomRow(int16_t x, int16_t y) {
  if (x < kMargin || x > kScreenSize - kMargin) {
    return -1;
  }
  for (uint8_t i = 0; i < gbRomCount; ++i) {
    int16_t rowY;
    romRowRect(i, rowY);
    if (y >= rowY && y < rowY + (kRomRowH - 6)) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

void drawRetroPage() {
  if (gbIsLoaded()) {
    // Le rendu du jeu lui-meme vient de gbBlitLine(), appelee par
    // gbRunFrame() depuis loop() -- ici on affiche juste le cadre/titre
    // une fois, le jeu se dessine par-dessus a chaque frame.
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 4);
    gfx->print(gbRomTitle());
    return;
  }

  if (gbRomCount > 0) {
    drawSubHeader("JEUX - choisis une ROM", kPalette[2]);
    for (uint8_t i = 0; i < gbRomCount; ++i) {
      drawRomRow(i);
    }
    return;
  }

  drawSubHeader("JEUX", kPalette[2]);
  const char *lines[] = {
      "Aucune ROM trouvee dans /games.",
      "",
      "Moteur : Walnut-CGB (GB/GBC, licence MIT).",
      "GBA ecarte : ~20fps mesures sur ESP32-S3,",
      "pas fluide avec les coeurs existants.",
      "",
      "Pour jouer : carte SD formatee FAT32,",
      "dossier /games/, un ou plusieurs fichiers",
      ".gb ou .gbc dedans (ROM homebrew/domaine",
      "public -- pas de ROM commerciale fournie).",
      "",
      "Pas de son pour l'instant (voir",
      "docs/AZ2_EMULATION_JEUX.md).",
      "",
      "Retouche cette page pour reessayer",
      "de scanner la carte SD.",
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
      "Audio: Teensy 4.1, 5 moteurs, 8 pistes, FX maitre",
      "Controle: croix + 4 boutons + 3 potards (Teensy)",
      "Build: screen_esp, 2026-09-15",
  };
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  for (uint8_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
    gfx->setCursor(kMargin, static_cast<int16_t>(90 + i * 22));
    gfx->print(lines[i]);
  }
}

// ---------------------------------------------------------------------
// Page CONFIGURATION -- reglage de l'ecran de veille "Matrix" (demande le
// 2026-09-15). 0 = desactive. Reglage en RAM uniquement pour l'instant
// (pas de sauvegarde flash/NVS -- revient a la valeur par defaut au
// redemarrage, a ajouter plus tard si besoin).
// ---------------------------------------------------------------------
// Remis a 60 (1 minute, demande explicitement) -- le vrai probleme
// n'etait pas la duree mais un bug qui relancait la veille juste apres
// l'avoir quittee (voir le commentaire pres de "nowForIdle" dans loop()).
// Reste reglable en direct depuis cette page.
uint16_t screensaverTimeoutSec = 60;
constexpr uint16_t kScreensaverStepSec = 10;
constexpr uint16_t kScreensaverMaxSec = 600;

constexpr int16_t kCfgRowY = 140;
constexpr int16_t kCfgRowH = 50;
constexpr int16_t kCfgBtnW = 60;

void drawConfigPage() {
  drawSubHeader("CONFIGURATION", kPalette[3]);

  const int16_t minusX = kMargin;
  const int16_t plusX = static_cast<int16_t>(kScreenSize - kMargin - kCfgBtnW);

  gfx->fillRect(kMargin, kCfgRowY, kScreenSize - 2 * kMargin, kCfgRowH, RGB565_BLACK);
  gfx->drawRect(minusX, kCfgRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->drawRect(plusX, kCfgRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(minusX + 20), static_cast<int16_t>(kCfgRowY + 10));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 20), static_cast<int16_t>(kCfgRowY + 10));
  gfx->print('+');

  char buf[24];
  if (screensaverTimeoutSec == 0) {
    snprintf(buf, sizeof(buf), "DESACTIVE");
  } else {
    snprintf(buf, sizeof(buf), "%u s", screensaverTimeoutSec);
  }
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 50), static_cast<int16_t>(kCfgRowY + 15));
  gfx->print(buf);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, static_cast<int16_t>(kCfgRowY - 20));
  gfx->print("ECRAN DE VEILLE (MATRIX) APRES");
}

bool hitTestCfgMinus(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kCfgRowY, kCfgBtnW, kCfgRowH);
}
bool hitTestCfgPlus(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kCfgBtnW), kCfgRowY, kCfgBtnW, kCfgRowH);
}

// ---------------------------------------------------------------------
// Ecran de veille "Matrix" -- s'active apres screensaverTimeoutSec sans
// activite (toucher ecran OU NAV:/BTN: du Teensy, voir loop()/
// handleTeensyLine()). Demande le 2026-09-15, reprend l'esthetique
// "pixel art / synthwave / matrice" du projet depuis le debut.
// ---------------------------------------------------------------------
constexpr int16_t kMatrixCharW = 12;
constexpr int16_t kMatrixCharH = 16;
constexpr uint8_t kMatrixCols = kScreenSize / kMatrixCharW;
constexpr int16_t kMatrixRows = kScreenSize / kMatrixCharH;
constexpr uint8_t kMatrixTrailLen = 10;

bool screensaverActive = false;
uint32_t lastActivityMs = 0;
int16_t matrixDropRow[kMatrixCols];
uint32_t matrixLastStepMs = 0;

char matrixRandomChar() {
  return static_cast<char>(random(33, 126));
}

void screensaverEnter() {
  screensaverActive = true;
  gfx->fillScreen(RGB565_BLACK);
  for (uint8_t c = 0; c < kMatrixCols; ++c) {
    matrixDropRow[c] = static_cast<int16_t>(-random(0, kMatrixRows));
  }
}

void drawScreen(Screen s);  // definie plus bas, utilisee ici

void screensaverExit() {
  screensaverActive = false;
  drawScreen(currentScreen);
}

// Rythme + couleurs adoucis le 2026-09-15 ("l'ecran de veille saute tout
// le temps a la figure, faut le rendre moins present") -- plus lent
// (110ms/pas au lieu de 60) et moins lumineux (plus de blanc pur en
// tete de goutte).
void screensaverStep() {
  const uint32_t now = millis();
  if (now - matrixLastStepMs < 110) {
    return;
  }
  matrixLastStepMs = now;

  gfx->setTextSize(2);
  for (uint8_t c = 0; c < kMatrixCols; ++c) {
    const int16_t x = static_cast<int16_t>(c * kMatrixCharW);

    const int16_t tailRow = static_cast<int16_t>(matrixDropRow[c] - kMatrixTrailLen);
    if (tailRow >= 0 && tailRow < kMatrixRows) {
      gfx->setTextColor(RGB565_BLACK);
      gfx->setCursor(x, static_cast<int16_t>(tailRow * kMatrixCharH));
      gfx->print(matrixRandomChar());
    }

    const int16_t trailRow = static_cast<int16_t>(matrixDropRow[c] - 1);
    if (trailRow >= 0 && trailRow < kMatrixRows) {
      gfx->setTextColor(RGB565(0, 90, 40));
      gfx->setCursor(x, static_cast<int16_t>(trailRow * kMatrixCharH));
      gfx->print(matrixRandomChar());
    }

    if (matrixDropRow[c] >= 0 && matrixDropRow[c] < kMatrixRows) {
      gfx->setTextColor(RGB565(110, 200, 120));
      gfx->setCursor(x, static_cast<int16_t>(matrixDropRow[c] * kMatrixCharH));
      gfx->print(matrixRandomChar());
    }

    ++matrixDropRow[c];
    if (matrixDropRow[c] - kMatrixTrailLen > kMatrixRows) {
      matrixDropRow[c] = static_cast<int16_t>(-random(0, kMatrixRows));
    }
  }
}

void noteActivity() {
  lastActivityMs = millis();
}

void drawScreen(Screen s) {
  switch (s) {
    case Screen::Menu: drawMenu(); return;
    case Screen::Controls: drawControlsPage(); return;
    case Screen::Audio: drawAudioPage(); return;
    case Screen::Sequencer: drawSequencerPage(); return;
    case Screen::Engines: drawEnginesPage(); return;
    case Screen::Retro: drawRetroPage(); return;
    case Screen::Config: drawConfigPage(); return;
    case Screen::Links: drawLinksPage(); return;
    case Screen::About: drawAboutPage(); return;
  }
}

void goTo(Screen s) {
  // Rescanne /games et decharge la ROM GB en entrant/sortant de la page
  // JEUX (voir gb_emulator.h) -- libere la PSRAM des qu'on quitte, evite
  // de garder une ROM chargee inutilement sur les autres pages. Le scan
  // remplit gbRomNames[]/gbRomCount, affiches en liste par
  // drawRetroPage() (demande 2026-09-15 : "il nous faut un menu ...
  // dans une liste de rom pas uniquement un jeux").
  if (s == Screen::Retro && !gbIsLoaded()) {
    gbRomCount = gbScanRoms(gbRomNames);
  } else if (s != Screen::Retro && gbIsLoaded()) {
    gbUnload();
  }

  // Revenir sur le menu depuis un autre ecran retombe toujours sur la
  // grille des 4 categories, jamais au milieu d'une sous-liste --
  // simple, pas d'etat perime a gerer (voir la page Menu plus haut).
  if (s == Screen::Menu) {
    menuCategory = -1;
    menuSelected = 0;
  }

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

  if (line.startsWith("NAV:")) {
    const int firstColon = line.indexOf(':');
    const int secondColon = line.indexOf(':', firstColon + 1);
    if (firstColon >= 0 && secondColon >= 0) {
      const String dir = line.substring(firstColon + 1, secondColon);
      const bool pressed = line.endsWith("DOWN");
      if (pressed) {
        noteActivity();
        if (screensaverActive) {
          screensaverExit();
        }
      }
      int8_t index = -1;
      if (dir == "UP") index = 0;
      else if (dir == "DOWN") index = 1;
      else if (dir == "LEFT") index = 2;
      else if (dir == "RIGHT") index = 3;

      if (index >= 0) {
        navState[index] = pressed;
        if (currentScreen == Screen::Controls && !screensaverActive) {
          drawNavBox(static_cast<uint8_t>(index));
        }
        // Page JEUX : la croix pilote directement le Game Boy (voir
        // gb_emulator.h -- GbButton::Up/Down/Left/Right sont dans le
        // meme ordre que index ici, 0-3).
        if (currentScreen == Screen::Retro) {
          gbSetButton(static_cast<GbButton>(index), pressed);
        }
        // Menu principal : la croix deplace la selection surlignee --
        // demande 2026-09-15 ("il faut que ca serve dans les menus"),
        // confirmer avec BTN:A (voir plus bas). Grille de categories
        // (menuCategory<0) : les 4 directions naviguent le 2x2. Sous-
        // liste (menuCategory>=0) : HAUT/BAS parcourent la liste,
        // GAUCHE revient a la grille.
        if (pressed && currentScreen == Screen::Menu) {
          if (menuCategory < 0) {
            const int8_t previous = menuSelected;
            switch (index) {
              case 0: if (menuSelected >= 2) menuSelected -= 2; break;               // HAUT
              case 1: if (menuSelected < 2) menuSelected += 2; break;                // BAS
              case 2: if (menuSelected % 2 == 1) menuSelected -= 1; break;           // GAUCHE
              case 3: if (menuSelected % 2 == 0) menuSelected += 1; break;           // DROITE
            }
            if (menuSelected != previous) {
              drawCategoryCard(static_cast<uint8_t>(previous));
              drawCategoryCard(static_cast<uint8_t>(menuSelected));
            }
          } else if (index == 0 || index == 1) {
            uint8_t items[kMenuItemCount];
            const uint8_t count = categoryItems(static_cast<MenuCat>(menuCategory), items);
            const int8_t previous = menuSelected;
            if (index == 0) {
              menuSelected = static_cast<int8_t>((menuSelected == 0) ? count - 1 : menuSelected - 1);
            } else {
              menuSelected = static_cast<int8_t>((menuSelected + 1) % count);
            }
            drawMenuSubRow(static_cast<uint8_t>(previous), items[previous]);
            drawMenuSubRow(static_cast<uint8_t>(menuSelected), items[menuSelected]);
          } else if (index == 2) {
            menuCategory = -1;
            menuSelected = 0;
            drawMenu();
          }
        }
        // Sur la page SEQUENCEUR, HAUT/BAS transpose la note du dernier
        // pas touche (voir selectedSeqTrack/Step) -- demande le
        // 2026-09-15 ("prend le sequenceur du dexed touch"), reutilise
        // la croix qu'on vient de cabler au lieu d'une interface
        // piano-roll complete.
        if (pressed && currentScreen == Screen::Sequencer &&
            selectedSeqTrack >= 0 && selectedSeqStep >= 0 && (index == 0 || index == 1)) {
          const uint8_t t = static_cast<uint8_t>(selectedSeqTrack);
          const uint8_t s = static_cast<uint8_t>(selectedSeqStep);
          const int newNote = constrain(static_cast<int>(seqStepNote[t][s]) + (index == 0 ? 1 : -1), 0, 127);
          char msg[20];
          snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", t, s, newNote);
          sendToTeensy(msg);
        }
      }
    }
  } else if (line.startsWith("BTN:") && line.length() >= 6) {
    const char letter = line.charAt(4);
    const bool pressed = line.endsWith("DOWN");
    if (pressed) {
      noteActivity();
      if (screensaverActive) {
        screensaverExit();
      }
    }
    const int8_t index = letter - 'A';
    if (index >= 0 && index < 4) {
      btnState[index] = pressed;
      if (currentScreen == Screen::Controls && !screensaverActive) {
        drawBtnBox(static_cast<uint8_t>(index));
      }
      // Page JEUX : A/B -> boutons Game Boy A/B. SELECT/START sont
      // passes aux encodeurs 2/3 (voir ENC: plus bas) -- demande
      // 2026-09-15 ("les boutons start select faut les config sur les
      // encodeurs ... comme ca on a a/b, start/select et les
      // gachettes") : C/D se liberent pour un futur role de gachette
      // (pas encore assigne).
      const bool inGbGame = (currentScreen == Screen::Retro && gbIsLoaded());
      if (currentScreen == Screen::Retro && index < 2) {
        static const GbButton kGbMap[2] = {GbButton::A, GbButton::B};
        gbSetButton(kGbMap[index], pressed);
      }
      // Menu principal : A confirme la selection surlignee par la
      // croix (voir menuSelected ci-dessus) -- demande 2026-09-15.
      // Grille de categories -> entre dans la categorie ; sous-liste ->
      // ouvre la page choisie (comme un tap tactile).
      if (pressed && letter == 'A' && currentScreen == Screen::Menu) {
        if (menuCategory < 0) {
          menuCategory = menuSelected;
          menuSelected = 0;
          drawMenu();
        } else {
          uint8_t items[kMenuItemCount];
          const uint8_t count = categoryItems(static_cast<MenuCat>(menuCategory), items);
          if (menuSelected < count) {
            goTo(kMenuItems[items[menuSelected]].target);
          }
        }
      }
      // Dans une sous-liste du menu : B revient a la grille de
      // categories (pas besoin de ressortir de Screen::Menu).
      if (pressed && letter == 'B' && currentScreen == Screen::Menu && menuCategory >= 0) {
        menuCategory = -1;
        menuSelected = 0;
        drawMenu();
      }
      // Partout ailleurs (sauf en pleine partie GB, ou B est le bouton
      // B du jeu) : B revient au menu -- convention manette classique,
      // meme demande ("il faut que ca serve dans les menus").
      if (pressed && letter == 'B' && currentScreen != Screen::Menu && !inGbGame) {
        goTo(Screen::Menu);
      }
    }
  } else if (line.startsWith("POT:")) {
    const int firstColon = line.indexOf(':');
    const int secondColon = line.indexOf(':', firstColon + 1);
    if (firstColon >= 0 && secondColon >= 0) {
      const uint8_t index = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
      const uint8_t value = static_cast<uint8_t>(line.substring(secondColon + 1).toInt());
      if (index < 3) {
        potValue[index] = value;
        if (currentScreen == Screen::Controls && !screensaverActive) {
          drawPotBar(index);
        }
      }
    }
  } else if (line.startsWith("ENC:")) {
    // Bouton poussoir integre a l'encodeur (voir AZ2_Protocol.h) --
    // temoin visuel sur la page CONTROLES (allume le slider POT
    // correspondant a 100% pendant l'appui, voir drawPotBar()), PLUS en
    // page JEUX : encodeur 1 (Reverb) -> SELECT, encodeur 2 (Delay) ->
    // START (demande 2026-09-15, "faut les config sur les encodeurs ...
    // comme ca on a a/b, start/select et les gachettes" -- libere C/D
    // pour un futur role de gachette). Encodeur 0 (Volume) reserve au
    // declencheur d'enregistrement de sample (voir gb_audio.h).
    const int firstColon = line.indexOf(':');
    const int secondColon = line.indexOf(':', firstColon + 1);
    if (firstColon >= 0 && secondColon >= 0) {
      const uint8_t index = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
      const bool pressed = line.endsWith("DOWN");
      if (index < 3) {
        noteActivity();
        if (screensaverActive) {
          screensaverExit();
        }
        encSwState[index] = pressed;
        if (currentScreen == Screen::Controls && !screensaverActive) {
          drawPotBar(index);
        }
        if (currentScreen == Screen::Retro && gbIsLoaded()) {
          if (index == 1) {
            gbSetButton(GbButton::Select, pressed);
          } else if (index == 2) {
            gbSetButton(GbButton::Start, pressed);
          }
        }
      }
    }
  } else if (line.startsWith("NOTE:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t note = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && step < kSeqStepCount) {
        seqStepNote[track][step] = note;
        if (currentScreen == Screen::Sequencer && !screensaverActive) {
          drawSeqCell(track, step);
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
        if (currentScreen == Screen::Sequencer && !screensaverActive) {
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
        if (currentScreen == Screen::Sequencer && !screensaverActive) {
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
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawSeqTransport();
      }
    }
  } else if (line.startsWith("BPM:")) {
    const float value = line.substring(4).toFloat();
    if (value > 0.0f) {
      seqBpm = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawSeqTempo();
      }
    }
  } else if (line.startsWith("DIV:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(4).toInt());
    if (value > 0) {
      seqStepsPerBeat = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
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
  if (currentScreen != Screen::Controls && currentScreen != Screen::Links) {
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

// Rendu Game Boy (voir gb_emulator.h/.cpp) : hors namespace anonyme pour
// avoir un lien externe (appelee depuis gb_emulator.cpp, autre unite de
// compilation) tout en gardant acces a `gfx`/`kScreenSize` (recherche de
// nom non qualifiee, valide pour le reste du fichier apres la fermeture
// du namespace). 160x144 -> mise a l'echelle x3 = 480x432, centree
// verticalement (24px de marge haut/bas).
constexpr int16_t kGbScaledW = 160 * 3;
constexpr int16_t kGbScaledH = 144 * 3;
constexpr int16_t kGbScreenTop = (kScreenSize - kGbScaledH) / 2;

// Un SEUL draw16bitRGBBitmap() par ligne source (480x3 d'un coup) au
// lieu de 3 -- demande le 2026-09-15 ("on a des sauts d'images, on peut
// stabiliser"), moins d'appels = moins de surcharge par appel vers le
// bus RGB parallele.
void gbBlitLine(int line, const uint16_t *row) {
  static uint16_t scaledBlock[kGbScaledW * 3];
  for (int x = 0; x < 160; ++x) {
    const uint16_t c = row[x];
    const int16_t base = static_cast<int16_t>(x * 3);
    scaledBlock[base] = c;
    scaledBlock[base + 1] = c;
    scaledBlock[base + 2] = c;
  }
  // Les 2 autres rangees de sortie sont identiques a la premiere.
  memcpy(scaledBlock + kGbScaledW, scaledBlock, kGbScaledW * sizeof(uint16_t));
  memcpy(scaledBlock + kGbScaledW * 2, scaledBlock, kGbScaledW * sizeof(uint16_t));

  const int16_t y = static_cast<int16_t>(kGbScreenTop + line * 3);
  gfx->draw16bitRGBBitmap(0, y, scaledBlock, kGbScaledW, 3);
}

void setup() {
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:ROLE:ESP32_SCREEN_TEST");

  // Memes valeurs par defaut que seedDefaultNotes() cote Teensy --
  // corrige des le premier NOTE:/HELLO recu si jamais desynchronise.
  static const uint8_t kDefaultNotes[kSeqTrackCount] = {48, 55, 60, 64};
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    for (uint8_t s = 0; s < kSeqStepCount; ++s) {
      seqStepNote[t][s] = kDefaultNotes[t];
    }
  }

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

  // Init SD APRES l'ecran (voir commentaire sur kSdCsPin plus haut) --
  // carte pas forcement presente, echec propre attendu tant qu'elle n'est
  // pas inseree (voir AZ2_EMULATION_JEUX.md, ROMs GB/GBC).
  SPI.begin(kSdClkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  if (SD.begin(kSdCsPin, SPI)) {
    Serial.print("SD:READY:size_mb=");
    Serial.println(static_cast<uint32_t>(SD.cardSize() / (1024 * 1024)));
  } else {
    Serial.println("SD:NOT_PRESENT");
  }
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
    if (menuCategory < 0) {
      const int8_t hit = hitTestCategoryCard(x, y);
      if (hit >= 0) {
        menuCategory = hit;
        menuSelected = 0;
        drawMenu();
      }
    } else if (hitBack(x, y)) {
      // "< CATEGORIE" en haut a gauche (voir drawSubHeader) -> retour a
      // la grille, meme zone tactile que le retour au menu habituel.
      menuCategory = -1;
      menuSelected = 0;
      drawMenu();
    } else {
      uint8_t items[kMenuItemCount];
      const uint8_t count = categoryItems(static_cast<MenuCat>(menuCategory), items);
      const int8_t hit = hitTestMenuSubRow(x, y, count);
      if (hit >= 0) {
        menuSelected = hit;  // garde la croix synchronisee avec le dernier choix tactile
        goTo(kMenuItems[items[hit]].target);
      }
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
      // Selectionne ce pas pour la croix (HAUT/BAS transpose sa note,
      // voir handleTeensyLine() -> NAV:) -- que le pas soit allume ou
      // eteint par ce meme toucher.
      const int8_t prevTrack = selectedSeqTrack;
      const int8_t prevStep = selectedSeqStep;
      selectedSeqTrack = static_cast<int8_t>(track);
      selectedSeqStep = static_cast<int8_t>(step);
      if (prevTrack >= 0 && prevStep >= 0 && (prevTrack != selectedSeqTrack || prevStep != selectedSeqStep)) {
        drawSeqCell(static_cast<uint8_t>(prevTrack), static_cast<uint8_t>(prevStep));  // efface l'ancien surlignage
      }

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
  } else if (currentScreen == Screen::Config) {
    if (hitTestCfgMinus(x, y)) {
      screensaverTimeoutSec = screensaverTimeoutSec >= kScreensaverStepSec ? screensaverTimeoutSec - kScreensaverStepSec : 0;
      drawConfigPage();
    } else if (hitTestCfgPlus(x, y)) {
      screensaverTimeoutSec = static_cast<uint16_t>(min<uint32_t>(screensaverTimeoutSec + kScreensaverStepSec, kScreensaverMaxSec));
      drawConfigPage();
    }
  } else if (currentScreen == Screen::Retro && !gbIsLoaded()) {
    if (gbRomCount > 0) {
      // Liste de ROM affichee : toucher une ligne la charge et demarre
      // le jeu (demande 2026-09-15, "une liste de rom pas uniquement
      // un jeux").
      const int8_t rowIndex = hitTestRomRow(x, y);
      if (rowIndex >= 0) {
        if (gbLoadRom(gbRomNames[rowIndex])) {
          drawRetroPage();
        }
      }
    } else {
      // Aucune ROM trouvee : toucher n'importe ou rescanne /games (utile
      // si la carte SD vient d'etre inseree).
      gbRomCount = gbScanRoms(gbRomNames);
      drawRetroPage();
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
  // Anti-rebond bruit I2C (voir "il apparait des que je touche un
  // bouton" -- 2026-09-15) : le bus tactile (GPIO40/41) peut capter du
  // bruit electrique d'un switch pas encore fixe proprement -- un seul
  // echantillon parasite peut avoir des coordonnees qui tombent par
  // hasard dans l'ecran (touchCount/BAD_COUNT ne filtre que les cas
  // grossiers, voir readTouches()). On n'accepte donc un DOWN qu'apres
  // 2 lectures consecutives actives -- un vrai doigt reste pose largement
  // plus longtemps qu'un tour de loop(), un blip electrique non.
  static bool pendingActive[2] = {false, false};
  const uint32_t now = millis();

  readTeensyStatus();

  TouchPoint touches[2];
  readTouches(touches);

  for (uint8_t slot = 0; slot < 2; ++slot) {
    const bool rawActive = touches[slot].active;
    const bool active = rawActive && (wasActive[slot] || pendingActive[slot]);
    pendingActive[slot] = rawActive;

    if (active && !wasActive[slot]) {
      noteActivity();
      if (screensaverActive) {
        screensaverExit();
      } else {
        handleTouchDown(slot, touches[slot].x, touches[slot].y);
      }
    } else if (!active && wasActive[slot]) {
      if (!screensaverActive) {
        handleTouchUp(slot);
      }
    }
    wasActive[slot] = active;
  }

  // Ecran de veille "Matrix" (voir docs, page CONFIGURATION) : s'active
  // apres screensaverTimeoutSec sans activite (0 = desactive). BUG
  // CORRIGE le 2026-09-15 ("quand je reveille l'ecran ca remet en mode
  // veille") : ce test utilisait le "now" fige en haut de loop(), mais
  // noteActivity() (appelee juste au-dessus sur un reveil) remet
  // lastActivityMs a une lecture PLUS RECENTE de millis() -- "now"
  // devenait alors plus petit que lastActivityMs, et la soustraction
  // non signee "now - lastActivityMs" debordait vers un nombre enorme,
  // redeclenchant la veille instantanement dans le MEME tour de boucle.
  // Fix : relire millis() ici, apres tout traitement d'activite.
  const uint32_t nowForIdle = millis();
  if (!screensaverActive && screensaverTimeoutSec > 0 &&
      (nowForIdle - lastActivityMs) >= static_cast<uint32_t>(screensaverTimeoutSec) * 1000UL) {
    screensaverEnter();
  }
  if (screensaverActive) {
    screensaverStep();
  }

  // Emulateur Game Boy (page JEUX, voir gb_emulator.h) : cadence a
  // ~59,7 images/s (periode Game Boy reelle) tant qu'une ROM est chargee
  // et que cette page est affichee -- pas de garantie que l'ESP32-S3
  // tienne cette cadence en pratique (pas encore mesure faute de ROM
  // disponible pour tester), c'est juste la cible, pas un benchmark.
  static uint32_t lastGbFrameMs = 0;
  if (currentScreen == Screen::Retro && gbIsLoaded() && !screensaverActive) {
    if (now - lastGbFrameMs >= 17) {
      lastGbFrameMs = now;
      gbRunFrame();
    }
  }

  if (now - lastHeartbeatMs >= 1000) {
    lastHeartbeatMs = now;
    Serial.println("DISPLAY:ALIVE:TICK");
  }
}
