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
#include <databus/Arduino_ESP32RGBPanel.h>
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <AZ2_Protocol.h>
#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include "gb_emulator.h"
#include "nes_emulator.h"
#include "ngp_emulator.h"
#ifdef AZ2_DIRECT_PANEL
#include "AZ2_RGB_Direct.h"
#endif
#if defined(AZ2_GB_DUAL_CORE_BLIT) || defined(AZ2_NES_DUAL_CORE_BLIT) || defined(AZ2_NGP_DUAL_CORE_BLIT)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

void drawGbViewportFrame();
void flushUiCanvas();
uint8_t gbDisplayScale = 2;

void ngpRenderFrameOnCore(const uint16_t *pixels);

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

#ifndef AZ2_DIRECT_PANEL
// PCLK 10 MHz (2026-10-01, etait 12). Le DMA du panneau lit le framebuffer en
// PSRAM en concurrence avec le blit des emulateurs : a 12 MHz il manquait de
// donnees, des lignes s'affichaient decalees ("l'ecran sautille") jusqu'au
// recalage VSYNC. 10 MHz = ~35 Hz de balayage, -17 % de debit PSRAM, valide a
// l'oeil en X2 et X3. Le bounce buffer a ete essaye avant : il remplacait le
// decalage par des bandes qui clignotent en X3, rejete. 20 MHz : image
// brouillee (voir AZ2_AUDIT_PILOTE_RGB_2026-09-20.md).
Arduino_ESP32RGBPanel *rgbPanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 10000000 /* prefer_speed, voir note PCLK plus haut */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 0 /* bounce_buffer_size_px */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    kScreenSize, kScreenSize, rgbPanel, 2 /* rotation: ecran tete-en-bas */,
    true, bus, GFX_NOT_DEFINED, gc9503v_type1_init_operations, sizeof(gc9503v_type1_init_operations));
#else
const int kDirectDataPins[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                 7, 6, 5, 4, 3, 2, 1, 0};
AZ2RgbDirect directPanel(bus, gc9503v_type1_init_operations,
                          sizeof(gc9503v_type1_init_operations),
                          18, 17, 16, 21, kDirectDataPins,
                          kScreenSize, kScreenSize, 12000000);
AZ2RgbDirectOutput directOutput(&directPanel);
Arduino_Canvas directCanvas(kScreenSize, kScreenSize, &directOutput);
Arduino_GFX *gfx = &directCanvas;
#endif

const uint16_t kPalette[] = {
    RGB565(255, 60, 172), RGB565(70, 220, 255), RGB565(255, 170, 50),
    RGB565(170, 90, 255), RGB565(255, 225, 80),
};
constexpr uint8_t kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);
constexpr uint16_t kDim = RGB565(140, 140, 150);

// Identite visuelle PAR MOTEUR (2026-09-22, "il faut que les fenetres de
// controle aient chacune une identite par moteur") -- jusqu'ici la page
// PATCH (et le reste de l'appli) coloraient tout en fonction de la PISTE
// (kPalette[track % kPaletteCount]) : DEXED sur la piste 0 et DEXED sur
// la piste 3 n'avaient rien de visuellement commun, alors que BRAIDS et
// DEXED sur la MEME piste se ressemblaient (meme couleur de piste). Ce
// tableau donne au contraire une couleur STABLE par moteur, memorisable
// independamment de la piste -- utilise par la page PATCH (voir
// engineAccent()/patchAccent() plus bas, apres trackEngine[]). Index
// aligne sur az2::kEngine* (voir AZ2_Protocol.h), meme convention que
// kEngineNames[].
const uint16_t kEngineAccent[az2::kEngineCount] = {
    RGB565(255, 195, 40),   // DEXED    -- or/cuivre FM
    RGB565(255, 110, 140),  // EPIANO   -- rose electrique
    RGB565(90, 170, 255),   // BRAIDS   -- bleu electrique
    RGB565(120, 230, 120),  // KARPLUS  -- vert corde pincee
    RGB565(255, 120, 30),   // ANALOG   -- orange chaud analogique
    RGB565(255, 70, 190),   // SAMPLER  -- magenta
    RGB565(255, 60, 60),    // DRUM     -- rouge percussif
    RGB565(180, 100, 255),  // GRANULAR -- violet nuage de grains
    RGB565(70, 220, 200),   // SPECTRAL -- cyan/teal spectral
};

// Assombrit une couleur RGB565 (garde la teinte, baisse la luminosite) --
// sert aux bandes verticales de mesure du sequenceur, voir seqBandColor().
uint16_t dimColor(uint16_t c, uint8_t shift) {
  const uint16_t r = static_cast<uint16_t>(((c >> 11) & 0x1F) >> shift);
  const uint16_t g = static_cast<uint16_t>(((c >> 5) & 0x3F) >> shift);
  const uint16_t b = static_cast<uint16_t>((c & 0x1F) >> shift);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
constexpr uint16_t kFaint = RGB565(90, 90, 110);
// [2026-09-26] Fond des cadres selectionnes du menu principal (voir
// drawCategoryCard()) -- gris fonce neutre, plus clair que le noir mais
// jamais aussi vif qu'un accent de moteur.
constexpr uint16_t kPanel2 = RGB565(28, 32, 40);

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

uint8_t touchInvalidFrames = 0;
void touchFrameInvalid() {
  if (++touchInvalidFrames < 3) return;
  touchInvalidFrames = 0;
  Wire.end();
  delayMicroseconds(200);
  Wire.begin(kTouchSdaPin, kTouchSclPin);
  Wire.setClock(400000);
  Serial.println("TOUCH:I2C_RECOVERED");
}

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
    touchFrameInvalid();
    return 0xFF;
  }

  constexpr uint8_t kReadLen = 11;  // registres 0x02 a 0x0C inclus
  if (Wire.requestFrom(kTouchI2cAddr, kReadLen) != kReadLen) {
    reportTouchI2cError("SHORT_READ");
    touchFrameInvalid();
    return 0xFF;
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
    touchFrameInvalid();
    return 0xFF;
  }
  touchInvalidFrames = 0;

  if (touchCount >= 1) {
    const int16_t rawX = static_cast<int16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
    const int16_t rawY = static_cast<int16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
    const int16_t x = static_cast<int16_t>(
#ifdef AZ2_DIRECT_PANEL
        rawX
#else
        (kScreenSize - 1) - rawX
#endif
    );
    const int16_t y = static_cast<int16_t>(
#ifdef AZ2_DIRECT_PANEL
        rawY
#else
        (kScreenSize - 1) - rawY
#endif
    );
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
    const int16_t x = static_cast<int16_t>(
#ifdef AZ2_DIRECT_PANEL
        rawX
#else
        (kScreenSize - 1) - rawX
#endif
    );
    const int16_t y = static_cast<int16_t>(
#ifdef AZ2_DIRECT_PANEL
        rawY
#else
        (kScreenSize - 1) - rawY
#endif
    );
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
enum class Screen : uint8_t { Menu, Controls, Audio, Sampler, Sequencer, Engines, Retro, Config, Links, About, Patch, Song, Project, Mixer, StepSeq, EmuPicker, NesRetro, NgpRetro };
Screen currentScreen = Screen::Menu;
// "Retour" (2026-09-19, "il faut pas que ca revienne aux menu general
// il faut que ca revienne d'un etage seulement") -- UN SEUL niveau
// memorise (pas une pile complete), mis a jour a chaque goTo() reel
// (voir plus bas) : suffit pour "revenir d'ou on vient" partout, sans
// la complexite d'une vraie pile d'historique.
Screen navPrevious = Screen::Menu;
Screen enginesReturnScreen = Screen::Menu;
Screen patchReturnScreen = Screen::Engines;

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
  nesSerial1Lock();
  Serial1.println(message);
  nesSerial1Unlock();
}

// ---------------------------------------------------------------------
// Chrome commun : en-tete avec fleche retour + titre, bas d'ecran = etat
// du lien Teensy.
// ---------------------------------------------------------------------
constexpr int16_t kMargin = 24;
constexpr int16_t kStatusY = kScreenSize - 30;

// Boutons AFFICHAGE (X2/X3) de la page JEUX -- voir hitTest assorti dans
// handleTouchDown() (Screen::Retro && !gbIsLoaded()) et kRomRowTop plus bas,
// decale pour laisser la place a ces boutons agrandis.
constexpr int16_t kScaleBtnY = 64;
constexpr int16_t kScaleBtnW = 150;
constexpr int16_t kScaleBtnH = 34;
constexpr int16_t kScaleBtnX2 = 120;
constexpr int16_t kScaleBtnX3 = 306;

// Page EMULATEURS (Screen::EmuPicker, 2026-09-27) : choix du moteur avant
// d'entrer dans Screen::Retro. 3 cartes : GAME BOY (Peanut-GB, valide pleine
// vitesse, seule cible reellement jouable sur CE firmware), GBC (Walnut-CGB
// + meme decoupage double coeur, encore instable -- bug de blocage au
// demarrage non resolu, voir docs) et NES (pas commence). GBC/NES ne
// peuvent pas etre compiles dans le MEME firmware que Peanut-GB (collision
// de symboles, gb_emulator.cpp vs gb_emulator_peanut.cpp) -- boutons actifs
// (tactile + croix/A), mais qui affichent honnetement leur etat au lieu de
// pretendre lancer un jeu, voir emuPickerActivate().
constexpr int16_t kEmuCardX = kMargin;
constexpr int16_t kEmuCardW = kScreenSize - 2 * kMargin;
constexpr int16_t kEmuCardH = 88;
constexpr int16_t kEmuCardGap = 10;
constexpr int16_t kEmuCardGbY = 76;
constexpr int16_t kEmuCardGbcY = kEmuCardGbY + kEmuCardH + kEmuCardGap;
constexpr int16_t kEmuCardNesY = kEmuCardGbcY + kEmuCardH + kEmuCardGap;
constexpr int16_t kEmuCardNeoY = kEmuCardNesY + kEmuCardH + kEmuCardGap;
constexpr uint8_t kEmuCardCount = 4;
int8_t emuPickerSelected = 0;

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
void drawSubHeader(const char *title, uint16_t accent) {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(accent);
  gfx->setTextSize(2);
  gfx->setCursor(kMargin, 28);
  gfx->print("< ");
  gfx->print(title);
  gfx->drawFastHLine(kMargin, 60, kScreenSize - 2 * kMargin, kFaint);
}

bool hitBack(int16_t x, int16_t y) {
  return inBox(x, y, 0, 0, 90, 50);
}

// Indication visuelle de ce que controlent les 2 encodeurs
// "reglables" (index 1 et 2 -- l'encodeur 0/VOLUME garde un role FIXE,
// voir le commentaire d'updateEncoders() cote Teensy) sur l'ecran
// actuellement affiche -- demande 2026-09-19 ("on va leur attribuer
// une couleur et les inclure a chaque fois dans l'application ... on
// les utilise pas assez"). Couleur FIXE par encodeur (les 2 memes
// couleurs partout ou ce petit indicateur apparait) : l'utilisateur
// apprend "orange = encodeur 1, cyan = encodeur 2" une seule fois,
// seul le LIBELLE change selon la page/le parametre au focus.
// `label == nullptr` -> "--" en gris : cet encodeur n'a pas de role
// sur cet ecran (position stable, pas d'element qui disparait).
constexpr uint16_t kEnc1Color = RGB565(255, 140, 20);  // orange
constexpr uint16_t kEnc2Color = RGB565(60, 220, 255);  // cyan
void drawEncoderHint(uint8_t slot, const char *label) {
  const uint16_t color = (slot == 0) ? kEnc1Color : kEnc2Color;
  constexpr int16_t sw = 9;
  constexpr int16_t y = 14;
  const int16_t x = static_cast<int16_t>(kScreenSize - kMargin - (slot == 0 ? 168 : 78));
  gfx->fillRect(x, y, sw, sw, RGB565_BLACK);  // efface le libelle precedent (largeur variable)
  gfx->fillRect(static_cast<int16_t>(x - 2), static_cast<int16_t>(y - 2),
                static_cast<int16_t>((slot == 0 ? 168 : 78) - 4), static_cast<int16_t>(sw + 4), RGB565_BLACK);
  gfx->fillRect(x, y, sw, sw, color);
  gfx->setTextSize(1);
  gfx->setTextColor(label ? RGB565_WHITE : kFaint);
  gfx->setCursor(static_cast<int16_t>(x + sw + 4), static_cast<int16_t>(y + 1));
  gfx->print(label ? label : "--");
}
void drawEncoderHints(const char *label1, const char *label2) {
  drawEncoderHint(0, label1);
  drawEncoderHint(1, label2);
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
    {"AZ-TRACKER", "sequenceur, moteurs, audio"},
    {"JEUX", "emulateur Game Boy"},
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
    // [2026-09-26] Demande : "un sequenceur piste en plus du sequenceur
    // tracker, plus traditionnel comme le OP1" -- meme donnees
    // (seqStepOn/seqStepNote/seqStepProb), vue alternative en gros blocs
    // par piste au lieu de la grille dense multi-colonnes. Voir
    // drawStepSeqPage().
    {"SEQ. PAS", "vue step, style OP-1", Screen::StepSeq, MenuCat::Musique},
    {"MOTEURS", "moteur + patch par piste", Screen::Engines, MenuCat::Musique},
    {"PATCH", "filtre + ADSR + forme d'onde", Screen::Patch, MenuCat::Musique},
    {"MIXER", "volume de toutes les pistes", Screen::Mixer, MenuCat::Musique},
    {"SONG", "chaine les patterns", Screen::Song, MenuCat::Musique},
    {"PROJETS", "liste, charger et sauver", Screen::Project, MenuCat::Musique},
    {"AUDIO", "jouer le Teensy depuis l'ecran", Screen::Audio, MenuCat::Musique},
    {"JEUX", "choisir un emulateur", Screen::EmuPicker, MenuCat::Jeux},
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
int8_t menuReturnCategory = -1;
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

// [2026-09-26] Grille 2x2 gardee (des lignes pleine largeur cassaient la
// navigation croix GAUCHE/DROITE entre colonnes, essaye puis annule) --
// seul l'en-tete "AZ-2 / CHOISIS UNE SECTION" reste retire (espace juge
// inutile, voir drawMenu()), la place gagnee sert a des cadres plus
// grands qu'avant (kCatTop remonte d'autant).
constexpr int16_t kCatTop = 24;
constexpr int16_t kCatGap = 16;
constexpr int16_t kCatW = (kScreenSize - 2 * kMargin - kCatGap) / 2;
constexpr int16_t kCatH = (kStatusY - kCatTop - kCatGap) / 2;

void catRect(uint8_t index, int16_t &x, int16_t &y) {
  x = static_cast<int16_t>(kMargin + (index % 2) * (kCatW + kCatGap));
  y = static_cast<int16_t>(kCatTop + (index / 2) * (kCatH + kCatGap));
}

// [2026-09-26] Trame/matrice discrete en fond de la page menu (avant les
// cadres) -- lignes fines espacees de 24px, tres attenuees. Purement
// decoratif, dessine une seule fois par entree sur la page.
void drawMenuMatrixBackdrop() {
  constexpr int16_t kGridStep = 24;
  constexpr uint16_t kGridColor = RGB565(20, 22, 28);
  for (int16_t gx = 0; gx < kScreenSize; gx += kGridStep) {
    gfx->drawFastVLine(gx, 0, kStatusY, kGridColor);
  }
  for (int16_t gy = 0; gy < kStatusY; gy += kGridStep) {
    gfx->drawFastHLine(0, gy, kScreenSize, kGridColor);
  }
}

void drawCategoryCard(uint8_t index) {
  int16_t cellX, cellY;
  catRect(index, cellX, cellY);
  // [2026-09-26] Cadre retreci a l'interieur de sa cellule (demande : "un
  // peu plus petit"), bordure epaissie a 3px (3 rects imbriques -- GFX ne
  // fait que des contours 1px nativement).
  constexpr int16_t kInset = 10;
  constexpr int16_t kBorderThickness = 3;
  const int16_t x = static_cast<int16_t>(cellX + kInset);
  const int16_t y = static_cast<int16_t>(cellY + kInset);
  const int16_t w = static_cast<int16_t>(kCatW - 2 * kInset);
  const int16_t h = static_cast<int16_t>(kCatH - 2 * kInset);
  const uint16_t accent = kPalette[index % kPaletteCount];
  const bool selected = (index == menuSelected);
  gfx->fillRect(x, y, w, h, selected ? kPanel2 : RGB565_BLACK);
  for (int16_t b = 0; b < kBorderThickness; ++b) {
    gfx->drawRect(static_cast<int16_t>(x + b), static_cast<int16_t>(y + b),
                  static_cast<int16_t>(w - 2 * b), static_cast<int16_t>(h - 2 * b),
                  selected ? accent : kFaint);
  }
  // Lisere de couleur a gauche : identifie la categorie meme non
  // selectionnee, sans avoir besoin d'un fond plein.
  gfx->fillRect(x, y, kBorderThickness + 1, h, accent);
  gfx->setTextSize(2);
  gfx->setTextColor(selected ? accent : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + 16), static_cast<int16_t>(y + h / 2 - 18));
  gfx->print(kCategories[index].label);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 16), static_cast<int16_t>(y + h / 2 + 8));
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
    // [2026-09-26] En-tete "AZ-2 / CHOISIS UNE SECTION" retire (demande
    // utilisateur, "rien a faire la") -- l'espace sert desormais a des
    // cadres de categorie plus grands (voir kCatTop/kCatH ci-dessous).
    gfx->fillScreen(RGB565_BLACK);
    drawMenuMatrixBackdrop();
    for (uint8_t i = 0; i < kMenuCatCount; ++i) {
      drawCategoryCard(i);
    }
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
constexpr int16_t kGridLeft = 8;
constexpr int16_t kGridTop = 28;
constexpr int16_t kGridCell = 104;
constexpr int16_t kGridGap = 4;

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
bool padMenuOpen = false;
uint8_t padMenuIndex = 0;
constexpr uint8_t kPadMenuCount = 5;
const char *const kPadMenuItems[kPadMenuCount] = {"JEU LIBRE", "EDITER LE PAS", "SAMPLER", "MOTEURS", "SEQUENCEUR"};
uint32_t encPressStartedMs[3] = {};
constexpr uint32_t kEncoderLongPressMs = 700;
// -1 = generique (voix live Dexed fixe, comportement d'origine) ; sinon
// = piste dont les pads jouent le VRAI moteur/patch (2026-09-19, "on
// ajoute un bouton dans la fenetre du tracker pour ... joue
// l'instrument de la piste") -- distinct de padEditsStep : celui-ci
// decide si les presses sont EN PLUS ecrites sur le pas selectionne,
// celui-la decide quelle SOURCE SONORE joue. Les 2 sont orthogonaux.
// Mis a jour par le bouton CLAVIER du tracker (voir hitTestTrkSideBtn(3))
// et remis a -1 en entrant sur AUDIO depuis le menu general (voir les 2
// sites goTo(kMenuItems[...].target)).
int8_t padTargetTrack = -1;
// Chemin SD (carte du Teensy) actuellement assigne a chaque pad, ""
// = aucun (2026-09-19, "il faut pouvoir aussi les sauvegarder dans le
// projet global") -- mis a jour par l'echo PADSAMPLE:<pad>:READY:
// path=... cote Teensy (voir handleTeensyLine()), pas ecrit
// directement ici : reste ainsi TOUJOURS synchronise avec ce qui est
// reellement charge, que l'assignation vienne d'ici, du kit de depart
// au boot du Teensy, ou d'un chargement de projet.
char padSamplePath[az2::kPadCount][64] = {};
extern int8_t selectedSeqTrack;  // definie plus bas, avec le reste de l'etat du sequenceur
extern int8_t selectedSeqStep;

void drawAudioPage() {
  gfx->fillScreen(RGB565_BLACK);
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    drawAudioCell(pad, false);
  }
  heldAudioPad[0] = -1;
  heldAudioPad[1] = -1;
}

void drawPadMenu() {
  constexpr int16_t x = 54;
  constexpr int16_t y = 62;
  constexpr int16_t w = 372;
  constexpr int16_t h = 350;
  gfx->fillRect(x, y, w, h, RGB565_BLACK);
  gfx->drawRect(x, y, w, h, kPalette[2]);
  gfx->setTextSize(2);
  gfx->setTextColor(kPalette[2]);
  gfx->setCursor(x + 18, y + 18);
  gfx->print("PAD 4X4");
  gfx->setTextSize(1);
  for (uint8_t i = 0; i < kPadMenuCount; ++i) {
    const int16_t rowY = static_cast<int16_t>(y + 58 + i * 52);
    const bool selected = i == padMenuIndex;
    if (selected) gfx->fillRect(x + 12, rowY - 4, w - 24, 38, kPalette[1]);
    gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(x + 28, rowY + 8);
    gfx->print(kPadMenuItems[i]);
  }
  gfx->setTextColor(kDim);
  gfx->setCursor(x + 18, y + h - 24);
  gfx->print("TOURNER: choisir  CLIC: ouvrir  LONG: sortir");
}

extern int8_t selectedEngineTrack;
void goTo(Screen s);
void activatePadMenuItem() {
  padMenuOpen = false;
  switch (padMenuIndex) {
    case 0:
      padEditsStep = false;
      padTargetTrack = selectedSeqTrack;
      drawAudioPage();
      break;
    case 1:
      padEditsStep = true;
      padTargetTrack = selectedSeqTrack;
      drawAudioPage();
      break;
    case 2: goTo(Screen::Sampler); break;
    case 3:
      selectedEngineTrack = selectedSeqTrack;
      goTo(Screen::Engines);
      break;
    default: goTo(Screen::Sequencer); break;
  }
}

constexpr uint8_t kSamplerRows = 6;
char samplerFiles[kSamplerRows][64] = {};
bool samplerIsDir[kSamplerRows] = {};
char samplerFolder[64] = "/samples";
uint16_t samplerOffset = 0;
uint16_t samplerTotal = 0;
uint8_t samplerVisible = 0;
uint8_t samplerSelectedRow = 0;
uint8_t samplerSelectedPad = 0;
uint8_t samplerKitSlot = 0;
bool samplerNeedsRedraw = false;
int8_t heldSamplerFile[2] = {-1, -1};
char samplerUiStatus[40] = {};
// Fusion PATCH <-> navigateur SD (2026-09-23, "attribuer des samples ...
// il faut faire une fusion") : cet ecran servait jusqu'ici exclusivement
// aux 16 pads. samplerTargetIsTrack bascule son comportement pour cibler
// a la place le moteur SAMPLER d'UNE piste (samplerTargetTrack) -- la
// grille de 16 pads est alors remplacee par un simple indicateur de
// piste (voir drawSamplerPage()), et "AFFECTER" envoie TRACKSAMPLE: au
// lieu de PADSAMPLE: (voir samplerAssignSelected()). false = comportement
// pads inchange.
bool samplerTargetIsTrack = false;
int8_t samplerTargetTrack = -1;
// [2026-09-25] Vrai quand le navigateur SD (meme ecran, meme liste) a ete
// ouvert depuis la ligne SAMPLE du moteur GRANULAR plutot que pour le
// SAMPLER -- change juste le message envoye au choix d'un fichier
// (GRANULAR_SAMPLE: au lieu de TRACKSAMPLE:), voir samplerAssignSelected().
// N'a de sens que si samplerTargetIsTrack est aussi vrai.
bool samplerTargetIsGranular = false;
void saveSamplerKit();
void loadSamplerKit();
extern bool screensaverActive;
void drawSamplerPage();
void drawSamplerTrackPanel(uint8_t track);  // definie plus bas, voir son commentaire
void goTo(Screen s);

void requestSamplerList() {
  for (uint8_t i = 0; i < kSamplerRows; ++i) {
    samplerFiles[i][0] = '\0';
    samplerIsDir[i] = false;
  }
  samplerVisible = 0;
  char msg[96];
  snprintf(msg, sizeof(msg), "SAMPLELIST:%s:%u", samplerFolder, samplerOffset);
  sendToTeensy(msg);
}

void samplerOpenSelected() {
  if (samplerSelectedRow >= samplerVisible || !samplerIsDir[samplerSelectedRow]) return;
  snprintf(samplerFolder, sizeof(samplerFolder), "%s", samplerFiles[samplerSelectedRow]);
  samplerOffset = 0;
  samplerSelectedRow = 0;
  requestSamplerList();
  drawSamplerPage();
}

void samplerGoUp() {
  if (strcmp(samplerFolder, "/samples") == 0) {
    // Mode piste (2026-09-23, voir samplerTargetIsTrack) : retour a la
    // page PATCH d'ou l'on est venu, pas a la page AUDIO/pads -- cet
    // ecran n'a jamais ete ouvert depuis le menu PAD dans ce cas.
    if (samplerTargetIsTrack) {
      samplerTargetIsTrack = false;
      samplerTargetIsGranular = false;
      goTo(Screen::Patch);
    } else {
      goTo(Screen::Audio);
    }
    return;
  }
  char *slash = strrchr(samplerFolder, '/');
  if (slash && slash > samplerFolder) *slash = '\0';
  else snprintf(samplerFolder, sizeof(samplerFolder), "/samples");
  samplerOffset = 0;
  samplerSelectedRow = 0;
  requestSamplerList();
  drawSamplerPage();
}

void drawSamplerPage() {
  const uint8_t targetTrack = static_cast<uint8_t>(samplerTargetTrack);
  drawSubHeader("SAMPLEUR", kPalette[2]);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(20, 72);
  gfx->print(samplerTargetIsGranular ? "GRANULAR  choisir un WAV"
             : samplerTargetIsTrack  ? "PISTE  choisir un WAV pour son SAMPLER"
                                      : "PAD   toucher pour jouer / choisir");
  gfx->setCursor(242, 72);
  gfx->print(String(samplerFolder).substring(0, 35));
  if (samplerTargetIsTrack) {
    // Fusion PATCH <-> navigateur SD (2026-09-23) : la grille de 16 pads
    // n'a pas de sens ici (une seule piste, pas 16 cibles) -- un panneau
    // unique rappelle plutot quelle piste va recevoir le fichier choisi
    // dans la liste. Fonction definie PLUS BAS dans ce fichier (pas ici)
    // : elle a besoin de patchAccent()/trackEngine[]/trackSamplePath[],
    // tous declares apres ce point -- drawSamplerPage() est appelee bien
    // avant leur declaration par plusieurs sites (menu PAD, etc.).
    drawSamplerTrackPanel(targetTrack);
  } else {
    for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
      const int16_t x = 20 + (pad % 4) * 49;
      const int16_t y = 96 + (pad / 4) * 49;
      const bool selected = pad == samplerSelectedPad;
      gfx->fillRect(x, y, 44, 44, selected ? kPalette[2] : RGB565_BLACK);
      gfx->drawRect(x, y, 44, 44, selected ? RGB565_WHITE : kFaint);
      gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
      gfx->setCursor(x + 10, y + 5);
      gfx->printf("%02u", pad);
      gfx->setCursor(x + 6, y + 23);
      gfx->print(padSamplePath[pad][0] ? "WAV" : "---");
    }
  }
  for (uint8_t row = 0; row < kSamplerRows; ++row) {
    const int16_t y = 96 + row * 43;
    gfx->fillRect(240, y, 220, 39, row == samplerSelectedRow ? RGB565(35, 55, 75) : RGB565_BLACK);
    gfx->drawRect(240, y, 220, 39, row == samplerSelectedRow ? kPalette[2] : kFaint);
    gfx->setTextColor(samplerFiles[row][0] ? RGB565_WHITE : kDim);
    gfx->setCursor(246, y + 5);
    const char *name = strrchr(samplerFiles[row], '/');
    if (samplerFiles[row][0]) {
      if (samplerIsDir[row]) gfx->print("[DOSSIER] ");
      gfx->print(String(name ? name + 1 : samplerFiles[row]).substring(0, samplerIsDir[row] ? 24 : 34));
    } else if (row == 0 && samplerVisible == 0) gfx->print("Dossier vide / actualiser");
    gfx->setCursor(246, y + 21);
    if (samplerFiles[row][0]) gfx->printf("%u  %.31s", samplerOffset + row + 1, samplerFiles[row]);
  }
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(20, 310);
  if (!samplerTargetIsTrack) {
    gfx->printf("PAD %02u : %.55s", samplerSelectedPad,
                padSamplePath[samplerSelectedPad][0] ? padSamplePath[samplerSelectedPad] : "aucun sample");
  }
  gfx->setTextColor(kDim);
  gfx->setCursor(20, 336);
  gfx->print(samplerUiStatus[0] ? samplerUiStatus
             : samplerTargetIsTrack ? "Retour (C) : garde le sample actuel"
                                     : "Glisser WAV vers pad");
  gfx->drawRect(20, 354, 95, 42, kFaint);
  gfx->drawRect(123, 354, 95, 42, kFaint);
  gfx->drawRect(240, 354, 220, 42, kPalette[2]);
  gfx->setCursor(39, 369); gfx->print("< LISTE");
  gfx->setCursor(145, 369); gfx->print("LISTE >");
  gfx->setCursor(298, 369);
  gfx->print(samplerIsDir[samplerSelectedRow] ? "OUVRIR (A)" : "AFFECTER (A)");
  // KIT sauver/charger n'a de sens que pour les pads (2026-09-23) -- une
  // piste n'a qu'UN sample CUSTOM, pas 16, rien a grouper en kit.
  if (!samplerTargetIsTrack) {
    gfx->drawRect(20, 408, 145, 38, kFaint);
    gfx->drawRect(170, 408, 135, 38, kPalette[2]);
    gfx->drawRect(310, 408, 150, 38, kPalette[3]);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(31, 422); gfx->printf("KIT %u / 4", samplerKitSlot + 1);
    gfx->setCursor(207, 422); gfx->print("SAUVER");
    gfx->setCursor(360, 422); gfx->print("CHARGER");
  }
}

void samplerAssignSelected() {
  if (samplerSelectedRow >= samplerVisible || samplerIsDir[samplerSelectedRow] ||
      !samplerFiles[samplerSelectedRow][0]) return;
  char msg[96];
  if (samplerTargetIsTrack && samplerTargetIsGranular) {
    // [2026-09-25] Meme liste/navigateur, destination GRANULAR_SAMPLE:
    // (voir startWavToGranular() cote Teensy) au lieu de TRACKSAMPLE:.
    snprintf(msg, sizeof(msg), "GRANULAR_SAMPLE:%s", samplerFiles[samplerSelectedRow]);
    sendToTeensy(msg);
    snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Chargement granular...");
  } else if (samplerTargetIsTrack) {
    // Fusion PATCH <-> navigateur SD (2026-09-23) : meme fichier, meme
    // liste, mais destination = TRACKSAMPLE:<piste> au lieu de
    // PADSAMPLE:<pad> -- voir loadWavIntoTrackSampler() cote Teensy.
    snprintf(msg, sizeof(msg), "TRACKSAMPLE:%d:%s", samplerTargetTrack, samplerFiles[samplerSelectedRow]);
    sendToTeensy(msg);
    snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Chargement piste %d...", samplerTargetTrack + 1);
  } else {
    snprintf(msg, sizeof(msg), "PADSAMPLE:%u:%s", samplerSelectedPad, samplerFiles[samplerSelectedRow]);
    sendToTeensy(msg);
    snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Chargement pad %u...", samplerSelectedPad);
  }
  drawSamplerPage();
}

// Ouvre le navigateur SD (ecran SAMPLEUR) cible sur la piste `track` au
// lieu des pads (2026-09-23, "fusion" demandee -- voir
// samplerTargetIsTrack). Repart toujours de la racine /samples : la
// derniere position de navigation des PADS (samplerFolder) n'a pas de
// raison de rester pertinente en changeant de cible, et inversement.
void goTo(Screen s);
void openSamplerForTrack(uint8_t track) {
  samplerTargetIsTrack = true;
  samplerTargetIsGranular = false;
  samplerTargetTrack = static_cast<int8_t>(track);
  snprintf(samplerFolder, sizeof(samplerFolder), "/samples");
  samplerOffset = 0;
  samplerSelectedRow = 0;
  samplerUiStatus[0] = '\0';
  goTo(Screen::Sampler);
}

// [2026-09-25] Meme navigateur, cible GRANULAR (voir samplerTargetIsGranular)
// -- demande utilisateur : pouvoir importer un WAV present sur la carte SD
// du Teensy pour le moteur granulaire, jusqu'ici uniquement charge au
// demarrage (kGranularStartupSample, cote Teensy) sans aucun moyen d'en
// choisir un autre depuis l'ecran.
void openSamplerForGranular(uint8_t track) {
  samplerTargetIsTrack = true;
  samplerTargetIsGranular = true;
  samplerTargetTrack = static_cast<int8_t>(track);
  snprintf(samplerFolder, sizeof(samplerFolder), "/samples");
  samplerOffset = 0;
  samplerSelectedRow = 0;
  samplerUiStatus[0] = '\0';
  goTo(Screen::Sampler);
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
constexpr uint8_t kSeqStepsPerMeasure = 16;
constexpr uint8_t kSeqMaxMeasures = 8;
constexpr uint8_t kSeqStepCount = kSeqStepsPerMeasure * kSeqMaxMeasures;
constexpr uint8_t kSeqVisibleSteps = kSeqStepsPerMeasure;
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
uint8_t patternMeasures[kPatternCount] = {1, 1, 1, 1, 1, 1, 1, 1};
uint8_t seqVisibleMeasure = 0;

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
const uint16_t kStepFxColors[] = {kDim, kPalette[1], kPalette[2], kPalette[3], kPalette[4], kPalette[0]};
uint8_t seqCurrentStep = 0;
bool seqPlaying = false;
bool seqRecording = false;
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
// Focus clavier du panneau latéral du tracker. Faux = grille NOTE/INST/...
// ; vrai = boutons MOTEUR/PATCH/EFFET/CLAVIER/METRO/SAUVER.
bool seqSideFocus = false;
uint8_t seqSideIndex = 0;
// 0 = lignes du tracker, 1 = selecteur de piste, 2 = selecteur de pattern.
uint8_t seqVerticalFocus = 0;

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
// CRUSH/DELAY (2026-09-23) : effets audio par piste (bitcrusher/echo),
// jusque-la sans aucun acces UI, rejoignent ARP/CUT/RET dans la meme
// colonne FX -- VAL devient un numero de patch pour ces deux-la (voir
// kCrushPresets[]/kDelayPresets[] cote Teensy), pas une valeur brute.
const char *const kStepFxNames[] = {"---", "ARP", "CUT", "RET", "CRUSH", "DELAY"};
constexpr uint8_t kStepFxCount = sizeof(kStepFxNames) / sizeof(kStepFxNames[0]);

void formatNoteName(uint8_t note, char *out, size_t outSize) {
  const int octave = static_cast<int>(note) / 12 - 1;  // note 60 = C4, convention M8/LSDJ
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
  const uint8_t firstVisible = static_cast<uint8_t>(seqVisibleMeasure * kSeqStepsPerMeasure);
  if (step < firstVisible || step >= firstVisible + kSeqVisibleSteps) return;
  const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
  const uint8_t visibleRow = static_cast<uint8_t>(step - firstVisible);
  const int16_t y = static_cast<int16_t>(kDetailTop + visibleRow * (kDetailRowH + kDetailRowGap));
  const bool on = seqStepOn[currentPattern][track][step];
  const bool rowSelected = (step == selectedSeqStep);
  const bool playhead = (step == seqCurrentStep);
  const uint16_t accent = kPalette[track % kPaletteCount];

  // kDetailGridRight (bord de la grille), PAS kSeqRightEdge (bord de
  // l'ECRAN, recouvrait le panneau lateral -- voir son commentaire).
  // Le pas lu doit rester visible sur la grille noire. L'ancien décalage
  // de luminosité (4) écrasait presque entièrement la couleur RGB565 : le
  // Teensy envoyait bien CLOCK, mais le curseur semblait absent.
  gfx->fillRect(kDetailLeft, y, kDetailGridRight - kDetailLeft, kDetailRowH,
                playhead ? dimColor(accent, 1) : RGB565_BLACK);
  if (playhead) {
    gfx->drawRect(kDetailLeft, y, kDetailGridRight - kDetailLeft, kDetailRowH,
                  RGB565_WHITE);
  }

  char buf[8];
  gfx->setTextSize(1);

  // PAS
  gfx->setTextColor(kDim);
  snprintf(buf, sizeof(buf), "%03d", step + 1);
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

// trackEngine[]/patchAccent() sont definis plus bas (page MOTEURS) --
// declares en avance ici pour que drawTrkTrackRow() puisse afficher le nom
// et la couleur du moteur de la piste (2026-09-23).
extern uint8_t trackEngine[];
uint16_t patchAccent(uint8_t track);

// Selecteur de piste ("< PISTE N >", meme motif que la page PATCH) --
// desormais AU-DESSUS des colonnes NOTE/INST/FX/VAL/PROB/COND, toujours visible
// (plus besoin de toucher deux fois un pas pour changer de piste).
void drawTrkTrackRow() {
  gfx->fillRect(kMargin, kTrkTrackRowY, kScreenSize - 2 * kMargin, kTrkTrackRowH, RGB565_BLACK);
  gfx->setTextSize(2);
  // Couleur + nom du moteur (pas juste la couleur de piste generique) --
  // meme motif que drawPatchTrackRow()/drawEngTrackRow() (2026-09-23,
  // "il faut ajouter le moteur dans la page principale du tracker") :
  // centrage dynamique puisque la largeur du texte varie selon le nom.
  const uint16_t accent = patchAccent(selectedSeqTrack);
  if (seqVerticalFocus == 1) gfx->drawRect(kMargin, kTrkTrackRowY, kScreenSize - 2 * kMargin, kTrkTrackRowH, accent);
  gfx->setTextColor(accent);
  char buf[32];
  snprintf(buf, sizeof(buf), "< PISTE %d - %s >", selectedSeqTrack + 1,
           az2::engineName(trackEngine[selectedSeqTrack]));  // +1 : affichage "plus musicien"
  const int16_t textW = static_cast<int16_t>(strlen(buf) * 12);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - textW / 2), kTrkTrackRowY);
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
constexpr int16_t kTrkControlsY = kDetailTop + kSeqVisibleSteps * (kDetailRowH + kDetailRowGap) + 6;
// Retour a la taille/disposition d'origine (2026-09-19, retour
// utilisateur explicite : "la ligne du bas on la laisse comme elle
// est") -- METRONOME (et les autres boutons) vont dans le panneau
// LATERAL (voir drawTrkSidePanel()), pas ici.
constexpr int16_t kTrkControlsH = 34;
constexpr int16_t kTrkControlsW = kScreenSize - 2 * kMargin;
constexpr int16_t kTrkPlayW = kTrkControlsW / 5;
constexpr int16_t kTrkRecX = kMargin + kTrkPlayW;
constexpr int16_t kTrkRecW = kTrkControlsW / 5;
constexpr int16_t kTrkBpmX = kTrkRecX + kTrkRecW;
constexpr int16_t kTrkBpmW = kTrkControlsW / 5;
constexpr int16_t kTrkDivX = kTrkBpmX + kTrkBpmW;
constexpr int16_t kTrkDivW = kTrkControlsW / 5;
constexpr int16_t kTrkLenX = kTrkDivX + kTrkDivW;
constexpr int16_t kTrkLenW = kTrkControlsW - kTrkPlayW - kTrkRecW - kTrkBpmW - kTrkDivW;

void drawTrkControls() {
  gfx->fillRect(kMargin, kTrkControlsY, kTrkControlsW, kTrkControlsH, RGB565_BLACK);
  gfx->drawRect(kMargin, kTrkControlsY, kTrkPlayW, kTrkControlsH, kFaint);
  gfx->drawRect(kTrkRecX, kTrkControlsY, kTrkRecW, kTrkControlsH, seqRecording ? RGB565_RED : kFaint);
  gfx->drawRect(kTrkBpmX, kTrkControlsY, kTrkBpmW, kTrkControlsH, kFaint);
  gfx->drawRect(kTrkDivX, kTrkControlsY, kTrkDivW, kTrkControlsH, kFaint);
  gfx->drawRect(kTrkLenX, kTrkControlsY, kTrkLenW, kTrkControlsH, kFaint);

  gfx->setTextSize(2);
  gfx->setTextColor(seqPlaying ? kPalette[1] : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kTrkControlsY + 8));
  gfx->print(seqPlaying ? "STOP" : "PLAY");
  gfx->setTextColor(seqRecording ? RGB565_RED : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kTrkRecX + 8), static_cast<int16_t>(kTrkControlsY + 8));
  gfx->print("REC");


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

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kTrkLenX + 4), static_cast<int16_t>(kTrkControlsY + 2));
  gfx->print("MESURES");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kTrkLenX + 4), static_cast<int16_t>(kTrkControlsY + 14));
  gfx->printf("%u", patternMeasures[currentPattern]);
}

