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
enum class Screen : uint8_t { Menu, Controls, Audio, Sequencer, Engines, Retro, Config, Links, About, Patch, Song };
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
    {"PATCH", "filtre + ADSR + forme d'onde", Screen::Patch, MenuCat::Musique},
    {"SONG", "chainer les patterns", Screen::Song, MenuCat::Musique},
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
// 8 pistes / 16 pas : doit rester aligne avec kTrackCount/kStepCount
// cote Teensy (src_teensy/az2_audio/main.cpp).
constexpr uint8_t kSeqTrackCount = 8;
constexpr uint8_t kSeqStepCount = 16;
// Bord droit commun a la vue tracker (colonnes NOTE/INST/FX/VAL, voir
// plus bas) -- meme marge que le reste de l'appli (kMargin), plus de
// grille a aligner dessus depuis le retrait de l'ancienne vue ON/OFF
// (2026-09-16, voir plus bas).
constexpr int16_t kSeqRightEdge = kScreenSize - kMargin;

// 8 patterns (voir PATTERN:/SONGSET:/SONGLEN:/SONGMODE: dans
// AZ2_Protocol.h et patterns[] cote Teensy) -- demande 2026-09-16 ("il
// faut un tracker complet ... plus qu'assembler des patterns"). L'ESP32
// garde SES PROPRES copies des 8 grilles (pas de "dump" complet envoye
// par le Teensy au changement de pattern, trop volumineux -- 8x8x16
// pas) : seedees identiquement a seedDefaultNotes() cote Teensy au
// boot, puis tenues a jour par les memes echos NOTE:/STEP:/INST:/SFX:
// qu'avant, mais indexes par currentPattern (le pattern actuellement
// EDITE des deux cotes, voir PATTERN: plus bas).
constexpr uint8_t kPatternCount = 8;
uint8_t currentPattern = 0;

bool seqStepOn[kPatternCount][kSeqTrackCount][kSeqStepCount] = {};
// Note par pas (voir NOTE: dans AZ2_Protocol.h -- porte de MicroDexed-touch,
// demande le 2026-09-15 "prend le sequenceur du dexed touch"). Meme
// defauts que seedDefaultNotes() cote Teensy tant que le NOTE: echo n'est
// pas arrive.
uint8_t seqStepNote[kPatternCount][kSeqTrackCount][kSeqStepCount];
// Colonnes INST/FX/VAL du tracker (voir AZ2_TRACKER_ETUDE.md, INST:/SFX:
// dans AZ2_Protocol.h et handleInstCommand()/handleStepFxCommand() cote
// Teensy) -- 0xFF = patch par defaut de la piste, 0 = pas d'effet.
uint8_t seqStepPatch[kPatternCount][kSeqTrackCount][kSeqStepCount];
uint8_t seqStepFx[kPatternCount][kSeqTrackCount][kSeqStepCount] = {};
uint8_t seqStepFxVal[kPatternCount][kSeqTrackCount][kSeqStepCount] = {};

// Song : liste ordonnee de patterns a enchainer (voir songPatterns[]
// cote Teensy). Meme longueur max (kSongLength=16).
constexpr uint8_t kSongLength = 16;
uint8_t songPatterns[kSongLength] = {};
uint8_t songLen = 0;
bool songMode = false;
// Une couleur par effet (voir kPalette) -- reperage visuel rapide sur
// la grille ET la vue detail, demande le 2026-09-15 ("on met de la
// couleur, des effets"). kDim pour "aucun effet" (index 0). Doit rester
// alignee avec StepFx cote Teensy (None/Arp/Cut/Retrig).
const uint16_t kStepFxColors[] = {kDim, kPalette[1], kPalette[2], kPalette[3]};
uint8_t seqCurrentStep = 0;
bool seqPlaying = false;
float seqBpm = 120.0f;
uint8_t seqStepsPerBeat = 4;

// Piste/pas selectionnes dans la vue tracker (voir plus bas) -- la
// croix du Teensy (NAV:UP/DOWN) change la valeur de la colonne
// seqDetailCol du pas selectionne. Demarre a (0,0), pas (-1,-1) : la
// vue tracker est desormais TOUJOURS active (voir seqDetailMode), donc
// il faut toujours une piste/un pas valides des le boot.
int8_t selectedSeqTrack = 0;
int8_t selectedSeqStep = 0;

// Vue tracker (colonnes NOTE/INST/FX/VAL d'UNE piste, comme l'ecran
// phrase de LSDJ/M8 -- voir AZ2_TRACKER_ETUDE.md) -- devenue la SEULE
// vue de la page SEQUENCEUR le 2026-09-16 (retour utilisateur : "on a
// pas de tracker a la M8 LSDJ", la grille ON/OFF + double-tap pour
// voir le detail ne correspondait pas a l'experience tracker attendue).
// La variable reste (toujours true) pour ne pas casser tout le code de
// navigation/edition deja ecrit autour, mais rien ne la remet plus a
// false -- plus de grille a laquelle "revenir".
bool seqDetailMode = true;
int8_t seqDetailCol = 0;  // 0=NOTE 1=INST 2=FX 3=VAL, voir kSeqDetailColNames

// ---------------------------------------------------------------------
// Vue tracker (colonnes NOTE/INST/FX/VAL d'une piste, voir
// docs/AZ2_TRACKER_ETUDE.md) -- devenue la vue PRINCIPALE ET UNIQUE de
// la page SEQUENCEUR le 2026-09-16 (retour utilisateur : "on a pas de
// tracker a la M8 LSDJ" -- chez ces references, l'ecran affiche
// TOUJOURS la colonne d'une piste, pas une grille ON/OFF qu'il faut
// toucher deux fois pour voir le detail). L'ancienne grille 8 pistes
// (drawSeqGrid()/drawSeqCell()/drawSeqTransport()/drawSeqTempo()/
// drawSeqDivision()) est supprimee -- code mort une fois cette vue
// devenue la seule (recuperable dans l'historique git si jamais utile).
constexpr int16_t kTrkTrackRowY = 66;
constexpr int16_t kTrkTrackRowH = 22;
constexpr int16_t kDetailTop = kTrkTrackRowY + kTrkTrackRowH + 18;  // +18 = place pour l'en-tete de colonnes
constexpr int16_t kDetailRowH = 16;
constexpr int16_t kDetailRowGap = 2;
constexpr int16_t kDetailLeft = kMargin;
constexpr int16_t kDetailStepW = 26;
constexpr int16_t kDetailNoteW = 60;
constexpr int16_t kDetailInstW = 50;
constexpr int16_t kDetailFxW = 50;
constexpr int16_t kDetailValW = 44;

const char *const kNoteNames[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
const char *const kStepFxNames[] = {"---", "ARP", "CUT", "RET"};
constexpr uint8_t kStepFxCount = sizeof(kStepFxNames) / sizeof(kStepFxNames[0]);

void formatNoteName(uint8_t note, char *out, size_t outSize) {
  const int octave = static_cast<int>(note) / 12 - 1;  // MIDI 60 = C4, convention M8/LSDJ
  snprintf(out, outSize, "%s%d", kNoteNames[note % 12], octave);
}

int16_t detailColX(uint8_t col) {
  // 0=STEP (pas de colonne editable, juste le numero), 1=NOTE, 2=INST,
  // 3=FX, 4=VAL -- decalage de 1 par rapport a seqDetailCol (qui ne
  // compte que les colonnes editables).
  switch (col) {
    case 0: return kDetailLeft;
    case 1: return static_cast<int16_t>(kDetailLeft + kDetailStepW);
    case 2: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW);
    case 3: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW);
    default: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW);
  }
}

void drawDetailHeader() {
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  const int16_t y = static_cast<int16_t>(kDetailTop - 14);
  gfx->setCursor(static_cast<int16_t>(detailColX(0) + 2), y);
  gfx->print("PAS");
  gfx->setCursor(static_cast<int16_t>(detailColX(1) + 2), y);
  gfx->print("NOTE");
  gfx->setCursor(static_cast<int16_t>(detailColX(2) + 2), y);
  gfx->print("INST");
  gfx->setCursor(static_cast<int16_t>(detailColX(3) + 2), y);
  gfx->print("FX");
  gfx->setCursor(static_cast<int16_t>(detailColX(4) + 2), y);
  gfx->print("VAL");
}

