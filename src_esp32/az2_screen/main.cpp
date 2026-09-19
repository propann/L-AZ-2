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
enum class Screen : uint8_t { Menu, Controls, Audio, Sequencer, Engines, Retro, Config, Links, About, Patch, Song, Project };
Screen currentScreen = Screen::Menu;
// "Retour" (2026-09-19, "il faut pas que ca revienne aux menu general
// il faut que ca revienne d'un etage seulement") -- UN SEUL niveau
// memorise (pas une pile complete), mis a jour a chaque goTo() reel
// (voir plus bas) : suffit pour "revenir d'ou on vient" partout, sans
// la complexite d'une vraie pile d'historique.
Screen navPrevious = Screen::Menu;

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

// Exclue de la page SEQUENCEUR depuis le 2026-09-19 ("on peut enlever
// la ligne du bas tensy en vert gagner de la place pour des bouton de
// transport plus gros") -- cette page a deja tres peu de marge
// verticale (grille 16 pas + panneau lateral + barre transport, voir
// kTrkControlsY), et cette ligne n'apporte rien d'utile pendant
// l'edition d'un morceau (le lien Teensy est deja implicitement
// confirme par le simple fait que jouer/editer fonctionne). Exclue
// aussi de la page PATCH le meme jour ("on a le tensy en vert en bas
// qui nous empeche de voir les dernieres lignes") -- meme raisonnement,
// la page defile deja (voir patchScroll) et a besoin de tout l'espace
// vertical disponible.
bool drawLinkStatusExcluded() {
  return currentScreen == Screen::Sequencer || currentScreen == Screen::Patch;
}

void drawLinkStatus() {
  if (drawLinkStatusExcluded()) {
    return;
  }
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
    {"PROJET", "sauvegarder / charger tout le morceau", Screen::Project, MenuCat::Musique},
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

// Temoin de potard superpose EN HAUT de l'ecran, visible sur N'IMPORTE
// QUELLE page (2026-09-18, retour utilisateur : "quand on bouge le
// volume on a pas de jauge qui apparait, il faut que ca apparaisse en
// haut ou bas qu'on sache ou on en est") -- drawPotBar() existant reste
// EXCLUSIF a la page CONTROLES (position fixe dans sa mise en page),
// celui-ci est un second affichage indépendant, une simple bande fine
// tout en haut (y=0..10, avant le contenu de n'importe quelle page).
// S'auto-efface apres kPotToastTimeoutMs en relancant un rendu complet
// de la page courante (voir loop()) -- plus simple/robuste que de
// retenir ce qu'il y avait dessous pour le redessiner a la main.
constexpr int16_t kPotToastY = 0;
constexpr int16_t kPotToastH = 10;
constexpr uint32_t kPotToastTimeoutMs = 1500;
bool potToastActive = false;
uint32_t potToastLastMs = 0;

void drawPotToast(uint8_t index) {
  gfx->fillRect(0, kPotToastY, kScreenSize, kPotToastH, RGB565_BLACK);
  const int16_t fillW = static_cast<int16_t>(static_cast<float>(potValue[index]) / 127.0f * kScreenSize);
  if (fillW > 0) {
    gfx->fillRect(0, kPotToastY, fillW, kPotToastH, kPalette[index % kPaletteCount]);
  }
  potToastActive = true;
  potToastLastMs = millis();
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

// Clavier tactile comme editeur live (demande 2026-09-15, etape 6 de
// AZ2_TRACKER_ETUDE.md) : "taper un pad pendant qu'un pas de
// sequenceur est selectionne doit pouvoir poser cette note sur le pas
// au lieu de/en plus de croix haut/bas". selectedSeqTrack/
// selectedSeqStep valent TOUJOURS quelque chose depuis la refonte du
// tracker (plus jamais -1, voir leur declaration) -- impossible de
// deviner "un pas est selectionne" a partir de leur seule valeur.
// D'ou un bouton D dedie (libre, la GB n'utilise que A/B/C -- voir
// drawGbRecIndicator()/le commentaire de C) pour bascule explicite,
// pour ne jamais ecraser une composition par accident en jouant
// simplement sur les pads.
bool padEditsStep = false;
extern int8_t selectedSeqTrack;  // definie plus bas, avec le reste de l'etat du sequenceur
extern int8_t selectedSeqStep;

void drawAudioPage() {
  char title[48];
  if (padEditsStep) {
    snprintf(title, sizeof(title), "AUDIO - pose sur piste %d pas %d (D)", selectedSeqTrack, selectedSeqStep);
  } else {
    snprintf(title, sizeof(title), "AUDIO - touche pour jouer (D=poser sur pas)");
  }
  drawSubHeader(title, kPalette[2]);
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
// Bord droit commun a la vue tracker (colonnes NOTE/INST/FX/VAL/PROB/COND, voir
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
// Probabilite/condition par pas (2026-09-17, voir PROB:/COND: dans
// AZ2_Protocol.h et SequencerTrack::stepProb/stepCondition cote Teensy).
// seqStepProb DOIT etre seede a 100 (pas 0) au boot, sinon l'ecran
// afficherait "0%" avant le premier echo PROB: -- voir le bloc de seed
// plus bas (meme endroit que seqStepPatch=0xFF).
uint8_t seqStepProb[kPatternCount][kSeqTrackCount][kSeqStepCount];
uint8_t seqStepCondition[kPatternCount][kSeqTrackCount][kSeqStepCount] = {};

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
bool metronomeOn = false;  // 2026-09-19, voir METRO: cote Teensy

// Piste/pas selectionnes dans la vue tracker (voir plus bas) -- la
// croix du Teensy (NAV:UP/DOWN) change la valeur de la colonne
// seqDetailCol du pas selectionne. Demarre a (0,0), pas (-1,-1) : la
// vue tracker est desormais TOUJOURS active (voir seqDetailMode), donc
// il faut toujours une piste/un pas valides des le boot.
int8_t selectedSeqTrack = 0;
int8_t selectedSeqStep = 0;

// Vue tracker (colonnes NOTE/INST/FX/VAL/PROB/COND d'UNE piste, comme l'ecran
// phrase de LSDJ/M8 -- voir AZ2_TRACKER_ETUDE.md) -- devenue la SEULE
// vue de la page SEQUENCEUR le 2026-09-16 (retour utilisateur : "on a
// pas de tracker a la M8 LSDJ", la grille ON/OFF + double-tap pour
// voir le detail ne correspondait pas a l'experience tracker attendue).
// La variable reste (toujours true) pour ne pas casser tout le code de
// navigation/edition deja ecrit autour, mais rien ne la remet plus a
// false -- plus de grille a laquelle "revenir".
bool seqDetailMode = true;
// 0=NOTE 1=INST 2=FX 3=VAL 4=PROB 5=COND (les 2 derniers ajoutes le
// 2026-09-17) -- voir detailColX()/drawDetailRow() pour l'affichage et
// le switch(seqDetailCol) dans le gestionnaire de croix pour l'edition.
int8_t seqDetailCol = 0;

// ---------------------------------------------------------------------
// Vue tracker (colonnes NOTE/INST/FX/VAL/PROB/COND d'une piste, voir
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
// PRB/CND ajoutees le 2026-09-17 (voir PROB:/COND:) -- largeurs choisies
// etroites expres ("100%"/"1:2"/"FILL" tiennent a la taille de texte 1)
// pour rester dans les ~202px encore libres a droite de VAL avant le
// panneau lateral (voir kTrkSideX plus bas, qui retrecit d'autant).
// PAS VERIFIE A L'ECRAN (compile seulement, pas de materiel branche
// cette session) -- premiere chose a regarder au prochain flash reel.
constexpr int16_t kDetailProbW = 42;
constexpr int16_t kDetailCondW = 46;
// Bord DROIT reel de la grille (fin de la colonne CND), PAS
// kSeqRightEdge (bord de l'ECRAN, bien plus loin -- voir plus bas,
// kTrkSideX/kTrkSideW l'utilisent a raison pour placer le panneau
// LATERAL apres la grille). Bug trouve le 2026-09-19 ("les lignes
// disparaissent a mesure que j'edite ... ca efface le cadre [du
// panneau lateral]") : drawDetailRow() effacait (fillRect noir) toute
// la largeur jusqu'a kSeqRightEdge a chaque ligne editee -- ca
// recouvrait donc systematiquement la bande du panneau lateral a la
// meme hauteur Y que la ligne editee, un peu plus a chaque pas edite.
constexpr int16_t kDetailGridRight = kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW +
                                      kDetailValW + kDetailProbW + kDetailCondW;

const char *const kNoteNames[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
const char *const kStepFxNames[] = {"---", "ARP", "CUT", "RET"};
constexpr uint8_t kStepFxCount = sizeof(kStepFxNames) / sizeof(kStepFxNames[0]);

void formatNoteName(uint8_t note, char *out, size_t outSize) {
  const int octave = static_cast<int>(note) / 12 - 1;  // MIDI 60 = C4, convention M8/LSDJ
  snprintf(out, outSize, "%s%d", kNoteNames[note % 12], octave);
}

int16_t detailColX(uint8_t col) {
  // 0=STEP (pas de colonne editable, juste le numero), 1=NOTE, 2=INST,
  // 3=FX, 4=VAL, 5=PROB, 6=COND (2 dernieres ajoutees le 2026-09-17) --
  // decalage de 1 par rapport a seqDetailCol (qui ne compte que les
  // colonnes editables, 0-5).
  switch (col) {
    case 0: return kDetailLeft;
    case 1: return static_cast<int16_t>(kDetailLeft + kDetailStepW);
    case 2: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW);
    case 3: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW);
    case 4: return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW);
    case 5:
      return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW +
                                   kDetailValW);
    default:
      return static_cast<int16_t>(kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW +
                                   kDetailValW + kDetailProbW);
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
  gfx->setCursor(static_cast<int16_t>(detailColX(5) + 2), y);
  gfx->print("PRB");
  gfx->setCursor(static_cast<int16_t>(detailColX(6) + 2), y);
  gfx->print("CND");
}

void drawDetailRow(uint8_t step) {
  const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
  const int16_t y = static_cast<int16_t>(kDetailTop + step * (kDetailRowH + kDetailRowGap));
  const bool on = seqStepOn[currentPattern][track][step];
  const bool rowSelected = (step == selectedSeqStep);
  const bool playhead = (step == seqCurrentStep);
  const uint16_t accent = kPalette[track % kPaletteCount];

  // kDetailGridRight (bord de la grille), PAS kSeqRightEdge (bord de
  // l'ECRAN, recouvrait le panneau lateral -- voir son commentaire).
  gfx->fillRect(kDetailLeft, y, kDetailGridRight - kDetailLeft, kDetailRowH,
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

  // PROB (colonne editable 4, 2026-09-17) -- masquee ("--") a 100%
  // (comportement d'origine, pas de bruit visuel sur un pattern qui
  // n'utilise pas la fonction).
  const uint8_t prob = seqStepProb[currentPattern][track][step];
  const bool probSel = rowSelected && seqDetailCol == 4;
  if (probSel) {
    gfx->fillRect(detailColX(5), y, kDetailProbW, kDetailRowH, accent);
  }
  gfx->setTextColor(probSel ? RGB565_BLACK : (on ? RGB565_WHITE : kFaint));
  if (prob >= 100) {
    snprintf(buf, sizeof(buf), "--");
  } else {
    snprintf(buf, sizeof(buf), "%d", prob);
  }
  gfx->setCursor(static_cast<int16_t>(detailColX(5) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);

  // COND (colonne editable 5, 2026-09-17) -- meme principe visuel que
  // FX/VAL, label lisible via az2::stepConditionLabel() (partage avec le
  // Teensy pour ne pas dupliquer l'encodage).
  const uint8_t cond = seqStepCondition[currentPattern][track][step];
  const bool condSel = rowSelected && seqDetailCol == 5;
  if (condSel) {
    gfx->fillRect(detailColX(6), y, kDetailCondW, kDetailRowH, accent);
  }
  gfx->setTextColor(condSel ? RGB565_BLACK : (on ? RGB565_WHITE : kFaint));
  az2::stepConditionLabel(cond, buf, sizeof(buf));
  gfx->setCursor(static_cast<int16_t>(detailColX(6) + 2), static_cast<int16_t>(y + 6));
  gfx->print(buf);
}

// Selecteur de piste ("< PISTE N >", meme motif que la page PATCH) --
// desormais AU-DESSUS des colonnes NOTE/INST/FX/VAL/PROB/COND, toujours visible
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
// Retour a la taille/disposition d'origine (2026-09-19, retour
// utilisateur explicite : "la ligne du bas on la laisse comme elle
// est") -- METRONOME (et les autres boutons) vont dans le panneau
// LATERAL (voir drawTrkSidePanel()), pas ici.
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
// PRB/CND (2026-09-17) retrecissent ce panneau de ~88px (kDetailProbW +
// kDetailCondW) -- PAS VERIFIE A L'ECRAN si ce qui reste (~106px) est
// encore assez large pour le contenu de drawTrkSidePanel(), a l'oeil au
// prochain flash reel.
constexpr int16_t kTrkSideX = kDetailLeft + kDetailStepW + kDetailNoteW + kDetailInstW + kDetailFxW + kDetailValW +
                               kDetailProbW + kDetailCondW + kTrkSideGap;
constexpr int16_t kTrkSideW = kSeqRightEdge - kTrkSideX;

constexpr int16_t kTrkSideH = kSeqStepCount * (kDetailRowH + kDetailRowGap);

// Panneau lateral repense le 2026-09-19 (retour utilisateur sur
// materiel reel : "le cadre du patch est toujours pas bon" + "des
// bouton plus gros un bouton pour le clavier un pour les effet un
// pour le moteur et patch") -- l'ancien resume texte (moteur/patch/
// cutoff/reso/adsr) disparait completement, remplace par 5 boutons
// PLEINE LARGEUR empiles qui couvrent toute la hauteur du panneau :
// MOTEUR, PATCH, EFFET, CLAVIER, METRONOME. Chacun ouvre directement
// la page correspondante pour la piste du tracker actuellement
// affichee (sauf METRONOME, une simple bascule, et EFFET qui reste
// dans le tracker mais amene le focus croix sur la colonne FX).
constexpr uint8_t kTrkSideBtnCount = 5;
constexpr int16_t kTrkSideBtnGap = 4;
constexpr int16_t kTrkSideBtnH = (kTrkSideH - (kTrkSideBtnCount - 1) * kTrkSideBtnGap) / kTrkSideBtnCount;

int16_t trkSideBtnY(uint8_t idx) {
  return static_cast<int16_t>(kDetailTop + idx * (kTrkSideBtnH + kTrkSideBtnGap));
}

void drawTrkSideBtn(uint8_t idx, uint16_t color, bool filled, const char *label) {
  const int16_t y = trkSideBtnY(idx);
  if (filled) {
    gfx->fillRect(static_cast<int16_t>(kTrkSideX + 1), y, static_cast<int16_t>(kTrkSideW - 2), kTrkSideBtnH, color);
    gfx->setTextColor(RGB565_BLACK);
  } else {
    gfx->fillRect(static_cast<int16_t>(kTrkSideX + 1), y, static_cast<int16_t>(kTrkSideW - 2), kTrkSideBtnH,
                   RGB565_BLACK);
    gfx->drawRect(static_cast<int16_t>(kTrkSideX + 1), y, static_cast<int16_t>(kTrkSideW - 2), kTrkSideBtnH, color);
    gfx->setTextColor(color);
  }
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kTrkSideX + 10), static_cast<int16_t>(y + kTrkSideBtnH / 2 - 8));
  gfx->print(label);
}

bool hitTestTrkSideBtn(uint8_t idx, int16_t x, int16_t y) {
  return inBox(x, y, kTrkSideX, trkSideBtnY(idx), kTrkSideW, kTrkSideBtnH);
}

void drawTrkSidePanel() {
  const uint16_t accent = kPalette[selectedSeqTrack % kPaletteCount];

  gfx->fillRect(kTrkSideX, kDetailTop, kTrkSideW, kTrkSideH, RGB565_BLACK);
  gfx->drawRect(kTrkSideX, kDetailTop, kTrkSideW, kTrkSideH, accent);

  drawTrkSideBtn(0, accent, false, "MOTEUR");
  drawTrkSideBtn(1, accent, true, "PATCH");
  drawTrkSideBtn(2, kPalette[2], false, "EFFET");
  drawTrkSideBtn(3, kPalette[5], false, "CLAVIER");
  drawTrkSideBtn(4, metronomeOn ? kPalette[3] : kFaint, metronomeOn, "METRO");
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
  // kDetailGridRight (bord de la grille), pas kSeqRightEdge (bord de
  // l'ecran) -- inoffensif en pratique (un toucher dans le panneau
  // lateral finissait par ne matcher aucune colonne de toute facon),
  // corrige par coherence avec le fix de drawDetailRow() ci-dessus.
  if (x < kDetailLeft || x > kDetailGridRight) {
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
// Doit rester identique aux valeurs de boot du Teensy. DEXED est encore
// selectionnable, mais pas active par defaut tant que son souffle sur le
// materiel reel n'est pas resolu.
uint8_t trackEngine[kSeqTrackCount] = {az2::kEngineAnalog, az2::kEngineAnalog, az2::kEngineEPiano, az2::kEngineBraids,
                                      az2::kEngineAnalog, az2::kEngineAnalog, az2::kEngineEPiano, az2::kEngineBraids};
uint8_t trackPatch[kSeqTrackCount] = {0, 0, 0, 0, 0, 0, 0, 0};
// Mute/solo (MUTE:/SOLO:, priorite #1 de la liste indispensable) --
// bascules cote Teensy dans trackMuted[]/trackSoloed[]/trackEffectiveGain(),
// voir le piege Braids documente dans AZ2_FEUILLE_DE_ROUTE.md.
bool trackMuted[kSeqTrackCount] = {};
bool trackSoloed[kSeqTrackCount] = {};

// Page MOTEURS repensee le 2026-09-19 (retour utilisateur sur materiel
// reel : "il faut l'organiser differemment une liste de moteur a
// gauche et a chaque moteur selectionne ca met les patch a droite et
// en dessous la mini fenetre de reglage du patch si on clic dessus on
// arrive a la page de reglage de patch"). Piste choisie en haut (meme
// motif "< PISTE N >" que PATCH/SEQUENCEUR), puis 2 colonnes : la
// liste des 6 MOTEURS a gauche, la liste des patches du moteur
// ASSIGNE a cette piste a droite (defile si plus de kEngListVisibleRows
// -- jusqu'a 255 pour DEXED), puis un bandeau "mini reglages" en bas,
// tactile, qui ouvre la page PATCH complete.
//
// Choisir une ligne (croix ou tactile) = assigner IMMEDIATEMENT (pas
// d'etape de confirmation separee -- meme convention "manipulation
// directe" que le reste de l'appli) : la liste MOTEUR n'a donc pas
// besoin d'un curseur distinct de trackEngine[piste], ni la liste
// PATCH d'un curseur distinct de trackPatch[piste] -- juste
// engPatchScroll pour savoir quelle partie de la liste (potentiellement
// longue) est actuellement visible.
int8_t selectedEngineTrack = 0;
// false = focus croix sur la liste MOTEUR (gauche), true = liste PATCH
// (droite) -- GAUCHE/DROITE bascule (spatial, plus intuitif qu'avant
// avec de vraies colonnes cote a cote), HAUT/BAS deplace/choisit dans
// la colonne au focus.
bool engineColPatch = false;
uint16_t engPatchScroll = 0;
// true = focus croix sur la ligne PISTE en haut (GAUCHE/DROITE change
// alors la piste), false = focus dans une des 2 listes (voir
// engineColPatch pour laquelle) -- voir le gestionnaire NAV: pour le
// detail complet (2026-09-19, 2e passe suite au retour "on est un peu
// dans le desordre de controle").
bool engOnTrackRow = false;

constexpr int16_t kEngTrackRowY = 66;
constexpr int16_t kEngTrackRowH = 22;
constexpr int16_t kEngListTop = kEngTrackRowY + kEngTrackRowH + 10;
constexpr int16_t kEngListLeftW = (kScreenSize - 2 * kMargin) * 4 / 10;
constexpr int16_t kEngGap = 6;
constexpr int16_t kEngListRightX = kMargin + kEngListLeftW + kEngGap;
constexpr int16_t kEngListRightW = kScreenSize - 2 * kMargin - kEngListLeftW - kEngGap;
constexpr int16_t kEngListRowH = 42;
// 6 -- tombe pile sur le nombre de moteurs (liste gauche jamais
// scrollee), la liste PATCH (droite) partage la meme fenetre/hauteur
// et defile au-dela (voir engPatchScroll), meme principe que
// patchScroll sur la page PATCH.
constexpr uint8_t kEngListVisibleRows = az2::kEngineCount;
constexpr int16_t kEngListH = kEngListVisibleRows * kEngListRowH;
constexpr int16_t kEngMiniY = kEngListTop + kEngListH + 10;
constexpr int16_t kEngMiniH = 66;

void drawEngTrackRow() {
  const int16_t w = static_cast<int16_t>(kScreenSize - 2 * kMargin);
  gfx->fillRect(kMargin, kEngTrackRowY, w, kEngTrackRowH, RGB565_BLACK);
  // Contour blanc quand le focus croix est sur cette ligne (2026-09-19,
  // voir engOnTrackRow) -- seul indice visuel de "ou" on est avant de
  // bouger, important puisque GAUCHE/DROITE change de sens selon le
  // focus (piste ici, moteur/patch sinon).
  if (engOnTrackRow) {
    gfx->drawRect(kMargin, kEngTrackRowY, w, kEngTrackRowH, RGB565_WHITE);
  }
  gfx->setTextSize(2);
  gfx->setTextColor(kPalette[selectedEngineTrack % kPaletteCount]);
  char buf[16];
  snprintf(buf, sizeof(buf), "< PISTE %d >", selectedEngineTrack);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 55), kEngTrackRowY);
  gfx->print(buf);
}
bool hitTestEngTrackPrev(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kEngTrackRowY, static_cast<int16_t>(kScreenSize / 2 - kMargin), kEngTrackRowH);
}
bool hitTestEngTrackNext(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize / 2), kEngTrackRowY,
               static_cast<int16_t>(kScreenSize / 2 - kMargin), kEngTrackRowH);
}