bool hitTestTrkPlay(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kTrkControlsY, kTrkPlayW, kTrkControlsH);
}
bool hitTestTrkRec(int16_t x, int16_t y) {
  return inBox(x, y, kTrkRecX, kTrkControlsY, kTrkRecW, kTrkControlsH);
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
bool hitTestTrkLength(int16_t x, int16_t y) {
  return inBox(x, y, kTrkLenX, kTrkControlsY, kTrkLenW, kTrkControlsH);
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

constexpr int16_t kTrkSideH = kSeqVisibleSteps * (kDetailRowH + kDetailRowGap);

// Panneau lateral repense le 2026-09-19 (retour utilisateur sur
// materiel reel : "le cadre du patch est toujours pas bon" + "des
// bouton plus gros un bouton pour le clavier un pour les effet un
// pour le moteur et patch") -- l'ancien resume texte (moteur/patch/
// cutoff/reso/adsr) disparait completement, remplace par 5 boutons
// PLEINE LARGEUR empiles qui couvrent toute la hauteur du panneau :
// MOTEUR, PATCH, EFFET, PAD 4X4, METRONOME. Chacun ouvre directement
// la page correspondante pour la piste du tracker actuellement
// affichee (sauf METRONOME, une simple bascule, et EFFET qui reste
// dans le tracker mais amene le focus croix sur la colonne FX).
constexpr uint8_t kTrkSideBtnCount = 6;
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

  drawTrkSideBtn(0, accent, seqSideFocus && seqSideIndex == 0, "MOTEUR");
  drawTrkSideBtn(1, accent, seqSideFocus && seqSideIndex == 1, "PATCH");
  drawTrkSideBtn(2, kPalette[2], seqSideFocus && seqSideIndex == 2, "EFFET");
  drawTrkSideBtn(3, kPalette[4], seqSideFocus && seqSideIndex == 3, "PAD 4X4");
  drawTrkSideBtn(4, metronomeOn ? kPalette[3] : kFaint, seqSideFocus && seqSideIndex == 4, "METRO");
  // 2026-09-19, "dans le tracker il manque le bouton sauvegarder" --
  // sauvegarde tout le morceau (voir saveProject()) dans l'emplacement
  // PROJET actuellement choisi (projectSlot, meme emplacement que la
  // page PROJET -- pas un 2e systeme de slots), sans quitter le
  // tracker. Rempli en vert un court instant apres l'appui (voir le
  // hitTest correspondant plus bas) -- seul retour visuel disponible,
  // la sauvegarde elle-meme reste quasi instantanee.
  drawTrkSideBtn(5, RGB565(60, 200, 90), seqSideFocus && seqSideIndex == 5, "SAUVER");
}

void drawSeqDetailPage() {
  char title[32];
  snprintf(title, sizeof(title), "PATTERN %u  M%u/%u", currentPattern + 1, seqVisibleMeasure + 1,
           patternMeasures[currentPattern]);
  drawSubHeader(title, kPalette[0]);
  if (seqVerticalFocus == 2) gfx->drawRect(88, 2, 238, 43, kPalette[0]);
  drawTrkTrackRow();
  drawDetailHeader();
  const uint8_t first = static_cast<uint8_t>(seqVisibleMeasure * kSeqStepsPerMeasure);
  for (uint8_t row = 0; row < kSeqVisibleSteps; ++row) {
    drawDetailRow(static_cast<uint8_t>(first + row));
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
void drawStepSeqPage();  // definie plus bas, voir son commentaire (apres patchAccent())

void switchToPattern(uint8_t p) {
  currentPattern = static_cast<uint8_t>(p % kPatternCount);
  seqVisibleMeasure = 0;
  selectedSeqStep = 0;
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
  for (uint8_t row = 0; row < kSeqVisibleSteps; ++row) {
    const int16_t rowY = static_cast<int16_t>(kDetailTop + row * (kDetailRowH + kDetailRowGap));
    if (y >= rowY && y < rowY + kDetailRowH) {
      return static_cast<int8_t>(seqVisibleMeasure * kSeqStepsPerMeasure + row);
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

// Couleur d'identite du moteur `engine` (voir kEngineAccent[] plus haut),
// et raccourci pour "le moteur actuellement charge sur cette piste" --
// utilises par la page PATCH pour que toute la page se colore selon le
// moteur en cours d'edition plutot que selon le numero de piste.
uint16_t engineAccent(uint8_t engine) {
  return engine < az2::kEngineCount ? kEngineAccent[engine] : kFaint;
}
uint16_t patchAccent(uint8_t track) {
  return engineAccent(trackEngine[track]);
}

// ---------------------------------------------------------------------
// Page SEQ. PAS -- vue alternative du sequenceur, style step-sequencer
// "traditionnel" (OP-1 : gros blocs par piste, un ecran = une mesure de
// 16 pas) demandee en plus du tracker dense existant (drawSeqDetailPage),
// PAS a sa place. Memes donnees exactement (seqStepOn/seqStepNote/
// seqStepProb, currentPattern, selectedSeqTrack) -- change seulement la
// facon de les montrer/editer. Un seul curseur de pas partage avec le
// tracker (selectedSeqStep, voir seqVisibleMeasure) : passer d'une vue a
// l'autre garde la position.
// ---------------------------------------------------------------------
constexpr int16_t kStepSeqTabH = 22;
constexpr int16_t kStepSeqCols = 8;
constexpr int16_t kStepSeqRows = 4;
constexpr int16_t kStepSeqGap = 6;
// [2026-09-26] 32 pas visibles d'un coup (demande, "on peut afficher 32")
// au lieu de 16 -- fenetre INDEPENDANTE de seqVisibleMeasure (qui reste
// geree par le tracker classique pour ses propres besoins d'affichage) :
// cette page calcule sa propre fenetre de 32 directement a partir de
// selectedSeqStep, sans toucher a l'etat du tracker.
constexpr uint8_t kStepSeqWindow = kStepSeqCols * kStepSeqRows;

constexpr int16_t kStepSeqTabsY = 84;
constexpr int16_t kStepSeqGridTop = 134;
constexpr int16_t kStepSeqCellH = 46;

uint8_t stepSeqFirstStep() {
  return static_cast<uint8_t>((selectedSeqStep / kStepSeqWindow) * kStepSeqWindow);
}

void drawStepSeqPage() {
  drawSubHeader("SEQ. PAS", kEngineAccent[trackEngine[selectedSeqTrack]]);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, 70);
  gfx->printf("PATTERN %02u", currentPattern + 1);

  // Onglets de piste (8), lisere colore par moteur assigne, piste
  // courante plus epaisse -- tactiles, voir handleTouchDown().
  const int16_t tabW = (kScreenSize - 2 * kMargin - (kSeqTrackCount - 1) * 4) / kSeqTrackCount;
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    const int16_t tx = static_cast<int16_t>(kMargin + t * (tabW + 4));
    const uint16_t accent = patchAccent(t);
    const bool cur = (t == selectedSeqTrack);
    gfx->drawRect(tx, kStepSeqTabsY, tabW, kStepSeqTabH, accent);
    if (cur) {
      gfx->drawRect(static_cast<int16_t>(tx + 1), static_cast<int16_t>(kStepSeqTabsY + 1),
                    static_cast<int16_t>(tabW - 2), static_cast<int16_t>(kStepSeqTabH - 2), accent);
    }
  }

  const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
  const uint8_t firstStep = stepSeqFirstStep();
  const uint8_t localStep = static_cast<uint8_t>(selectedSeqStep - firstStep);
  const uint8_t activeSteps = static_cast<uint8_t>(patternMeasures[currentPattern] * kSeqStepsPerMeasure);
  const uint16_t accent = patchAccent(track);

  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(kMargin, 116);
  gfx->printf("%u %s   PAS %02u", track + 1, az2::engineName(trackEngine[track]), selectedSeqStep + 1);

  // Grille 4x8 (32 pas) -- les pas au-dela de la longueur reelle du
  // pattern (activeSteps) restent visibles mais attenues/non tactiles,
  // pour que la fenetre garde toujours 32 cases meme sur un pattern d'1
  // seule mesure.
  const int16_t gridW = kScreenSize - 2 * kMargin;
  const int16_t cellW = (gridW - (kStepSeqCols - 1) * kStepSeqGap) / kStepSeqCols;
  for (uint8_t i = 0; i < kStepSeqWindow; ++i) {
    const uint8_t row = i / kStepSeqCols;
    const uint8_t col = i % kStepSeqCols;
    const int16_t bx = static_cast<int16_t>(kMargin + col * (cellW + kStepSeqGap));
    const int16_t by = static_cast<int16_t>(kStepSeqGridTop + row * (kStepSeqCellH + kStepSeqGap));
    const uint8_t globalStep = static_cast<uint8_t>(firstStep + i);
    const bool beyondPattern = globalStep >= activeSteps;
    const bool on = !beyondPattern && seqStepOn[currentPattern][track][globalStep];
    const bool cur = (i == localStep);
    gfx->fillRect(bx, by, cellW, kStepSeqCellH, on ? accent : RGB565_BLACK);
    gfx->drawRect(bx, by, cellW, kStepSeqCellH, on ? accent : (beyondPattern ? RGB565(30, 30, 36) : kFaint));
    if (cur) {
      gfx->drawRect(static_cast<int16_t>(bx - 2), static_cast<int16_t>(by - 2),
                    static_cast<int16_t>(cellW + 4), static_cast<int16_t>(kStepSeqCellH + 4), RGB565_WHITE);
    }
    gfx->setTextSize(1);
    gfx->setTextColor(on ? RGB565_BLACK : (beyondPattern ? RGB565(40, 40, 48) : kFaint));
    gfx->setCursor(static_cast<int16_t>(bx + 3), static_cast<int16_t>(by + kStepSeqCellH - 11));
    gfx->print(globalStep + 1);
  }

  // Panneau d'info du pas au curseur : note + probabilite (pas de
  // vitesse/velocite par pas dans le modele de donnees actuel).
  const int16_t infoY = static_cast<int16_t>(kStepSeqGridTop + kStepSeqRows * (kStepSeqCellH + kStepSeqGap) + 4);
  gfx->drawRect(kMargin, infoY, gridW, 44, kFaint);
  char noteBuf[8];
  formatNoteName(seqStepNote[currentPattern][track][selectedSeqStep], noteBuf, sizeof(noteBuf));
  gfx->setTextSize(2);
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(kMargin + 12), static_cast<int16_t>(infoY + 12));
  gfx->print(noteBuf);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(kMargin + 90), static_cast<int16_t>(infoY + 10));
  gfx->printf("PROB %u%%", seqStepProb[currentPattern][track][selectedSeqStep]);
  gfx->setCursor(static_cast<int16_t>(kMargin + 90), static_cast<int16_t>(infoY + 26));
  gfx->print(seqPlaying ? "PLAY" : "STOP");

  gfx->setTextColor(kFaint);
  gfx->setCursor(kMargin, static_cast<int16_t>(kStatusY - 14));
  gfx->print("Toucher : piste/pas -- ENC2 piste, ENC3 note, C play/stop");
}

// Tactile : ligne des onglets de piste, ou l'une des 32 cases de la
// grille (voir drawStepSeqPage() pour la geometrie identique -- meme
// constantes reutilisees pour rester synchronisees).
int8_t hitTestStepSeqTab(int16_t x, int16_t y) {
  if (y < kStepSeqTabsY || y >= kStepSeqTabsY + kStepSeqTabH) return -1;
  const int16_t tabW = (kScreenSize - 2 * kMargin - (kSeqTrackCount - 1) * 4) / kSeqTrackCount;
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    const int16_t tx = static_cast<int16_t>(kMargin + t * (tabW + 4));
    if (x >= tx && x < tx + tabW) return static_cast<int8_t>(t);
  }
  return -1;
}

int8_t hitTestStepSeqCell(int16_t x, int16_t y) {
  const int16_t gridW = kScreenSize - 2 * kMargin;
  const int16_t cellW = (gridW - (kStepSeqCols - 1) * kStepSeqGap) / kStepSeqCols;
  if (y < kStepSeqGridTop || x < kMargin) return -1;
  const int16_t relY = static_cast<int16_t>(y - kStepSeqGridTop);
  const int16_t relX = static_cast<int16_t>(x - kMargin);
  const int16_t row = relY / (kStepSeqCellH + kStepSeqGap);
  const int16_t col = relX / (cellW + kStepSeqGap);
  if (row < 0 || row >= kStepSeqRows || col < 0 || col >= kStepSeqCols) return -1;
  if (relY % (kStepSeqCellH + kStepSeqGap) >= kStepSeqCellH) return -1;  // dans le gap vertical
  if (relX % (cellW + kStepSeqGap) >= cellW) return -1;  // dans le gap horizontal
  return static_cast<int8_t>(row * kStepSeqCols + col);
}

// Annonce par announceHello() cote Teensy (RACK_CAP:0/1, audit 2026-09-22) :
// vrai seulement si CE Teensy est compile avec AZ2_EXTERNAL_RACK, donc si
// GRANULAR/SPECTRAL produisent reellement un son. Faux par defaut (avant
// le premier HELLO) -- comportement le plus sur, la liste MOTEURS grise
// ces deux entrees tant qu'on n'a pas la confirmation du Teensy en face.
// Mise a jour dans handleTeensyLine(), lue par drawEngListRow().
bool teensyRackCapable = false;

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
uint8_t engineParamBank = 0;
uint32_t engineAnimFrame = 0;
uint32_t engineAnimLastMs = 0;
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
// Sept moteurs tiennent dans la page avec une hauteur compacte ; le même
// gabarit reste lisible lorsque le catalogue repasse à six.
constexpr int16_t kEngListRowH = 28;
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
  const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
  // Meme traitement que drawPatchTrackRow() (2026-09-22, identite par
  // moteur) : nom du moteur inclus et colore par moteur, centrage
  // dynamique puisque la largeur du texte varie selon le nom.
  gfx->setTextColor(patchAccent(t));
  char buf[32];
  snprintf(buf, sizeof(buf), "< PISTE %d - %s >", t + 1, az2::engineName(trackEngine[t]));  // +1 : affichage "plus musicien"
  const int16_t textW = static_cast<int16_t>(strlen(buf) * 12);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - textW / 2), kEngTrackRowY);
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
  // Couleur du moteur DE CETTE LIGNE, pas de la piste (2026-09-22,
  // identite par moteur -- voir kEngineAccent[]) : la liste MOTEURS montre
  // les 9 moteurs a la fois, chacun garde sa propre couleur qu'il soit
  // selectionne ou non sur N'IMPORTE QUELLE piste.
  const uint16_t accent = engineAccent(engineIdx);
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
  // Texte attenue pour GRANULAR/SPECTRAL si ce Teensy n'a pas confirme
  // AZ2_EXTERNAL_RACK (RACK_CAP:, voir teensyRackCapable/handleTeensyLine())
  // -- avant ce garde-fou (audit 2026-09-22), ces deux entrees se
  // selectionnaient normalement sur le couple de firmwares de production
  // sans jamais produire le moindre son.
  const bool rackDisabled = !teensyRackCapable &&
      (engineIdx == az2::kEngineGranular || engineIdx == az2::kEngineSpectral);
  gfx->setTextColor(isCurrent ? RGB565_BLACK : (rackDisabled ? kFaint : RGB565_WHITE));
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
  const uint16_t accent = patchAccent(t);

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
    const uint16_t accent = patchAccent(static_cast<uint8_t>(selectedEngineTrack));
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
  const uint16_t accent = patchAccent(t);
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

// Zone graphique compacte : une silhouette par moteur et deux valeurs
// directement pilotables par les potentiometres 2/3. Elle reste dans le
// bandeau existant pour conserver la navigation tactile actuelle.
void drawEngVisualizer() {
  const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
  const uint16_t accent = patchAccent(t);
  const int16_t x = kMargin;
  const int16_t y = kEngMiniY;
  const int16_t w = static_cast<int16_t>(kScreenSize - 2 * kMargin);
  const int16_t h = kEngMiniH;
  const uint8_t phase = static_cast<uint8_t>(engineAnimFrame & 0x3f);
  gfx->fillRect(x, y, w, h, RGB565_BLACK);
  gfx->drawRect(x, y, w, h, accent);
  const int16_t cx = static_cast<int16_t>(x + 32);
  const int16_t cy = static_cast<int16_t>(y + h / 2 + 3);
  gfx->drawCircle(cx, cy, 20, kFaint);
  gfx->drawCircle(cx, cy, static_cast<int16_t>(10 + phase / 8), accent);
  if (trackEngine[t] == az2::kEngineDexed) {
    for (uint8_t i = 0; i < 4; ++i) {
      gfx->drawLine(cx - 18 + i * 12, cy - 15, cx - 9 + i * 12, cy + 15, accent);
    }
  } else if (trackEngine[t] == az2::kEngineDrum) {
    gfx->fillCircle(cx, cy, static_cast<int16_t>(5 + phase / 16), accent);
    gfx->drawCircle(cx, cy, 15, accent);
  } else if (trackEngine[t] == az2::kEngineSampler) {
    gfx->drawLine(cx - 18, cy + 10, cx - 6, cy - 12, accent);
    gfx->drawLine(cx - 6, cy - 12, cx + 8, cy + 5, accent);
    gfx->drawLine(cx + 8, cy + 5, cx + 18, cy - 15, accent);
  } else {
    for (int16_t i = -18; i < 18; i += 3) {
      gfx->drawPixel(static_cast<int16_t>(cx + i), static_cast<int16_t>(cy + ((i * 7 + phase * 3) % 18)), accent);
    }
  }
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 66), static_cast<int16_t>(y + 4));
  gfx->print(engineParamBank == 0 ? "FILTRE  POT2/POT3" : "PATCH  POT2/POT3");
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + 66), static_cast<int16_t>(y + 16));
  gfx->print(az2::engineName(trackEngine[t]));
  char buf[36];
  if (engineParamBank == 0) {
    snprintf(buf, sizeof(buf), "CUTOFF %3u  RESO %3u", trackCutoff[t], trackReso[t]);
  } else if (trackEngine[t] == az2::kEngineDexed) {
    snprintf(buf, sizeof(buf), "ALGO %2u  FDBK %u", static_cast<unsigned>(trackAlgo[t] + 1), trackFeedback[t]);
  } else {
    snprintf(buf, sizeof(buf), "ATTACK %3u  DECAY %3u", trackAttack[t], trackDecay[t]);
  }
  gfx->setTextColor(accent);
  gfx->setCursor(static_cast<int16_t>(x + 66), static_cast<int16_t>(y + 30));
  gfx->print(buf);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 66), static_cast<int16_t>(y + 47));
  gfx->print("B / ENC1 : changer de banque");
}

bool hitTestEngMini(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kEngMiniY, static_cast<int16_t>(kScreenSize - 2 * kMargin), kEngMiniH);
}

void drawEnginesPage() {
  drawSubHeader("MOTEURS", kPalette[2]);
  drawEngTrackRow();
  drawEngLists();
  drawEngVisualizer();
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
// Focus sur l'oscilloscope (2026-09-23, "je suis coince dans les patch,
// je peux pas descendre ... passer par l'onde affichee pour regler les
// patch") : atteint en appuyant BAS depuis la ligne SLOT (jusque-la la
// derniere ligne, BAS n'y faisait rien -- voir patchStepVisual()).
// UN SEUL point ADSR actif a la fois (2026-09-23, suite -- "une petite
// boule sur les points, en cliquant sur le bouton de l'encodeur on passe
// au suivant") : cliquer le bouton-poussoir d'un des 2 encodeurs 2/3 (ou
// GAUCHE/DROITE, meme effet, plus accessible) avance/recule dans
// ATTACK->DECAY->SUSTAIN->RELEASE->ATTACK... ; tourner N'IMPORTE LEQUEL
// des 2 encodeurs regle la valeur du point actuellement selectionne
// (indifferemment lequel -- pas 2 parametres distincts par encodeur ici).
bool patchOnScopeRow = false;
uint8_t patchScopeAdsrPoint = 0;  // 0=ATTACK 1=DECAY 2=SUSTAIN 3=RELEASE
// Meme principe que patchScopeAdsrPoint, pour l'encodeur 2 (2026-09-24,
// "on va refaire leur config globale ... sur le 2 et 3 encodeur") :
// dedie CUTOFF/RESONANCE en permanence sur la page PATCH, cliquer son
// bouton-poussoir bascule l'un vers l'autre. Remplace l'ancien systeme
// par ligne (patchEncoderLogicalRow()) pour cet encodeur -- toujours
// utilisable pour VOLUME/SLOT/extra via A maintenu + croix.
uint8_t patchScopeFilterPoint = 0;  // 0=CUTOFF 1=RESONANCE
// Focus de la liste de presets placee a droite de l'oscilloscope. Cette
// liste etait tactile uniquement : avec la croix il etait impossible de
// choisir CLOUD/AIR/etc. La page PATCH s'ouvre maintenant sur cette liste ;
// A entre/sort de l'edition, HAUT/BAS choisissent alors le preset. Hors
// edition, BAS/DROITE descend dans les reglages comme dans les autres pages.
bool patchOnPresetList = true;
bool patchPresetEditing = false;  // A bascule navigation <-> modification du cadre
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
// Valeurs par defaut NON NULLES (2026-09-24, "on ne voit pas l'ADSR" --
// la courbe ecrasee/invisible sur l'oscilloscope venait de la, pas d'un
// bug de rendu) : le Teensy n'a JAMAIS memorise ces reglages nulle part
// (FILT:/ENV: appliquent directement l'AudioFilterStateVariable/
// AudioEffectEnvelope sans garder la valeur brute 0-127 recue) --
// announceHello() ne peut donc pas les rappeler a la connexion comme il
// le fait pour ENGINE:/PATCH:/SMODE:. L'ECRAN est en pratique la seule
// source de verite pour ces 6 valeurs ; les laisser a 0 par defaut (C++
// zero-initialise les tableaux globaux) rendait l'ancien affichage
// texte juste trompeur (montrait "000"), mais rendait la NOUVELLE
// courbe ADSR litteralement ecrasee sur elle-meme (voir
// drawPatchScopeAdsrOverlay(), segW(0) = largeur minimale). Choisies
// pour donner un son et une courbe raisonnables des le premier contact ;
// poussees au Teensy a chaque ouverture de PATCH (voir goTo()) pour que
// le son reel corresponde enfin a ce qui est affiche.
uint8_t trackCutoff[kSeqTrackCount] = {127, 127, 127, 127, 127, 127, 127, 127};  // 127 = 15000Hz (voir handleFiltCommand())
uint8_t trackReso[kSeqTrackCount] = {};                                          // 0 = resonance minimale (0.7)
uint8_t trackAttack[kSeqTrackCount] = {10, 10, 10, 10, 10, 10, 10, 10};
uint8_t trackDecay[kSeqTrackCount] = {25, 25, 25, 25, 25, 25, 25, 25};
uint8_t trackSustain[kSeqTrackCount] = {90, 90, 90, 90, 90, 90, 90, 90};
uint8_t trackRelease[kSeqTrackCount] = {40, 40, 40, 40, 40, 40, 40, 40};
// Reglages propres au moteur DEXED (DXP:, voir handleDexedParamCommand()
// cote Teensy) -- demande 2026-09-16 ("il faut des reglages, on a pas de
// reglages dans la fenetre dexed du tracker") : l'ADSR generique
// (ENV:) n'est PAS ecoutee par Dexed (sa propre EG interne au patch DX7
// la remplace), une piste Dexed n'avait donc aucun reglage utile sur la
// page PATCH avant ca. Algorithme stocke brut 0-31 (affiche +1, cf.
// convention DX7 1-32), feedback 0-7.
uint8_t trackAlgo[kSeqTrackCount] = {};
uint8_t trackFeedback[kSeqTrackCount] = {};
uint8_t rackParamVal[kSeqTrackCount][az2::kRackGranularParamCount] = {};
uint8_t patchExtraVal[kSeqTrackCount][17] = {};

// Miroir cote ecran de rackOwnerTrack[] (source de verite cote Teensy,
// voir son commentaire dans src_teensy/az2_audio/main.cpp) -- tenu a jour
// par les echos RACK_OWNER: (handleTeensyLine()). -1 = pas encore connu/
// libre. GRANULAR et SPECTRAL n'existant qu'en un seul exemplaire
// physique sur le rack externe, seule la piste proprietaire doit
// envoyer RACK_PARAM: en direct (sendPatchExtra()/patchApplyDelta(),
// SEUL chemin qui contourne le modele par piste normal -- voir le
// commentaire de rackClaimOwnership() cote Teensy).
int8_t rackOwnerTrack[az2::kRackEngineCount] = {-1, -1};

bool isRackTrack(uint8_t track) {
  return trackEngine[track] == az2::kEngineGranular ||
         trackEngine[track] == az2::kEngineSpectral;
}
uint8_t rackEngineForTrack(uint8_t track) {
  return trackEngine[track] == az2::kEngineGranular ? az2::kRackEngineGranular
                                                    : az2::kRackEngineSpectral;
}
// false si "track" s'est fait voler son moteur rack par une autre piste
// (voir rackOwnerTrack[] plus haut) -- utilise pour ne PAS envoyer
// RACK_PARAM: en direct depuis une piste qui ne controle plus la voix
// physique (sendPatchExtra()/patchApplyDelta()).
bool isRackOwner(uint8_t track) {
  return isRackTrack(track) && rackOwnerTrack[rackEngineForTrack(track)] == static_cast<int8_t>(track);
}
void loadRackPresetValues(uint8_t track, uint8_t patch) {
  if (!isRackTrack(track) || patch >= az2::kRackPatchCount) return;
  const uint8_t count = az2::rackParamCount(rackEngineForTrack(track));
  for (uint8_t parameter = 0; parameter < count; ++parameter) {
    const uint8_t value = trackEngine[track] == az2::kEngineGranular
                              ? az2::kRackGranularPresets[patch][parameter]
                              : az2::kRackSpectralPresets[patch][parameter];
    rackParamVal[track][parameter] = value;
    if (parameter >= 6) patchExtraVal[track][parameter - 6] = value;
  }
}
const char *const kPatchLabels[6] = {"CUTOFF", "RESONANCE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
const char *const kPatchLabelsDexed[6] = {"CUTOFF", "RESONANCE", "ALGO (DX7)", "FEEDBACK", "--", "--"};

// Lignes 2-5 de la page PATCH : ADSR generique, SAUF pour les pistes rack
// (ce sont de vrais parametres GRANULAR/SPECTRAL distincts, voir
// isRackTrack()/rackParamName(), rien a voir avec l'ADSR) et Dexed (ALGO/
// FEEDBACK en 2-3, rows 4-5 deja sans effet chez lui). Retirees d'ici le
// 2026-09-24 ("ça enleve 4 boutons de la page patch [...] les boutons
// encodeur pour regler l'ADSR") : l'ADSR se regle desormais UNIQUEMENT
// via l'oscilloscope (patchOnScopeRow, encodeurs 2/3) -- ces 4 lignes
// fixes encombraient la navigation croix pour rien une fois ce chemin en
// place. patchStepVisual()/patchStepLogical() sautent deja les lignes
// inactives (meme mecanisme que Dexed 4-5 avant ce changement), donc BAS
// depuis RESONANCE (ligne 1) va desormais direct aux lignes extra/VOLUME/
// SLOT, plus besoin de traverser 4 lignes mortes pour "descendre
// facilement". Ne concerne QUE les 6 lignes fixes (0-5) -- "row < 6"
// ajoute avec les lignes extra (2026-09-18) : sans lui, ce test
// desactiverait a tort TOUTES les lignes extra (6 et plus). Les appelants
// ne doivent d'ailleurs appeler ceci que pour row < 6 (voir
// patchExtraCount()/patchVolRow() pour le reste de la page) -- le clamp
// reste par securite si jamais appele hors de ce domaine.
bool patchRowActive(uint8_t track, uint8_t row) {
  if (isRackTrack(track)) return true;
  if (trackEngine[track] == az2::kEngineDexed) return !(row >= 4 && row < 6);
  return row < 2;
}

const char *patchRowLabel(uint8_t track, uint8_t row) {
  if (isRackTrack(track)) return az2::rackParamName(rackEngineForTrack(track), row);
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
  if (isRackTrack(track)) return rackParamVal[track][row];
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
    case az2::kEngineSampler: return 2;  // MODE (one-shot/gate) + SAMPLE (2026-09-23, choix libre d'un WAV)
    // +1 (2026-09-25) : derniere ligne = SAMPLE, meme principe que Sampler
    // ci-dessus -- ouvre le navigateur SD au lieu d'etre un parametre RACK_PARAM
    // numerique (voir sendPatchExtra()). SPECTRAL non concerne pour l'instant.
    case az2::kEngineGranular: return az2::kRackGranularParamCount - 6 + 1;
    case az2::kEngineSpectral: return az2::kRackSpectralParamCount - 6;
    default: return 0;
  }
}
uint8_t patchVolRow(uint8_t track) { return static_cast<uint8_t>(6 + patchExtraCount(track)); }
uint8_t patchSlotRow(uint8_t track) { return static_cast<uint8_t>(patchVolRow(track) + 1); }
uint8_t patchTotalRows(uint8_t track) { return static_cast<uint8_t>(patchSlotRow(track) + 1); }

// Liste ORDONNEE des lignes logiques "gardees" -- actives dans 0-5 (voir
// patchRowActive(), l'ADSR n'y figure plus pour la plupart des moteurs
// depuis le 2026-09-24), toutes les lignes extra (6..volRow-1, toujours
// gardees), puis VOLUME (volRow). SLOT n'y figure jamais : elle reste
// TOUJOURS seule sur sa propre ligne visuelle juste apres, geree a part
// par les appelants (meme convention qu'avant ce changement). Utilisee
// par patchVisualRow()/patchIsRightCol() (position avant/apres) ET par
// drawPatchWindow() (l'inverse -- quelles lignes vont dans tel numero de
// ligne visuelle) : centralise ici pour que les deux sens restent
// coherents, plutot que deux implementations paralleles qui pourraient
// diverger.
constexpr uint8_t kPatchMaxKeptRows = 24;  // 6 fixes + 17 extra (DEXED, le pire cas) + volume, marge incluse
uint8_t patchKeptRows(uint8_t track, uint8_t (&out)[kPatchMaxKeptRows]) {
  const uint8_t volRow = patchVolRow(track);
  uint8_t count = 0;
  for (uint8_t r = 0; r <= volRow && count < kPatchMaxKeptRows; ++r) {
    if (r < 6 && !patchRowActive(track, r)) continue;
    out[count++] = r;
  }
  return count;
}
int8_t patchKeptIndexOf(uint8_t track, uint8_t logicalRow) {
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t count = patchKeptRows(track, kept);
  for (uint8_t i = 0; i < count; ++i) {
    if (kept[i] == logicalRow) return static_cast<int8_t>(i);
  }
  return -1;
}

// 2 parametres par ligne VISUELLE (2026-09-19, "mettre 2 reglage par
// ligne pour gagner de la place") -- tous les parametres "simples"
// (0..patchVolRow(track) inclus, donc VOLUME participe aussi a la
// paire) partagent leur ligne 2 par 2 ; SLOT (le dernier, 3 sous-
// boutons deja denses -- SLOT/SAVE/LOAD) reste TOUJOURS seul sur sa
// propre ligne. logicalRow reste le meme index qu'avant (0-5 base,
// 6..volRow-1 extra, volRow=volume, slotRow=slot) -- seule la
// POSITION A L'ECRAN change ici, pas la numerotation logique (utilisee
// par selectedPatchRow/patchScroll, la navigation croix reste
// coherente sans etre reecrite en profondeur). Compacte les lignes
// INACTIVES (2026-09-24) : leur position dans kept[] est ce qui compte,
// pas leur numero logique brut.
int16_t patchVisualRow(uint8_t track, uint8_t logicalRow) {
  const uint8_t volRow = patchVolRow(track);
  if (logicalRow <= volRow) {
    const int8_t idx = patchKeptIndexOf(track, logicalRow);
    return static_cast<int16_t>((idx < 0 ? 0 : idx) / 2);
  }
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t count = patchKeptRows(track, kept);
  return static_cast<int16_t>((count + 1) / 2);  // SLOT juste apres les paires
}
uint8_t patchTotalVisualRows(uint8_t track) {
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t count = patchKeptRows(track, kept);
  return static_cast<uint8_t>((count + 1) / 2 + 1);
}
// true = colonne DROITE (position impaire dans kept[]), false = GAUCHE --
// sans objet pour la ligne SLOT (toujours pleine largeur, jamais appele
// pour elle).
bool patchIsRightCol(uint8_t track, uint8_t logicalRow) {
  const int8_t idx = patchKeptIndexOf(track, logicalRow);
  return idx >= 0 && (idx % 2) == 1;
}

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
  const bool wasRight = (row <= static_cast<int8_t>(volRow)) && patchIsRightCol(track, static_cast<uint8_t>(row));
  int16_t visualRow = patchVisualRow(track, static_cast<uint8_t>(row));
  visualRow = static_cast<int16_t>(visualRow + dir);
  if (visualRow < 0) {
    return -1;
  }
  if (visualRow > slotVisual) {
    return row;  // BAS depuis SLOT : ne bouge pas (voir patchOnScopeRow, l'appelant gere l'entree dans le focus oscilloscope)
  }
  if (visualRow == slotVisual) {
    return static_cast<int8_t>(slotRow);
  }
  // kept[] (voir patchKeptRows()) ne contient QUE des lignes actives par
  // construction -- plus besoin de boucler pour sauter les lignes
  // inactives (2026-09-24, ancien commentaire "ligne inactive, continue
  // de chercher" : le filtrage se fait maintenant en amont, dans
  // patchKeptRows()/patchRowActive(), pas ici).
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t keptCount = patchKeptRows(track, kept);
  const uint8_t leftIdx = static_cast<uint8_t>(visualRow * 2);
  const uint8_t rightIdx = static_cast<uint8_t>(leftIdx + 1);
  const uint8_t idx = (wasRight && rightIdx < keptCount) ? rightIdx : leftIdx;
  return idx < keptCount ? static_cast<int8_t>(kept[idx]) : row;
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
    case az2::kEngineSampler: return extraIdx == 0 ? "MODE" : "SAMPLE";
    case az2::kEngineGranular:
      if (extraIdx == az2::kRackGranularParamCount - 6) return "SAMPLE";
      return az2::rackParamName(rackEngineForTrack(track), extraIdx + 6);
    case az2::kEngineSpectral:
      return az2::rackParamName(rackEngineForTrack(track), extraIdx + 6);
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
  if (trackEngine[track] == az2::kEngineSampler) {
    return 1;
  }
  if (trackEngine[track] == az2::kEngineGranular && extraIdx == az2::kRackGranularParamCount - 6) {
    return 1;  // ligne SAMPLE, meme convention cosmetique que Sampler ci-dessus
  }
  return 127;
}

// Valeur courante de chaque ligne extra, par piste -- 17 = le plus
// grand des 3 moteurs concernes (DEXED), reutilise tel quel pour
// EPIANO (12) et BRAIDS (2), le reste de la ligne n'etant simplement
// jamais lu/affiche pour ces moteurs (voir patchExtraCount()).
uint8_t trackSamplerGate[kSeqTrackCount] = {};
// Chemin SD (carte du Teensy) charge dans le buffer CUSTOM de chaque
// piste (2026-09-23, voir samplerTargetIsTrack plus haut) -- "" = aucun.
// Meme convention que padSamplePath[] : jamais ecrit directement ici,
// seulement via l'echo TRACKSAMPLE:<piste>:READY:path=.../CLEARED cote
// Teensy (voir handleTeensyLine()), donc toujours synchronise avec ce qui
// est reellement charge.
char trackSamplePath[kSeqTrackCount][64] = {};

// Panneau affiche par drawSamplerPage() a la place de la grille de 16
// pads quand samplerTargetIsTrack est actif (2026-09-23, voir son
// commentaire pres de la declaration de drawSamplerPage()). Definie ici
// (et non avec le reste de l'ecran SAMPLEUR, bien plus haut dans ce
// fichier) car elle a besoin de patchAccent()/trackEngine[]/
// trackSamplePath[], tous declares apres cet ancien emplacement.
void drawSamplerTrackPanel(uint8_t track) {
  const uint16_t accent = patchAccent(track);
  gfx->fillRect(20, 96, 196, 196, RGB565_BLACK);
  gfx->drawRect(20, 96, 196, 196, accent);
  gfx->setTextColor(accent);
  gfx->setTextSize(2);
  gfx->setCursor(36, 116);
  gfx->printf("PISTE %d", track + 1);
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(36, 148);
  gfx->print(az2::engineName(trackEngine[track]));
  gfx->setTextColor(kDim);
  gfx->setCursor(36, 172);
  gfx->print(trackSamplePath[track][0] ? "sample actuel :" : "aucun sample");
  if (trackSamplePath[track][0]) {
    gfx->setTextColor(RGB565_WHITE);
    const char *name = strrchr(trackSamplePath[track], '/');
    gfx->setCursor(36, 188);
    gfx->print(String(name ? name + 1 : trackSamplePath[track]).substring(0, 28));
  }
}