void drawDetailRow(uint8_t step) {
  const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
  const int16_t y = static_cast<int16_t>(kDetailTop + step * (kDetailRowH + kDetailRowGap));
  const bool on = seqStepOn[currentPattern][track][step];
  const bool rowSelected = (step == selectedSeqStep);
  const bool playhead = (step == seqCurrentStep);
  const uint16_t accent = kPalette[track % kPaletteCount];

  gfx->fillRect(kDetailLeft, y, kSeqRightEdge - kDetailLeft, kDetailRowH,
                playhead ? dimColor(accent, 4) : RGB565_BLACK);

  char buf[8];
  gfx->setTextSize(1);

  // PAS
  gfx->setTextColor(kDim);
  snprintf(buf, sizeof(buf), "%02d", step);
  gfx->setCursor(static_cast<int16_t>(detailColX(0) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);

  // NOTE (colonne editable 0)
  const bool noteSel = rowSelected && seqDetailCol == 0;
  if (noteSel) {
    gfx->fillRect(detailColX(1), y, kDetailNoteW, kDetailRowH, accent);
  }
  gfx->setTextColor(noteSel ? RGB565_BLACK : (on ? RGB565_WHITE : kFaint));
  if (on) {
    formatNoteName(seqStepNote[currentPattern][track][step], buf, sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "---");
  }
  gfx->setCursor(static_cast<int16_t>(detailColX(1) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);

  // INST (colonne editable 1) -- 0xFF = defaut piste, affiche "--"
  const bool instSel = rowSelected && seqDetailCol == 1;
  if (instSel) {
    gfx->fillRect(detailColX(2), y, kDetailInstW, kDetailRowH, accent);
  }
  gfx->setTextColor(instSel ? RGB565_BLACK : (on ? RGB565_WHITE : kFaint));
  const uint8_t patch = seqStepPatch[currentPattern][track][step];
  if (patch == 0xFF) {
    snprintf(buf, sizeof(buf), "--");
  } else {
    snprintf(buf, sizeof(buf), "%02d", patch);
  }
  gfx->setCursor(static_cast<int16_t>(detailColX(2) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);

  // FX (colonne editable 2) -- une couleur par effet (kStepFxColors),
  // demande 2026-09-15 ("on met de la couleur, des effets").
  const uint8_t fxId = seqStepFx[currentPattern][track][step];
  const bool fxSel = rowSelected && seqDetailCol == 2;
  if (fxSel) {
    gfx->fillRect(detailColX(3), y, kDetailFxW, kDetailRowH, accent);
  } else if (on && fxId != 0) {
    // Fond legerement teinte de la couleur de l'effet, meme quand la
    // ligne n'est pas selectionnee -- repere visuel au premier coup
    // d'oeil, pas besoin d'ouvrir chaque pas pour voir ce qui a un FX.
    gfx->fillRect(detailColX(3), y, kDetailFxW, kDetailRowH, dimColor(kStepFxColors[fxId], 4));
  }
  gfx->setTextColor(fxSel ? RGB565_BLACK : (on ? kStepFxColors[fxId] : kFaint));
  gfx->setCursor(static_cast<int16_t>(detailColX(3) + 2), static_cast<int16_t>(y + 6));
  gfx->print(kStepFxNames[fxId]);

  // VAL (colonne editable 3) -- vide si pas d'effet actif
  const bool valSel = rowSelected && seqDetailCol == 3;
  if (valSel) {
    gfx->fillRect(detailColX(4), y, kDetailValW, kDetailRowH, accent);
  }
  gfx->setTextColor(valSel ? RGB565_BLACK : (on ? kStepFxColors[fxId] : kFaint));
  if (fxId == 0) {
    snprintf(buf, sizeof(buf), "--");
  } else {
    snprintf(buf, sizeof(buf), "%02X", seqStepFxVal[currentPattern][track][step]);
  }
  gfx->setCursor(static_cast<int16_t>(detailColX(4) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);
}

// Selecteur de piste ("< PISTE N >", meme motif que la page PATCH) --
// desormais AU-DESSUS de la colonne NOTE/INST/FX/VAL, toujours visible
// (plus besoin de toucher deux fois un pas pour changer de piste).
void drawTrkTrackRow() {
  gfx->fillRect(kMargin, kTrkTrackRowY, kScreenSize - 2 * kMargin, kTrkTrackRowH, RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(kPalette[selectedSeqTrack % kPaletteCount]);
  char buf[16];
  snprintf(buf, sizeof(buf), "< PISTE %d >", selectedSeqTrack);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 55), kTrkTrackRowY);
  gfx->print(buf);
}

bool hitTestTrkTrackPrev(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kTrkTrackRowY, kScreenSize / 2 - kMargin, kTrkTrackRowH);
}
bool hitTestTrkTrackNext(int16_t x, int16_t y) {
  return inBox(x, y, kScreenSize / 2, kTrkTrackRowY, kScreenSize / 2 - kMargin, kTrkTrackRowH);
}

// Transport + tempo + division, compactes sur UNE ligne (plus de place
// pour ca en 3 lignes empilees une fois les 16 pas affiches en
// permanence -- voir kDetailTop/kDetailRowH). PLAY/STOP a gauche, BPM
// au milieu (moitie gauche = -5, moitie droite = +5), DIVISION a
// droite (toucher = cran suivant).
constexpr int16_t kTrkControlsY = kDetailTop + kSeqStepCount * (kDetailRowH + kDetailRowGap) + 6;
constexpr int16_t kTrkControlsH = 34;
constexpr int16_t kTrkControlsW = kScreenSize - 2 * kMargin;
constexpr int16_t kTrkPlayW = kTrkControlsW / 3;
constexpr int16_t kTrkBpmX = kMargin + kTrkPlayW;
constexpr int16_t kTrkBpmW = kTrkControlsW / 3;
constexpr int16_t kTrkDivX = kTrkBpmX + kTrkBpmW;
constexpr int16_t kTrkDivW = kTrkControlsW - kTrkPlayW - kTrkBpmW;

void drawTrkControls() {
  gfx->fillRect(kMargin, kTrkControlsY, kTrkControlsW, kTrkControlsH, RGB565_BLACK);
  gfx->drawRect(kMargin, kTrkControlsY, kTrkPlayW, kTrkControlsH, kFaint);
  gfx->drawRect(kTrkBpmX, kTrkControlsY, kTrkBpmW, kTrkControlsH, kFaint);
  gfx->drawRect(kTrkDivX, kTrkControlsY, kTrkDivW, kTrkControlsH, kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(seqPlaying ? kPalette[1] : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kTrkControlsY + 8));
  gfx->print(seqPlaying ? "STOP" : "PLAY");

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kTrkBpmX + 4), static_cast<int16_t>(kTrkControlsY + 2));
  gfx->print("BPM -/+");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", static_cast<int>(seqBpm + 0.5f));
  gfx->setCursor(static_cast<int16_t>(kTrkBpmX + 4), static_cast<int16_t>(kTrkControlsY + 14));
  gfx->print(buf);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kTrkDivX + 4), static_cast<int16_t>(kTrkControlsY + 2));
  gfx->print("DIVISION");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kTrkDivX + 4), static_cast<int16_t>(kTrkControlsY + 14));
  gfx->print(az2::divisionLabel(seqStepsPerBeat));
}

bool hitTestTrkPlay(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kTrkControlsY, kTrkPlayW, kTrkControlsH);
}
// -1 = aucun, 0 = moitie gauche (-5 BPM), 1 = moitie droite (+5 BPM).
int8_t hitTestTrkBpm(int16_t x, int16_t y) {
  if (y < kTrkControlsY || y >= kTrkControlsY + kTrkControlsH) {
    return -1;
  }
  if (x >= kTrkBpmX && x < kTrkBpmX + kTrkBpmW / 2) {
    return 0;
  }
  if (x >= kTrkBpmX + kTrkBpmW / 2 && x < kTrkBpmX + kTrkBpmW) {
    return 1;
  }
  return -1;
}
bool hitTestTrkDiv(int16_t x, int16_t y) {
  return inBox(x, y, kTrkDivX, kTrkControlsY, kTrkDivW, kTrkControlsH);
}

// Panneau "patch actif" -- demande 2026-09-16 ("on a de la place, on
// affiche le patch actif et ses reglages du cote droit ... on met le
// patch en cours dans la piste selectionnee") : les colonnes NOTE/
// INST/FX/VAL ne remplissent pas toute la largeur (kSeqRightEdge), il
// reste ~194px a droite -- moteur + patch de la piste EDITEE
// (selectedSeqTrack), memes tableaux que la page MOTEURS (declares
// plus bas dans ce fichier, voir la declaration anticipee ci-dessous).
extern uint8_t trackEngine[kSeqTrackCount];
extern uint8_t trackPatch[kSeqTrackCount];
extern uint8_t trackCutoff[kSeqTrackCount];
extern uint8_t trackReso[kSeqTrackCount];
extern uint8_t trackAttack[kSeqTrackCount];
extern uint8_t trackDecay[kSeqTrackCount];
extern uint8_t trackSustain[kSeqTrackCount];
extern uint8_t trackRelease[kSeqTrackCount];
extern uint8_t trackAlgo[kSeqTrackCount];
extern uint8_t trackFeedback[kSeqTrackCount];

constexpr int16_t kTrkSideGap = 8;
constexpr int16_t kTrkSideX = kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW + kDetailValW +
                               kTrkSideGap;
constexpr int16_t kTrkSideW = kSeqRightEdge - kTrkSideX;

constexpr int16_t kTrkSideH = kSeqStepCount * (kDetailRowH + kDetailRowGap);
constexpr int16_t kTrkExpandH = 26;
constexpr int16_t kTrkExpandY = kDetailTop + kTrkSideH - kTrkExpandH;

void drawTrkSidePanel() {
  const uint8_t t = static_cast<uint8_t>(selectedSeqTrack);
  const uint16_t accent = kPalette[selectedSeqTrack % kPaletteCount];

  gfx->fillRect(kTrkSideX, kDetailTop, kTrkSideW, kTrkSideH, RGB565_BLACK);
  gfx->drawRect(kTrkSideX, kDetailTop, kTrkSideW, kTrkSideH, accent);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 4));
  gfx->print("PATCH ACTIF");

  gfx->setTextSize(2);
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 18));
  gfx->print(az2::engineName(trackEngine[t]));

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 40));
  gfx->print(az2::enginePatchName(trackEngine[t], trackPatch[t]));

  // Reglages de base (voir patchParamRef()) -- demande 2026-09-16 ("on
  // affiche les reglages [de patch] du cote droit"). Controle complet
  // (encore) uniquement sur la page PATCH, voir le bouton AGRANDIR.
  char buf[20];
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 58));
  snprintf(buf, sizeof(buf), "CUTOFF %3d", trackCutoff[t]);
  gfx->print(buf);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 70));
  snprintf(buf, sizeof(buf), "RESO   %3d", trackReso[t]);
  gfx->print(buf);
  // ADSR generique n'a aucun effet sur Dexed (voir patchRowActive()) --
  // montrer ALGO/FEEDBACK a la place ici aussi, sinon le panneau
  // afficherait un reglage qui ne sert a rien pour ce moteur.
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 84));
  if (trackEngine[t] == az2::kEngineDexed) {
    gfx->print("ALGO/FDBK");
    gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 96));
    snprintf(buf, sizeof(buf), "%d / %d", trackAlgo[t] + 1, trackFeedback[t]);
  } else {
    gfx->print("ADSR");
    gfx->setCursor(static_cast<int16_t>(kTrkSideX + 6), static_cast<int16_t>(kDetailTop + 96));
    snprintf(buf, sizeof(buf), "%d/%d/%d/%d", trackAttack[t], trackDecay[t], trackSustain[t], trackRelease[t]);
  }
  gfx->print(buf);

  // Bouton AGRANDIR -- ouvre la page PATCH complete (filtre/ADSR
  // editables + oscilloscope) pour CETTE piste (demande : "un bouton
  // pour agrandir et avoir le controle total du patch").
  gfx->fillRect(static_cast<int16_t>(kTrkSideX + 1), kTrkExpandY, static_cast<int16_t>(kTrkSideW - 2),
                kTrkExpandH, accent);
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_BLACK);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 12), static_cast<int16_t>(kTrkExpandY + 8));
  gfx->print("AGRANDIR >");
}

bool hitTestTrkExpand(int16_t x, int16_t y) {
  return inBox(x, y, kTrkSideX, kTrkExpandY, kTrkSideW, kTrkExpandH);
}

void drawSeqDetailPage() {
  char title[16];
  snprintf(title, sizeof(title), "PATTERN %d", currentPattern);
  drawSubHeader(title, kPalette[0]);
  drawTrkTrackRow();
  drawDetailHeader();
  for (uint8_t s = 0; s < kSeqStepCount; ++s) {
    drawDetailRow(s);
  }
  drawTrkSidePanel();
  drawTrkControls();
}

// Zone tactile sur le titre de l'en-tete (juste apres la fleche retour,
// voir hitBack()) -- toucher cycle le pattern EDITE (voir currentPattern,
// PATTERN: dans AZ2_Protocol.h).
bool hitTestPatternHeader(int16_t x, int16_t y) {
  return inBox(x, y, 90, 0, 200, 50);
}

void drawSequencerPage();  // definie plus bas -- seul appelant de switchToPattern()

void switchToPattern(uint8_t p) {
  currentPattern = static_cast<uint8_t>(p % kPatternCount);
  char msg[12];
  snprintf(msg, sizeof(msg), "PATTERN:%d", currentPattern);
  sendToTeensy(msg);
  drawSequencerPage();
}

