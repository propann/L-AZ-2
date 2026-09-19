// AZ-3 - Firmware Pico (RP2040) : PANNEAU DE CONTROLE COMPLET.
//
// Historique : ce Pico avait ete tente puis abandonne sur l'AZ-2 le
// 2026-09-14 (boitier plein + mux LED jamais fonctionnel, voir
// docs/AZ2_CABLAGE_PICO.md), les commandes etant recablees en direct sur le
// Teensy (croix + A/B/C/D + 3 encodeurs). L'AZ-3 fait le chemin inverse et
// va plus loin : le Teensy ne garde AUCUNE commande, il redevient un moteur
// audio pur, et TOUTE la facade vit ici.
//
// Ce qui disparait par rapport a l'AZ-2 : la croix directionnelle et les
// boutons A/B/C/D physiques. Leurs messages NAV:/BTN: continuent pourtant
// d'exister -- ils sont SYNTHETISES a partir des encodeurs (voir
// kEncoderRoles plus bas). C'est volontaire : l'UI de l'ESP32 ecran (4680
// lignes) consomme deja ce vocabulaire, elle continue de fonctionner sans
// qu'on y touche une seule ligne.
//
// CE QUI EST CONFIRME SUR LE VRAI MATERIEL (2026-09-13, firmware d'origine) :
// le scan de la matrice SparkFun (PAD:00..15 DOWN/UP/HOLD) et les encodeurs.
// CE QUI NE L'EST PAS : le retour LED. Le mux LED n'a jamais allume une
// seule LED -- cause identifiee (broche EN laissee en l'air) mais la
// correction EN -> GND n'a JAMAIS ETE REVERIFIEE avant l'abandon du projet.
// Le chemin mux LED est donc conserve tel quel ici, avec son mode LEDTEST :
// c'est un test de 5 minutes une fois le Pico rebranche, pas un chantier.
//
// --------------------------------------------------------------------
// MATERIEL
// --------------------------------------------------------------------
// - Matrice SparkFun 4x4 RGB : VRAIE matrice ligne/colonne (pas 16 boutons
//   independants). 4 colonnes pilotees + 4 lignes lues. Les LED sont trois
//   matrices 4x4 superposees (une par couleur) partageant les memes 4
//   colonnes cathodes. Voir
//   https://learn.sparkfun.com/tutorials/button-pad-hookup-guide/all
//   (errata connu : reperage Vert/Bleu inverse sur certains lots).
// - DEUX CD74HC4067 (16 voies) PARTAGEANT LES MEMES 4 LIGNES D'ADRESSE :
//     * mux LED  (sortie) : les 12 lignes d'anode RGB, canal = couleur*4+ligne
//     * mux IN   (entree) : les boutons d'encodeur + 12 canaux de reserve
//   Partager S0-S3 entre les deux coute 4 broches au lieu de 8 et donne 32
//   canaux pour 6 broches au total. Chaque mux garde SA propre broche SIG.
//   RAPPEL CRITIQUE : la broche EN de CHAQUE 4067 est active a l'etat BAS et
//   DOIT etre reliee au GND commun. Laissee en l'air, le mux entier est
//   desactive en permanence et aucun canal ne passe jamais -- c'est
//   exactement le symptome qui a coute le projet Pico en 2026-09.
// - N encodeurs EC11 (quadrature A/B + bouton). A/B sur de vraies GPIO (la
//   quadrature a besoin d'un echantillonnage rapide et regulier, un mux lui
//   ferait rater des crans) ; le bouton, lui, est lent et va sur le mux IN.
//
// --------------------------------------------------------------------
// BUDGET GPIO (le Pico RP2040 a 26 broches utilisables : 0-22 et 26-28 ;
// 23 = mode alim SMPS, 24 = detection VBUS, 25 = LED embarquee)
// --------------------------------------------------------------------
// Un encodeur coute 2 broches. Le brochage ci-dessous tient 4 encodeurs
// avec 2 broches de marge. Pour aller plus loin :
//   - 5 encodeurs : utiliser les 2 broches libres (27, 28).
//   - 6 ou 7      : deporter EN PLUS les 4 lignes de matrice sur le mux IN
//                   (canaux 4-7), ce qui libere GPIO 6-9.
//   - 8 et plus   : le RP2040 ne suffit plus. Il faut une carte RP2350B
//                   (Pico Plus 2, 48 GPIO). ATTENTION : un "Pico 2"
//                   standard est un RP2350A et a exactement les memes 26
//                   GPIO -- il ne resout rien.
// Voir docs/AZ3_PANNEAU_PICO.md pour le tableau complet.