void drawEngListRow(uint8_t engineIdx) {
  const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
  const int16_t y = static_cast<int16_t>(kEngListTop + engineIdx * kEngListRowH);
  const bool isCurrent = (engineIdx == trackEngine[t]);
  const bool focused = isCurrent && !engineColPatch && !engOnTrackRow;
  const uint16_t accent = kPalette[t % kPaletteCount];
  const int16_t h = static_cast<int16_t>(kEngListRowH - 2);

  gfx->fillRect(kMargin, y, kEngListLeftW, h, isCurrent ? accent : RGB565_BLACK);
  gfx->drawRect(kMargin, y, kEngListLeftW, h, isCurrent ? accent : kFaint);
  // Contour BLANC EPAIS (3px, 2026-09-19 -- "on fait un truc en
  // surbrillance plus visible pour qu'on voit mieux ce qui est
  // selectionne") quand c'est la ligne au focus croix -- un seul pixel
  // se voyait mal par-dessus le remplissage deja colore.
  if (focused) {
    for (int16_t o = 0; o < 3; ++o) {
      gfx->drawRect(static_cast<int16_t>(kMargin + 1 + o), static_cast<int16_t>(y + 1 + o),
                    static_cast<int16_t>(kEngListLeftW - 2 - 2 * o), static_cast<int16_t>(h - 2 - 2 * o), RGB565_WHITE);
    }
  }
  gfx->setTextSize(2);
  gfx->setTextColor(isCurrent ? RGB565_BLACK : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(y + h / 2 - 8));
  gfx->print(az2::engineName(engineIdx));
}

// slot = position dans la fenetre visible (0..kEngListVisibleRows-1),
// PAS l'index de patch lui-meme (voir engPatchScroll).
void drawEngPatchRow(uint8_t slot) {
  const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
  const uint16_t patchIdx = static_cast<uint16_t>(engPatchScroll + slot);
  const int16_t y = static_cast<int16_t>(kEngListTop + slot * kEngListRowH);
  const int16_t h = static_cast<int16_t>(kEngListRowH - 2);
  const uint16_t count = az2::enginePatchCount(trackEngine[t]);

  if (patchIdx >= count) {
    gfx->fillRect(kEngListRightX, y, kEngListRightW, h, RGB565_BLACK);
    return;
  }
  const bool isCurrent = (patchIdx == trackPatch[t]);
  const bool focused = isCurrent && engineColPatch && !engOnTrackRow;
  const uint16_t accent = kPalette[t % kPaletteCount];

  gfx->fillRect(kEngListRightX, y, kEngListRightW, h, isCurrent ? accent : RGB565_BLACK);
  gfx->drawRect(kEngListRightX, y, kEngListRightW, h, isCurrent ? accent : kFaint);
  // Contour epais -- meme raison/meme technique que drawEngListRow().
  if (focused) {
    for (int16_t o = 0; o < 3; ++o) {
      gfx->drawRect(static_cast<int16_t>(kEngListRightX + 1 + o), static_cast<int16_t>(y + 1 + o),
                    static_cast<int16_t>(kEngListRightW - 2 - 2 * o), static_cast<int16_t>(h - 2 - 2 * o), RGB565_WHITE);
    }
  }
  gfx->setTextSize(1);
  gfx->setTextColor(isCurrent ? RGB565_BLACK : RGB565_WHITE);
  char buf[24];
  snprintf(buf, sizeof(buf), "%3d %s", patchIdx, az2::enginePatchName(trackEngine[t], static_cast<uint8_t>(patchIdx)));
  gfx->setCursor(static_cast<int16_t>(kEngListRightX + 6), static_cast<int16_t>(y + h / 2 - 4));
  gfx->print(buf);
}

void drawEngLists() {
  for (uint8_t i = 0; i < az2::kEngineCount; ++i) {
    drawEngListRow(i);
  }
  for (uint8_t s = 0; s < kEngListVisibleRows; ++s) {
    drawEngPatchRow(s);
  }
  // Cadre epais AUTOUR DE TOUTE LA LISTE qui a le focus croix
  // (2026-09-19, meme demande que le contour de ligne plus haut) --
  // dessine EN DERNIER (par-dessus les lignes) pour voir tout de suite
  // "de quel cote" on est (moteur ou patch), pas seulement quelle
  // ligne precise -- utile des qu'on hesite en un coup d'oeil rapide.
  if (!engOnTrackRow) {
    const uint16_t accent = kPalette[selectedEngineTrack % kPaletteCount];
    const int16_t boxX = engineColPatch ? kEngListRightX : kMargin;
    const int16_t boxW = engineColPatch ? kEngListRightW : kEngListLeftW;
    for (int16_t o = 0; o < 3; ++o) {
      gfx->drawRect(static_cast<int16_t>(boxX - o), static_cast<int16_t>(kEngListTop - o),
                    static_cast<int16_t>(boxW + 2 * o), static_cast<int16_t>(kEngListH + 2 * o), accent);
    }
  }
}

// Bandeau "mini reglages" en bas -- resume le patch actif (2 lignes
// cles selon le moteur, meme logique que patchRowLabel()/
// patchParamRef() de la page PATCH), tactile : ouvre cette meme page
// PATCH complete pour aller plus loin (voir hitTestEngMini()).
void drawEngMiniPatch() {
  const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
  const uint16_t accent = kPalette[t % kPaletteCount];
  const int16_t w = static_cast<int16_t>(kScreenSize - 2 * kMargin);

  gfx->fillRect(kMargin, kEngMiniY, w, kEngMiniH, RGB565_BLACK);
  gfx->drawRect(kMargin, kEngMiniY, w, kEngMiniH, accent);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kEngMiniY + 4));
  gfx->print("REGLAGES DU PATCH -- toucher pour tout regler >");

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char buf[28];
  if (trackEngine[t] == az2::kEngineDexed) {
    snprintf(buf, sizeof(buf), "ALGO %d  FDBK %d", trackAlgo[t] + 1, trackFeedback[t]);
  } else {
    snprintf(buf, sizeof(buf), "CUTOFF %d  RESO %d", trackCutoff[t], trackReso[t]);
  }
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kEngMiniY + 20));
  gfx->print(buf);

  gfx->setTextSize(1);
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kEngMiniY + 46));
  gfx->print(az2::enginePatchName(trackEngine[t], trackPatch[t]));
}

bool hitTestEngMini(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kEngMiniY, static_cast<int16_t>(kScreenSize - 2 * kMargin), kEngMiniH);
}

void drawEnginesPage() {
  drawSubHeader("MOTEURS", kPalette[2]);
  drawEngTrackRow();
  drawEngLists();
  drawEngMiniPatch();
}

// Renvoie l'index de moteur (0-5) touche dans la liste GAUCHE, -1 si
// hors zone.
int8_t hitTestEngListRow(int16_t x, int16_t y) {
  if (x < kMargin || x >= kMargin + kEngListLeftW || y < kEngListTop || y >= kEngListTop + kEngListH) {
    return -1;
  }
  return static_cast<int8_t>((y - kEngListTop) / kEngListRowH);
}

// Renvoie le SLOT (0..kEngListVisibleRows-1, PAS l'index de patch --
// voir engPatchScroll) touche dans la liste DROITE, -1 si hors zone.
int8_t hitTestEngPatchRow(int16_t x, int16_t y) {
  if (x < kEngListRightX || x >= kEngListRightX + kEngListRightW || y < kEngListTop || y >= kEngListTop + kEngListH) {
    return -1;
  }
  return static_cast<int8_t>((y - kEngListTop) / kEngListRowH);
}

