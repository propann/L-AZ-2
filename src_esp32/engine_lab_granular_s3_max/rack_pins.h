#pragma once

#include <stdint.h>

namespace az2::rack {

// Source de verite du cablage du module granulaire ESP32-S3 N16R8.
// Modifier ce fichier ET docs/AZ2_RACK_PINOUT.md dans le meme commit.
constexpr int8_t kI2sBclkPin = 7;
constexpr int8_t kI2sWsPin = 9;
constexpr int8_t kI2sDataOutPin = 11;

// Reserve pour la future liaison de controle au Serial7 du Teensy.
constexpr int8_t kControlTxPin = 16;  // S3 TX -> Teensy pin 28 RX7
constexpr int8_t kControlRxPin = 18;  // S3 RX <- Teensy pin 29 TX7

constexpr uint32_t kControlBaud = 921600;

}  // namespace az2::rack