int8_t hitTestDetailRow(int16_t x, int16_t y) {
  if (x < kDetailLeft || x > kSeqRightEdge) {
    return -1;
  }
  for (uint8_t s = 0; s < kSeqStepCount; ++s) {
    const int16_t rowY = static_cast<int16_t>(kDetailTop + s * (kDetailRowH + kDetailRowGap));
    if (y >= rowY && y < rowY + kDetailRowH) {
      return static_cast<int8_t>(s);
    }
  }
  return -1;
}

// Page SEQUENCEUR = toujours la vue tracker (voir drawSeqDetailPage()
// et le commentaire sur seqDetailMode plus haut) -- plus de grille
// ON/OFF separee.
void drawSequencerPage() {
  drawSeqDetailPage();
}

// ---------------------------------------------------------------------
// Page MOTEURS -- moteur + patch par piste, demandee le 2026-09-14 ("les
// moteurs audio ne sont pas selectionnables ni reglables ... faut faire
// un truc propre"). Une ligne par piste : toucher la moitie gauche
// change de moteur (ENGINE:), la moitie droite change de patch (PATCH:)
// -- le Teensy applique et renvoie confirmation, voir handleTeensyLine().
// ---------------------------------------------------------------------
// Meme motif que seedDefaultNotes() cote Teensy (Dexed/Dexed/EPiano/
// Braids repete sur les pistes 4-7) -- juste le defaut affiche avant
// que announceHello() ne confirme l'etat reel a la connexion.
uint8_t trackEngine[kSeqTrackCount] = {az2::kEngineDexed, az2::kEngineDexed, az2::kEngineEPiano, az2::kEngineBraids,
                                        az2::kEngineDexed, az2::kEngineDexed, az2::kEngineEPiano, az2::kEngineBraids};
uint8_t trackPatch[kSeqTrackCount] = {0, 0, 0, 0, 0, 0, 0, 0};

// Navigation croix sur la page MOTEURS -- demande 2026-09-16 ("dans la
// fenetre des moteurs j'ai pas le controle joystick pour choisir et
// regler les moteurs"). HAUT/BAS deplacent la piste selectionnee,
// GAUCHE/DROITE changent la valeur de la colonne actuellement au focus
// (moteur ou patch), BTN:A bascule le focus entre les deux colonnes --
// meme esprit que seqDetailCol sur le sequenceur (GAUCHE/DROITE = quelle
// colonne, HAUT/BAS = valeur), adapte ici car HAUT/BAS sert a choisir la
// piste (8 lignes, pas de "curseur de pas" a deplacer separement).
int8_t selectedEngineTrack = 0;
bool engineColPatch = false;

constexpr int16_t kEngRowTop = 90;
// Retreci (etait 70/10) pour que 8 pistes tiennent -- voir le meme fix
// que kSeqCellH/kSeqGapY plus haut (kSeqTrackCount 4 -> 8, 2026-09-15).
constexpr int16_t kEngRowH = 40;
constexpr int16_t kEngRowGap = 4;
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
  const bool selected = (track == selectedEngineTrack);

  gfx->fillRect(kEngLeft, y, kEngWidth, kEngRowH - kEngRowGap, RGB565_BLACK);
  gfx->drawRect(kEngLeft, y, kEngWidth, kEngRowH - kEngRowGap, accent);
  if (selected) {
    // Piste selectionnee (croix) : contour double pour rester visible
    // meme quand la couleur d'accent est deja vive.
    gfx->drawRect(static_cast<int16_t>(kEngLeft + 1), static_cast<int16_t>(y + 1), kEngWidth - 2,
                  kEngRowH - kEngRowGap - 2, RGB565_WHITE);
  }
  gfx->drawFastVLine(kEngSplitX, y, kEngRowH - kEngRowGap, kFaint);

  char title[12];
  snprintf(title, sizeof(title), "P%d", track);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kEngLeft + 6), static_cast<int16_t>(y + 2));
  gfx->print(title);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kEngLeft + 6), static_cast<int16_t>(y + 16));
  gfx->print(az2::engineName(trackEngine[track]));
  // Colonne au focus (croix GAUCHE/DROITE + BTN:A pour basculer) :
  // soulignee, uniquement sur la piste selectionnee.
  if (selected && !engineColPatch) {
    gfx->drawFastHLine(static_cast<int16_t>(kEngLeft + 6), static_cast<int16_t>(y + 32),
                        static_cast<int16_t>(kEngSplitX - kEngLeft - 12), RGB565_WHITE);
  }

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kEngSplitX + 6), static_cast<int16_t>(y + 2));
  gfx->print("PATCH");
  gfx->setTextSize(2);
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(kEngSplitX + 6), static_cast<int16_t>(y + 16));
  gfx->print(az2::enginePatchName(trackEngine[track], trackPatch[track]));
  if (selected && engineColPatch) {
    gfx->drawFastHLine(static_cast<int16_t>(kEngSplitX + 6), static_cast<int16_t>(y + 32),
                        static_cast<int16_t>(kEngLeft + kEngWidth - kEngSplitX - 12), RGB565_WHITE);
  }
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
// Page PATCH -- filtre + ADSR + oscilloscope en direct, demande le
// 2026-09-15 ("on fait en sorte que nos moteurs audio soient completement
// configurables dans une fenetre ou on voit l'onde du son jouer evoluer
// en modifiant le patch ... il faut tout un filtre"). Colonne de gauche =
// coupure/resonance du filtre (FILT:, voir trackFilter[] cote Teensy),
// ADSR (ENV:, s'applique au moteur ANALOG -- voir handleEnvCommand()
// cote Teensy pour le pourquoi). Le tracer d'onde recoit les paquets
// SCOPE (voir kScopePacketMagic dans AZ2_Protocol.h) de la piste
// actuellement affichee -- envoie SCOPE:piste en entrant/changeant de
// piste, SCOPE:OFF en quittant (voir goTo()).
// ---------------------------------------------------------------------
int8_t patchTrack = 0;
// cutoff, resonance, attaque, chute, maintien, relachement -- tous 0-127,
// memes defauts "neutres" que cote Teensy (grand ouvert / ADSR rapide).
// PAR PISTE (pas juste un etat transitoire de la page PATCH) -- demande
// 2026-09-16 ("on affiche les reglages [de patch] du cote droit [du
// sequenceur]") : il faut connaitre les vraies valeurs de N'IMPORTE
// QUELLE piste pour les montrer dans le panneau lateral du sequenceur
// (voir drawTrkSidePanel()), pas seulement celle actuellement ouverte
// sur la page PATCH (patchTrack). Mises a jour par les echos FILT:/
// ENV: du Teensy (voir handleTeensyLine()), pas seulement par les
// boutons -/+ de cette page.
uint8_t trackCutoff[kSeqTrackCount];
uint8_t trackReso[kSeqTrackCount];
uint8_t trackAttack[kSeqTrackCount];
uint8_t trackDecay[kSeqTrackCount];
uint8_t trackSustain[kSeqTrackCount];
uint8_t trackRelease[kSeqTrackCount];
// Reglages propres au moteur DEXED (DXP:, voir handleDexedParamCommand()
// cote Teensy) -- demande 2026-09-16 ("il faut des reglages, on a pas de
// reglages dans la fenetre dexed du tracker") : l'ADSR generique
// (ENV:) n'est PAS ecoutee par Dexed (sa propre EG interne au patch DX7
// la remplace), une piste Dexed n'avait donc aucun reglage utile sur la
// page PATCH avant ca. Algorithme stocke brut 0-31 (affiche +1, cf.
// convention DX7 1-32), feedback 0-7.
uint8_t trackAlgo[kSeqTrackCount] = {};
uint8_t trackFeedback[kSeqTrackCount] = {};
const char *const kPatchLabels[6] = {"CUTOFF", "RESONANCE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
const char *const kPatchLabelsDexed[6] = {"CUTOFF", "RESONANCE", "ALGO (DX7)", "FEEDBACK", "--", "--"};

// Lignes 2-5 de la page PATCH dependent du moteur de la piste : ADSR
// generique pour tout le monde SAUF Dexed (rows 4-5 sans effet chez lui,
// voir le commentaire au-dessus de trackAlgo[]).
bool patchRowActive(uint8_t track, uint8_t row) {
  return !(trackEngine[track] == az2::kEngineDexed && row >= 4);
}

const char *patchRowLabel(uint8_t track, uint8_t row) {
  return (trackEngine[track] == az2::kEngineDexed) ? kPatchLabelsDexed[row] : kPatchLabels[row];
}

uint8_t patchRowMax(uint8_t track, uint8_t row) {
  if (trackEngine[track] == az2::kEngineDexed) {
    if (row == 2) return 31;  // ALGO : 32 algorithmes DX7 (0-31)
    if (row == 3) return 7;   // FEEDBACK : 0-7
  }
  return 127;
}

// Reference directe dans le tableau du bon parametre pour une piste
// donnee -- evite de dupliquer un switch partout ou un reglage est
// lu/ecrit (page PATCH ET panneau lateral du sequenceur). Rows 4-5 sur
// une piste Dexed renvoient un stockage inerte (trackSustain/Release,
// jamais envoyes -- voir patchRowActive(), la page ne dessine ni
// n'autorise le toucher dessus).
uint8_t &patchParamRef(uint8_t track, uint8_t row) {
  if (trackEngine[track] == az2::kEngineDexed) {
    switch (row) {
      case 0: return trackCutoff[track];
      case 1: return trackReso[track];
      case 2: return trackAlgo[track];
      case 3: return trackFeedback[track];
      case 4: return trackSustain[track];
      default: return trackRelease[track];
    }
  }
  switch (row) {
    case 0: return trackCutoff[track];
    case 1: return trackReso[track];
    case 2: return trackAttack[track];
    case 3: return trackDecay[track];
    case 4: return trackSustain[track];
    default: return trackRelease[track];
  }
}

uint8_t scopeSamples[az2::kScopeSamplesPerPacket] = {};
bool scopeHasData = false;

constexpr int16_t kPatchTrackRowY = 66;
constexpr int16_t kPatchScopeTop = 96;
constexpr int16_t kPatchScopeH = 90;
constexpr int16_t kPatchRowTop = kPatchScopeTop + kPatchScopeH + 14;
constexpr int16_t kPatchRowH = 34;
constexpr int16_t kPatchBtnW = 36;

void drawPatchTrackRow() {
  gfx->fillRect(kMargin, kPatchTrackRowY, kScreenSize - 2 * kMargin, 22, RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(kPalette[patchTrack % kPaletteCount]);
  char buf[16];
  snprintf(buf, sizeof(buf), "< PISTE %d >", patchTrack);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 55), kPatchTrackRowY);
  gfx->print(buf);
}