#include <Arduino.h>
#include <AZ2_Protocol.h>

namespace {

// =====================================================================
// BROCHAGE
// =====================================================================

// --- UART vers le Teensy ---
// Pins par defaut de Serial1 sur ce framework (UART0) : TX=GPIO0, RX=GPIO1.
// Cote Teensy c'est Serial7 (pins 28/29) en AZ-3 : l'ex-Serial3 (pins
// 14/15) du lien Pico d'origine a ete reaffecte a l'encodeur 1 sur l'AZ-2,
// et Serial2 (7/8) comme Serial5 (20/21) sont pris par l'I2S et le bouton B.

// --- Matrice : colonnes pilotees, lignes lues ---
// COL_0..COL_3 = pads {0,4,8,12} / {1,5,9,13} / {2,6,10,14} / {3,7,11,15}
// (nommage AZ-2, voir AZ2_CABLAGE_BASE.md). Verifier ces 4 fils contre le
// guide SparkFun : les colonnes y sont les cathodes LED communes, pas de
// vrais GND.
constexpr int kColPins[4] = {2, 3, 4, 5};
// ROW_0..ROW_3 = pads {0,1,2,3} / {4,5,6,7} / {8,9,10,11} / {12,13,14,15}
constexpr int kRowPins[4] = {6, 7, 8, 9};

// --- Lignes d'adresse PARTAGEES par les deux CD74HC4067 ---
constexpr int kMuxS0 = 10, kMuxS1 = 11, kMuxS2 = 12, kMuxS3 = 13;

// --- Mux LED (sortie) : 12 lignes d'anode RGB, canal = couleur*4 + ligne ---
constexpr int kLedMuxSignal = 14;   // vers l'entree commune, AVEC resistance serie
constexpr uint8_t kColorCount = 3;  // 0=rouge, 1=vert, 2=bleu (cf. errata SparkFun)

// --- Mux IN (entree) : boutons d'encodeur + reserve ---
// SIG est lu en INPUT_PULLUP : le contact ferme relie le canal au GND
// commun. La resistance passante du 4067 (~70 ohms) est negligeable devant
// le pull-up interne (~50 kohms), la lecture reste franche.
constexpr int kInMuxSignal = 15;
// Canal du mux IN portant le bouton de l'encodeur i. Les canaux 4-15
// restent libres pour de futurs boutons de facade (PLAY/STOP/REC...) --
// ou pour les 4 lignes de matrice si on passe a 6-7 encodeurs.
constexpr uint8_t kEncoderButtonChannel[] = {0, 1, 2, 3};

// --- LED embarquee : test visuel "il tourne" sans port serie ---
constexpr int kOnboardLedPin = 25;

// =====================================================================
// ENCODEURS
// =====================================================================

// Role de la rotation d'un encodeur. La croix et les boutons A/B/C/D
// n'existent plus physiquement : ce sont les encodeurs qui reproduisent
// leurs messages, pour que l'ESP32 ecran n'ait pas a changer.
enum class EncoderRole : uint8_t {
  NavVertical,    // un cran -> NAV:UP / NAV:DOWN
  NavHorizontal,  // un cran -> NAV:LEFT / NAV:RIGHT
  Pot,            // accumule une valeur absolue 0-127 -> POT:<n>
  Macro,          // delta brut -> MACRO:<n> (transpose, etc.)
};

struct EncoderConfig {
  int pinA;
  int pinB;
  EncoderRole role;
  uint8_t roleIndex;   // n de POT:<n> / MACRO:<n> ; ignore pour les roles NAV
  char buttonLetter;   // lettre emise en BTN:<X> au clic ; '\0' = ENC:<i> brut
  uint8_t potInitial;  // valeur de depart du role Pot (ignore sinon)
};

// TABLE DE BROCHAGE ET DE ROLES -- c'est ici, et nulle part ailleurs, qu'on
// ajoute ou retire un encodeur. Le reste du firmware s'adapte tout seul.
//
// Correspondance avec l'AZ-2 qu'on remplace :
//   ENC1 -> ancienne croix haut/bas + bouton A (valider)
//   ENC2 -> ancienne croix gauche/droite + bouton B (retour)
//   ENC3 -> ancien POT:0 (volume general) + bouton C
//   ENC4 -> ancien POT:1 (reverb) + bouton D
// POT:2 (delay) n'a plus de molette avec 4 encodeurs -- il reste reglable
// depuis l'ecran, et retrouve une molette des qu'un 5e encodeur est cable
// sur les GPIO 27/28 encore libres.
constexpr EncoderConfig kEncoders[] = {
    {16, 17, EncoderRole::NavVertical, 0, 'A', 0},
    {18, 19, EncoderRole::NavHorizontal, 0, 'B', 0},
    {20, 21, EncoderRole::Pot, 0, 'C', 100},  // volume : demarrer audible
    {22, 26, EncoderRole::Pot, 1, 'D', 0},    // reverb : a zero au demarrage
};
constexpr uint8_t kEncoderCount = sizeof(kEncoders) / sizeof(kEncoders[0]);

static_assert(kEncoderCount <= sizeof(kEncoderButtonChannel), "un canal de mux IN par bouton d'encodeur");

// =====================================================================
// TEMPS
// =====================================================================

constexpr uint32_t kPadHoldMs = 600;
constexpr uint32_t kButtonDebounceMs = 15;
constexpr uint32_t kPadDebounceMs = 12;
constexpr uint32_t kColumnSettleUs = 5;
// Temps d'etablissement apres un changement d'adresse de mux, avant de
// lire SIG. Le CD74HC4067 commute en ~100 ns ; 5 us couvre largement ca
// plus la capacite parasite du cablage.
constexpr uint32_t kMuxSettleUs = 5;
// Un cran d'encodeur ne "tient" pas une direction : on emet DOWN puis UP
// dans la foulee. L'ESP32 agit sur :DOWN et ne fait que memoriser l'etat
// sur :UP (voir son handler NAV:), donc une impulsion suffit pour la
// navigation dans les menus.
constexpr int32_t kQuadratureStepsPerDetent = 4;
// Amplitude ajoutee/retiree a un role Pot par cran, sur l'echelle 0-127.
constexpr int32_t kPotStepPerDetent = 2;
// Limite de debit des messages POT: -- evite de saturer l'UART sur une
// rotation tres rapide. Les crans sont des evenements discrets, il n'y a
// pas de bruit ADC a lisser comme avec de vrais potentiometres.
constexpr uint32_t kPotMinIntervalMs = 20;

// =====================================================================
// ETAT
// =====================================================================

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

struct EncoderState {
  uint8_t quadrature = 0;    // 2 derniers etats A/B empiles
  int32_t accum = 0;         // transitions depuis le dernier cran
  int16_t pendingDetents = 0;  // crans franchis, pas encore traduits

