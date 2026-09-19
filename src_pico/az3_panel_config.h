// AZ-3 - Panneau de controle : BROCHAGE ET ROLES.
//
// Tout ce qui depend du cablage est ici, et nulle part ailleurs. main.cpp ne
// contient que de la logique, az3_panel_io.h que l'acces au materiel.
//
// Feuille de cablage complete : docs/AZ3_CABLAGE_PANNEAU.md
// Conception du firmware      : docs/AZ3_PANNEAU_PICO.md
//
// --------------------------------------------------------------------
// LE PRINCIPE, EN UNE PHRASE
// --------------------------------------------------------------------
// Un CD74HC4067 est un interrupteur qui relie UN canal a la fois a un seul
// fil. Tout le brochage decoule de cette phrase :
//
//   - BOUTONS   -> le mux est parfait. Un bouton est lent ; lire 16 canaux
//                  l'un apres l'autre prend ~80 us, personne ne le voit.
//   - LIGNES DE -> le mux convient aussi : c'est la meme chose qu'un bouton,
//     MATRICE      lu une colonne a la fois.
//   - ENCODEURS -> NON. A et B doivent etre lus au MEME INSTANT ; le mux les
//                  lit a ~6 us d'ecart et peut renvoyer un etat faux pendant
//                  une transition. La quadrature reste sur de vraies GPIO.
//   - LED       -> NON. Une seule LED allumee a la fois sur 48 = 2 % du
//                  temps, et un 4067 ne regule aucun courant. Les LED sont
//                  donc confiees a un IS31FL3731 en I2C : il fait le
//                  balayage EN MATERIEL, a courant constant, et ne coute que
//                  2 broches au lieu de 5. Voir az3_led_driver.h.
//
// Plusieurs mux PARTAGENT les memes 4 lignes d'adresse et ne coutent qu'UNE
// broche chacun (leur SIG). C'est ce qui rend ce brochage possible.

#pragma once

#include <Arduino.h>
#include <AZ2_Protocol.h>