// N'efface/redessine QUE l'interieur (pas le cadre, voir drawPatchPage()
// qui le dessine une seule fois en entrant sur la page) -- appelee a
// chaque paquet SCOPE recu (~15/s, voir kScopeSendIntervalMs cote
// Teensy), redessiner le cadre a chaque fois donnait un effet de
// scintillement genant ("la fenetre patch ... elle scintille un peu
// trop").
void drawPatchScope() {
  gfx->fillRect(static_cast<int16_t>(kMargin + 1), static_cast<int16_t>(kPatchScopeTop + 1),
                static_cast<int16_t>(kScreenSize - 2 * kMargin - 2), static_cast<int16_t>(kPatchScopeH - 2),
                RGB565_BLACK);
  if (!scopeHasData) {
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kPatchScopeTop + kPatchScopeH / 2 - 4));
    gfx->print("(silence -- joue une note sur cette piste)");
    return;
  }
  const int16_t w = static_cast<int16_t>(kScreenSize - 2 * kMargin);
  int16_t prevX = kMargin, prevY = static_cast<int16_t>(kPatchScopeTop + kPatchScopeH / 2);
  const uint16_t traceColor = kPalette[patchTrack % kPaletteCount];
  for (uint8_t i = 0; i < az2::kScopeSamplesPerPacket; ++i) {
    const int16_t x = static_cast<int16_t>(kMargin + (i * w) / (az2::kScopeSamplesPerPacket - 1));
    const int16_t y = static_cast<int16_t>(kPatchScopeTop + 1 +
                                            ((255 - scopeSamples[i]) * (kPatchScopeH - 2)) / 255);
    if (i > 0) {
      gfx->drawLine(prevX, prevY, x, y, traceColor);
    }
    prevX = x;
    prevY = y;
  }
}

void patchRowRect(uint8_t i, int16_t &y) {
  y = static_cast<int16_t>(kPatchRowTop + i * kPatchRowH);
}

void drawPatchRow(uint8_t i) {
  int16_t y;
  patchRowRect(i, y);
  const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
  const uint8_t track = static_cast<uint8_t>(patchTrack);

  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, rowH, RGB565_BLACK);

  if (!patchRowActive(track, i)) {
    // Piste DEXED : lignes 4-5 sans effet (ADSR generique non ecoutee,
    // voir patchRowActive()) -- grisees plutot que des +/- qui ne
    // feraient rien.
    gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, rowH, kFaint);
    gfx->setTextSize(1);
    gfx->setTextColor(kFaint);
    gfx->setCursor(static_cast<int16_t>(kMargin + 6), static_cast<int16_t>(y + 4));
    gfx->print("(sans effet sur ce moteur)");
    return;
  }

  const int16_t minusX = static_cast<int16_t>(kScreenSize - kMargin - 2 * kPatchBtnW - 4);
  const int16_t plusX = static_cast<int16_t>(kScreenSize - kMargin - kPatchBtnW);

  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, rowH, kFaint);
  gfx->drawRect(minusX, y, kPatchBtnW, rowH, kFaint);
  gfx->drawRect(plusX, y, kPatchBtnW, rowH, kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kMargin + 6), static_cast<int16_t>(y + 4));
  gfx->print(patchRowLabel(track, i));

  // ALGO (Dexed) affiche 1-32 (convention DX7), stocke 0-31 en interne.
  const bool isDexedAlgo = (trackEngine[track] == az2::kEngineDexed && i == 2);
  const uint8_t raw = patchParamRef(track, i);
  char buf[6];
  snprintf(buf, sizeof(buf), "%3d", isDexedAlgo ? raw + 1 : raw);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 130), static_cast<int16_t>(y + 2));
  gfx->print(buf);

  gfx->setCursor(static_cast<int16_t>(minusX + 12), static_cast<int16_t>(y + 4));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 12), static_cast<int16_t>(y + 4));
  gfx->print('+');
}

void sendPatchFilt() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  char msg[24];
  snprintf(msg, sizeof(msg), "FILT:%d:%d:%d", patchTrack, trackCutoff[t], trackReso[t]);
  sendToTeensy(msg);
}

void sendPatchEnv() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  char msg[24];
  snprintf(msg, sizeof(msg), "ENV:%d:%d:%d:%d:%d", patchTrack, trackAttack[t], trackDecay[t], trackSustain[t],
           trackRelease[t]);
  sendToTeensy(msg);
}

// index : 0=algorithme, 1=feedback -- voir handleDexedParamCommand()
// cote Teensy.
void sendPatchDxp(uint8_t index) {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  char msg[24];
  snprintf(msg, sizeof(msg), "DXP:%d:%d:%d", t, index, index == 0 ? trackAlgo[t] : trackFeedback[t]);
  sendToTeensy(msg);
}

// Sauvegarde/chargement de patch (demande 2026-09-16 : "on doit
// pouvoir sauvegarder les patch") -- un "patch" ici = moteur + patch
// integre + filtre + ADSR de la piste affichee (patchTrack), ecrit sur
// la carte SD de l'ESP32 (deja utilisee pour les ROM GB) dans
// /patches/N.txt, format simple "moteur,patch,cutoff,reso,a,d,s,r".
// 8 emplacements numerotes, comme les patterns.
constexpr uint8_t kPatchSlotCount = 8;
uint8_t patchSlot = 0;
constexpr int16_t kPatchSlotY = kPatchRowTop + 6 * kPatchRowH + 6;
constexpr int16_t kPatchSlotH = 32;
constexpr int16_t kPatchSlotBtnW = (kScreenSize - 2 * kMargin) / 3;

void drawPatchSlotRow() {
  const int16_t saveX = static_cast<int16_t>(kMargin + kPatchSlotBtnW);
  const int16_t loadX = static_cast<int16_t>(kMargin + 2 * kPatchSlotBtnW);
  gfx->fillRect(kMargin, kPatchSlotY, kScreenSize - 2 * kMargin, kPatchSlotH, RGB565_BLACK);
  gfx->drawRect(kMargin, kPatchSlotY, kPatchSlotBtnW, kPatchSlotH, kFaint);
  gfx->drawRect(saveX, kPatchSlotY, kPatchSlotBtnW, kPatchSlotH, kFaint);
  gfx->drawRect(loadX, kPatchSlotY, kPatchSlotBtnW, kPatchSlotH, kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char buf[12];
  snprintf(buf, sizeof(buf), "SLOT %d", patchSlot);
  gfx->setCursor(static_cast<int16_t>(kMargin + 6), static_cast<int16_t>(kPatchSlotY + 8));
  gfx->print(buf);

  gfx->setTextColor(kPalette[1 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(saveX + 14), static_cast<int16_t>(kPatchSlotY + 8));
  gfx->print("SAVE");

  gfx->setTextColor(kPalette[2 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(loadX + 14), static_cast<int16_t>(kPatchSlotY + 8));
  gfx->print("LOAD");
}

bool hitTestPatchSlotNum(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kPatchSlotY, kPatchSlotBtnW, kPatchSlotH);
}
bool hitTestPatchSlotSave(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kMargin + kPatchSlotBtnW), kPatchSlotY, kPatchSlotBtnW, kPatchSlotH);
}
bool hitTestPatchSlotLoad(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kMargin + 2 * kPatchSlotBtnW), kPatchSlotY, kPatchSlotBtnW, kPatchSlotH);
}

// Ecrit tel quel (pas d'ajout) -- SD.remove() d'abord pour eviter tout
// risque d'ancien contenu residuel si FILE_WRITE ouvrait en ajout sur
// cette version de la lib (pas verifie, prudence peu couteuse ici).
void savePatchSlot(uint8_t slot) {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  SD.mkdir("/patches");
  char path[24];
  snprintf(path, sizeof(path), "/patches/%d.txt", slot);
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.print("PATCH_SAVE_ERROR:");
    Serial.println(path);
    return;
  }
  // 10 champs depuis l'ajout DXP (algo/feedback Dexed, sinon perdus au
  // rechargement -- PATCH: recharge tout le voice data DX7 depuis la
  // banque, voir loadDexedPatch() cote Teensy). Fichiers a 8 champs
  // (avant DXP) restent lisibles, voir loadPatchSlot().
  f.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", trackEngine[t], trackPatch[t], trackCutoff[t], trackReso[t],
            trackAttack[t], trackDecay[t], trackSustain[t], trackRelease[t], trackAlgo[t], trackFeedback[t]);
  f.close();
  Serial.print("PATCH_SAVED:");
  Serial.println(path);
}

// Charge le slot et APPLIQUE au Teensy (ENGINE:/PATCH:/FILT:/ENV:) --
// les tableaux locaux (trackEngine[] etc.) sont mis a jour par les
// echos normaux du Teensy une fois les commandes traitees, pas ici.
void loadPatchSlot(uint8_t slot) {
  char path[24];
  snprintf(path, sizeof(path), "/patches/%d.txt", slot);
  File f = SD.open(path);
  if (!f) {
    Serial.print("PATCH_LOAD_EMPTY:");
    Serial.println(path);
    return;
  }
  const String line = f.readStringUntil('\n');
  f.close();

  int vals[10] = {};
  int idx = 0;
  int start = 0;
  for (int i = 0; i <= line.length() && idx < 10; ++i) {
    if (i == line.length() || line.charAt(i) == ',') {
      vals[idx++] = line.substring(start, i).toInt();
      start = i + 1;
    }
  }
  if (idx < 8) {
    Serial.print("PATCH_LOAD_BADFILE:");
    Serial.println(path);
    return;
  }
  // Fichier a 8 champs (avant l'ajout DXP) : algo/feedback pas stockes,
  // laisse la banque de patch Dexed les fixer (comportement d'avant).
  const bool hasDxp = (idx >= 10);

  const uint8_t t = static_cast<uint8_t>(patchTrack);
  char msg[24];
  snprintf(msg, sizeof(msg), "ENGINE:%d:%d", t, vals[0]);
  sendToTeensy(msg);
  snprintf(msg, sizeof(msg), "PATCH:%d:%d", t, vals[1]);
  sendToTeensy(msg);
  snprintf(msg, sizeof(msg), "FILT:%d:%d:%d", t, vals[2], vals[3]);
  sendToTeensy(msg);
  snprintf(msg, sizeof(msg), "ENV:%d:%d:%d:%d:%d", t, vals[4], vals[5], vals[6], vals[7]);
  sendToTeensy(msg);
  if (hasDxp) {
    // Envoyes APRES PATCH: expres -- PATCH: recharge tout le voice data
    // DX7 depuis la banque (voir loadDexedPatch()), qui ecraserait ces
    // deux octets s'ils partaient avant.
    snprintf(msg, sizeof(msg), "DXP:%d:0:%d", t, vals[8]);
    sendToTeensy(msg);
    snprintf(msg, sizeof(msg), "DXP:%d:1:%d", t, vals[9]);
    sendToTeensy(msg);
  }
  Serial.print("PATCH_LOADED:");
  Serial.println(path);
}

