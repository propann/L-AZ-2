#ifndef CONFIG_H
#define CONFIG_H

// [2026-09-27] Config minimale pour l'integration AZ-2 -- le config.h
// original d'Anemoia-ESP32 ne contenait que des broches materielles pour
// SON propre hardware de reference (SD/boutons/DAC), aucune n'est utilisee
// par le coeur lui-meme (cpu6502/ppu2C02/apu2A03/bus/cartridge/mapper*) : le
// grep confirme que seuls COMPOSITE_VIDEO, DEBUG, ENABLE_PROFILING et
// FRAMESKIP sont testes par #ifdef dans le coeur. AZ-2 fournit son propre
// affichage (Arduino_GFX, pas TFT_eSPI), son propre SD (deja cable) et son
// propre audio (paquets vers le Teensy, voir nes_emulator.cpp) -- rien de
// ces #define materiels ne s'applique ici.
//
// Laisses INTENTIONNELLEMENT non definis :
// - COMPOSITE_VIDEO : on utilise le chemin framebuffer RGB565 normal.
// - DEBUG / ENABLE_PROFILING : desactives par defaut, decommenter au besoin.
// - FRAMESKIP : rendu chaque frame par defaut, comme Peanut-GB/Walnut-CGB.

#endif