  uint8_t potValue = 0;     // role Pot : valeur absolue courante 0-127
  uint8_t potLastSent = 255;  // 255 = jamais envoye
  uint32_t potLastSentMs = 0;
  bool btnState = false;
  bool btnLastRaw = false;
  uint32_t btnLastChangeMs = 0;
};

EncoderState encoderStates[kEncoderCount];

// =====================================================================
// EMISSION
// =====================================================================

// Tout part en double : USB (diagnostic au moniteur serie, utilisable sans
// Teensy branche) et UART (le vrai destinataire).
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

void sendPot(uint8_t index, uint8_t value) {
  az2::printPot(Serial, index, value);
  az2::printPot(Serial1, index, value);
}

void sendBtn(char letter, bool pressed) {
  az2::printBtn(Serial, letter, pressed);
  az2::printBtn(Serial1, letter, pressed);
}

void sendEnc(uint8_t index, bool pressed) {
  az2::printEnc(Serial, index, pressed);
  az2::printEnc(Serial1, index, pressed);
}

// Impulsion de croix synthetique : un cran d'encodeur = un appui + un
// relachement immediat.
void sendNavPulse(const char *direction) {
  az2::printNav(Serial, direction, true);
  az2::printNav(Serial1, direction, true);
  az2::printNav(Serial, direction, false);
  az2::printNav(Serial1, direction, false);
}

// =====================================================================
// MULTIPLEXEURS
// =====================================================================

void writeMuxAddress(uint8_t channel) {
  digitalWrite(kMuxS0, bitRead(channel, 0));
  digitalWrite(kMuxS1, bitRead(channel, 1));
  digitalWrite(kMuxS2, bitRead(channel, 2));
  digitalWrite(kMuxS3, bitRead(channel, 3));
}

// Lecture d'un canal du mux IN. true = contact ferme (canal tire au GND).
bool readInputMux(uint8_t channel) {
  writeMuxAddress(channel);
  delayMicroseconds(kMuxSettleUs);
  return digitalRead(kInMuxSignal) == LOW;
}

void releaseAllColumns() {
  for (int col : kColPins) {
    pinMode(col, INPUT);
  }
}

// Allume brievement le canal LED d'une couleur/ligne donnee. Appele une
// fois par couleur active pendant la colonne en cours -- le balayage
// colonne x couleur x ligne donne l'effet "allume en continu" par
// persistance retinienne (POV).
void pulseLedChannel(uint8_t color, uint8_t row) {
  writeMuxAddress(static_cast<uint8_t>(color * 4 + row));
  digitalWrite(kLedMuxSignal, HIGH);
  delayMicroseconds(150);
  digitalWrite(kLedMuxSignal, LOW);
}

// Definie plus bas avec le reste des encodeurs -- declaree ici parce que le
// balayage LED l'appelle apres chaque impulsion (voir scanColumnLeds).
void pollEncoders();

// =====================================================================
// MATRICE
// =====================================================================

// Mode test materiel : un pad physiquement presse s'allume tout de suite en
// blanc, meme sans Teensy branche -- ca prime sur l'etat "musical" venant du
// Teensy, qui ne reste visible que quand le pad n'est pas presse.
uint8_t effectiveColorMask(uint8_t pad) {
  return padState[pad] ? 0b111 : teensyColorMask[pad];
}

// Boutons et LED sont scannes en 2 PASSES SEPAREES (et non colonne par
// colonne en alternant les deux) : voir le rapport temps reel du
// 2026-09-14 -- avec l'ancien code, appuyer sur un pad de la colonne 3
// attendait que les colonnes 0-2 aient fini boutons+LED (les pulses LED
// coutent jusqu'a ~1.8 ms/colonne si plusieurs sont allumees), soit du
// temps mort ajoute avant meme l'envoi de l'evenement. En lisant TOUTES
// les colonnes (rapide, sans delay) avant la phase LED (lente), un appui
// est detecte et envoye des le debut du tour de loop().

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

// Passe 2/2 : rafraichissement LED de la colonne (lent) -- appelee APRES
// que les 4 colonnes aient deja ete scannees pour les boutons, donc jamais
// sur le chemin critique d'un appui.
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
        // Voir pollEncoders() : c'est ICI que se joue la fiabilite de la
        // quadrature. Sonder apres chaque impulsion borne l'intervalle
        // entre deux lectures a ~150 us, meme quand les 48 LED sont
        // allumees.
        pollEncoders();
      }
    }
  }
}