void sendPatchExtra(uint8_t track, uint8_t extraIdx) {
  char msg[48];
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
    case az2::kEngineSampler:
      if (extraIdx == 1) {
        // Ligne SAMPLE (2026-09-23) : pas un reglage numerique -- ouvre
        // directement le navigateur SD (voir openSamplerForTrack()) des
        // que l'utilisateur "regle" cette ligne (A + HAUT/BAS, meme geste
        // que n'importe quel autre parametre). patchExtraVal[] vient
        // d'etre modifie par le delta qui a mene ici mais n'est jamais
        // lu pour cette ligne (voir patchExtraLabel()/drawPatchExtraRow()) --
        // sans consequence.
        openSamplerForTrack(track);
        return;
      }
      snprintf(msg, sizeof(msg), "SMODE:%d:%d", track, patchExtraVal[track][extraIdx] ? 1 : 0);
      break;
    case az2::kEngineGranular:
      if (extraIdx == az2::kRackGranularParamCount - 6) {
        // Ligne SAMPLE (2026-09-25, meme principe que kEngineSampler plus
        // haut) : ouvre le navigateur SD au lieu d'ecrire dans
        // rackParamVal[] (qui n'a de toute facon pas de case pour cet
        // index -- tableau dimensionne exactement a kRackGranularParamCount).
        openSamplerForGranular(track);
        return;
      }
      rackParamVal[track][extraIdx + 6] = patchExtraVal[track][extraIdx];
      if (!isRackOwner(track)) return;
      snprintf(msg, sizeof(msg), "RACK_PARAM:%s:%d:%d",
               az2::kRackEngineNames[rackEngineForTrack(track)], extraIdx + 6,
               patchExtraVal[track][extraIdx]);
      break;
    case az2::kEngineSpectral:
      // Garde localement la valeur voulue meme si "track" n'est plus
      // proprietaire (reprend effet des qu'elle reclame le moteur, voir
      // ENGINE: -> setTrackEngine()), mais n'envoie RACK_PARAM: QUE si
      // elle l'est encore : ce message ne porte aucun identifiant de
      // piste (voir isRackOwner()) et irait sinon perturber la piste qui
      // possede reellement la voix physique.
      rackParamVal[track][extraIdx + 6] = patchExtraVal[track][extraIdx];
      if (!isRackOwner(track)) return;
      snprintf(msg, sizeof(msg), "RACK_PARAM:%s:%d:%d",
               az2::kRackEngineNames[rackEngineForTrack(track)], extraIdx + 6,
               patchExtraVal[track][extraIdx]);
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
uint8_t scopeRenderedSamples[az2::kScopeSamplesPerPacket] = {};
bool scopeHasData = false;
bool scopeRendered = false;
bool patchUiNeedsRedraw = false;

constexpr int16_t kPatchTrackRowY = 66;
constexpr int16_t kPatchScopeTop = 96;
constexpr int16_t kPatchScopeH = 90;
constexpr int16_t kPatchScopeW = 232;
constexpr int16_t kPatchListGap = 8;
constexpr int16_t kPatchListX = kMargin + kPatchScopeW + kPatchListGap;
constexpr int16_t kPatchListW = kScreenSize - kMargin - kPatchListX;
constexpr uint8_t kPatchListRows = 4;
constexpr int16_t kPatchListRowH = 20;
uint16_t patchListScroll = 0;
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
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  // Couleur ET nom du moteur (2026-09-22, identite par moteur -- voir
  // kEngineAccent[]) plutot que juste "< PISTE N >" colore par piste :
  // on sait maintenant d'un coup d'oeil QUEL moteur on regle, pas
  // seulement sur quelle piste, avant meme de lire les libelles des
  // lignes en dessous.
  gfx->setTextColor(patchAccent(t));
  char buf[32];
  snprintf(buf, sizeof(buf), "< PISTE %d - %s >", t + 1, az2::engineName(trackEngine[t]));  // +1 : affichage "plus musicien"
  // Largeur variable selon le nom du moteur (DEXED vs SPECTRAL) -- centre
  // reellement au lieu d'un decalage fixe qui ne convenait qu'a l'ancien
  // texte de longueur constante. Police par defaut GFX : 6px/caractere,
  // x2 en taille 2.
  const int16_t textW = static_cast<int16_t>(strlen(buf) * 12);
  gfx->setCursor(static_cast<int16_t>(kScreenSize / 2 - textW / 2), kPatchTrackRowY);
  gfx->print(buf);
}

void keepPatchListVisible() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const uint16_t current = trackPatch[t];
  const uint16_t count = az2::enginePatchCount(trackEngine[t]);
  if (current < patchListScroll) patchListScroll = current;
  if (current >= patchListScroll + kPatchListRows)
    patchListScroll = static_cast<uint16_t>(current - kPatchListRows + 1);
  const uint16_t maxScroll = count > kPatchListRows ? static_cast<uint16_t>(count - kPatchListRows) : 0;
  if (patchListScroll > maxScroll) patchListScroll = maxScroll;
}

void drawPatchList() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const uint16_t accent = patchAccent(t);
  const uint16_t count = az2::enginePatchCount(trackEngine[t]);
  keepPatchListVisible();
  gfx->fillRect(kPatchListX, kPatchScopeTop, kPatchListW, kPatchScopeH, RGB565_BLACK);
  const uint16_t listBorder = patchPresetEditing ? RGB565(255, 210, 40)
                              : patchOnPresetList ? RGB565_WHITE : accent;
  gfx->drawRect(kPatchListX, kPatchScopeTop, kPatchListW, kPatchScopeH, listBorder);
  if (patchOnPresetList)
    gfx->drawRect(kPatchListX + 1, kPatchScopeTop + 1, kPatchListW - 2, kPatchScopeH - 2, accent);
  gfx->setTextSize(1);
  for (uint8_t row = 0; row < kPatchListRows; ++row) {
    const uint16_t patch = static_cast<uint16_t>(patchListScroll + row);
    const int16_t y = static_cast<int16_t>(kPatchScopeTop + 5 + row * kPatchListRowH);
    if (patch >= count) continue;
    const bool selected = patch == trackPatch[t];
    if (selected) gfx->fillRect(kPatchListX + 3, y - 2, kPatchListW - 6, kPatchListRowH - 1, accent);
    gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(kPatchListX + 7, y + 4);
    gfx->printf("%03u %s", patch + 1, az2::enginePatchName(trackEngine[t], patch));
  }
}

int16_t hitTestPatchList(int16_t x, int16_t y) {
  if (!inBox(x, y, kPatchListX, kPatchScopeTop, kPatchListW, kPatchScopeH)) return -1;
  const int row = (y - (kPatchScopeTop + 3)) / kPatchListRowH;
  if (row < 0 || row >= kPatchListRows) return -1;
  const uint16_t patch = static_cast<uint16_t>(patchListScroll + row);
  return patch < az2::enginePatchCount(trackEngine[static_cast<uint8_t>(patchTrack)])
             ? static_cast<int16_t>(patch)
             : -1;
}

void drawPatchScopeAdsrOverlay();  // definie plus bas, utilisee ici

// N'efface/redessine QUE l'interieur (pas le cadre, voir drawPatchPage()
// qui le dessine une seule fois en entrant sur la page) -- appelee a
// chaque paquet SCOPE recu (~15/s, voir kScopeSendIntervalMs cote
// Teensy), redessiner le cadre a chaque fois donnait un effet de
// scintillement genant ("la fenetre patch ... elle scintille un peu
// trop").
void drawPatchScope() {
  const int16_t w = static_cast<int16_t>(kPatchScopeW - 2);
  auto pointX = [w](uint8_t i) -> int16_t {
    return static_cast<int16_t>(kMargin + (i * w) / (az2::kScopeSamplesPerPacket - 1));
  };
  auto pointY = [](uint8_t sample) -> int16_t {
    return static_cast<int16_t>(kPatchScopeTop + 1 +
                                ((255 - sample) * (kPatchScopeH - 2)) / 255);
  };
  // Efface uniquement l'ancienne courbe. Le fond et le cadre restent
  // immobiles, ce qui évite le scintillement visible à chaque paquet.
  if (scopeRendered) {
    int16_t prevX = pointX(0);
    int16_t prevY = pointY(scopeRenderedSamples[0]);
    for (uint8_t i = 1; i < az2::kScopeSamplesPerPacket; ++i) {
      const int16_t x = pointX(i);
      const int16_t y = pointY(scopeRenderedSamples[i]);
      gfx->drawLine(prevX, prevY, x, y, RGB565_BLACK);
      prevX = x;
      prevY = y;
    }
    scopeRendered = false;
  }
  if (!scopeHasData) {
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(kPatchScopeTop + kPatchScopeH / 2 - 4));
    gfx->print("(silence -- B pour tester)");
    // BUG trouve en relisant (2026-09-23) : ce retour anticipe sautait
    // drawPatchScopeAdsrOverlay() plus bas -- en silence (l'etat par
    // defaut en arrivant sur la page, avant tout appui B), le focus
    // oscilloscope/ADSR (patchOnScopeRow) etait invisible : ni cadre, ni
    // courbe, ni points, alors meme que BAS venait d'y entrer. Il faut
    // l'overlay ici aussi.
    drawPatchScopeAdsrOverlay();
    return;
  }
  int16_t prevX = pointX(0), prevY = pointY(scopeSamples[0]);
  const uint16_t traceColor = patchAccent(static_cast<uint8_t>(patchTrack));
  for (uint8_t i = 0; i < az2::kScopeSamplesPerPacket; ++i) {
    const int16_t x = pointX(i);
    const int16_t y = pointY(scopeSamples[i]);
    if (i > 0) {
      gfx->drawLine(prevX, prevY, x, y, traceColor);
    }
    prevX = x;
    prevY = y;
    scopeRenderedSamples[i] = scopeSamples[i];
  }
  scopeRendered = true;
  drawPatchScopeAdsrOverlay();
}

// Courbe ADSR schematique superposee a l'oscilloscope (2026-09-23) --
// redessinee a CHAQUE rafraichissement du scope (pas une seule fois),
// sinon le trace-erase-redraw de la forme d'onde ci-dessus l'efface
// petit a petit. 4 segments de largeur nominale egale (attaque/chute/
// maintien fixe/relachement), la largeur des 3 segments temporels
// (pas le maintien, qui est un NIVEAU pas une duree) est modulee par le
// reglage 0-127 correspondant -- schematique, pas une reproduction
// exacte de la courbe de AudioEffectEnvelope, mais bouge visiblement a
// chaque tour d'encodeur, ce qui est le but ("on doit pouvoir la
// regler avec les encodeurs").
void drawPatchScopeAdsrOverlay() {
  // Position/visibilite memorisees de l'appel precedent, pour EFFACER
  // avant de redessiner -- sinon tourner un encodeur (qui bouge la ligne
  // a chaque appel) ou sortir du focus (qui fait disparaitre les points)
  // laisse une trainee : rien d'autre n'efface ces pixels-la (l'effacement
  // de la trace audio plus haut ne connait que scopeRenderedSamples).
  // Meme principe que scopeRendered/scopeRenderedSamples juste au-dessus.
  static bool prevLineValid = false;
  static int16_t prevLineX[5] = {};
  static int16_t prevLineY[5] = {};
  static bool prevDotsValid = false;
  static int16_t prevDotX[4] = {};
  static int16_t prevDotY[4] = {};

  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const int16_t w = static_cast<int16_t>(kPatchScopeW - 2);
  const int16_t top = static_cast<int16_t>(kPatchScopeTop + 1);
  const int16_t h = static_cast<int16_t>(kPatchScopeH - 2);
  const int16_t x0 = kMargin;
  auto segW = [w](uint8_t val127) -> int16_t {
    const float frac = 0.2f + 0.8f * (static_cast<float>(val127) / 127.0f);
    return static_cast<int16_t>((static_cast<float>(w) / 4.0f) * frac);
  };
  const int16_t aW = segW(trackAttack[t]);
  const int16_t dW = segW(trackDecay[t]);
  const int16_t rW = segW(trackRelease[t]);
  const int16_t sW = static_cast<int16_t>(w / 4);  // maintien : largeur fixe, c'est un niveau pas une duree
  const int16_t susY = static_cast<int16_t>(top + h - (static_cast<int32_t>(trackSustain[t]) * h) / 127);
  const int16_t baseY = static_cast<int16_t>(top + h);
  const int16_t peakY = top;
  const int16_t x1 = static_cast<int16_t>(x0 + aW);
  const int16_t x2 = static_cast<int16_t>(x1 + dW);
  const int16_t x3 = static_cast<int16_t>(x2 + sW);
  const int16_t x4 = static_cast<int16_t>(x3 + rW);

  if (prevLineValid) {
    gfx->drawLine(prevLineX[0], prevLineY[0], prevLineX[1], prevLineY[1], RGB565_BLACK);
    gfx->drawLine(prevLineX[1], prevLineY[1], prevLineX[2], prevLineY[2], RGB565_BLACK);
    gfx->drawLine(prevLineX[2], prevLineY[2], prevLineX[3], prevLineY[3], RGB565_BLACK);
    gfx->drawLine(prevLineX[3], prevLineY[3], prevLineX[4], prevLineY[4], RGB565_BLACK);
  }
  if (prevDotsValid) {
    for (uint8_t i = 0; i < 4; ++i) {
      gfx->fillCircle(prevDotX[i], prevDotY[i], 4, RGB565_BLACK);  // rayon 4 : couvre le plein (4) ET le creux (3)
    }
  }

  // Rouge vif fixe (2026-09-24, "on ne voit pas l'ADSR ... on fait une
  // ligne rouge avec des points petits bleus pour materialiser l'ADSR") :
  // avant, la ligne changeait de couleur/intensite selon le focus
  // (blanc vif si patchOnScopeRow, sinon la couleur du moteur assombrie
  // aux 3/4 -- bien trop discret sur un petit ecran a cote d'un trace
  // audio lumineux). Desormais TOUJOURS la meme couleur bien visible,
  // focus ou pas -- la courbe est un repere permanent, pas juste un
  // etat d'edition.
  constexpr uint16_t kAdsrLineColor = RGB565(255, 40, 40);
  constexpr uint16_t kAdsrDotColor = RGB565(70, 140, 255);
  gfx->drawLine(x0, baseY, x1, peakY, kAdsrLineColor);
  gfx->drawLine(x1, peakY, x2, susY, kAdsrLineColor);
  gfx->drawLine(x2, susY, x3, susY, kAdsrLineColor);
  gfx->drawLine(x3, susY, x4, baseY, kAdsrLineColor);
  prevLineX[0] = x0; prevLineY[0] = baseY;
  prevLineX[1] = x1; prevLineY[1] = peakY;
  prevLineX[2] = x2; prevLineY[2] = susY;
  prevLineX[3] = x3; prevLineY[3] = susY;
  prevLineX[4] = x4; prevLineY[4] = baseY;
  prevLineValid = true;

  // "Une petite boule sur les points" : un marqueur bleu par point ADSR
  // (fin d'attaque/chute/maintien/relachement), TOUJOURS affiche (meme
  // raison que la ligne ci-dessus -- materialiser l'ADSR en permanence,
  // pas seulement en focus). Le point actuellement selectionne
  // (patchScopeAdsrPoint, voir le clic d'encodeur dans
  // handleTeensyLine()) ressort en plus gros/blanc UNIQUEMENT quand on
  // est en train de l'editer (patchOnScopeRow), pour montrer lequel des
  // 4 bouge avec les encodeurs.
  {
    const int16_t px[4] = {x1, x2, x3, x4};
    const int16_t py[4] = {peakY, susY, susY, baseY};
    for (uint8_t i = 0; i < 4; ++i) {
      if (i == patchScopeAdsrPoint) {
        gfx->fillCircle(px[i], py[i], 4, RGB565_WHITE);
      } else {
        gfx->fillCircle(px[i], py[i], 3, kAdsrDotColor);
      }
      prevDotX[i] = px[i];
      prevDotY[i] = py[i];
    }
    prevDotsValid = true;
  }
  // Bordure toujours redessinee dans la couleur courante (position FIXE --
  // un simple contour ecrase l'ancien sans besoin de l'effacer d'abord,
  // contrairement a la ligne/aux points qui bougent).
  gfx->drawRect(kMargin, kPatchScopeTop, kPatchScopeW, kPatchScopeH,
                patchOnScopeRow ? RGB565_WHITE : patchAccent(t));
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
  x = patchIsRightCol(track, logicalRow) ? static_cast<int16_t>(kMargin + halfW + kPatchColGap) : kMargin;
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
  const uint16_t accent = patchAccent(track);

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
  const uint16_t accent = patchAccent(track);

  gfx->fillRect(x, y, w, rowH, RGB565_BLACK);
  gfx->drawRect(x, y, w, rowH, rowSelected ? accent : kFaint);

  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3));
  gfx->print(patchExtraLabel(track, extraIdx));

  // Ligne SAMPLE du moteur SAMPLER (2026-09-23) : pas une valeur
  // numerique (voir sendPatchExtra()) -- la ligne est trop etroite pour
  // un nom de fichier complet (moitie de largeur, appariee 2 par ligne),
  // juste un indicateur "charge/vide" comme les cellules pad de l'ecran
  // SAMPLEUR (voir drawSamplerPage()).
  gfx->setTextSize(2);
  if (trackEngine[track] == az2::kEngineSampler && extraIdx == 1) {
    gfx->setTextColor(trackSamplePath[track][0] ? RGB565_WHITE : kFaint);
    gfx->setCursor(static_cast<int16_t>(x + w - 50), static_cast<int16_t>(y + 3));
    gfx->print(trackSamplePath[track][0] ? "WAV" : "...");
    return;
  }

  char buf[6];
  snprintf(buf, sizeof(buf), "%3d", patchExtraVal[track][extraIdx]);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(x + w - 34), static_cast<int16_t>(y + 3));
  gfx->print(buf);
}

void sendPatchFilt() {
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  char msg[48];
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
  const uint16_t accent = patchAccent(t);

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
  const uint16_t accent = patchAccent(t);
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
// "<path>.bak", puis renomme le "tmp" vers le nom final. Le fichier
// final peut etre momentanement absent entre les deux renommages ;
// l'ancien contenu reste alors dans .bak.
// `content` : callback qui ecrit dans le fichier ouvert (permet de
// reutiliser cette fonction pour un patch (1 ligne) ou un projet
// (bien plus long) sans dupliquer la logique tmp/bak/rename).
bool projectStructureValid(File &f);

uint32_t savedFileCrc(File &f, size_t byteCount) {
  uint32_t crc = 0xFFFFFFFFUL;
  f.seek(0);
  for (size_t i = 0; i < byteCount; ++i) {
    const int value = f.read();
    if (value < 0) return 0;
    crc ^= static_cast<uint8_t>(value);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
  }
  return ~crc;
}

// Les anciens fichiers sans CRC restent lisibles. Les nouveaux portent
// un pied de 18 octets : AZ2CRC32:XXXXXXXX\n.
bool savedFileCrcValid(File &f) {
  const size_t size = f.size();
  if (size == 0) return false;
  char header[7] = {};
  f.seek(0);
  const bool versioned = size >= 6 && f.readBytes(header, 6) == 6 && strncmp(header, "AZ2V2\n", 6) == 0;
  if (size < 18) return !versioned;
  f.seek(size - 18);
  char footer[19] = {};
  if (f.readBytes(footer, 18) != 18) return false;
  if (strncmp(footer, "AZ2CRC32:", 9) != 0) return !versioned;
  if (footer[17] != '\n') return false;
  uint32_t expected = 0;
  for (uint8_t i = 9; i < 17; ++i) {
    const char c = footer[i];
    expected <<= 4;
    if (c >= '0' && c <= '9') expected |= c - '0';
    else if (c >= 'A' && c <= 'F') expected |= c - 'A' + 10;
    else return false;
  }
  return savedFileCrc(f, size - 18) == expected;
}

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
  if (f.print("AZ2V2\n") != 6) {
    f.close();
    SD.remove(tmpPath);
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

  File payload = SD.open(tmpPath);
  if (!payload) return false;
  const uint32_t crc = savedFileCrc(payload, written);
  payload.close();
  File footer = SD.open(tmpPath, FILE_APPEND);
  if (!footer) return false;
  const size_t footerBytes = footer.printf("AZ2CRC32:%08lX\n", static_cast<unsigned long>(crc));
  footer.close();
  File complete = SD.open(tmpPath);
  const bool valid = complete && footerBytes == 18 && complete.size() == written + 18 &&
                     savedFileCrcValid(complete) &&
                     (strncmp(path, "/projects/", 10) != 0 || projectStructureValid(complete));
  if (complete) complete.close();
  if (!valid) {
    SD.remove(tmpPath);
    return false;
  }

  bool previousMovedToBackup = false;
  if (SD.exists(path)) {
    File previous = SD.open(path);
    const bool previousValid = previous && savedFileCrcValid(previous) &&
                               (strncmp(path, "/projects/", 10) != 0 || projectStructureValid(previous));
    if (previous) previous.close();
    if (previousValid) {
      // La version courante est la derniere copie sure : elle remplace
      // l'ancien backup seulement apres verification du nouveau tmp.
      if ((SD.exists(bakPath) && !SD.remove(bakPath)) || !SD.rename(path, bakPath)) {
        SD.remove(tmpPath);
        return false;
      }
      previousMovedToBackup = true;
    } else {
      // Si le fichier principal est corrompu, le .bak peut etre la seule
      // copie valide. Ne jamais l'ecraser avec ce fichier corrompu.
      if (!SD.remove(path)) {
        SD.remove(tmpPath);
        return false;
      }
    }
  }
  if (!SD.rename(tmpPath, path)) {
    // Echec du dernier renommage : restaure l'ancien fichier depuis le
    // backup plutot que de laisser "path" absent.
    if (previousMovedToBackup) SD.rename(bakPath, path);
    SD.remove(tmpPath);
    return false;
  }
  return true;
}

File openSavedFile(const char *path) {
  File f = SD.open(path);
  if (f && savedFileCrcValid(f)) {
    f.seek(0);
    return f;
  }
  if (f) f.close();
  char bakPath[40];
  snprintf(bakPath, sizeof(bakPath), "%s.bak", path);
  // Coupure entre renommages, ou fichier principal incomplet/corrompu.
  File backup = SD.open(bakPath);
  if (backup && savedFileCrcValid(backup)) backup.seek(0);
  else {
    if (backup) backup.close();
    return File();
  }
  return backup;
}

void samplerKitPath(char *path, size_t size) {
  snprintf(path, size, "/kits/%u.kit", samplerKitSlot);
}

void saveSamplerKit() {
  SD.mkdir("/kits");
  char path[24];
  samplerKitPath(path, sizeof(path));
  const bool ok = atomicSaveFile(path, [&](File &f) {
    f.print("KIT:1\n");
    for (uint8_t pad = 0; pad < az2::kPadCount; ++pad)
      if (padSamplePath[pad][0]) f.printf("PAD:%u:%s\n", pad, padSamplePath[pad]);
  });
  Serial.printf(ok ? "KIT_SAVED:%s\n" : "KIT_SAVE_ERROR:%s\n", path);
  snprintf(samplerUiStatus, sizeof(samplerUiStatus), ok ? "Kit %u sauve" : "Erreur sauvegarde kit %u", samplerKitSlot + 1);
  if (currentScreen == Screen::Sampler) drawSamplerPage();
}

void loadSamplerKit() {
  char path[24];
  samplerKitPath(path, sizeof(path));
  File f = openSavedFile(path);
  if (!f) {
    Serial.printf("KIT_LOAD_EMPTY:%s\n", path);
    snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Kit %u vide", samplerKitSlot + 1);
    drawSamplerPage();
    return;
  }
  String line = f.readStringUntil('\n');
  if (line == "AZ2V2") line = f.readStringUntil('\n');
  bool seen[az2::kPadCount] = {};
  bool valid = line == "KIT:1";
  while (f.available() && valid) {
    line = f.readStringUntil('\n');
    if (line.startsWith("AZ2CRC32:")) break;
    if (!line.startsWith("PAD:")) { valid = false; break; }
    const int colon = line.indexOf(':', 4);
    const int pad = line.substring(4, colon).toInt();
    const String wav = line.substring(colon + 1);
    valid = colon > 4 && pad >= 0 && pad < az2::kPadCount && !seen[pad] &&
            wav.startsWith("/samples/") && wav.length() < 64;
    if (valid) seen[pad] = true;
  }
  if (!valid) {
    f.close();
    Serial.printf("KIT_LOAD_BADFILE:%s\n", path);
    snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Kit %u invalide", samplerKitSlot + 1);
    drawSamplerPage();
    return;
  }
  f.seek(0);
  char msg[96];
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    snprintf(msg, sizeof(msg), "PADSAMPLE:%u:-", pad);
    sendToTeensy(msg);
    padSamplePath[pad][0] = '\0';
  }
  while (f.available()) {
    line = f.readStringUntil('\n');
    if (!line.startsWith("PAD:")) continue;
    snprintf(msg, sizeof(msg), "PADSAMPLE:%s", line.substring(4).c_str());
    sendToTeensy(msg);
  }
  f.close();
  Serial.printf("KIT_LOADED:%s\n", path);
  snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Kit %u charge", samplerKitSlot + 1);
  if (currentScreen == Screen::Sampler && !screensaverActive) drawSamplerPage();
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
    if (isRackTrack(t)) {
      f.print("RACK");
      const uint8_t count = az2::rackParamCount(rackEngineForTrack(t));
      for (uint8_t parameter = 0; parameter < count; ++parameter) {
        f.printf(",%u", rackParamVal[t][parameter]);
      }
      f.println();
    }
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
  File f = openSavedFile(path);
  if (!f) {
    Serial.print("PATCH_LOAD_EMPTY:");
    Serial.println(path);
    return;
  }
  String line = f.readStringUntil('\n');
  if (line == "AZ2V2") line = f.readStringUntil('\n');
  const String rackLine = f.available() ? f.readStringUntil('\n') : String();
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
  char msg[48];
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
  if ((vals[0] == az2::kEngineGranular || vals[0] == az2::kEngineSpectral) &&
      rackLine.startsWith("RACK,")) {
    const uint8_t rackEngine = vals[0] == az2::kEngineGranular
                                   ? az2::kRackEngineGranular : az2::kRackEngineSpectral;
    const uint8_t count = az2::rackParamCount(rackEngine);
    int position = 5;
    for (uint8_t parameter = 0; parameter < count; ++parameter) {
      const int comma = rackLine.indexOf(',', position);
      const String field = comma < 0 ? rackLine.substring(position)
                                     : rackLine.substring(position, comma);
      const int value = constrain(field.toInt(), 0, 127);
      snprintf(msg, sizeof(msg), "RACK_PARAM:%s:%u:%d",
               az2::kRackEngineNames[rackEngine], parameter, value);
      sendToTeensy(msg);
      position = comma < 0 ? rackLine.length() : comma + 1;
    }
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
  const uint8_t volRow = patchVolRow(t);
  const uint8_t slotRow = patchSlotRow(t);
  const uint8_t totalVisual = patchTotalVisualRows(t);
  const uint8_t slotVisual = static_cast<uint8_t>(patchVisualRow(t, slotRow));
  // kept[] (voir patchKeptRows()) donne le sens INVERSE de patchVisualRow()
  // -- quelles lignes logiques vont dans telle ligne visuelle -- une fois
  // les lignes ADSR inactives compactees hors de la mise en page
  // (2026-09-24) : "visualRow*2/+1" ne pointe plus vers les bonnes lignes
  // logiques des que des lignes ont ete sautees en amont.
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t keptCount = patchKeptRows(t, kept);
  for (uint8_t slot = 0; slot < kPatchVisibleRows; ++slot) {
    const uint8_t visualRow = static_cast<uint8_t>(patchScroll + slot);
    const int16_t y = static_cast<int16_t>(kPatchRowTop + slot * kPatchRowH);
    gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kPatchRowH - 4, RGB565_BLACK);
    if (visualRow >= totalVisual) {
      continue;
    }
    if (visualRow == slotVisual) {
      drawPatchSlotRow();
      continue;
    }
    const uint8_t leftIdx = static_cast<uint8_t>(visualRow * 2U);
    const uint8_t rightIdx = static_cast<uint8_t>(leftIdx + 1U);
    const int16_t rows[2] = {leftIdx < keptCount ? static_cast<int16_t>(kept[leftIdx]) : static_cast<int16_t>(-1),
                             rightIdx < keptCount ? static_cast<int16_t>(kept[rightIdx]) : static_cast<int16_t>(-1)};
    for (const int16_t logicalRow : rows) {
      if (logicalRow < 0 || logicalRow > volRow) continue;
      if (logicalRow < 6) drawPatchRow(static_cast<uint8_t>(logicalRow));
      else if (logicalRow == volRow) drawVolRow();
      else drawPatchExtraRow(static_cast<uint8_t>(logicalRow));
    }
  }

  // Repère compact : la croix BAS/HAUT fait défiler les pages de réglages.
  gfx->fillRect(kScreenSize - 88, kPatchRowTop - 14, 64, 12, RGB565_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kScreenSize - 88, kPatchRowTop - 13);
  gfx->printf("%u-%u/%u", patchScroll + 1,
              min(static_cast<uint8_t>(patchScroll + kPatchVisibleRows), totalVisual), totalVisual);
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

// Ligne LOGIQUE controlee par l'encodeur `slot` (0=encodeur1, 1=
// encodeur2) etant donne la selection croix actuelle -- les 2
// encodeurs "reglables" agissent sur les 2 valeurs de la ligne
// VISUELLE actuellement selectionnee (voir patchVisualRow()), meme
// quand une seule des deux est "selectionnee" par la croix (voir
// selectedPatchRow) : acces tactile direct aux 2 valeurs d'un coup,
// complement naturel du reglage croix (2026-09-19, "les 2 [encodeurs]
// peuvent servir aux reglages de parametres ... on les utilise pas
// assez"). -1 si cet encodeur n'a rien a controler sur la ligne
// actuelle (ex: encodeur 2 sur SLOT, ou sur une ligne dont la colonne
// droite n'existe pas).
int8_t patchEncoderLogicalRow(uint8_t t, uint8_t slot) {
  const uint8_t slotRow = patchSlotRow(t);
  const int16_t visualRow = patchVisualRow(t, static_cast<uint8_t>(selectedPatchRow));
  if (visualRow == patchVisualRow(t, slotRow)) {
    return (slot == 0) ? static_cast<int8_t>(slotRow) : -1;
  }
  // kept[] (voir patchKeptRows()) : meme raison que drawPatchWindow(),
  // "visualRow*2/+1" ne pointe plus vers les bonnes lignes logiques une
  // fois les lignes ADSR inactives compactees hors de la mise en page
  // (2026-09-24).
  uint8_t kept[kPatchMaxKeptRows];
  const uint8_t keptCount = patchKeptRows(t, kept);
  const uint8_t leftIdx = static_cast<uint8_t>(visualRow * 2);
  const uint8_t rightIdx = static_cast<uint8_t>(leftIdx + 1);
  if (slot == 0) {
    return leftIdx < keptCount ? static_cast<int8_t>(kept[leftIdx]) : -1;
  }
  return rightIdx < keptCount ? static_cast<int8_t>(kept[rightIdx]) : -1;
}

const char *patchEncoderLabel(uint8_t t, uint8_t slot) {
  const int8_t row = patchEncoderLogicalRow(t, slot);
  if (row < 0) {
    return nullptr;
  }
  const uint8_t volRow = patchVolRow(t);
  const uint8_t slotRow = patchSlotRow(t);
  if (static_cast<uint8_t>(row) == volRow) {
    return "VOLUME";
  }
  if (static_cast<uint8_t>(row) == slotRow) {
    return "SLOT";
  }
  if (row < 6) {
    return patchRowLabel(t, static_cast<uint8_t>(row));
  }
  return patchExtraLabel(t, static_cast<uint8_t>(row - 6));
}

void updatePatchEncoderHints() {
  // Encodeurs 2/3 dedies EN PERMANENCE sur cette page (2026-09-24, voir
  // le gestionnaire POT:) -- CUTOFF/RESONANCE et ADSR, independamment de
  // la ligne selectionnee par la croix. Pistes rack exclues pour
  // l'encodeur 2 (garde l'ancien systeme par ligne, voir isRackTrack()).
  static const char *const kFilterPointNames[2] = {"CUTOFF", "RESONANCE"};
  static const char *const kAdsrPointNames[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
  const uint8_t t = static_cast<uint8_t>(patchTrack);
  const char *label0 = isRackTrack(t) ? patchEncoderLabel(t, 0) : kFilterPointNames[patchScopeFilterPoint];
  drawEncoderHints(label0, kAdsrPointNames[patchScopeAdsrPoint]);
}

void drawPatchPage() {
  // En-tete colore selon le MOTEUR de la piste affichee, pas une couleur
  // fixe (2026-09-22, identite par moteur -- voir kEngineAccent[]) :
  // premiere chose visible en entrant sur la page, avant meme la ligne
  // PISTE juste en dessous.
  const uint16_t accent = patchAccent(static_cast<uint8_t>(patchTrack));
  drawSubHeader("PATCH", accent);
  updatePatchEncoderHints();
  drawPatchTrackRow();
  // Cadre du tracer dessine UNE fois ici -- drawPatchScope() (appelee a
  // chaque paquet SCOPE recu) ne touche plus que l'interieur, voir son
  // commentaire. Meme couleur d'identite que le reste de la page (au lieu
  // de kFaint fixe) -- la fenetre entiere "devient" la couleur du moteur.
  gfx->drawRect(kMargin, kPatchScopeTop, kPatchScopeW, kPatchScopeH, accent);
  drawPatchScope();
  drawPatchList();
  drawPatchWindow();
}

// Vue compacte du moteur SAMPLER dans PATCH : les 16 pads restent la banque
// logique du patch, et la liste SD dessous expose les WAV disponibles pour
// remplacer rapidement le pad selectionne. L'ecran SAMPLEUR complet reste
// disponible pour naviguer dans les dossiers.
void drawPatchSamplerBrowser() {
  constexpr int16_t x = 250;
  constexpr int16_t y = 198;
  constexpr int16_t w = 210;
  constexpr int16_t listY = 198;
  constexpr int16_t listH = 244;
  const uint16_t accent = kEngineAccent[az2::kEngineSampler];
  gfx->fillRect(x, listY, w, listH, RGB565_BLACK);
  gfx->drawRect(x, listY, w, listH, kPalette[2]);
  gfx->setTextColor(kPalette[2]);
  gfx->setCursor(x + 6, listY + 5);
  gfx->print("SAMPLES DISPONIBLES");
  gfx->setTextColor(RGB565_WHITE);
  for (uint8_t row = 0; row < 6; ++row) {
    const int16_t ry = listY + 22 + row * 27;
    if (!samplerFiles[row][0]) {
      gfx->setTextColor(kDim);
      gfx->setCursor(x + 8, ry);
      gfx->print(row == 0 ? "ouvrir SAMPLEUR pour charger" : "");
      continue;
    }
    if (row == samplerSelectedRow) gfx->fillRect(x + 3, ry - 2, w - 6, 17, RGB565(35, 55, 75));
    gfx->setTextColor(row == samplerSelectedRow ? RGB565_WHITE : kDim);
    const char *name = strrchr(samplerFiles[row], '/');
    gfx->setCursor(x + 8, ry + 2);
    gfx->printf("%u %s", samplerOffset + row + 1,
                String(name ? name + 1 : samplerFiles[row]).substring(0, 27).c_str());
  }
  gfx->setTextColor(kDim);
  gfx->setCursor(x + 6, listY + listH - 12);
  gfx->print("PAD selectionne : AFFECTER");
}

void drawPatchSamplerPage() {
  constexpr int16_t leftX = 20;
  constexpr int16_t leftY = 98;
  constexpr int16_t leftW = 222;
  constexpr int16_t rightX = 252;
  constexpr int16_t rightY = 98;
  constexpr int16_t rightW = 208;
  constexpr int16_t panelH = 306;
  const uint16_t accent = kEngineAccent[az2::kEngineSampler];

  gfx->fillRect(leftX, leftY, leftW, panelH, RGB565_BLACK);
  gfx->drawRect(leftX, leftY, leftW, panelH, accent);
  gfx->setTextSize(1);
  gfx->setTextColor(accent);
  gfx->setCursor(leftX + 8, leftY + 8);
  gfx->print("PAD 4X4 - SONS ATTRIBUES");
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    const uint8_t col = pad % 2;
    const uint8_t row = pad / 2;
    const int16_t x = leftX + 6 + col * 108;
    const int16_t y = leftY + 27 + row * 33;
    const bool selected = pad == samplerSelectedPad;
    if (selected) gfx->fillRect(x, y, 102, 28, accent);
    gfx->drawRect(x, y, 102, 28, selected ? RGB565_WHITE : kFaint);
    gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(x + 5, y + 4);
    gfx->printf("PAD %02u", pad + 1);
    const char *name = padSamplePath[pad][0] ? strrchr(padSamplePath[pad], '/') : nullptr;
    gfx->setCursor(x + 5, y + 16);
    gfx->print(padSamplePath[pad][0]
                   ? String(name ? name + 1 : padSamplePath[pad]).substring(0, 15)
                   : "(vide)");
  }

  gfx->fillRect(rightX, rightY, rightW, panelH, RGB565_BLACK);
  gfx->drawRect(rightX, rightY, rightW, panelH, kPalette[2]);
  gfx->setTextColor(kPalette[2]);
  gfx->setCursor(rightX + 8, rightY + 8);
  gfx->print("EXPLORATEUR /samples");
  for (uint8_t row = 0; row < kSamplerRows; ++row) {
    const int16_t y = rightY + 28 + row * 39;
    const bool selected = row == samplerSelectedRow;
    if (selected) gfx->fillRect(rightX + 4, y - 3, rightW - 8, 31, RGB565(35, 55, 75));
    gfx->drawRect(rightX + 4, y - 3, rightW - 8, 31, selected ? kPalette[2] : kFaint);
    gfx->setTextColor(selected ? RGB565_WHITE : kDim);
    const char *name = samplerFiles[row][0] ? strrchr(samplerFiles[row], '/') : nullptr;
    gfx->setCursor(rightX + 10, y + 4);
    gfx->printf("%02u  %s", samplerOffset + row + 1,
                samplerFiles[row][0] ? String(name ? name + 1 : samplerFiles[row]).substring(0, 25).c_str()
                                      : "-");
    gfx->setCursor(rightX + 10, y + 17);
    gfx->print(samplerFiles[row][0] ? "WAV disponible" : "");
  }

  gfx->setTextColor(kDim);
  gfx->setCursor(leftX, 420);
  gfx->printf("PAD %02u selectionne   %s", samplerSelectedPad + 1,
              samplerUiStatus[0] ? samplerUiStatus : "choisir un WAV puis AFFECTER");
  gfx->drawRect(20, 438, 88, 28, kFaint);
  gfx->drawRect(114, 438, 88, 28, kFaint);
  gfx->drawRect(208, 438, 108, 28, kPalette[2]);
  gfx->drawRect(322, 438, 68, 28, kPalette[1]);
  gfx->drawRect(396, 438, 64, 28, kPalette[3]);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(29, 448); gfx->print("< LISTE");
  gfx->setCursor(122, 448); gfx->print("LISTE >");
  gfx->setCursor(224, 448); gfx->print("AFFECTER");
  gfx->setCursor(332, 448); gfx->print("SAUVER");
  gfx->setCursor(405, 448); gfx->print("CHARGER");
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
    if (isRackTrack(t)) {
      // Valeur locale (param, reference dans rackParamVal[]) deja mise a
      // jour ci-dessus meme si "t" n'est plus proprietaire -- seul
      // l'envoi reel est conditionne, voir isRackOwner().
      if (isRackOwner(t)) {
        char msg[48];
        snprintf(msg, sizeof(msg), "RACK_PARAM:%s:%d:%d",
                 az2::kRackEngineNames[rackEngineForTrack(t)], selectedPatchRow, param);
        sendToTeensy(msg);
      }
    } else if (selectedPatchRow < 2) {
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
// Page MIXER -- volume de TOUTES les pistes en un coup d'oeil (demande
// 2026-09-19, "le mixeur doit gerer le volume de toutes les voix" --
// jusqu'ici seule la ligne VOLUME de la page PATCH permettait de regler
// UNE piste a la fois, il fallait changer de piste pour comparer/regler
// les autres). 8 barres verticales façon table de mixage. Croix :
// GAUCHE/DROITE choisit la piste, HAUT/BAS regle directement son volume
// (pas besoin de maintenir A -- metaphore fader, geste le plus frequent
// sur cette page). BTN:A = mute, BTN:D = solo (memes conventions que la
// page MOTEURS pour SOLO). Encodeurs 1/2 CONTEXTUELS (2026-09-19, "on
// va [leur] attribuer une couleur et les inclure a chaque fois dans
// l'application ... on les utilise pas assez") : encodeur 1 = choisit
// la piste, encodeur 2 = son volume -- voir drawEncoderHints() et le
// dispatch POT: plus bas.
// ---------------------------------------------------------------------
int8_t selectedMixerTrack = 0;
constexpr int16_t kMixerBarTop = 90;
constexpr int16_t kMixerBarBottom = 370;
constexpr int16_t kMixerBarH = kMixerBarBottom - kMixerBarTop;
constexpr int16_t kMixerColW = (kScreenSize - 2 * kMargin) / kSeqTrackCount;
constexpr int16_t kMixerBarW = kMixerColW - 14;

void drawMixerTrack(uint8_t t) {
  const int16_t x = static_cast<int16_t>(kMargin + t * kMixerColW + (kMixerColW - kMixerBarW) / 2);
  const bool selected = (selectedMixerTrack == static_cast<int8_t>(t));
  const uint16_t accent = kPalette[t % kPaletteCount];

  // Efface toute la colonne (barre + libelles au-dessus/en dessous) --
  // simple et robuste, pas de redessin partiel "au bon endroit" (meme
  // choix que drawPatchWindow()).
  gfx->fillRect(static_cast<int16_t>(kMargin + t * kMixerColW), static_cast<int16_t>(kMixerBarTop - 16),
                kMixerColW, static_cast<int16_t>(kMixerBarH + 40), RGB565_BLACK);

  const int16_t fillH = static_cast<int16_t>((static_cast<int32_t>(trackVolume[t]) * kMixerBarH) / 127);
  const int16_t fillY = static_cast<int16_t>(kMixerBarBottom - fillH);
  const uint16_t barColor = trackMuted[t] ? kFaint : accent;
  gfx->drawRect(x, kMixerBarTop, kMixerBarW, kMixerBarH, kFaint);
  if (fillH > 0) {
    gfx->fillRect(x, fillY, kMixerBarW, fillH, barColor);
  }
  if (selected) {
    // Contour blanc epais (3px, meme convention que la page MOTEURS
    // depuis "on fait un truc en surbrillance plus visible").
    gfx->drawRect(static_cast<int16_t>(x - 3), static_cast<int16_t>(kMixerBarTop - 3),
                  static_cast<int16_t>(kMixerBarW + 6), static_cast<int16_t>(kMixerBarH + 6), RGB565_WHITE);
    gfx->drawRect(static_cast<int16_t>(x - 4), static_cast<int16_t>(kMixerBarTop - 4),
                  static_cast<int16_t>(kMixerBarW + 8), static_cast<int16_t>(kMixerBarH + 8), RGB565_WHITE);
  }

  gfx->setTextSize(1);
  gfx->setTextColor(accent);
  char buf[4];
  // t+1 (2026-09-19, "les pistes c'est mieux de les numeroter de 1 a 8
  // ... c'est plus musicien" -- affichage seulement, t reste 0-7 en
  // interne/dans le protocole, voir trackVolume[]/VOL:).
  snprintf(buf, sizeof(buf), "%d", t + 1);
  gfx->setCursor(static_cast<int16_t>(x + kMixerBarW / 2 - 3), static_cast<int16_t>(kMixerBarTop - 14));
  gfx->print(buf);

  gfx->setTextColor(RGB565_WHITE);
  char volBuf[5];
  snprintf(volBuf, sizeof(volBuf), "%d", trackVolume[t]);
  gfx->setCursor(static_cast<int16_t>(x + kMixerBarW / 2 - (trackVolume[t] >= 100 ? 9 : 6)),
                 static_cast<int16_t>(kMixerBarBottom + 6));
  gfx->print(volBuf);

  if (trackMuted[t] || trackSoloed[t]) {
    gfx->setTextColor(trackMuted[t] ? RGB565(200, 60, 60) : RGB565(60, 220, 90));
    gfx->setCursor(static_cast<int16_t>(x + kMixerBarW / 2 - 3), static_cast<int16_t>(kMixerBarBottom + 18));
    gfx->print(trackMuted[t] ? "M" : "S");
  }
}

// Boutons MUTE/SOLO tactiles (2026-09-19, "il faut que les 2 controles
// soit[ent] boutons tactil[es]" -- jusqu'ici seuls B/D physiques
// marchaient, rien a toucher a l'ecran) -- agissent sur la piste
// SELECTIONNEE (meme reflexe que B/D), pas sur celle touchee (pas de
// tap-and-select combine ici, la selection se fait separement via une
// colonne ou GAUCHE/DROITE).
constexpr int16_t kMixerBtnY = kMixerBarBottom + 34;
constexpr int16_t kMixerBtnH = 32;
constexpr int16_t kMixerBtnW = (kScreenSize - 2 * kMargin - 8) / 2;
constexpr int16_t kMixerMuteX = kMargin;
constexpr int16_t kMixerSoloX = kMargin + kMixerBtnW + 8;

void drawMixerActionBtns() {
  const uint8_t t = static_cast<uint8_t>(selectedMixerTrack);
  constexpr uint16_t kMuteColor = RGB565(200, 60, 60);
  constexpr uint16_t kSoloColor = RGB565(60, 220, 90);

  gfx->fillRect(kMixerMuteX, kMixerBtnY, kMixerBtnW, kMixerBtnH, trackMuted[t] ? kMuteColor : RGB565_BLACK);
  gfx->drawRect(kMixerMuteX, kMixerBtnY, kMixerBtnW, kMixerBtnH, kMuteColor);
  gfx->fillRect(kMixerSoloX, kMixerBtnY, kMixerBtnW, kMixerBtnH, trackSoloed[t] ? kSoloColor : RGB565_BLACK);
  gfx->drawRect(kMixerSoloX, kMixerBtnY, kMixerBtnW, kMixerBtnH, kSoloColor);

  gfx->setTextSize(2);
  gfx->setTextColor(trackMuted[t] ? RGB565_BLACK : kMuteColor);
  gfx->setCursor(static_cast<int16_t>(kMixerMuteX + kMixerBtnW / 2 - 28), static_cast<int16_t>(kMixerBtnY + 8));
  gfx->print("MUTE");
  gfx->setTextColor(trackSoloed[t] ? RGB565_BLACK : kSoloColor);
  gfx->setCursor(static_cast<int16_t>(kMixerSoloX + kMixerBtnW / 2 - 24), static_cast<int16_t>(kMixerBtnY + 8));
  gfx->print("SOLO");
}

bool hitTestMixerMute(int16_t x, int16_t y) {
  return inBox(x, y, kMixerMuteX, kMixerBtnY, kMixerBtnW, kMixerBtnH);
}
bool hitTestMixerSolo(int16_t x, int16_t y) {
  return inBox(x, y, kMixerSoloX, kMixerBtnY, kMixerBtnW, kMixerBtnH);
}

// Bascule mute (true) ou solo (false) sur la piste SELECTIONNEE --
// factorise (2026-09-19) pour etre appele aussi bien par B/D
// (physique) que par les boutons tactiles MUTE/SOLO ci-dessus, sans
// dupliquer l'envoi MUTE:/SOLO: + le redessin.
void toggleMixerMuteSolo(bool mute) {
  const uint8_t t = static_cast<uint8_t>(selectedMixerTrack);
  char msg[12];
  if (mute) {
    trackMuted[t] = !trackMuted[t];
    snprintf(msg, sizeof(msg), "MUTE:%d:%d", t, trackMuted[t] ? 1 : 0);
  } else {
    trackSoloed[t] = !trackSoloed[t];
    snprintf(msg, sizeof(msg), "SOLO:%d:%d", t, trackSoloed[t] ? 1 : 0);
  }
  sendToTeensy(msg);
  drawMixerTrack(t);
  drawMixerActionBtns();
}

void drawMixerPage() {
  drawSubHeader("MIXER", kPalette[5 % kPaletteCount]);
  drawEncoderHints("PISTE", "VOLUME");
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    drawMixerTrack(t);
  }
  drawMixerActionBtns();
}

bool hitTestMixerTrack(int16_t x, int16_t y, uint8_t &track) {
  if (y < kMixerBarTop - 16 || y > kMixerBarBottom + 30) {
    return false;
  }
  const int col = (x - kMargin) / kMixerColW;
  if (col < 0 || col >= static_cast<int>(kSeqTrackCount)) {
    return false;
  }
  track = static_cast<uint8_t>(col);
  return true;
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
constexpr int16_t kSongSlotH = 46;
constexpr int16_t kSongProjectsY = 408;
constexpr int16_t kSongProjectsH = 42;
uint8_t songSelectedSlot = 0;

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
  gfx->drawRect(x, y, kSongSlotW, kSongSlotH, i == songSelectedSlot ? RGB565_WHITE : kFaint);

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
  gfx->fillRect(kMargin, kSongProjectsY, kScreenSize - 2 * kMargin, kSongProjectsH, RGB565_BLACK);
  gfx->drawRect(kMargin, kSongProjectsY, kScreenSize - 2 * kMargin, kSongProjectsH, kPalette[3]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(kMargin + 28, kSongProjectsY + 10);
  gfx->print("PROJETS  >   B");
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, 457);
  gfx->print("Croix: case/pattern  A: mode  D: longueur  C: retour");
}

bool hitTestSongProjects(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kSongProjectsY, kScreenSize - 2 * kMargin, kSongProjectsH);
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
// Listes de ROM en PSRAM (2026-10-01) : 3 x 16 Ko de simples libelles d'UI
// qui manquaient en RAM interne (firmware Walnut + coeur 0 : DRAM depassee de
// 6,5 Ko). Alloues dans allocRomNameLists(), en tete de setup().
char (*gbRomNames)[kGbRomNameLen] = nullptr;
uint8_t gbRomCount = 0;

bool gbSettingsMenuOpen = false;
bool gbCheatMenuOpen = false;
int8_t gbSettingsSelectedRow = 0;
int8_t gbCheatSelectedRow = 0;

// [2026-09-27] 90 -> 104 : laisse la place aux boutons AFFICHAGE agrandis
// (kScaleBtnY=64 + kScaleBtnH=34 = 98, +6px de marge comme ailleurs dans
// cette page, ex. kRomRowH-6).
constexpr int16_t kRomRowTop = 104;
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
// Libelle lisible d'une ROM pour les listes JEUX / NES / NGP : garde le nom
// du jeu seul (sans dossier, sans extension, sans les tags No-Intro du type
// "(France) (Rev 1) [!]"), puis tronque avec ".." pour tenir dans le cadre.
// L'identifiant SD complet reste intact dans les tableaux *RomNames[].
constexpr size_t kRomLabelChars = 33;  // taille de texte 2 = 12 px/caractere
void formatRomLabel(const char *fullName, char *out, size_t outSize) {
  const char *start = strrchr(fullName, '/');
  start = (start != nullptr) ? start + 1 : fullName;
  size_t len = strlen(start);
  const char *dot = strrchr(start, '.');
  if (dot != nullptr) len = static_cast<size_t>(dot - start);
  // Coupe au premier tag "(" ou "[" s'il reste un nom devant.
  for (size_t i = 1; i < len; ++i) {
    if (start[i] == '(' || start[i] == '[') {
      len = i;
      break;
    }
  }
  while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '_')) --len;
  if (len == 0) {  // nom entierement entre crochets : garder le nom brut
    start = fullName;
    len = strlen(fullName);
  }
  const size_t maxChars = min(kRomLabelChars, outSize - 1);
  if (len <= maxChars) {
    memcpy(out, start, len);
    out[len] = '\0';
  } else {
    memcpy(out, start, maxChars - 2);
    out[maxChars - 2] = '.';
    out[maxChars - 1] = '.';
    out[maxChars] = '\0';
  }
}

