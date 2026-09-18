#pragma once

// AudioPlaySampler : lecture d'un sample PCM 16 bits mono FIXE (flash
// ou PSRAM -- les deux sont directement adressables sur cette
// architecture, pas besoin de pgm_read_word comme sur AVR) avec suivi
// de note (resampling lineaire). Un seul coup (one-shot), pas de
// bouclage -- pense pour de la percussion/un instrument sample
// courte-duree, pas pour une nappe tenue. Ajoute le 2026-09-18 ("on va
// mettre en route le sampleur"), voir docs/AZ2_SAMPLEUR.md.
//
// A INCLURE DEPUIS main.cpp, DANS le meme bloc "namespace { ... }" et
// APRES la definition de midiNoteToFreq() -- cette classe l'utilise
// directement (pas de re-declaration ici, evite de dupliquer la
// formule).
//
// Resampling lineaire simple (pas de filtre anti-repliement) : suffit
// largement pour ce type de contenu (percussion, coups courts) --
// largement dans la marge CPU mesuree le 2026-09-18 (7-11% avec 8
// voix, tous moteurs confondus). Un vrai sampleur "hi-fi" (filtre
// polyphase, type Resampler.h deja present dans la lib Audio Teensy
// mais pense pour de l'ajustement fin de cadence d'horloge, pas du
// pitch-shift sur plusieurs octaves) resterait une piste d'amelioration
// future si le besoin se fait sentir, pas un prerequis pour un premier
// sampleur qui sonne.
class AudioPlaySampler : public AudioStream {
 public:
  AudioPlaySampler() : AudioStream(0, nullptr) {}

  // sampleData/sampleLen : buffer PCM 16 bits mono 44.1kHz (flash
  // PROGMEM OU PSRAM). rootNote : note MIDI a laquelle ce sample doit
  // jouer a sa vitesse d'origine (pas de resampling, step=1.0).
  void setSample(const int16_t *data, uint32_t len, uint8_t rootNote) {
    sampleData_ = data;
    sampleLen_ = len;
    rootNote_ = rootNote;
  }

  bool hasSample() const { return sampleData_ != nullptr && sampleLen_ > 1; }

  void noteOn(uint8_t note, uint8_t velocity) {
    if (!hasSample()) {
      return;
    }
    pos_ = 0.0f;
    step_ = midiNoteToFreq(note) / midiNoteToFreq(rootNote_);
    amp_ = static_cast<float>(velocity) / 127.0f;
    playing_ = true;
  }

  // One-shot : laisse le sample se terminer naturellement (comme un
  // vrai coup de batterie/percussion) -- noteOff() ne coupe rien,
  // coherent avec le contenu de depart (Kick/Snare). Existe surtout
  // pour la symetrie d'API avec les 5 autres moteurs (trackNoteOff()
  // cote appelant reste un seul chemin generique).
  void noteOff() {}

  virtual void update(void) {
    audio_block_t *block = allocate();
    if (block == nullptr) {
      return;
    }
    if (!playing_) {
      release(block);
      return;
    }
    int i = 0;
    for (; i < AUDIO_BLOCK_SAMPLES; ++i) {
      const uint32_t idx = static_cast<uint32_t>(pos_);
      if (idx + 1 >= sampleLen_) {
        playing_ = false;
        break;
      }
      const float frac = pos_ - static_cast<float>(idx);
      const float s0 = static_cast<float>(sampleData_[idx]);
      const float s1 = static_cast<float>(sampleData_[idx + 1]);
      const float interpolated = s0 + (s1 - s0) * frac;
      block->data[i] = static_cast<int16_t>(constrain(interpolated * amp_, -32768.0f, 32767.0f));
      pos_ += step_;
    }
    // Fin de sample en cours de bloc (i < AUDIO_BLOCK_SAMPLES) : le
    // reste du bloc doit rester a zero, pas garder les octets
    // precedents du pool partage (voir allocate(), le contenu n'est
    // pas garanti a zero).
    for (; i < AUDIO_BLOCK_SAMPLES; ++i) {
      block->data[i] = 0;
    }
    transmit(block);
    release(block);
  }

 private:
  const int16_t *sampleData_ = nullptr;
  uint32_t sampleLen_ = 0;
  uint8_t rootNote_ = 60;
  float pos_ = 0.0f;
  float step_ = 1.0f;
  float amp_ = 1.0f;
  bool playing_ = false;
};
