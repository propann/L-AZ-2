#pragma once

#include <stdint.h>

namespace az2::rack {

// Source de verite du cablage du module granulaire ESP32-S3 N16R8.
// Modifier ce fichier ET docs/AZ2_RACK_PINOUT.md dans le meme commit.
constexpr int8_t kI2sBclkPin = 7;
constexpr int8_t kI2sWsPin = 9;
constexpr int8_t kI2sDataOutPin = 11;

// Faisceau du moteur spectral vers le S3 agrégateur. Le WROOM partage les
// horloges GPIO7/GPIO9 mais possède une ligne DATA séparée vers cette entrée.
constexpr int8_t kSpectralDataInPin = 12;   // WROOM GPIO22 -> S3 GPIO12
constexpr int8_t kSpectralControlTxPin = 4; // S3 TX -> WROOM GPIO16 RX
constexpr int8_t kSpectralControlRxPin = 5; // S3 RX <- WROOM GPIO17 TX

// Reserve pour la future liaison de controle au Serial7 du Teensy.
constexpr int8_t kControlTxPin = 16;  // S3 TX -> Teensy pin 28 RX7
constexpr int8_t kControlRxPin = 18;  // S3 RX <- Teensy pin 29 TX7

constexpr uint32_t kControlBaud = 921600;

}  // namespace az2::rack