void scanMatrix() {
  for (uint8_t col = 0; col < 4; ++col) {
    scanColumnButtons(col);
    pollEncoders();
  }
  for (uint8_t col = 0; col < 4; ++col) {
    scanColumnLeds(col);
    pollEncoders();
  }
}

// =====================================================================
// ENCODEURS
// =====================================================================

// Un cran vient d'etre franchi : traduire selon le role de l'encodeur.
void applyDetent(uint8_t index, int32_t direction) {
  const EncoderConfig &cfg = kEncoders[index];
  EncoderState &st = encoderStates[index];

  switch (cfg.role) {
    case EncoderRole::NavVertical:
      sendNavPulse(direction > 0 ? "DOWN" : "UP");
      break;

    case EncoderRole::NavHorizontal:
      sendNavPulse(direction > 0 ? "RIGHT" : "LEFT");
      break;

    case EncoderRole::Pot: {
      const int32_t next = static_cast<int32_t>(st.potValue) + direction * kPotStepPerDetent;
      st.potValue = static_cast<uint8_t>(constrain(next, 0, 127));
      break;
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
  EncoderState &st = encoderStates[index];

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

// ECHANTILLONNAGE DE LA QUADRATURE -- separe de la traduction en messages.
//
// Le probleme a resoudre : le balayage LED en POV peut occuper jusqu'a ~7 ms
// par tour de boucle (4 colonnes x jusqu'a 12 impulsions de 150 us). Si on
// ne lit les encodeurs qu'une fois par tour, une rotation rapide franchit
// plusieurs crans entre deux lectures et le firmware en perd. Le defaut
// existait dans le firmware Pico d'origine, ou il passait inapercu :
// presque aucune LED ne s'allumait, donc la boucle etait rapide.
//
// La solution retenue est le SONDAGE FREQUENT plutot que l'interruption.
// pollEncoders() est appele depuis l'interieur du balayage LED, apres chaque
// impulsion -- l'intervalle maximal tombe donc a ~150 us, largement sous la
// milliseconde qui separe deux transitions meme sur une rotation tres rapide.
// Cout : 2 digitalRead par encodeur, soit ~5 % du temps d'une impulsion.
//
// Pourquoi pas attachInterrupt(), theoriquement plus propre : sur le coeur
// Arduino-mbed, attachInterrupt cree un objet InterruptIn qui prend la main
// sur la broche, et faire un digitalRead de cette meme broche depuis l'ISR
// est un comportement qui demande une validation sur le vrai materiel. Le
// sondage, lui, reutilise exactement le chemin de code deja confirme
// fonctionnel le 2026-09-13. Si la quadrature s'avere malgre tout trop lente
// une fois les LED reellement allumees, l'interruption est le plan B.
void pollEncoders() {
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    const EncoderConfig &cfg = kEncoders[i];
    EncoderState &st = encoderStates[i];

    const uint8_t a = digitalRead(cfg.pinA);
    const uint8_t b = digitalRead(cfg.pinB);
    const uint8_t current = static_cast<uint8_t>((a << 1) | b);
    st.quadrature = static_cast<uint8_t>(((st.quadrature << 2) | current) & 0x0F);
    st.accum += kQuadratureTable[st.quadrature];

    while (st.accum >= kQuadratureStepsPerDetent) {
      st.accum -= kQuadratureStepsPerDetent;
      ++st.pendingDetents;
    }
    while (st.accum <= -kQuadratureStepsPerDetent) {
      st.accum += kQuadratureStepsPerDetent;
      --st.pendingDetents;
    }
  }
}

// Traduit en messages les crans accumules par pollEncoders(). Separe parce
// que l'emission serie n'a rien a faire au milieu d'un balayage LED : elle
// etirerait l'impulsion en cours et ferait clignoter la matrice.
void drainEncoderRotation(uint8_t index) {
  int16_t detents = encoderStates[index].pendingDetents;
  encoderStates[index].pendingDetents = 0;
  for (; detents > 0; --detents) {
    applyDetent(index, 1);
  }
  for (; detents < 0; ++detents) {
    applyDetent(index, -1);
  }
}

// Le bouton d'un encodeur est lu via le mux IN, pas via une GPIO dediee.
void updateEncoderButton(uint8_t index, uint32_t now) {
  const EncoderConfig &cfg = kEncoders[index];
  EncoderState &st = encoderStates[index];

  const bool raw = readInputMux(kEncoderButtonChannel[index]);
  if (raw != st.btnLastRaw) {
    st.btnLastRaw = raw;
    st.btnLastChangeMs = now;
  }

  if ((now - st.btnLastChangeMs) < kButtonDebounceMs || raw == st.btnState) {
    return;
  }

  st.btnState = raw;
  // Un encodeur porteur d'une lettre remplace le bouton A/B/C/D disparu ;
  // sinon on emet l'evenement ENC: brut, comme avant.
  if (cfg.buttonLetter != '\0') {
    sendBtn(cfg.buttonLetter, raw);
  } else {
    sendEnc(index, raw);
  }
}

void scanEncoders() {
  const uint32_t now = millis();
  pollEncoders();
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    drainEncoderRotation(i);
    updateEncoderButton(i, now);
    flushPot(i, now);
  }
}

// =====================================================================
// DIAGNOSTIC LEDTEST
// =====================================================================
// Suspend le scan normal et maintient UN SEUL canal du mux LED allume en
// continu (pas en pulse POV de 150 us -- trop bref pour verifier au
// multimetre ou meme a l'oeil de facon fiable), colonne par colonne, canal
// par canal, avec le detail imprime en clair. Se pilote depuis le moniteur
// serie USB du Pico, sans Teensy :
//   LEDTEST        -- defilement automatique (~0,7 s/canal)
//   LEDTEST:ALL    -- pareil mais les 4 colonnes ensemble (jusqu'a 4 LED a
//                     la fois, pratique si une seule LED est HS)
//   LEDTEST:STOP   -- arret, retour au scan normal
//   MUXTEST        -- etat des 16 canaux du mux IN (boutons d'encodeur)
//
// C'EST LE PREMIER TEST A FAIRE au rebranchement du Pico : la panne LED de
// 2026-09 avait pour cause une broche EN laissee en l'air, et la correction
// (EN -> GND commun) n'a jamais ete reverifiee.
bool ledTestActive = false;
bool ledTestAllColumns = false;
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
  Serial.println("LEDTEST:RAPPEL -- broche EN de CHAQUE CD74HC4067 reliee au GND commun ?");
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
    // ligne/couleur) allumees simultanement -- pour maximiser la chance de
    // voir quelque chose si une seule LED est peut-etre HS.
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

  writeMuxAddress(ledTestChannel);
  digitalWrite(kLedMuxSignal, HIGH);

  ++ledTestChannel;
  if (ledTestChannel >= 12) {  // canaux 12-15 du mux LED inutilises
    ledTestChannel = 0;
    ledTestColumn = static_cast<uint8_t>((ledTestColumn + 1) % 4);
  }
}

