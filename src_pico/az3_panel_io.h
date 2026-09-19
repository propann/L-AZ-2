// AZ-3 - Panneau de controle : ACCES AU MATERIEL.
//
// Cette couche ne connait ni le protocole AZ2, ni les roles des encodeurs,
// ni les modes. Elle sait seulement lire des contacts et des broches de
// quadrature. Toute la logique est dans main.cpp, tout le brochage dans
// az3_panel_config.h.
//
// Les LED ne passent PAS par ici : elles sont entierement gerees par le
// driver I2C (az3_led_driver.h), qui fait son balayage en materiel.

#pragma once

#include <Arduino.h>

#include "az3_panel_config.h"

namespace az3 {
namespace io {

// =====================================================================
// MULTIPLEXEURS
// =====================================================================
// Tous les mux partagent les memes 4 lignes d'adresse et n'ont chacun que
// leur propre SIG. Poser l'adresse une fois puis lire plusieurs SIG permet
// de lire N mux pour le prix d'un seul etablissement.

inline void setMuxAddress(uint8_t channel) {
  digitalWrite(kMuxAddrPins[0], bitRead(channel, 0));
  digitalWrite(kMuxAddrPins[1], bitRead(channel, 1));
  digitalWrite(kMuxAddrPins[2], bitRead(channel, 2));
  digitalWrite(kMuxAddrPins[3], bitRead(channel, 3));
}

// Lit un canal du mux BOUTONS. true = contact ferme (canal tire au GND).
inline bool readButtonChannel(uint8_t channel) {
  setMuxAddress(channel);
  delayMicroseconds(kMuxSettleUs);
  return digitalRead(kBtnMuxSignalPin) == LOW;
}

// =====================================================================
// MATRICE DE BOUTONS
// =====================================================================
// Une colonne est pilotee a LOW, les autres sont laissees en haute impedance
// (INPUT) plutot que mises a HIGH : si deux touches d'une meme ligne sont
// enfoncees, deux colonnes se retrouvent reliees entre elles. En haute
// impedance ca ne fait rien ; avec une sortie a HIGH face a une sortie a LOW,
// c'est un court-circuit franc entre deux GPIO.

inline void releaseAllColumns() {
  for (int col : kColPins) {
    pinMode(col, INPUT);
  }
}

inline void driveColumn(uint8_t col) {
  releaseAllColumns();
  pinMode(kColPins[col], OUTPUT);
  digitalWrite(kColPins[col], LOW);
  delayMicroseconds(kColumnSettleUs);
}

// Lit les 4 lignes de la colonne actuellement pilotee. Les lignes passent par
// le mux LIGNES : c'est ce qui libere 4 GPIO et permet le 6e encodeur.
//
// Le temps d'etablissement est plus long que pour un bouton (voir
// kRowMuxSettleUs) : une ligne non selectionnee est completement flottante,
// et le pull-up interne du Pico doit recharger la capacite du cablage.
inline void readColumnRows(bool out[4]) {
  for (uint8_t row = 0; row < 4; ++row) {
    setMuxAddress(kRowMuxChannel[row]);
    delayMicroseconds(kRowMuxSettleUs);
    out[row] = digitalRead(kRowMuxSignalPin) == LOW;
  }
}

// =====================================================================
// QUADRATURE
// =====================================================================
// Les broches A/B restent sur de vraies GPIO : A et B doivent etre lues au
// meme instant, ce qu'un mux ne sait pas faire. C'est aussi ce qui fixe le
// plafond a 6 encodeurs (2 broches chacun, et il n'en reste plus).

// Renvoie l'etat brut : bit 2i = A de l'encodeur i, bit 2i+1 = B.
inline uint16_t readEncoderLines() {
  uint16_t lines = 0;
  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    if (digitalRead(kEncoderPinsAB[i][0]) == HIGH) {
      lines |= static_cast<uint16_t>(1u << (i * 2));
    }
    if (digitalRead(kEncoderPinsAB[i][1]) == HIGH) {
      lines |= static_cast<uint16_t>(1u << (i * 2 + 1));
    }
  }
  return lines;
}

// =====================================================================
// DEMARRAGE
// =====================================================================

inline void begin() {
  for (int pin : kMuxAddrPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  pinMode(kBtnMuxSignalPin, INPUT_PULLUP);
  pinMode(kRowMuxSignalPin, INPUT_PULLUP);

  releaseAllColumns();

  for (uint8_t i = 0; i < kEncoderCount; ++i) {
    pinMode(kEncoderPinsAB[i][0], INPUT_PULLUP);
    pinMode(kEncoderPinsAB[i][1], INPUT_PULLUP);
  }

  pinMode(kOnboardLedPin, OUTPUT);
}

}  // namespace io
}  // namespace az3
