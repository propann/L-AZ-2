#pragma once

#include <Arduino.h>

namespace az2 {

constexpr uint32_t kControlBaud = 230400;

// Son de l'emulateur GB, ESP32 -> Teensy (demande 2026-09-15, "il faut
// un emulateur complet classe" + "envoyer sous forme de paquet ... pour
// que le DAC le joue"). Framing binaire distinct du protocole texte
// habituel (toujours ASCII imprimable, jamais l'octet 0x01) : sur le
// MEME lien Serial1 que NAV:/BTN:/POT:/etc, un paquet est
// [kGbAudioPacketMagic][longueur 1 octet][longueur octets de PCM mono
// 8 bits non signe, AUDIO_SAMPLE_RATE Hz -- voir minigb_apu.h cote
// ESP32]. Pas de saut de ligne, pas de contenu ASCII -- le lecteur cote
// Teensy doit reconnaitre l'octet magique AVANT d'accumuler une ligne
// texte (voir readStream()/AudioRxState dans src_teensy/az2_audio/
// main.cpp). Debit choisi expres bas (8 kHz mono 8 bits = 8 Ko/s) pour
// tenir large dans le budget du lien 230400 bauds (~23 Ko/s bruts)
// mele au reste du trafic de controle.
constexpr uint8_t kGbAudioPacketMagic = 0x01;
constexpr uint32_t kGbAudioSampleRate = 8000;

// Oscilloscope, Teensy -> ESP32 cette fois (demande 2026-09-15, "une
// fenetre ou on voit l'onde du son jouer evoluer en modifiant le
// patch"). Meme principe de paquet binaire que le son GB (magique +
// longueur + octets), octet magique different (0x02, jamais 0x01) pour
// rester distinguable si jamais les deux sens se retrouvaient un jour
// sur le meme flux logique -- ici c'est surtout par clarte, chaque
// direction du lien Serial1 (TX Teensy / TX ESP32) est deja separee
// physiquement. Paquet = 32 octets de PCM mono 8 bits (decime x4 depuis
// les blocs 128 echantillons/44.1kHz de la lib Audio Teensy -- largement
// suffisant pour un tracer visuel, pas de la haute-fidelite).
constexpr uint8_t kScopePacketMagic = 0x02;
constexpr uint8_t kScopeSamplesPerPacket = 32;
constexpr uint8_t kPadCount = 16;
// Gamme chromatique des 16 pads (voix live) : pad 0 = kPadBaseNote
// (MIDI), pad 15 = kPadBaseNote+15. 48 = C3. Partage entre les deux
// cartes depuis le 2026-09-17 (avant : duplique en dur cote Teensy
// seulement -- l'ESP32 en a besoin pour "poser" une note de pad
// directement sur un pas du sequenceur, voir padEditsStep dans
// src_esp32/az2_screen/main.cpp).
constexpr uint8_t kPadBaseNote = 48;
constexpr uint8_t kPadRows = 4;
constexpr uint8_t kPadCols = 4;

constexpr const char *kHelloControl = "HELLO:ESP32_CONTROL";
constexpr const char *kHelloAudio = "HELLO:TEENSY_AUDIO";
constexpr const char *kPlay = "PLAY";
constexpr const char *kStop = "STOP";
constexpr const char *kRecToggle = "REC:TOGGLE";
constexpr const char *kStatusReady = "READY";
constexpr const char *kStatusPlaying = "PLAYING";
constexpr const char *kStatusStopped = "STOPPED";

inline uint8_t padId(uint8_t row, uint8_t col) {
  return (row * kPadCols) + col;
}

inline bool validPad(uint8_t pad) {
  return pad < kPadCount;
}

inline void printPadEvent(Print &out, uint8_t pad, bool pressed, uint8_t velocity = 110) {
  if (!validPad(pad)) {
    return;
  }

  out.print("PAD:");
  if (pad < 10) {
    out.print('0');
  }
  out.print(pad);
  out.print(pressed ? ":DOWN:vel=" : ":UP");
  if (pressed) {
    out.print(velocity);
  }
  out.println();
}

inline void printPadHold(Print &out, uint8_t pad) {
  if (!validPad(pad)) {
    return;
  }

  out.print("PAD:");
  if (pad < 10) {
    out.print('0');
  }
  out.print(pad);
  out.println(":HOLD");
}

inline void printMacro(Print &out, uint8_t index, int32_t delta) {
  out.print("MACRO:");
  out.print(index);
  out.print(':');
  if (delta >= 0) {
    out.print('+');
  }
  out.println(delta);
}

inline void printLedEvent(Print &out, uint8_t pad, const char *state) {
  if (!validPad(pad)) {
    return;
  }

  out.print("LED:");
  if (pad < 10) {
    out.print('0');
  }
  out.print(pad);
  out.print(':');
  out.println(state);
}

inline void printStatus(Print &out, const char *role, const char *state) {
  out.print("STATUS:");
  out.print(role);
  out.print(':');
  out.println(state);
}

inline void printBpm(Print &out, float bpm) {
  out.print("BPM:");
  out.println(bpm, 2);
}

// Croix directionnelle + 4 boutons, cablees directement sur le Teensy
// (voir AZ2_CABLAGE_MASTER.md) depuis l'abandon du Pico/matrice SparkFun
// le 2026-09-14 ("ça m'a soule, on fait sans la matrice de bouton"). Ces
// noms servent aussi de futurs boutons de jeu (croix + A/B/C/D) pour le
// mode JEUX (voir AZ2_EMULATION_JEUX.md).
inline void printNav(Print &out, const char *direction, bool pressed) {
  out.print("NAV:");
  out.print(direction);
  out.println(pressed ? ":DOWN" : ":UP");
}

inline void printBtn(Print &out, char button, bool pressed) {
  out.print("BTN:");
  out.print(button);
  out.println(pressed ? ":DOWN" : ":UP");
}

// Potentiometre : valeur ABSOLUE (pas un delta comme printMacro/l'ancien
// encodeur Pico) -- 0-127, format compatible MIDI CC en vue d'une
// eventuelle sortie MIDI plus tard. Depuis le 2026-09-15, les 3
// "potards" sont en realite des encodeurs rotatifs incrementaux (voir
// AZ2_CABLAGE_MASTER.md) -- POT: reste le meme protocole (valeur
// absolue accumulee cote Teensy a chaque cran), rien a changer cote
// ESP32/consommateurs de ce message.
inline void printPot(Print &out, uint8_t index, uint8_t value) {
  out.print("POT:");
  out.print(index);
  out.print(':');
  out.println(value);
}

// Bouton poussoir integre a chaque encodeur rotatif (voir
// AZ2_CABLAGE_MASTER.md) -- namespace separe de BTN: (croix/A-D) pour ne
// pas entrer en collision avec le mapping manette du mode JEUX
// (GbButton::A/B/Select/Start, voir AZ2_EMULATION_JEUX.md). Pas de
// fonction musicale assignee pour l'instant, juste transmis -- meme
// principe que BTN:C/BTN:D, libres pour un usage futur.
inline void printEnc(Print &out, uint8_t index, bool pressed) {
  out.print("ENC:");
  out.print(index);
  out.println(pressed ? ":DOWN" : ":UP");
}

inline void printPattern(Print &out, uint8_t pattern) {
  out.print("PATTERN:");
  if (pattern < 10) {
    out.print('0');
  }
  out.println(pattern);
}

inline void printClock(Print &out, uint16_t bar, uint8_t step) {
  out.print("CLOCK:bar=");
  out.print(bar);
  out.print(":step=");
  out.println(step);
}

// ---------------------------------------------------------------------
// Division du sequenceur (voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md, "vrai
// sequenceur") : le pas dure 1/stepsPerBeat de noire. 4 = croche pointee.. .
// non -- 4 = double-croche (1/16, comportement d'origine), etc. Table
// partagee ESP32/Teensy/Pico pour que le cycle de choix cote ecran tombe
// toujours sur une valeur que le Teensy sait jouer.
// ---------------------------------------------------------------------
struct DivisionOption {
  uint8_t stepsPerBeat;
  const char *label;
};

constexpr DivisionOption kDivisionOptions[] = {
    {1, "1/4"},
    {2, "1/8"},
    {3, "1/8 T"},
    {4, "1/16"},
    {6, "1/16 T"},
    {8, "1/32"},
};
constexpr uint8_t kDivisionOptionCount = sizeof(kDivisionOptions) / sizeof(kDivisionOptions[0]);

inline const char *divisionLabel(uint8_t stepsPerBeat) {
  for (uint8_t i = 0; i < kDivisionOptionCount; ++i) {
    if (kDivisionOptions[i].stepsPerBeat == stepsPerBeat) {
      return kDivisionOptions[i].label;
    }
  }
  return "?";
}

inline void printDivision(Print &out, uint8_t stepsPerBeat) {
  out.print("DIV:");
  out.println(stepsPerBeat);
}

// ---------------------------------------------------------------------
// Moteurs et patchs par piste (voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md, etape
// 5 avancee suite a la demande explicite du 2026-09-14 : "les moteurs
// audio ne sont pas selectionnables ni reglables ... faut faire un truc
// propre"). Les NOMS sont ici (partages ESP32/Teensy/Pico pour l'affichage
// et le cycle de choix) ; les VRAIES donnees de patch (voix Dexed, index
// de forme Braids...) restent cote Teensy dans src_teensy/az2_audio/
// main.cpp, dans le MEME ORDRE que ces tables -- a garder synchronise a
// la main si on ajoute/retire un patch.
// ---------------------------------------------------------------------
constexpr uint8_t kEngineDexed = 0;
constexpr uint8_t kEngineEPiano = 1;
constexpr uint8_t kEngineBraids = 2;
// Ajoutes le 2026-09-15 ("on voit si on peut ajouter des moteurs audio")
// -- objets STANDARD de la lib Audio Teensy (AudioSynthKarplusStrong,
// AudioSynthWaveform+AudioEffectEnvelope), zero vendoring supplementaire,
// choisis pour couvrir 2 familles de synthese totalement differentes des
// 3 premiers moteurs (physique/corde pincee, et analogique/soustractif).
constexpr uint8_t kEngineKarplus = 3;
constexpr uint8_t kEngineAnalog = 4;
constexpr uint8_t kEngineCount = 5;

constexpr const char *kEngineNames[kEngineCount] = {"DEXED", "EPIANO", "BRAIDS", "KARPLUS", "ANALOG"};

constexpr uint8_t kDexedPatchCount = 8;
constexpr const char *kDexedPatchNames[kDexedPatchCount] = {
    "FM-Rhodes", "Steinway", "Korg CX3", "Leadharp",
    "FatSynth A", "Jupiter 8", "Mini-Moog", "Moog Strings",
};

constexpr uint8_t kEPianoPatchCount = 5;
constexpr const char *kEPianoPatchNames[kEPianoPatchCount] = {
    "Default", "Bright", "Mellow", "Autopan", "Tremolo",
};

constexpr uint8_t kBraidsPatchCount = 8;
constexpr const char *kBraidsPatchNames[kBraidsPatchCount] = {
    "CSAW", "Saw/Square", "Triple Saw", "Toy",
    "Vosim", "FM", "Plucked", "Saw Swarm",
};

// AudioSynthKarplusStrong n'expose aucun parametre de forme (juste
// noteOn(freq,vel)/noteOff()) -- un seul "patch" possible avec cet objet.
constexpr uint8_t kKarplusPatchCount = 1;
constexpr const char *kKarplusPatchNames[kKarplusPatchCount] = {"Corde pincee"};

// AudioSynthWaveform : la forme d'onde EST le patch (WAVEFORM_* dans
// synth_waveform.h) -- ordre choisi pour couvrir les classiques
// analogiques (sinus/dent de scie/carre/triangle), voir applyTrackPatch()
// cote Teensy pour la correspondance avec les constantes WAVEFORM_*.
constexpr uint8_t kAnalogPatchCount = 4;
constexpr const char *kAnalogPatchNames[kAnalogPatchCount] = {
    "Sinus", "Dent de scie", "Carre", "Triangle",
};

inline uint8_t enginePatchCount(uint8_t engine) {
  switch (engine) {
    case kEngineDexed: return kDexedPatchCount;
    case kEngineEPiano: return kEPianoPatchCount;
    case kEngineBraids: return kBraidsPatchCount;
    case kEngineKarplus: return kKarplusPatchCount;
    case kEngineAnalog: return kAnalogPatchCount;
    default: return 1;
  }
}

inline const char *enginePatchName(uint8_t engine, uint8_t patch) {
  switch (engine) {
    case kEngineDexed: return patch < kDexedPatchCount ? kDexedPatchNames[patch] : "?";
    case kEngineEPiano: return patch < kEPianoPatchCount ? kEPianoPatchNames[patch] : "?";
    case kEngineBraids: return patch < kBraidsPatchCount ? kBraidsPatchNames[patch] : "?";
    case kEngineKarplus: return patch < kKarplusPatchCount ? kKarplusPatchNames[patch] : "?";
    case kEngineAnalog: return patch < kAnalogPatchCount ? kAnalogPatchNames[patch] : "?";
    default: return "?";
  }
}

inline const char *engineName(uint8_t engine) {
  return engine < kEngineCount ? kEngineNames[engine] : "?";
}

// ESP32/Pico -> Teensy: choix direct (pas de +1/-1, l'ecran calcule le
// prochain index avec enginePatchCount()/kEngineCount et l'envoie tel
// quel ; le Teensy renvoie confirmation sur les 3 liens, voir relayLine
// cote az2_audio).
inline void printEngineSelect(Print &out, uint8_t track, uint8_t engine) {
  out.print("ENGINE:");
  out.print(track);
  out.print(':');
  out.println(engine);
}

inline void printPatchSelect(Print &out, uint8_t track, uint8_t patch) {
  out.print("PATCH:");
  out.print(track);
  out.print(':');
  out.println(patch);
}

} // namespace az2