// Redessin complet -- utilise par les echos ENGINE:/PATCH:/MUTE:/SOLO:
// (voir plus bas) qui redessinaient auparavant UNE ligne (drawEngRow(),
// ancien format 8 pistes) -- desormais un seul track affiche a la
// fois, un redessin complet est aussi simple et sans risque d'oubli.
void drawEngRow(uint8_t track) {
  if (track == selectedEngineTrack && currentScreen == Screen::Engines) {
    drawEnginesPage();
  }
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
// true = focus croix sur la ligne PISTE en haut (GAUCHE/DROITE change
// alors la piste), false = focus dans la grille de parametres (voir
// selectedPatchRow, declare plus bas) -- meme convention que
// engOnTrackRow sur la page MOTEURS (2026-09-19, "la droite gauche
// change les piste il faut que ca change les reglages selectionnes
// [a la place]") : on atteint la ligne PISTE en montant (HAUT) depuis
// la toute premiere ligne de la grille, jamais autrement. Declare ici
// (avant drawPatchTrackRow()) plutot qu'aux cotes de selectedPatchRow
// car utilise des la definition de cette fonction, plus bas dans ce
// meme bloc.
bool patchOnTrackRow = false;
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
// voir le commentaire au-dessus de trackAlgo[]). Ne concerne QUE les 6
// lignes fixes (0-5) -- "row < 6" ajoute avec les lignes extra
// (2026-09-18) : sans lui, "row >= 4" desactiverait a tort TOUTES les
// lignes extra de DEXED (6 et plus), qui n'ont rien a voir avec ce cas
// Dexed-ADSR precis. Les appelants ne doivent d'ailleurs appeler ceci
// que pour row < 6 (voir patchExtraCount()/patchVolRow() pour le reste
// de la page) -- le clamp reste par securite si jamais appele hors de
// ce domaine.
bool patchRowActive(uint8_t track, uint8_t row) {
  return !(trackEngine[track] == az2::kEngineDexed && row >= 4 && row < 6);
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

// ---------------------------------------------------------------------
// Lignes "extra" de la page PATCH (2026-09-18, "un editeur de patch
// complet et completement reglable ... sovegardable") -- au-dela des
// 6 lignes filtre/ADSR-ou-DXP fixes ci-dessus, chaque moteur peut
// exposer un nombre different de parametres supplementaires (DXR:/EXP:/
// BXP: cote Teensy, voir handleDexedRawCommand()/handleEPianoParamCommand()/
// handleBraidsParamCommand()). Ces lignes viennent APRES les 6 lignes
// fixes et AVANT volume/slot (qui se decalent donc dynamiquement --
// voir patchVolRow()/patchSlotRow() -- au lieu d'etre fixes a 6/7).
// KARPLUS/ANALOG n'ont rien de plus a exposer (0 ligne extra, page
// inchangee -- meme mise en page qu'avant ce chantier).
uint8_t patchExtraCount(uint8_t track) {
  switch (trackEngine[track]) {
    case az2::kEngineDexed: return 17;   // parametres globaux DX7 (hors algo/feedback deja lignes 2-3, hors nom)
    case az2::kEngineEPiano: return 12;  // les 12 parametres continus mdaEPiano
    case az2::kEngineBraids: return 2;   // color, timbre (shape reste sur la page MOTEURS)
    default: return 0;
  }
}
uint8_t patchVolRow(uint8_t track) { return static_cast<uint8_t>(6 + patchExtraCount(track)); }
uint8_t patchSlotRow(uint8_t track) { return static_cast<uint8_t>(patchVolRow(track) + 1); }
uint8_t patchTotalRows(uint8_t track) { return static_cast<uint8_t>(patchSlotRow(track) + 1); }

// 2 parametres par ligne VISUELLE (2026-09-19, "mettre 2 reglage par
// ligne pour gagner de la place") -- tous les parametres "simples"
// (0..patchVolRow(track) inclus, donc VOLUME participe aussi a la
// paire) partagent leur ligne 2 par 2 ; SLOT (le dernier, 3 sous-
// boutons deja denses -- SLOT/SAVE/LOAD) reste TOUJOURS seul sur sa
// propre ligne. logicalRow reste le meme index qu'avant (0-5 base,
// 6..volRow-1 extra, volRow=volume, slotRow=slot) -- seule la
// POSITION A L'ECRAN change ici, pas la numerotation logique (utilisee
// par selectedPatchRow/patchScroll, la navigation croix reste
// coherente sans etre reecrite en profondeur).
int16_t patchVisualRow(uint8_t track, uint8_t logicalRow) {
  const uint8_t volRow = patchVolRow(track);
  if (logicalRow <= volRow) {
    return static_cast<int16_t>(logicalRow / 2);
  }
  return static_cast<int16_t>((volRow + 2) / 2);  // = ceil((volRow+1)/2), la ligne SLOT juste apres les paires
}
uint8_t patchTotalVisualRows(uint8_t track) {
  const uint8_t volRow = patchVolRow(track);
  return static_cast<uint8_t>((volRow + 2) / 2 + 1);
}
// true = colonne DROITE (logicalRow impair), false = GAUCHE -- sans
// objet pour la ligne SLOT (toujours pleine largeur, jamais appele
// pour elle).
bool patchIsRightCol(uint8_t logicalRow) { return (logicalRow % 2) == 1; }

// Deplacement HAUT/BAS (2026-09-19, suite du chantier "2 reglages par
// ligne") : monte/descend d'une ligne VISUELLE en gardant la meme
// colonne (gauche/droite) quand elle existe, saute les lignes
// inactives (DEXED 4-5, voir patchRowActive()). Renvoie -1 quand on
// doit sortir vers la ligne PISTE (HAUT depuis la toute premiere
// ligne) -- l'appelant bascule alors patchOnTrackRow. Ne bouge pas
// (renvoie `row` tel quel) quand on est deja sur la derniere ligne
// (BAS depuis SLOT).
int8_t patchStepVisual(uint8_t track, int8_t row, int8_t dir) {
  const uint8_t volRow = patchVolRow(track);
  const uint8_t slotRow = patchSlotRow(track);
  const int16_t slotVisual = patchVisualRow(track, slotRow);
  const bool wasRight = (row <= static_cast<int8_t>(volRow)) && patchIsRightCol(static_cast<uint8_t>(row));
  int16_t visualRow = patchVisualRow(track, static_cast<uint8_t>(row));
  for (;;) {
    visualRow = static_cast<int16_t>(visualRow + dir);
    if (visualRow < 0) {
      return -1;
    }
    if (visualRow > slotVisual) {
      return row;
    }
    int8_t candidate;
    if (visualRow == slotVisual) {
      candidate = static_cast<int8_t>(slotRow);
    } else {
      const int8_t left = static_cast<int8_t>(visualRow * 2);
      const int8_t right = static_cast<int8_t>(visualRow * 2 + 1);
      candidate = (wasRight && right <= static_cast<int8_t>(volRow)) ? right : left;
    }
    if (candidate == static_cast<int8_t>(slotRow) || candidate >= 6 ||
        patchRowActive(track, static_cast<uint8_t>(candidate))) {
      return candidate;
    }
    // Ligne inactive : continue de chercher dans la meme direction
    // plutot que de s'arreter dessus (meme esprit que la boucle
    // GAUCHE/DROITE ci-dessous).
  }
}

// Deplacement GAUCHE/DROITE (2026-09-19, "la droite gauche [doit]
// changer les reglages selectionnes [dans la fenetre des patch]" --
// avant, GAUCHE/DROITE changeait de piste ici ; ce role passe
// desormais a la ligne PISTE, voir patchOnTrackRow) : parcourt l'ORDRE
// LOGIQUE des parametres (0,1 = 1ere ligne visuelle colonne gauche/
// droite, 2,3 = 2e ligne, etc.) -- comme une grille lue de gauche a
// droite puis ligne suivante, ce qui fait naturellement alterner les
// 2 colonnes d'une meme ligne puis passer a la ligne suivante en
// bout de course. Boucle (dernier <-> premier), saute les lignes
// inactives.
int8_t patchStepLogical(uint8_t track, int8_t row, int8_t dir, uint8_t total) {
  int8_t next = row;
  for (uint8_t tries = 0; tries < total; ++tries) {
    next = static_cast<int8_t>(((next + dir) % total + total) % total);
    if (next >= 6 || patchRowActive(track, static_cast<uint8_t>(next))) {
      return next;
    }
  }
  return row;
}

// Octet brut DXR (0-144) pour chaque ligne extra DEXED, dans l'ordre
// d'affichage -- voir DexedVoiceParameters dans dexed.h cote Teensy
// (offset 126 dans le buffer deballe, ajoute ici a l'envoi/la lecture).
// ALGO/FEEDBACK/NOM exclus (deja geres ou hors scope, voir
// patchExtraCount()).
constexpr uint8_t kDexedExtraRaw[17] = {
    0, 1, 2, 3, 4, 5, 6, 7,  // PEG R1-4, L1-4
    10,                      // OSC_KEY_SYNC
    11, 12, 13, 14,          // LFO SPEED/DELAY/PMD/AMD
    15, 16, 17,              // LFO SYNC/WAVE/PMS
    18,                      // TRANSPOSE
};
constexpr const char *kDexedExtraLabel[17] = {
    "PEG R1", "PEG R2", "PEG R3", "PEG R4", "PEG L1", "PEG L2", "PEG L3", "PEG L4",
    "KEY SYNC", "LFO SPEED", "LFO DELAY", "LFO PMD", "LFO AMD", "LFO SYNC", "LFO WAVE", "LFO PMS",
    "TRANSPOSE",
};
constexpr uint8_t kDexedExtraMax[17] = {
    99, 99, 99, 99, 99, 99, 99, 99,
    1, 99, 99, 99, 99, 1, 5, 7,
    48,
};

constexpr const char *kEPianoExtraLabel[12] = {
    "DECAY", "RELEASE", "HARDNESS", "TREBLE", "TREMOLO", "LFO RATE",
    "VEL SENSE", "STEREO", "TUNE", "DETUNE", "OVERDRIVE", "VOLUME",
};

constexpr const char *kBraidsExtraLabel[2] = {"COLOR", "TIMBRE"};

const char *patchExtraLabel(uint8_t track, uint8_t extraIdx) {
  switch (trackEngine[track]) {
    case az2::kEngineDexed: return kDexedExtraLabel[extraIdx];
    case az2::kEngineEPiano: return kEPianoExtraLabel[extraIdx];
    case az2::kEngineBraids: return kBraidsExtraLabel[extraIdx];
    default: return "?";
  }
}

uint8_t patchExtraMax(uint8_t track, uint8_t extraIdx) {
  // EPIANO/BRAIDS : toutes leurs lignes extra sont sur l'echelle
  // 0-127 (voir EXP:/BXP: cote Teensy) -- seul DEXED a une plage
  // reelle differente par parametre (voir kDexedExtraMax, reprise des
  // vraies bornes DX7).
  if (trackEngine[track] == az2::kEngineDexed) {
    return kDexedExtraMax[extraIdx];
  }
  return 127;
}

// Valeur courante de chaque ligne extra, par piste -- 17 = le plus
// grand des 3 moteurs concernes (DEXED), reutilise tel quel pour
// EPIANO (12) et BRAIDS (2), le reste de la ligne n'etant simplement
// jamais lu/affiche pour ces moteurs (voir patchExtraCount()).
uint8_t patchExtraVal[kSeqTrackCount][17] = {};

void sendPatchExtra(uint8_t track, uint8_t extraIdx) {
  char msg[24];
  switch (trackEngine[track]) {
    case az2::kEngineDexed:
      snprintf(msg, sizeof(msg), "DXR:%d:%d:%d", track, kDexedExtraRaw[extraIdx], patchExtraVal[track][extraIdx]);
      break;
    case az2::kEngineEPiano:
      snprintf(msg, sizeof(msg), "EXP:%d:%d:%d", track, extraIdx, patchExtraVal[track][extraIdx]);
      break;
    case az2::kEngineBraids:
      snprintf(msg, sizeof(msg), "BXP:%d:%d:%d", track, extraIdx, patchExtraVal[track][extraIdx]);
      break;
    default:
      return;
  }
  sendToTeensy(msg);
}

// Interroge le Teensy pour peupler patchExtraVal[track][] a l'entree
// sur la page/piste/moteur -- sans ca, l'ecran afficherait 0 pour tout
// tant que l'utilisateur n'a pas lui-meme modifie chaque ligne au
// moins une fois. Les reponses arrivent en DXR:/EXP: (BXP: n'a pas de
// forme "?", voir handleBraidsParamCommand() cote Teensy -- write-only,
// pas grave : color/timbre partent a 0 a l'affectation du moteur, une
// valeur de depart raisonnable pour 2 reglages de couleur sonore).
void queryPatchExtra(uint8_t track) {
  const uint8_t count = patchExtraCount(track);
  char msg[16];
  for (uint8_t i = 0; i < count; ++i) {
    if (trackEngine[track] == az2::kEngineDexed) {
      snprintf(msg, sizeof(msg), "DXR?%d:%d", track, kDexedExtraRaw[i]);
      sendToTeensy(msg);
    } else if (trackEngine[track] == az2::kEngineEPiano) {
      snprintf(msg, sizeof(msg), "EXP?%d:%d", track, i);
      sendToTeensy(msg);
    }
  }
}

uint8_t scopeSamples[az2::kScopeSamplesPerPacket] = {};
bool scopeHasData = false;

constexpr int16_t kPatchTrackRowY = 66;
constexpr int16_t kPatchScopeTop = 96;
constexpr int16_t kPatchScopeH = 90;
constexpr int16_t kPatchRowTop = kPatchScopeTop + kPatchScopeH + 14;
constexpr int16_t kPatchRowH = 34;

void drawPatchTrackRow() {
  const int16_t w = static_cast<int16_t>(kScreenSize - 2 * kMargin);
  gfx->fillRect(kMargin, kPatchTrackRowY, w, 22, RGB565_BLACK);
  // Contour blanc quand le focus croix est sur cette ligne (2026-09-19,
  // meme convention que drawEngTrackRow()/engOnTrackRow sur la page
  // MOTEURS) -- seul indice visuel de "ou" on est avant de bouger,
  // important puisque GAUCHE/DROITE change de sens selon le focus
  // (piste ici, reglage de patch sinon).
  if (patchOnTrackRow) {
    gfx->drawRect(kMargin, kPatchTrackRowY, w, 22, RGB565_WHITE);
  }
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

// Nombre de lignes VISUELLES affichees a l'ecran en meme temps --
// INCHANGE depuis l'origine de cette page (8, meme disposition pixel),
// mais depuis le 2026-09-19 chaque ligne visuelle peut contenir 2
// parametres LOGIQUES (voir patchVisualRow()) -- la fenetre defile
// desormais sur des lignes VISUELLES, pas logiques.
constexpr uint8_t kPatchVisibleRows = 8;
uint8_t patchScroll = 0;  // 1ere ligne VISUELLE affichee en haut de la fenetre
constexpr int16_t kPatchColGap = 8;

// Si le parametre logique `logicalRow` est actuellement dans la
// fenetre visible, calcule sa position (y/x/largeur -- x/w different
// selon la colonne gauche/droite pour les lignes appariees, voir
// patchVisualRow()/patchIsRightCol(), SLOT reste seule/pleine largeur)
// et renvoie true ; sinon renvoie false SANS rien dessiner -- les
// appelants existants (echo FILT:/ENV:/DXP:/VOL: notamment)
// redessinent par NUMERO DE PARAMETRE LOGIQUE, pas par position
// ecran : quand ce parametre est scrolle hors champ, le bon geste est
// de ne rien dessiner (le shadow reste a jour, il se redessine
// correctement au retour), pas de planter ou de dessiner au mauvais
// endroit.
bool patchRowVisible(uint8_t logicalRow, int16_t &y, int16_t &x, int16_t &w) {
  const uint8_t track = static_cast<uint8_t>(patchTrack);
  const uint8_t volRow = patchVolRow(track);
  const int16_t visualRow = patchVisualRow(track, logicalRow);
  if (visualRow < patchScroll || visualRow >= patchScroll + kPatchVisibleRows) {
    return false;
  }
  y = static_cast<int16_t>(kPatchRowTop + (visualRow - patchScroll) * kPatchRowH);
  if (logicalRow > volRow) {
    // Ligne SLOT : seule sur sa ligne, pleine largeur.
    x = kMargin;
    w = static_cast<int16_t>(kScreenSize - 2 * kMargin);
    return true;
  }
  const int16_t halfW = static_cast<int16_t>((kScreenSize - 2 * kMargin - kPatchColGap) / 2);
  x = patchIsRightCol(logicalRow) ? static_cast<int16_t>(kMargin + halfW + kPatchColGap) : kMargin;
  w = halfW;
  return true;
}

// Ligne selectionnee par la croix (2026-09-18, retour utilisateur sur
// materiel reel : "la fenetre de patch on peut rien regler avec les
// boutons" -- comme le panneau tracker avant son propre fix, cette page
// n'etait pilotable qu'au tactile). 0-5 = les 6 lignes filtre/ADSR-ou-
// DXP (voir patchParamRef()), 6..patchVolRow(t)-1 = les lignes extra du
// moteur (voir patchExtraCount()), puis VOLUME puis SLOT/SAVE/LOAD
// (index dynamique, voir patchVolRow()/patchSlotRow() -- ne sont plus
// fixes a 6/7 depuis l'ajout des lignes extra). Meme convention que
// seqDetailCol : GAUCHE/DROITE changent la piste (patchTrack), HAUT/BAS
// SEULS deplacent la ligne selectionnee, A maintenu + HAUT/BAS edite sa
// valeur -- SAUF sur la ligne SLOT ou A maintenu + GAUCHE/DROITE
// declenchent SAVE/LOAD (pas une valeur continue) -- voir le
// gestionnaire de croix plus bas.
int8_t selectedPatchRow = 0;

// Dessine le parametre LOGIQUE `i` (0-5 uniquement -- les lignes extra
// ont leur propre fonction, drawPatchExtraRow() plus bas) si il est
// actuellement visible dans la fenetre de defilement ; ne fait rien
// sinon (voir patchRowVisible()). Demi-largeur depuis le 2026-09-19
// ("mettre 2 reglage par ligne pour gagner de la place") -- PLUS de
// boutons tactiles +/- ici (pas la place, voir le commentaire de
// patchRowVisible()) : edition croix uniquement (A maintenu +
// HAUT/BAS), deja le moyen principal utilise partout ce soir. Le
// tactile reste utile pour SELECTIONNER une case (tap = select),
// juste plus pour l'editer directement.
void drawPatchRow(uint8_t i) {
  int16_t y, x, w;
  if (!patchRowVisible(i, y, x, w)) {
    return;
  }
  const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
  const uint8_t track = static_cast<uint8_t>(patchTrack);
  const bool rowSelected = !patchOnTrackRow && (selectedPatchRow == static_cast<int8_t>(i));
  const uint16_t accent = kPalette[track % kPaletteCount];

  gfx->fillRect(x, y, w, rowH, RGB565_BLACK);

  if (!patchRowActive(track, i)) {
    // Piste DEXED : lignes 4-5 sans effet (ADSR generique non ecoutee,
    // voir patchRowActive()) -- grisees plutot qu'une valeur qui ne
    // sert a rien pour ce moteur.
    gfx->drawRect(x, y, w, rowH, kFaint);
    gfx->setTextSize(1);
    gfx->setTextColor(kFaint);
    gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 4));
    gfx->print("(sans effet)");
    return;
  }

  gfx->drawRect(x, y, w, rowH, rowSelected ? accent : kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3));
  gfx->print(patchRowLabel(track, i));

  // ALGO (Dexed) affiche 1-32 (convention DX7), stocke 0-31 en interne.
  const bool isDexedAlgo = (trackEngine[track] == az2::kEngineDexed && i == 2);
  const uint8_t raw = patchParamRef(track, i);
  char buf[6];
  snprintf(buf, sizeof(buf), "%3d", isDexedAlgo ? raw + 1 : raw);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + w - 34), static_cast<int16_t>(y + 3));
  gfx->print(buf);
}

// Dessine une ligne EXTRA (logicalRow >= 6, < patchVolRow(track)) --
// meme habillage visuel que drawPatchRow() ci-dessus (demi-largeur,
// sans boutons tactiles +/-, voir son commentaire), juste une source
// de donnees differente (patchExtraVal[] au lieu de patchParamRef()).
// Ne fait rien si hors fenetre visible, meme convention que
// drawPatchRow().
void drawPatchExtraRow(uint8_t logicalRow) {
  int16_t y, x, w;
  if (!patchRowVisible(logicalRow, y, x, w)) {
    return;
  }
  const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
  const uint8_t track = static_cast<uint8_t>(patchTrack);
  const uint8_t extraIdx = static_cast<uint8_t>(logicalRow - 6);
  const bool rowSelected = !patchOnTrackRow && (selectedPatchRow == static_cast<int8_t>(logicalRow));
  const uint16_t accent = kPalette[track % kPaletteCount];

  gfx->fillRect(x, y, w, rowH, RGB565_BLACK);
  gfx->drawRect(x, y, w, rowH, rowSelected ? accent : kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3));
  gfx->print(patchExtraLabel(track, extraIdx));

  char buf[6];
  snprintf(buf, sizeof(buf), "%3d", patchExtraVal[track][extraIdx]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + w - 34), static_cast<int16_t>(y + 3));
  gfx->print(buf);
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

// Volume par piste (VOL:, priorite #4 de la liste indispensable,
// AZ2_BENCHMARK_CONCURRENCE.md) -- 7e ligne, VOLONTAIREMENT separee du
// systeme patchParamRef()/patchRowLabel() des 6 lignes filtre/ADSR-ou-
// DXP au-dessus (celui-ci change de sens selon le moteur, le volume
// s'applique lui a TOUS les moteurs de la meme facon -- pas la peine de
// le meler a cette logique conditionnelle). Voir trackVolume[] et le
// commentaire du piege Braids cote Teensy (AZ2_FEUILLE_DE_ROUTE.md).
uint8_t trackVolume[kSeqTrackCount] = {127, 127, 127, 127, 127, 127, 127, 127};

