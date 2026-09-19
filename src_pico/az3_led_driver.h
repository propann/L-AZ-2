// AZ-3 - Pilote LED IS31FL3731 (I2C) pour la matrice SparkFun 4x4 RGB.
//
// --------------------------------------------------------------------
// POURQUOI CE COMPOSANT
// --------------------------------------------------------------------
// Le CD74HC4067 est un commutateur analogique : il ne laisse passer qu'UN
// canal a la fois. Sur 4 colonnes x 12 anodes = 48 creneaux, chaque LED ne
// serait allumee que 2 % du temps, sans aucune regulation de courant -- et
// les trois couleurs, qui n'ont pas la meme tension de seuil, n'auraient pas
// la meme luminosite. C'est la raison de fond de l'echec de 2026-09, la
// broche EN en l'air n'etant que la cause immediate.
//
// L'IS31FL3731 fait le balayage EN MATERIEL, a courant constant, avec un PWM
// 8 bits par LED. Le Pico se contente d'ecrire une image : plus aucune
// contrainte temps reel cote firmware, et 2 broches au lieu de 5.
//
// --------------------------------------------------------------------
// GEOMETRIE -- LE SEUL POINT A VERIFIER SUR LA DATASHEET
// --------------------------------------------------------------------
// Le pad SparkFun demande 12 lignes d'ANODE x 4 lignes de CATHODE.
// L'IS31FL3731 est une matrice 16 x 9 : un cote a 16 sorties (les broches
// C1-C16 du module), l'autre en a 9 (A1-A9). Reste a savoir LEQUEL est le
// cote anode -- c'est la seule chose qui change le cablage, et la datasheet
// la donne en une ligne (schema d'application type : regarder si l'anode des
// LED va sur une broche C ou sur une broche A).
//
//   - Cote anode = C (16 sorties)  -> UN SEUL module suffit : 12 anodes sur
//     C1-C12, 4 colonnes sur A1-A4.        -> kAnodeSide = AnodeSide::C16
//   - Cote anode = A (9 sorties)   -> DEUX modules : 6 anodes chacun, meme
//     bus I2C, adresses distinctes.        -> kAnodeSide = AnodeSide::A9
//
// Les deux cas sont geres ici : une seule constante a changer, rien d'autre.
// En commander deux est la decision qui ne peut pas etre fausse -- dans le
// cas favorable le second sert de rechange.
//
// Module valide pour ce projet : "IS31FL3731 2946" (reference du breakout
// Adafruit du meme nom), c'est-a-dire le DRIVER NU avec ses broches sorties
// sur connecteur. Surtout pas une matrice LED deja peuplee : nos LED sont
// deja dans le pad SparkFun.
//
// --------------------------------------------------------------------
// ADRESSES I2C
// --------------------------------------------------------------------
// La broche AD fixe l'adresse : GND -> 0x74, VCC -> 0x75, SDA -> 0x76,
// SCL -> 0x77.

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "az3_panel_config.h"

