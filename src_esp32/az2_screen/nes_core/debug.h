#ifndef DEBUG_H
#define DEBUG_H

#include "config.h"
#include <Arduino.h>

// [2026-09-27] Version AZ-2 : le fichier original loggait la config de
// broches materielle propre a Anemoia (TFT/SD/manettes GPIO), aucune ne
// s'applique ici (voir config.h). log_pin_config() n'a plus de sens sur
// AZ-2 -- retiree avec le reste des dependances materielles Anemoia
// (TFT_eSPI, verification OPTIMIZATION_FLAGS specifique a leur build).
#ifdef DEBUG
    #define LOG(msg)       Serial.println(msg)
    #define LOGF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
    #define LOG(msg)
    #define LOGF(fmt, ...)
#endif

#endif
