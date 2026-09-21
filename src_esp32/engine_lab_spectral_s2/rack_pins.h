#pragma once

#include <stdint.h>

namespace az2::spectral {

// Banc autonome ESP-WROOM-32D -> DAC I2S. Ces broches ne concernent pas
// l'ESP32-S3 granulaire. Toute future liaison WROOM -> S3 devra conserver un
// connecteur distinct et être validée avant soudure.
constexpr int8_t kI2sBclkPin = 26;
constexpr int8_t kI2sWsPin = 25;
constexpr int8_t kI2sDataOutPin = 22;

// Réservées au futur contrôle/agrégateur. Inutilisées dans le banc USB.
constexpr int8_t kControlRxPin = 16;
constexpr int8_t kControlTxPin = 17;
constexpr uint32_t kControlBaud = 921600;

}  // namespace az2::spectral
