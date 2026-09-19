#pragma once

#include <Arduino.h>

namespace az2 {

// 921600 depuis le 2026-09-18 (etait 230400) -- palier A de
// AZ2_EMULATION_JEUX.md ("porter l'UART ecran<->Teensy a 921600 bauds,
// apres test d'erreurs sur cable reel") : prerequis pour ameliorer la
// qualite audio du jeu (8kHz mono 8 bits actuel -- voir gb_emulator.cpp/
// kGbAudioSampleRate plus bas) sans saturer le lien. A VERIFIER sur
// materiel reel a chaque flash : si la liaison ESP32<->Teensy devient
// instable/corrompue (commandes qui se perdent, croix qui ne repond
// plus, jeu GB qui plante) apres ce changement, revenir a 230400 --
// aucune donnee de fiabilite sur cable long/bruite a cette vitesse
// avant cette session.
constexpr uint32_t kControlBaud = 921600;

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
// main.cpp).
//
// PLAFOND REEL A CONNAITRE avant de monter ce chiffre encore plus haut :
// la longueur du paquet est un SEUL OCTET (max 255), et le nombre
// d'echantillons par paquet vaut AUDIO_SAMPLE_RATE/VERTICAL_SYNC (voir
// minigb_apu.h, VERTICAL_SYNC ~= 59.7275 Hz, la vraie cadence GB) --
// donc AUDIO_SAMPLE_RATE ne doit jamais depasser ~15200 Hz (255*59.7275)
// sous peine de deborder ce champ et de corrompre le flux (paquets
// audio ET commandes qui suivent, tout le lecteur AudioRxState cote
// Teensy se desynchronise). 14000 Hz choisi le 2026-09-18 (etait 8000)
// -- palier A de AZ2_EMULATION_JEUX.md, avec de la marge (234
// echantillons/paquet, pas 255) -- rendu possible par le passage a
// 921600 bauds (kControlBaud ci-dessus, ~92 Ko/s bruts ; 14 kHz mono 8
// bits = 14 Ko/s, tient large). Pour aller plus haut (ex. 22050 Hz vise
// a l'origine dans AZ2_ETAT_DES_LIEUX.md), il FAUT d'abord elargir ce
// champ de longueur a 16 bits (protocole audio V2, deja note dans la
// feuille de route) -- pas fait ici, cible volontairement gardee sous
// le plafond actuel plutot que de risquer un paquet mal forme.
constexpr uint8_t kGbAudioPacketMagic = 0x01;
constexpr uint32_t kGbAudioSampleRate = 14000;
// Nombre d'echantillons envoyes par paquet -- TOUJOURS le meme (voir
// AUDIO_SAMPLES dans minigb_apu.h cote ESP32, meme formule reprise ici
// pour que le Teensy puisse verifier la longueur recue SANS dependre de
// ce header ESP32-only). A garder synchronise a la main si
// kGbAudioSampleRate change (voir aussi le plafond ~15200 Hz documente
// plus haut). Utilise cote Teensy comme garde-fou anti-desynchronisation
// (voir AudioRxState/readStream dans src_teensy/az2_audio/main.cpp,
// meme bug/correctif que kScopeSamplesPerPacket cote ESP32 -- trouve le
// 2026-09-18, voir AZ2_ETAT_DES_LIEUX.md).
constexpr uint8_t kGbAudioSamplesPerPacket = static_cast<uint8_t>(kGbAudioSampleRate / (4194304.0 / 70224.0));

// ---------------------------------------------------------------------
// Audio GB V2 — CONTRAT PREPARE, NON ACTIVE SUR LE FIL.
// Le V1 ci-dessus reste le format utilise par les deux firmwares. Ces
// constantes/helpers permettent de developper et tester le futur format
// sans basculer une seule extremite par erreur.
//
// Frame V2 proposee :
// [magic=0x03][version=2][flags][format][seqLE16][payloadLenLE16]
// [sampleRateLE16][payload...][crc16LE]
// CRC-16/CCITT-FALSE sur version..fin payload (magic et CRC exclus).
// flags bit0 = stereo. format 1=PCM_U8, 2=PCM_S16LE.
// ---------------------------------------------------------------------
// Pilot disabled until full ESP32+Teensy hardware qualification; setting
// this true on BOTH boards still requires explicit READY handshake.
constexpr bool kGbAudioV2PilotEnabled = false;
constexpr const char *kGbAudioV2Query = "GBV2:QUERY";
constexpr const char *kGbAudioV2Ready = "GBV2:READY";
constexpr uint8_t kGbAudioV2Magic = 0x03;
constexpr uint8_t kGbAudioV2Version = 2;
constexpr uint8_t kGbAudioV2FlagStereo = 0x01;
constexpr uint8_t kGbAudioV2FormatPcmU8 = 1;
constexpr uint8_t kGbAudioV2FormatPcmS16Le = 2;
constexpr uint8_t kGbAudioV2HeaderBytes = 10;
constexpr uint8_t kGbAudioV2CrcBytes = 2;
constexpr uint16_t kGbAudioV2MaxPayload = 2048;

inline uint16_t readLe16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

inline void writeLe16(uint8_t *p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value & 0xFFu);
  p[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

inline uint16_t crc16CcittFalse(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

inline bool gbAudioV2HeaderSane(const uint8_t *header, size_t len) {
  if (header == nullptr || len < kGbAudioV2HeaderBytes) return false;
  if (header[0] != kGbAudioV2Magic || header[1] != kGbAudioV2Version) return false;
  const uint8_t format = header[3];
  if (format != kGbAudioV2FormatPcmU8 && format != kGbAudioV2FormatPcmS16Le) return false;
  if (header[2] & ~kGbAudioV2FlagStereo) return false;
  const uint16_t payloadLen = readLe16(header + 6);
  const uint16_t sampleRate = readLe16(header + 8);
  const uint8_t bytesPerSample = format == kGbAudioV2FormatPcmU8 ? 1 : 2;
  const uint8_t channels = (header[2] & kGbAudioV2FlagStereo) ? 2 : 1;
  return payloadLen > 0 && payloadLen <= kGbAudioV2MaxPayload &&
         payloadLen % (bytesPerSample * channels) == 0 &&
         sampleRate >= 8000 && sampleRate <= 48000;
}

// Wire encoder/decoder used by both boards. A decoder instance is owned
// by each receiving stream; feed() consumes exactly one byte. Payload is
// bounded and CRC-checked BEFORE any sample is made visible to the caller.
// On a damaged header/CRC, the following byte can start a fresh packet.
// The parser intentionally does not scan inside opaque audio payload.
struct GbAudioV2Frame {
  uint16_t sequence = 0;
  uint16_t sampleRate = 0;
  uint16_t payloadLen = 0;
  uint8_t flags = 0;
  uint8_t format = 0;
  uint8_t payload[kGbAudioV2MaxPayload] = {};
};

inline size_t encodeGbAudioV2(uint8_t *out, size_t capacity, const GbAudioV2Frame &frame) {
  if (out == nullptr || capacity < kGbAudioV2HeaderBytes + frame.payloadLen + kGbAudioV2CrcBytes)
    return 0;
  uint8_t header[kGbAudioV2HeaderBytes] = {
      kGbAudioV2Magic, kGbAudioV2Version, frame.flags, frame.format, 0, 0, 0, 0, 0, 0};
  writeLe16(header + 4, frame.sequence);
  writeLe16(header + 6, frame.payloadLen);
  writeLe16(header + 8, frame.sampleRate);
  if (!gbAudioV2HeaderSane(header, sizeof(header))) return 0;
  for (size_t i = 0; i < sizeof(header); ++i) out[i] = header[i];
  for (size_t i = 0; i < frame.payloadLen; ++i) out[sizeof(header) + i] = frame.payload[i];
  const uint16_t crc = crc16CcittFalse(out + 1, sizeof(header) - 1 + frame.payloadLen);
  writeLe16(out + sizeof(header) + frame.payloadLen, crc);
  return sizeof(header) + frame.payloadLen + kGbAudioV2CrcBytes;
}

struct GbAudioV2Decoder {
  uint8_t header[kGbAudioV2HeaderBytes] = {};
  uint16_t position = 0;
  uint16_t expected = 0;
  uint16_t receivedCrc = 0;
  uint32_t rejectedHeaders = 0;
  uint32_t rejectedCrc = 0;
  uint32_t timeouts = 0;
  GbAudioV2Frame frame;

  void reset() {
    position = 0;
    expected = 0;
    receivedCrc = 0;
  }
  void timeout() {
    if (position != 0) ++timeouts;
    reset();
  }
  // true: exactly one validated frame has just completed.
  bool feed(uint8_t byte) {
    if (position == 0) {
      if (byte != kGbAudioV2Magic) return false;
      header[0] = byte;
      position = 1;
      return false;
    }
    if (position < kGbAudioV2HeaderBytes) {
      header[position++] = byte;
      if (position == kGbAudioV2HeaderBytes) {
        if (!gbAudioV2HeaderSane(header, sizeof(header))) {
          ++rejectedHeaders;
          reset();
          return false;
        }
        frame.flags = header[2];
        frame.format = header[3];
        frame.sequence = readLe16(header + 4);
        frame.payloadLen = readLe16(header + 6);
        frame.sampleRate = readLe16(header + 8);
        expected = static_cast<uint16_t>(kGbAudioV2HeaderBytes + frame.payloadLen +
                                         kGbAudioV2CrcBytes);
      }
      return false;
    }
    if (position < kGbAudioV2HeaderBytes + frame.payloadLen) {
      frame.payload[position - kGbAudioV2HeaderBytes] = byte;
      ++position;
      return false;
    }
    if (position == kGbAudioV2HeaderBytes + frame.payloadLen) {
      receivedCrc = byte;
      ++position;
      return false;
    }
    receivedCrc |= static_cast<uint16_t>(byte) << 8;
    // Compute CRC incrementally over the header (excluding magic) and
    // payload. Equivalent to crc16CcittFalse() over contiguous wire bytes.
    uint16_t crc = 0xFFFFu;
    for (size_t i = 1; i < kGbAudioV2HeaderBytes; ++i) {
      crc ^= static_cast<uint16_t>(header[i]) << 8;
      for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                             : static_cast<uint16_t>(crc << 1);
    }
    for (size_t i = 0; i < frame.payloadLen; ++i) {
      crc ^= static_cast<uint16_t>(frame.payload[i]) << 8;
      for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                             : static_cast<uint16_t>(crc << 1);
    }
    const uint16_t expectedCrc = receivedCrc;
    reset();
    if (crc != expectedCrc) {
      ++rejectedCrc;
      return false;
    }
    return true;
  }
};

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
// Ajoute le 2026-09-18 ("on va mettre en route le sampleur") -- lecture
// PCM 16 bits mono avec suivi de note (AudioPlaySampler, voir
// az2_sampler.h cote Teensy), pas une bibliotheque de synthese comme
// les 5 precedents. PSRAM/carte SD deja anticipes au niveau materiel
// (voir external_psram_size/psramTestBuffer dans main.cpp) mais jamais
// exploites avant ce soir -- voir docs/AZ2_SAMPLEUR.md.
constexpr uint8_t kEngineSampler = 5;
constexpr uint8_t kEngineCount = 6;

constexpr const char *kEngineNames[kEngineCount] = {"DEXED", "EPIANO", "BRAIDS", "KARPLUS", "ANALOG", "SAMPLER"};

constexpr uint8_t kDexedPatchCount = 255;
// 255 des 256 vrais patches d'usine du Yamaha DX7 original (banques
// ROM1-ROM4, A et B) -- voir src_teensy/az2_audio/az2_dexed_bank_data.h
// pour les octets/le commentaire complet (meme source, meme ordre,
// MEME TAILLE que cote Teensy -- les deux DOIVENT rester synchronises
// a la main). 255 et PAS 256 : 0xFF (255) est deja le sentinel "pas
// d'override de patch par pas" (voir stepPatch/seqStepPatch) -- un
// vrai patch DEXED numero 255 serait indiscernable de ce sentinel, le
// tout dernier patch de ROM4B ("EXPLOSION") est donc volontairement
// laisse de cote.
constexpr const char *kDexedPatchNames[kDexedPatchCount] = {
    "BRASS   1", "BRASS   2", "BRASS   3", "STRINGS 1", "STRINGS 2", "STRINGS 3", "ORCHESTRA", 
    "PIANO   1", "PIANO   2", "PIANO   3", "E.PIANO 1", "GUITAR  1", "GUITAR  2", "SYN-LEAD 1", 
    "BASS    1", "BASS    2", "E.ORGAN 1", "PIPES   1", "HARPSICH 1", "CLAV    1", "VIBE    1", 
    "MARIMBA", "KOTO", "FLUTE   1", "ORCH-CHIME", "TUB BELLS", "STEEL DRUM", "TIMPANI", 
    "REFS WHISL", "VOICE   1", "TRAIN", "TAKE OFF", "PIANO   4", "PIANO   5", "E.PIANO 2", 
    "E.PIANO 3", "E.PIANO 4", "PIANO 5THS", "CELESTE", "TOY PIANO", "HARPSICH 2", "HARPSICH 3", 
    "CLAV    2", "CLAV    3", "E.ORGAN 2", "E.ORGAN 3", "E.ORGAN 4", "E.ORGAN 5", "PIPES   2", 
    "PIPES   3", "PIPES   4", "CALIOPE", "ACCORDION", "SITAR", "GUITAR  3", "GUITAR  4", 
    "GUITAR  5", "GUITAR  6", "LUTE", "BANJO", "HARP    1", "HARP    2", "BASS    3", "BASS    4", 
    "PICCOLO", "FLUTE   2", "OBOE", "CLARINET", "SAX BC", "BASSOON", "STRINGS 4", "STRINGS 5", 
    "STRINGS 6", "STRINGS 7", "STRINGS 8", "BRASS   4", "BRASS   5", "BRASS 6 BC", "BRASS   7", 
    "BRASS   8", "RECORDER", "HARMONICA1", "HRMNCA2 BC", "VOICE   2", "VOICE   3", "GLOKENSPL", 
    "VIBE    2", "XYLOPHONE", "CHIMES", "GONG    1", "GONG    2", "BELLS", "COW BELL", "BLOCK", 
    "FLEXATONE", "LOG DRUM", "SYN-LEAD 2", "SYN-LEAD 3", "SYN-LEAD 4", "SYN-LEAD 5", "SYN-CLAV 1", 
    "SYN-CLAV 2", "SYN-CLAV 3", "SYN-PIANO", "SYNBRASS 1", "SYNBRASS 2", "SYNORGAN 1", 
    "SYNORGAN 2", "SYN-VOX", "SYN-ORCH", "SYN-BASS 1", "SYN-BASS 2", "HARP-FLUTE", "BELL-FLUTE", 
    "E.P-BRS BC", "T.BL-EXPA", "CHIME-STRG", "B.DRM-SNAR", "SHIMMER", "EVOLUTION", "WATER GDN", 
    "WASP STING", "LASER GUN", "DESCENT", "OCTAVE WAR", "GRAND PRIX", "ST.HELENS", "EXPLOSION", 
    "FLUTE   1", "HARPSICH 1", "STRG ENS 1", "BRIGHT BOW", "BRASSHORNS", "BR TRUMPET", "MARIMBA", 
    "E.PIANO 1", "PIANO   1", "PIPES   1", "E.ORGAN 1", "E.BASS  1", "CLAV    1", "HARMONICA1", 
    "JAZZ GUIT1", "PRC SYNTH1", "SAX BC", "FRETLESS 1", "HARP    1", "TIMPANI", "HEAVYMETAL", 
    "STEEL DRUM", "SYN-LEAD 1", "VOICES BC", "CLAV ENS", "LASERSWEEP", "TUB ERUPT", "GRAND PRIX", 
    "REFS WHISL", "TRAIN", "BRASS S H", "TAKE OFF", "PIANO   2", "E.GRAND 1", "E.GRAND 2", 
    "HONKY TONK", "E.PIANO 2", "E.PIANO 3", "E.PIANO 4", "CELESTE", "FUNK CLAV", "CLAV ENS 2", 
    "PERC CLAV", "HARPSICH 2", "E.ORGAN 2", "E.ORGAN 3", "60-S ORGAN", "PIPES   2", "PIPES   3", 
    "CALIOPE", "ACCORDION", "TOY PIANO", "SITAR", "KOTO", "JAZZ GUIT2", "SPANISHGTR", "FOLK GUIT", 
    "LUTE", "BANJO", "CLAS.GUIT", "HARP    2", "E.BASS  2", "FRETLESS 2", "PLUCK BASS", "PICCOLO", 
    "FLUTE   2", "OBOE", "CLARINET", "BASSOON", "PAN FLUTE", "LEAD BRASS", "HORNS", "SOLO TBONE", 
    "BRASS BC", "BRASS 5THS", "SYNTHBRASS", "STRG QRT 1", "STRG ENS 2", "VIOLA SECN", "STRGS LOW", 
    "HIGH STRGS", "PIZZ STGS", "STG CRSNDO", "STGS 5THS", "BELLS", "TUB BELLS", "RECORDERS", 
    "CHIMES", "VOICES", "XYLOPHONE", "COWBELL", "WOOD BLOCK", "FLEXATONE", "LOG DRUM", "GLOKENSPL", 
    "VIBE", "CLAV-E.PNO", "PERC BRASS", "PRC SYNTH2", "HARPSI-STG", "CHIME-STRG", "HARP-FLUTE", 
    "BELL-FLUTE", "STRG-CHIME", "STRG-MARIM", "STRG-PIZZT", "ORCHESTRA", "LEAD GUITR", "PIANO-BRS", 
    "BRS-CHIME", "B.DRM-SNAR", "E.P-BRS BC", "ORG-BRS BC", "CLV-BRS BC", "WHISTLES", "FILTER SWP", 
    "FUNKY RISE", "WILD BOAR", "SHIMMER", "EVOLUTION", "WATER GDN", "WASP STING", "MULTI NOTE", 
    "DESCENT", "OCTAVE WAR", "..GOTCHA..", "ST.HELENS",
};

constexpr uint8_t kEPianoPatchCount = 5;
constexpr const char *kEPianoPatchNames[kEPianoPatchCount] = {
    "Default", "Bright", "Mellow", "Autopan", "Tremolo",
};

// 43 depuis le 2026-09-18 ("recuperer un max de patch pour tout les
// moteur") -- Synth_Braids expose 43 algorithmes utilisables
// (MacroOscillatorShape, voir settings.h : WAVETABLES/QUESTION_MARK/
// YOUR_ALGO restent commentes dans la lib elle-meme, donc exclus ici
// aussi), pas les 8 choisis a la main le 2026-09-15. Meme ordre que
// l'enum d'origine -- voir kBraidsShapeValues cote Teensy (main.cpp),
// qui reference les VRAIES constantes nommees (MACRO_OSC_SHAPE_*) au
// lieu de deviner leur valeur numerique.
constexpr uint8_t kBraidsPatchCount = 43;
constexpr const char *kBraidsPatchNames[kBraidsPatchCount] = {
    "CSAW", "Morph", "Saw/Square", "Sine/Triangle", "Buzz",
    "Square Sub", "Saw Sub", "Square Sync", "Saw Sync", "Triple Saw",
    "Triple Square", "Triple Triangle", "Triple Sine", "Triple RingMod", "Saw Swarm",
    "Saw Comb", "Toy",
    "Filtre LP", "Filtre Peak", "Filtre BP", "Filtre HP", "Vosim",
    "Vowel", "Vowel FOF",
    "Harmonics",
    "FM", "Feedback FM", "Chaotic FM",
    "Plucked", "Bowed", "Blown", "Fluted", "Struck Bell", "Struck Drum",
    "Kick", "Cymbal", "Snare",
    "Filtered Noise", "Twin Peaks Noise", "Clocked Noise", "Granular Cloud", "Particle Noise",
    "Digital Mod",
};

// AudioSynthKarplusStrong n'expose aucun parametre de forme (juste
// noteOn(freq,vel)/noteOff()) -- un seul "patch" possible avec cet objet.
constexpr uint8_t kKarplusPatchCount = 1;
constexpr const char *kKarplusPatchNames[kKarplusPatchCount] = {"Corde pincee"};

// AudioSynthWaveform : la forme d'onde EST le patch (WAVEFORM_* dans
// synth_waveform.h). 11 depuis le 2026-09-18 ("recuperer un max de
// patch pour tout les moteur"), pas les 4 classiques de depart --
// ajoute Pulse/Dent de scie inversee/Sample & Hold/Triangle variable,
// plus les versions "bandlimited" (anti-repliement, meilleure qualite
// que les versions simples pour les memes formes) de dent de
// scie/carre/pulse. WAVEFORM_ARBITRARY volontairement exclu (demande
// une table d'onde fournie separement, pas juste un choix d'enum).
// Voir applyTrackPatch() cote Teensy pour la correspondance avec les
// constantes WAVEFORM_*.
constexpr uint8_t kAnalogPatchCount = 11;
constexpr const char *kAnalogPatchNames[kAnalogPatchCount] = {
    "Sinus", "Dent de scie", "Carre", "Triangle", "Pulse",
    "Dent inversee", "Sample & Hold", "Triangle var.",
    "Dent BL", "Carre BL", "Pulse BL",
};

// SAMPLER (2026-09-18) : 2 samples de depart embarques en flash (voir
// az2_sampler_data.h cote Teensy), deja vendores dans le repo avec
// MicroDexed-touch (meme licence GPLv3, voir AZ2_LICENCES.md) --
// d'autres pourront s'ajouter (SD/PSRAM, voir docs/AZ2_SAMPLEUR.md)
// sans que cette table grandisse necessairement au meme rythme (un
// index au-dela de kSamplerPatchCount reste gere -- voir
// applyTrackPatch() cote Teensy).
constexpr uint8_t kSamplerPatchCount = 3;
constexpr uint8_t kSamplerGbCapturePatch = 2;
constexpr const char *kSamplerPatchNames[kSamplerPatchCount] = {"Kick", "Snare", "GB Capture"};

// uint16_t (pas uint8_t) depuis le passage de DEXED a 256 patches
// (2026-09-18, voir kDexedPatchCount plus haut) -- un uint8_t aurait
// tronque 256 en 0, transformant tout "% count" en division par zero
// (comportement indefini) partout ou ce compte sert a boucler sur les
// patches (voir les appelants, ESP32 ET Teensy).
inline uint16_t enginePatchCount(uint8_t engine) {
  switch (engine) {
    case kEngineDexed: return kDexedPatchCount;
    case kEngineEPiano: return kEPianoPatchCount;
    case kEngineBraids: return kBraidsPatchCount;
    case kEngineKarplus: return kKarplusPatchCount;
    case kEngineAnalog: return kAnalogPatchCount;
    case kEngineSampler: return kSamplerPatchCount;
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
    case kEngineSampler: return patch < kSamplerPatchCount ? kSamplerPatchNames[patch] : "?";
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

// ---------------------------------------------------------------------
// Conditions de declenchement par pas (trig conditions, cf. Elektron) --
// ajoute le 2026-09-17 ("on travaille le tracker on fait un truc qui
// eclate tout", fonction la plus citee dans l'etude concurrence face a
// Elektron/M8). PROB:<piste>:<pas>:<0-100> (probabilite en %) et
// COND:<piste>:<pas>:<octet ci-dessous> sont geres cote Teensy
// (handleProbCommand()/handleCondCommand() dans src_teensy/az2_audio/
// main.cpp) ; l'encodage vit ici pour que l'ESP32 puisse afficher les
// memes labels sans dupliquer la logique.
//
// Encodage d'un octet de condition (0-255) :
//   kStepCondAlways (0)   = aucune condition, comportement d'origine.
//   1..0x88 (nibbles)     = "K:N" -- ne joue que la Keme fois sur N
//                           passages du pattern (nibble bas = K, nibble
//                           haut = N, 1 <= K <= N <= 8). Le compteur de
//                           passages (voir patternLoopCount cote Teensy)
//                           avance de 1 a chaque redemarrage du pattern
//                           (currentStep revient a 0), commun a TOUS les
//                           pas/pistes -- pas un compteur par pas.
//   kStepCondFill (0xFE)  = ne joue que si un "fill" est actif (FILL:1).
//   kStepCondNotFill(0xFF)= ne joue que si PAS de fill actif.
constexpr uint8_t kStepCondAlways = 0;
constexpr uint8_t kStepCondFill = 0xFE;
constexpr uint8_t kStepCondNotFill = 0xFF;

inline uint8_t stepConditionEncode(uint8_t k, uint8_t n) {
  if (n < 1 || n > 8 || k < 1 || k > n) {
    return kStepCondAlways;  // entree hors bornes -> ne bloque jamais plutot que produire un octet invalide
  }
  return static_cast<uint8_t>((n << 4) | k);
}

inline bool stepConditionMet(uint8_t cond, uint32_t loopCount, bool fillActive) {
  if (cond == kStepCondAlways) {
    return true;
  }
  if (cond == kStepCondFill) {
    return fillActive;
  }
  if (cond == kStepCondNotFill) {
    return !fillActive;
  }
  const uint8_t n = static_cast<uint8_t>(cond >> 4);
  const uint8_t k = static_cast<uint8_t>(cond & 0x0F);
  if (n == 0 || k == 0 || k > n) {
    return true;  // octet jamais produit par stepConditionEncode(), mais ne bloque jamais si recu tel quel
  }
  return (loopCount % n) == static_cast<uint32_t>(k - 1);
}

// Cycle ordonne propose cote UI (ESP32, voir seqDetailCol) -- ALWAYS, les
// ratios classiques jusqu'a 4 (comme un premier jeu de trig conditions
// Elektron), puis FILL/NOT FILL. Volontairement PAS tous les octets
// valides (jusqu'a 8:8) pour rester rapide a parcourir a l'ecran ; rien
// n'empeche d'envoyer un octet en dehors de ce cycle directement en COND:.
constexpr uint8_t kStepConditionCycle[] = {
    kStepCondAlways,
    0x21, 0x22,              // 1:2, 2:2
    0x31, 0x32, 0x33,        // 1:3, 2:3, 3:3
    0x41, 0x42, 0x43, 0x44,  // 1:4, 2:4, 3:4, 4:4
    kStepCondFill, kStepCondNotFill,
};
constexpr uint8_t kStepConditionCycleCount = sizeof(kStepConditionCycle) / sizeof(kStepConditionCycle[0]);

inline void stepConditionLabel(uint8_t cond, char *out, size_t outSize) {
  if (cond == kStepCondAlways) {
    snprintf(out, outSize, "---");
  } else if (cond == kStepCondFill) {
    snprintf(out, outSize, "FILL");
  } else if (cond == kStepCondNotFill) {
    snprintf(out, outSize, "!FIL");
  } else {
    const uint8_t n = static_cast<uint8_t>(cond >> 4);
    const uint8_t k = static_cast<uint8_t>(cond & 0x0F);
    snprintf(out, outSize, "%d:%d", k, n);
  }
}

} // namespace az2
