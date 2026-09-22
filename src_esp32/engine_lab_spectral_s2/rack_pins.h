#pragma once

#include <stdint.h>

namespace az2::spectral {

// En banc autonome, le WROOM est maître. Dans le rack final, GPIO26/GPIO25
// deviennent entrées esclaves et reçoivent l'horloge commune Teensy via le S3.
constexpr int8_t kI2sBclkPin = 26;
constexpr int8_t kI2sWsPin = 25;
constexpr int8_t kI2sDataOutPin = 22;

// Contrôle point-à-point avec le S3 agrégateur. Inutilisé dans le banc USB.
constexpr int8_t kControlRxPin = 16;
constexpr int8_t kControlTxPin = 17;
constexpr uint32_t kControlBaud = 115200;

}  // namespace az2::spectral