void drawPatchPage() {
  drawSubHeader("PATCH", kPalette[4]);
  drawPatchTrackRow();
  // Cadre du tracer dessine UNE fois ici -- drawPatchScope() (appelee a
  // chaque paquet SCOPE recu) ne touche plus que l'interieur, voir son
  // commentaire.
  gfx->drawRect(kMargin, kPatchScopeTop, kScreenSize - 2 * kMargin, kPatchScopeH, kFaint);
  drawPatchScope();
  for (uint8_t i = 0; i < 6; ++i) {
    drawPatchRow(i);
  }
  drawPatchSlotRow();
}

bool hitTestPatchTrackPrev(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kPatchTrackRowY, kScreenSize / 2 - kMargin, 22);
}
bool hitTestPatchTrackNext(int16_t x, int16_t y) {
  return inBox(x, y, kScreenSize / 2, kPatchTrackRowY, kScreenSize / 2 - kMargin, 22);
}

// Renvoie -1 (aucun), sinon l'index de ligne (0-5) ; isPlusSide indique -/+.
int8_t hitTestPatchRow(int16_t x, int16_t y, bool &isPlusSide) {
  for (uint8_t i = 0; i < 6; ++i) {
    int16_t rowY;
    patchRowRect(i, rowY);
    const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
    const int16_t minusX = static_cast<int16_t>(kScreenSize - kMargin - 2 * kPatchBtnW - 4);
    const int16_t plusX = static_cast<int16_t>(kScreenSize - kMargin - kPatchBtnW);
    if (inBox(x, y, minusX, rowY, kPatchBtnW, rowH)) {
      isPlusSide = false;
      return static_cast<int8_t>(i);
    }
    if (inBox(x, y, plusX, rowY, kPatchBtnW, rowH)) {
      isPlusSide = true;
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------
// Page SONG -- chainage de patterns, demande le 2026-09-16 ("c'est
// plus un sequenceur qui peut nous permettre d'assembler des patterns,
// mais il faut un tracker complet"). Modele Polyend (le plus simple des
// 3 references etudiees, voir AZ2_TRACKER_ETUDE.md) : chaque case de
// song = un pattern ENTIER (8 pistes ensemble). MODE bascule boucle
// simple (comportement d'origine) / song ; LONGUEUR (0-16) ; grille de
// 16 cases, touche pour cycler le pattern assigne (0-7).
// ---------------------------------------------------------------------
constexpr int16_t kSongModeRowY = 90;
constexpr int16_t kSongRowH = 38;
constexpr int16_t kSongBtnW = 70;
constexpr int16_t kSongLenRowY = kSongModeRowY + kSongRowH + 8;
constexpr int16_t kSongGridTop = kSongLenRowY + kSongRowH + 12;
constexpr uint8_t kSongCols = 4;
constexpr int16_t kSongSlotGap = 6;
constexpr int16_t kSongSlotW = (kScreenSize - 2 * kMargin - (kSongCols - 1) * kSongSlotGap) / kSongCols;
constexpr int16_t kSongSlotH = 56;

void drawSongModeRow() {
  gfx->fillRect(kMargin, kSongModeRowY, kScreenSize - 2 * kMargin, kSongRowH, RGB565_BLACK);
  gfx->drawRect(kMargin, kSongModeRowY, kScreenSize - 2 * kMargin, kSongRowH, kFaint);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kSongModeRowY + 3));
  gfx->print("MODE (toucher pour changer)");
  gfx->setTextSize(2);
  gfx->setTextColor(songMode ? kPalette[1] : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kSongModeRowY + 16));
  gfx->print(songMode ? "SONG" : "BOUCLE SIMPLE");
}

void drawSongLenRow() {
  const int16_t minusX = kMargin;
  const int16_t plusX = static_cast<int16_t>(kScreenSize - kMargin - kSongBtnW);
  gfx->fillRect(kMargin, kSongLenRowY, kScreenSize - 2 * kMargin, kSongRowH, RGB565_BLACK);
  gfx->drawRect(minusX, kSongLenRowY, kSongBtnW, kSongRowH, kFaint);
  gfx->drawRect(plusX, kSongLenRowY, kSongBtnW, kSongRowH, kFaint);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(minusX + 24), static_cast<int16_t>(kSongLenRowY + 6));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 24), static_cast<int16_t>(kSongLenRowY + 6));
  gfx->print('+');
  char buf[20];
  snprintf(buf, sizeof(buf), "LONGUEUR: %d", songLen);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 55), static_cast<int16_t>(kSongLenRowY + 10));
  gfx->print(buf);
}

void songSlotRect(uint8_t i, int16_t &x, int16_t &y) {
  const uint8_t col = static_cast<uint8_t>(i % kSongCols);
  const uint8_t row = static_cast<uint8_t>(i / kSongCols);
  x = static_cast<int16_t>(kMargin + col * (kSongSlotW + kSongSlotGap));
  y = static_cast<int16_t>(kSongGridTop + row * (kSongSlotH + kSongSlotGap));
}

void drawSongSlot(uint8_t i) {
  int16_t x, y;
  songSlotRect(i, x, y);
  const bool active = i < songLen;
  const uint8_t patt = songPatterns[i];

  gfx->fillRect(x, y, kSongSlotW, kSongSlotH, active ? kPalette[patt % kPaletteCount] : RGB565_BLACK);
  gfx->drawRect(x, y, kSongSlotW, kSongSlotH, kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(active ? RGB565_BLACK : kDim);
  char lbl[6];
  snprintf(lbl, sizeof(lbl), "%02d", i);
  gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3));
  gfx->print(lbl);

  gfx->setTextSize(3);
  gfx->setTextColor(active ? RGB565_BLACK : kFaint);
  char buf[4];
  if (active) {
    snprintf(buf, sizeof(buf), "%d", patt);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  gfx->setCursor(static_cast<int16_t>(x + kSongSlotW / 2 - 10), static_cast<int16_t>(y + kSongSlotH / 2 - 12));
  gfx->print(buf);
}

void drawSongPage() {
  drawSubHeader("SONG", kPalette[3]);
  drawSongModeRow();
  drawSongLenRow();
  for (uint8_t i = 0; i < kSongLength; ++i) {
    drawSongSlot(i);
  }
}

bool hitTestSongMode(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kSongModeRowY, kScreenSize - 2 * kMargin, kSongRowH);
}
bool hitTestSongLenMinus(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kSongLenRowY, kSongBtnW, kSongRowH);
}
bool hitTestSongLenPlus(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kSongBtnW), kSongLenRowY, kSongBtnW, kSongRowH);
}
int8_t hitTestSongSlot(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < kSongLength; ++i) {
    int16_t sx, sy;
    songSlotRect(i, sx, sy);
    if (inBox(x, y, sx, sy, kSongSlotW, kSongSlotH)) {
      return static_cast<int8_t>(i);
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
// Pagination (demande 2026-09-17, "met en plus des trucs cool ... genre
// 20 30") -- avant, toutes les ROM trouvees etaient dessinees a la
// suite sans defilement : au-dela de ~9-10 lignes, le reste tombait
// hors ecran (480px de haut) et devenait injoignable au tactile. 8
// lignes visibles (meme convention que les 8 pistes ailleurs dans
// l'appli) + une rangee de pagination en bas si besoin.
constexpr uint8_t kRomVisibleRows = 8;
constexpr int16_t kRomListH = kRomVisibleRows * kRomRowH;
constexpr int16_t kRomPageY = kRomRowTop + kRomListH + 6;
constexpr int16_t kRomPageH = 32;
constexpr int16_t kRomPageBtnW = 60;
uint8_t gbRomScroll = 0;  // index (absolu) de la 1ere ROM visible

void romRowRect(uint8_t visibleRow, int16_t &y) {
  y = static_cast<int16_t>(kRomRowTop + visibleRow * kRomRowH);
}

// index : absolu dans gbRomNames[], pas relatif a la page -- ne dessine
// rien s'il tombe hors de la fenetre visible actuelle (gbRomScroll).
void drawRomRow(uint8_t index) {
  if (index < gbRomScroll || index >= gbRomScroll + kRomVisibleRows) {
    return;
  }
  int16_t y;
  romRowRect(static_cast<uint8_t>(index - gbRomScroll), y);
  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6, RGB565_BLACK);
  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6, kPalette[index % kPaletteCount]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 10), static_cast<int16_t>(y + 6));
  gfx->print(gbRomNames[index]);
}

// Renvoie l'index ABSOLU (pas relatif a la page) de la ROM touchee.
int8_t hitTestRomRow(int16_t x, int16_t y) {
  if (x < kMargin || x > kScreenSize - kMargin) {
    return -1;
  }
  const uint8_t visibleCount = static_cast<uint8_t>(min<int>(kRomVisibleRows, gbRomCount - gbRomScroll));
  for (uint8_t row = 0; row < visibleCount; ++row) {
    int16_t rowY;
    romRowRect(row, rowY);
    if (y >= rowY && y < rowY + (kRomRowH - 6)) {
      return static_cast<int8_t>(gbRomScroll + row);
    }
  }
  return -1;
}

void drawRomPageRow() {
  if (gbRomCount <= kRomVisibleRows) {
    return;  // tout tient sur une page, pas besoin de pagination
  }
  const int16_t prevX = kMargin;
  const int16_t nextX = static_cast<int16_t>(kScreenSize - kMargin - kRomPageBtnW);
  gfx->fillRect(kMargin, kRomPageY, kScreenSize - 2 * kMargin, kRomPageH, RGB565_BLACK);
  gfx->drawRect(prevX, kRomPageY, kRomPageBtnW, kRomPageH, kFaint);
  gfx->drawRect(nextX, kRomPageY, kRomPageBtnW, kRomPageH, kFaint);
  gfx->setTextSize(2);
  gfx->setTextColor(gbRomScroll > 0 ? RGB565_WHITE : kFaint);
  gfx->setCursor(static_cast<int16_t>(prevX + 14), static_cast<int16_t>(kRomPageY + 8));
  gfx->print('<');
  gfx->setTextColor(gbRomScroll + kRomVisibleRows < gbRomCount ? RGB565_WHITE : kFaint);
  gfx->setCursor(static_cast<int16_t>(nextX + 14), static_cast<int16_t>(kRomPageY + 8));
  gfx->print('>');

  char buf[16];
  const uint8_t lastShown = static_cast<uint8_t>(min<int>(gbRomScroll + kRomVisibleRows, gbRomCount));
  snprintf(buf, sizeof(buf), "%d-%d / %d", gbRomScroll + 1, lastShown, gbRomCount);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 30), static_cast<int16_t>(kRomPageY + 12));
  gfx->print(buf);
}

bool hitTestRomPagePrev(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kRomPageY, kRomPageBtnW, kRomPageH);
}
bool hitTestRomPageNext(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kRomPageBtnW), kRomPageY, kRomPageBtnW, kRomPageH);
}