// VOLUME participe desormais a l'appariement 2-par-ligne comme les
// autres parametres simples (2026-09-19) -- meme habillage/memes
// contraintes de largeur que drawPatchRow(), plus de boutons tactiles
// +/- (voir son commentaire).
void drawVolRow() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  int16_t y, x, w;
  if (!patchRowVisible(patchVolRow(t), y, x, w)) {
    return;
  }
  const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
  const bool rowSelected = !patchOnTrackRow && (selectedPatchRow == static_cast<int8_t>(patchVolRow(t)));
  const uint16_t accent = kPalette[t % kPaletteCount];

  gfx->fillRect(x, y, w, rowH, RGB565_BLACK);
  gfx->drawRect(x, y, w, rowH, rowSelected ? accent : kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3));
  gfx->print("VOLUME");

  char buf[6];
  snprintf(buf, sizeof(buf), "%3d", trackVolume[t]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + w - 34), static_cast<int16_t>(y + 3));
  gfx->print(buf);
}

void sendPatchVol() {
  char msg[16];
  snprintf(msg, sizeof(msg), "VOL:%d:%d", patchTrack, trackVolume[static_cast<uint8_t>(patchTrack)]);
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
constexpr int16_t kPatchSlotH = 32;
constexpr int16_t kPatchSlotBtnW = (kScreenSize - 2 * kMargin) / 3;

void drawPatchSlotRow() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  int16_t y, rx, rw;
  if (!patchRowVisible(patchSlotRow(t), y, rx, rw)) {
    return;
  }
  const int16_t saveX = static_cast<int16_t>(kMargin + kPatchSlotBtnW);
  const int16_t loadX = static_cast<int16_t>(kMargin + 2 * kPatchSlotBtnW);
  const bool rowSelected = !patchOnTrackRow && (selectedPatchRow == static_cast<int8_t>(patchSlotRow(t)));
  const uint16_t accent = kPalette[t % kPaletteCount];
  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kPatchSlotH, RGB565_BLACK);
  gfx->drawRect(kMargin, y, kPatchSlotBtnW, kPatchSlotH, rowSelected ? accent : kFaint);
  gfx->drawRect(saveX, y, kPatchSlotBtnW, kPatchSlotH, rowSelected ? accent : kFaint);
  gfx->drawRect(loadX, y, kPatchSlotBtnW, kPatchSlotH, rowSelected ? accent : kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char buf[12];
  snprintf(buf, sizeof(buf), "SLOT %d", patchSlot);
  gfx->setCursor(static_cast<int16_t>(kMargin + 6), static_cast<int16_t>(y + 8));
  gfx->print(buf);

  gfx->setTextColor(kPalette[1 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(saveX + 14), static_cast<int16_t>(y + 8));
  gfx->print("SAVE");

  gfx->setTextColor(kPalette[2 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(loadX + 14), static_cast<int16_t>(y + 8));
  gfx->print("LOAD");
}

// Position dynamique (2026-09-19, bug reel trouve en reprenant ce
// chantier : ces 3 hitTest*() utilisaient encore une constante figee
// "kPatchSlotY" calculee pour l'ANCIENNE mise en page 1-reglage-par-
// ligne -- depuis l'appariement 2-par-ligne, la ligne SLOT n'est plus
// forcement a la position visuelle 7 (ex. KARPLUS/ANALOG, 0 ligne
// extra : elle tombe desormais en position visuelle 4), le tactile
// visait donc completement a cote sans jamais planter (juste un tap
// qui "ne fait rien"). Recalcule via patchRowVisible() a chaque appel,
// comme drawPatchSlotRow() le fait deja -- renvoie false si la ligne
// est actuellement scrollee hors champ (aucun hit possible).
bool hitTestPatchSlotNum(int16_t x, int16_t y) {
  int16_t ry, rx, rw;
  if (!patchRowVisible(patchSlotRow(static_cast<uint8_t>(patchTrack)), ry, rx, rw)) {
    return false;
  }
  return inBox(x, y, kMargin, ry, kPatchSlotBtnW, kPatchSlotH);
}
bool hitTestPatchSlotSave(int16_t x, int16_t y) {
  int16_t ry, rx, rw;
  if (!patchRowVisible(patchSlotRow(static_cast<uint8_t>(patchTrack)), ry, rx, rw)) {
    return false;
  }
  return inBox(x, y, static_cast<int16_t>(kMargin + kPatchSlotBtnW), ry, kPatchSlotBtnW, kPatchSlotH);
}
bool hitTestPatchSlotLoad(int16_t x, int16_t y) {
  int16_t ry, rx, rw;
  if (!patchRowVisible(patchSlotRow(static_cast<uint8_t>(patchTrack)), ry, rx, rw)) {
    return false;
  }
  return inBox(x, y, static_cast<int16_t>(kMargin + 2 * kPatchSlotBtnW), ry, kPatchSlotBtnW, kPatchSlotH);
}

// Ecrit tel quel (pas d'ajout) -- SD.remove() d'abord pour eviter tout
// risque d'ancien contenu residuel si FILE_WRITE ouvrait en ajout sur
// cette version de la lib (pas verifie, prudence peu couteuse ici).
// Ecriture ATOMIQUE (2026-09-19, defaut P1 signale par l'audit de
// code du meme jour) : l'ancien code faisait SD.remove(path) PUIS
// SD.open(path, FILE_WRITE) -- une coupure de courant, une erreur SD
// ou un plantage entre ces 2 etapes detruisait la derniere sauvegarde
// valide sans rien la remplacer. Sequence utilisee ici a la place
// (meme principe partout ou ce fichier sauvegarde quelque chose sur
// la carte SD -- patch, projet) : ecrit dans "<path>.tmp", verifie
// une taille non nulle, deplace l'ancien fichier (s'il existe) vers
// "<path>.bak", puis renomme le "tmp" vers le nom final. A aucun
// moment le fichier "final" n'est absent ou tronque -- soit l'ancien
// est encore la, soit le nouveau y est deja completement.
// `content` : callback qui ecrit dans le fichier ouvert (permet de
// reutiliser cette fonction pour un patch (1 ligne) ou un projet
// (bien plus long) sans dupliquer la logique tmp/bak/rename).
template <typename WriteFn>
bool atomicSaveFile(const char *path, WriteFn writeContent) {
  char tmpPath[40];
  char bakPath[40];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  snprintf(bakPath, sizeof(bakPath), "%s.bak", path);

  SD.remove(tmpPath);  // reste eventuel d'une tentative precedente avortee
  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) {
    return false;
  }
  writeContent(f);
  f.close();
  // Taille verifiee APRES fermeture, pas avant (bug reel trouve sur le
  // vrai materiel le 2026-09-19 : f.size() juste apres les f.printf()
  // mais AVANT f.close() renvoyait 0 sur cette implementation FS --
  // sans doute des octets pas encore vidanges/comptabilises tant que
  // le fichier reste ouvert -- ce qui faisait echouer TOUTE
  // sauvegarde avec PATCH_SAVE_ERROR/PROJECT_SAVE_ERROR alors que
  // l'ecriture elle-meme avait reussi. Reouvrir le fichier pour
  // verifier sa taille est donc fait expres, une seconde lecture, plus
  // lent mais fiable.
  File check = SD.open(tmpPath);
  const size_t written = check ? check.size() : 0;
  if (check) {
    check.close();
  }
  if (written == 0) {
    // Rien ecrit -- ne remplace surtout pas une sauvegarde valide par
    // un fichier vide, abandonne proprement.
    SD.remove(tmpPath);
    return false;
  }

  if (SD.exists(path)) {
    SD.remove(bakPath);  // .bak precedent, si un jour restaure manuellement puis jamais nettoye
    SD.rename(path, bakPath);
  }
  if (!SD.rename(tmpPath, path)) {
    // Echec du dernier renommage : restaure l'ancien fichier depuis le
    // backup plutot que de laisser "path" absent.
    SD.rename(bakPath, path);
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

void savePatchSlot(uint8_t slot) {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  SD.mkdir("/patches");
  char path[24];
  snprintf(path, sizeof(path), "/patches/%d.txt", slot);

  // 10 champs depuis l'ajout DXP (algo/feedback Dexed, sinon perdus au
  // rechargement -- PATCH: recharge tout le voice data DX7 depuis la
  // banque, voir loadDexedPatch() cote Teensy). Fichiers a 8 champs
  // (avant DXP) restent lisibles, voir loadPatchSlot().
  const bool ok = atomicSaveFile(path, [&](File &f) {
    f.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", trackEngine[t], trackPatch[t], trackCutoff[t], trackReso[t],
              trackAttack[t], trackDecay[t], trackSustain[t], trackRelease[t], trackAlgo[t], trackFeedback[t]);
  });
  if (!ok) {
    Serial.print("PATCH_SAVE_ERROR:");
    Serial.println(path);
    return;
  }
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

// Dessine (ou efface) TOUTE la fenetre de 8 lignes visibles d'un coup
// -- utilisee par drawPatchPage() (premier affichage) ET par tout
// changement qui peut deplacer patchScroll (navigation croix au-dela
// des 6 lignes fixes, changement de piste/moteur). Plus simple et plus
// robuste qu'un redessin ligne-par-ligne "au bon endroit" une fois le
// defilement possible -- un peu plus de travail ecran, sans
// consequence (pas appelee a haute frequence, contrairement au tracer
// scope qui a son propre chemin dedie).
void drawPatchWindow() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const uint8_t total = patchTotalRows(t);
  const uint8_t volRow = patchVolRow(t);
  const uint8_t slotRow = patchSlotRow(t);
  for (uint8_t slot = 0; slot < kPatchVisibleRows; ++slot) {
    const uint8_t logicalRow = static_cast<uint8_t>(patchScroll + slot);
    if (logicalRow >= total) {
      int16_t y, rx, rw;
      if (patchRowVisible(logicalRow, y, rx, rw)) {
        gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kPatchRowH - 4, RGB565_BLACK);
      }
      continue;
    }
    if (logicalRow < 6) {
      drawPatchRow(logicalRow);
    } else if (logicalRow == volRow) {
      drawVolRow();
    } else if (logicalRow == slotRow) {
      drawPatchSlotRow();
    } else {
      drawPatchExtraRow(logicalRow);
    }
  }
}

// Redessine UNE ligne logique par son numero, quel que soit son type
// (fixe/extra/volume/slot) -- petit repartiteur utilise par la
// navigation croix (deux lignes a rafraichir a chaque pas : celle
// qu'on quitte et celle qu'on selectionne), pour ne pas dupliquer ce
// if/else a chaque appel.
void redrawPatchLogicalRow(uint8_t track, uint8_t row) {
  if (row < 6) {
    drawPatchRow(row);
  } else if (row == patchVolRow(track)) {
    drawVolRow();
  } else if (row == patchSlotRow(track)) {
    drawPatchSlotRow();
  } else {
    drawPatchExtraRow(row);
  }
}

void drawPatchPage() {
  drawSubHeader("PATCH", kPalette[4]);
  drawPatchTrackRow();
  // Cadre du tracer dessine UNE fois ici -- drawPatchScope() (appelee a
  // chaque paquet SCOPE recu) ne touche plus que l'interieur, voir son
  // commentaire.
  gfx->drawRect(kMargin, kPatchScopeTop, kScreenSize - 2 * kMargin, kPatchScopeH, kFaint);
  drawPatchScope();
  drawPatchWindow();
}

bool hitTestPatchTrackPrev(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kPatchTrackRowY, kScreenSize / 2 - kMargin, 22);
}
bool hitTestPatchTrackNext(int16_t x, int16_t y) {
  return inBox(x, y, kScreenSize / 2, kPatchTrackRowY, kScreenSize / 2 - kMargin, 22);
}

// Renvoie -1 (aucun), sinon l'index du PARAMETRE LOGIQUE touche (0..
// patchVolRow(track) inclus -- base+extra+VOLUME, tous appariees 2 par
// ligne desormais ; SLOT garde ses propres hitTest*() dedies, voir
// plus bas, jamais couvert ici). Un tap SELECTIONNE seulement
// (2026-09-19, plus d'edition +/- directe au tactile -- voir le
// commentaire de patchRowVisible()) : l'edition se fait ensuite a la
// croix (A maintenu + HAUT/BAS), meme reflexe que partout ailleurs.
int8_t hitTestPatchParam(int16_t x, int16_t y) {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const uint8_t volRow = patchVolRow(t);
  const int16_t rowH = static_cast<int16_t>(kPatchRowH - 4);
  for (uint8_t i = 0; i <= volRow; ++i) {
    int16_t rowY, rowX, rowW;
    if (!patchRowVisible(i, rowY, rowX, rowW)) {
      continue;
    }
    if (inBox(x, y, rowX, rowY, rowW, rowH)) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

// Applique `delta` a la valeur du parametre actuellement selectionne
// (selectedPatchRow) et l'envoie au Teensy -- factorise le 2026-09-19
// ("on devrait pouvoir changer les valeurs ... en maintenant A et en
// pressant droite/gauche [en plus de haut/bas]") : A+GAUCHE/DROITE
// edite desormais la valeur exactement comme A+HAUT/BAS (meme
// fonction, meme convention de signe -- DROITE/HAUT = +1). Reprend
// mot pour mot la logique qui vivait avant dans le gestionnaire de
// croix (voir son commentaire pour le detail de chaque cas : VOLUME/
// SLOT/ligne fixe-ou-DXP/ligne EXTRA).
void patchApplyDelta(uint8_t t, int delta) {
  const uint8_t volRow = patchVolRow(t);
  const uint8_t slotRow = patchSlotRow(t);
  if (selectedPatchRow == static_cast<int8_t>(volRow)) {
    uint8_t &vol = trackVolume[t];
    vol = static_cast<uint8_t>(constrain(static_cast<int>(vol) + delta, 0, 127));
    drawVolRow();
    sendPatchVol();
  } else if (selectedPatchRow == static_cast<int8_t>(slotRow)) {
    patchSlot = static_cast<uint8_t>((static_cast<int>(patchSlot) + delta + kPatchSlotCount) % kPatchSlotCount);
    drawPatchSlotRow();
  } else if (selectedPatchRow < 6 && patchRowActive(t, static_cast<uint8_t>(selectedPatchRow))) {
    uint8_t &param = patchParamRef(t, static_cast<uint8_t>(selectedPatchRow));
    param = static_cast<uint8_t>(constrain(static_cast<int>(param) + delta, 0,
                                             static_cast<int>(patchRowMax(t, static_cast<uint8_t>(selectedPatchRow)))));
    drawPatchRow(static_cast<uint8_t>(selectedPatchRow));
    if (selectedPatchRow < 2) {
      sendPatchFilt();
    } else if (trackEngine[t] == az2::kEngineDexed) {
      sendPatchDxp(static_cast<uint8_t>(selectedPatchRow - 2));
    } else {
      sendPatchEnv();
    }
  } else if (selectedPatchRow >= 6) {
    const uint8_t extraIdx = static_cast<uint8_t>(selectedPatchRow - 6);
    uint8_t &val = patchExtraVal[t][extraIdx];
    val = static_cast<uint8_t>(
        constrain(static_cast<int>(val) + delta, 0, static_cast<int>(patchExtraMax(t, extraIdx))));
    drawPatchExtraRow(static_cast<uint8_t>(selectedPatchRow));
    sendPatchExtra(t, extraIdx);
  }
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
// Selection croix (demande 2026-09-17 : "je peux pas selectionner une
// rom avec la croix et A/B et avoir le nom en surbrillance") -- avant,
// la croix sur cette page etait toujours routee vers gbSetButton() (les
// boutons du JEU), meme quand aucune ROM n'etait encore chargee, donc
// aucune navigation clavier possible dans la liste.
int8_t selectedRomIndex = 0;

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
  if (index == selectedRomIndex) {
    // Ligne selectionnee par la croix -- meme convention que la piste
    // choisie sur la page MOTEURS (contour blanc double).
    gfx->drawRect(static_cast<int16_t>(kMargin + 1), static_cast<int16_t>(y + 1), kScreenSize - 2 * kMargin - 2,
                  kRomRowH - 8, RGB565_WHITE);
  }
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
      "Son du jeu route vers le DAC du Teensy.",
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
constexpr int16_t kSwingRowY = kScaleRowY + kCfgRowH + 40;

// Swing/shuffle (SWING:, priorite #3 de la liste indispensable) -- 0-127,
// pas de granularite fine cote Teensy (4 ticks/pas max, voir
// handleSwingCommand() et le commentaire de swingAmount la-bas) donc pas
// a pas de 32 ici (127/4) pour que chaque appui +/- change reellement
// quelque chose d'audible plutot que des crans invisibles.
uint8_t swingValue = 0;
constexpr uint8_t kSwingStep = 32;

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

  // Swing (voir swingValue plus haut) -- 0 = pas de swing (comportement
  // d'origine).
  gfx->fillRect(kMargin, kSwingRowY, kScreenSize - 2 * kMargin, kCfgRowH, RGB565_BLACK);
  gfx->drawRect(minusX, kSwingRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->drawRect(plusX, kSwingRowY, kCfgBtnW, kCfgRowH, kFaint);
  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(minusX + 20), static_cast<int16_t>(kSwingRowY + 10));
  gfx->print('-');
  gfx->setCursor(static_cast<int16_t>(plusX + 20), static_cast<int16_t>(kSwingRowY + 10));
  gfx->print('+');
  snprintf(buf, sizeof(buf), "%3d", swingValue);
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - 30), static_cast<int16_t>(kSwingRowY + 15));
  gfx->print(buf);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, static_cast<int16_t>(kSwingRowY - 20));
  gfx->print("SWING (0 = aucun)");
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