// Etat brut des 16 canaux du mux IN -- verifie d'un coup le cablage des
// boutons d'encodeur et repere les canaux libres.
void muxTest() {
  Serial.print("MUXTEST:IN:");
  for (uint8_t ch = 0; ch < 16; ++ch) {
    Serial.print(readInputMux(ch) ? '1' : '0');
  }
  Serial.println();
  Serial.println("MUXTEST:1=contact ferme (canal tire au GND). Canaux 0-3 = boutons encodeurs 1-4.");
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
      } else if (line == "MUXTEST") {
        muxTest();
      }
      line = "";
      continue;
    }
    if (line.length() < 32) {
      line += c;
    }
  }
}

// =====================================================================
// RECEPTION TEENSY
// =====================================================================

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

// =====================================================================
// DEMARRAGE
// =====================================================================

void setupMatrixPins() {
  for (int row : kRowPins) {
    pinMode(row, INPUT_PULLUP);
  }
  releaseAllColumns();

  pinMode(kMuxS0, OUTPUT);
  pinMode(kMuxS1, OUTPUT);
  pinMode(kMuxS2, OUTPUT);
  pinMode(kMuxS3, OUTPUT);
  pinMode(kLedMuxSignal, OUTPUT);
  digitalWrite(kLedMuxSignal, LOW);
  pinMode(kInMuxSignal, INPUT_PULLUP);
}