void drawRomRow(uint8_t index) {
  if (index < gbRomScroll || index >= gbRomScroll + kRomVisibleRows) {
    return;
  }
  int16_t y;
  romRowRect(static_cast<uint8_t>(index - gbRomScroll), y);
  const bool selected = (index == selectedRomIndex);
  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6,
                selected ? kPalette[1] : RGB565_BLACK);
  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, kRomRowH - 6,
                selected ? kPalette[1] : kPalette[index % kPaletteCount]);
  if (selected) {
    // Ligne selectionnee par la croix -- meme convention que la piste
    // choisie sur la page MOTEURS (contour blanc double).
    gfx->drawRect(static_cast<int16_t>(kMargin + 1), static_cast<int16_t>(y + 1), kScreenSize - 2 * kMargin - 2,
                  kRomRowH - 8, RGB565_WHITE);
  }
  gfx->setTextSize(2);
  gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
  gfx->setCursor(static_cast<int16_t>(kMargin + 10), static_cast<int16_t>(y + 6));
  char label[kRomLabelChars + 1];
  formatRomLabel(gbRomNames[index], label, sizeof(label));
  gfx->print(label);
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
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_RED);
    gfx->setCursor(kRecX, kRecY);
    gfx->print("REC");
  }
}

void drawGbSettingsMenu() {
  // Coordonnées de centrage de notre overlay de menu
  const int16_t menuX = 60;
  const int16_t menuY = 60;
  const int16_t menuW = 360;
  const int16_t menuH = 360;

  // Dessiner l'arrière-plan opaque et la bordure stylisée
  gfx->fillRect(menuX, menuY, menuW, menuH, RGB565_BLACK);
  gfx->drawRect(menuX, menuY, menuW, menuH, kPalette[2]);
  gfx->drawRect(menuX + 2, menuY + 2, menuW - 4, menuH - 4, kFaint);

  // Titre du menu
  gfx->setTextSize(2);
  gfx->setTextColor(kPalette[1]);
  gfx->setCursor(menuX + 20, menuY + 25);
  if (gbCheatMenuOpen) {
    gfx->print("TRICHES (CHEATS)");
  } else {
    gfx->print("OPTIONS JEU");
  }

  gfx->setTextSize(1);
  if (gbCheatMenuOpen) {
    uint8_t count = gbGetCheatCount();
    if (count == 0) {
      gfx->setTextColor(kDim);
      gfx->setCursor(menuX + 30, menuY + 100);
      gfx->print("Aucune triche disponible.");
      gfx->setCursor(menuX + 30, menuY + 300);
      gfx->setTextColor(RGB565_WHITE);
      gfx->print("[B] Retour");
    } else {
      for (uint8_t i = 0; i < count; ++i) {
        const bool selected = (i == gbCheatSelectedRow);
        const int16_t rowY = menuY + 80 + i * 32;

        if (selected) {
          gfx->fillRect(menuX + 15, rowY - 6, menuW - 30, 26, kPalette[2]);
          gfx->setTextColor(RGB565_BLACK);
        } else {
          gfx->setTextColor(RGB565_WHITE);
        }

        GbCheat *cheat = gbGetCheat(i);
        if (cheat != nullptr) {
          gfx->setCursor(menuX + 25, rowY);
          gfx->print(cheat->name);

          if (selected) {
            gfx->setTextColor(RGB565_BLACK);
          } else {
            gfx->setTextColor(cheat->enabled ? RGB565_GREEN : kDim);
          }
          gfx->setCursor(menuX + 250, rowY);
          gfx->print(cheat->enabled ? "[ACTIF]" : "[DESACTIVE]");
        }
      }
      gfx->setTextColor(kDim);
      gfx->setCursor(menuX + 20, menuY + 325);
      gfx->print("Croix: Naviguer | A: Basculer | B: Retour");
    }
  } else {
    static const char *const kOptions[] = {
      "Continuer le jeu",
      "Sauvegarder la partie (SAVE)",
      "Charger la partie (LOAD)",
      "Triches (Cheats)",
      "Quitter le jeu (EXIT)"
    };
    constexpr uint8_t kOptCount = 5;

    for (uint8_t i = 0; i < kOptCount; ++i) {
      const bool selected = (i == gbSettingsSelectedRow);
      const int16_t rowY = menuY + 80 + i * 40;

      if (selected) {
        gfx->fillRect(menuX + 15, rowY - 10, menuW - 30, 34, kPalette[2]);
        gfx->setTextColor(RGB565_BLACK);
      } else {
        gfx->setTextColor(RGB565_WHITE);
      }

      gfx->setCursor(menuX + 30, rowY);
      gfx->print(kOptions[i]);

      if (i == 3) {
        char buf[16];
        snprintf(buf, sizeof(buf), "(%d dispo)", gbGetCheatCount());
        if (selected) {
          gfx->setTextColor(RGB565_BLACK);
        } else {
          gfx->setTextColor(kDim);
        }
        gfx->setCursor(menuX + 200, rowY);
        gfx->print(buf);
      }
    }

    gfx->setTextColor(kDim);
    gfx->setCursor(menuX + 20, menuY + 325);
    gfx->print("Croix: Naviguer | A: Choisir | B: Fermer");
  }
#ifdef AZ2_DIRECT_PANEL
  flushUiCanvas();
#else
  if (uint16_t *framebuffer = gfx->getFramebuffer(); framebuffer != nullptr) {
    esp_cache_msync(framebuffer, static_cast<size_t>(kScreenSize * kScreenSize * sizeof(uint16_t)),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  }
#endif
}

// [2026-09-27] Ecran intermediaire entre le menu et Screen::Retro : choix du
// moteur d'emulation. 3 cartes, statut honnete plutot que promettre un jeu
// qui ne demarre pas :
//   0 = GAME BOY (Peanut-GB) : seule cible reellement jouable sur CE
//       firmware, X3 a 59,76fps valide sur materiel (voir AZ2_ETAT_ACTUEL.md).
//   1 = GAME BOY COLOR (Walnut-CGB) : meme decoupage double coeur teste,
//       mais bug de blocage au demarrage pas encore resolu -- build de labo
//       separee (collision de symboles avec Peanut-GB, pas compilable dans
//       le meme firmware).
//   2 = NES : etude faite (Anemoia-ESP32, GPLv3, coeur decouple du
//       materiel), pas encore implemente.
//   3 = NEO GEO POCKET : pas etudie, a faire plus tard. PAS le Neo Geo
//       arcade/AES (celui-la est hors de portee : cartouches 40-100+ Mo,
//       double CPU 68000+Z80, aucun projet microcontroleur connu). Le
//       Neo Geo Pocket (Color) est une console portable SNK 1998-99,
//       TLCS-900H, cartouches 2-4 Mo, 160x152 -- echelle comparable au
//       GB/GBC deja valides, PAS comparable au Neo Geo arcade.
// kEmuCardTitles utilise par emuPickerActivate() -- garder synchronise avec
// drawEmuPickerPage() si l'ordre change.
constexpr const char *kEmuCardTitles[kEmuCardCount] = {"GAME BOY", "GAME BOY COLOR", "NES",
                                                        "NEO GEO POCKET"};

// Couleur "en cours" (ni pret/vert ni desactive/gris) -- meme convention
// RGB565() que kDim/kFaint plus haut dans ce fichier.
constexpr uint16_t kWarnAmber = RGB565(255, 170, 0);

// [2026-09-27] 110px/2 lignes de sous-titre -> 88px/1 ligne : 4 cartes
// tiennent desormais dans les 480px de l'ecran (voir kEmuCardH/kEmuCardGap).
void drawEmuCard(uint8_t index, int16_t y, const char *sub, const char *cta, uint16_t accent,
                  bool filled) {
  const bool selected = (emuPickerSelected == index);
  if (selected) {
    gfx->fillRoundRect(kEmuCardX, y, kEmuCardW, kEmuCardH, 10, kPalette[1]);
    gfx->setTextColor(RGB565_BLACK);
  } else if (filled) {
    gfx->fillRoundRect(kEmuCardX, y, kEmuCardW, kEmuCardH, 10, accent);
    gfx->setTextColor(RGB565_BLACK);
  } else {
    gfx->fillRoundRect(kEmuCardX, y, kEmuCardW, kEmuCardH, 10, RGB565_BLACK);
    gfx->setTextColor(accent);
  }
  gfx->drawRoundRect(kEmuCardX, y, kEmuCardW, kEmuCardH, 10, selected ? kPalette[1] : accent);
  if (selected) {
    // Curseur croix/A : cadre exterieur supplementaire, visible sans tactile.
    gfx->drawRoundRect(static_cast<int16_t>(kEmuCardX - 3), static_cast<int16_t>(y - 3),
                        static_cast<int16_t>(kEmuCardW + 6), static_cast<int16_t>(kEmuCardH + 6), 12,
                        RGB565_WHITE);
  }
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kEmuCardX + 16), static_cast<int16_t>(y + 8));
  gfx->print(kEmuCardTitles[index]);
  gfx->setTextSize(1);
  gfx->setCursor(static_cast<int16_t>(kEmuCardX + 16), static_cast<int16_t>(y + 34));
  gfx->print(sub);
  gfx->setTextSize(2);
  gfx->setCursor(static_cast<int16_t>(kEmuCardX + 16), static_cast<int16_t>(y + 56));
  gfx->print(cta);
}

void drawEmuPickerPage() {
  drawSubHeader("EMULATEURS", kPalette[2]);
  constexpr uint16_t kGameBoyYellow = RGB565(255, 225, 80);
  // Libelles = coeur reellement compile + cadence mesuree sur materiel
  // (2026-10-01, Zelda / Zelda DX, X2 et X3).
#ifdef AZ2_GB_CORE_PEANUT
  drawEmuCard(0, kEmuCardGbY, "Peanut-GB - jeux .gb - 59,7 fps X2/X3", "JOUER >", kGameBoyYellow, true);
  drawEmuCard(1, kEmuCardGbcY, "Walnut-CGB - firmware separe", "INDISPONIBLE", kGameBoyYellow, true);
#else
  drawEmuCard(0, kEmuCardGbY, "Walnut-CGB - jeux .gb - 59,7 fps X2/X3", "JOUER >", kGameBoyYellow, true);
  drawEmuCard(1, kEmuCardGbcY, "Walnut-CGB - jeux .gbc - 59,7 fps X2/X3", "JOUER >", kGameBoyYellow, true);
#endif
  drawEmuCard(2, kEmuCardNesY, "Anemoia 6502 - jeux .nes - 49-50 fps", "JOUER >", kGameBoyYellow, true);
  drawEmuCard(3, kEmuCardNeoY, "RACE TLCS-900H - jeux .ngp/.ngc", "QUALIF EN COURS",
              kGameBoyYellow, true);
}

// Active la carte selectionnee (tap ou croix+A). Le build courant compile
// Walnut-CGB, qui detecte DMG vs GBC depuis l'entete de chaque ROM ; les
// cartes GAME BOY et GAME BOY COLOR restent donc une distinction de parcours.
// Les environnements Peanut-GB labo utilisent l'autre backend et excluent la
// carte GBC pour eviter la collision de symboles. NES (index 2) et Neo Geo
// Pocket (index 3) sont integres dans screen_esp, avec NGP encore en
// qualification materielle.
void emuPickerActivate(uint8_t index) {
  emuPickerSelected = static_cast<int8_t>(index);
#ifdef AZ2_NES_ENABLED
  if (index == 2) {
    goTo(Screen::NesRetro);
    return;
  }
#endif
  if (index == 3) {
    goTo(Screen::NgpRetro);
    return;
  }
  if (index == 0) {
    gbSetRomKind(GbRomKind::Dmg);  // carte GAME BOY : .gb seulement
    goTo(Screen::Retro);
    return;
  }
#ifndef AZ2_GB_CORE_PEANUT
  if (index == 1) {
    gbSetRomKind(GbRomKind::Cgb);  // carte GAME BOY COLOR : .gbc seulement
    goTo(Screen::Retro);
    return;
  }
#endif
  const char *msg;
  uint16_t color;
  switch (index) {
    case 1:
      msg = "GBC : valide (59,7fps) - firmware labo separe, reflash requis";
      color = kWarnAmber;
      break;
    case 2:
      msg = "NES : 49-50fps, FRAMESKIP actif";
      color = kWarnAmber;
      break;
    default:
      msg = "Emulateur indisponible";
      color = kDim;
      break;
  }
  gfx->fillRect(0, 0, kScreenSize, 22, RGB565_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  gfx->setCursor(kMargin, 5);
  gfx->print(msg);
}

// [2026-09-27] Page NES : premiere passe fonctionnelle (voir nes_emulator.cpp
// pour ce qui manque encore -- sauvegarde SRAM, double coeur, audio). Volontairement
// minimale (pas de X2/X3, pas de pagination, pas de menu en jeu) pour valider
// le coeur d'abord, comme la toute premiere version de la page JEUX GB.
char (*nesRomNames)[kNesRomNameLen] = nullptr;  // PSRAM, voir gbRomNames
uint8_t nesRomCount = 0;
int8_t nesRomScroll = 0;
uint8_t nesSelectedRomIndex = 0;
constexpr uint8_t kNesVisibleRows = 10;
constexpr int16_t kNesRowH = 36;
constexpr int16_t kNesRowTop = 70;
// Image NES agrandie sur toute la dalle 480x480. Le ratio est legerement
// etire (480/256 contre 480/240), mais la lecture est bien plus confortable
// et reste coherente avec le framebuffer RGB direct.
constexpr int16_t kNesScreenLeft = 0;
constexpr int16_t kNesScreenTop = 0;

void drawNesPage() {
  if (nesIsLoaded()) {
    gfx->fillRect(0, 0, kScreenSize, kNesScreenTop, RGB565_BLACK);
    gfx->fillRect(0, static_cast<int16_t>(kNesScreenTop + 240), kScreenSize,
                  static_cast<int16_t>(kScreenSize - kNesScreenTop - 240), RGB565_BLACK);
    gfx->fillRect(0, 0, kNesScreenLeft, kScreenSize, RGB565_BLACK);
    gfx->fillRect(static_cast<int16_t>(kNesScreenLeft + 256), 0,
                  static_cast<int16_t>(kScreenSize - kNesScreenLeft - 256), kScreenSize, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 4);
    gfx->print(nesRomTitle());
    gfx->setCursor(static_cast<int16_t>(kScreenSize - kMargin - 66), 4);
    gfx->print("C:QUITTER D:SAVE");
    return;
  }
  drawSubHeader("NES - choisis une ROM", kPalette[2]);
  if (nesRomCount == 0) {
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 80);
    gfx->print("Aucune ROM trouvee dans /nes.");
    return;
  }
  gfx->fillRect(kMargin, kNesRowTop, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                static_cast<int16_t>(kNesVisibleRows * kNesRowH), RGB565_BLACK);
  const uint8_t visible =
      static_cast<uint8_t>(min<int>(kNesVisibleRows, nesRomCount - nesRomScroll));
  for (uint8_t i = 0; i < visible; ++i) {
    const uint8_t idx = static_cast<uint8_t>(nesRomScroll + i);
    const int16_t y = static_cast<int16_t>(kNesRowTop + i * kNesRowH);
    const bool selected = (idx == nesSelectedRomIndex);
    gfx->fillRect(kMargin, y, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                  static_cast<int16_t>(kNesRowH - 6), selected ? kPalette[1] : RGB565_BLACK);
    gfx->drawRect(kMargin, y, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                  static_cast<int16_t>(kNesRowH - 6),
                  selected ? kPalette[1] : kPalette[idx % kPaletteCount]);
    gfx->setTextSize(2);
    gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(y + 8));
    char label[kRomLabelChars + 1];
    formatRomLabel(nesRomNames[idx], label, sizeof(label));
    gfx->print(label);
  }
}