bool hitTestSwingMinus(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kSwingRowY, kCfgBtnW, kCfgRowH);
}
bool hitTestSwingPlus(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kScreenSize - kMargin - kCfgBtnW), kSwingRowY, kCfgBtnW, kCfgRowH);
}

// ---------------------------------------------------------------------
// Page PROJET -- sauvegarde/chargement du morceau ENTIER (patterns,
// song, tempo/division, gamme, moteur+patch+filtre+ADSR par piste),
// pas juste un patch (voir savePatchSlot()/loadPatchSlot() plus haut,
// qui ne couvrent qu'UNE piste). Demande 2026-09-16 ("qu'on puisse
// creer facilement un projet, le sauvegarder"), priorite #2 de la liste
// d'ameliorations indispensables (AZ2_BENCHMARK_CONCURRENCE.md) --
// implementee le 2026-09-17. Fichier texte simple sur la SD de l'ESP32
// (meme carte que les ROM/patches), format ligne par ligne, lisible a
// l'oeil -- "fichiers ouverts" comme le reste du projet. Toutes les
// donnees existent DEJA cote ESP32 (miroir local de l'etat du
// sequenceur) -- sauvegarder = juste les ecrire ; charger = les
// relire ET renvoyer les commandes normales au Teensy (meme principe
// que loadPatchSlot(), a plus grande echelle).
// ---------------------------------------------------------------------
constexpr uint8_t kProjectSlotCount = 4;
uint8_t projectSlot = 0;
constexpr int16_t kProjectSlotY = 120;
constexpr int16_t kProjectSlotH = 40;
constexpr int16_t kProjectBtnW = (kScreenSize - 2 * kMargin) / 3;

void drawProjectPage() {
  drawSubHeader("PROJET", kPalette[3]);

  const int16_t saveX = static_cast<int16_t>(kMargin + kProjectBtnW);
  const int16_t loadX = static_cast<int16_t>(kMargin + 2 * kProjectBtnW);
  gfx->fillRect(kMargin, kProjectSlotY, kScreenSize - 2 * kMargin, kProjectSlotH, RGB565_BLACK);
  gfx->drawRect(kMargin, kProjectSlotY, kProjectBtnW, kProjectSlotH, kFaint);
  gfx->drawRect(saveX, kProjectSlotY, kProjectBtnW, kProjectSlotH, kFaint);
  gfx->drawRect(loadX, kProjectSlotY, kProjectBtnW, kProjectSlotH, kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char buf[12];
  snprintf(buf, sizeof(buf), "SLOT %d", projectSlot);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kProjectSlotY + 10));
  gfx->print(buf);
  gfx->setTextColor(kPalette[1 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(saveX + 16), static_cast<int16_t>(kProjectSlotY + 10));
  gfx->print("SAVE");
  gfx->setTextColor(kPalette[2 % kPaletteCount]);
  gfx->setCursor(static_cast<int16_t>(loadX + 16), static_cast<int16_t>(kProjectSlotY + 10));
  gfx->print("LOAD");

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  const char *lines[] = {
      "Sauvegarde TOUT le morceau : patterns,",
      "chainage song, tempo/division, gamme,",
      "moteur/patch/filtre/ADSR de chaque piste.",
      "",
      "4 emplacements -- /projects/N.proj sur",
      "la carte SD de l'ecran.",
  };
  for (uint8_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
    gfx->setCursor(kMargin, static_cast<int16_t>(kProjectSlotY + kProjectSlotH + 20 + i * 18));
    gfx->print(lines[i]);
  }
}

bool hitTestProjectSlotNum(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kProjectSlotY, kProjectBtnW, kProjectSlotH);
}
bool hitTestProjectSlotSave(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kMargin + kProjectBtnW), kProjectSlotY, kProjectBtnW, kProjectSlotH);
}
bool hitTestProjectSlotLoad(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kMargin + 2 * kProjectBtnW), kProjectSlotY, kProjectBtnW, kProjectSlotH);
}

void saveProject(uint8_t slot) {
  SD.mkdir("/projects");
  char path[24];
  snprintf(path, sizeof(path), "/projects/%d.proj", slot);

  // Ecriture atomique (voir atomicSaveFile() plus haut, meme defaut P1
  // signale par l'audit du 2026-09-19 que savePatchSlot()) -- un
  // fichier projet est bien plus gros/long a ecrire qu'un patch, donc
  // bien plus expose a une coupure en cours de route.
  const bool ok = atomicSaveFile(path, [&](File &f) {
    f.printf("BPM:%d\n", static_cast<int>(seqBpm + 0.5f));
    f.printf("DIV:%d\n", seqStepsPerBeat);
    f.printf("SCALE:%d\n", currentScaleIndex);
    f.printf("SWING:%d\n", swingValue);
    f.printf("SONGMODE:%d\n", songMode ? 1 : 0);
    f.printf("SONGLEN:%d\n", songLen);
    for (uint8_t i = 0; i < songLen; ++i) {
      f.printf("SONGSET:%d:%d\n", i, songPatterns[i]);
    }
    for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
      // Mute inclus (13e champ) mais PAS solo -- solo est un outil de
      // monitoring live, pas une decision de composition (convention
      // habituelle DAW/mixeurs : le solo ne survit pas a une
      // sauvegarde).
      f.printf("TRACK:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", t, trackEngine[t], trackPatch[t], trackCutoff[t],
               trackReso[t], trackAttack[t], trackDecay[t], trackSustain[t], trackRelease[t], trackAlgo[t],
               trackFeedback[t], trackVolume[t], trackMuted[t] ? 1 : 0);
    }
    for (uint8_t p = 0; p < kPatternCount; ++p) {
      for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
        for (uint8_t s = 0; s < kSeqStepCount; ++s) {
          // 10 champs depuis l'ajout de PROB/COND (2026-09-17, 9e/10e
          // champs) -- voir loadProject() pour la lecture
          // retro-compatible des fichiers a 8 champs (avant cet ajout).
          f.printf("STEP:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", p, t, s, seqStepOn[p][t][s] ? 1 : 0, seqStepNote[p][t][s],
                   seqStepPatch[p][t][s], seqStepFx[p][t][s], seqStepFxVal[p][t][s], seqStepProb[p][t][s],
                   seqStepCondition[p][t][s]);
        }
      }
    }
  });
  if (!ok) {
    Serial.print("PROJECT_SAVE_ERROR:");
    Serial.println(path);
    return;
  }
  Serial.print("PROJECT_SAVED:");
  Serial.println(path);
}

// Coupe une ligne "cle:reste" -- renvoie "reste" (String vide si pas de
// ':'). Petit utilitaire local, le reste du fichier utilise deja ce
// motif partout (indexOf(':') + substring()) mais ligne par ligne ici
// simplifie la lecture du fichier projet.
String afterColon(const String &line) {
  const int i = line.indexOf(':');
  return (i < 0) ? String("") : line.substring(i + 1);
}

void loadProject(uint8_t slot) {
  char path[24];
  snprintf(path, sizeof(path), "/projects/%d.proj", slot);
  File f = SD.open(path);
  if (!f) {
    Serial.print("PROJECT_LOAD_EMPTY:");
    Serial.println(path);
    return;
  }

  char msg[32];
  // PAS static -- doit repartir de -1 a CHAQUE appel de loadProject(),
  // sinon un second chargement dont le premier pattern coinciderait
  // avec le dernier pattern du fichier precedent sauterait a tort le
  // PATTERN: initial (bug trouve a la relecture avant de flasher).
  int8_t lastPattern = -1;
  while (f.available()) {
    const String line = f.readStringUntil('\n');
    if (line.startsWith("BPM:")) {
      const int bpm = afterColon(line).toInt();
      snprintf(msg, sizeof(msg), "BPM:%d", bpm);
      sendToTeensy(msg);
    } else if (line.startsWith("DIV:")) {
      const int div = afterColon(line).toInt();
      snprintf(msg, sizeof(msg), "DIV:%d", div);
      sendToTeensy(msg);
    } else if (line.startsWith("SCALE:")) {
      currentScaleIndex = static_cast<uint8_t>(afterColon(line).toInt() % kScaleCount);
    } else if (line.startsWith("SWING:")) {
      // Absent des projets sauvegardes avant cet ajout -- swingValue
      // reste a sa valeur courante (0 par defaut) dans ce cas, meme
      // esprit que les champs optionnels des lignes TRACK:.
      swingValue = static_cast<uint8_t>(constrain(afterColon(line).toInt(), 0, 127));
      snprintf(msg, sizeof(msg), "SWING:%d", swingValue);
      sendToTeensy(msg);
    } else if (line.startsWith("SONGMODE:")) {
      const int v = afterColon(line).toInt();
      snprintf(msg, sizeof(msg), "SONGMODE:%d", v);
      sendToTeensy(msg);
    } else if (line.startsWith("SONGLEN:")) {
      const int v = afterColon(line).toInt();
      snprintf(msg, sizeof(msg), "SONGLEN:%d", v);
      sendToTeensy(msg);
    } else if (line.startsWith("SONGSET:")) {
      const String rest = afterColon(line);
      const int c = rest.indexOf(':');
      if (c >= 0) {
        snprintf(msg, sizeof(msg), "SONGSET:%s:%s", rest.substring(0, c).c_str(), rest.substring(c + 1).c_str());
        sendToTeensy(msg);
      }
    } else if (line.startsWith("TRACK:")) {
      // 13 champs depuis l'ajout de MUTE (12e = volume, 13e = mute) --
      // fichiers a 11 ou 12 champs (avant ces ajouts) restent lisibles,
      // les champs manquants gardent leur valeur courante par defaut.
      int vals[13] = {};
      int idx = 0, start = 0;
      const String rest = afterColon(line);
      for (int i = 0; i <= rest.length() && idx < 13; ++i) {
        if (i == rest.length() || rest.charAt(i) == ',') {
          vals[idx++] = rest.substring(start, i).toInt();
          start = i + 1;
        }
      }
      if (idx >= 11) {
        const int t = vals[0];
        snprintf(msg, sizeof(msg), "ENGINE:%d:%d", t, vals[1]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "PATCH:%d:%d", t, vals[2]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "FILT:%d:%d:%d", t, vals[3], vals[4]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "ENV:%d:%d:%d:%d:%d", t, vals[5], vals[6], vals[7], vals[8]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "DXP:%d:0:%d", t, vals[9]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "DXP:%d:1:%d", t, vals[10]);
        sendToTeensy(msg);
        if (idx >= 12) {
          snprintf(msg, sizeof(msg), "VOL:%d:%d", t, vals[11]);
          sendToTeensy(msg);
        }
        if (idx >= 13) {
          snprintf(msg, sizeof(msg), "MUTE:%d:%d", t, vals[12]);
          sendToTeensy(msg);
        }
      }
    } else if (line.startsWith("STEP:")) {
      // 10 champs depuis l'ajout de PROB/COND (2026-09-17) -- 8 champs
      // acceptes aussi (fichiers sauvegardes avant cet ajout), prob/cond
      // gardent alors leur valeur courante deja seedee (100/0).
      int vals[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
      int idx = 0, start = 0;
      const String rest = afterColon(line);
      for (int i = 0; i <= rest.length() && idx < 10; ++i) {
        if (i == rest.length() || rest.charAt(i) == ',') {
          vals[idx++] = rest.substring(start, i).toInt();
          start = i + 1;
        }
      }
      if (idx == 8 || idx == 10) {
        const uint8_t p = static_cast<uint8_t>(vals[0]);
        const uint8_t t = static_cast<uint8_t>(vals[1]);
        const uint8_t s = static_cast<uint8_t>(vals[2]);
        // STEP:/NOTE:/INST:/SFX:/PROB:/COND: (protocole existant) sont
        // tous par pattern COURANT cote Teensy -- il faut d'abord
        // basculer sur le pattern p, sinon on ecrirait dans le mauvais
        // pattern. Le fichier est trie par pattern croissant (voir
        // saveProject()), donc un simple "si different du dernier"
        // suffit, pas besoin de detecter les sauts.
        if (p != lastPattern) {
          snprintf(msg, sizeof(msg), "PATTERN:%d", p);
          sendToTeensy(msg);
          lastPattern = p;
        }
        const int prob = (idx == 10) ? vals[8] : 100;
        const int cond = (idx == 10) ? vals[9] : 0;
        if (p < kPatternCount && t < kSeqTrackCount && s < kSeqStepCount) {
          seqStepOn[p][t][s] = vals[3] != 0;
          seqStepNote[p][t][s] = static_cast<uint8_t>(vals[4]);
          seqStepPatch[p][t][s] = static_cast<uint8_t>(vals[5]);
          seqStepFx[p][t][s] = static_cast<uint8_t>(vals[6]);
          seqStepFxVal[p][t][s] = static_cast<uint8_t>(vals[7]);
          seqStepProb[p][t][s] = static_cast<uint8_t>(prob);
          seqStepCondition[p][t][s] = static_cast<uint8_t>(cond);
        }
        snprintf(msg, sizeof(msg), "STEP:%d:%d:%d", t, s, vals[3]);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", t, s, vals[4]);
        sendToTeensy(msg);
        // Toujours envoye, MEME 255 (0xFF = "pas d'override, patch par
        // defaut de la piste") -- bug reel trouve par l'audit du
        // 2026-09-19 : sauter l'envoi quand vals[5]==0xFF laissait
        // l'ancienne valeur du Teensy en place si la memoire courante
        // avait deja un override sur ce pas, rendant le chargement
        // d'un projet potentiellement different du projet sauvegarde.
        snprintf(msg, sizeof(msg), "INST:%d:%d:%d", t, s, vals[5]);
        sendToTeensy(msg);
        if (idx == 10) {
          snprintf(msg, sizeof(msg), "PROB:%d:%d:%d", t, s, prob);
          sendToTeensy(msg);
          snprintf(msg, sizeof(msg), "COND:%d:%d:%d", t, s, cond);
          sendToTeensy(msg);
        }
        snprintf(msg, sizeof(msg), "SFX:%d:%d:%d:%d", t, s, vals[6], vals[7]);
        sendToTeensy(msg);
      }
    }
  }
  f.close();
  // Repart sur le pattern 0, comme au demarrage -- evite de rester
  // affiche sur le dernier pattern du fichier charge sans le vouloir.
  snprintf(msg, sizeof(msg), "PATTERN:%d", 0);
  sendToTeensy(msg);
  Serial.print("PROJECT_LOADED:");
  Serial.println(path);
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
    case Screen::Project: drawProjectPage(); return;
  }
}