void setupEncoderPins() {
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    pinMode(kEncoders[i].pinA, INPUT_PULLUP);
    pinMode(kEncoders[i].pinB, INPUT_PULLUP);
    // Le bouton passe par le mux IN : pas de pinMode par encodeur ici.
    if (kEncoders[i].role == EncoderRole::Pot) {
      encoderStates[i].potValue = kEncoders[i].potInitial;
    }
  }
}

void printBootInfo() {
  Serial.println("AZ2:ROLE:PICO_KEYPAD");
  Serial.println("AZ2:FEATURE:SPARKFUN_4X4_MATRIX");
  Serial.println("AZ2:FEATURE:LED_RGB_MUX");
  Serial.println("AZ2:FEATURE:INPUT_MUX");
  Serial.print("AZ2:FEATURE:ENCODERS_");
  Serial.println(kEncoderCount);
  // La croix et A/B/C/D n'existent plus en materiel : l'ESP32 recoit
  // toujours NAV:/BTN:, synthetises par les encodeurs.
  Serial.println("AZ2:FEATURE:NAV_FROM_ENCODERS");
}

// LED embarquee : clignote pour un test visuel "il tourne" sans avoir
// besoin d'un ordinateur branche.
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
    // Les encodeurs sont volontairement inertes pendant LEDTEST : lire le
    // mux d'entree changerait l'adresse PARTAGEE par les deux mux et
    // deplacerait la LED que le diagnostic doit justement maintenir figee
    // ~0,7 s. C'est un mode de diagnostic, pas un mode de jeu.
    ledTestStep();
  } else {
    scanMatrix();
    scanEncoders();
  }
  readTeensyStatus();
  heartbeat();
}