// Sampler GB (demande 2026-09-15 "sampler la Game Boy", precisee
// 2026-09-17 "REC/STOP, capter les sons de l'emulateur") -- encodeur 0
// (Volume, bouton integre) demarre/arrete l'enregistrement cote Teensy
// (REC:START/REC:STOP, voir handleRecCommand() dans src_teensy/
// az2_audio/main.cpp -- ecrit un .wav sur LA SD DU TEENSY, pas celle-ci).
// gbRecActive suit l'etat CONFIRME par l'echo REC:STARTED:/REC:STOPPED:
// (voir handleTeensyLine()), jamais mis a jour de facon optimiste --
// meme convention que FILT:/ENV:, important ici car le Teensy peut
// aussi arreter tout seul (garde-fou 30s, voir kGbRecMaxSamples).
// Phase 1 : demarrer/arreter + indicateur seulement. PAS FAIT (voir
// AZ2_FEUILLE_DE_ROUTE.md) : vue d'onde en direct, decoupage tactile,
// decoupage automatique, clavier de nom personnalise.
bool gbRecActive = false;

void drawGbRecIndicator() {
  if (!gbIsLoaded()) {
    return;
  }
  constexpr int16_t kRecX = kMargin;
  constexpr int16_t kRecY = 16;
  gfx->fillRect(kRecX, kRecY, 90, 12, RGB565_BLACK);
  if (gbRecActive) {
    // Pas de glyphe rond (police GFX par defaut non verifiee pour ca) --
    // "REC" seul en rouge suffit a etre visible/comprehensible.
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_RED);
    gfx->setCursor(kRecX, kRecY);
    gfx->print("REC");
  }
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
    // Rappel discret : C quitte la partie (voir le commentaire pres de
    // "il faut un truc pour sortir de l'emulateur", 2026-09-17).
    gfx->setCursor(static_cast<int16_t>(kScreenSize - kMargin - 48), 4);
    gfx->print("C:MENU");
    drawGbRecIndicator();
    return;
  }

  if (gbRomCount > 0) {
    drawSubHeader("JEUX - choisis une ROM", kPalette[2]);
    if (gbRomScroll > 0 && gbRomScroll >= gbRomCount) {
      gbRomScroll = 0;  // securite si la liste a change depuis (rescan)
    }
    // Efface toute la zone de liste avant de redessiner -- la derniere
    // page peut avoir moins de lignes que kRomVisibleRows, sinon
    // d'anciennes lignes resteraient affichees en dessous.
    gfx->fillRect(kMargin, kRomRowTop, kScreenSize - 2 * kMargin, kRomListH, RGB565_BLACK);
    const uint8_t lastVisible = static_cast<uint8_t>(min<int>(gbRomScroll + kRomVisibleRows, gbRomCount));
    for (uint8_t i = gbRomScroll; i < lastVisible; ++i) {
      drawRomRow(i);
    }
    drawRomPageRow();
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
      "Controle: croix + 4 boutons + 3 encodeurs rotatifs",
      "",
      "Manette Game Boy (page JEUX) :",
      "  Croix = D-pad, A/B = A/B",
      "  Encodeur 1 (bouton) = SELECT",
      "  Encodeur 2 (bouton) = START",
      "  C = quitter la partie (D libre)",
      "",
      "Build: screen_esp, 2026-09-17",
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
constexpr int16_t kScaleRowY = kCfgRowY + kCfgRowH + 40;

// Gammes (demande 2026-09-15, "on ajoute les gammes accord") --
// verrouillage a la saisie : quand on transpose une note (croix HAUT/
// BAS sur un pas, grille ou vue detail du sequenceur), on saute
// directement a la prochaine note DANS LA GAMME au lieu d'un simple
// demi-ton. CHROMATIQUE (toutes les notes, index 0) = comportement
// d'origine, donc rien ne change tant qu'on n'a pas touche ce reglage.
// Racine fixee a C pour cette premiere version (pas de transposition de
// tonalite editable) -- juste le TYPE de gamme.
const uint8_t kScaleChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const uint8_t kScaleMajor[] = {0, 2, 4, 5, 7, 9, 11};
const uint8_t kScaleMinor[] = {0, 2, 3, 5, 7, 8, 10};
const uint8_t kScaleMajorPenta[] = {0, 2, 4, 7, 9};
const uint8_t kScaleMinorPenta[] = {0, 3, 5, 7, 10};
const uint8_t *const kScales[] = {kScaleChromatic, kScaleMajor, kScaleMinor, kScaleMajorPenta, kScaleMinorPenta};
const uint8_t kScaleLens[] = {12, 7, 7, 5, 5};
const char *const kScaleNames[] = {"CHROMATIQUE (C)", "MAJEUR (C)", "MINEUR (C)", "PENTA MAJ (C)", "PENTA MIN (C)"};
constexpr uint8_t kScaleCount = sizeof(kScaleNames) / sizeof(kScaleNames[0]);
uint8_t currentScaleIndex = 0;

bool noteInScale(uint8_t note) {
  const uint8_t pc = static_cast<uint8_t>(note % 12);
  const uint8_t *scale = kScales[currentScaleIndex];
  for (uint8_t i = 0; i < kScaleLens[currentScaleIndex]; ++i) {
    if (scale[i] == pc) {
      return true;
    }
  }
  return false;
}

// Avance/recule (dir = +1/-1) jusqu'a la prochaine note DANS LA GAMME,
// au maximum un tour chromatique complet (12 demi-tons) -- si rien
// trouve avant d'atteindre une borne (0/127), garde la note de depart.
uint8_t nextNoteInScale(uint8_t note, int8_t dir) {
  int n = static_cast<int>(note);
  for (uint8_t i = 0; i < 12; ++i) {
    n += dir;
    if (n < 0 || n > 127) {
      break;
    }
    if (noteInScale(static_cast<uint8_t>(n))) {
      return static_cast<uint8_t>(n);
    }
  }
  return note;
}

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

  // Gamme (voir noteInScale()/nextNoteInScale() -- demande 2026-09-15,
  // "on ajoute les gammes accord"). CHROMATIQUE (index 0) = comportement
  // d'origine (aucune restriction), donc rien ne change tant qu'on n'y
  // touche pas.
  gfx->fillRect(kMargin, kScaleRowY, kScreenSize - 2 * kMargin, kCfgRowH, RGB565_BLACK);
  gfx->drawRect(minusX, kScaleRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->drawRect(plusX, kScaleRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(minusX + 20), static_cast<int16_t>(kScaleRowY + 10));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 20), static_cast<int16_t>(kScaleRowY + 10));
  gfx->print('+');
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 70), static_cast<int16_t>(kScaleRowY + 15));
  gfx->print(kScaleNames[currentScaleIndex]);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, static_cast<int16_t>(kScaleRowY - 20));
  gfx->print("GAMME (croix/pas du sequenceur en tonalite de C)");
}

bool hitTestCfgMinus(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kCfgRowY, kCfgBtnW, kCfgRowH);
}
bool hitTestCfgPlus(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kCfgBtnW), kCfgRowY, kCfgBtnW, kCfgRowH);
}