// Bande de 8 lignes NES (256px RGB565), agrandie en 480x480 -- meme principe
// que gbBlitLine(), avec flip 180 degres (meme panneau physique que la GB).
void nesBlitBandImpl(int bandIndex, const uint16_t *pixels) {
#ifdef AZ2_DIRECT_PANEL
  constexpr int16_t kBandLines = 8;
  const int16_t sourceTop = static_cast<int16_t>(bandIndex * kBandLines);
  const int16_t sourceRows = min<int16_t>(kBandLines, 240 - sourceTop);
  static uint16_t *scaledBand = nullptr;
  static uint8_t xMap[kScreenSize];
  static bool mapReady = false;
  if (!scaledBand) {
    scaledBand = static_cast<uint16_t *>(
        heap_caps_malloc(static_cast<size_t>(kScreenSize) * 16U * sizeof(uint16_t),
                         MALLOC_CAP_SPIRAM));
  }
  if (!scaledBand) return;
  if (!mapReady) {
    for (int16_t x = 0; x < kScreenSize; ++x) {
      xMap[x] = static_cast<uint8_t>((static_cast<uint32_t>(x) * 256U) / kScreenSize);
    }
    mapReady = true;
  }
  for (int16_t row = 0; row < sourceRows; ++row) {
    uint16_t *dst = scaledBand + row * 2 * kScreenSize;
    const uint16_t *src = pixels + row * 256;
    for (int16_t x = 0; x < kScreenSize; ++x) {
      dst[x] = src[xMap[x]];
    }
    memcpy(dst + kScreenSize, dst, kScreenSize * sizeof(uint16_t));
  }
  directPanel.copyRotatedRgb565(scaledBand, 0, static_cast<int16_t>(sourceTop * 2),
                                kScreenSize, static_cast<int16_t>(sourceRows * 2));
  if ((bandIndex & 3) == 3) yield();
  return;
#else
  uint16_t *framebuffer = gfx->getFramebuffer();
  if (framebuffer == nullptr) return;
  constexpr int16_t kBandLines = 8;
  const int16_t sourceTop = static_cast<int16_t>(bandIndex * kBandLines);
  // La conversion 256 -> 480 est identique pour toutes les bandes. La
  // construire une seule fois retire toutes les divisions du chemin chaud
  // (115 200 lectures de pixels par frame), sans changer le mapping ni la
  // rotation.
  static uint8_t nesXMap[kScreenSize];
  static bool nesXMapReady = false;
  if (!nesXMapReady) {
    for (int16_t dstX = 0; dstX < kScreenSize; ++dstX) {
      nesXMap[dstX] = static_cast<uint8_t>(
          (static_cast<uint32_t>(kScreenSize - 1 - dstX) * 256u) /
          static_cast<uint32_t>(kScreenSize));
    }
    nesXMapReady = true;
  }
  for (int16_t row = 0; row < kBandLines; ++row) {
    const uint16_t *src = pixels + row * 256;
    const int16_t dstY = static_cast<int16_t>(
        kScreenSize - 1 - ((sourceTop + row) * 2));
    uint16_t *dst0 = framebuffer + dstY * kScreenSize;
    uint16_t *dst1 = dst0 - kScreenSize;
    // NES 256x240 -> ecran 480x480 : l'echelle horizontale est 480/256,
    // pas 2. Ecrire 512 pixels debordait de 32 pixels sur chaque ligne.
    // Le calcul inverse conserve le flip 180 degres et reste sans lecture
    // hors limites du framebuffer.
    for (int16_t dstX = 0; dstX < kScreenSize; ++dstX) {
      const uint16_t color = src[nesXMap[dstX]];
      dst0[dstX] = color;
      dst1[dstX] = color;
    }
  }
  // Le panneau RGB lit le framebuffer en DMA : publier chaque bande evite
  // l'ecran noir/tearing observe avec un unique flush de 480x480.
  const int16_t firstDstY = static_cast<int16_t>(kScreenSize - 1 - (sourceTop * 2 + 15));
  esp_cache_msync(framebuffer + firstDstY * kScreenSize, static_cast<size_t>(16) * kScreenSize * sizeof(uint16_t),
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  // La frame NES peut enchainer 30 bandes sans repasser par loop(). Ceder
  // regulierement la main evite le timeout du watchdog sur le Core 1.
  if ((bandIndex & 3) == 3) yield();
#endif
}

char (*ngpRomNames)[kNgpRomNameLen] = nullptr;  // PSRAM, voir gbRomNames
uint8_t ngpRomCount = 0;
int8_t ngpRomScroll = 0;
uint8_t ngpSelectedRomIndex = 0;
constexpr uint8_t kNgpVisibleRows = 10;
constexpr int16_t kNgpRowH = 36;
constexpr int16_t kNgpRowTop = 70;

void drawNgpPage() {
  if (ngpIsLoaded()) {
    gfx->fillRect(0, 0, kScreenSize, 12, RGB565_BLACK);
    gfx->fillRect(0, 468, kScreenSize, 12, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 4);
    gfx->print(ngpRomTitle());
    gfx->setCursor(static_cast<int16_t>(kScreenSize - kMargin - 66), 4);
    gfx->print("C:QUITTER");
    return;
  }
  drawSubHeader("NEO GEO POCKET - choisis une ROM", kPalette[2]);
  if (ngpRomCount == 0) {
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 80);
    gfx->print("Aucune ROM .ngp/.ngc dans /games.");
    return;
  }
  gfx->fillRect(kMargin, kNgpRowTop, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                static_cast<int16_t>(kNgpVisibleRows * kNgpRowH), RGB565_BLACK);
  const uint8_t visible = static_cast<uint8_t>(min<int>(kNgpVisibleRows, ngpRomCount - ngpRomScroll));
  for (uint8_t i = 0; i < visible; ++i) {
    const uint8_t idx = static_cast<uint8_t>(ngpRomScroll + i);
    const int16_t y = static_cast<int16_t>(kNgpRowTop + i * kNgpRowH);
    const bool selected = (idx == ngpSelectedRomIndex);
    gfx->fillRect(kMargin, y, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                  static_cast<int16_t>(kNgpRowH - 6), selected ? kPalette[1] : RGB565_BLACK);
    gfx->drawRect(kMargin, y, static_cast<int16_t>(kScreenSize - 2 * kMargin),
                  static_cast<int16_t>(kNgpRowH - 6), selected ? kPalette[1] : kPalette[idx % kPaletteCount]);
    gfx->setTextSize(2);
    gfx->setTextColor(selected ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(static_cast<int16_t>(kMargin + 8), static_cast<int16_t>(y + 8));
    char label[kRomLabelChars + 1];
    formatRomLabel(ngpRomNames[idx], label, sizeof(label));
    gfx->print(label);
  }
}

void drawRetroPage() {
  if (gbIsLoaded()) {
    // Le rendu du jeu lui-meme vient de gbBlitLine(), appelee par
    // gbRunFrame() depuis loop() -- ici on affiche juste le cadre/titre
    // une fois, le jeu se dessine par-dessus a chaque frame.
    // Ne pas utiliser fillScreen() ici : après la lecture de la ROM et de la
    // SRAM, un flush complet du framebuffer PSRAM rendait visible un flash
    // noir dans la liste des ROM. On efface seulement la zone de jeu et le
    // bandeau, puis les premières lignes GB remplissent le reste.
    gfx->fillRect(0, 24, kScreenSize, 432, RGB565_BLACK);
    gfx->fillRect(0, 0, kScreenSize, 24, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 4);
    gfx->print(gbRomTitle());
    // Rappel discret : C quitte la partie (voir le commentaire pres de
    // "il faut un truc pour sortir de l'emulateur", 2026-09-17).
    gfx->setCursor(static_cast<int16_t>(kScreenSize - kMargin - 94), 4);
    gfx->print("C:MENU D:SAVE");
    drawGbRecIndicator();
    return;
  }

  if (gbRomCount > 0) {
    drawSubHeader(emuPickerSelected == 1 ? "GAME BOY COLOR - choisis une ROM"
                                         : "GAME BOY - choisis une ROM",
                  kPalette[2]);
    // Choix du rendu avant le lancement : 2× garde le plein débit, 3×
    // remplit la largeur de l'écran. Le choix reste actif pour la ROM
    // suivante jusqu'à ce que l'utilisateur le change.
    // [2026-09-27] Agrandis (demande utilisateur, "un peu petits") : 110x24
    // -> 150x34, texte taille 2 au lieu de 1. Voir hitTest assorti plus bas
    // (Screen::Retro && !gbIsLoaded()) et kRomRowTop decale de 90 a 104 pour
    // laisser la place sans chevaucher la liste de ROM.
    gfx->setTextSize(1);
    gfx->setTextColor(kDim);
    gfx->setCursor(kMargin, 76);
    gfx->print("AFFICHAGE");
    const bool scale2 = gbDisplayScale == 2;
    gfx->fillRect(kScaleBtnX2, kScaleBtnY, kScaleBtnW, kScaleBtnH, scale2 ? kPalette[1] : RGB565_BLACK);
    gfx->drawRect(kScaleBtnX2, kScaleBtnY, kScaleBtnW, kScaleBtnH, kPalette[1]);
    gfx->setTextSize(2);
    gfx->setTextColor(scale2 ? RGB565_BLACK : RGB565_WHITE);
    gfx->setCursor(kScaleBtnX2 + 27, kScaleBtnY + 9);
    gfx->print("X2 60FPS");
    gfx->fillRect(kScaleBtnX3, kScaleBtnY, kScaleBtnW, kScaleBtnH, scale2 ? RGB565_BLACK : kPalette[1]);
    gfx->drawRect(kScaleBtnX3, kScaleBtnY, kScaleBtnW, kScaleBtnH, kPalette[1]);
    gfx->setTextColor(scale2 ? RGB565_WHITE : RGB565_BLACK);
    gfx->setCursor(kScaleBtnX3 + 27, kScaleBtnY + 9);
    gfx->print("X3 ECRAN");
    gfx->setTextSize(1);
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
#ifdef AZ2_GB_CORE_PEANUT
      "Moteur : Peanut-GB (GB seul, pas de",
      "couleur GBC), licence MIT.",
#else
      "Moteur : Walnut-CGB (GB/GBC, licence MIT).",
#endif
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
// Modes : Matrix classique, pluie des notes du pattern, 8 pistes,
// tableau de bord musical. Un seul selecteur pilote tactile ET croix/A.
enum class SaverStyle : uint8_t { Matrix, NoteRain, EightTracks, Dashboard, Count };
SaverStyle screensaverStyle = SaverStyle::NoteRain;
uint8_t configSelectedRow = 0;
Preferences saverPreferences;
void persistSaverSettings() {
  if (!saverPreferences.begin("az2-saver", false)) {
    Serial.println("SAVER:NVS_OPEN_ERROR");
    return;
  }
  saverPreferences.putUChar("style", static_cast<uint8_t>(screensaverStyle));
  saverPreferences.putUShort("timeout", screensaverTimeoutSec);
  saverPreferences.end();
}
void restoreSaverSettings() {
  if (!saverPreferences.begin("az2-saver", true)) {
    Serial.println("SAVER:NVS_READ_ERROR");
    return;
  }
  const uint8_t storedStyle = saverPreferences.getUChar("style", static_cast<uint8_t>(SaverStyle::NoteRain));
  const uint16_t storedTimeout = saverPreferences.getUShort("timeout", 60);
  saverPreferences.end();
  screensaverStyle = storedStyle < static_cast<uint8_t>(SaverStyle::Count) ?
      static_cast<SaverStyle>(storedStyle) : SaverStyle::NoteRain;
  screensaverTimeoutSec = storedTimeout <= 600 ? storedTimeout : 60;
}
constexpr int16_t kSaverStyleY = 76;
constexpr int16_t kSaverStyleH = 46;
constexpr const char *kSaverStyleNames[] = {"MATRIX", "PLUIE DE NOTES", "8 PISTES", "DASHBOARD"};
constexpr uint16_t kScreensaverStepSec = 10;
constexpr uint16_t kScreensaverMaxSec = 600;

constexpr int16_t kCfgRowY = 165;
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

  // Le focus est commun a la croix et au tactile. HAUT/BAS deplacent
  // le cadre blanc ; GAUCHE/DROITE modifient, A confirme/incremente.
  const int16_t rowYs[4] = {kSaverStyleY, kCfgRowY, kScaleRowY, kSwingRowY};
  for (uint8_t row = 0; row < 4; ++row) {
    if (configSelectedRow == row) {
      gfx->drawRect(kMargin - 3, rowYs[row] - 3, kScreenSize - 2 * kMargin + 6,
                    kCfgRowH + 6, RGB565_WHITE);
    }
  }
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, kSaverStyleY - 16);
  gfx->print("STYLE VEILLE (CROIX + A / TACTILE)");
  gfx->fillRect(kMargin, kSaverStyleY, kScreenSize - 2 * kMargin, kSaverStyleH, RGB565_BLACK);
  gfx->drawRect(minusX, kSaverStyleY, kCfgBtnW, kSaverStyleH, kFaint);
  gfx->drawRect(plusX, kSaverStyleY, kCfgBtnW, kSaverStyleH, kFaint);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(minusX + 20, kSaverStyleY + 13);
  gfx->print('<');
  gfx->setCursor(plusX + 20, kSaverStyleY + 13);
  gfx->print('>');
  gfx->setTextSize(1);
  gfx->setCursor(kScreenSize / 2 - 52, kSaverStyleY + 17);
  gfx->print(kSaverStyleNames[static_cast<uint8_t>(screensaverStyle)]);

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

void configChangeRow(int8_t delta) {
  switch (configSelectedRow) {
    case 0: {
      const int count = static_cast<int>(SaverStyle::Count);
      screensaverStyle = static_cast<SaverStyle>(
          (static_cast<int>(screensaverStyle) + count + delta) % count);
      break;
    }
    case 1:
      screensaverTimeoutSec = static_cast<uint16_t>(constrain(
          static_cast<int>(screensaverTimeoutSec) + delta * kScreensaverStepSec,
          0, static_cast<int>(kScreensaverMaxSec)));
      break;
    case 2:
      currentScaleIndex = static_cast<uint8_t>(
          (currentScaleIndex + kScaleCount + delta) % kScaleCount);
      break;
    case 3: {
      swingValue = static_cast<uint8_t>(constrain(
          static_cast<int>(swingValue) + delta * kSwingStep, 0, 127));
      char msg[16];
      snprintf(msg, sizeof(msg), "SWING:%d", swingValue);
      sendToTeensy(msg);
      break;
    }
  }
  if (configSelectedRow <= 1) persistSaverSettings();
  drawConfigPage();
}

bool hitTestSaverStyle(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kSaverStyleY, kScreenSize - 2 * kMargin, kSaverStyleH);
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
char projectStatus[64] = "Choisir un projet avec la croix";
int8_t projectSaveArmedSlot = -1;
uint32_t projectSaveArmedUntilMs = 0;
constexpr int16_t kProjectListY = 90;
constexpr int16_t kProjectRowH = 64;
constexpr int16_t kProjectRowGap = 9;
constexpr int16_t kProjectActionY = 390;
constexpr int16_t kProjectActionH = 52;
constexpr int16_t kProjectActionGap = 8;
constexpr int16_t kProjectActionW = (kScreenSize - 2 * kMargin - kProjectActionGap) / 2;

void projectPath(uint8_t slot, char *path, size_t size) {
  snprintf(path, size, "/projects/%u.proj", slot);
}

void drawProjectRow(uint8_t slot) {
  const int16_t y = static_cast<int16_t>(kProjectListY + slot * (kProjectRowH + kProjectRowGap));
  const bool selected = slot == projectSlot;
  char path[24];
  projectPath(slot, path, sizeof(path));
  File f = SD.open(path);
  const bool present = static_cast<bool>(f);
  const size_t bytes = present ? f.size() : 0;
  if (f) f.close();
  const uint16_t accent = selected ? kPalette[3] : kFaint;
  gfx->fillRect(kMargin, y, kScreenSize - 2 * kMargin, kProjectRowH, RGB565_BLACK);
  gfx->drawRect(kMargin, y, kScreenSize - 2 * kMargin, kProjectRowH, accent);
  if (selected) gfx->fillRect(kMargin + 1, y + 1, 5, kProjectRowH - 2, accent);
  gfx->setTextSize(2);
  gfx->setTextColor(selected ? RGB565_WHITE : kDim);
  gfx->setCursor(kMargin + 15, y + 8);
  gfx->printf("%u  PROJET %u", slot + 1, slot);
  gfx->setTextSize(1);
  gfx->setTextColor(present ? kPalette[1] : kDim);
  gfx->setCursor(kMargin + 17, y + 37);
  if (present) gfx->printf("%s  -  %lu Ko sur SD ESP32", path, static_cast<unsigned long>((bytes + 1023) / 1024));
  else gfx->printf("%s  -  emplacement vide", path);
}

void drawProjectPage() {
  drawSubHeader("PROJETS - SD ESP32", kPalette[3]);
  for (uint8_t slot = 0; slot < kProjectSlotCount; ++slot) drawProjectRow(slot);
  const int16_t loadX = kMargin;
  const int16_t saveX = static_cast<int16_t>(kMargin + kProjectActionW + kProjectActionGap);
  gfx->fillRect(kMargin, kProjectActionY, kScreenSize - 2 * kMargin, kProjectActionH, RGB565_BLACK);
  gfx->drawRect(loadX, kProjectActionY, kProjectActionW, kProjectActionH, kPalette[1]);
  gfx->drawRect(saveX, kProjectActionY, kProjectActionW, kProjectActionH, kPalette[2]);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(loadX + 31, kProjectActionY + 14);
  gfx->print("CHARGER A");
  gfx->setCursor(saveX + 39, kProjectActionY + 14);
  gfx->print("SAUVER D");
  gfx->setTextSize(1);
  gfx->setTextColor(kDim);
  gfx->setCursor(kMargin, 450);
  gfx->print(projectStatus);
  gfx->setCursor(kMargin, 466);
  gfx->print("Haut/Bas: choisir  B: song  C: retour");
}

int8_t hitTestProjectRow(int16_t x, int16_t y) {
  if (x < kMargin || x >= kScreenSize - kMargin) return -1;
  for (uint8_t slot = 0; slot < kProjectSlotCount; ++slot) {
    const int16_t top = static_cast<int16_t>(kProjectListY + slot * (kProjectRowH + kProjectRowGap));
    if (y >= top && y < top + kProjectRowH) return static_cast<int8_t>(slot);
  }
  return -1;
}

bool hitTestProjectLoad(int16_t x, int16_t y) {
  return inBox(x, y, kMargin, kProjectActionY, kProjectActionW, kProjectActionH);
}

bool hitTestProjectSave(int16_t x, int16_t y) {
  return inBox(x, y, static_cast<int16_t>(kMargin + kProjectActionW + kProjectActionGap),
               kProjectActionY, kProjectActionW, kProjectActionH);
}

void saveProject(uint8_t slot);
void loadProject(uint8_t slot);

void selectProjectSlot(uint8_t slot) {
  projectSlot = slot;
  projectSaveArmedSlot = -1;
  snprintf(projectStatus, sizeof(projectStatus), "Projet %u selectionne", slot + 1);
  drawProjectPage();
}

void requestProjectSave() {
  char path[24];
  projectPath(projectSlot, path, sizeof(path));
  const uint32_t now = millis();
  if (SD.exists(path) && (projectSaveArmedSlot != static_cast<int8_t>(projectSlot) ||
                          static_cast<int32_t>(projectSaveArmedUntilMs - now) <= 0)) {
    projectSaveArmedSlot = static_cast<int8_t>(projectSlot);
    projectSaveArmedUntilMs = now + 5000;
    snprintf(projectStatus, sizeof(projectStatus), "Projet %u existe : D/SAUVER encore pour remplacer", projectSlot + 1);
    drawProjectPage();
    return;
  }
  projectSaveArmedSlot = -1;
  saveProject(projectSlot);
  drawProjectPage();
}

void requestProjectLoad() {
  projectSaveArmedSlot = -1;
  loadProject(projectSlot);
  drawProjectPage();
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
    for (uint8_t p = 0; p < kPatternCount; ++p) {
      f.printf("PLEN:%d:%d\n", p, patternMeasures[p]);
    }
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
    // Kit de batterie / echantillons assignes aux pads (2026-09-19, "il
    // faut pouvoir aussi les sauvegarder dans le projet global") --
    // seulement les pads REELLEMENT assignes (padSamplePath[pad][0] !=
    // '\0', tenu a jour par l'echo PADSAMPLE:...:READY:path=..., voir
    // handleTeensyLine()) ; les autres pads restent silencieusement
    // absents du fichier (comportement au chargement inchange pour eux).
    for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
      if (padSamplePath[pad][0] != '\0') {
        f.printf("PADSAMPLE:%d:%s\n", pad, padSamplePath[pad]);
      }
    }
    // Sample CUSTOM du moteur SAMPLER d'une piste (2026-09-23, "fusion"
    // avec le navigateur SD des pads) -- meme convention que ci-dessus :
    // seules les pistes reellement chargees (trackSamplePath[t][0] !=
    // '\0') apparaissent. trackPatch[t] (deja ecrit par la ligne TRACK:
    // ci-dessus) peut valoir kSamplerCustomPatch (3) sans que ce fichier
    // ne soit encore charge au retour (SD retiree, chemin efface) --
    // loadProject() doit rester tolerant a cette absence.
    for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
      if (trackSamplePath[t][0] != '\0') {
        f.printf("TRACKSAMPLE:%d:%s\n", t, trackSamplePath[t]);
      }
    }
    for (uint8_t p = 0; p < kPatternCount; ++p) {
      for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
        const uint8_t activeSteps = static_cast<uint8_t>(patternMeasures[p] * kSeqStepsPerMeasure);
        for (uint8_t s = 0; s < activeSteps; ++s) {
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
    snprintf(projectStatus, sizeof(projectStatus), "Erreur sauvegarde %s", path);
    return;
  }
  Serial.print("PROJECT_SAVED:");
  Serial.println(path);
  snprintf(projectStatus, sizeof(projectStatus), "Projet %u sauvegarde sur SD", slot + 1);
}

// Coupe une ligne "cle:reste" -- renvoie "reste" (String vide si pas de
// ':'). Petit utilitaire local, le reste du fichier utilise deja ce
// motif partout (indexOf(':') + substring()) mais ligne par ligne ici
// simplifie la lecture du fichier projet.
String afterColon(const String &line) {
  const int i = line.indexOf(':');
  return (i < 0) ? String("") : line.substring(i + 1);
}

bool projectNumber(const String &text, int low, int high, int &value) {
  if (text.isEmpty()) return false;
  char *end = nullptr;
  const long parsed = strtol(text.c_str(), &end, 10);
  if (*end != '\0' || parsed < low || parsed > high) return false;
  value = static_cast<int>(parsed);
  return true;
}

bool projectCsv(const String &text, int *values, uint8_t minCount, uint8_t maxCount, uint8_t &count) {
  count = 0;
  int start = 0;
  for (int i = 0; i <= text.length(); ++i) {
    if (i != text.length() && text.charAt(i) != ',') continue;
    if (count >= maxCount || !projectNumber(text.substring(start, i), 0, 255, values[count])) return false;
    ++count;
    start = i + 1;
  }
  return count >= minCount;
}

bool projectStructureValid(File &f) {
  f.seek(0);
  uint16_t tracks = 0;
  uint16_t steps = 0;
  bool hasBpm = false;
  bool hasSongLen = false;
  bool seenSongSet[kSongLength] = {};
  bool seenPadSample[az2::kPadCount] = {};
  bool seenTrackSample[kSeqTrackCount] = {};
  bool seenTrack[kSeqTrackCount] = {};
  // static, pas une variable locale ordinaire : 8*8*128 = 8192 octets, soit
  // a peu pres toute la pile par defaut de la tache principale sur l'ESP32
  // (~8 Ko) -- l'allouer sur la pile faisait planter/redemarrer la carte
  // pile au moment de la sauvegarde du morceau (atomicSaveFile() appelle
  // cette fonction juste apres l'ecriture pour verifier le fichier).
  // memset() le reinitialise a chaque appel puisque l'init "= {}" d'un
  // tableau static ne s'execute qu'une seule fois, au tout premier appel.
  static bool seenStep[kPatternCount][kSeqTrackCount][kSeqStepCount];
  memset(seenStep, 0, sizeof(seenStep));
  bool seenPatternLength[kPatternCount] = {};
  uint8_t filePatternMeasures[kPatternCount] = {1, 1, 1, 1, 1, 1, 1, 1};
  bool valid = true;
  while (f.available()) {
    const String line = f.readStringUntil('\n');
    int v = 0;
    int values[13] = {};
    uint8_t count = 0;
    if (line.startsWith("BPM:")) {
      hasBpm = projectNumber(afterColon(line), 30, 300, v);
      valid &= hasBpm;
    } else if (line.startsWith("DIV:")) {
      valid &= projectNumber(afterColon(line), 1, 255, v);
      bool supported = false;
      for (uint8_t i = 0; i < az2::kDivisionOptionCount; ++i)
        supported |= v == az2::kDivisionOptions[i].stepsPerBeat;
      valid &= supported;
    } else if (line.startsWith("SCALE:")) {
      valid &= projectNumber(afterColon(line), 0, kScaleCount - 1, v);
    } else if (line.startsWith("SWING:")) {
      valid &= projectNumber(afterColon(line), 0, 127, v);
    } else if (line.startsWith("SONGMODE:")) {
      valid &= projectNumber(afterColon(line), 0, 1, v);
    } else if (line.startsWith("SONGLEN:")) {
      hasSongLen = projectNumber(afterColon(line), 0, kSongLength, v);
      valid &= hasSongLen;
    } else if (line.startsWith("PLEN:")) {
      const String rest = afterColon(line);
      const int colon = rest.indexOf(':');
      int measures = 0;
      const bool entryValid = colon > 0 && projectNumber(rest.substring(0, colon), 0, kPatternCount - 1, v) &&
                              projectNumber(rest.substring(colon + 1), 1, kSeqMaxMeasures, measures) &&
                              !seenPatternLength[v];
      valid &= entryValid;
      if (entryValid) {
        seenPatternLength[v] = true;
        filePatternMeasures[v] = static_cast<uint8_t>(measures);
      }
    } else if (line.startsWith("SONGSET:")) {
      const String rest = afterColon(line);
      const int colon = rest.indexOf(':');
      int pattern = 0;
      const bool entryValid = colon > 0 && projectNumber(rest.substring(0, colon), 0, kSongLength - 1, v) &&
                              projectNumber(rest.substring(colon + 1), 0, kPatternCount - 1, pattern) &&
                              !seenSongSet[v];
      valid &= entryValid;
      if (entryValid) seenSongSet[v] = true;
    } else if (line.startsWith("PADSAMPLE:")) {
      const String rest = afterColon(line);
      const int colon = rest.indexOf(':');
      const String wav = colon > 0 ? rest.substring(colon + 1) : String();
      const bool entryValid = colon > 0 && projectNumber(rest.substring(0, colon), 0, az2::kPadCount - 1, v) &&
                              !seenPadSample[v] && wav.startsWith("/samples/") &&
                              wav.length() < sizeof(padSamplePath[0]);
      valid &= entryValid;
      if (entryValid) seenPadSample[v] = true;
    } else if (line.startsWith("TRACKSAMPLE:")) {
      const String rest = afterColon(line);
      const int colon = rest.indexOf(':');
      const String wav = colon > 0 ? rest.substring(colon + 1) : String();
      const bool entryValid = colon > 0 && projectNumber(rest.substring(0, colon), 0, kSeqTrackCount - 1, v) &&
                              !seenTrackSample[v] && wav.startsWith("/samples/") &&
                              wav.length() < sizeof(trackSamplePath[0]);
      valid &= entryValid;
      if (entryValid) seenTrackSample[v] = true;
    } else if (line.startsWith("TRACK:")) {
      if (!projectCsv(afterColon(line), values, 11, 13, count)) { valid = false; continue; }
      const int t = values[0];
      if (t >= kSeqTrackCount || seenTrack[t] || values[1] >= az2::kEngineCount ||
          values[2] >= az2::enginePatchCount(values[1]) ||
          values[9] > 31 || values[10] > 7 ||
          (count >= 12 && values[11] > 127) || (count >= 13 && values[12] > 1)) {
        valid = false;
        continue;
      }
      seenTrack[t] = true;
      ++tracks;
    } else if (line.startsWith("STEP:")) {
      if (!projectCsv(afterColon(line), values, 8, 10, count) || (count != 8 && count != 10)) {
        valid = false;
        continue;
      }
      const int p = values[0], t = values[1], s = values[2];
      if (p >= kPatternCount || t >= kSeqTrackCount || s >= kSeqStepCount || seenStep[p][t][s] ||
          values[3] > 1 || values[4] > 127 || values[6] > 3 ||
          (count == 10 && values[8] > 100)) {
        valid = false;
        continue;
      }
      seenStep[p][t][s] = true;
      ++steps;
    }
  }
  f.seek(0);
  uint16_t expectedSteps = 0;
  for (uint8_t p = 0; p < kPatternCount; ++p) {
    expectedSteps = static_cast<uint16_t>(expectedSteps + filePatternMeasures[p] * kSeqStepsPerMeasure * kSeqTrackCount);
    const uint8_t activeSteps = static_cast<uint8_t>(filePatternMeasures[p] * kSeqStepsPerMeasure);
    for (uint8_t t = 0; t < kSeqTrackCount; ++t)
      for (uint8_t s = 0; s < activeSteps; ++s) valid &= seenStep[p][t][s];
  }
  return valid && hasBpm && hasSongLen && tracks == kSeqTrackCount &&
         steps == expectedSteps;
}

void loadProject(uint8_t slot) {
  char path[24];
  snprintf(path, sizeof(path), "/projects/%d.proj", slot);
  File f = openSavedFile(path);
  if (!f) {
    Serial.print("PROJECT_LOAD_EMPTY:");
    Serial.println(path);
    snprintf(projectStatus, sizeof(projectStatus), "Projet %u absent de la SD", slot + 1);
    return;
  }
  if (!projectStructureValid(f)) {
    f.close();
    char bakPath[40];
    snprintf(bakPath, sizeof(bakPath), "%s.bak", path);
    f = SD.open(bakPath);
    if (!f || !savedFileCrcValid(f) || !projectStructureValid(f)) {
      if (f) f.close();
      Serial.print("PROJECT_LOAD_BADFILE:");
      Serial.println(path);
      snprintf(projectStatus, sizeof(projectStatus), "Projet %u invalide (voir journal)", slot + 1);
      return;
    }
  }

  char msg[32];
  // Un projet sans ligne PADSAMPLE pour un pad signifie "pad vide".
  // Repartir d'un kit neutre avant de rejouer les assignations.
  for (uint8_t pad = 0; pad < az2::kPadCount; ++pad) {
    snprintf(msg, sizeof(msg), "PADSAMPLE:%u:-", pad);
    sendToTeensy(msg);
    padSamplePath[pad][0] = '\0';
  }
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
    } else if (line.startsWith("PLEN:")) {
      const String rest = afterColon(line);
      const int c = rest.indexOf(':');
      if (c >= 0) {
        const uint8_t p = static_cast<uint8_t>(rest.substring(0, c).toInt());
        const uint8_t measures = static_cast<uint8_t>(rest.substring(c + 1).toInt());
        if (p < kPatternCount && measures >= 1 && measures <= kSeqMaxMeasures) {
          patternMeasures[p] = measures;
          snprintf(msg, sizeof(msg), "PLEN:%u:%u", p, measures);
          sendToTeensy(msg);
        }
      }
    } else if (line.startsWith("SONGSET:")) {
      const String rest = afterColon(line);
      const int c = rest.indexOf(':');
      if (c >= 0) {
        snprintf(msg, sizeof(msg), "SONGSET:%s:%s", rest.substring(0, c).c_str(), rest.substring(c + 1).c_str());
        sendToTeensy(msg);
      }
    } else if (line.startsWith("PADSAMPLE:")) {
      // Kit de batterie / echantillons de pad sauvegardes (2026-09-19,
      // voir saveProject()) -- renvoie la meme commande que
      // l'assignation manuelle, le Teensy la traite identiquement
      // (charge le WAV depuis SA propre carte SD, echo PADSAMPLE:...:
      // READY:... qui remet padSamplePath[] a jour cote ESP32).
      const String rest = afterColon(line);
      const int c = rest.indexOf(':');
      if (c >= 0) {
        char padMsg[96];
        snprintf(padMsg, sizeof(padMsg), "PADSAMPLE:%s:%s", rest.substring(0, c).c_str(), rest.substring(c + 1).c_str());
        sendToTeensy(padMsg);
      }
    } else if (line.startsWith("TRACKSAMPLE:")) {
      // Meme principe que PADSAMPLE: ci-dessus, pour le sample CUSTOM
      // d'une piste (2026-09-23) -- le Teensy charge le WAV et bascule
      // lui-meme trackPatch[] sur kSamplerCustomPatch (voir
      // loadWavIntoTrackSampler()), donc envoye APRES la ligne TRACK: ci-
      // dessous n'ecraserait rien : c'est l'ordre inverse qui compterait
      // (une ligne TRACK: DOIT deja avoir mis trackPatch a 3 pour ce cas,
      // ce chargement ne fait alors que remplir le buffer correspondant).
      const String rest = afterColon(line);
      const int c = rest.indexOf(':');
      if (c >= 0) {
        char trackMsg[96];
        snprintf(trackMsg, sizeof(trackMsg), "TRACKSAMPLE:%s:%s", rest.substring(0, c).c_str(),
                 rest.substring(c + 1).c_str());
        sendToTeensy(trackMsg);
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
  snprintf(projectStatus, sizeof(projectStatus), "Projet %u charge", slot + 1);
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

// Les caracteres et les notes viennent du miroir EXISTANT du tracker,
// jamais d'une chaine decorative inventee ni d'un scan SD en animation.
char trackerRainChar(uint8_t column, int16_t row) {
  const uint8_t track = static_cast<uint8_t>(
      screensaverStyle == SaverStyle::EightTracks ? (column / 5) % kSeqTrackCount :
      column % kSeqTrackCount);
  const uint8_t step = static_cast<uint8_t>(
      (static_cast<int>(seqCurrentStep) + static_cast<int>(row) + kSeqStepCount * 4) % kSeqStepCount);
  if (!seqStepOn[currentPattern][track][step]) return '-';
  constexpr char kPitchLetters[12] = {'C','c','D','d','E','F','f','G','g','A','a','B'};
  return kPitchLetters[seqStepNote[currentPattern][track][step] % 12];
}

void drawSaverDashboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565(100, 190, 120));
  gfx->setCursor(24, 32);
  gfx->print("AZ-2  TRACKER");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  char header[48];
  snprintf(header, sizeof(header), "PAT %02u  STEP %02u", currentPattern + 1, seqCurrentStep + 1);
  gfx->setCursor(24, 86);
  gfx->print(header);
  snprintf(header, sizeof(header), "BPM %u  %s", static_cast<unsigned>(seqBpm + 0.5f),
           seqPlaying ? "PLAY" : "STOP");
  gfx->setCursor(24, 117);
  gfx->print(header);
  gfx->setTextSize(2);
  for (uint8_t t = 0; t < kSeqTrackCount; ++t) {
    const uint8_t step = seqCurrentStep % kSeqStepCount;
    const bool on = seqStepOn[currentPattern][t][step];
    const uint8_t noteNum = seqStepNote[currentPattern][t][step];
    static const char *const notes[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    char line[44];
    snprintf(line, sizeof(line), "T%u %-3s %s", t + 1, on ? notes[noteNum % 12] : "---",
             az2::engineName(trackEngine[t]));
    gfx->setTextColor(on ? RGB565(100, 190, 120) : RGB565(65, 100, 75));
    gfx->setCursor(24, static_cast<int16_t>(165 + t * 32));
    gfx->print(line);
  }
}

void screensaverEnter() {
  screensaverActive = true;
  gfx->fillScreen(RGB565_BLACK);
  matrixLastStepMs = millis();
  for (uint8_t c = 0; c < kMatrixCols; ++c) {
    matrixDropRow[c] = static_cast<int16_t>(-random(0, kMatrixRows));
  }
  if (screensaverStyle == SaverStyle::Dashboard) drawSaverDashboard();
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
  const uint32_t intervalMs = screensaverStyle == SaverStyle::Dashboard ? 500UL : 110UL;
  if (now - matrixLastStepMs < intervalMs) return;
  matrixLastStepMs = now;
  if (screensaverStyle == SaverStyle::Dashboard) {
    drawSaverDashboard();
    return;
  }

  gfx->setTextSize(2);
  for (uint8_t c = 0; c < kMatrixCols; ++c) {
    // Le mode 8 pistes garde une colonne sur cinq pour chaque piste ;
    // les autres colonnes sont vides pour une lecture moins dense.
    if (screensaverStyle == SaverStyle::EightTracks && c % 5 != 0) continue;
    const int16_t x = static_cast<int16_t>(c * kMatrixCharW);

    const int16_t tailRow = static_cast<int16_t>(matrixDropRow[c] - kMatrixTrailLen);
    if (tailRow >= 0 && tailRow < kMatrixRows) {
      // Un espace en mode texte transparent n'efface pas l'ancien glyphe :
      // nettoyer physiquement la cellule du bout de la trainee.
      gfx->fillRect(x, static_cast<int16_t>(tailRow * kMatrixCharH),
                    kMatrixCharW, kMatrixCharH, RGB565_BLACK);
    }

    const int16_t trailRow = static_cast<int16_t>(matrixDropRow[c] - 1);
    if (trailRow >= 0 && trailRow < kMatrixRows) {
      gfx->setTextColor(RGB565(0, 90, 40));
      gfx->setCursor(x, static_cast<int16_t>(trailRow * kMatrixCharH));
      gfx->print(screensaverStyle == SaverStyle::Matrix ? matrixRandomChar() :
                 trackerRainChar(c, trailRow));
    }

    if (matrixDropRow[c] >= 0 && matrixDropRow[c] < kMatrixRows) {
      gfx->setTextColor(RGB565(110, 200, 120));
      gfx->setCursor(x, static_cast<int16_t>(matrixDropRow[c] * kMatrixCharH));
      gfx->print(screensaverStyle == SaverStyle::Matrix ? matrixRandomChar() :
                 trackerRainChar(c, matrixDropRow[c]));
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
    case Screen::Menu: drawMenu(); break;
    case Screen::Controls: drawControlsPage(); break;
    case Screen::Audio: drawAudioPage(); break;
    case Screen::Sampler: drawSamplerPage(); break;
    case Screen::Sequencer: drawSequencerPage(); break;
    case Screen::Engines: drawEnginesPage(); break;
    case Screen::EmuPicker: drawEmuPickerPage(); break;
#ifdef AZ2_NES_ENABLED
    case Screen::NesRetro: drawNesPage(); break;
#else
    case Screen::NesRetro: break;
#endif
    case Screen::NgpRetro: drawNgpPage(); break;
    case Screen::Retro: drawRetroPage(); break;
    case Screen::Config: drawConfigPage(); break;
    case Screen::Links: drawLinksPage(); break;
    case Screen::About: drawAboutPage(); break;
    case Screen::Patch: drawPatchPage(); break;
    case Screen::Song: drawSongPage(); break;
    case Screen::Project: drawProjectPage(); break;
    case Screen::Mixer: drawMixerPage(); break;
    case Screen::StepSeq: drawStepSeqPage(); break;
  }
  flushUiCanvas();
}

void goTo(Screen s) {
  if (s != Screen::Retro) {
    gbSettingsMenuOpen = false;
    gbCheatMenuOpen = false;
  }
  // La sortie de la page Jeux doit etre annulee si la carte SD refuse
  // la sauvegarde : conserver le jeu en RAM et la navigation intacte.
  if (s != Screen::Retro && gbIsLoaded() && !gbUnload()) {
    Serial.println("GB:NAV_BLOCKED_UNSAVED_RAM");
    // L'image GB occupe y=24..455 : afficher l'erreur dans la
    // bande de titre, sans masquer l'ecran du jeu ni effacer la RAM.
    gfx->fillRect(0, 0, kScreenSize, 22, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_RED);
    gfx->setCursor(kMargin, 5);
    gfx->print("SD SAVE ERROR - C:RETRY");
    return;
  }
  if (s != Screen::NgpRetro && ngpIsLoaded()) {
    ngpUnload();
  }
  // Parents stables des pages imbriquees : entrer dans PATCH depuis
  // MOTEURS doit permettre PATCH -> MOTEURS -> page d'origine, sans que
  // le premier retour ecrase la destination du second.
  if (s == Screen::Engines && currentScreen != Screen::Patch) enginesReturnScreen = currentScreen;
  if (s == Screen::Patch) patchReturnScreen = currentScreen;

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
#ifdef AZ2_NES_ENABLED
  if (s == Screen::NesRetro && !nesIsLoaded()) {
    nesRomCount = nesScanRoms(nesRomNames);
    nesRomScroll = 0;
    nesSelectedRomIndex = 0;
  }
#endif
  if (s == Screen::NgpRetro && !ngpIsLoaded()) {
    ngpRomCount = ngpScanRoms(ngpRomNames);
    ngpRomScroll = 0;
    ngpSelectedRomIndex = 0;
  }
  if (s == Screen::EmuPicker) {
    emuPickerSelected = 0;
  }
  if (s == Screen::Sampler) {
    snprintf(samplerFolder, sizeof(samplerFolder), "/samples");
    samplerOffset = 0;
    samplerSelectedRow = 0;
    samplerUiStatus[0] = '\0';
    sendToTeensy("PADSAMPLE?");
    requestSamplerList();
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
    menuCategory = menuReturnCategory >= 0 ? menuReturnCategory : -1;
    menuReturnCategory = -1;
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
    patchOnPresetList = true; // A puis HAUT/BAS modifient le preset affiche a cote de l'onde
    patchPresetEditing = false;
    patchOnScopeRow = false;  // voir patchOnScopeRow -- jamais focus en entrant sur la page
    patchScopeAdsrPoint = 0;
    patchScopeFilterPoint = 0;
    queryPatchExtra(static_cast<uint8_t>(patchTrack));
    // Le Teensy ne memorise pas ces 6 valeurs (voir le commentaire pres de
    // leur declaration) -- sans cet envoi, le son reel restait sur son
    // defaut de boot (setup()) alors que l'ecran affichait autre chose.
    sendPatchFilt();
    sendPatchEnv();
  } else if (currentScreen == Screen::Patch) {
    sendToTeensy("SCOPE:OFF");
  }

  currentScreen = s;
  drawScreen(s);
}

// [2026-09-27] Ouvre une categorie du menu principal -- si elle ne contient
// qu'UN SEUL item (ex. Jeux -> JEUX -> EmuPicker), saute directement sur sa
// cible au lieu d'afficher une sous-liste a un seul choix (demande
// utilisateur : "il faut enlever une etape ... le bouton JEUX va a la
// fenetre d'emulateur direct"). Categories a plusieurs items (Musique,
// Config, Doc) gardent la sous-liste normale.
void enterMenuCategory(uint8_t catIndex) {
  uint8_t items[kMenuItemCount];
  const uint8_t count = categoryItems(static_cast<MenuCat>(catIndex), items);
  if (count == 1) {
    if (kMenuItems[items[0]].target == Screen::Audio) {
      padTargetTrack = selectedSeqTrack;
      padEditsStep = false;
    }
    // La sous-liste (1 seul item) est sautee : un "retour" doit remonter
    // direct a la grille des 4 categories, pas y retomber -- menuReturnCategory
    // a -1 (pas catIndex) pour que goTo(Screen::Menu) affiche menuCategory=-1.
    menuReturnCategory = -1;
    goTo(kMenuItems[items[0]].target);
  } else {
    menuCategory = static_cast<int8_t>(catIndex);
    menuSelected = 0;
    drawMenu();
  }
}

// Valide la ligne selectionnee du menu reglages/triches (voir
// drawGbSettingsMenu()) -- appelee par la lettre A ET par le bouton
// poussoir de l'encodeur 2 (demande 2026-09-25, navigation a la molette).
void gbSettingsMenuConfirm() {
  if (gbCheatMenuOpen) {
    uint8_t count = gbGetCheatCount();
    if (gbCheatSelectedRow < count) {
      gbToggleCheat(gbCheatSelectedRow);
      drawGbSettingsMenu();
    }
    return;
  }
  if (gbSettingsSelectedRow == 0) {
    gbSettingsMenuOpen = false;
    drawRetroPage();
    drawGbViewportFrame();
  } else if (gbSettingsSelectedRow == 1) {
    const bool ok = gbSaveNow();
    gfx->fillRect(80, 200, 320, 100, RGB565_BLACK);
    gfx->drawRect(80, 200, 320, 100, kPalette[2]);
    gfx->setTextSize(2);
    gfx->setTextColor(ok ? RGB565_GREEN : RGB565_RED);
    gfx->setCursor(100, 240);
    gfx->print(ok ? "SAVE REUSSIE" : "ECHEC SAVE SD");
#ifdef AZ2_DIRECT_PANEL
    flushUiCanvas();
#else
    if (uint16_t *framebuffer = gfx->getFramebuffer(); framebuffer != nullptr) {
      esp_cache_msync(framebuffer, static_cast<size_t>(kScreenSize * kScreenSize * sizeof(uint16_t)), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif
    delay(1000);
    drawGbSettingsMenu();
  } else if (gbSettingsSelectedRow == 2) {
    const bool ok = gbLoadNow();
    gfx->fillRect(80, 200, 320, 100, RGB565_BLACK);
    gfx->drawRect(80, 200, 320, 100, kPalette[2]);
    gfx->setTextSize(2);
    gfx->setTextColor(ok ? RGB565_GREEN : RGB565_RED);
    gfx->setCursor(100, 240);
    gfx->print(ok ? "LOAD REUSSI" : "ECHEC LOAD SD");
#ifdef AZ2_DIRECT_PANEL
    flushUiCanvas();
#else
    if (uint16_t *framebuffer = gfx->getFramebuffer(); framebuffer != nullptr) {
      esp_cache_msync(framebuffer, static_cast<size_t>(kScreenSize * kScreenSize * sizeof(uint16_t)), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif
    delay(1000);
    drawGbSettingsMenu();
  } else if (gbSettingsSelectedRow == 3) {
    gbCheatMenuOpen = true;
    gbCheatSelectedRow = 0;
    drawGbSettingsMenu();
  } else if (gbSettingsSelectedRow == 4) {
    gbSettingsMenuOpen = false;
    gbCheatMenuOpen = false;
    goTo(navPrevious);
  }
}

// Retour/sortie du menu reglages/triches -- appelee par la lettre B ET par
// le bouton poussoir de l'encodeur 3 (demande 2026-09-25).
void gbSettingsMenuBack() {
  if (gbCheatMenuOpen) {
    gbCheatMenuOpen = false;
    drawGbSettingsMenu();
  } else {
    gbSettingsMenuOpen = false;
    drawRetroPage();
    drawGbViewportFrame();
  }
}

// ---------------------------------------------------------------------
// Reception Teensy : met a jour l'etat partage + le journal + la page
// courante si elle affiche la donnee concernee.
// ---------------------------------------------------------------------
String teensyLine;

void handleTeensyLine(const String &line) {
  if (line == az2::kGbAudioV2Ready) {
    gbSetAudioV2Ready(true);
  } else if (line == "GBV2:DISABLED") {
    gbSetAudioV2Ready(false);
  } else if (line == "RACK_CAP:1" || line == "RACK_CAP:0") {
    const bool cap = (line == "RACK_CAP:1");
    if (cap != teensyRackCapable) {
      teensyRackCapable = cap;
      if (currentScreen == Screen::Engines && !screensaverActive) drawEnginesPage();
    }
  }
  Serial.print("TEENSY:");
  Serial.println(line);
  pushLog(line);

  // [2026-09-25] Ces deux familles de messages partaient bien du Teensy
  // (confirme par audit) mais n'avaient AUCUN retour visuel a l'ecran --
  // seulement pushLog() ci-dessus (log discret, pas un vrai retour pour
  // l'utilisateur). Toast simple, pas de delay() bloquant ici (voir plus
  // bas dans ce fichier : un delay() dans handleTeensyLine() a deja
  // retarde BTN:/NAV: par le passe) -- le toast reste affiche jusqu'a la
  // prochaine navigation, qui redessine l'ecran normalement.
  if (line.startsWith("SAMPLER:GB_CAPTURE:") || line.startsWith("GRANULAR_SAMPLE:")) {
    const bool isReady = line.indexOf(":READY:") >= 0;
    const bool isQueued = line.indexOf(":QUEUED:") >= 0;
    const bool isSent = line.indexOf(":SENT:") >= 0;
    const bool isError = line.indexOf(":ERROR") >= 0 || line.indexOf("_ERROR") >= 0;
    if (isReady || isQueued || isSent || isError) {
      gfx->fillRect(80, 190, 320, 100, RGB565_BLACK);
      gfx->drawRect(80, 190, 320, 100, kPalette[2]);
      gfx->setTextSize(2);
      gfx->setTextColor(isError ? RGB565_RED : RGB565_GREEN);
      gfx->setCursor(100, 220);
      if (line.startsWith("SAMPLER:GB_CAPTURE:")) {
        gfx->print(isReady ? "SAMPLE GB PRET" : "ECHEC CAPTURE GB");
      } else {
        gfx->print(isError ? "ECHEC SAMPLE GRANULAR" : "SAMPLE GRANULAR OK");
      }
#ifdef AZ2_DIRECT_PANEL
      flushUiCanvas();
#else
      if (uint16_t *framebuffer = gfx->getFramebuffer(); framebuffer != nullptr) {
        esp_cache_msync(framebuffer, static_cast<size_t>(kScreenSize * kScreenSize * sizeof(uint16_t)), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
      }
#endif
    }
  }

  if (line.startsWith("SAMPLEFILE:") || line.startsWith("SAMPLEDIR:")) {
    const bool isDir = line.startsWith("SAMPLEDIR:");
    const int prefixLen = isDir ? 10 : 11;
    const int colon = line.indexOf(':', prefixLen);
    if (colon > prefixLen) {
      const int row = line.substring(prefixLen, colon).toInt();
      if (row >= 0 && row < kSamplerRows) {
        line.substring(colon + 1).toCharArray(samplerFiles[row], sizeof(samplerFiles[row]));
        samplerIsDir[row] = isDir;
        if (row + 1 > samplerVisible) samplerVisible = row + 1;
      }
    }
    return;
  }
  if (line.startsWith("SAMPLELIST:DONE:")) {
    const int colon = line.indexOf(':', 16);
    samplerTotal = static_cast<uint16_t>(line.substring(16, colon).toInt());
    samplerNeedsRedraw = true;
    return;
  }

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
        if (pressed && currentScreen == Screen::StepSeq && (index == 2 || index == 3)) {
          // GAUCHE/DROITE deplace le curseur de pas sur toute la longueur
          // reelle du pattern (pas seulement la fenetre de 32 affichee) --
          // meme variable selectedSeqStep que le tracker classique, pour
          // que la position reste coherente en changeant de vue.
          const uint8_t activeSteps =
              static_cast<uint8_t>(patternMeasures[currentPattern] * kSeqStepsPerMeasure);
          selectedSeqStep = static_cast<int8_t>(
              (selectedSeqStep + (index == 3 ? 1 : activeSteps - 1)) % activeSteps);
          drawStepSeqPage();
        }
        if (pressed && currentScreen == Screen::Sampler && !screensaverActive) {
          if (index == 0) {
            if (samplerSelectedRow > 0) --samplerSelectedRow;
            else if (samplerOffset >= kSamplerRows) {
              samplerOffset -= kSamplerRows;
              samplerSelectedRow = kSamplerRows - 1;
              requestSamplerList();
            }
          } else if (index == 1) {
            if (samplerSelectedRow + 1 < samplerVisible) ++samplerSelectedRow;
            else if (samplerOffset + kSamplerRows < samplerTotal) {
              samplerOffset += kSamplerRows;
              samplerSelectedRow = 0;
              requestSamplerList();
            }
          } else {
            samplerSelectedPad = static_cast<uint8_t>((samplerSelectedPad + (index == 3 ? 1 : 15)) % 16);
          }
          drawSamplerPage();
        }
        if (pressed && currentScreen == Screen::Project && !screensaverActive) {
          const int8_t delta = (index == 0 || index == 2) ? -1 : 1;
          selectProjectSlot(static_cast<uint8_t>((projectSlot + kProjectSlotCount + delta) % kProjectSlotCount));
        }
        if (pressed && currentScreen == Screen::Song && !screensaverActive) {
          if (index == 0 || index == 1) {
            const int next = constrain(static_cast<int>(songSelectedSlot) + (index == 0 ? -4 : 4), 0, kSongLength - 1);
            songSelectedSlot = static_cast<uint8_t>(next);
          } else {
            const uint8_t slot = songSelectedSlot;
            songPatterns[slot] = static_cast<uint8_t>((songPatterns[slot] + (index == 2 ? kPatternCount - 1 : 1)) % kPatternCount);
            if (slot >= songLen) {
              songLen = slot + 1;
              char lenMsg[16];
              snprintf(lenMsg, sizeof(lenMsg), "SONGLEN:%u", songLen);
              sendToTeensy(lenMsg);
            }
            char msg[20];
            snprintf(msg, sizeof(msg), "SONGSET:%u:%u", slot, songPatterns[slot]);
            sendToTeensy(msg);
          }
          drawSongPage();
        }
        if (currentScreen == Screen::Controls && !screensaverActive) {
          drawNavBox(static_cast<uint8_t>(index));
        }
        // Page JEUX, partie en cours : la croix pilote directement le
        // Game Boy (voir gb_emulator.h -- GbButton::Up/Down/Left/Right
        // sont dans le meme ordre que index ici, 0-3).
        if (currentScreen == Screen::Retro && gbIsLoaded()) {
          if (gbSettingsMenuOpen) {
            if (pressed) {
              if (gbCheatMenuOpen) {
                uint8_t count = gbGetCheatCount();
                if (count > 0) {
                  if (index == 1) { // BAS
                    gbCheatSelectedRow = (gbCheatSelectedRow + 1) % count;
                    drawGbSettingsMenu();
                  } else if (index == 0) { // HAUT
                    gbCheatSelectedRow = (gbCheatSelectedRow + count - 1) % count;
                    drawGbSettingsMenu();
                  }
                }
              } else {
                if (index == 1) { // BAS
                  gbSettingsSelectedRow = (gbSettingsSelectedRow + 1) % 5;
                  drawGbSettingsMenu();
                } else if (index == 0) { // HAUT
                  gbSettingsSelectedRow = (gbSettingsSelectedRow + 4) % 5;
                  drawGbSettingsMenu();
                }
              }
            }
          } else {
            gbSetButton(static_cast<GbButton>(index), pressed);
          }
        }
#ifdef AZ2_NES_ENABLED
        if (currentScreen == Screen::NesRetro && nesIsLoaded()) {
          if (index <= 3) nesSetButton(static_cast<NesButton>(index + 4), pressed);
        }
        if (pressed && currentScreen == Screen::NesRetro && !nesIsLoaded() && nesRomCount > 0 &&
            (index == 0 || index == 1)) {
          if (index == 1 && nesSelectedRomIndex + 1 < nesRomCount) ++nesSelectedRomIndex;
          if (index == 0 && nesSelectedRomIndex > 0) --nesSelectedRomIndex;
          if (nesSelectedRomIndex < nesRomScroll) nesRomScroll = nesSelectedRomIndex;
          if (nesSelectedRomIndex >= nesRomScroll + kNesVisibleRows)
            nesRomScroll = nesSelectedRomIndex - kNesVisibleRows + 1;
          drawNesPage();
        }
#endif
        if (currentScreen == Screen::NgpRetro && ngpIsLoaded()) {
          ngpSetButton(static_cast<NgpButton>(index), pressed);
        }
        if (pressed && currentScreen == Screen::NgpRetro && !ngpIsLoaded() && ngpRomCount > 0 &&
            (index == 0 || index == 1)) {
          if (index == 1 && ngpSelectedRomIndex + 1 < ngpRomCount) ++ngpSelectedRomIndex;
          if (index == 0 && ngpSelectedRomIndex > 0) --ngpSelectedRomIndex;
          if (ngpSelectedRomIndex < ngpRomScroll) ngpRomScroll = ngpSelectedRomIndex;
          if (ngpSelectedRomIndex >= ngpRomScroll + kNgpVisibleRows)
            ngpRomScroll = ngpSelectedRomIndex - kNgpVisibleRows + 1;
          drawNgpPage();
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
        if (pressed && currentScreen == Screen::Config) {
          if (index == 0) configSelectedRow = (configSelectedRow + 3) % 4;
          else if (index == 1) configSelectedRow = (configSelectedRow + 1) % 4;
          else if (index == 2 || index == 3) configChangeRow(index == 2 ? -1 : 1);
          if (index == 0 || index == 1) drawConfigPage();
        }
        if (pressed && currentScreen == Screen::EmuPicker && (index == 0 || index == 1)) {
          // HAUT/BAS seulement (croix GAUCHE/DROITE inutilisee, cartes
          // empilees verticalement) -- meme convention que Config ci-dessus.
          emuPickerSelected = static_cast<int8_t>(
              index == 0 ? (emuPickerSelected + kEmuCardCount - 1) % kEmuCardCount
                         : (emuPickerSelected + 1) % kEmuCardCount);
          drawEmuPickerPage();
        }
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
          if (seqSideFocus) {
            if (index == 0 || index == 1) {
              const int delta = index == 0 ? -1 : 1;
              seqSideIndex = static_cast<uint8_t>(constrain(static_cast<int>(seqSideIndex) + delta, 0, 5));
              drawTrkSidePanel();
            } else if (index == 2) {
              seqSideFocus = false;
              seqDetailCol = 5;
              drawTrkSidePanel();
              drawDetailRow(s);
            }
          } else if (seqVerticalFocus == 2) {
            if (index == 2 || index == 3) {
              switchToPattern(static_cast<uint8_t>(currentPattern + (index == 3 ? 1 : kPatternCount - 1)));
            } else if (index == 1) {
              seqVerticalFocus = 1;
              drawSeqDetailPage();
            }
          } else if (seqVerticalFocus == 1) {
            if (index == 2 || index == 3) {
              selectedSeqTrack = static_cast<int8_t>(
                  (selectedSeqTrack + (index == 3 ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
              drawSeqDetailPage();
            } else if (index == 0) {
              seqVerticalFocus = 2;
              drawSeqDetailPage();
            } else if (index == 1) {
              seqVerticalFocus = 0;
              drawSeqDetailPage();
            }
          } else {
            if (index == 2 || index == 3) {
              if ((index == 3 && seqDetailCol == 5) || (index == 2 && seqDetailCol == 0)) {
                seqSideFocus = true;
                seqSideIndex = index == 3 ? 0 : 5;
                drawTrkSidePanel();
              } else {
                const int8_t prevCol = seqDetailCol;
                seqDetailCol = static_cast<int8_t>((seqDetailCol + (index == 3 ? 1 : 5)) % 6);
                if (seqDetailCol != prevCol) {
                  drawDetailRow(s);
                }
              }
            } else if ((index == 0 || index == 1) && !btnState[0]) {
              // [2026-09-25] Bug reel trouve en testant : cette condition
              // comparait au premier pas de la mesure VISIBLE (firstVisible)
              // au lieu du tout premier pas du pattern (0) -- sur la mesure
              // 2+, HAUT au premier pas de cette mesure partait donc dans le
              // focus vertical au lieu de redescendre a la mesure precedente,
              // rendant les mesures au-dela de la premiere inaccessibles en
              // arriere avec la croix ("affiche la mesure 2 mais on peut pas
              // remonter"). Comparer a 0 (vrai debut du pattern) au lieu de
              // firstVisible corrige le probleme sans toucher au cas normal
              // (mesure 1, firstVisible vaut deja 0).
              if (index == 0 && selectedSeqStep == 0) {
                seqVerticalFocus = 1;
                drawSeqDetailPage();
                return;
              }
              const int8_t prevStep = selectedSeqStep;
              const int8_t delta = (index == 0) ? -1 : 1;
              const int maxStep = patternMeasures[currentPattern] * kSeqStepsPerMeasure - 1;
              selectedSeqStep = static_cast<int8_t>(
                  constrain(static_cast<int>(selectedSeqStep) + delta, 0, maxStep));
              if (selectedSeqStep != prevStep) {
                const uint8_t newMeasure = static_cast<uint8_t>(selectedSeqStep / kSeqStepsPerMeasure);
                if (newMeasure != seqVisibleMeasure) {
                  seqVisibleMeasure = newMeasure;
                  drawSeqDetailPage();
                } else {
                  drawDetailRow(static_cast<uint8_t>(prevStep));
                  drawDetailRow(static_cast<uint8_t>(selectedSeqStep));
                }
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
          if (patchOnScopeRow) {
            // Focus sur l'oscilloscope (voir patchOnScopeRow plus haut) :
            // HAUT ressort vers SLOT, GAUCHE/DROITE avance/recule le point
            // ADSR selectionne (meme effet que le clic d'un des 2
            // encodeurs, voir handleTeensyLine() "ENCSW:"), BAS ne fait
            // rien (deja tout en bas).
            if (index == 0) {
              patchOnScopeRow = false;
              selectedPatchRow = static_cast<int8_t>(slotRow);
              drawPatchScope();
              redrawPatchLogicalRow(t, slotRow);
              updatePatchEncoderHints();
            } else if (index == 2 || index == 3) {
              patchScopeAdsrPoint = static_cast<uint8_t>((patchScopeAdsrPoint + (index == 3 ? 1 : 3)) % 4);
              drawPatchScope();
              updatePatchEncoderHints();
            }
          } else if (patchOnPresetList) {
            // Pendant l'appui physique sur A, ne laisse pas la meme
            // direction tomber jusqu'a patchApplyDelta() (qui editerait
            // par erreur un parametre du bas alors que le focus est ici).
            if (!btnState[0] && patchPresetEditing && (index == 0 || index == 1)) {
              const uint16_t count = az2::enginePatchCount(trackEngine[t]);
              if (count > 0) {
                const int direction = index == 0 ? -1 : 1;
                const uint16_t next = static_cast<uint16_t>(
                    (static_cast<int>(trackPatch[t]) + direction + count) % count);
                char msg[20];
                snprintf(msg, sizeof(msg), "PATCH:%u:%u", t, next);
                sendToTeensy(msg);
              }
            } else if (!btnState[0] && !patchPresetEditing && (index == 1 || index == 3)) {
              patchOnPresetList = false;
              patchPresetEditing = false;
              drawPatchList();
              redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
              updatePatchEncoderHints();
            } else if (!btnState[0] && !patchPresetEditing && (index == 0 || index == 2)) {
              patchOnPresetList = false;
              patchPresetEditing = false;
              patchOnTrackRow = true;
              drawPatchList();
              drawPatchTrackRow();
            }
          } else if (btnState[0] && !patchOnTrackRow && selectedPatchRow == static_cast<int8_t>(slotRow) &&
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
              // Meme raison qu'a l'entree sur la page (voir goTo()) : le
              // Teensy ne memorise pas FILT/ENV, chaque piste doit repousser
              // les siens en changeant de piste ici.
              sendPatchFilt();
              sendPatchEnv();
              drawPatchPage();
            } else if (index == 1) {  // BAS -- entre dans la grille au focus
              patchOnTrackRow = false;
              patchOnPresetList = true;
              patchPresetEditing = false;
              drawPatchTrackRow();
              drawPatchList();
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
            } else if (next == prevRow && index == 1 && prevRow == static_cast<int8_t>(slotRow)) {
              // BAS depuis SLOT (derniere ligne, patchStepVisual() ne
              // bouge pas) -- entre dans le focus oscilloscope/ADSR (voir
              // patchOnScopeRow plus haut, "je suis coince ... je peux
              // pas descendre").
              patchOnScopeRow = true;
              redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
              drawPatchScope();
              updatePatchEncoderHints();
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
          // Les encodeurs 1/2 suivent la ligne VISUELLE selectionnee
          // (voir patchEncoderLogicalRow()) -- met a jour leur libelle a
          // chaque mouvement croix, que la selection ait change de
          // ligne ou juste de piste/valeur (peu couteux, un redessin de
          // texte).
          if (!screensaverActive) {
            updatePatchEncoderHints();
          }
        }
        // Page MIXER (2026-09-19, "le mixeur doit gerer le volume de
        // toutes les voix") : GAUCHE/DROITE choisit la piste, HAUT/BAS
        // regle DIRECTEMENT son volume (pas besoin de maintenir A --
        // metaphore fader, le geste le plus frequent sur cette page,
        // voir aussi l'encodeur 2 pour la meme action -- dispatch
        // POT: plus bas).
        if (pressed && currentScreen == Screen::Mixer) {
          if (index == 2 || index == 3) {
            selectedMixerTrack = static_cast<int8_t>(
                (selectedMixerTrack + (index == 3 ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
            drawMixerPage();
          } else if (index == 0 || index == 1) {
            const uint8_t t = static_cast<uint8_t>(selectedMixerTrack);
            const int delta = (index == 0) ? 1 : -1;
            uint8_t &vol = trackVolume[t];
            vol = static_cast<uint8_t>(constrain(static_cast<int>(vol) + delta, 0, 127));
            char msg[16];
            snprintf(msg, sizeof(msg), "VOL:%d:%d", t, vol);
            sendToTeensy(msg);
            drawMixerTrack(t);
          }
        }
      }
    }
  } else if (line.startsWith("BTN:") && line.length() >= 6) {
    const char letter = line.charAt(4);
    const bool pressed = line.endsWith("DOWN");
    // Snapshot avant les actions de cet appui : ouvrir la carte NES avec A
    // ne doit pas reutiliser le meme A pour lancer sa premiere ROM.
    const Screen screenAtButton = currentScreen;
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
      if (pressed && currentScreen == Screen::Sampler && !screensaverActive) {
        if (letter == 'A') {
          if (samplerIsDir[samplerSelectedRow]) samplerOpenSelected();
          else samplerAssignSelected();
        }
        else if (letter == 'B') {
          char msg[32];
          snprintf(msg, sizeof(msg), "PAD:%02u:DOWN:vel=100", samplerSelectedPad);
          sendToTeensy(msg);
        }
      } else if (!pressed && currentScreen == Screen::Sampler && letter == 'B') {
        char msg[24];
        snprintf(msg, sizeof(msg), "PAD:%02u:UP", samplerSelectedPad);
        sendToTeensy(msg);
      }
      // Page JEUX : A/B -> boutons Game Boy A/B. SELECT/START sont
      // passes aux encodeurs 2/3 (voir ENC: plus bas). C/D restent
      // libres dans l'emulateur pour les futurs roles de gachette.
      const bool inGbGame = (currentScreen == Screen::Retro && gbIsLoaded());
      if (inGbGame && index < 2) {
        if (gbSettingsMenuOpen) {
          if (pressed) {
            if (letter == 'A') {
              gbSettingsMenuConfirm();
            } else if (letter == 'B') {
              gbSettingsMenuBack();
            }
          }
        } else {
          static const GbButton kGbMap[2] = {GbButton::A, GbButton::B};
          gbSetButton(kGbMap[index], pressed);
        }
      }
      const bool inNgpGame = (currentScreen == Screen::NgpRetro && ngpIsLoaded());
      if (inNgpGame && index < 2) {
        ngpSetButton(static_cast<NgpButton>(static_cast<uint8_t>(NgpButton::A) + index), pressed);
      }
#ifdef AZ2_NES_ENABLED
      if (currentScreen == Screen::NesRetro && nesIsLoaded() && index < 2) {
        nesSetButton(static_cast<NesButton>(index), pressed);
      }
      if (currentScreen == Screen::NesRetro && nesIsLoaded() && letter == 'D' && pressed) {
        nesSaveRam();
      }
#endif
      if (pressed && letter == 'A' && screenAtButton == Screen::NgpRetro &&
          currentScreen == Screen::NgpRetro && !ngpIsLoaded() && ngpRomCount > 0) {
        if (ngpLoadRom(ngpRomNames[ngpSelectedRomIndex])) drawNgpPage();
      }
      // Page JEUX, liste de ROM (pas encore charge) : A charge la ROM
      // choisie par la croix -- meme convention que le tactile
      // (toucher une ligne), et que A pour confirmer ailleurs (menu).
      if (pressed && letter == 'A' && currentScreen == Screen::Retro && !gbIsLoaded() && gbRomCount > 0) {
        if (gbLoadRom(gbRomNames[selectedRomIndex])) {
          drawRetroPage();
          drawGbViewportFrame();
        }
      }
      // C/D ne quittent plus la partie et ne declenchent plus de sauvegarde:
      // ils restent reserves aux futures gachettes dans l'emulateur.
      // Menu principal : A confirme la selection surlignee par la
      // croix (voir menuSelected ci-dessus) -- demande 2026-09-15.
      // Grille de categories -> entre dans la categorie ; sous-liste ->
      // ouvre la page choisie (comme un tap tactile).
      if (pressed && letter == 'A' && currentScreen == Screen::Config) {
        configChangeRow(1);
      }
      if (pressed && letter == 'A' && currentScreen == Screen::Audio) {
        goTo(Screen::Sampler);
      }
      if (pressed && currentScreen == Screen::Project) {
        if (letter == 'A') requestProjectLoad();
        else if (letter == 'B') goTo(Screen::Song);
        else if (letter == 'D') requestProjectSave();
      }
      if (pressed && currentScreen == Screen::Song) {
        if (letter == 'A') {
          songMode = !songMode;
          char msg[16];
          snprintf(msg, sizeof(msg), "SONGMODE:%u", songMode ? 1 : 0);
          sendToTeensy(msg);
          drawSongPage();
        } else if (letter == 'B') {
          goTo(Screen::Project);
        } else if (letter == 'D') {
          songLen = static_cast<uint8_t>((songLen + 1) % (kSongLength + 1));
          char msg[16];
          snprintf(msg, sizeof(msg), "SONGLEN:%u", songLen);
          sendToTeensy(msg);
          drawSongPage();
        }
      }
      // A confirme le bouton latéral actuellement sélectionné par la
      // croix dans le tracker. Les boutons tactiles gardent leur action
      // directe ; ce chemin rend les mêmes fonctions accessibles sans
      // toucher l'écran.
      if (pressed && letter == 'A' && currentScreen == Screen::Sequencer && seqSideFocus) {
        switch (seqSideIndex) {
          case 0:
            selectedEngineTrack = selectedSeqTrack;
            goTo(Screen::Engines);
            break;
          case 1:
            patchTrack = selectedSeqTrack;
            scopeHasData = false;
            goTo(Screen::Patch);
            break;
          case 2:
            seqSideFocus = false;
            seqDetailCol = 2;
            drawTrkSidePanel();
            drawDetailRow(static_cast<uint8_t>(selectedSeqStep));
            break;
          case 3:
            padEditsStep = true;
            padTargetTrack = selectedSeqTrack;
            goTo(Screen::Audio);
            break;
          case 4: {
            char msg[12];
            snprintf(msg, sizeof(msg), "METRO:%d", metronomeOn ? 0 : 1);
            sendToTeensy(msg);
            break;
          }
          case 5:
            saveProject(projectSlot);
            break;
        }
      }
      // Page SEQUENCEUR : les boutons physiques donnent accès aux pages
      // moteur/patch et au transport sans devoir toucher le petit panneau
      // lateral. B ouvre MOTEURS sur la piste du tracker ; dans cette page
      // la croix GAUCHE/DROITE choisit MOTEUR ou PATCH, HAUT/BAS change la
      // selection, et A ouvre ensuite le detail du patch. C est reserve a
      // PLAY/STOP ; D reste le FILL tant qu'il est maintenu.
      if (pressed && letter == 'A' && currentScreen == Screen::EmuPicker) {
        emuPickerActivate(static_cast<uint8_t>(emuPickerSelected));
      }
#ifdef AZ2_NES_ENABLED
      if (pressed && letter == 'A' && screenAtButton == Screen::NesRetro &&
          currentScreen == Screen::NesRetro && !nesIsLoaded() && nesRomCount > 0) {
        if (nesLoadRom(nesRomNames[nesSelectedRomIndex])) drawNesPage();
      }
#endif
      if (pressed && letter == 'A' && currentScreen == Screen::Menu) {
        if (menuCategory < 0) {
          enterMenuCategory(static_cast<uint8_t>(menuSelected));
        } else {
          uint8_t items[kMenuItemCount];
          const uint8_t count = categoryItems(static_cast<MenuCat>(menuCategory), items);
          if (menuSelected < count) {
            // AUDIO ouverte depuis le menu general (pas depuis le
            // bouton CLAVIER du tracker) : pas de piste de reference,
            // repart sur la voix live generique -- sinon un etat
            // laisse par une visite CLAVIER precedente resterait colle
            // (meme raisonnement que patchScroll/engPatchScroll a
            // chaque entree de page).
            if (kMenuItems[items[menuSelected]].target == Screen::Audio) {
              padTargetTrack = selectedSeqTrack;
              padEditsStep = false;
            }
            menuReturnCategory = menuCategory;
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
      // Page PATCH : meme philosophie que les autres cadres de reglage.
      // La croix seule navigue entre les zones ; A verrouille/deverrouille
      // l'edition du cadre de presets. Le liseré jaune confirme le mode
      // edition, puis HAUT/BAS appliquent CLOUD/AIR/etc. Un second A rend
      // la croix a la navigation normale vers les parametres du bas.
      if (pressed && letter == 'A' && currentScreen == Screen::Patch && patchOnPresetList) {
        patchPresetEditing = !patchPresetEditing;
        drawPatchList();
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
      if (pressed && letter == 'C' && currentScreen == Screen::Sampler) {
        samplerGoUp();
      } else if (pressed && letter == 'C' &&
                 (currentScreen == Screen::Sequencer || currentScreen == Screen::StepSeq)) {
        // [2026-09-25] PLAY/STOP du tracker n'etait accessible qu'au toucher
        // (hitTestTrkPlay()) malgre le commentaire plus haut annoncant C
        // pour PLAY/STOP sur cette page -- jamais reellement cable, C
        // tombait dans le "retour" generique ci-dessous et faisait quitter
        // la page. Meme commande que le toucher. Partagee avec SEQ. PAS
        // (2026-09-26), meme transport.
        sendToTeensy(seqPlaying ? az2::kStop : az2::kPlay);
      } else if (pressed && letter == 'A' && currentScreen == Screen::StepSeq) {
        // [2026-09-26] Bascule ON/OFF du pas au curseur -- meme message
        // que le tracker classique (STEP:piste:pas:0/1, voir
        // hitTestDetailRow()), juste declenche par A ici au lieu du
        // double-toucher.
        const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
        const uint8_t step = static_cast<uint8_t>(selectedSeqStep);
        const bool newState = !seqStepOn[currentPattern][track][step];
        seqStepOn[currentPattern][track][step] = newState;
        char msg[20];
        snprintf(msg, sizeof(msg), "STEP:%d:%d:%d", track, step, newState ? 1 : 0);
        sendToTeensy(msg);
        drawStepSeqPage();
      } else if (pressed && letter == 'C' && currentScreen != Screen::Menu && !inGbGame &&
                 !inNgpGame && !(currentScreen == Screen::NesRetro && nesIsLoaded())) {
        // Toute page ouverte depuis une catégorie revient à cette
        // catégorie, même après un détour par MOTEURS, PATCH ou AUDIO.
        // Les pages ouvertes hors menu gardent le retour d'un niveau.
        if (currentScreen == Screen::Patch) {
          goTo(patchReturnScreen);
        } else if (currentScreen == Screen::Engines) {
          goTo(enginesReturnScreen);
        } else if (currentScreen == Screen::Retro) {
          goTo(Screen::EmuPicker);
#ifdef AZ2_NES_ENABLED
        } else if (currentScreen == Screen::NesRetro) {
          nesUnload();
          goTo(Screen::EmuPicker);
#endif
        } else if (currentScreen == Screen::NgpRetro) {
          ngpUnload();
          goTo(Screen::EmuPicker);
        } else if (menuReturnCategory >= 0) {
          goTo(Screen::Menu);
        } else {
          goTo(navPrevious);
        }
      }
      // Page PATCH : B joue/coupe une note de test DIRECTEMENT sur la
      // piste affichee (2026-09-19, "il faut utiliser le bouton B pour
      // jouer une note qu'on entende les modifications") -- indispensable
      // pour entendre l'effet d'un reglage en cours d'edition sans
      // devoir lancer PLAY/le sequenceur. Note fixe (note 60 = C4,
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
      if (currentScreen == Screen::Engines && letter == 'B') {
        // [2026-09-26] Demande : pouvoir tester le son d'un moteur SANS
        // quitter la page MOTEURS (avant, seul B->bascule banque de
        // parametres etait cable ici ; tester demandait d'aller sur
        // PATCH, voir A ci-dessus). Meme convention TEST:piste:60:1/0
        // que la page PATCH, en plus du basculement de banque existant
        // (garde les deux, pas de conflit reel : la banque bascule au
        // press, la note joue tant que B reste enfonce).
        char msg[16];
        snprintf(msg, sizeof(msg), "TEST:%d:60:%d", selectedEngineTrack, pressed ? 1 : 0);
        sendToTeensy(msg);
        if (pressed) {
          engineParamBank = static_cast<uint8_t>(engineParamBank ^ 1U);
          drawEngVisualizer();
        }
      }
      // Page MIXER (2026-09-19, "le mixeur doit gerer le volume de
      // toutes les voix") : B = mute, D = solo sur la piste
      // actuellement selectionnee (GAUCHE/DROITE ou encodeur 1, voir le
      // dispatch POT: plus bas) -- memes conventions que MUTE:/SOLO:
      // partout ailleurs (Engines page notamment pour SOLO), avec cette
      // fois un vrai indicateur visuel (M/S sous la barre, voir
      // drawMixerTrack()). PAS le bouton A : bug reel trouve en testant
      // -- A est le bouton "confirmer" du menu, qui vient de faire
      // goTo(Screen::Mixer) DANS CE MEME appui (currentScreen a deja
      // change avant que ce bloc ne s'execute plus bas dans la meme
      // fonction) -- entrer sur la page mettait donc aussitot la piste 0
      // en mute par accident. B est libre sur cette page (seulement pris
      // par la page PATCH, voir plus bas).
      if (pressed && currentScreen == Screen::Mixer && (letter == 'B' || letter == 'D')) {
        toggleMixerMuteSolo(letter == 'B');
      }
    }
  } else if (line.startsWith("TURN:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t index = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const int direction = line.substring(i2 + 1).toInt();
      if (currentScreen == Screen::Audio && padMenuOpen && index == 1 && direction != 0) {
        padMenuIndex = static_cast<uint8_t>(
            (padMenuIndex + (direction > 0 ? 1 : kPadMenuCount - 1)) % kPadMenuCount);
        drawPadMenu();
      }
      // [2026-09-26] Page SEQ. PAS : encodeur 2 change de piste, encodeur
      // 3 change la note du pas au curseur -- memes encodeurs Reverb/Delay
      // que le reste de l'appli (deja reutilises pour la navigation
      // ailleurs, voir le menu JEUX plus bas), donc leur rotation agit
      // aussi sur les envois FX en temps reel cote Teensy : compromis deja
      // accepte par le reste du code, pas nouveau ici.
      if (currentScreen == Screen::StepSeq && direction != 0) {
        if (index == 1) {
          selectedSeqTrack = static_cast<int8_t>(
              (selectedSeqTrack + (direction > 0 ? 1 : kSeqTrackCount - 1)) % kSeqTrackCount);
          drawStepSeqPage();
        } else if (index == 2) {
          const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
          const uint8_t step = static_cast<uint8_t>(selectedSeqStep);
          const uint8_t newNote = nextNoteInScale(seqStepNote[currentPattern][track][step],
                                                   static_cast<int8_t>(direction));
          seqStepNote[currentPattern][track][step] = newNote;
          char msg[20];
          snprintf(msg, sizeof(msg), "NOTE:%d:%d:%d", track, step, newNote);
          sendToTeensy(msg);
          drawStepSeqPage();
        }
      }
      // Menu reglages/triches JEUX : demande 2026-09-25, naviguer a la
      // molette de l'encodeur 2 plutot qu'a la croix.
      if (currentScreen == Screen::Retro && gbIsLoaded() && gbSettingsMenuOpen &&
          index == 1 && direction != 0) {
        if (gbCheatMenuOpen) {
          const uint8_t count = gbGetCheatCount();
          if (count > 0) {
            gbCheatSelectedRow = static_cast<int8_t>(
                (gbCheatSelectedRow + (direction > 0 ? 1 : count - 1)) % count);
            drawGbSettingsMenu();
          }
        } else {
          gbSettingsSelectedRow = static_cast<int8_t>(
              (gbSettingsSelectedRow + (direction > 0 ? 1 : 4)) % 5);
          drawGbSettingsMenu();
        }
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
        // Toast retire pour les encodeurs 2/3 sur la page PATCH (2026-09-24,
        // "il y a une barre en haut de l'ecran, il faut retirer... sur le 2
        // et 3 encodeur") : desormais dedies CUTOFF/RESONANCE et ADSR avec
        // leur propre retour visuel permanent (courbe sur l'oscilloscope,
        // indices en bas d'ecran) -- le toast faisait doublon et cachait
        // une partie de l'ecran par-dessus.
        const bool suppressToast = currentScreen == Screen::Patch && (index == 1 || index == 2);
        if (!screensaverActive && !suppressToast) {
          drawPotToast(index);
        }
      }
      // Encodeurs 1/2 CONTEXTUELS (2026-09-19, "on va leur attribuer une
      // couleur et les inclure a chaque fois dans l'application ... on
      // les utilise pas assez") -- l'encodeur 0/VOLUME garde son role
      // fixe cote Teensy (voir updateEncoders()), celui-ci ne fait rien
      // de plus ici. `slot` 0=encodeur1, 1=encodeur2 (voir
      // drawEncoderHints()). Valeur ABSOLUE 0-127 (position accumulee
      // cote Teensy, pas un delta) -- reaffecter un encodeur a un
      // nouveau parametre peut donc faire "sauter" sa valeur au premier
      // mouvement (pas de "soft takeover" en v1, simplicite demandee ce
      // soir pour les effets par piste, meme esprit ici).
      if (index == 1 || index == 2) {
        const uint8_t slot = static_cast<uint8_t>(index - 1);
        if (currentScreen == Screen::Audio && padMenuOpen) {
          // Navigation geree par TURN: relatif, jamais par POT: absolu.
        } else if (currentScreen == Screen::Mixer) {
          if (slot == 0) {
            const int track = (static_cast<int>(value) * kSeqTrackCount) / 128;
            selectedMixerTrack = static_cast<int8_t>(constrain(track, 0, kSeqTrackCount - 1));
            if (!screensaverActive) {
              drawMixerPage();
            }
          } else {
            const uint8_t t = static_cast<uint8_t>(selectedMixerTrack);
            trackVolume[t] = value;
            char msg[16];
            snprintf(msg, sizeof(msg), "VOL:%d:%d", t, value);
            sendToTeensy(msg);
            if (!screensaverActive) {
              drawMixerTrack(t);
            }
          }
        } else if (currentScreen == Screen::Patch && slot == 1) {
          // Encodeur 3 (2026-09-24, "il faut que l'encodeur le controle,
          // l'encodeur 3 ... et l'encodeur pour la valeur") : DEDIE a
          // l'ADSR sur la page PATCH, EN PERMANENCE -- plus besoin
          // d'entrer un focus particulier (voir patchOnScopeRow, la
          // courbe est de toute facon toujours affichee maintenant). Un
          // seul point actif a la fois (patchScopeAdsrPoint, avance par
          // clic du bouton-poussoir de CET encodeur, voir
          // handleTeensyLine() "ENCSW:"), le meme reglage sous-jacent que
          // les anciennes lignes 2-5 fixes.
          const uint8_t t = static_cast<uint8_t>(patchTrack);
          uint8_t *const points[4] = {&trackAttack[t], &trackDecay[t], &trackSustain[t], &trackRelease[t]};
          *points[patchScopeAdsrPoint] = value;
          sendPatchEnv();
          if (!screensaverActive) {
            drawPatchScope();
          }
        } else if (currentScreen == Screen::Patch) {
          // Encodeur 2 (2026-09-24, meme redesign qu'encodeur 3 ci-dessus) :
          // dedie CUTOFF/RESONANCE en permanence, remplace l'ancien systeme
          // par ligne (patchEncoderLogicalRow(), qui atterrissait sur
          // n'importe quelle ligne selectionnee -- "ça bouge le cutoff,
          // faut retirer ce truc"). VOLUME/SLOT/lignes extra restent
          // reglables via A maintenu + croix (patchApplyDelta()), juste
          // plus par encodeur.
          const uint8_t t = static_cast<uint8_t>(patchTrack);
          if (isRackTrack(t)) {
            // Pistes rack (GRANULAR/SPECTRAL) : lignes 0-5 sont de vrais
            // parametres distincts, pas CUTOFF/RESONANCE (voir
            // patchRowActive()) -- garde l'ancien systeme par ligne pour
            // elles, non concernees par ce redesign.
            const int8_t row = patchEncoderLogicalRow(t, slot);
            if (row >= 0 && row < 6) {
              const uint8_t maxVal = patchRowMax(t, static_cast<uint8_t>(row));
              uint8_t &param = patchParamRef(t, static_cast<uint8_t>(row));
              param = static_cast<uint8_t>((static_cast<uint32_t>(value) * maxVal) / 127);
              if (isRackOwner(t)) {
                char msg[48];
                snprintf(msg, sizeof(msg), "RACK_PARAM:%s:%d:%d",
                         az2::kRackEngineNames[rackEngineForTrack(t)], row, param);
                sendToTeensy(msg);
              }
              if (!screensaverActive) {
                drawPatchRow(static_cast<uint8_t>(row));
              }
            }
          } else {
            uint8_t *const points[2] = {&trackCutoff[t], &trackReso[t]};
            *points[patchScopeFilterPoint] = value;
            sendPatchFilt();
            if (!screensaverActive) {
              redrawPatchLogicalRow(t, patchScopeFilterPoint);
            }
          }
        } else if (currentScreen == Screen::Engines && !screensaverActive) {
          const uint8_t t = static_cast<uint8_t>(selectedEngineTrack);
          if (engineParamBank == 0) {
            if (slot == 0) trackCutoff[t] = value;
            else trackReso[t] = value;
            char msg[24];
            snprintf(msg, sizeof(msg), "FILT:%u:%u:%u", t, trackCutoff[t], trackReso[t]);
            sendToTeensy(msg);
          } else if (trackEngine[t] == az2::kEngineDexed) {
            if (slot == 0) trackAlgo[t] = static_cast<uint8_t>((static_cast<uint16_t>(value) * 32U) / 128U);
            else trackFeedback[t] = static_cast<uint8_t>((static_cast<uint16_t>(value) * 8U) / 128U);
            char msg[24];
            snprintf(msg, sizeof(msg), "DXP:%u:%u:%u", t, slot, slot == 0 ? trackAlgo[t] : trackFeedback[t]);
            sendToTeensy(msg);
          } else {
            if (slot == 0) trackAttack[t] = value;
            else trackDecay[t] = value;
            char msg[32];
            snprintf(msg, sizeof(msg), "ENV:%u:%u:%u:%u:%u", t, trackAttack[t], trackDecay[t], trackSustain[t], trackRelease[t]);
            sendToTeensy(msg);
          }
          drawEngVisualizer();
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
        if (pressed) encPressStartedMs[index] = millis();
        if (currentScreen == Screen::Controls && !screensaverActive) {
          drawPotBar(index);
        }
        if (currentScreen == Screen::Retro && gbIsLoaded()) {
          // Declenchement du menu d'options par l'appui simultane sur les
          // encodeurs 0 et 1 (1+2 est deja le combo de sortie ci-dessous).
          // L'un des deux est toujours detecte en premier (impossible
          // d'appuyer pile au meme instant) : son action individuelle
          // (Select, ou bascule REC sur l'encodeur 0) part donc avant que
          // le combo ne soit reconnu -- on l'annule explicitement a
          // l'ouverture pour eviter un Select emule bloque ou un
          // enregistrement audio declenche par erreur.
          if (encSwState[0] && encSwState[1]) {
            if (pressed) {
              gbSettingsMenuOpen = !gbSettingsMenuOpen;
              gbCheatMenuOpen = false;
              if (gbSettingsMenuOpen) {
                gbSetButton(GbButton::Select, false);
                gbSetButton(GbButton::Start, false);
                if (gbRecActive) {
                  sendToTeensy("REC:STOP");
                }
                drawGbSettingsMenu();
              } else {
                drawRetroPage();
                drawGbViewportFrame();
              }
            }
          } else if (!gbSettingsMenuOpen) {
            if (index == 1) {
              gbSetButton(GbButton::Select, pressed);
            } else if (index == 2) {
              gbSetButton(GbButton::Start, pressed);
            } else if (index == 0 && !pressed) {
              const bool longPress = (millis() - encPressStartedMs[index]) >= kEncoderLongPressMs;
              if (longPress) {
                sendToTeensy(gbRecActive ? "REC:STOP" : "REC:START");
              }
            }
            // Sortie volontaire et difficile a declencher par erreur:
            // START + SELECT maintenus simultanement.
            if ((index == 1 || index == 2) && encSwState[1] && encSwState[2]) {
              goTo(navPrevious);
            }
          } else if (gbSettingsMenuOpen) {
            // Demande 2026-09-25 : bouton poussoir de l'encodeur 2 valide
            // (comme la lettre A), celui de l'encodeur 3 sort du menu
            // (comme la lettre B) -- la molette de l'encodeur 2 navigue
            // (voir TURN: plus haut).
            if (pressed) {
              if (index == 1) {
                gbSettingsMenuConfirm();
              } else if (index == 2) {
                gbSettingsMenuBack();
              }
            }
          }
        }
#ifdef AZ2_NES_ENABLED
        if (currentScreen == Screen::NesRetro && nesIsLoaded()) {
          if (index == 1) {
            nesSetButton(NesButton::Select, pressed);
          } else if (index == 2) {
            nesSetButton(NesButton::Start, pressed);
          }
          // Meme sortie que GB : START + SELECT maintenus ensemble.
          if ((index == 1 || index == 2) && encSwState[1] && encSwState[2]) {
            nesSetButton(NesButton::Select, false);
            nesSetButton(NesButton::Start, false);
            nesUnload();
            goTo(navPrevious);
          }
        }
#endif
        if (currentScreen == Screen::NgpRetro && ngpIsLoaded()) {
          if (index == 1) {
            ngpSetButton(NgpButton::Select, pressed);
          } else if (index == 2) {
            ngpSetButton(NgpButton::Start, pressed);
          }
          if ((index == 1 || index == 2) && encSwState[1] && encSwState[2]) {
            ngpSetButton(NgpButton::Select, false);
            ngpSetButton(NgpButton::Start, false);
            ngpUnload();
            goTo(navPrevious);
          }
        }
        if (currentScreen == Screen::Audio && index == 1 && !pressed) {
          const bool longPress = (millis() - encPressStartedMs[index]) >= kEncoderLongPressMs;
          if (longPress) {
            if (padMenuOpen) {
              padMenuOpen = false;
              drawAudioPage();
            } else {
              goTo(Screen::Sequencer);
            }
          } else if (padMenuOpen) {
            activatePadMenuItem();
          } else {
            padMenuOpen = true;
            drawPadMenu();
          }
        }
        if (currentScreen == Screen::Engines && index == 1 && pressed) {
          engineParamBank = static_cast<uint8_t>(engineParamBank ^ 1U);
          drawEngVisualizer();
        }
        if (currentScreen == Screen::Patch && index == 1 && pressed && !isRackTrack(static_cast<uint8_t>(patchTrack))) {
          // Clic du bouton-poussoir de l'encodeur 2, DEDIE a CUTOFF/
          // RESONANCE en permanence sur cette page (2026-09-24, voir le
          // commentaire pres du gestionnaire POT: ci-dessus) : bascule
          // l'un vers l'autre. Pistes rack exclues (garde l'ancien systeme
          // par ligne, voir isRackTrack() dans le gestionnaire POT:).
          patchScopeFilterPoint = static_cast<uint8_t>(patchScopeFilterPoint ^ 1U);
          updatePatchEncoderHints();
        }
        if (currentScreen == Screen::Patch && index == 2 && pressed) {
          // Clic du bouton-poussoir de l'encodeur 3, DEDIE a l'ADSR en
          // permanence sur cette page (2026-09-24, voir le commentaire pres
          // du "slot == 1" dans le gestionnaire POT: ci-dessus) : avance au
          // point suivant ATTACK->DECAY->SUSTAIN->RELEASE->ATTACK...
          patchScopeAdsrPoint = static_cast<uint8_t>((patchScopeAdsrPoint + 1) % 4);
          drawPatchScope();
          updatePatchEncoderHints();
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
  } else if (line.startsWith("PLEN:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t pattern = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t measures = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (pattern < kPatternCount && measures >= 1 && measures <= kSeqMaxMeasures) {
        patternMeasures[pattern] = measures;
        if (pattern == currentPattern) {
          if (seqVisibleMeasure >= measures) seqVisibleMeasure = static_cast<uint8_t>(measures - 1);
          const uint8_t maxStep = static_cast<uint8_t>(measures * kSeqStepsPerMeasure - 1);
          if (selectedSeqStep > maxStep) selectedSeqStep = maxStep;
          if (currentScreen == Screen::Sequencer && !screensaverActive) drawSeqDetailPage();
        }
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
        const uint8_t previousEngine = trackEngine[track];
        trackEngine[track] = engine;
        if (currentScreen == Screen::Engines) {
          // Nouveau moteur = nombre de patches different -- repart du
          // haut de la liste PATCH plutot que de garder un defilement
          // qui ne correspondrait plus a rien (voir drawEngPatchRow()).
          if (track == selectedEngineTrack) {
            engPatchScroll = 0;
            drawEngListRow(previousEngine);
            drawEngListRow(engine);
            for (uint8_t row = 0; row < kEngListVisibleRows; ++row) drawEngPatchRow(row);
            drawEngVisualizer();
          }
        } else if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          // Les lignes 2-5 changent de sens selon le moteur (ADSR vs
          // ALGO/FEEDBACK Dexed, voir patchRowLabel()), ET le nombre de
          // lignes extra change aussi (2026-09-18, voir
          // patchExtraCount()) -- repart du haut plutot que de garder
          // une selection/un defilement qui ne correspondrait plus a
          // rien pour ce nouveau moteur, puis relit ses valeurs.
          selectedPatchRow = 0;
          patchScroll = 0;
          patchListScroll = 0;
          queryPatchExtra(track);
          drawPatchPage();
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        } else if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchList();
        }
      }
    }
  } else if (line.startsWith("RACK_OWNER:")) {
    // Echo de rackClaimOwnership() cote Teensy (voir son commentaire) --
    // "RACK_OWNER:GRANULAR:<piste>" ou "RACK_OWNER:SPECTRAL:<piste>".
    // Tient a jour rackOwnerTrack[] pour que sendPatchExtra()/
    // patchApplyDelta() sachent si CETTE piste a encore le droit
    // d'envoyer RACK_PARAM: en direct (isRackOwner()).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const String engineName = line.substring(i1 + 1, i2);
      const uint8_t track = static_cast<uint8_t>(line.substring(i2 + 1).toInt());
      if (track < kSeqTrackCount) {
        for (uint8_t slot = 0; slot < az2::kRackEngineCount; ++slot) {
          if (engineName == az2::kRackEngineNames[slot]) {
            rackOwnerTrack[slot] = static_cast<int8_t>(track);
            break;
          }
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
        loadRackPresetValues(track, patch);
        if (currentScreen == Screen::Engines) {
          // Fait suivre le defilement de la liste PATCH si besoin --
          // meme si ce changement vient d'ailleurs que cette page (ex:
          // colonne INST du tracker), la liste doit rester coherente
          // avec ce qui est reellement charge des qu'on y revient.
          if (track == selectedEngineTrack &&
              (patch < engPatchScroll || patch >= engPatchScroll + kEngListVisibleRows)) {
            engPatchScroll = (patch < kEngListVisibleRows) ? 0 : static_cast<uint16_t>(patch - kEngListVisibleRows + 1);
          }
          if (track == selectedEngineTrack) {
            for (uint8_t row = 0; row < kEngListVisibleRows; ++row) drawEngPatchRow(row);
            drawEngVisualizer();
          }
        } else if (currentScreen == Screen::Sequencer && track == selectedSeqTrack && !screensaverActive) {
          drawTrkSidePanel();
        } else if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          patchUiNeedsRedraw = true;
        }
      }
    }
  } else if (line.startsWith("SMODE:")) {
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    if (i1 >= 0 && i2 >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      const uint8_t mode = static_cast<uint8_t>(line.substring(i2 + 1).toInt()) > 0 ? 1 : 0;
      if (track < kSeqTrackCount) {
        trackSamplerGate[track] = mode;
        patchExtraVal[track][0] = mode;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive) {
          drawPatchExtraRow(6);
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
          // Lignes 2-5 desormais inactives (donc absentes de la mise en
          // page, voir patchRowActive()/patchKeptRows()) pour tout moteur
          // sauf DEXED -- les redessiner inconditionnellement ici
          // dessinait au mauvais endroit (2026-09-24, patchVisualRow()
          // retombe sur 0 pour une ligne absente de kept[]). L'ADSR vit
          // maintenant sur l'oscilloscope (drawPatchScope()) pour ces
          // moteurs-la.
          for (uint8_t row = 2; row < 6; ++row) {
            if (patchRowActive(track, row)) drawPatchRow(row);
          }
          drawPatchScope();
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
  } else if (line.startsWith("PADSAMPLE:") && line.endsWith(":CLEARED")) {
    const int pad = line.substring(10, line.indexOf(':', 10)).toInt();
    if (pad >= 0 && pad < az2::kPadCount) {
      padSamplePath[pad][0] = '\0';
      samplerNeedsRedraw = true;
    }
  } else if (line.startsWith("PADSAMPLE:") && line.indexOf(":READY:path=") > 0) {
    // Echo de l'assignation d'un sample a un pad (voir
    // loadWavIntoPadSampler() cote Teensy) -- garde padSamplePath[] a
    // jour meme si l'assignation vient d'ailleurs (kit de depart au
    // boot du Teensy, chargement de projet) : necessaire pour pouvoir
    // RE-sauvegarder cette assignation dans le projet global (demande
    // 2026-09-19, "il faut pouvoir aussi les sauvegarder dans le
    // projet global").
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int pathIdx = line.indexOf("path=");
    if (i1 >= 0 && i2 >= 0 && pathIdx >= 0) {
      const uint8_t pad = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      if (pad < az2::kPadCount) {
        const String path = line.substring(pathIdx + 5);
        path.toCharArray(padSamplePath[pad], sizeof(padSamplePath[pad]));
        snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Pad %u pret", pad);
        samplerNeedsRedraw = true;
      }
    }
  } else if (line.startsWith("TRACKSAMPLE:") && line.endsWith(":CLEARED")) {
    const int track = line.substring(12, line.indexOf(':', 12)).toInt();
    if (track >= 0 && track < kSeqTrackCount) {
      trackSamplePath[track][0] = '\0';
      samplerNeedsRedraw = true;
      if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive &&
          trackEngine[track] == az2::kEngineSampler) {
        drawPatchExtraRow(static_cast<uint8_t>(1 + 6));
      }
    }
  } else if (line.startsWith("TRACKSAMPLE:") && line.indexOf(":READY:path=") > 0) {
    // Meme principe que l'echo PADSAMPLE:...:READY: ci-dessus, pour le
    // sample CUSTOM d'une piste (2026-09-23) -- garde trackSamplePath[]
    // a jour quelle que soit l'origine (choix depuis cet ecran, ou
    // chargement de projet).
    const int i1 = line.indexOf(':');
    const int i2 = line.indexOf(':', i1 + 1);
    const int pathIdx = line.indexOf("path=");
    if (i1 >= 0 && i2 >= 0 && pathIdx >= 0) {
      const uint8_t track = static_cast<uint8_t>(line.substring(i1 + 1, i2).toInt());
      if (track < kSeqTrackCount) {
        const String path = line.substring(pathIdx + 5);
        path.toCharArray(trackSamplePath[track], sizeof(trackSamplePath[track]));
        snprintf(samplerUiStatus, sizeof(samplerUiStatus), "Piste %d prete", track + 1);
        samplerNeedsRedraw = true;
        if (currentScreen == Screen::Patch && track == patchTrack && !screensaverActive &&
            trackEngine[track] == az2::kEngineSampler) {
          drawPatchExtraRow(static_cast<uint8_t>(1 + 6));
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
    // Une capture cree un nouveau SAMPLE_xxx.wav sur la SD du Teensy.
    // Rafraichir le navigateur si l'utilisateur est deja dans le sampleur,
    // sinon le fichier n'apparait qu'apres une nouvelle navigation/requete.
    if (line.startsWith("REC:STOPPED:") && currentScreen == Screen::Sampler &&
        strcmp(samplerFolder, "/samples") == 0) {
      samplerOffset = 0;
      samplerSelectedRow = 0;
      requestSamplerList();
      drawSamplerPage();
    }
  }

  if (currentScreen == Screen::Links) {
    drawLinksPage();
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

// Un echo PATCH peut modifier jusqu'a 18 valeurs. Le redessin complet depuis
// handleTeensyLine() bloquait la vidange UART et retardait BTN:A:UP/NAV:, ce
// qui donnait l'impression qu'un bouton restait bloque puis repartait.
// `patchUiNeedsRedraw` est declare avec l'etat PATCH plus haut ; les echos sont
// consolides et dessines une seule fois depuis loop().

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

// Public trampoline required by nes_emulator.cpp. The UI helpers above live
// in the private translation-unit namespace; keep the emulator callback
// interface global without exposing the display object to the NES core.
void nesBlitBand(int bandIndex, const uint16_t *pixels) {
  nesBlitBandImpl(bandIndex, pixels);
}

namespace {

constexpr size_t kNgpFramePixels = 160U * 152U;

#if defined(AZ2_NGP_DUAL_CORE_BLIT) && defined(AZ2_DIRECT_PANEL)
QueueHandle_t ngpBlitReadyQueue = nullptr;
QueueHandle_t ngpBlitFreeQueue = nullptr;
TaskHandle_t ngpBlitTaskHandle = nullptr;
uint16_t *ngpBlitBuffers[2] = {nullptr, nullptr};
bool ngpBlitPipelineReady = false;

void ngpBlitConsumerTask(void *) {
  uint16_t *buffer = nullptr;
  for (;;) {
    if (xQueueReceive(ngpBlitReadyQueue, &buffer, portMAX_DELAY) != pdTRUE) continue;
    ngpRenderFrameOnCore(buffer);
    xQueueSend(ngpBlitFreeQueue, &buffer, portMAX_DELAY);
  }
}

bool ngpStartBlitPipeline() {
  if (ngpBlitPipelineReady) return true;
  ngpBlitReadyQueue = xQueueCreate(2, sizeof(uint16_t *));
  ngpBlitFreeQueue = xQueueCreate(2, sizeof(uint16_t *));
  if (!ngpBlitReadyQueue || !ngpBlitFreeQueue) return false;
  for (uint8_t i = 0; i < 2; ++i) {
    ngpBlitBuffers[i] = static_cast<uint16_t *>(
        heap_caps_malloc(kNgpFramePixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    if (!ngpBlitBuffers[i]) return false;
    xQueueSend(ngpBlitFreeQueue, &ngpBlitBuffers[i], 0);
  }
  if (xTaskCreatePinnedToCore(ngpBlitConsumerTask, "ngp_blit", 8192, nullptr, 2,
                              &ngpBlitTaskHandle, 0) != pdPASS) {
    return false;
  }
  ngpBlitPipelineReady = true;
  Serial.println("NGP:BLIT:CORE0:READY");
  return true;
}

#endif

}  // namespace

// RACE produit 160x152 RGB565. Le framebuffer RGB de l'AZ-2 est ecrit
// directement, avec les memes axes retournes que GB/NES. Le mapping precalcule
// evite les divisions dans les 218 880 pixels de chaque frame.
void ngpRenderFrameOnCore(const uint16_t *pixels) {
#ifdef AZ2_DIRECT_PANEL
  uint16_t *framebuffer = nullptr;
#else
  uint16_t *framebuffer = gfx->getFramebuffer();
#endif
#ifdef AZ2_DIRECT_PANEL
  if (!pixels) return;
#else
  if (!framebuffer || !pixels) return;
#endif
  // Ecriture par bandes courtes : le panneau RGB lit la framebuffer en DMA.
  // Une publication par frame complete provoque du tearing/tremblement ; 8
  // lignes source est le compromis deja valide pour GB/NES.
  constexpr int16_t kSourceRowsPerBand = 8;
  for (int16_t bandStart = 0; bandStart < 152; bandStart += kSourceRowsPerBand) {
#ifdef AZ2_DIRECT_PANEL
    // Reprendre le buffer arriere a chaque bande : le callback du panneau
    // peut changer le buffer libre pendant le calcul RACE.
    framebuffer = static_cast<uint16_t *>(directPanel.writableFrameBuffer());
    if (!framebuffer) return;
#endif
    const int16_t bandEnd = min<int16_t>(bandStart + kSourceRowsPerBand, 152);
    for (int16_t srcY = bandStart; srcY < bandEnd; ++srcY) {
    uint16_t *dst = framebuffer + static_cast<size_t>(12 + (151 - srcY) * 3) * kScreenSize;
    const uint16_t *src = pixels + static_cast<size_t>(srcY) * 160U;
    uint16_t *out = dst;
    for (int16_t srcX = 159; srcX >= 0; --srcX) {
      const uint16_t color = src[srcX];
      *out++ = color;
      *out++ = color;
      *out++ = color;
    }
    memcpy(dst + kScreenSize, dst, kScreenSize * sizeof(uint16_t));
    memcpy(dst + 2 * kScreenSize, dst, kScreenSize * sizeof(uint16_t));
    }
    const int16_t firstDstY = static_cast<int16_t>(12 + (151 - bandEnd) * 3 + 3);
    const int16_t outputRows = static_cast<int16_t>((bandEnd - bandStart) * 3);
    esp_cache_msync(framebuffer + firstDstY * kScreenSize,
                    static_cast<size_t>(outputRows) * kScreenSize * sizeof(uint16_t),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (((bandStart / kSourceRowsPerBand) & 3) == 3) yield();
  }
}

void ngpBlitFrame(const uint16_t *pixels) {
  if (!pixels) return;
#if defined(AZ2_NGP_DUAL_CORE_BLIT) && defined(AZ2_DIRECT_PANEL)
  if (ngpStartBlitPipeline()) {
    uint16_t *copy = nullptr;
    if (xQueueReceive(ngpBlitFreeQueue, &copy, 0) == pdTRUE) {
      memcpy(copy, pixels, kNgpFramePixels * sizeof(uint16_t));
      if (xQueueSend(ngpBlitReadyQueue, &copy, 0) == pdTRUE) return;
      xQueueSend(ngpBlitFreeQueue, &copy, 0);
      return;
    }
    // Le core 0 est encore en train de publier la frame precedente : on
    // laisse l'affichage stable et on saute cette frame plutot que bloquer
    // le coeur TLCS-900H.
    return;
  }
#endif
  ngpRenderFrameOnCore(pixels);
}

void ngpBlitWaitIdle() {
#if defined(AZ2_NGP_DUAL_CORE_BLIT) && defined(AZ2_DIRECT_PANEL)
  if (!ngpBlitPipelineReady) return;
  const uint32_t start = millis();
  while ((uxQueueMessagesWaiting(ngpBlitFreeQueue) < 2U) && (millis() - start < 100U)) {
    vTaskDelay(1);
  }
#endif
}

// Rendu Game Boy (voir gb_emulator.h/.cpp) : hors namespace anonyme pour
// avoir un lien externe (appelee depuis gb_emulator.cpp, autre unite de
// compilation) tout en gardant acces a `gfx`/`kScreenSize` (recherche de
// nom non qualifiee, valide pour le reste du fichier apres la fermeture
// du namespace). 160x144 -> mise a l'echelle x3 = 480x432, centree
// verticalement (24px de marge haut/bas).
constexpr int16_t kGbMaxScaledW = 160 * 3;
constexpr int16_t kGbMaxScaledRowsPerBand = 8 * 3;
int16_t gbScaledW() { return static_cast<int16_t>(160 * gbDisplayScale); }
int16_t gbScaledH() { return static_cast<int16_t>(144 * gbDisplayScale); }
int16_t gbScreenLeft() { return static_cast<int16_t>((kScreenSize - gbScaledW()) / 2); }
int16_t gbScreenTop() { return static_cast<int16_t>((kScreenSize - gbScaledH()) / 2); }

#ifdef AZ2_DIRECT_PANEL
bool uiCanvasFlushPending = false;
uint32_t uiCanvasLastFlushMs = 0;
void flushUiCanvas() { uiCanvasFlushPending = true; }
void serviceUiCanvas() {
  const uint32_t now = millis();
  if (!uiCanvasFlushPending || now - uiCanvasLastFlushMs < 40) return;
  directCanvas.flush();
  uiCanvasLastFlushMs = now;
  uiCanvasFlushPending = false;
}
#else
void flushUiCanvas() {}
void serviceUiCanvas() {}
#endif

void drawGbViewportFrame() {
  constexpr int16_t kFrame = 4;
  const int16_t x = static_cast<int16_t>(gbScreenLeft() - kFrame);
  const int16_t y = static_cast<int16_t>(gbScreenTop() - kFrame);
  const int16_t w = static_cast<int16_t>(gbScaledW() + 2 * kFrame);
  const int16_t h = static_cast<int16_t>(gbScaledH() + 2 * kFrame);
  gfx->drawRect(x, y, w, h, kPalette[2]);
  gfx->drawRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                static_cast<int16_t>(w - 2), static_cast<int16_t>(h - 2), kFaint);
#ifdef AZ2_DIRECT_PANEL
  flushUiCanvas();
#else
  if (uint16_t *framebuffer = gfx->getFramebuffer(); framebuffer != nullptr) {
    esp_cache_msync(framebuffer, static_cast<size_t>(kScreenSize * kScreenSize * sizeof(uint16_t)),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  }
#endif
}

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

#ifdef AZ2_GB_DUAL_CORE_BLIT
// [2026-09-27] Copie+flush (~16,4ms, voir docs/AZ2_MESURE_EMULATEUR_GB_*)
// deplaces sur une tache dediee au core 0 (libre : ce firmware n'utilise ni
// WiFi ni BT) pendant que le coeur GB + la mise a l'echelle restent sur le
// core 1 (le "loopTask" Arduino habituel). Double buffer + 2 files FreeRTOS
// (la synchro elle-meme assure la visibilite memoire entre coeurs, pas
// besoin de esp_cache_msync pour ca -- seulement pour la coherence avec le
// DMA du panneau, deja fait cote consommateur). gbBandFreeQueue est
// prerempli avec les 2 index au demarrage ; le producteur en prend un avant
// de remplir une bande (bloque si les 2 sont encore chez le consommateur --
// c'est le seul point de contre-pression, aucune donnee n'est ecrasee en
// cours de lecture).
// [2026-10-01] Le coeur 1 ne transmet plus des bandes deja agrandies
// (480 px x 24 lignes, 2 tampons seulement) mais les lignes SOURCE du coeur
// (160 px x 8 lignes) : 9 fois plus petites, donc une image entiere (18
// bandes) tient dans la meme RAM. Avant, avec 2 tampons, le coeur 1 attendait
// le coeur 0 a chaque bande des que l'affichage depassait la cadence : mesure
// Walnut X3, 15,2 ms de coeur en X2 -> 18,7 ms en X3, 47-50 fps. Desormais le
// coeur 0 fait l'agrandissement ET la copie, avec tout le temps d'une image
// affichee (Walnut n'en dessine qu'une sur deux) pour la finir.
constexpr int16_t kGbSourceRowsPerBand = 8;
constexpr uint8_t kGbBandPool = 18;  // 144 / 8 : une image GB complete
struct GbBandJob {
  uint8_t idx;          // tampon source dans gbSrcBandBuf
  int16_t sourceStart;  // premiere ligne GB (0-143) de la bande
  int16_t sourceRows;   // lignes valides dans la bande (8, ou moins en fin)
  uint8_t scale;        // X2/X3 fige a l'emission (peut changer entre 2 frames)
  bool lastOfFrame;
};
QueueHandle_t gbBandReadyQueue = nullptr;
QueueHandle_t gbBandFreeQueue = nullptr;
uint16_t gbSrcBandBuf[kGbBandPool][160 * kGbSourceRowsPerBand];
// [2026-09-27] Bug reel trouve sur Walnut-CGB (GBC) : gb_run_frame_dualfetch()
// saute __gb_draw_line()/gbBlitLine() entierement quand gb->lcd_blank est
// vrai (walnut_cgb.h, "if(!gb->lcd_blank) __gb_draw_line(gb);") -- si le LCD
// se coupe EN PLEIN MILIEU d'une bande (8 lignes), cette bande n'atteint
// jamais bandComplete (sourceRowInBand==7 ni line==143 ne sont revus), donc
// son buffer reste indefiniment "sorti" du pool. Apres 2 bandes abandonnees
// (2 buffers), gbBandFreeQueue est vide et le PRODUCTEUR bloque a jamais au
// prochain sourceRowInBand==0 (xQueueReceive portMAX_DELAY) -- gele loop()
// entier (boutons, tactile, heartbeat serie inclus, confirme sur materiel).
// currentBandBufIdx/checkedOut sortis de gbBlitLine() (etaient des static
// locales) pour etre lisibles depuis gbBlitEndOfFrame(), appelee une fois
// par frame par gbRunFrame() (gb_emulator.cpp ET gb_emulator_peanut.cpp),
// DONC toujours executee meme si gbBlitLine() n'a pas tourne cette frame.
// 0 (pas 0xFF) : index tableau valide des le premier appel, meme si aucun
// xQueueReceive n'a encore reussi -- evite un acces hors bornes si jamais
// gbBlitLine() lit gCurrentBandBufIdx avant sa premiere ecriture reelle.
uint8_t gCurrentBandBufIdx = 0;
bool gGbBandCheckedOut = false;

// [2026-09-27] Toutes les attentes de ce pipeline (ici et dans gbBlitLine())
// sont bornees, PAS portMAX_DELAY -- le blanking LCD mi-bande (voir plus
// haut) n'a pas suffi a expliquer un blocage reproduit une 2e fois apres ce
// premier correctif : cause exacte encore incertaine (peut-etre le
// consommateur lui-meme qui cale sur ce coeur GBC). Plutot que de
// continuer a deviner scenario par scenario, une attente bornee garantit
// que loop() (tactile/boutons/heartbeat) ne peut plus JAMAIS se figer
// indefiniment, quelle que soit la cause reelle -- au pire l'image GB se
// degrade/gele, le reste de l'appareil reste reactif.
constexpr TickType_t kGbBandWaitTicks = pdMS_TO_TICKS(50);

// Rend au pool un buffer sorti mais jamais soumis cette frame (bande
// abandonnee par un blanking LCD en cours de route) -- ne l'affiche pas
// (donnees partielles), se contente de restaurer l'invariant du pool.
// No-op si la derniere bande commencee a normalement atteint bandComplete.
void gbBlitEndOfFrame() {
  if (gGbBandCheckedOut) {
    xQueueSend(gbBandFreeQueue, &gCurrentBandBufIdx, kGbBandWaitTicks);
    gGbBandCheckedOut = false;
  }
}

void gbBlitConsumerTask(void *) {
  extern volatile uint32_t gGbDisplayLastUs;
  extern volatile uint32_t gGbBlitScaleLastUs;
  extern volatile uint32_t gGbBlitCopyLastUs;
  extern volatile uint32_t gGbBlitFlushLastUs;
  extern volatile bool gGbDisplayHappenedThisFrame;
  static uint32_t consumerDisplayUs = 0;
  static uint32_t consumerScaleUs = 0;
  static uint32_t consumerCopyUs = 0;
  static uint32_t consumerFlushUs = 0;
  GbBandJob job;
  for (;;) {
    if (xQueueReceive(gbBandReadyQueue, &job, portMAX_DELAY) != pdTRUE) continue;
    const uint32_t drawStartUs = micros();
    const int16_t scale = job.scale;
    const int16_t scaledW = static_cast<int16_t>(160 * scale);
    const int16_t screenLeft = static_cast<int16_t>((kScreenSize - scaledW) / 2);
    const int16_t scaledTop = static_cast<int16_t>((kScreenSize - 144 * scale) / 2);
    const int16_t y = static_cast<int16_t>(scaledTop + job.sourceStart * scale);
    const int16_t outputRows = static_cast<int16_t>(job.sourceRows * scale);
    uint16_t *framebuffer = gfx->getFramebuffer();
    // Ligne agrandie preparee en RAM interne puis recopiee par memcpy
    // contigu en PSRAM (plus rapide que des ecritures PSRAM pixel a pixel,
    // voir AZ2_AUDIT_PILOTE_RGB_2026-09-20.md).
    static uint16_t scaledRow[kGbMaxScaledW];
    uint32_t scaleUs = 0;
    const uint32_t copyStartUs = micros();
    if (framebuffer != nullptr) {
      for (int16_t r = 0; r < job.sourceRows; ++r) {
        const uint32_t scaleStartUs = micros();
        const uint16_t *src = gbSrcBandBuf[job.idx] + r * 160;
        uint16_t *dst = scaledRow;
        // Panneau monte tete-en-bas : x inverse ici, y inverse a la copie.
        for (int x = 159; x >= 0; --x) {
          const uint16_t c = src[x];
          for (int16_t k = 0; k < scale; ++k) *dst++ = c;
        }
        scaleUs += micros() - scaleStartUs;
        for (int16_t dy = 0; dy < scale; ++dy) {
          const int16_t dstY = static_cast<int16_t>(kScreenSize - 1 - (y + r * scale + dy));
          memcpy(framebuffer + dstY * kScreenSize + screenLeft, scaledRow,
                 scaledW * sizeof(uint16_t));
        }
      }
    }
    consumerCopyUs += micros() - copyStartUs - scaleUs;
    consumerScaleUs += scaleUs;
    const uint32_t flushStartUs = micros();
    if (framebuffer != nullptr) {
      const int16_t firstDstY = static_cast<int16_t>(kScreenSize - 1 - (y + outputRows - 1));
      esp_cache_msync(framebuffer + firstDstY * kScreenSize,
                      static_cast<size_t>(outputRows) * kScreenSize * sizeof(uint16_t),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
    consumerFlushUs += micros() - flushStartUs;
    consumerDisplayUs += micros() - drawStartUs;
    xQueueSend(gbBandFreeQueue, &job.idx, kGbBandWaitTicks);
    if (job.lastOfFrame) {
      gGbBlitScaleLastUs = consumerScaleUs;
      gGbBlitCopyLastUs = consumerCopyUs;
      gGbBlitFlushLastUs = consumerFlushUs;
      gGbDisplayLastUs = consumerDisplayUs;
      gGbDisplayHappenedThisFrame = true;
      consumerScaleUs = 0;
      consumerDisplayUs = 0;
      consumerCopyUs = 0;
      consumerFlushUs = 0;
    }
  }
}

void gbBlitDualCoreInit() {
  if (gbBandReadyQueue != nullptr) return;
  gbBandReadyQueue = xQueueCreate(kGbBandPool, sizeof(GbBandJob));
  gbBandFreeQueue = xQueueCreate(kGbBandPool, sizeof(uint8_t));
  for (uint8_t i = 0; i < kGbBandPool; ++i) {
    xQueueSend(gbBandFreeQueue, &i, 0);
  }
  // [2026-09-27] 4096 -> 8192 par prudence : la carte a fige/perdu l'USB
  // apres une session de jeu avec ce prototype, cause pas encore identifiee
  // avec certitude -- pile un peu large ecarte cette hypothese a peu de
  // frais avant de chercher ailleurs.
  xTaskCreatePinnedToCore(gbBlitConsumerTask, "gbBlit", 8192, nullptr, 1, nullptr, 0);
}
#else
// Stub : gbRunFrame() (gb_emulator.cpp ET gb_emulator_peanut.cpp) l'appelle
// inconditionnellement une fois par frame, meme sur les firmwares qui ne
// definissent pas AZ2_GB_DUAL_CORE_BLIT (pas de pool de buffers a liberer).
void gbBlitEndOfFrame() {}
#endif

void gbBlitLine(int line, const uint16_t *row) {
#ifdef AZ2_GB_DUAL_CORE_BLIT
  // Coeur 1 : simple copie de la ligne source (320 o) dans le tampon de la
  // bande courante, envoi au coeur 0 en fin de bande. Agrandissement, copie
  // framebuffer et writeback cache : gbBlitConsumerTask() sur le coeur 0.
  const int16_t rowInBand = static_cast<int16_t>(line % kGbSourceRowsPerBand);
  if (rowInBand == 0) {
    // Attente BORNEE : le pool couvre une image entiere, on n'attend que si
    // le coeur 0 a plus d'une image de retard.
    if (xQueueReceive(gbBandFreeQueue, &gCurrentBandBufIdx, kGbBandWaitTicks) == pdTRUE) {
      gGbBandCheckedOut = true;
    }
  }
  memcpy(gbSrcBandBuf[gCurrentBandBufIdx] + rowInBand * 160, row, 160 * sizeof(uint16_t));
  if (rowInBand == kGbSourceRowsPerBand - 1 || line == 143) {
    GbBandJob job{gCurrentBandBufIdx, static_cast<int16_t>(line - rowInBand),
                  static_cast<int16_t>(rowInBand + 1), static_cast<uint8_t>(gbDisplayScale),
                  line == 143};
    if (xQueueSend(gbBandReadyQueue, &job, kGbBandWaitTicks) != pdTRUE) {
      // Coeur 0 bloque : rendre le tampon plutot que de le perdre (bande
      // non affichee, mais le pool reste complet).
      xQueueSend(gbBandFreeQueue, &gCurrentBandBufIdx, 0);
    }
    gGbBandCheckedOut = false;
  }
  return;
#else
  // [2026-09-25] Essaye a 16 (bandes deux fois plus grandes, 9 flushs/frame
  // au lieu de 18) pour reduire le cout esp_cache_msync() -- confirme
  // scintillant sur materiel reel, comme le regroupement total deja
  // essaye et annule plus haut. Revenu a 8 : ce pipeline semble sensible
  // a la frequence de synchronisation avec le scan DMA de l'ecran, pas
  // seulement a son cout CPU total.
  constexpr int16_t kGbSourceRowsPerBand = 8;
  const int16_t scale = gbDisplayScale;
  const int16_t scaledW = gbScaledW();
  const int16_t scaledTop = gbScreenTop();
  const int16_t screenLeft = gbScreenLeft();
#ifndef AZ2_GB_DUAL_CORE_BLIT
  static uint16_t scaledBand[kGbMaxScaledW * kGbMaxScaledRowsPerBand];
  static uint32_t copyFrameUs = 0;
  static uint32_t flushFrameUs = 0;
#endif
  static uint32_t displayFrameUs = 0;
  static uint32_t scaleFrameUs = 0;
  const int16_t sourceRowInBand = static_cast<int16_t>(line % kGbSourceRowsPerBand);
#ifdef AZ2_GB_DUAL_CORE_BLIT
  if (sourceRowInBand == 0) {
    // Nouvelle bande : recupere un buffer libre aupres du consommateur.
    // Attente BORNEE (kGbBandWaitTicks, pas portMAX_DELAY) -- voir le
    // commentaire pres de sa declaration : si le consommateur ne repond
    // pas a temps (cause exacte encore incertaine sur le coeur GBC),
    // continuer avec l'index precedent degrade juste cette bande au lieu
    // de figer loop() entier. gGbBandCheckedOut=true tant que bandComplete
    // n'a pas soumis cette bande -- voir gbBlitEndOfFrame() pour le filet
    // de securite si un blanking LCD saute la fin de bande avant ca.
    if (xQueueReceive(gbBandFreeQueue, &gCurrentBandBufIdx, kGbBandWaitTicks) == pdTRUE) {
      gGbBandCheckedOut = true;
    }
  }
  uint16_t *scaledBand = gbBandBuf[gCurrentBandBufIdx];
#endif
  uint16_t *scaledRow = scaledBand + sourceRowInBand * scale * scaledW;

  const uint32_t scaleStartUs = micros();
  // Ecriture sequentielle des lignes deja inversees : elle evite le calcul
  // d'offset et la boucle interne pour les deux modes reels X2/X3.
  if (scale == 3) {
    uint16_t *dst = scaledRow;
    for (int x =
#ifdef AZ2_DIRECT_PANEL
             0;
#else
             159;
#endif
#ifdef AZ2_DIRECT_PANEL
         x < 160;
         ++x) {
#else
         x >= 0;
         --x) {
#endif
      const uint16_t c = row[x];
      *dst++ = c;
      *dst++ = c;
      *dst++ = c;
    }
  } else if (scale == 2) {
    uint16_t *dst = scaledRow;
    for (int x =
#ifdef AZ2_DIRECT_PANEL
             0;
#else
             159;
#endif
#ifdef AZ2_DIRECT_PANEL
         x < 160;
         ++x) {
#else
         x >= 0;
         --x) {
#endif
      const uint16_t c = row[x];
      *dst++ = c;
      *dst++ = c;
    }
  } else {
    for (int x = 0; x < 160; ++x) {
      scaledRow[159 - x] = row[x];
    }
  }
  // Les 2 autres rangees de sortie sont identiques a la premiere.
  for (int16_t dy = 1; dy < scale; ++dy) {
    memcpy(scaledRow + scaledW * dy, scaledRow, scaledW * sizeof(uint16_t));
  }
  scaleFrameUs += micros() - scaleStartUs;

  const bool bandComplete = sourceRowInBand == (kGbSourceRowsPerBand - 1) || line == 143;
  if (bandComplete) {
    const int16_t sourceBandStart = static_cast<int16_t>(line - sourceRowInBand);
    const int16_t sourceRows = static_cast<int16_t>(sourceRowInBand + 1);
    const int16_t y = static_cast<int16_t>(scaledTop + sourceBandStart * scale);
    const int16_t outputRows = static_cast<int16_t>(sourceRows * scale);
#ifdef AZ2_GB_DUAL_CORE_BLIT
    // Copie+flush faits par gbBlitConsumerTask() sur le core 0 -- on empile
    // juste le travail ici. currentBandBufIdx sera reacquis au debut de la
    // prochaine bande (sourceRowInBand==0 plus haut), jamais reecrit avant
    // que le consommateur ait rendu ce meme index via gbBandFreeQueue.
    GbBandJob job{scaledBand, y, outputRows, scaledW, screenLeft, line == 143};
    if (xQueueSend(gbBandReadyQueue, &job, kGbBandWaitTicks) == pdTRUE) {
      gGbBandCheckedOut = false;
    } else {
      // Consommateur trop en retard pour accepter cette bande a temps :
      // rendre le buffer directement (best effort, bande non affichee)
      // plutot que de le laisser "sorti" jusqu'a gbBlitEndOfFrame() -- une
      // bande suivante DANS LA MEME frame acquerrait sinon un nouvel index
      // sans jamais rendre celui-ci (fuite silencieuse du pool).
      xQueueSend(gbBandFreeQueue, &gCurrentBandBufIdx, kGbBandWaitTicks);
      gGbBandCheckedOut = false;
    }
    if (line == 143) {
      extern volatile uint32_t gGbBlitScaleLastUs;
      gGbBlitScaleLastUs = scaleFrameUs;
      scaleFrameUs = 0;
    }
#else
    const uint32_t drawStartUs = micros();
    // Le chemin générique Arduino_GFX refait les contrôles de clipping et la
    // rotation 180 degrés pour chaque bande. Le panneau RGB expose déjà son
    // framebuffer PSRAM : on écrit directement les lignes inversées, puis on
    // invalide uniquement leur plage de cache. Le rendu reste identique,
    // mais évite une copie intermédiaire et ses pics de latence.
    const uint32_t copyStartUs = micros();
#ifdef AZ2_DIRECT_PANEL
    directPanel.copyRotatedRgb565(scaledBand, screenLeft, y, scaledW, outputRows);
#else
    uint16_t *framebuffer = gfx->getFramebuffer();
    if (framebuffer != nullptr) {
      for (int16_t srcY = 0; srcY < outputRows; ++srcY) {
        const int16_t dstY = static_cast<int16_t>(kScreenSize - 1 - (y + srcY));
        uint16_t *dst = framebuffer + dstY * kScreenSize + screenLeft;
        const uint16_t *src = scaledBand + srcY * scaledW;
        memcpy(dst, src, scaledW * sizeof(uint16_t));
      }
    } else {
      // Sécurité pour un autre pilote d’écran qui ne fournirait pas de
      // framebuffer accessible.
      gfx->draw16bitRGBBitmap(screenLeft, y, scaledBand, scaledW, outputRows);
    }
#endif
    copyFrameUs += micros() - copyStartUs;
    const uint32_t flushStartUs = micros();
#ifndef AZ2_DIRECT_PANEL
    // [2026-09-25] Regroupement en un seul flush par frame essaye puis
    // annule : sur materiel reel, resultat mesure PIRE (plus lent ET
    // scintillement visible), pas meilleur comme le suggerait l'analyse a
    // froid dans AZ2_MESURE_EMULATEUR_GB_2026-09-24.md §9 -- un seul gros
    // flush tardif a plus de chances de tomber en pleine lecture DMA de
    // l'ecran qu'une serie de petits flushs frequents. Revenu au flush
    // par bande. Voir doc pour le detail.
    if (framebuffer != nullptr) {
      const int16_t firstDstY = static_cast<int16_t>(kScreenSize - 1 - (y + outputRows - 1));
      esp_cache_msync(framebuffer + firstDstY * kScreenSize,
                      static_cast<size_t>(outputRows * kScreenSize * sizeof(uint16_t)),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif
    flushFrameUs += micros() - flushStartUs;
    displayFrameUs += micros() - drawStartUs;
    if (line == 143) {
      extern volatile uint32_t gGbDisplayLastUs;
      extern volatile uint32_t gGbBlitScaleLastUs;
      extern volatile uint32_t gGbBlitCopyLastUs;
      extern volatile uint32_t gGbBlitFlushLastUs;
      extern volatile bool gGbDisplayHappenedThisFrame;
      gGbDisplayLastUs = displayFrameUs;
      gGbBlitScaleLastUs = scaleFrameUs;
      gGbBlitCopyLastUs = copyFrameUs;
      gGbBlitFlushLastUs = flushFrameUs;
      gGbDisplayHappenedThisFrame = true;
      displayFrameUs = 0;
      scaleFrameUs = 0;
      copyFrameUs = 0;
      flushFrameUs = 0;
    }
#endif
  }
#endif  // AZ2_GB_DUAL_CORE_BLIT
}

volatile uint32_t gGbDisplayLastUs = 0;
volatile uint32_t gGbBlitScaleLastUs = 0;
volatile uint32_t gGbBlitCopyLastUs = 0;
volatile uint32_t gGbBlitFlushLastUs = 0;
// [2026-09-25] Instrumentation : distingue le temps CPU+PPU pur du temps
// d'affichage a l'INTERIEUR de core_avg_us (gbBlitLine() est appelee de
// maniere synchrone DANS gb_run_frame(), donc son cout est deja inclus
// dans le chrono brut autour de gb_run_frame() -- ce booleen dit si un
// rendu a reellement eu lieu cette frame precise, pour permettre de
// soustraire son cout et isoler le cœur pur (voir gbRunFrame()).
volatile bool gGbDisplayHappenedThisFrame = false;

// PSRAM d'abord, RAM interne en repli : la liste doit exister quoi qu'il
// arrive, les pages JEUX/NES/NGP l'indexent sans test.
template <size_t N>
char (*allocRomNameList(size_t rows))[N] {
  const size_t bytes = rows * N;
  void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) p = calloc(1, bytes);
  return static_cast<char (*)[N]>(p);
}

void allocRomNameLists() {
  gbRomNames = allocRomNameList<kGbRomNameLen>(kGbMaxRoms);
#ifdef AZ2_NES_ENABLED
  nesRomNames = allocRomNameList<kNesRomNameLen>(kNesMaxRoms);
#endif
  ngpRomNames = allocRomNameList<kNgpRomNameLen>(kNgpMaxRoms);
}

void setup() {
  allocRomNameLists();
  // Tampon d'envoi pour le port de debug (2026-10-01). Sans lui, chaque
  // Serial.print() attend que la FIFO materielle de 128 o se vide : la
  // ligne GB:PERF (~250 o a 230400 bauds) bloquait loop() ~11 ms une fois
  // par seconde, soit une image GB perdue chaque seconde ("pause toutes les
  // secondes" en GBC). Doit etre appele AVANT begin().
  Serial.setTxBufferSize(2048);
  Serial.begin(230400);
  delay(300);
  Serial.println("AZ2:ROLE:ESP32_SCREEN_TEST");
#ifdef AZ2_GB_DUAL_CORE_BLIT
  // La tache peut demarrer avant gfx : elle reste bloquee sur
  // gbBandReadyQueue (vide) jusqu'au premier gbBlitLine() d'une vraie
  // partie, bien apres l'init ecran plus bas dans ce setup().
  gbBlitDualCoreInit();
#endif
#ifdef AZ2_NES_DUAL_CORE
  nesInitDualCore();
#endif
  restoreSaverSettings();

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
  // Tampon d'envoi (2026-10-01) : chaque frame d'emulateur envoie un paquet
  // audio de 236 o au Teensy ; sans tampon, Serial1.write() attendait la
  // FIFO materielle de 128 o (~1,2 ms bloquees par frame, mesure
  // audio_avg_us ~1,9 ms sur Walnut). 4 paquets de marge.
  Serial1.setTxBufferSize(1024);
  Serial1.begin(az2::kControlBaud, SERIAL_8N1, kTeensyRxPin, kTeensyTxPin);
  gbSetAudioV2Ready(false);
  sendToTeensy(az2::kHelloControl);
  sendToTeensy("PADSAMPLE?");
  if (az2::kGbAudioV2PilotEnabled) {
    sendToTeensy(az2::kGbAudioV2Query);
  }

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
  } else if (currentScreen == Screen::Sampler && hitBack(x, y)) {
    samplerGoUp();
  } else if (currentScreen != Screen::Menu && currentScreen != Screen::Audio && hitBack(x, y)) {
    if (currentScreen == Screen::Patch) goTo(patchReturnScreen);
    else if (currentScreen == Screen::Engines) goTo(enginesReturnScreen);
    else if (currentScreen == Screen::Retro) goTo(Screen::EmuPicker);
    else if (currentScreen == Screen::NgpRetro) goTo(Screen::EmuPicker);
    else goTo(Screen::Menu);
  } else if (currentScreen == Screen::Menu) {
    if (menuCategory < 0) {
      const int8_t hit = hitTestCategoryCard(x, y);
      if (hit >= 0) {
        enterMenuCategory(static_cast<uint8_t>(hit));
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
        // Meme reset qu'au clavier physique (voir l'autre site
        // goTo(kMenuItems[...].target) plus haut) -- AUDIO depuis le
        // menu general repart sur la voix live generique.
        if (kMenuItems[items[hit]].target == Screen::Audio) {
          padTargetTrack = selectedSeqTrack;
          padEditsStep = false;
        }
        menuReturnCategory = menuCategory;
        goTo(kMenuItems[items[hit]].target);
      }
    }
  } else if (currentScreen == Screen::Audio) {
    if (padMenuOpen) {
      return;
    }
    const int8_t pad = hitTestAudioPad(x, y);
    if (pad >= 0 && pad != heldAudioPad[0] && pad != heldAudioPad[1]) {
      heldAudioPad[slot] = pad;
      drawAudioCell(static_cast<uint8_t>(pad), true);
      if (padEditsStep) {
        // "Poser" la note sur le pas selectionne (voir padEditsStep) --
        // allume aussi le pas (STEP: ON), sinon la note posee ne
        // s'entendrait jamais en lecture.
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
      }
      // Joue EN PLUS le son en direct (2026-09-19, "joue l'instrument
      // de la piste" -- avant, poser une note en mode padEditsStep
      // restait totalement silencieux, on n'entendait jamais ce qu'on
      // venait de programmer). track= present -> le vrai moteur/patch
      // de la piste (voir handlePadCommand() cote Teensy) ; absent ->
      // voix live Dexed generique, comportement d'origine.
      {
        char msg[32];
        if (padTargetTrack >= 0) {
          snprintf(msg, sizeof(msg), "PAD:%02d:DOWN:vel=100:track=%d", pad, padTargetTrack);
        } else {
          snprintf(msg, sizeof(msg), "PAD:%02d:DOWN:vel=100", pad);
        }
        sendToTeensy(msg);
      }
      if (seqRecording && padEditsStep && padTargetTrack >= 0) {
        const uint8_t activeSteps = static_cast<uint8_t>(patternMeasures[currentPattern] * kSeqStepsPerMeasure);
        selectedSeqStep = static_cast<int8_t>((selectedSeqStep + 1) % activeSteps);
        seqVisibleMeasure = static_cast<uint8_t>(selectedSeqStep / kSeqStepsPerMeasure);
      }
    }
  } else if (currentScreen == Screen::Sampler) {
    // KIT sauver/charger n'existe que pour les pads (2026-09-23, voir le
    // commentaire de drawSamplerPage()) -- le bouton n'est plus dessine en
    // mode piste, mais garde quand meme le tap ici pour ne rien declencher
    // sur une zone d'ecran devenue vide.
    if (!samplerTargetIsTrack && inBox(x, y, 20, 408, 145, 38)) {
      samplerKitSlot = static_cast<uint8_t>((samplerKitSlot + 1) % 4);
      drawSamplerPage();
    } else if (!samplerTargetIsTrack && inBox(x, y, 170, 408, 135, 38)) {
      saveSamplerKit();
    } else if (!samplerTargetIsTrack && inBox(x, y, 310, 408, 150, 38)) {
      loadSamplerKit();
    } else if (inBox(x, y, 20, 354, 95, 42)) {
      samplerOffset = samplerOffset >= kSamplerRows ? samplerOffset - kSamplerRows : 0;
      samplerSelectedRow = 0;
      requestSamplerList();
      drawSamplerPage();
    } else if (inBox(x, y, 123, 354, 95, 42)) {
      if (samplerOffset + kSamplerRows < samplerTotal) samplerOffset += kSamplerRows;
      samplerSelectedRow = 0;
      requestSamplerList();
      drawSamplerPage();
    } else if (inBox(x, y, 240, 354, 220, 42)) {
      if (samplerIsDir[samplerSelectedRow]) samplerOpenSelected();
      else samplerAssignSelected();
    } else if (!samplerTargetIsTrack && inBox(x, y, 20, 96, 191, 191)) {
      const int col = (x - 20) / 49;
      const int row = (y - 96) / 49;
      if (col < 4 && row < 4 && (x - 20) % 49 < 44 && (y - 96) % 49 < 44) {
        samplerSelectedPad = static_cast<uint8_t>(row * 4 + col);
        char msg[32];
        snprintf(msg, sizeof(msg), "PAD:%02u:DOWN:vel=100", samplerSelectedPad);
        sendToTeensy(msg);
        heldAudioPad[slot] = samplerSelectedPad;
        drawSamplerPage();
      }
    } else if (inBox(x, y, 240, 96, 220, 254)) {
      const int row = (y - 96) / 43;
      if (row < kSamplerRows && samplerFiles[row][0]) {
        samplerSelectedRow = static_cast<uint8_t>(row);
        if (samplerIsDir[row]) samplerOpenSelected();
        else {
          heldSamplerFile[slot] = static_cast<int8_t>(row);
          drawSamplerPage();
        }
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
      seqVisibleMeasure = 0;
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
      // padTargetTrack = piste actuelle (2026-09-19, suite : "joue
      // l'instrument de la piste") -- les pads sonnent desormais avec
      // le vrai moteur/patch de CETTE piste, plus la voix live Dexed
      // fixe generique.
      padEditsStep = true;
      padTargetTrack = selectedSeqTrack;
      goTo(Screen::Audio);
    } else if (hitTestTrkSideBtn(4, x, y)) {
      // METRONOME (2026-09-19) -- pas d'affichage optimiste, attend
      // l'echo METRO: confirme (meme convention que le reste de cette
      // page).
      char msg[12];
      snprintf(msg, sizeof(msg), "METRO:%d", metronomeOn ? 0 : 1);
      sendToTeensy(msg);
    } else if (hitTestTrkSideBtn(5, x, y)) {
      // SAUVER (2026-09-19, "dans le tracker il manque le bouton
      // sauvegarder") -- meme emplacement PROJET que la page dediee
      // (projectSlot), sauve tout le morceau sans quitter le tracker.
      // Flash bref en vert plein comme seul retour visuel (voir
      // drawTrkSideBtn(), pas de toast dedie pour l'instant -- la page
      // PROJET elle-meme n'en a pas non plus, coherent).
      drawTrkSideBtn(5, RGB565(60, 200, 90), true, "SAUVER");
      saveProject(projectSlot);
      drawTrkSideBtn(5, RGB565(60, 200, 90), false, "SAUVER");
    } else if (hitTestTrkRec(x, y)) {
      seqRecording = !seqRecording;
      drawTrkControls();
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
    } else if (hitTestTrkLength(x, y)) {
      const uint8_t measures = static_cast<uint8_t>((patternMeasures[currentPattern] % kSeqMaxMeasures) + 1);
      char msg[20];
      snprintf(msg, sizeof(msg), "PLEN:%u:%u", currentPattern, measures);
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
      patchOnPresetList = false;
      patchPresetEditing = false;
      char msg[12];
      snprintf(msg, sizeof(msg), "SCOPE:%d", patchTrack);
      sendToTeensy(msg);
      queryPatchExtra(static_cast<uint8_t>(patchTrack));
      sendPatchFilt();
      sendPatchEnv();
      drawPatchPage();
    } else {
      const int16_t patchHit = hitTestPatchList(x, y);
      const int8_t row = hitTestPatchParam(x, y);
      if (patchHit >= 0) {
        patchOnPresetList = true;
        patchOnTrackRow = false;
        patchPresetEditing = false;
        char msg[16];
        snprintf(msg, sizeof(msg), "PATCH:%u:%u", t, static_cast<unsigned>(patchHit));
        sendToTeensy(msg);
        drawPatchTrackRow();
        drawPatchList();
      } else if (row >= 0 && (row >= 6 || patchRowActive(t, static_cast<uint8_t>(row)))) {
        const int8_t prevRow = selectedPatchRow;
        selectedPatchRow = row;
        const bool leavingTrackRow = patchOnTrackRow;
        patchOnTrackRow = false;
        patchOnPresetList = false;
        patchPresetEditing = false;
        drawPatchList();
        if (leavingTrackRow) {
          drawPatchTrackRow();
        }
        redrawPatchLogicalRow(t, static_cast<uint8_t>(prevRow));
        redrawPatchLogicalRow(t, static_cast<uint8_t>(selectedPatchRow));
        updatePatchEncoderHints();
      } else if (hitTestPatchSlotNum(x, y)) {
        patchSlot = static_cast<uint8_t>((patchSlot + 1) % kPatchSlotCount);
        drawPatchSlotRow();
      } else if (hitTestPatchSlotSave(x, y)) {
        savePatchSlot(patchSlot);
      } else if (hitTestPatchSlotLoad(x, y)) {
        loadPatchSlot(patchSlot);
      }
    }
  } else if (currentScreen == Screen::Mixer) {
    // Tap = SELECTIONNE seulement (meme convention que partout ce soir)
    // -- l'edition du volume se fait ensuite a la croix (HAUT/BAS) ou a
    // l'encodeur 2.
    uint8_t track;
    if (hitTestMixerTrack(x, y, track)) {
      selectedMixerTrack = static_cast<int8_t>(track);
      drawMixerPage();
    } else if (hitTestMixerMute(x, y)) {
      // Boutons MUTE/SOLO tactiles (2026-09-19, "il faut que les 2
      // controles soit[ent] boutons tactil[es]") -- meme action que
      // B/D physiques, voir toggleMixerMuteSolo().
      toggleMixerMuteSolo(true);
    } else if (hitTestMixerSolo(x, y)) {
      toggleMixerMuteSolo(false);
    }
  } else if (currentScreen == Screen::StepSeq) {
    // [2026-09-26] Demande : "tout rendre tactile" -- toucher un onglet
    // change de piste, toucher une case bascule le pas ET y deplace le
    // curseur (memes messages STEP:/etat que le clavier physique, voir
    // le gestionnaire BTN: 'A' plus haut).
    const int8_t tab = hitTestStepSeqTab(x, y);
    const int8_t cell = hitTestStepSeqCell(x, y);
    if (tab >= 0) {
      selectedSeqTrack = tab;
      drawStepSeqPage();
    } else if (cell >= 0) {
      const uint8_t globalStep = static_cast<uint8_t>(stepSeqFirstStep() + cell);
      const uint8_t activeSteps =
          static_cast<uint8_t>(patternMeasures[currentPattern] * kSeqStepsPerMeasure);
      if (globalStep < activeSteps) {
        const uint8_t track = static_cast<uint8_t>(selectedSeqTrack);
        if (globalStep == selectedSeqStep) {
          const bool newState = !seqStepOn[currentPattern][track][globalStep];
          seqStepOn[currentPattern][track][globalStep] = newState;
          char msg[20];
          snprintf(msg, sizeof(msg), "STEP:%d:%d:%d", track, globalStep, newState ? 1 : 0);
          sendToTeensy(msg);
        }
        selectedSeqStep = static_cast<int8_t>(globalStep);
        drawStepSeqPage();
      }
    }
  } else if (currentScreen == Screen::Song) {
    if (hitTestSongProjects(x, y)) {
      goTo(Screen::Project);
    } else if (hitTestSongMode(x, y)) {
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
        songSelectedSlot = static_cast<uint8_t>(slot);
        songPatterns[slot] = static_cast<uint8_t>((songPatterns[slot] + 1) % kPatternCount);
        if (slot >= songLen) {
          songLen = static_cast<uint8_t>(slot + 1);
          drawSongLenRow();
        }
        char msg[16];
        snprintf(msg, sizeof(msg), "SONGSET:%d:%d", slot, songPatterns[slot]);
        sendToTeensy(msg);
        drawSongPage();
      }
    }
  } else if (currentScreen == Screen::Project) {
    const int8_t row = hitTestProjectRow(x, y);
    if (row >= 0) selectProjectSlot(static_cast<uint8_t>(row));
    else if (hitTestProjectLoad(x, y)) requestProjectLoad();
    else if (hitTestProjectSave(x, y)) requestProjectSave();
  } else if (currentScreen == Screen::Config) {
    if (hitTestSaverStyle(x, y)) {
      configSelectedRow = 0;
      configChangeRow(x < kScreenSize / 2 ? -1 : 1);
    } else if (hitTestCfgMinus(x, y)) {
      configSelectedRow = 1;
      configChangeRow(-1);
    } else if (hitTestCfgPlus(x, y)) {
      configSelectedRow = 1;
      configChangeRow(1);
    } else if (hitTestScaleMinus(x, y)) {
      configSelectedRow = 2;
      currentScaleIndex = static_cast<uint8_t>((currentScaleIndex + kScaleCount - 1) % kScaleCount);
      drawConfigPage();
    } else if (hitTestScalePlus(x, y)) {
      configSelectedRow = 2;
      currentScaleIndex = static_cast<uint8_t>((currentScaleIndex + 1) % kScaleCount);
      drawConfigPage();
    } else if (hitTestSwingMinus(x, y) || hitTestSwingPlus(x, y)) {
      configSelectedRow = 3;
      const int delta = hitTestSwingPlus(x, y) ? kSwingStep : -kSwingStep;
      swingValue = static_cast<uint8_t>(constrain(static_cast<int>(swingValue) + delta, 0, 127));
      drawConfigPage();
      char msg[16];
      snprintf(msg, sizeof(msg), "SWING:%d", swingValue);
      sendToTeensy(msg);
    }
  } else if (currentScreen == Screen::EmuPicker) {
    if (x >= kEmuCardX && x < kEmuCardX + kEmuCardW) {
      if (y >= kEmuCardGbY && y < kEmuCardGbY + kEmuCardH) {
        emuPickerActivate(0);
      } else if (y >= kEmuCardGbcY && y < kEmuCardGbcY + kEmuCardH) {
        emuPickerActivate(1);
      } else if (y >= kEmuCardNesY && y < kEmuCardNesY + kEmuCardH) {
        emuPickerActivate(2);
      } else if (y >= kEmuCardNeoY && y < kEmuCardNeoY + kEmuCardH) {
        emuPickerActivate(3);
      }
    }
  } else if (currentScreen == Screen::Retro && !gbIsLoaded()) {
    if (gbRomCount > 0) {
      if (y >= kScaleBtnY && y < kScaleBtnY + kScaleBtnH && x >= kScaleBtnX2 &&
          x < kScaleBtnX2 + kScaleBtnW) {
        gbDisplayScale = 2;
        drawRetroPage();
        return;
      }
      if (y >= kScaleBtnY && y < kScaleBtnY + kScaleBtnH && x >= kScaleBtnX3 &&
          x < kScaleBtnX3 + kScaleBtnW) {
        gbDisplayScale = 3;
        drawRetroPage();
        return;
      }
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
            drawGbViewportFrame();
          }
        }
      }
    } else {
      // Aucune ROM trouvee : toucher n'importe ou rescanne /games (utile
      // si la carte SD vient d'etre inseree).
      gbRomCount = gbScanRoms(gbRomNames);
      drawRetroPage();
    }
  } else if (currentScreen == Screen::NgpRetro && !ngpIsLoaded()) {
    if (ngpRomCount > 0 && y >= kNgpRowTop && y < kNgpRowTop + kNgpVisibleRows * kNgpRowH) {
      const uint8_t row = static_cast<uint8_t>((y - kNgpRowTop) / kNgpRowH);
      const uint8_t index = static_cast<uint8_t>(ngpRomScroll + row);
      if (index < ngpRomCount && ngpLoadRom(ngpRomNames[index])) drawNgpPage();
    } else if (ngpRomCount == 0) {
      ngpRomCount = ngpScanRoms(ngpRomNames);
      drawNgpPage();
    }
  }
}

void handleTouchUp(uint8_t slot, int16_t x, int16_t y) {
  if (currentScreen == Screen::Sampler && heldSamplerFile[slot] >= 0 &&
      inBox(x, y, 20, 96, 191, 191)) {
    const int col = (x - 20) / 49;
    const int row = (y - 96) / 49;
    if (col < 4 && row < 4 && (x - 20) % 49 < 44 && (y - 96) % 49 < 44) {
      samplerSelectedRow = static_cast<uint8_t>(heldSamplerFile[slot]);
      samplerSelectedPad = static_cast<uint8_t>(row * 4 + col);
      samplerAssignSelected();
      drawSamplerPage();
    }
  }
  heldSamplerFile[slot] = -1;
  if ((currentScreen == Screen::Audio || currentScreen == Screen::Sampler) && heldAudioPad[slot] >= 0) {
    if (currentScreen == Screen::Audio) drawAudioCell(static_cast<uint8_t>(heldAudioPad[slot]), false);
    char msg[24];
    if (padTargetTrack >= 0) {
      snprintf(msg, sizeof(msg), "PAD:%02d:UP:track=%d", heldAudioPad[slot], padTargetTrack);
    } else {
      snprintf(msg, sizeof(msg), "PAD:%02d:UP", heldAudioPad[slot]);
    }
    sendToTeensy(msg);
  }
  heldAudioPad[slot] = -1;
}

void loop() {
  static uint32_t lastHeartbeatMs = 0;
  static bool wasActive[2] = {false, false};
  static int16_t lastTouchX[2] = {};
  static int16_t lastTouchY[2] = {};
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
  const bool gbGameActive = (currentScreen == Screen::Retro && gbIsLoaded());

  // [2026-09-27] REVERT : readTeensyStatus() ne sert pas qu'a l'UI tracker --
  // c'est le SEUL chemin par lequel les BTN:/NAV: du Teensy (croix, A/B,
  // encodeurs -- les vraies commandes physiques du jeu, voir handleTeensyLine()
  // "BTN:" ligne ~6490) arrivent jusqu'a l'emulateur GB. La gater derriere
  // !gbGameActive a coupe les boutons pendant le jeu (confirme par test
  // materiel). Remise en inconditionnel.
  readTeensyStatus();
  if (!gbGameActive && patchUiNeedsRedraw) {
    patchUiNeedsRedraw = false;
    if (currentScreen == Screen::Patch && !screensaverActive) {
      // Le moteur, le cadre du scope et la piste n'ont pas change : ne
      // rafraichir que les deux zones affectees par le nouveau preset.
      drawPatchList();
      drawPatchWindow();
      updatePatchEncoderHints();
    }
  }
  if (!gbGameActive && samplerNeedsRedraw && currentScreen == Screen::Sampler && !screensaverActive) {
    samplerNeedsRedraw = false;
    drawSamplerPage();
  }
  // Dessin du tracer scope differe hors de readTeensyStatus() -- voir
  // commentaire ScopeRxState/scopeNeedsRedraw plus haut : dessiner un
  // paquet a la fois DANS la boucle de lecture Serial1 bloquait la
  // lecture assez longtemps pour perdre des octets a 921600 bauds.
  if (!gbGameActive && scopeNeedsRedraw) {
    scopeNeedsRedraw = false;
    if (currentScreen == Screen::Patch && !screensaverActive) {
      drawPatchScope();
    }
  }

  TouchPoint touches[2] = {};
  const uint8_t touchReadCount = !gbGameActive ? readTouches(touches) : 0;
  const bool touchReadValid = touchReadCount != 0xFF;

  for (uint8_t slot = 0; slot < 2; ++slot) {
    if (gbGameActive) {
      pendingActive[slot] = false;
      wasActive[slot] = false;
      continue;
    }
    if (!touchReadValid) continue;
    const bool rawActive = touches[slot].active;
    if (rawActive) {
      lastTouchX[slot] = touches[slot].x;
      lastTouchY[slot] = touches[slot].y;
    }
    // Debounce SYMETRIQUE dans les deux sens (2026-09-24) : `active` ne
    // change que si LA MEME lecture brute est confirmee par 2 tours de
    // boucle consecutifs. Avant, une seule lecture rawActive=false
    // suffisait a couper le toucher instantanement (aucun debounce a la
    // relache, contrairement a l'armement qui en exigeait deja 2) -- un
    // simple bruit du capteur pendant un doigt pose sans bouger etait
    // donc lu comme un relache-puis-repose, redeclenchant
    // handleTouchDown() (donc une NOUVELLE note) en plein milieu d'un
    // appui continu. Signale comme "l'ANALOG a des sautes d'humeur, le
    // note change sans raison" -- capture serie a l'appui : NOTE:/STEP:
    // renvoyes en rafale toutes les ~100-200ms pendant un pad tenu sans
    // interruption.
    bool active = wasActive[slot];
    if (rawActive != active && rawActive == pendingActive[slot]) {
      active = rawActive;
    }
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
        handleTouchUp(slot, lastTouchX[slot], lastTouchY[slot]);
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
  // Inhibe l'ENTREE en veille tant qu'une ROM GB est chargee
  // (2026-09-19, "audit de faisabilite des racks logiciels", R1 :
  // "la veille peut arreter le jeu a l'issue du delai sans interaction,
  // meme si une ROM est chargee") -- bug reel confirme en relisant le
  // code : gbRunFrame() lui-meme est conditionne par !screensaverActive
  // (voir plus bas), la veille ne se contente donc pas de cacher le
  // jeu, elle le MET EN PAUSE completement. Rester idle 60s (le delai
  // par defaut) sans toucher a un bouton pendant une scene calme d'un
  // jeu est un scenario parfaitement normal, pas une vraie inactivite.
  // Le delai d'inactivite normal reprend normalement des qu'on quitte
  // la page JEUX ou decharge la ROM (currentScreen/gbIsLoaded()
  // reevalues a chaque tour de boucle, rien a reinitialiser).
  if (!screensaverActive && !gbGameActive && screensaverTimeoutSec > 0 &&
      (nowForIdle - lastActivityMs) >= static_cast<uint32_t>(screensaverTimeoutSec) * 1000UL) {
    screensaverEnter();
  }
  if (screensaverActive) {
    screensaverStep();
  }

  // Efface le temoin de potard (voir drawPotToast()) apres son delai --
  // relance un rendu complet de la page courante plutot que de retenir
  // ce qu'il y avait sous la bande, plus simple/robuste.
  if (!gbGameActive && potToastActive && (nowForIdle - potToastLastMs) >= kPotToastTimeoutMs) {
    potToastActive = false;
    if (!screensaverActive) {
      drawScreen(currentScreen);
    }
  }

  if (!gbGameActive && currentScreen == Screen::Engines && !screensaverActive &&
      (nowForIdle - engineAnimLastMs) >= 80U) {
    engineAnimLastMs = nowForIdle;
    ++engineAnimFrame;
    drawEngVisualizer();
  }

  // Emulateur Game Boy : une frame dure exactement 70224 cycles a
  // 4 194 304 Hz, soit 16 742,706298 us. Garder seulement 16 742 us
  // cree une petite derive permanente ; l'accumulateur de reste ci-dessous
  // alterne 16 742/16 743 us et conserve la cadence native sur la duree.
  constexpr uint32_t kGbFramePeriodUs = 16742;
  constexpr uint32_t kGbFrameRemainder = 2962432;
  constexpr uint32_t kGbClockHz = 4194304;
  // Le bandeau de diagnostic est utile en qualification, mais inutile en
  // jeu et provoque un rafraichissement parasite chaque seconde. Les mesures
  // restent envoyees sur le port serie pour les tests.
  // [2026-09-25] Reactive a la demande de l'utilisateur : seul moyen de
  // suivre fps/frames manquees sans rouvrir le port serie (qui redemarre
  // la carte via le circuit auto-reset CH340 et coupe le test en cours).
  constexpr bool kGbPerfOverlay = true;
  static uint32_t nextGbFrameUs = 0;
  static uint32_t gbFrameFraction = 0;
  static uint32_t gbFrameCount = 0;
  static uint32_t gbFrameTimeTotalUs = 0;
  static uint32_t gbFrameTimeMaxUs = 0;
  static uint32_t gbMissedFrames = 0;
  static uint32_t gbFpsWindowStartMs = 0;
  if (currentScreen == Screen::Retro && gbIsLoaded() && !screensaverActive && !gbSettingsMenuOpen) {
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
      const GbRuntimeStats runtime = gbRuntimeStats();
      const uint32_t cpuX100 = static_cast<uint32_t>(
          (static_cast<uint64_t>(frameAvgUs) * 10000ULL) / 16743ULL);
      // [2026-09-27] Un seul Serial.write() au lieu de ~20 Serial.print()
      // separes -- corruption reproduite sur materiel (des fragments GB:PERF
      // se retrouvaient entrelaces avec la ligne suivante, meme sans aucun
      // trafic Teensy concurrent, donc pas une histoire de tache concurrente
      // cote firmware : la cause reelle semble etre le nombre d'appels
      // separes, chacun pouvant etre coupe par un flush/refill UART partiel).
      // Construire la ligne complete dans un buffer local puis l'envoyer
      // d'un coup rend l'ecriture atomique du point de vue de l'appelant.
      char perfLine[256];
      const int perfLineLen = snprintf(
          perfLine, sizeof(perfLine),
          "GB:PERF:fps_x100=%lu:frame_us_avg=%lu:frame_us_max=%lu:work_p99_us=%lu:"
          "core_avg_us=%lu:cpu_only_avg_us=%lu:display_avg_us=%lu:audio_avg_us=%lu:"
          "core_max_us=%lu:display_max_us=%lu:audio_max_us=%lu:blit_scale_us=%lu:"
          "blit_copy_us=%lu:blit_flush_us=%lu:cpu_x100=%lu:heap_free_kb=%lu:"
          "psram_free_kb=%lu:missed=%lu\n",
          static_cast<unsigned long>(fpsX100), static_cast<unsigned long>(frameAvgUs),
          static_cast<unsigned long>(gbFrameTimeMaxUs), static_cast<unsigned long>(runtime.p99WorkUs),
          static_cast<unsigned long>(runtime.avgCoreUs), static_cast<unsigned long>(runtime.avgCpuOnlyUs),
          static_cast<unsigned long>(runtime.avgDisplayUs), static_cast<unsigned long>(runtime.avgAudioUs),
          static_cast<unsigned long>(runtime.maxCoreUs), static_cast<unsigned long>(runtime.maxDisplayUs),
          static_cast<unsigned long>(runtime.maxAudioUs), static_cast<unsigned long>(gGbBlitScaleLastUs),
          static_cast<unsigned long>(gGbBlitCopyLastUs), static_cast<unsigned long>(gGbBlitFlushLastUs),
          static_cast<unsigned long>(cpuX100), static_cast<unsigned long>(ESP.getFreeHeap() / 1024U),
          static_cast<unsigned long>(ESP.getFreePsram() / 1024U), static_cast<unsigned long>(gbMissedFrames));
      if (perfLineLen > 0) {
        Serial.write(reinterpret_cast<const uint8_t *>(perfLine),
                     min(static_cast<size_t>(perfLineLen), sizeof(perfLine) - 1));
      }

      if (kGbPerfOverlay) {
        // Diagnostic discret dans la bande superieure reservee au mode GB.
        // Ne touche jamais aux 432 px de l'image du jeu (y=24..455).
        gfx->fillRect(120, 0, 270, 22, RGB565_BLACK);
        gfx->setTextSize(1);
        gfx->setTextColor(gbMissedFrames == 0 ? kDim : RGB565_RED);
        gfx->setCursor(120, 5);
        char perfLabel[48];
        snprintf(perfLabel, sizeof(perfLabel), "%lu.%02luFPS %luus M%lu S%u",
                 static_cast<unsigned long>(fpsX100 / 100),
                 static_cast<unsigned long>(fpsX100 % 100),
                 static_cast<unsigned long>(frameAvgUs),
                 static_cast<unsigned long>(gbMissedFrames),
                 static_cast<unsigned>(runtime.autosaveFailures));
        gfx->print(perfLabel);
      }
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

#ifdef AZ2_NES_ENABLED
  if (currentScreen == Screen::NesRetro && nesIsLoaded() && !screensaverActive) {
    nesRunFrame();
    static uint32_t lastNesPerfMs = 0;
    const uint32_t nesNowMs = millis();
    if (nesNowMs - lastNesPerfMs >= 1000) {
      const NesRuntimeStats nesPerf = nesRuntimeStats();
      Serial.print("NES:PERF:fps_x10=");
      Serial.print(nesPerf.fpsX10);
      Serial.print(":core_avg_us=");
      Serial.print(nesPerf.avgCoreUs);
      Serial.print(":core_max_us=");
      Serial.print(nesPerf.maxCoreUs);
      Serial.print(":frames=");
      Serial.println(nesPerf.totalFrames);
      lastNesPerfMs = nesNowMs;
    }
  }
#endif

  if (currentScreen == Screen::NgpRetro && ngpIsLoaded() && !screensaverActive) {
    ngpRunFrame();
    static uint32_t lastNgpPerfMs = 0;
    const uint32_t ngpNowMs = millis();
    if (ngpNowMs - lastNgpPerfMs >= 1000) {
      const NgpRuntimeStats ngpPerf = ngpRuntimeStats();
      Serial.print("NGP:PERF:fps_x10=");
      Serial.print(ngpPerf.fpsX10);
      Serial.print(":core_avg_us=");
      Serial.print(ngpPerf.avgCoreUs);
      Serial.print(":core_max_us=");
      Serial.print(ngpPerf.maxCoreUs);
      Serial.print(":frames=");
      Serial.println(ngpPerf.totalFrames);
      lastNgpPerfMs = ngpNowMs;
    }
  }

  if (now - lastHeartbeatMs >= 1000) {
    lastHeartbeatMs = now;
    Serial.println("DISPLAY:ALIVE:TICK");
  }
  serviceUiCanvas();
}