namespace az3 {
namespace led {

// --- Registres (datasheet IS31FL3731) ---
constexpr uint8_t kRegCommand = 0xFD;       // selecteur de page
constexpr uint8_t kPageFunction = 0x0B;     // page des registres de fonction
constexpr uint8_t kPageFrame0 = 0x00;       // page image 0 (la seule utilisee)

constexpr uint8_t kFnConfig = 0x00;         // mode de fonctionnement
constexpr uint8_t kFnPictureFrame = 0x01;   // image affichee en mode Picture
constexpr uint8_t kFnShutdown = 0x0A;       // 0 = arret, 1 = marche

constexpr uint8_t kFrameLedControl = 0x00;  // 18 octets : 1 bit d'activation par LED
constexpr uint8_t kFramePwm = 0x24;         // 144 octets : 1 PWM par LED

constexpr uint8_t kModePicture = 0x00;      // affiche une image fixe, pas d'animation

// --- LA constante a ajuster apres lecture de la datasheet (voir en-tete) ---
enum class AnodeSide : uint8_t {
  C16,  // anodes sur les 16 sorties -> un seul module
  A9,   // anodes sur les 9 sorties  -> deux modules
};
constexpr AnodeSide kAnodeSide = AnodeSide::C16;

constexpr uint8_t kChipCount = (kAnodeSide == AnodeSide::C16) ? 1 : 2;
constexpr uint8_t kAnodeCount = kColorCount * 4;  // 12
constexpr uint8_t kAnodesPerChip = kAnodeCount / kChipCount;

// Adresses I2C, fixees par la broche AD de chaque module.
constexpr uint8_t kChipAddress[2] = {0x74, 0x75};

static_assert(kChipCount * kAnodesPerChip == kAnodeCount,
              "les 12 anodes RGB doivent etre exactement couvertes par les modules");
// La contrainte s'inverse selon la topologie : ce qui va sur le cote a 9
// sorties doit tenir dans 9, ce qui va sur le cote a 16 doit tenir dans 16.
//   C16 : anodes sur le cote 16, les 4 colonnes sur le cote 9
//   A9  : anodes sur le cote 9,  les 4 colonnes sur le cote 16
static_assert(kAnodeSide == AnodeSide::C16 ? (kAnodesPerChip <= 16 && 4 <= 9)
                                           : (kAnodesPerChip <= 9 && 4 <= 16),
              "geometrie incompatible avec la matrice 16x9 du driver");

// Une LED du pad est reperee par (couleur, ligne, colonne) :
//   anode   = couleur * 4 + ligne   -> 0..11
//   cathode = colonne               -> 0..3
inline uint8_t anodeOf(uint8_t color, uint8_t row) {
  return static_cast<uint8_t>(color * 4 + row);
}

// Position dans le plan PWM du driver. Le plan est organise en 9 groupes de
// 16 registres : l'index externe (0-8) est le cote "A", l'index interne
// (0-15) le cote "C". Selon de quel cote sont les anodes, le role des deux
// s'inverse -- c'est tout ce que kAnodeSide change.
//
//   anodes sur C : groupe = colonne (0-3),      position = anode locale
//   anodes sur A : groupe = anode locale (0-5), position = colonne
inline uint8_t pwmOffset(uint8_t localAnode, uint8_t col) {
  if (kAnodeSide == AnodeSide::C16) {
    return static_cast<uint8_t>(col * 16 + localAnode);
  }
  return static_cast<uint8_t>(localAnode * 16 + col);
}

// Image locale : une valeur PWM par LED, indexee par [anode][colonne].
// 12 x 4 = 48 octets, negligeable.
inline uint8_t frameBuffer[kAnodeCount][4] = {};
inline bool frameDirty = true;
inline uint32_t lastFlushMs = 0;
inline bool chipPresent[2] = {};

// Le coeur Arduino-mbed n'a pas Wire.setSDA()/setSCL() : les broches se
// donnent au CONSTRUCTEUR. On declare donc notre propre instance plutot que
// d'utiliser le Wire global, qui serait cable sur les broches par defaut.
inline arduino::MbedI2C bus(kI2cSdaPin, kI2cSclPin);

// --- Acces I2C bas niveau ---

inline bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  bus.beginTransmission(address);
  bus.write(reg);
  bus.write(value);
  return bus.endTransmission() == 0;
}

inline bool selectPage(uint8_t address, uint8_t page) {
  return writeRegister(address, kRegCommand, page);
}

// Ecrit un bloc contigu, par morceaux de 16 octets. Le tampon d'emission de
// MbedI2C fait 256 octets, donc ce decoupage n'est pas strictement necessaire
// ici -- il l'est sur les coeurs ou Wire se limite a 32 octets, et un
// depassement s'y traduirait par une ecriture SILENCIEUSEMENT tronquee.
inline bool writeBlock(uint8_t address, uint8_t reg, const uint8_t *data, uint8_t len) {
  constexpr uint8_t kChunk = 16;
  for (uint8_t offset = 0; offset < len; offset += kChunk) {
    const uint8_t n = min(static_cast<uint8_t>(len - offset), kChunk);
    bus.beginTransmission(address);
    bus.write(static_cast<uint8_t>(reg + offset));
    bus.write(data + offset, static_cast<int>(n));
    if (bus.endTransmission() != 0) {
      return false;
    }
  }
  return true;
}

// --- Initialisation ---

// N'active que les positions REELLEMENT cablees et laisse les autres
// eteintes -- sinon la puce piloterait des sorties dans le vide. Le registre
// d'activation est un champ de bits : 2 octets par groupe, un bit par
// position dans le groupe.
inline bool enableWiredLeds(uint8_t address) {
  uint8_t control[18] = {};
  for (uint8_t localAnode = 0; localAnode < kAnodesPerChip; ++localAnode) {
    for (uint8_t col = 0; col < 4; ++col) {
      const uint8_t offset = pwmOffset(localAnode, col);
      const uint8_t group = static_cast<uint8_t>(offset / 16);
      const uint8_t bit = static_cast<uint8_t>(offset % 16);
      control[group * 2 + bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
  }
  return writeBlock(address, kFrameLedControl, control, sizeof(control));
}

inline bool beginChip(uint8_t address) {
  // Arret le temps de configurer, puis marche -- sequence recommandee par la
  // datasheet pour ne pas afficher de contenu indefini au demarrage.
  if (!selectPage(address, kPageFunction)) {
    return false;  // pas d'accuse de reception : puce absente ou mal adressee
  }
  writeRegister(address, kFnShutdown, 0x00);
  delay(10);
  writeRegister(address, kFnConfig, kModePicture);
  writeRegister(address, kFnPictureFrame, 0x00);

  if (!selectPage(address, kPageFrame0)) {
    return false;
  }
  // Tout le plan PWM a zero : sinon la puce afficherait le contenu aleatoire
  // de sa RAM au premier allumage.
  uint8_t zeros[144] = {};
  writeBlock(address, kFramePwm, zeros, sizeof(zeros));
  enableWiredLeds(address);

  selectPage(address, kPageFunction);
  writeRegister(address, kFnShutdown, 0x01);
  return selectPage(address, kPageFrame0);
}

inline void begin() {
  bus.begin();
  // 400 kHz : une trame complete (48 registres) passe en ~1,5 ms, tres loin
  // d'etre un probleme a 50 Hz de rafraichissement.
  bus.setClock(400000);

  for (uint8_t chip = 0; chip < kChipCount; ++chip) {
    chipPresent[chip] = beginChip(kChipAddress[chip]);
  }
  frameDirty = true;
}

// --- Ecriture de l'image ---

inline void setLed(uint8_t color, uint8_t row, uint8_t col, uint8_t pwm) {
  if (color >= kColorCount || row >= 4 || col >= 4) {
    return;
  }
  uint8_t &slot = frameBuffer[anodeOf(color, row)][col];
  if (slot != pwm) {
    slot = pwm;
    frameDirty = true;
  }
}

inline void setPadColor(uint8_t pad, uint8_t colorMask, uint8_t pwm) {
  if (!az2::validPad(pad)) {
    return;
  }
  const uint8_t row = static_cast<uint8_t>(pad / 4);
  const uint8_t col = static_cast<uint8_t>(pad % 4);
  for (uint8_t color = 0; color < kColorCount; ++color) {
    setLed(color, row, col, bitRead(colorMask, color) ? pwm : 0);
  }
}

inline void clear() {
  for (auto &anode : frameBuffer) {
    for (uint8_t &slot : anode) {
      slot = 0;
    }
  }
  frameDirty = true;
}

// Envoie l'image aux puces si elle a change. A appeler depuis loop() : il n'y
// a AUCUNE contrainte temps reel ici, le balayage est fait par le materiel.
// Rien a voir avec un POV logiciel, ou rater une echeance fait clignoter.
inline void flush(uint32_t now) {
  if (!frameDirty || (now - lastFlushMs) < kLedRefreshMs) {
    return;
  }
  lastFlushMs = now;
  frameDirty = false;

  for (uint8_t chip = 0; chip < kChipCount; ++chip) {
    if (!chipPresent[chip]) {
      continue;
    }
    for (uint8_t local = 0; local < kAnodesPerChip; ++local) {
      const uint8_t anode = static_cast<uint8_t>(chip * kAnodesPerChip + local);
      for (uint8_t col = 0; col < 4; ++col) {
        const uint8_t reg =
            static_cast<uint8_t>(kFramePwm + pwmOffset(local, col));
        writeRegister(kChipAddress[chip], reg, frameBuffer[anode][col]);
      }
    }
  }
}

// --- Diagnostic ---

// Balaye les adresses possibles de la broche AD et dit lesquelles repondent.
// C'est le premier test a faire : une puce absente de cette liste est une
// puce mal alimentee, mal adressee, ou dont le SDA/SCL est inverse.
inline void scanBus(Print &out) {
  out.print("LEDSCAN:");
  bool any = false;
  for (uint8_t address = 0x74; address <= 0x77; ++address) {
    bus.beginTransmission(address);
    if (bus.endTransmission() == 0) {
      out.print(" 0x");
      out.print(address, HEX);
      any = true;
    }
  }
  if (!any) {
    out.print(" AUCUNE PUCE (verifier alim, SDA/SCL, broche AD)");
  }
  out.println();

  out.print("LEDSCAN:attendues:");
  for (uint8_t chip = 0; chip < kChipCount; ++chip) {
    out.print(" 0x");
    out.print(kChipAddress[chip], HEX);
    out.print(chipPresent[chip] ? "(ok)" : "(MUETTE)");
  }
  out.println();
}

}  // namespace led
}  // namespace az3