bool hitTestScaleMinus(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kScaleRowY, kCfgBtnW, kCfgRowH);
}
bool hitTestScalePlus(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kCfgBtnW), kScaleRowY, kCfgBtnW, kCfgRowH);
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
    case Screen::Patch: drawPatchPage(); return;
    case Screen::Song: drawSongPage(); return;
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
    gbRomScroll = 0;
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

  // Page PATCH : l'oscilloscope (voir kScopePacketMagic) ne doit tourner
  // QUE quand cette page est affichee -- cout CPU nul cote Teensy sinon
  // (voir scopeQueue.end() dans handleScopeCommand()).
  if (s == Screen::Patch) {
    char msg[12];
    snprintf(msg, sizeof(msg), "SCOPE:%d", patchTrack);
    sendToTeensy(msg);
    scopeHasData = false;
  } else if (currentScreen == Screen::Patch) {
    sendToTeensy("SCOPE:OFF");
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
        // Page MOTEURS : voir le commentaire de selectedEngineTrack plus
        // haut. HAUT/BAS changent la piste, GAUCHE/DROITE la valeur de la
        // colonne au focus (moteur ou patch).
        if (pressed && currentScreen == Screen::Engines) {
          if (index == 0 || index == 1) {
            const int8_t previous = selectedEngineTrack;
            selectedEngineTrack = static_cast<int8_t>((selectedEngineTrack + (index == 1 ? 1 : kSeqTrackCount - 1)) %
                                                        kSeqTrackCount);
            drawEngRow(static_cast<uint8_t>(previous));
            drawEngRow(static_cast<uint8_t>(selectedEngineTrack));
          } else if (index == 2 || index == 3) {
            const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
            const int dir = (index == 3) ? 1 : -1;
            char msg[16];
            if (!engineColPatch) {
              const uint8_t nextEngine = static_cast<uint8_t>((trackEngine[t] + dir + az2::kEngineCount) % az2::kEngineCount);
              snprintf(msg, sizeof(msg), "ENGINE:%d:%d", t, nextEngine);
            } else {
              const uint8_t count = az2::enginePatchCount(trackEngine[t]);
              const uint8_t nextPatch = static_cast<uint8_t>((trackPatch[t] + dir + count) % count);
              snprintf(msg, sizeof(msg), "PATCH:%d:%d", t, nextPatch);
            }
            sendToTeensy(msg);
          }
        }
        // Sur la page SEQUENCEUR (vue tracker, voir seqDetailMode plus
        // haut), la croix edite le pas selectionne -- demande le
        // 2026-09-14 ("prend le sequenceur du dexed touch"), etendue le
        // 2026-09-15 ("un tracker 8 pistes") : GAUCHE/DROITE choisissent
        // la colonne (NOTE/INST/FX/VAL), HAUT/BAS modifient sa valeur.
        if (pressed && currentScreen == Screen::Sequencer && selectedSeqTrack >= 0 && selectedSeqStep >= 0) {
          const uint8_t t = static_cast<uint8_t>(selectedSeqTrack);
          const uint8_t s = static_cast<uint8_t>(selectedSeqStep);
          {
            if (index == 2 || index == 3) {
              const int8_t prevCol = seqDetailCol;
              seqDetailCol = static_cast<int8_t>((seqDetailCol + (index == 3 ? 1 : 3)) % 4);
              if (seqDetailCol != prevCol) {
                drawDetailRow(s);
              }
            } else if (index == 0 || index == 1) {
              const int dir = (index == 0) ? 1 : -1;
              char msg[24];
              switch (seqDetailCol) {
                case 0: {
                  const uint8_t newNote = nextNoteInScale(seqStepNote[currentPattern][t][s], static_cast<int8_t>(dir));
                  snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", t, s, newNote);
                  sendToTeensy(msg);
                  break;
                }
                case 1: {
                  const uint8_t count = az2::enginePatchCount(trackEngine[t]);
                  const int base = (seqStepPatch[currentPattern][t][s] == 0xFF) ? trackPatch[t] : seqStepPatch[currentPattern][t][s];
                  const int newPatch = (base + dir + count) % count;
                  snprintf(msg, sizeof(msg), "INST:%d:%d:%d", t, s, newPatch);
                  sendToTeensy(msg);
                  break;
                }
                case 2: {
                  const int newFx = (seqStepFx[currentPattern][t][s] + dir + kStepFxCount) % kStepFxCount;
                  snprintf(msg, sizeof(msg), "SFX:%d:%d:%d:%d", t, s, newFx, seqStepFxVal[currentPattern][t][s]);
                  sendToTeensy(msg);
                  break;
                }
                default: {
                  const int newVal = constrain(static_cast<int>(seqStepFxVal[currentPattern][t][s]) + dir, 0, 255);
                  snprintf(msg, sizeof(msg), "SFX:%d:%d:%d:%d", t, s, seqStepFx[currentPattern][t][s], newVal);
                  sendToTeensy(msg);
                  break;
                }
              }
            }
          }
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
      // Sortir d'une partie : demande 2026-09-17 ("il faut un truc pour
      // sortir de l'emulateur cote code") -- B est deja pris par le jeu
      // (voir plus bas) donc pas utilisable comme "retour menu" ici. La
      // Game Boy d'origine n'a pas de boutons L/R : C et D restent donc
      // libres meme en pleine partie (voir AZ2_TODO_PICO.md, "gachette"
      // jamais assignee) -- C sert desormais a quitter proprement
      // (goTo(Screen::Menu) sauvegarde la RAM cartouche via gbUnload()
      // avant de liberer la ROM, meme chemin que changer de page).
      if (pressed && letter == 'C' && inGbGame) {
        goTo(Screen::Menu);
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
      // Page MOTEURS : A bascule le focus croix entre la colonne MOTEUR
      // et la colonne PATCH (voir engineColPatch plus haut).
      if (pressed && letter == 'A' && currentScreen == Screen::Engines) {
        engineColPatch = !engineColPatch;
        drawEngRow(static_cast<uint8_t>(selectedEngineTrack));
      }
      // Dans une sous-liste du menu : B revient a la grille de
      // categories (pas besoin de ressortir de Screen::Menu).
      if (pressed && letter == 'B' && currentScreen == Screen::Menu && menuCategory >= 0) {
        menuCategory = -1;
        menuSelected = 0;
        drawMenu();
      }
      if (pressed && letter == 'B' && currentScreen != Screen::Menu && !inGbGame) {
        // Partout ailleurs (sauf en pleine partie GB, ou B est le
        // bouton B du jeu) : B revient au menu -- convention manette
        // classique, meme demande ("il faut que ca serve dans les
        // menus").
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
    // pour un futur role de gachette). Encodeur 0 (Volume) declenche
    // desormais l'enregistrement de sample (voir drawGbRecIndicator()
    // et REC:START/REC:STOP, 2026-09-17).
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
          } else if (index == 0 && pressed) {
            // Sampler (voir drawGbRecIndicator()) : un appui = bascule
            // demarrer/arreter -- l'etat visuel n'est mis a jour qu'a
            // l'echo REC:STARTED:/REC:STOPPED: du Teensy, pas ici.
            sendToTeensy(gbRecActive ? "REC:STOP" : "REC:START");
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
        seqStepNote[currentPattern][track][step] = note;
        if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawDetailRow(step);
        }
      }
    }
  } else if (line.startsWith("INST:")) {
    // Colonne INST du tracker (voir AZ2_TRACKER_ETUDE.md, handleInstCommand()
    // cote Teensy) -- 255 = patch par defaut de la piste.
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t patch = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && step < kSeqStepCount) {
        seqStepPatch[currentPattern][track][step] = patch;
        if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawDetailRow(step);
        }
      }
    }
  } else if (line.startsWith("SFX:")) {
    // Colonne FX+VAL du tracker (voir handleStepFxCommand() cote
    // Teensy).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    const int i4 = line.indexOf(':', i3 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0 && i4 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t fx = static_cast<uint8_t>(line.substring(i3 + 1, i4).toInt());
      const uint8_t val = static_cast<uint8_t>(line.substring(i4 + 1).toInt());
      if (track < kSeqTrackCount && step < kSeqStepCount && fx < kStepFxCount) {
        seqStepFx[currentPattern][track][step] = fx;
        seqStepFxVal[currentPattern][track][step] = val;
        if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawDetailRow(step);
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
        seqStepOn[currentPattern][track][step] = on;
        if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawDetailRow(step);
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
          drawDetailRow(oldStep);
          drawDetailRow(seqCurrentStep);
        }
      }
    }
  } else if (line.startsWith("STATUS:TEENSY_AUDIO:")) {
    const bool nowPlaying = line.endsWith("PLAYING");
    if (nowPlaying != seqPlaying) {
      seqPlaying = nowPlaying;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawTrkControls();
      }
    }
  } else if (line.startsWith("BPM:")) {
    const float value = line.substring(4).toFloat();
    if (value > 0.0f) {
      seqBpm = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawTrkControls();
      }
    }
  } else if (line.startsWith("DIV:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(4).toInt());
    if (value > 0) {
      seqStepsPerBeat = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawTrkControls();
      }
    }
  } else if (line.startsWith("PATTERN:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(8).toInt());
    if (value < kPatternCount) {
      currentPattern = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawSequencerPage();
      }
    }
  } else if (line.startsWith("SONGMODE:")) {
    songMode = line.substring(9).toInt() != 0;
    if (currentScreen == Screen::Song && !screensaverActive) {
      drawSongPage();
    }
  } else if (line.startsWith("SONGLEN:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(8).toInt());
    if (value <= kSongLength) {
      songLen = value;
      if (currentScreen == Screen::Song && !screensaverActive) {
        drawSongPage();
      }
    }
  } else if (line.startsWith("SONGSET:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t pos = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t pattern = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (pos < kSongLength && pattern < kPatternCount) {
        songPatterns[pos] = pattern;
        if (pos + 1 > songLen) {
          songLen = static_cast<uint8_t>(pos + 1);
        }
        if (currentScreen == Screen::Song && !screensaverActive) {
          drawSongSlot(pos);
        }
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
        } else if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          // Les lignes 2-5 changent de sens selon le moteur (ADSR vs
          // ALGO/FEEDBACK Dexed, voir patchRowLabel()) -- tout redessiner.
          drawPatchPage();
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
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
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        }
      }
    }
  } else if (line.startsWith("FILT:")) {
    // Echo du filtre par piste (voir handleFiltCommand() cote Teensy) --
    // tient trackCutoff[]/trackReso[] a jour pour la page PATCH ET le
    // panneau lateral du sequenceur (voir drawTrkSidePanel()).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t cutoff = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t reso = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount) {
        trackCutoff[track] = cutoff;
        trackReso[track] = reso;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchRow(0);
          drawPatchRow(1);
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        }
      }
    }
  } else if (line.startsWith("ENV:")) {
    // Echo de l'ADSR par piste (voir handleEnvCommand() cote Teensy).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    const int i4 = line.indexOf(':', i3 + 1);
    const int i5 = line.indexOf(':', i4 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0 && i4 >= 0 && i5 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t a = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t d = static_cast<uint8_t>(line.substring(i3 + 1, i4).toInt());
      const uint8_t s = static_cast<uint8_t>(line.substring(i4 + 1, i5).toInt());
      const uint8_t r = static_cast<uint8_t>(line.substring(i5 + 1).toInt());
      if (track < kSeqTrackCount) {
        trackAttack[track] = a;
        trackDecay[track] = d;
        trackSustain[track] = s;
        trackRelease[track] = r;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          for (uint8_t row = 2; row < 6; ++row) {
            drawPatchRow(row);
          }
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        }
      }
    }
  } else if (line.startsWith("DXP:")) {
    // Echo des reglages Dexed (voir handleDexedParamCommand() cote
    // Teensy et le commentaire de trackAlgo[] plus haut) -- arrive apres
    // un +/- sur la page PATCH ET apres tout changement de patch/moteur
    // (announceDexedParams() cote Teensy), donc garde l'affichage a jour
    // meme quand l'algo/feedback changent parce que le PATCH a change,
    // pas juste par ce reglage-la.
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t index = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t value = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount) {
        if (index == 0) {
          trackAlgo[track] = value;
        } else if (index == 1) {
          trackFeedback[track] = value;
        }
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchRow(2);
          drawPatchRow(3);
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        }
      }
    }
  } else if (line.startsWith("REC:STARTED:")) {
    gbRecActive = true;
    if (currentScreen == Screen::Retro && !screensaverActive) {
      drawGbRecIndicator();
    }
  } else if (line.startsWith("REC:STOPPED:") || line.startsWith("REC:ERROR:")) {
    // ERROR arrive quand gbRecStart() echoue (SD absente/pleine) -- deja
    // false cote ESP32 dans ce cas (jamais passe a true), le mettre a
    // jour quand meme ne fait pas de mal et couvre STOPPED normalement.
    gbRecActive = false;
    if (currentScreen == Screen::Retro && !screensaverActive) {
      drawGbRecIndicator();
    }
  }

  if (currentScreen == Screen::Links) {
    drawLinksPage();
  }
  if (currentScreen != Screen::Controls && currentScreen != Screen::Links) {
    drawLinkStatus();
  }
}

// Etat de reception binaire (paquets oscilloscope, voir
// kScopePacketMagic) -- mele au flux texte habituel sur Serial1, meme
// principe que le son GB cote Teensy (voir AudioRxState dans
// src_teensy/az2_audio/main.cpp) mais dans l'autre sens.
struct ScopeRxState {
  bool inPacket = false;
  bool haveLen = false;
  uint8_t len = 0;
  uint8_t pos = 0;
  uint8_t buf[255];
};
ScopeRxState scopeRx;

// Paquet binaire recu du Teensy (voir kScopePacketMagic dans
// AZ2_Protocol.h) -- met a jour le tracer d'onde si la page PATCH est
// affichee.
void handleScopePacket(const uint8_t *data, uint8_t len) {
  const uint8_t n = min(len, az2::kScopeSamplesPerPacket);
  for (uint8_t i = 0; i < n; ++i) {
    scopeSamples[i] = data[i];
  }
  scopeHasData = true;
  if (currentScreen == Screen::Patch && !screensaverActive) {
    drawPatchScope();
  }
}