void goTo(Screen s) {
  // La sortie de la page Jeux doit etre annulee si la carte SD refuse
  // la sauvegarde : conserver le jeu en RAM et la navigation intacte.
  if (s != Screen::Retro && gbIsLoaded() && !gbUnload()) {
    Serial.println("GB:NAV_BLOCKED_UNSAVED_RAM");
    return;
  }
  // Memorise d'ou on vient (voir navPrevious plus haut) -- AVANT tout
  // le reste, pour que meme un "retour" (goTo(navPrevious)) enregistre
  // correctement l'etape precedente (permet de faire l'aller-retour
  // entre 2 pages avec le bouton retour, pas seulement "une fois").
  // Garde inutile si s == currentScreen (pas un vrai changement de
  // page, ex: re-goTo() sur l'ecran deja affiche).
  if (s != currentScreen) {
    navPrevious = currentScreen;
  }

  // Rescanne /games et decharge la ROM GB en entrant/sortant de la page
  // JEUX (voir gb_emulator.h) -- libere la PSRAM des qu'on quitte, evite
  // de garder une ROM chargee inutilement sur les autres pages. Le scan
  // remplit gbRomNames[]/gbRomCount, affiches en liste par
  // drawRetroPage() (demande 2026-09-15 : "il nous faut un menu ...
  // dans une liste de rom pas uniquement un jeux").
  if (s == Screen::Retro && !gbIsLoaded()) {
    gbRomCount = gbScanRoms(gbRomNames);
    gbRomScroll = 0;
    selectedRomIndex = 0;
  }

  // Page MOTEURS (2026-09-19) : aligne le defilement de la liste PATCH
  // sur le patch REELLEMENT charge de la piste affichee -- sinon un
  // defilement laisse par une visite precedente (piste/moteur
  // differents) pourrait ne plus rien montrer de pertinent en entrant.
  if (s == Screen::Engines) {
    const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
    const uint8_t patch = trackPatch[t];
    engPatchScroll = (patch < kEngListVisibleRows) ? 0 : static_cast<uint16_t>(patch - kEngListVisibleRows + 1);
    engOnTrackRow = false;  // atterrit dans la liste MOTEUR, pas sur la ligne PISTE
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
    // Repart toujours du haut de la page/de la fenetre en y entrant
    // (2026-09-18, lignes extra -- voir patchScroll()) et relit les
    // valeurs courantes du moteur affiche (sinon 0 partout tant que
    // l'utilisateur n'a pas lui-meme touche chaque ligne au moins une
    // fois, voir queryPatchExtra()).
    selectedPatchRow = 0;
    patchScroll = 0;
    patchOnTrackRow = false;  // atterrit dans la grille de parametres, pas sur la ligne PISTE
    queryPatchExtra(static_cast<uint8_t>(patchTrack));
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
        // Page JEUX, partie en cours : la croix pilote directement le
        // Game Boy (voir gb_emulator.h -- GbButton::Up/Down/Left/Right
        // sont dans le meme ordre que index ici, 0-3).
        if (currentScreen == Screen::Retro && gbIsLoaded()) {
          gbSetButton(static_cast<GbButton>(index), pressed);
        }
        // Page JEUX, liste de ROM (pas encore charge) : HAUT/BAS
        // deplacent la selection surlignee (voir selectedRomIndex plus
        // haut, demande 2026-09-17 -- "je peux pas selectionner une rom
        // avec la croix et avoir le nom en surbrillance"). Suit
        // automatiquement le defilement si la selection sort de la
        // fenetre visible (kRomVisibleRows).
        if (pressed && currentScreen == Screen::Retro && !gbIsLoaded() && gbRomCount > 0 &&
            (index == 0 || index == 1)) {
          const int8_t previous = selectedRomIndex;
          if (index == 1 && selectedRomIndex < gbRomCount - 1) {
            ++selectedRomIndex;
          } else if (index == 0 && selectedRomIndex > 0) {
            --selectedRomIndex;
          }
          if (selectedRomIndex != previous) {
            if (selectedRomIndex < gbRomScroll) {
              gbRomScroll = static_cast<uint8_t>(selectedRomIndex);
              drawRetroPage();
            } else if (selectedRomIndex >= gbRomScroll + kRomVisibleRows) {
              gbRomScroll = static_cast<uint8_t>(selectedRomIndex - kRomVisibleRows + 1);
              drawRetroPage();
            } else {
              drawRomRow(static_cast<uint8_t>(previous));
              drawRomRow(static_cast<uint8_t>(selectedRomIndex));
            }
          }
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
        // Page MOTEURS repensee UNE 2e FOIS le 2026-09-19 (retour
        // utilisateur sur materiel reel : "on est un peu dans le
        // desordre de controle ... il faut selectionner l'encadre
        // piste et droite gauche [pour changer de piste]") -- le
        // premier essai (A+GAUCHE/DROITE pour le focus, comme la ligne
        // SLOT de la page PATCH) portait a confusion ici, pas assez
        // evident. Nouveau modele SANS modificateur A : GAUCHE/DROITE
        // bascule TOUJOURS le focus entre liste MOTEUR et liste PATCH
        // quand on est DANS une de ces 2 listes (naturel, elles sont
        // cote a cote a l'ecran) ; pour changer de PISTE, il faut
        // d'abord remonter (HAUT) jusqu'a "sortir" de la liste -- la
        // ligne PISTE en haut devient alors le point selectionne
        // (engOnTrackRow), et GAUCHE/DROITE y change la piste. BAS
        // depuis la ligne PISTE redescend dans la liste au focus.
        if (pressed && currentScreen == Screen::Engines) {
          const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
          if (engOnTrackRow) {
            if (index == 2 || index == 3) {
              selectedEngineTrack = static_cast<int8_t>(
                  (selectedEngineTrack + (index == 3 ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
              engPatchScroll = 0;
              drawEnginesPage();
            } else if (index == 1) {  // BAS -- entre dans la liste au focus
              engOnTrackRow = false;
              drawEnginesPage();
            }
            // HAUT : deja tout en haut, no-op.
          } else if (index == 2 || index == 3) {
            engineColPatch = !engineColPatch;
            drawEnginesPage();
          } else if (index == 0 || index == 1) {
            const int dir = (index == 0) ? -1 : 1;
            if (!engineColPatch) {
              // Liste MOTEUR : PAS de bouclage -- HAUT depuis le tout
              // premier moteur (index 0) remonte a la ligne PISTE au
              // lieu de boucler sur le dernier moteur (SAMPLER),
              // sinon aucun moyen d'atteindre la ligne PISTE au clavier.
              if (dir < 0 && trackEngine[t] == 0) {
                engOnTrackRow = true;
                drawEnginesPage();
              } else {
                const int nextEngine = constrain(static_cast<int>(trackEngine[t]) + dir, 0, az2::kEngineCount - 1);
                char msg[16];
                snprintf(msg, sizeof(msg), "ENGINE:%d:%d", t, nextEngine);
                sendToTeensy(msg);
              }
            } else {
              // Liste PATCH : meme principe -- pas de bouclage, HAUT
              // depuis le patch 0 remonte a la ligne PISTE.
              if (dir < 0 && trackPatch[t] == 0) {
                engOnTrackRow = true;
                drawEnginesPage();
              } else {
                const uint16_t count = az2::enginePatchCount(trackEngine[t]);
                const int nextPatch = constrain(static_cast<int>(trackPatch[t]) + dir, 0, static_cast<int>(count) - 1);
                // Fait suivre le defilement si la nouvelle selection
                // sort de la fenetre visible -- meme principe que
                // patchScroll sur la page PATCH.
                if (nextPatch < engPatchScroll) {
                  engPatchScroll = static_cast<uint16_t>(nextPatch);
                } else if (nextPatch >= engPatchScroll + kEngListVisibleRows) {
                  engPatchScroll = static_cast<uint16_t>(nextPatch - kEngListVisibleRows + 1);
                }
                char msg[16];
                snprintf(msg, sizeof(msg), "PATCH:%d:%d", t, nextPatch);
                sendToTeensy(msg);
              }
            }
          }
        }
        // Sur la page SEQUENCEUR (vue tracker, voir seqDetailMode plus
        // haut), la croix edite le pas selectionne -- demande le
        // 2026-09-14 ("prend le sequenceur du dexed touch"), etendue le
        // 2026-09-15 ("un tracker 8 pistes"), puis PROB/COND le 2026-09-17.
        // Roles inverses le 2026-09-18 (retour utilisateur sur materiel reel,
        // "faut un bouton pour changer les valeurs et descendre avec les
        // fleches") : HAUT/BAS SEULS deplacent le pas selectionne (l'action
        // la plus frequente, disponible sans rien maintenir). A maintenu +
        // HAUT/BAS edite la valeur a la place -- **A**, pas C : plusieurs
        // captures serie sur le vrai materiel montrent BTN:A:DOWN/UP a
        // chaque appui, mais AUCUN BTN:C:* n'est jamais arrive malgre
        // plusieurs essais utilisateur -- bouton C probablement pas
        // fonctionnel sur ce montage (a netement verifier au multimetre/
        // continuite plus tard, pas urgent : A suffit). GAUCHE/DROITE
        // choisissent toujours la colonne (NOTE/INST/FX/VAL/PROB/COND),
        // inchange.
        if (pressed && currentScreen == Screen::Sequencer && selectedSeqTrack >= 0 && selectedSeqStep >= 0) {
          const uint8_t t = static_cast<uint8_t>(selectedSeqTrack);
          const uint8_t s = static_cast<uint8_t>(selectedSeqStep);
          {
            if (index == 2 || index == 3) {
              const int8_t prevCol = seqDetailCol;
              seqDetailCol = static_cast<int8_t>((seqDetailCol + (index == 3 ? 1 : 5)) % 6);
              if (seqDetailCol != prevCol) {
                drawDetailRow(s);
              }
            } else if ((index == 0 || index == 1) && !btnState[0]) {
              const int8_t prevStep = selectedSeqStep;
              const int8_t delta = (index == 0) ? -1 : 1;
              selectedSeqStep = static_cast<int8_t>(
                  constrain(static_cast<int>(selectedSeqStep) + delta, 0, static_cast<int>(kSeqStepCount) - 1));
              if (selectedSeqStep != prevStep) {
                drawDetailRow(static_cast<uint8_t>(prevStep));
                drawDetailRow(static_cast<uint8_t>(selectedSeqStep));
              }
            } else if (index == 0 || index == 1) {
              const int dir = (index == 0) ? 1 : -1;
              char msg[24];
              switch (seqDetailCol) {
                case 0: {
                  // Bug reel trouve le 2026-09-18 sur materiel : la colonne
                  // NOTE n'affiche jamais rien tant que le pas est OFF
                  // (drawDetailRow() force "---"), et rien au bouton/croix
                  // ne permettait d'allumer un pas (seul un tap tactile sur
                  // la ligne le faisait) -- editer semblait "ne rien faire".
                  // Fix : poser une note allume automatiquement le pas,
                  // comme dans tout vrai tracker (LSDJ/M8 : entrer une note
                  // active la ligne).
                  if (!seqStepOn[currentPattern][t][s]) {
                    snprintf(msg, sizeof(msg), "STEP:%d:%d:1", t, s);
                    sendToTeensy(msg);
                  }
                  const uint8_t newNote = nextNoteInScale(seqStepNote[currentPattern][t][s], static_cast<int8_t>(dir));
                  snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", t, s, newNote);
                  sendToTeensy(msg);
                  break;
                }
                case 1: {
                  const uint16_t count = az2::enginePatchCount(trackEngine[t]);  // uint16_t depuis DEXED=256 patches (2026-09-18)
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
                case 3: {
                  const int newVal = constrain(static_cast<int>(seqStepFxVal[currentPattern][t][s]) + dir, 0, 255);
                  snprintf(msg, sizeof(msg), "SFX:%d:%d:%d:%d", t, s, seqStepFx[currentPattern][t][s], newVal);
                  sendToTeensy(msg);
                  break;
                }
                case 4: {
                  // PROB (2026-09-17) -- +-1%, meme granularite "1 unite par
                  // pression" que VAL ci-dessus.
                  const int newProb = constrain(static_cast<int>(seqStepProb[currentPattern][t][s]) + dir, 0, 100);
                  snprintf(msg, sizeof(msg), "PROB:%d:%d:%d", t, s, newProb);
                  sendToTeensy(msg);
                  break;
                }
                default: {
                  // COND (2026-09-17) -- cycle dans az2::kStepConditionCycle
                  // (pas un increment brut d'octet, la plupart des octets ne
                  // sont pas des conditions valides -- voir AZ2_Protocol.h).
                  const uint8_t current = seqStepCondition[currentPattern][t][s];
                  int8_t curIdx = 0;
                  for (uint8_t i = 0; i < az2::kStepConditionCycleCount; ++i) {
                    if (az2::kStepConditionCycle[i] == current) {
                      curIdx = static_cast<int8_t>(i);
                      break;
                    }
                  }
                  const int8_t nextIdx = static_cast<int8_t>(
                      (curIdx + dir + az2::kStepConditionCycleCount) % az2::kStepConditionCycleCount);
                  snprintf(msg, sizeof(msg), "COND:%d:%d:%d", t, s, az2::kStepConditionCycle[nextIdx]);
                  sendToTeensy(msg);
                  break;
                }
              }
            }
          }
        }
        // Page PATCH : navigation repensee le 2026-09-19 (retour
        // utilisateur sur le chantier "2 reglages par ligne" : "la
        // droite gauche change les piste il faut que ce change les
        // reglage selectionner dans la fenetre des patch") -- meme
        // modele que la page MOTEURS ci-dessus (patchOnTrackRow miroir
        // d'engOnTrackRow) : GAUCHE/DROITE parcourent desormais les
        // reglages (patchStepLogical(), colonne gauche/droite d'une
        // meme ligne visuelle puis ligne suivante), HAUT/BAS montent/
        // descendent d'une ligne visuelle en gardant la colonne
        // (patchStepVisual()). La ligne PISTE (ou GAUCHE/DROITE change
        // vraiment de piste, comme avant ce chantier) ne s'atteint plus
        // qu'en montant depuis la toute premiere ligne. A maintenu +
        // HAUT/BAS OU A maintenu + GAUCHE/DROITE editent tous les deux
        // la valeur selectionnee (2026-09-19, suite : "on devrait
        // pouvoir changer les valeurs ... en maintenant A et en
        // pressant droite/gauche [en plus de haut/bas]" -- meme
        // fonction patchApplyDelta(), simple confort d'avoir le choix
        // du sens de croix). Ligne SLOT : A maintenu + GAUCHE/DROITE
        // reste SAVE/LOAD (verifie AVANT le cas general ci-dessous),
        // seule ligne ou GAUCHE/DROITE garde un role hors edition.
        if (pressed && currentScreen == Screen::Patch) {
          const uint8_t t = static_cast<uint8_t>(patchTrack);
          const uint8_t total = patchTotalRows(t);
          const uint8_t slotRow = patchSlotRow(t);
          if (btnState[0] && !patchOnTrackRow && selectedPatchRow == static_cast<int8_t>(slotRow) &&
              (index == 2 || index == 3)) {
            if (index == 3) {
              savePatchSlot(patchSlot);
            } else {
              loadPatchSlot(patchSlot);
            }
          } else if (patchOnTrackRow) {
            if (index == 2 || index == 3) {
              patchTrack = static_cast<int8_t>((patchTrack + (index == 3 ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
              scopeHasData = false;
              // Piste differente = potentiellement un moteur different,
              // donc un nombre de lignes different -- repart du haut de
              // la fenetre plutot que de garder une selection/un
              // defilement qui ne correspondrait plus a rien.
              selectedPatchRow = 0;
              patchScroll = 0;
              char msg[12];
              snprintf(msg, sizeof(msg), "SCOPE:%d", patchTrack);
              sendToTeensy(msg);
              queryPatchExtra(static_cast<uint8_t>(patchTrack));
              drawPatchPage();
            } else if (index == 1) {  // BAS -- entre dans la grille au focus
              patchOnTrackRow = false;
              drawPatchTrackRow();
              redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
            }
            // HAUT : deja tout en haut, no-op.
          } else if ((index == 2 || index == 3) && !btnState[0]) {
            const int8_t prevRow = selectedPatchRow;
            selectedPatchRow = patchStepLogical(t, selectedPatchRow, (index == 3) ? 1 : -1, total);
            if (selectedPatchRow != prevRow) {
              // Fait suivre le defilement (en lignes VISUELLES) si la
              // nouvelle selection sort de la fenetre visible -- meme
              // principe que selectedRomIndex/gbRomScroll pour la
              // liste de ROM.
              bool scrolled = false;
              const int16_t nextVisual = patchVisualRow(t, static_cast<uint8_t>(selectedPatchRow));
              if (nextVisual < patchScroll) {
                patchScroll = static_cast<uint8_t>(nextVisual);
                scrolled = true;
              } else if (nextVisual >= patchScroll + kPatchVisibleRows) {
                patchScroll = static_cast<uint8_t>(nextVisual - kPatchVisibleRows + 1);
                scrolled = true;
              }
              if (scrolled) {
                drawPatchWindow();
              } else {
                redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
                redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
              }
            }
          } else if ((index == 0 || index == 1) && !btnState[0]) {
            const int8_t prevRow = selectedPatchRow;
            const int8_t next = patchStepVisual(t, selectedPatchRow, (index == 0) ? -1 : 1);
            if (next < 0) {
              patchOnTrackRow = true;
              drawPatchTrackRow();
              redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
            } else if (next != prevRow) {
              selectedPatchRow = next;
              bool scrolled = false;
              const int16_t nextVisual = patchVisualRow(t, static_cast<uint8_t>(selectedPatchRow));
              if (nextVisual < patchScroll) {
                patchScroll = static_cast<uint8_t>(nextVisual);
                scrolled = true;
              } else if (nextVisual >= patchScroll + kPatchVisibleRows) {
                patchScroll = static_cast<uint8_t>(nextVisual - kPatchVisibleRows + 1);
                scrolled = true;
              }
              if (scrolled) {
                drawPatchWindow();
              } else {
                redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
                redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
              }
            }
          } else {
            // A maintenu (et pas le cas SLOT+GAUCHE/DROITE deja capte
            // plus haut) : edite la valeur de la ligne selectionnee,
            // HAUT/DROITE = +1, BAS/GAUCHE = -1 -- voir patchApplyDelta()
            // et le commentaire au-dessus de ce bloc.
            if (index == 0 || index == 1) {
              patchApplyDelta(t, (index == 0) ? 1 : -1);
            } else {
              patchApplyDelta(t, (index == 3) ? 1 : -1);
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
      if (inGbGame && index < 2) {
        static const GbButton kGbMap[2] = {GbButton::A, GbButton::B};
        gbSetButton(kGbMap[index], pressed);
      }
      // Page JEUX, liste de ROM (pas encore charge) : A charge la ROM
      // choisie par la croix -- meme convention que le tactile
      // (toucher une ligne), et que A pour confirmer ailleurs (menu).
      if (pressed && letter == 'A' && currentScreen == Screen::Retro && !gbIsLoaded() && gbRomCount > 0) {
        if (gbLoadRom(gbRomNames[selectedRomIndex])) {
          drawRetroPage();
        }
      }
      // Sortir d'une partie : demande 2026-09-17 ("il faut un truc pour
      // sortir de l'emulateur cote code") -- B est deja pris par le jeu
      // (voir plus bas) donc pas utilisable comme "retour" ici. La Game
      // Boy d'origine n'a pas de boutons L/R : C et D restent donc
      // libres meme en pleine partie (voir AZ2_TODO_PICO.md, "gachette"
      // jamais assignee) -- C sert a quitter proprement (goTo()
      // sauvegarde la RAM cartouche via gbUnload() avant de liberer la
      // ROM, meme chemin que changer de page) -- vers navPrevious
      // (2026-09-19, meme convention "retour d'un etage" que partout
      // ailleurs desormais), pas force au menu.
      if (pressed && letter == 'C' && inGbGame) {
        goTo(navPrevious);
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
      // Page MOTEURS (2026-09-19) : A ouvre la page PATCH complete pour
      // le patch actuellement selectionne -- demande explicite ("si on
      // en selectionne un [patch] il faut que quand on presse A ca
      // envoie aux reglages du patch"). Seulement quand le focus est
      // sur la liste PATCH (pas sur MOTEUR ni sur la ligne PISTE, ou A
      // n'aurait pas de sens ici) -- meme action que toucher le
      // bandeau mini-reglages en bas (voir hitTestEngMini()).
      if (pressed && letter == 'A' && currentScreen == Screen::Engines && !engOnTrackRow && engineColPatch) {
        patchTrack = selectedEngineTrack;
        scopeHasData = false;
        goTo(Screen::Patch);
      }
      // Bouton RETOUR : C (2026-09-19, "le bouton retour on le met sur
      // c c'est plus cool moins tendance a appuyer dessus" -- deplace
      // de B, confirme fonctionnel sur le vrai materiel : "tout les
      // bouton marche deja"). Revient d'UN SEUL ETAGE (navPrevious),
      // PAS force au menu general ("il faut pas que ca revienne au
      // menu general"). Exclu en pleine partie GB (C = quitter
      // proprement, voir plus haut -- meme esprit, juste un chemin
      // dedie qui sauvegarde la RAM cartouche avant).
      //
      // Dans une sous-liste du menu : cas particulier, revient a la
      // grille de categories (deja "un etage", pas besoin de
      // navPrevious ici -- on reste dans Screen::Menu).
      if (pressed && letter == 'C' && currentScreen == Screen::Menu && menuCategory >= 0) {
        menuCategory = -1;
        menuSelected = 0;
        drawMenu();
      }
      if (pressed && letter == 'C' && currentScreen != Screen::Menu && !inGbGame) {
        goTo(navPrevious);
      }
      // Page PATCH : B joue/coupe une note de test DIRECTEMENT sur la
      // piste affichee (2026-09-19, "il faut utiliser le bouton B pour
      // jouer une note qu'on entende les modifications") -- indispensable
      // pour entendre l'effet d'un reglage en cours d'edition sans
      // devoir lancer PLAY/le sequenceur. Note fixe (MIDI 60 = C4,
      // meme note utilisee pour tous les tests manuels ce soir) ;
      // TIENT tant que B reste enfonce (relachement = TEST:...:0,
      // jamais filtre par ecran -- meme raison que FILL: sur D plus
      // bas : si on change de page en gardant B enfonce, la note ne
      // doit pas rester bloquee "on").
      if (letter == 'B' && currentScreen == Screen::Patch) {
        char msg[16];
        snprintf(msg, sizeof(msg), "TEST:%d:60:%d", patchTrack, pressed ? 1 : 0);
        sendToTeensy(msg);
      } else if (letter == 'B' && !pressed) {
        // Relachement B ailleurs qu'en pleine page PATCH -- coupe quand
        // meme au cas ou on aurait change de page en gardant B enfonce
        // (meme garde-fou que FILL: sur D). patchTrack, pas
        // selectedSeqTrack/selectedEngineTrack : c'est la piste de la
        // page PATCH qui a pu recevoir un TEST:...:1 juste avant.
        char msg[16];
        snprintf(msg, sizeof(msg), "TEST:%d:60:0", patchTrack);
        sendToTeensy(msg);
      }
      // Page AUDIO : D bascule le clavier tactile entre "jouer en
      // direct" et "poser la note sur le pas selectionne du
      // sequenceur" (voir padEditsStep plus haut, etape 6 de
      // AZ2_TRACKER_ETUDE.md).
      if (pressed && letter == 'D' && currentScreen == Screen::Audio) {
        padEditsStep = !padEditsStep;
        drawAudioPage();
      }
      // Page SEQUENCEUR : D maintenu = "fill" (2026-09-17, voir
      // kStepCondFill/fillActive cote Teensy) -- D est deja pris sur
      // AUDIO/MOTEURS (ci-dessus/ci-dessous), mais LIBRE sur SEQUENCEUR
      // (la croix gere colonne/valeur, aucune lettre n'y est mappee).
      // D'abord cable directement cote Teensy (plus reactif), mais ca
      // entrait en collision avec les 2 usages ci-dessus (D restait lu
      // par le Teensy quelle que soit la page affichee a l'ecran) --
      // deplace ici, scope par ecran, pour ne modifier fillActive QUE
      // sur la page ou "fill" a un sens. RELACHEMENT jamais filtre par
      // ecran (contrairement a l'appui) : si on change de page en
      // gardant D enfonce (ex: B vers le menu), le relachement doit
      // quand meme eteindre fillActive, sinon il resterait bloque a
      // "actif" jusqu'au prochain appui+relachement sur SEQUENCEUR.
      if (letter == 'D') {
        if (pressed && currentScreen == Screen::Sequencer) {
          sendToTeensy("FILL:1");
        } else if (!pressed) {
          sendToTeensy("FILL:0");  // no-op cote Teensy si fillActive etait deja false
        }
      }
      // Page MOTEURS : D bascule SOLO pour la piste choisie (priorite
      // #1 de la liste indispensable). C bascule MUTE avant le
      // 2026-09-19 -- retire, C est desormais le bouton RETOUR global
      // (voir plus haut), le conflit aurait ouvert/coupe une piste par
      // erreur a chaque "retour" depuis cette page. MUTE reste
      // accessible par serie (MUTE:<piste>:<0|1>) mais n'a plus de
      // raccourci dedie ici pour l'instant -- pas plus grave que
      // l'absence d'indicateur visuel M/S depuis la refonte de cette
      // page (voir plus haut), a rouvrir ensemble si besoin.
      if (pressed && currentScreen == Screen::Engines && letter == 'D') {
        const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
        trackSoloed[t] = !trackSoloed[t];
        char msg[12];
        snprintf(msg, sizeof(msg), "SOLO:%d:%d", t, trackSoloed[t] ? 1 : 0);
        sendToTeensy(msg);
        drawEngRow(t);
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
        if (!screensaverActive) {
          drawPotToast(index);
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
  } else if (line.startsWith("PROB:")) {
    // Colonne PROB du tracker (voir handleProbCommand() cote Teensy).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t prob = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && step < kSeqStepCount && prob <= 100) {
        seqStepProb[currentPattern][track][step] = prob;
        if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawDetailRow(step);
        }
      }
    }
  } else if (line.startsWith("COND:")) {
    // Colonne COND du tracker (voir handleCondCommand() cote Teensy et
    // az2::stepConditionEncode()/stepConditionLabel() pour l'encodage).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t step = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t cond = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && step < kSeqStepCount) {
        seqStepCondition[currentPattern][track][step] = cond;
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
  } else if (line.startsWith("METRO:")) {
    metronomeOn = line.substring(6).toInt() != 0;
    if (currentScreen == Screen::Sequencer && !screensaverActive) {
      drawTrkControls();
    }
  } else if (line.startsWith("DIV:")) {
    const uint8_t value = static_cast<uint8_t>(line.substring(4).toInt());
    if (value > 0) {
      seqStepsPerBeat = value;
      if (currentScreen == Screen::Sequencer && !screensaverActive) {
        drawTrkControls();
      }
    }
  } else if (line.startsWith("SWING:")) {
    swingValue = static_cast<uint8_t>(constrain(line.substring(6).toInt(), 0, 127));
    if (currentScreen == Screen::Config && !screensaverActive) {
      drawConfigPage();
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
          // Nouveau moteur = nombre de patches different -- repart du
          // haut de la liste PATCH plutot que de garder un defilement
          // qui ne correspondrait plus a rien (voir drawEngPatchRow()).
          if (track == selectedEngineTrack) {
            engPatchScroll = 0;
          }
          drawEngRow(track);
        } else if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          // Les lignes 2-5 changent de sens selon le moteur (ADSR vs
          // ALGO/FEEDBACK Dexed, voir patchRowLabel()), ET le nombre de
          // lignes extra change aussi (2026-09-18, voir
          // patchExtraCount()) -- repart du haut plutot que de garder
          // une selection/un defilement qui ne correspondrait plus a
          // rien pour ce nouveau moteur, puis relit ses valeurs.
          selectedPatchRow = 0;
          patchScroll = 0;
          queryPatchExtra(track);
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
          // Fait suivre le defilement de la liste PATCH si besoin --
          // meme si ce changement vient d'ailleurs que cette page (ex:
          // colonne INST du tracker), la liste doit rester coherente
          // avec ce qui est reellement charge des qu'on y revient.
          if (track == selectedEngineTrack &&
              (patch < engPatchScroll || patch >= engPatchScroll + kEngListVisibleRows)) {
            engPatchScroll = (patch < kEngListVisibleRows) ? 0 : static_cast<uint16_t>(patch - kEngListVisibleRows + 1);
          }
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
  } else if (line.startsWith("VOL:")) {
    // Echo du volume par piste (voir handleVolCommand() cote Teensy).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t vol = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (track < kSeqTrackCount) {
        trackVolume[track] = vol;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawVolRow();
        }
      }
    }
  } else if (line.startsWith("MUTE:") || line.startsWith("SOLO:")) {
    // Echo mute/solo (voir handleMuteCommand()/handleSoloCommand() cote
    // Teensy) -- garde trackMuted[]/trackSoloed[] a jour meme si le
    // changement vient d'ailleurs (chargement de projet, par exemple).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const bool on = line.substring(i2 + 1).toInt() != 0;
      if (track < kSeqTrackCount) {
        if (line.startsWith("MUTE:")) {
          trackMuted[track] = on;
        } else {
          trackSoloed[track] = on;
        }
        if (currentScreen == Screen::Engines && !screensaverActive) {
          drawEngRow(track);
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
  } else if (line.startsWith("DXR:")) {
    // Echo/reponse des lignes extra DEXED (voir handleDexedRawCommand()
    // cote Teensy -- DXR:<piste>:<octet brut>:<valeur>, meme forme pour
    // une ecriture confirmee et une reponse a DXR?). Retrouve
    // l'extraIdx correspondant a cet octet brut (kDexedExtraRaw[]) --
    // un octet qui n'y figure pas (algo/feedback/nom) est simplement
    // ignore ici, deja gere par DXP: ou hors scope de l'editeur.
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t rawByte = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t value = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount) {
        for (uint8_t extraIdx = 0; extraIdx < 17; ++extraIdx) {
          if (kDexedExtraRaw[extraIdx] == rawByte) {
            patchExtraVal[track][extraIdx] = value;
            if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
              drawPatchExtraRow(static_cast<uint8_t>(6 + extraIdx));
            }
            break;
          }
        }
      }
    }
  } else if (line.startsWith("EXP:")) {
    // Echo/reponse des lignes extra EPIANO (voir
    // handleEPianoParamCommand() cote Teensy).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t index = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t value = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && index < 12) {
        patchExtraVal[track][index] = value;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchExtraRow(static_cast<uint8_t>(6 + index));
        }
      }
    }
  } else if (line.startsWith("BXP:")) {
    // Echo des lignes extra BRAIDS (voir handleBraidsParamCommand()
    // cote Teensy -- write-only, pas de forme "?", mais l'ecriture est
    // tout de meme relayee/confirmee).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int i3 = line.indexOf(':', i2 + 1);
    if (i1 >= 0 && i2 >= 0 && i3 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t index = static_cast<uint8_t>(line.substring(i2 + 1, i3).toInt());
      const uint8_t value = static_cast<uint8_t>(line.substring(i3 + 1).toInt());
      if (track < kSeqTrackCount && index < 2) {
        patchExtraVal[track][index] = value;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchExtraRow(static_cast<uint8_t>(6 + index));
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
  // Bug trouve le 2026-09-17 ("bug d'affichage quand ca commence a
  // demarrer l'emulation") : drawLinkStatus() se dessine en bas de
  // l'ecran (kStatusY = kScreenSize-30 = 450) -- ca chevauche le bas de
  // l'image du jeu (kGbScreenTop=24 a 24+432=456, voir gbBlitLine()).
  // Cette fonction tourne a CHAQUE ligne recue du Teensy, y compris
  // STATUS: envoye ~1x/s en continu -- une barre noire + texte
  // s'incrustait donc sur le bas de l'ecran de jeu toutes les secondes
  // pendant qu'une partie tournait. Exclu en plus de Controls/Links.
  const bool gbPlaying = (currentScreen == Screen::Retro && gbIsLoaded());
  if (currentScreen != Screen::Controls && currentScreen != Screen::Links && !gbPlaying) {
    drawLinkStatus();
  }
}

// Etat de reception binaire (paquets oscilloscope, voir
// kScopePacketMagic) -- mele au flux texte habituel sur Serial1, meme
// principe que le son GB cote Teensy (voir AudioRxState dans
// src_teensy/az2_audio/main.cpp) mais dans l'autre sens.
//
// [2026-09-18] Bug de desynchronisation trouve et corrige ici -- voir
// AZ2_ETAT_DES_LIEUX.md "bruit blanc..." : le flux SCOPE dump'e a la
// main montrait des valeurs chaotiques (0-255 sans forme d'onde) meme
// pour ANALOG (propre a l'oreille), preuve que c'etait un bug de
// RECEPTION et pas le bruit moteur lui-meme. Analyse : (1) aucune
// verification/CRC -- un octet perdu decale la lecture "longueur" sur
// n'importe quel octet suivant, puis "longueur" octets de charge utile
// arbitraires sont avales comme si c'etait le paquet -- si un octet de
// charge utile vaut par hasard 0x02 (1 chance sur 256, les echantillons
// PCM couvrent tout 0-255), il est repris comme un NOUVEAU magic et le
// desalignement s'auto-entretient indefiniment ; (2) handleScopePacket()
// appelait drawPatchScope() (dessin SPI, plusieurs ms) DEPUIS la boucle
// meme qui lit Serial1 octet par octet -- pendant ce temps le tampon RX
// materiel continue de se remplir sans etre vide ; (3) tampon RX materiel
// laisse a sa taille par defaut (256 o) alors que le lien tourne
// maintenant a 921600 bauds (kControlBaud) - a cette vitesse 256 o se
// remplit en ~2-3 ms, largement moins que le temps d'un dessin ecran ou
// d'une frame GB -- tout depassement fait perdre des octets EN SILENCE
// (HardwareSerial ne previent pas). Le (2)+(3) causaient des pertes
// d'octets, le (1) transformait chaque perte en desynchronisation
// durable -- d'ou le "bruit" apparemment aleatoire du tracer, identique
// que le moteur audio source soit propre ou casse.
// Correctifs : setRxBufferSize() plus genereux (voir setup()), dessin
// sorti de la boucle de lecture (scopeNeedsRedraw, voir loop()), et
// verification de longueur fixe (kScopeSamplesPerPacket -- le Teensy
// n'envoie jamais que cette taille, voir updateScope()) qui rejette et
// resynchronise immediatement tout paquet dont la longueur ne colle pas,
// au lieu d'avaler des octets de charge utile bidon.
struct ScopeRxState {
  bool inPacket = false;
  bool haveLen = false;
  uint8_t len = 0;
  uint8_t pos = 0;
  uint32_t lastByteMs = 0;
  uint8_t buf[255];
};
ScopeRxState scopeRx;

// Delai max pendant lequel on tolere d'etre "au milieu" d'un paquet
// scope sans recevoir l'octet suivant avant de forcer un retour en mode
// texte -- un paquet complet (magic+longueur+32 o) tient en ~0.3 ms a
// 921600 bauds, 20 ms est donc tres large et ne se declenche que si le
// lien est vraiment bloque/coupe.
constexpr uint32_t kScopeRxTimeoutMs = 20;

// Fixee par updateScope() cote Teensy (voir main.cpp la-bas) -- jamais
// une autre valeur en pratique ; servir de garde-fou de resynchronisation
// ici (voir commentaire ScopeRxState plus haut).
static_assert(az2::kScopeSamplesPerPacket <= 255, "longueur scope tient sur 1 octet");

// Dessin du tracer differe hors de la boucle de lecture Serial1 (voir
// commentaire ScopeRxState) -- pose juste un flag ici, le dessin reel se
// fait une fois par tour de loop().
bool scopeNeedsRedraw = false;

// Paquet binaire recu du Teensy (voir kScopePacketMagic dans
// AZ2_Protocol.h) -- copie les echantillons, le dessin se fait plus tard
// (voir scopeNeedsRedraw).
void handleScopePacket(const uint8_t *data, uint8_t len) {
  const uint8_t n = min(len, az2::kScopeSamplesPerPacket);
  for (uint8_t i = 0; i < n; ++i) {
    scopeSamples[i] = data[i];
  }
  scopeHasData = true;
  scopeNeedsRedraw = true;
}

void readTeensyStatus() {
  // Paquet reste "ouvert" trop longtemps (lien bloque/coupe au milieu) :
  // on abandonne et on retourne en mode texte plutot que de rester
  // coince a attendre un octet qui n'arrivera jamais.
  if (scopeRx.inPacket && (millis() - scopeRx.lastByteMs) > kScopeRxTimeoutMs) {
    scopeRx.inPacket = false;
  }

  while (Serial1.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(Serial1.read());

    if (scopeRx.inPacket) {
      scopeRx.lastByteMs = millis();
      if (!scopeRx.haveLen) {
        scopeRx.len = b;
        scopeRx.pos = 0;
        scopeRx.haveLen = true;
        // Le Teensy n'envoie jamais qu'une longueur fixe -- toute autre
        // valeur signifie qu'on a perdu l'alignement (magic tombe par
        // hasard dans de la charge utile bidon) : on rejette tout de
        // suite au lieu d'avaler N octets arbitraires en payload, ce qui
        // ne ferait qu'aggraver le desalignement.
        if (scopeRx.len != az2::kScopeSamplesPerPacket) {
          scopeRx.inPacket = false;
        } else if (scopeRx.len == 0) {
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
      scopeRx.lastByteMs = millis();
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

// Le rendu est regroupe par bandes de 8 lignes GB : 160x8 pixels deviennent
// un bloc 480x24. Cela ramene une image de 144 a 18 appels au pilote RGB,
// sans recreer le grand framebuffer PSRAM 480x432 qui avait scintille sur
// le vrai materiel. Le tampon de bande (~23 Kio) reste stable pendant
// l'appel et n'exige aucune synchronisation de deux grands framebuffers.
// [2026-09-18] Tentative de rendu "1 bloc PSRAM entier envoye a la fin
// de l'image" essayee par une autre session IA le meme jour -- ANNULEE
// : premier retour utilisateur sur le vrai materiel = "l'ecran
// scintille" (tremblement continu de toute l'image), jamais observe
// avec le rendu par bandes ci-dessous. Revenu au comportement PROUVE
// du 2026-09-15 (1 seul draw16bitRGBBitmap() par ligne source, bloc
// 480x3 -- voir le commentaire au-dessus de la fonction) plutot que de
// laisser un vrai regression visuelle en place. Walnut-CGB documente
// lui-meme une limite native a ce style de rendu ligne par ligne
// ("certaines animations ne s'affichent pas correctement, ex.
// Prehistorik Man") -- accepte comme compromis connu, pas un bug AZ-2.
void gbBlitLine(int line, const uint16_t *row) {
  constexpr int16_t kGbSourceRowsPerBand = 8;
  constexpr int16_t kGbScaledRowsPerBand = kGbSourceRowsPerBand * 3;
  static uint16_t scaledBand[kGbScaledW * kGbScaledRowsPerBand];
  const int16_t sourceRowInBand = static_cast<int16_t>(line % kGbSourceRowsPerBand);
  uint16_t *scaledRow = scaledBand + sourceRowInBand * 3 * kGbScaledW;

  for (int x = 0; x < 160; ++x) {
    const uint16_t c = row[x];
    const int16_t base = static_cast<int16_t>(x * 3);
    scaledRow[base] = c;
    scaledRow[base + 1] = c;
    scaledRow[base + 2] = c;
  }
  // Les 2 autres rangees de sortie sont identiques a la premiere.
  memcpy(scaledRow + kGbScaledW, scaledRow, kGbScaledW * sizeof(uint16_t));
  memcpy(scaledRow + kGbScaledW * 2, scaledRow, kGbScaledW * sizeof(uint16_t));

  const bool bandComplete = sourceRowInBand == (kGbSourceRowsPerBand - 1) || line == 143;
  if (bandComplete) {
    const int16_t sourceBandStart = static_cast<int16_t>(line - sourceRowInBand);
    const int16_t sourceRows = static_cast<int16_t>(sourceRowInBand + 1);
    const int16_t y = static_cast<int16_t>(kGbScreenTop + sourceBandStart * 3);
    gfx->draw16bitRGBBitmap(0, y, scaledBand, kGbScaledW, sourceRows * 3);
  }
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
        seqStepProb[p][t][s] = 100;    // idem : 100% = joue toujours
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

  // Si l'ecran ne s'initialise pas, on continue quand meme le reste du
  // setup() (Serial1 vers le Teensy, tactile, SD) au lieu de tout arreter
  // net : un ecran en panne ne doit pas priver aussi le suivi serie du
  // lien Teensy/tactile/SD (seul l'intro + le premier menu, qui ont
  // besoin de l'ecran, sont sautes).
  const bool displayReady = gfx->begin();
  if (displayReady) {
    Serial.println("DISPLAY:READY");
    runIntro();
    goTo(Screen::Menu);
  } else {
    Serial.println("DISPLAY:ERROR:BEGIN_FAILED");
  }

  // Tampon RX materiel agrandi AVANT begin() (sans effet apres) -- 256 o
  // par defaut se remplit en ~2-3 ms a 921600 bauds (kControlBaud), soit
  // moins que certains blocages du loop() (dessin ecran, frame GB) ;
  // tout depassement perd des octets EN SILENCE et desynchronise le
  // parseur SCOPE (voir ScopeRxState plus haut). 2048 o donne une marge
  // large sans cout memoire notable (PSRAM/RAM disponibles ici).
  Serial1.setRxBufferSize(2048);
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
      if (padEditsStep) {
        // "Poser" la note sur le pas selectionne (voir padEditsStep) au
        // lieu de jouer en direct -- allume aussi le pas (STEP: ON),
        // sinon la note posee ne s'entendrait jamais en lecture.
        const uint8_t t = static_cast<uint8_t>(selectedSeqTrack);
        const uint8_t s = static_cast<uint8_t>(selectedSeqStep);
        const uint8_t note = static_cast<uint8_t>(az2::kPadBaseNote + pad);
        seqStepOn[currentPattern][t][s] = true;
        seqStepNote[currentPattern][t][s] = note;
        char msg[24];
        snprintf(msg, sizeof(msg), "STEP:%d:%d:1", t, s);
        sendToTeensy(msg);
        snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", t, s, note);
        sendToTeensy(msg);
      } else {
        char msg[24];
        snprintf(msg, sizeof(msg), "PAD:%02d:DOWN:vel=100", pad);
        sendToTeensy(msg);
      }
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
    } else if (hitTestTrkSideBtn(0, x, y)) {
      // MOTEUR (2026-09-19) -- ouvre la page MOTEURS directement sur
      // cette piste (assignation moteur/patch integre par piste).
      selectedEngineTrack = selectedSeqTrack;
      goTo(Screen::Engines);
    } else if (hitTestTrkSideBtn(1, x, y)) {
      // PATCH (ex-"AGRANDIR") -- ouvre la page PATCH complete (filtre/
      // ADSR/DXR/EXP/BXP + oscilloscope) pour cette piste.
      patchTrack = selectedSeqTrack;
      scopeHasData = false;
      goTo(Screen::Patch);
    } else if (hitTestTrkSideBtn(2, x, y)) {
      // EFFET (2026-09-19) -- reste dans le tracker (les effets SFX
      // sont PAR PAS, pas par piste -- pas de page dediee pertinente),
      // amene juste le focus croix sur la colonne FX du pas
      // selectionne pour l'editer tout de suite (A maintenu + HAUT/BAS).
      if (selectedSeqStep >= 0) {
        seqDetailCol = 2;
        drawDetailRow(static_cast<uint8_t>(selectedSeqStep));
      }
    } else if (hitTestTrkSideBtn(3, x, y)) {
      // CLAVIER (2026-09-19) -- ouvre la page AUDIO (pad 4x4) avec
      // padEditsStep active d'office : "pouvoir jouer une patterne en
      // live et l'enregistrer", donc les pads posent la note sur le pas
      // selectionne du tracker au lieu de seulement jouer en direct
      // (voir padEditsStep, BTN:D bascule ce reglage sur cette page).
      padEditsStep = true;
      goTo(Screen::Audio);
    } else if (hitTestTrkSideBtn(4, x, y)) {
      // METRONOME (2026-09-19) -- pas d'affichage optimiste, attend
      // l'echo METRO: confirme (meme convention que le reste de cette
      // page).
      char msg[12];
      snprintf(msg, sizeof(msg), "METRO:%d", metronomeOn ? 0 : 1);
      sendToTeensy(msg);
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
    // Page repensee le 2026-09-19 -- liste MOTEUR a gauche, liste PATCH
    // (defilante) a droite, bandeau mini-reglages en bas (voir
    // drawEnginesPage() et son commentaire pour le detail complet).
    const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
    if (hitTestEngTrackPrev(x, y) || hitTestEngTrackNext(x, y)) {
      selectedEngineTrack = static_cast<int8_t>(
          (selectedEngineTrack + (hitTestEngTrackNext(x, y) ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
      engPatchScroll = 0;
      engOnTrackRow = true;  // toucher les fleches piste = focus croix sur la ligne PISTE, coherent
      drawEnginesPage();
    } else if (hitTestEngMini(x, y)) {
      // Bandeau mini-reglages -- ouvre la page PATCH complete pour
      // cette piste (demande explicite : "si on clic dessus on arrive
      // a la page de reglage de patch").
      patchTrack = selectedEngineTrack;
      scopeHasData = false;
      goTo(Screen::Patch);
    } else {
      const int8_t engineHit = hitTestEngListRow(x, y);
      const int8_t patchSlotHit = hitTestEngPatchRow(x, y);
      if (engineHit >= 0) {
        // Toucher deplace aussi le focus croix sur la liste touchee --
        // meme reflexe que partout ailleurs dans l'appli (tap = select
        // + edit, tactile et croix restent synchronises).
        engineColPatch = false;
        engOnTrackRow = false;
        char msg[16];
        snprintf(msg, sizeof(msg), "ENGINE:%d:%d", t, engineHit);
        sendToTeensy(msg);
      } else if (patchSlotHit >= 0) {
        const uint16_t patchIdx = static_cast<uint16_t>(engPatchScroll + patchSlotHit);
        if (patchIdx < az2::enginePatchCount(trackEngine[t])) {
          engineColPatch = true;
          engOnTrackRow = false;
          char msg[16];
          snprintf(msg, sizeof(msg), "PATCH:%d:%d", t, patchIdx);
          sendToTeensy(msg);
        }
      }
    }
  } else if (currentScreen == Screen::Patch) {
    // Tactile repense le 2026-09-19 en meme temps que la croix (voir
    // le gestionnaire NAV: pour le detail complet) : un tap dans la
    // grille SELECTIONNE seulement (plus d'edition +/- directe au
    // tactile, voir hitTestPatchParam()) -- l'edition se fait ensuite
    // a la croix, A maintenu + HAUT/BAS. Toucher les fleches PISTE
    // deplace aussi le focus croix sur la ligne PISTE (patchOnTrackRow),
    // meme reflexe que hitTestEngTrackPrev/Next sur la page MOTEURS.
    const uint8_t t = static_cast<uint8_t>(patchTrack);
    if (hitTestPatchTrackPrev(x, y) || hitTestPatchTrackNext(x, y)) {
      patchTrack = static_cast<int8_t>((patchTrack + (hitTestPatchTrackNext(x, y) ? 1 : kSeqTrackCount - 1)) %
                                        kSeqTrackCount);
      scopeHasData = false;
      selectedPatchRow = 0;
      patchScroll = 0;
      patchOnTrackRow = true;
      char msg[12];
      snprintf(msg, sizeof(msg), "SCOPE:%d", patchTrack);
      sendToTeensy(msg);
      queryPatchExtra(static_cast<uint8_t>(patchTrack));
      drawPatchPage();
    } else {
      const int8_t row = hitTestPatchParam(x, y);
      if (row >= 0 && (row >= 6 || patchRowActive(t, static_cast<uint8_t>(row)))) {
        const int8_t prevRow = selectedPatchRow;
        selectedPatchRow = row;
        const bool leavingTrackRow = patchOnTrackRow;
        patchOnTrackRow = false;
        if (leavingTrackRow) {
          drawPatchTrackRow();
        }
        redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
        redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
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
  } else if (currentScreen == Screen::Project) {
    if (hitTestProjectSlotNum(x, y)) {
      projectSlot = static_cast<uint8_t>((projectSlot + 1) % kProjectSlotCount);
      drawProjectPage();
    } else if (hitTestProjectSlotSave(x, y)) {
      saveProject(projectSlot);
    } else if (hitTestProjectSlotLoad(x, y)) {
      loadProject(projectSlot);
      // Les tableaux locaux (trackEngine[]/seqStepOn[]/etc.) sont mis a
      // jour par loadProject() ET par les echos normaux du Teensy au
      // fur et a mesure -- redessiner une fois de plus a la fin est
      // juste pour rafraichir CETTE page (rien de son contenu ne
      // depend des donnees chargees).
      drawProjectPage();
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
    } else if (hitTestSwingMinus(x, y) || hitTestSwingPlus(x, y)) {
      const int delta = hitTestSwingPlus(x, y) ? kSwingStep : -kSwingStep;
      swingValue = static_cast<uint8_t>(constrain(static_cast<int>(swingValue) + delta, 0, 127));
      drawConfigPage();
      char msg[16];
      snprintf(msg, sizeof(msg), "SWING:%d", swingValue);
      sendToTeensy(msg);
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
  // Dessin du tracer scope differe hors de readTeensyStatus() -- voir
  // commentaire ScopeRxState/scopeNeedsRedraw plus haut : dessiner un
  // paquet a la fois DANS la boucle de lecture Serial1 bloquait la
  // lecture assez longtemps pour perdre des octets a 921600 bauds.
  if (scopeNeedsRedraw) {
    scopeNeedsRedraw = false;
    if (currentScreen == Screen::Patch && !screensaverActive) {
      drawPatchScope();
    }
  }

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

  // Efface le temoin de potard (voir drawPotToast()) apres son delai --
  // relance un rendu complet de la page courante plutot que de retenir
  // ce qu'il y avait sous la bande, plus simple/robuste.
  if (potToastActive && (nowForIdle - potToastLastMs) >= kPotToastTimeoutMs) {
    potToastActive = false;
    if (!screensaverActive) {
      drawScreen(currentScreen);
    }
  }

  // Emulateur Game Boy : une frame dure exactement 70224 cycles a
  // 4 194 304 Hz, soit 16 742,706298 us. Garder seulement 16 742 us
  // cree une petite derive permanente ; l'accumulateur de reste ci-dessous
  // alterne 16 742/16 743 us et conserve la cadence native sur la duree.
  constexpr uint32_t kGbFramePeriodUs = 16742;
  constexpr uint32_t kGbFrameRemainder = 2962432;
  constexpr uint32_t kGbClockHz = 4194304;
  static uint32_t nextGbFrameUs = 0;
  static uint32_t gbFrameFraction = 0;
  static uint32_t gbFrameCount = 0;
  static uint32_t gbFrameTimeTotalUs = 0;
  static uint32_t gbFrameTimeMaxUs = 0;
  static uint32_t gbMissedFrames = 0;
  static uint32_t gbFpsWindowStartMs = 0;
  if (currentScreen == Screen::Retro && gbIsLoaded() && !screensaverActive) {
    const uint32_t nowUs = micros();
    if (nextGbFrameUs == 0) {
      nextGbFrameUs = nowUs;
    }
    if (static_cast<int32_t>(nowUs - nextGbFrameUs) >= 0) {
      const uint32_t frameStartUs = micros();
      gbRunFrame();
      const uint32_t frameDurationUs = micros() - frameStartUs;
      ++gbFrameCount;
      gbFrameTimeTotalUs += frameDurationUs;
      if (frameDurationUs > gbFrameTimeMaxUs) {
        gbFrameTimeMaxUs = frameDurationUs;
      }

      nextGbFrameUs += kGbFramePeriodUs;
      gbFrameFraction += kGbFrameRemainder;
      if (gbFrameFraction >= kGbClockHz) {
        ++nextGbFrameUs;
        gbFrameFraction -= kGbClockHz;
      }

      // Un retard superieur a deux frames n'est pas cache : on le compte,
      // puis on resynchronise pour conserver les commandes/UI reactives.
      // La cible de qualification impose que ce compteur reste a zero.
      const uint32_t afterFrameUs = micros();
      const int32_t lateUs = static_cast<int32_t>(afterFrameUs - nextGbFrameUs);
      if (lateUs > static_cast<int32_t>(kGbFramePeriodUs * 2)) {
        gbMissedFrames += static_cast<uint32_t>(lateUs) / 16743U;
        nextGbFrameUs = afterFrameUs + kGbFramePeriodUs;
        gbFrameFraction = kGbFrameRemainder;
      }
    }
    const uint32_t fpsElapsedMs = now - gbFpsWindowStartMs;
    if (fpsElapsedMs >= 1000) {
      const uint32_t fpsX100 = (fpsElapsedMs > 0)
                                   ? static_cast<uint32_t>((static_cast<uint64_t>(gbFrameCount) * 100000ULL) /
                                                           fpsElapsedMs)
                                   : 0;
      const uint32_t frameAvgUs = (gbFrameCount > 0) ? gbFrameTimeTotalUs / gbFrameCount : 0;
      Serial.print("GB:PERF:fps_x100=");
      Serial.print(fpsX100);
      Serial.print(":frame_us_avg=");
      Serial.print(frameAvgUs);
      Serial.print(":frame_us_max=");
      Serial.print(gbFrameTimeMaxUs);
      Serial.print(":missed=");
      Serial.println(gbMissedFrames);
      gbFrameCount = 0;
      gbFrameTimeTotalUs = 0;
      gbFrameTimeMaxUs = 0;
      gbMissedFrames = 0;
      gbFpsWindowStartMs = now;
    }
  } else {
    nextGbFrameUs = 0;
    gbFrameFraction = 0;
    gbFrameCount = 0;
    gbFrameTimeTotalUs = 0;
    gbFrameTimeMaxUs = 0;
    gbMissedFrames = 0;
    gbFpsWindowStartMs = now;
  }

  if (now - lastHeartbeatMs >= 1000) {
    lastHeartbeatMs = now;
    Serial.println("DISPLAY:ALIVE:TICK");
  }
}