namespace az3 {

// =====================================================================
// BROCHAGE
// =====================================================================
// Le RP2040 a 26 broches utilisables : GPIO 0-22 et 26-28. GPIO 23 pilote le
// mode d'alimentation SMPS, GPIO 24 detecte le VBUS, GPIO 25 est la LED
// embarquee -- les trois sont exclues.
//
//
//   0, 1     UART vers le Teensy (Serial7, pins 28/29)
//   2 - 5    colonnes de la matrice BOUTONS, pilotees
//   6 - 9    adresse S0-S3, PARTAGEE par tous les mux
//   10       SIG du mux BOUTONS   (entree)
//   11       SIG du mux LIGNES    (entree)
//   12, 13   I2C0 SDA/SCL -> driver LED IS31FL3731
//   14 - 28  quadrature des 6 encodeurs (12 broches)
//   25       LED embarquee (interne)
//
// 26 broches sur 26. Tout est affecte -- et c'est le passage des LED en I2C
// qui l'a permis : le mux LED coutait 1 broche de SIG, mais surtout il
// imposait de garder les colonnes LED sur les memes GPIO que les boutons.

constexpr int kOnboardLedPin = 25;

// L'UART vers le Teensy utilise les broches par defaut de Serial1 (UART0) :
// TX = GPIO 0, RX = GPIO 1.

// Colonnes de la matrice BOUTONS : mises a LOW une par une pendant le scan,
// en haute impedance sinon. Depuis le passage des LED en I2C, ces broches ne
// servent PLUS qu'aux boutons -- les cathodes LED du pad vont directement au
// driver IS31FL3731, qui les balaye lui-meme.
//
// A VERIFIER AU MULTIMETRE sur la carte SparkFun : ce firmware suppose que
// la matrice de BOUTONS et la matrice de LED sont deux circuits distincts
// (4+4 fils pour les boutons, 4+12 pour les LED). C'est ce que decrit le
// guide SparkFun, mais il signale aussi que le serigraphiage prete a
// confusion. Si les colonnes s'averent PARTAGEES entre boutons et LED, il
// faudra les rendre au driver LED et rescanner les boutons autrement.
constexpr int kColPins[4] = {2, 3, 4, 5};

// Adresse partagee par TOUS les multiplexeurs.
constexpr int kMuxAddrPins[4] = {6, 7, 8, 9};

// Un SIG par multiplexeur. C'est le seul cout d'un mux supplementaire :
// l'adresse, elle, est partagee.
constexpr int kBtnMuxSignalPin = 10;  // entree INPUT_PULLUP
constexpr int kRowMuxSignalPin = 11;  // entree INPUT_PULLUP

// --- Driver LED IS31FL3731, en I2C ---
// GPIO 12/13 ne sont pas choisies au hasard : ce sont des broches I2C0
// valides du RP2040 (SDA sur 0/4/8/12/16/20, SCL sur 1/5/9/13/17/21).
constexpr int kI2cSdaPin = 12;
constexpr int kI2cSclPin = 13;

// Quadrature sur GPIO directes, 2 broches par encodeur. C'est ce qui fixe le
// plafond : 6 encodeurs = 12 broches, et il n'en reste plus.
constexpr int kEncoderPinsAB[][2] = {
    {14, 15}, {16, 17}, {18, 19}, {20, 21}, {22, 26}, {27, 28},
};

// =====================================================================
// CANAUX DES MULTIPLEXEURS
// =====================================================================

constexpr uint8_t kMuxChannelCount = 16;

// 0 = rouge, 1 = vert, 2 = bleu. Errata SparkFun connu : le reperage
// Vert/Bleu est inverse sur certains lots. Si les couleurs sortent
// echangees, inverser les deux fils.
constexpr uint8_t kColorCount = 3;

// --- Mux LIGNES (entree) : les 4 lignes de la matrice + reserve ---
// Deporter les lignes ici libere 4 GPIO pour une seule broche de SIG. C'est
// exactement ce qui permet le 6e encodeur.
//
// ATTENTION AU TEMPS D'ETABLISSEMENT : une ligne non selectionnee par le mux
// est completement flottante. Quand on la selectionne, le pull-up interne du
// Pico (~50 kohms) doit recharger la capacite du cablage -- avec ~100 pF,
// la constante de temps est de ~5 us, donc il faut ~25 us pour une lecture
// franche (voir kRowMuxSettleUs). Poser un pull-up externe de 10 kohms sur
// chaque ligne ramene ca a ~1 us et supprime toute incertitude : c'est
// 4 resistances, vivement recommande.
constexpr uint8_t kRowMuxChannel[4] = {0, 1, 2, 3};
// Canaux 4-15 libres pour d'autres entrees lentes.

// --- Mux BOUTONS (entree) ---
// SIG est lu en INPUT_PULLUP, l'autre patte de chaque bouton va au GND : un
// contact ferme tire le canal au GND. La resistance passante du 4067
// (~70 ohms) est negligeable devant le pull-up interne.
// Canal 0..N-1 = clic de l'encodeur 1..N.
constexpr uint8_t kBtnMuxChannelPlay = 8;
constexpr uint8_t kBtnMuxChannelStop = 9;
constexpr uint8_t kBtnMuxChannelRec = 10;
constexpr uint8_t kBtnMuxChannelShift = 11;
// Canaux 12-15 libres.

// =====================================================================
// ENCODEURS
// =====================================================================

// Role de la ROTATION d'un encodeur. La croix et les boutons A/B/C/D
// n'existent plus en materiel : ce sont les encodeurs qui reproduisent leurs
// messages, pour que l'UI de l'ESP32 (4680 lignes) n'ait pas a changer.
enum class EncoderRole : uint8_t {
  NavVertical,    // un cran -> NAV:UP / NAV:DOWN
  NavHorizontal,  // un cran -> NAV:LEFT / NAV:RIGHT
  Pot,            // accumule une valeur absolue 0-127 -> POT:<n>
  Macro,          // delta brut -> MACRO:<n>
};

struct EncoderConfig {
  EncoderRole role;
  uint8_t roleIndex;   // n de POT:<n> / MACRO:<n> ; ignore pour les roles NAV
  char buttonLetter;   // lettre emise en BTN:<X> au clic ; '\0' = ENC:<i> brut
  uint8_t potInitial;  // valeur de depart du role Pot ; ignore sinon
};

// TABLE DES ROLES -- c'est ici, et nulle part ailleurs, qu'on ajoute, retire
// ou reaffecte un encodeur. Le reste du firmware s'adapte tout seul : canaux
// de mux, annonce au boot, diagnostics, protocole.
//
// Correspondance avec les commandes AZ-2 supprimees :
//   1 -> croix haut/bas + bouton A (valider)
//   2 -> croix gauche/droite + bouton B (retour)
//   3 -> POT:0 volume general + bouton C
//   4 -> POT:1 reverb + bouton D
//   5 -> POT:2 delay, qui retrouve enfin une molette
//   6 -> MACRO:0 transpose
constexpr EncoderConfig kEncoders[] = {
    {EncoderRole::NavVertical, 0, 'A', 0},
    {EncoderRole::NavHorizontal, 0, 'B', 0},
    {EncoderRole::Pot, 0, 'C', 100},   // volume : demarrer audible
    {EncoderRole::Pot, 1, 'D', 0},     // reverb : a zero au demarrage
    {EncoderRole::Pot, 2, '\0', 0},    // delay
    {EncoderRole::Macro, 0, '\0', 0},  // transpose
};
constexpr uint8_t kEncoderCount = sizeof(kEncoders) / sizeof(kEncoders[0]);

static_assert(kEncoderCount == sizeof(kEncoderPinsAB) / sizeof(kEncoderPinsAB[0]),
              "une paire de broches A/B par encodeur");
static_assert(kEncoderCount <= kBtnMuxChannelPlay,
              "un canal de mux par clic d'encodeur, avant les boutons de facade");

// =====================================================================
// MODE MANETTE
// =====================================================================
// Un encodeur emet une IMPULSION, il ne peut pas maintenir une direction --
// sans consequence pour les menus (l'ESP32 agit sur :DOWN), redhibitoire
// pour l'emulateur Game Boy. En mode manette, huit pads prennent le relais :
// eux savent rester enfonces.
//
// Le mapping colle a ce que l'ESP32 attend DEJA (voir ses handlers NAV:,
// BTN: et ENC:) : croix -> NAV:, A/B -> BTN:A/B, SELECT/START -> ENC:1/ENC:2.
// Aucune modification cote ecran.
//
//        COL_0   COL_1   COL_2   COL_3
// ROW_0          HAUT           SELECT
// ROW_1  GAUCHE         DROITE   START
// ROW_2          BAS               B
// ROW_3                            A

enum class GamepadAction : uint8_t { None, Nav, Btn, Enc };

struct GamepadBinding {
  GamepadAction action;
  const char *navDirection;  // action Nav
  char letter;               // action Btn
  uint8_t encIndex;          // action Enc
};

constexpr GamepadBinding kGamepadMap[az2::kPadCount] = {
    /* 00 */ {GamepadAction::None, nullptr, 0, 0},
    /* 01 */ {GamepadAction::Nav, "UP", 0, 0},
    /* 02 */ {GamepadAction::None, nullptr, 0, 0},
    /* 03 */ {GamepadAction::Enc, nullptr, 0, 1},  // SELECT
    /* 04 */ {GamepadAction::Nav, "LEFT", 0, 0},
    /* 05 */ {GamepadAction::None, nullptr, 0, 0},
    /* 06 */ {GamepadAction::Nav, "RIGHT", 0, 0},
    /* 07 */ {GamepadAction::Enc, nullptr, 0, 2},  // START
    /* 08 */ {GamepadAction::None, nullptr, 0, 0},
    /* 09 */ {GamepadAction::Nav, "DOWN", 0, 0},
    /* 10 */ {GamepadAction::None, nullptr, 0, 0},
    /* 11 */ {GamepadAction::Btn, nullptr, 'B', 0},
    /* 12 */ {GamepadAction::None, nullptr, 0, 0},
    /* 13 */ {GamepadAction::None, nullptr, 0, 0},
    /* 14 */ {GamepadAction::None, nullptr, 0, 0},
    /* 15 */ {GamepadAction::Btn, nullptr, 'A', 0},
};

// =====================================================================
// TEMPS ET ECHELLES
// =====================================================================

constexpr uint32_t kPadHoldMs = 600;
constexpr uint32_t kPadDebounceMs = 12;
constexpr uint32_t kButtonDebounceMs = 15;
constexpr uint32_t kColumnSettleUs = 5;

// Le CD74HC4067 commute en ~100 ns ; 5 us couvre largement ca plus la
// capacite parasite du cablage, POUR UN CANAL DEJA POLARISE (un bouton, dont
// l'autre patte est au GND).
constexpr uint32_t kMuxSettleUs = 5;
// Les lignes de matrice, elles, sont flottantes quand le mux ne les
// selectionne pas : il faut laisser le pull-up interne (~50 kohms) recharger
// la ligne. 25 us couvre ~5 constantes de temps avec 100 pF de cablage. Avec
// des pull-ups externes de 10 kohms (recommande), 5 us suffiraient largement.
constexpr uint32_t kRowMuxSettleUs = 25;

// 4 transitions de quadrature = 1 cran mecanique sur les EC11 courants.
constexpr int32_t kQuadratureStepsPerDetent = 4;
// Amplitude ajoutee/retiree a un role Pot par cran, sur l'echelle 0-127.
constexpr int32_t kPotStepPerDetent = 2;
// Limite de debit des messages POT: -- evite de saturer l'UART sur une
// rotation rapide. Les crans sont des evenements discrets, il n'y a pas de
// bruit ADC a lisser comme avec de vrais potentiometres.
constexpr uint32_t kPotMinIntervalMs = 20;

// Rythme de rafraichissement de l'image LED envoyee au driver. Le balayage
// lui-meme est fait EN MATERIEL par l'IS31FL3731 : on ne lui envoie une
// trame que quand l'etat a change, ou au plus a cette cadence.
constexpr uint32_t kLedRefreshMs = 20;  // 50 Hz, largement suffisant
// Intensite par defaut, 0-255 (PWM materiel du driver, par LED).
constexpr uint8_t kLedBrightness = 96;

}  // namespace az3