void readTeensyStatus() {
  while (Serial1.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(Serial1.read());

    if (scopeRx.inPacket) {
      if (!scopeRx.haveLen) {
        scopeRx.len = b;
        scopeRx.pos = 0;
        scopeRx.haveLen = true;
        if (scopeRx.len == 0) {
          scopeRx.inPacket = false;
        }
        continue;
      }
      scopeRx.buf[scopeRx.pos++] = b;
      if (scopeRx.pos >= scopeRx.len) {
        handleScopePacket(scopeRx.buf, scopeRx.len);
        scopeRx.inPacket = false;
      }
      continue;
    }
    if (b == az2::kScopePacketMagic) {
      scopeRx.inPacket = true;
      scopeRx.haveLen = false;
      continue;
    }

    const char c = static_cast<char>(b);
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

  // Memes valeurs par defaut que seedDefaultNotes() cote Teensy (les 8
  // patterns, pas seulement celui affiche au boot -- voir currentPattern)
  // -- corrige des le premier NOTE:/HELLO recu si jamais desynchronise.
  static const uint8_t kDefaultNotes[kSeqTrackCount] = {48, 55, 60, 64, 60, 67, 72, 76};
  for (uint8_t p = 0; p < kPatternCount; ++p) {
    for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
      for (uint8_t s = 0; s < kSeqStepCount; ++s) {
        seqStepNote[p][t][s] = kDefaultNotes[t];
        seqStepPatch[p][t][s] = 0xFF;  // meme defaut que cote Teensy (voir seedDefaultNotes())
      }
    }
  }
  // Memes defauts "neutres" que cote Teensy pour le filtre/ADSR de
  // chaque piste (grand ouvert / ADSR rapide) -- voir trackFilter[]/
  // trackAnalogEnv[] dans setup() cote Teensy.
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    trackCutoff[t] = 127;
    trackReso[t] = 0;
    trackAttack[t] = 3;
    trackDecay[t] = 20;
    trackSustain[t] = 89;
    trackRelease[t] = 38;
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

  if (currentScreen == Screen::Sequencer && hitTestPatternHeader(x, y)) {
    // Titre de l'en-tete ("SEQUENCEUR - PAT N" / "PN PISTE X - DETAIL")
    // -- toucher cycle le pattern EDITE (voir currentPattern, demande
    // 2026-09-16 "il faut un tracker complet").
    switchToPattern(static_cast<uint8_t>(currentPattern + 1));
  } else if (currentScreen != Screen::Menu && hitBack(x, y)) {
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
    // Vue tracker unique (voir seqDetailMode plus haut) : selecteur de
    // piste, transport/tempo/division compactes, 16 lignes NOTE/INST/
    // FX/VAL -- demande 2026-09-16 ("on a pas de tracker a la M8
    // LSDJ").
    if (hitTestTrkTrackPrev(x, y) || hitTestTrkTrackNext(x, y)) {
      selectedSeqTrack = static_cast<int8_t>(
          (selectedSeqTrack + (hitTestTrkTrackNext(x, y) ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
      selectedSeqStep = 0;
      drawSeqDetailPage();
    } else if (hitTestTrkExpand(x, y)) {
      // "AGRANDIR" -- ouvre la page PATCH complete pour la piste
      // actuellement affichee (demande : "un bouton pour agrandir et
      // avoir le controle total du patch des parametres").
      patchTrack = selectedSeqTrack;
      scopeHasData = false;
      goTo(Screen::Patch);
    } else if (hitTestTrkPlay(x, y)) {
      sendToTeensy(seqPlaying ? az2::kStop : az2::kPlay);
    } else if (hitTestTrkBpm(x, y) >= 0) {
      // +/- 5 BPM par toucher (moitie gauche/droite de la case BPM),
      // borne comme cote Teensy (30-300) -- pas d'affichage optimiste,
      // on attend l'echo BPM: confirme (voir handleTeensyLine()).
      const int8_t bpmHit = hitTestTrkBpm(x, y);
      const int newBpm = constrain(static_cast<int>(seqBpm + 0.5f) + (bpmHit == 0 ? -5 : 5), 30, 300);
      char msg[16];
      snprintf(msg, sizeof(msg), "BPM:%d", newBpm);
      sendToTeensy(msg);
    } else if (hitTestTrkDiv(x, y)) {
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
    } else {
      const int8_t hitStep = hitTestDetailRow(x, y);
      if (hitStep >= 0) {
        const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
        if (hitStep == selectedSeqStep) {
          // 2e toucher sur la ligne deja selectionnee -> bascule ON/OFF.
          const bool newState = !seqStepOn[currentPattern][track][hitStep];
          seqStepOn[currentPattern][track][hitStep] = newState;
          char msg[20];
          snprintf(msg, sizeof(msg), "STEP:%d:%d:%d", track, hitStep, newState ? 1 : 0);
          sendToTeensy(msg);
        }
        selectedSeqStep = hitStep;
        drawSeqDetailPage();  // 16 lignes seulement -- redessiner tout est bon marche
      }
    }
  } else if (currentScreen == Screen::Engines) {
    bool isPatchSide = false;
    const int8_t track = hitTestEngRow(x, y, isPatchSide);
    if (track >= 0) {
      // Un toucher deplace aussi le curseur croix sur la ligne/colonne
      // touchee -- sinon tactile et croix restent desynchronises (on
      // pouvait toucher la piste 5 puis un appui croix modifiait encore
      // la piste 0). Meme reflexe que le tracker (tap = select + edit).
      const int8_t previousTrack = selectedEngineTrack;
      const bool previousCol = engineColPatch;
      selectedEngineTrack = track;
      engineColPatch = isPatchSide;
      if (previousTrack != track) {
        drawEngRow(static_cast<uint8_t>(previousTrack));
      } else if (previousCol != engineColPatch) {
        drawEngRow(static_cast<uint8_t>(track));
      }

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
  } else if (currentScreen == Screen::Patch) {
    if (hitTestPatchTrackPrev(x, y) || hitTestPatchTrackNext(x, y)) {
      patchTrack = static_cast<int8_t>((patchTrack + (hitTestPatchTrackNext(x, y) ? 1 : kSeqTrackCount - 1)) %
                                        kSeqTrackCount);
      scopeHasData = false;
      char msg[12];
      snprintf(msg, sizeof(msg), "SCOPE:%d", patchTrack);
      sendToTeensy(msg);
      drawPatchPage();
    } else {
      bool isPlusSide = false;
      const int8_t row = hitTestPatchRow(x, y, isPlusSide);
      const uint8_t t = static_cast<uint8_t>(patchTrack);
      if (row >= 0 && patchRowActive(t, static_cast<uint8_t>(row))) {
        const int delta = isPlusSide ? 1 : -1;
        uint8_t &param = patchParamRef(t, static_cast<uint8_t>(row));
        param = static_cast<uint8_t>(constrain(static_cast<int>(param) + delta, 0, static_cast<int>(patchRowMax(t, static_cast<uint8_t>(row)))));
        drawPatchRow(static_cast<uint8_t>(row));
        if (row < 2) {
          sendPatchFilt();
        } else if (trackEngine[t] == az2::kEngineDexed) {
          sendPatchDxp(static_cast<uint8_t>(row - 2));  // row 2->index 0 (algo), row 3->index 1 (feedback)
        } else {
          sendPatchEnv();
        }
      } else if (hitTestPatchSlotNum(x, y)) {
        patchSlot = static_cast<uint8_t>((patchSlot + 1) % kPatchSlotCount);
        drawPatchSlotRow();
      } else if (hitTestPatchSlotSave(x, y)) {
        savePatchSlot(patchSlot);
      } else if (hitTestPatchSlotLoad(x, y)) {
        loadPatchSlot(patchSlot);
      }
    }
  } else if (currentScreen == Screen::Song) {
    if (hitTestSongMode(x, y)) {
      songMode = !songMode;
      char msg[12];
      snprintf(msg, sizeof(msg), "SONGMODE:%d", songMode ? 1 : 0);
      sendToTeensy(msg);
      drawSongModeRow();
    } else if (hitTestSongLenMinus(x, y)) {
      songLen = songLen > 0 ? static_cast<uint8_t>(songLen - 1) : 0;
      char msg[16];
      snprintf(msg, sizeof(msg), "SONGLEN:%d", songLen);
      sendToTeensy(msg);
      drawSongLenRow();
      drawSongSlot(songLen);  // la case qui vient de "sortir" de la longueur active doit s'assombrir
    } else if (hitTestSongLenPlus(x, y)) {
      const uint8_t previous = songLen;
      songLen = static_cast<uint8_t>(min<uint32_t>(songLen + 1, kSongLength));
      char msg[16];
      snprintf(msg, sizeof(msg), "SONGLEN:%d", songLen);
      sendToTeensy(msg);
      drawSongLenRow();
      if (songLen != previous) {
        drawSongSlot(previous);
      }
    } else {
      const int8_t slot = hitTestSongSlot(x, y);
      if (slot >= 0) {
        songPatterns[slot] = static_cast<uint8_t>((songPatterns[slot] + 1) % kPatternCount);
        if (slot >= songLen) {
          songLen = static_cast<uint8_t>(slot + 1);
          drawSongLenRow();
        }
        char msg[16];
        snprintf(msg, sizeof(msg), "SONGSET:%d:%d", slot, songPatterns[slot]);
        sendToTeensy(msg);
        drawSongSlot(static_cast<uint8_t>(slot));
      }
    }
  } else if (currentScreen == Screen::Config) {
    if (hitTestCfgMinus(x, y)) {
      screensaverTimeoutSec = screensaverTimeoutSec >= kScreensaverStepSec ? screensaverTimeoutSec - kScreensaverStepSec : 0;
      drawConfigPage();
    } else if (hitTestCfgPlus(x, y)) {
      screensaverTimeoutSec = static_cast<uint16_t>(min<uint32_t>(screensaverTimeoutSec + kScreensaverStepSec, kScreensaverMaxSec));
      drawConfigPage();
    } else if (hitTestScaleMinus(x, y)) {
      currentScaleIndex = static_cast<uint8_t>((currentScaleIndex + kScaleCount - 1) % kScaleCount);
      drawConfigPage();
    } else if (hitTestScalePlus(x, y)) {
      currentScaleIndex = static_cast<uint8_t>((currentScaleIndex + 1) % kScaleCount);
      drawConfigPage();
    }
  } else if (currentScreen == Screen::Retro && !gbIsLoaded()) {
    if (gbRomCount > 0) {
      // Liste de ROM affichee : toucher une ligne la charge et demarre
      // le jeu (demande 2026-09-15, "une liste de rom pas uniquement
      // un jeux"). Pagination (2026-09-17) : fleches +/- une page.
      if (hitTestRomPagePrev(x, y) && gbRomScroll > 0) {
        gbRomScroll = static_cast<uint8_t>(gbRomScroll >= kRomVisibleRows ? gbRomScroll - kRomVisibleRows : 0);
        drawRetroPage();
      } else if (hitTestRomPageNext(x, y) && gbRomScroll + kRomVisibleRows < gbRomCount) {
        gbRomScroll = static_cast<uint8_t>(gbRomScroll + kRomVisibleRows);
        drawRetroPage();
      } else {
        const int8_t rowIndex = hitTestRomRow(x, y);
        if (rowIndex >= 0) {
          if (gbLoadRom(gbRomNames[rowIndex])) {
            drawRetroPage();
          }
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
