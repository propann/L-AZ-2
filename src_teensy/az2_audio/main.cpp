// AZ-2 - Moteur audio Teensy v1 : multi-voix + sequenceur 16 pas.
//
// Architecture reprise de MicroDexed-touch (voir
// src_teensy/microdexed-touch/MicroDexed-touch/config.h: NUM_DEXED=4,
// sequenceur multi-pistes), adaptee a l'echelle AZ-2 v0 -- voir
// docs/AZ2_PORTAGE_MICRODEXED_TOUCH.md, "Plan de portage" etape 5:
//   - 4 voix "pistes" (une instance Dexed chacune) pour le sequenceur.
//   - 1 voix "live" dediee au jeu au clavier (pads/page AUDIO ecran),
//     separee du sequenceur pour ne pas se marcher dessus.
//   - sequenceur 16 pas x 4 pistes, une note fixe par piste pour l'instant
//     (edition de note par pas = futur, cf doc de portage).
//
// Lib Synth_Dexed deja vendored dans le repo:
// src_teensy/microdexed-touch/third-party/Synth_Dexed (voir
// platformio.ini: lib_extra_dirs pour master_teensy).

#include <Arduino.h>
#include <Audio.h>
#include <AZ2_Protocol.h>
#include <Encoder.h>
#include <SD.h>
#include <synth_dexed.h>
#include <synth_mda_epiano.h>
#include <synth_braids.h>
#include <malloc.h>  // mallinfo() -- voir checkHeap(), diagnostic 2026-09-18

// Rempli par le coeur Teensyduino au boot (startup.c) en sommant les 2
// puces PSRAM soudees au dos du Teensy 4.1 : 0 si aucune detectee, sinon
// leur taille totale en Mo (ex: 16 pour 2x8MB). Sert de base au sampleur
// (voir docs/AZ2_SAMPLEUR.md).
extern "C" uint8_t external_psram_size;

// Zone reservee en PSRAM pour de futurs samples -- pour l'instant juste un
// test de detection/continuite, pas encore utilisee par un lecteur audio.
EXTMEM uint8_t psramTestBuffer[1024];

namespace {

void checkPsram();  // definie plus bas, utilisee par handleCommand ("PSRAM?")
void checkHeap();   // definie plus bas, utilisee par handleCommand ("HEAP?")
void checkHeapTest(uint32_t bytes);  // definie plus bas, utilisee par handleCommand ("HEAPTEST:")

// 8 (au lieu de 4) depuis la demande du 2026-09-14 ("on peut augmenter
// les pistes monter a 8") -- performance mesuree reelle avant/apres ce
// changement, voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md.
constexpr uint8_t kTrackCount = 8;
constexpr uint8_t kStepCount = 16;
constexpr uint8_t kNotesPerTrack = 2;  // polyphonie legere par piste (accords)
constexpr uint8_t kLiveNotes = 4;      // polyphonie de la voix "jeu au clavier"

// --- Voix moteur : chaque piste a SES 3 instances de moteur (Dexed,
// EPiano, Braids) toujours creees, mais UNE SEULE connectee au mixeur a
// la fois (voir setTrackEngine()) -- selection dynamique demandee le
// 2026-09-14 ("les moteurs audio ne sont pas selectionnables ni
// reglables ... faut faire un truc propre"), voir
// AZ2_FEUILLE_DE_ROUTE_MOTEUR.md etape 5. Choix technique : rebrancher le
// graphe audio a l'usage (AudioConnection::connect()/disconnect(), API
// officielle de la lib Audio pour du patch runtime) plutot que de garder
// les 3 moteurs branches en permanence avec un gain a 0 -- la lib Audio
// ne fait tourner update() QUE sur les objets "actifs" (au moins une
// connexion), donc un moteur non selectionne ne consomme AUCUN CPU (voir
// AudioStream.cpp: software_isr() -> "if (p->active) p->update();").
AudioSynthDexed trackDexedEngine[kTrackCount] = {
    AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE), AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE),
    AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE), AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE),
    AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE), AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE),
    AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE), AudioSynthDexed(kNotesPerTrack, SAMPLE_RATE),
};
AudioSynthEPiano trackEPianoEngine[kTrackCount] = {
    AudioSynthEPiano(kNotesPerTrack), AudioSynthEPiano(kNotesPerTrack),
    AudioSynthEPiano(kNotesPerTrack), AudioSynthEPiano(kNotesPerTrack),
    AudioSynthEPiano(kNotesPerTrack), AudioSynthEPiano(kNotesPerTrack),
    AudioSynthEPiano(kNotesPerTrack), AudioSynthEPiano(kNotesPerTrack),
};
AudioSynthBraids trackBraidsEngine[kTrackCount];  // pas de parametre de constructeur
AudioSynthKarplusStrong trackKarplusEngine[kTrackCount];  // corde pincee, pas de parametre non plus
// Moteur "Analogique" = oscillateur continu (comme Braids) + enveloppe
// ADSR standard -- 2 objets chaines en permanence par piste (le "moteur"
// selectionnable, cote patchTrackIn[], c'est la SORTIE de l'enveloppe,
// pas l'oscillateur directement).
AudioSynthWaveform trackAnalogWave[kTrackCount];
// BUG REEL trouve le 2026-09-18 (retour utilisateur : "du bruit blanc"
// identique sur DEXED/EPIANO/BRAIDS/KARPLUS, mais ANALOG toujours
// propre) -- isole en direct : fermer le filtre (FILT:) fait
// disparaitre le bruit, l'ouvrir a moitie laisse une "courte note"
// audible sous des "petits clacs". Les 4 moteurs autres qu'ANALOG
// alimentaient trackFilter[] DIRECTEMENT (patchTrackIn[], voir
// setTrackEngine() plus bas) -- sans passage par une enveloppe, RIEN
// ne garantit un signal a zero strict entre les notes. AudioEffect-
// Envelope, lui, retombe a zero EXACT au repos (voir trackAnalogEnv[]
// ci-dessous) : c'est pour ca qu'ANALOG (le seul a passer par une
// enveloppe avant le filtre) restait propre -- le filtre resonant
// (AudioFilterStateVariable, boucle a retroaction) accumule/colore le
// moindre residu non-nul en continu, jusqu'a produire ce bruit.
// Fix : trackAnalogEnv[] devient une enveloppe PARTAGEE par les 5
// moteurs (pas juste ANALOG) -- patchTrackIn[] alimente maintenant
// TOUJOURS l'enveloppe (jamais le filtre directement), et
// trackNoteOn()/trackNoteOff() declenchent cette enveloppe pour TOUS
// les moteurs, pas seulement ANALOG (voir plus bas). L'ancienne
// connexion fixe oscillateur->enveloppe (patchAnalogEnv[]) disparait :
// patchTrackIn[] s'en charge desormais dynamiquement, comme pour les
// 4 autres moteurs. La connexion FIXE enveloppe->filtre
// (patchEnvToFilter[]) est declaree plus bas, apres trackFilter[].
AudioEffectEnvelope trackAnalogEnv[kTrackCount];
AudioSynthDexed liveVoice(kLiveNotes, SAMPLE_RATE);   // voix live (pads/ecran), pas concernee par le choix de moteur

constexpr float kBraidsActiveGain = 0.5f;  // meme niveau que les autres pistes

float midiNoteToFreq(uint8_t note) {
  return 440.0f * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

// AZ2_ROLE_AUDIO -> az2_sampler.h utilise midiNoteToFreq() defini
// juste au-dessus, doit donc etre inclus APRES (voir le commentaire
// en tete de ce header). Les 2 tableaux PROGMEM de depart (Kick/Snare)
// sont dans un header de donnees separe -- purement des tableaux
// constants, aucune dependance a l'ordre d'inclusion.
#include "az2_sampler_data.h"
#include "az2_sampler.h"

// 6e moteur (2026-09-18, "on va mettre en route le sampleur") -- voir
// kEngineSampler dans AZ2_Protocol.h et applyTrackPatch() plus bas
// pour le choix Kick/Snare par piste.
AudioPlaySampler trackSamplerEngine[kTrackCount];

// AudioMixer4 n'a que 4 entrees : avec 8 pistes il en faut 2 (groupe A =
// pistes 0-3, groupe B = pistes 4-7), combinees dans mixFinal avec la
// voix live -- voir trackGroupMixer()/trackGroupChannel() plus bas.
AudioMixer4 mixTracksA;  // pistes 0-3
AudioMixer4 mixTracksB;  // pistes 4-7
AudioMixer4 mixFinal;    // groupe A + groupe B + voix live (entree 3 libre)

// Bus d'effets maitre (demande du 2026-09-14 : "un mixeur general et des
// effets sur le son") -- en aval de mixFinal, pas par piste (un
// AudioEffectDelay pleine echelle coute ~350 Ko de RAM ; un seul sur le
// bus master est largement suffisant et abordable, un par piste ne le
// serait pas). mixMaster combine signal sec (0), reverb (1), delay (2)
// et le son de l'emulateur GB (3, voir gbAudioQueue plus bas -- demande
// 2026-09-15, "il faut un emulateur complet classe ... pour que le DAC
// le joue").
AudioEffectFreeverb reverbUnit;
AudioEffectDelay delayUnit;
AudioMixer4 mixMaster;
AudioOutputI2S i2sOut;

// Son de l'emulateur Game Boy (ESP32 -> Teensy, voir AZ2_Protocol.h
// "kGbAudioPacketMagic" et handleGbAudioPacket() plus bas) : ESP32
// envoie du PCM mono 8 bits a kGbAudioSampleRate Hz (8kHz, delibere --
// tient large dans le lien serie 230400 bauds) ; on le re-echantillonne
// vers 44.1kHz/16 bits et on le pousse dans cette queue, jouee comme
// n'importe quel autre "moteur" par le bus d'effets maitre (reverb/
// delay/volume s'appliquent donc dessus aussi si les potards sont
// tournes).
AudioPlayQueue gbAudioQueue;

// Oscilloscope (Teensy -> ESP32, voir AZ2_Protocol.h "kScopePacketMagic"
// et updateScope() plus bas) -- demande 2026-09-15 ("une fenetre ou on
// voit l'onde du son jouer evoluer en modifiant le patch"). AudioRecordQueue
// = tampon logiciel qui capture les blocs bruts (128 echantillons int16 a
// 44.1kHz) du point du graphe ou on la branche -- PAS connectee en
// permanence : patchScopeTap est rebranchee dynamiquement vers la sortie
// du filtre de la piste actuellement "observee" (voir handleScopeCommand()),
// desactivee (scopeQueue.end()) quand personne ne regarde -- cout CPU nul
// hors edition de patch.
AudioRecordQueue scopeQueue;
AudioConnection patchScopeTap;  // reconnectee dynamiquement, voir handleScopeCommand()
int8_t scopeTrack = -1;         // -1 = desactive

// Filtre PAR PISTE (demande 2026-09-15, "on fait un max de code ...
// tracker" -- filtre resonant classique de synthe/tracker). Un
// AudioFilterStateVariable par piste (Chamberlin SVF entier, tres bon
// marche en CPU -- verifie dans filter_variable.h/.cpp, standard PJRC,
// deja utilise a N instances dans de nombreux projets Teensy) INSERE
// entre le moteur actif de la piste et son mixeur de groupe (voir
// setTrackEngine() : patchTrackIn[] pointe maintenant vers le filtre,
// pas directement le groupe). Sortie 0 = passe-bas (lowpass), la seule
// utilisee pour l'instant (1=passe-bande, 2=passe-haut, pas exposes en
// v1). Frequence de coupure par defaut = grande ouverte (aucun filtrage
// audible tant qu'on ne touche pas FILT:, voir handleFiltCommand()).
AudioFilterStateVariable trackFilter[kTrackCount];

// Connexion FIXE enveloppe -> filtre, POUR LES 5 MOTEURS (2026-09-18,
// voir le commentaire de trackAnalogEnv[] plus haut pour le bug que ca
// corrige) -- trackAnalogEnv[track] est desormais le seul chemin vers
// trackFilter[track], quel que soit le moteur actif de la piste.
AudioConnection patchEnvToFilter[kTrackCount] = {
    AudioConnection(trackAnalogEnv[0], 0, trackFilter[0], 0), AudioConnection(trackAnalogEnv[1], 0, trackFilter[1], 0),
    AudioConnection(trackAnalogEnv[2], 0, trackFilter[2], 0), AudioConnection(trackAnalogEnv[3], 0, trackFilter[3], 0),
    AudioConnection(trackAnalogEnv[4], 0, trackFilter[4], 0), AudioConnection(trackAnalogEnv[5], 0, trackFilter[5], 0),
    AudioConnection(trackAnalogEnv[6], 0, trackFilter[6], 0), AudioConnection(trackAnalogEnv[7], 0, trackFilter[7], 0),
};

// Une connexion "prise" par piste, rebranchee vers le moteur actif de
// cette piste (voir setTrackEngine()) -- pas connectee au demarrage,
// setup() choisit le moteur par defaut de chaque piste comme n'importe
// quel autre changement. Pointe desormais vers trackAnalogEnv[track]
// (2026-09-18, TOUS moteurs -- voir plus haut), pas directement le
// filtre ni le mixeur de groupe.
AudioConnection patchTrackIn[kTrackCount];
// Connexion FIXE filtre -> groupe (channel deterministe par piste, pas
// besoin d'etre dynamique comme patchTrackIn[]) -- pistes 0-3 sur
// mixTracksA canaux 0-3, pistes 4-7 sur mixTracksB canaux 0-3 (voir
// trackGroupMixer()/trackGroupChannel()).
AudioConnection patchFilterToGroup[kTrackCount] = {
    AudioConnection(trackFilter[0], 0, mixTracksA, 0), AudioConnection(trackFilter[1], 0, mixTracksA, 1),
    AudioConnection(trackFilter[2], 0, mixTracksA, 2), AudioConnection(trackFilter[3], 0, mixTracksA, 3),
    AudioConnection(trackFilter[4], 0, mixTracksB, 0), AudioConnection(trackFilter[5], 0, mixTracksB, 1),
    AudioConnection(trackFilter[6], 0, mixTracksB, 2), AudioConnection(trackFilter[7], 0, mixTracksB, 3),
};
AudioConnection patchGroupA(mixTracksA, 0, mixFinal, 0);
AudioConnection patchGroupB(mixTracksB, 0, mixFinal, 1);
AudioConnection patchLiveIn(liveVoice, 0, mixFinal, 2);

// Metronome (2026-09-19, "il faut un bouton metronome ... pour
// activer/desactiver") -- occupe le 4e canal de mixFinal, laisse
// libre jusqu'ici (voir le commentaire de mixFinal). Un simple clic
// (sinus court, pas de sustain -- decay seul ramene a zero, pas besoin
// de noteOff() explicite) declenche depuis advanceTick() a chaque
// debut de temps (currentStep % stepsPerBeat == 0), accentue (plus
// aigu) sur le premier temps du pattern.
AudioSynthWaveform metroClick;
AudioEffectEnvelope metroEnv;
AudioConnection patchMetroEnv(metroClick, 0, metroEnv, 0);
AudioConnection patchMetroOut(metroEnv, 0, mixFinal, 3);
bool metronomeEnabled = false;

void triggerMetronome(bool accent) {
  if (!metronomeEnabled) {
    return;
  }
  metroClick.frequency(accent ? 1800.0f : 1200.0f);
  metroClick.amplitude(0.5f);
  metroEnv.noteOn();
}
AudioConnection patchFinalToMaster(mixFinal, 0, mixMaster, 0);  // signal sec
AudioConnection patchFinalToReverb(mixFinal, 0, reverbUnit, 0);
AudioConnection patchReverbToMaster(reverbUnit, 0, mixMaster, 1);
AudioConnection patchFinalToDelay(mixFinal, 0, delayUnit, 0);
AudioConnection patchDelayToMaster(delayUnit, 0, mixMaster, 2);
AudioConnection patchGbAudioToMaster(gbAudioQueue, 0, mixMaster, 3);
AudioConnection patchOutL(mixMaster, 0, i2sOut, 0);
AudioConnection patchOutR(mixMaster, 0, i2sOut, 1);

// Piste -> quel AudioMixer4 de groupe, et quel canal (0-3) dedans.
AudioMixer4 &trackGroupMixer(uint8_t track) {
  return track < 4 ? mixTracksA : mixTracksB;
}
uint8_t trackGroupChannel(uint8_t track) {
  return static_cast<uint8_t>(track % 4);
}

// Moteur et patch actuellement actifs par piste (voir
// AZ2_Protocol.h: kEngine*/kEngineCount, enginePatchCount()).
//
// BUG REEL confirme sur materiel le 2026-09-18 : DEXED produit du bruit
// au lieu d'une note des qu'on le declenche (voir AZ2_ETAT_DES_LIEUX.md,
// "diagnostic DEXED" -- isole par test binaire MUTE: piste par piste,
// pas un probleme materiel). Le vrai correctif (dans le coeur FM de
// Synth_Dexed) demande un outillage qui manque encore (SCOPE: cassee,
// voir le meme journal) -- en attendant, DEXED n'est plus le moteur par
// defaut d'AUCUNE piste : retour utilisateur explicite ("faut reparer
// ce truc que ca sonne plutot que ca fasse du bruit"). Remplace par
// ANALOG, seul moteur CONFIRME propre a l'oreille sur ce materiel (test
// isole piste 0, "ca c'est bon"). EPIANO/BRAIDS restent en defaut sur
// leurs pistes -- pas de bug signale dessus, mais pas non plus
// confirmes a l'oreille en train de jouer (seulement "pas de souffle
// residuel au repos" lors du test MUTE:) -- a surveiller. DEXED reste
// selectionnable a la main via ENGINE: pour continuer a le tester/le
// reparer, juste plus le choix qui demarre tout seul au boot.
uint8_t trackEngine[kTrackCount] = {
    az2::kEngineAnalog, az2::kEngineAnalog, az2::kEngineEPiano, az2::kEngineBraids,
    az2::kEngineAnalog, az2::kEngineAnalog, az2::kEngineEPiano, az2::kEngineBraids,
};
uint8_t trackPatch[kTrackCount] = {0, 0, 0, 0, 0, 0, 0, 0};

// Volume/mute/solo par piste (VOL:/MUTE:/SOLO:, priorites #1 et #4 de
// la liste indispensable, AZ2_BENCHMARK_CONCURRENCE.md). ATTENTION
// piege trouve le 2026-09-16 (voir AZ2_FEUILLE_DE_ROUTE.md, "piege
// mute/solo") : le gain du mixeur de groupe sert DEJA de porte note-on/
// off pour BRAIDS -- impossible de le multiplier n'importe quand sans
// risquer de casser cette porte. Solution retenue : trackNoteHeld[]
// suit si une note Braids est REELLEMENT en train de sonner sur cette
// piste ; applyGroupGainNow() (juste apres) sait alors calculer le bon
// gain dans TOUS les cas (Braids tenu, Braids au repos, autre moteur)
// et peut etre appelee a tout moment -- mute/demute est donc immediat
// meme sur une note Braids deja tenue, pas de decalage comme une
// premiere version (volume seul) l'avait accepte avant que ce
// mecanisme plus general soit trouve.
uint8_t trackVolume[kTrackCount] = {127, 127, 127, 127, 127, 127, 127, 127};
bool trackMuted[kTrackCount] = {};
bool trackSoloed[kTrackCount] = {};
bool trackNoteHeld[kTrackCount] = {};  // Braids seulement pour l'instant, voir trackNoteOn()/Off()

bool anyTrackSoloed() {
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    if (trackSoloed[t]) {
      return true;
    }
  }
  return false;
}

// Combine volume + mute + solo en UN seul multiplicateur 0.0-1.0 --
// mute gagne toujours ; sinon, si au moins une piste est soloed, seules
// les pistes soloed sont audibles ; sinon le volume normal s'applique.
float trackEffectiveGain(uint8_t track) {
  if (trackMuted[track]) {
    return 0.0f;
  }
  if (anyTrackSoloed() && !trackSoloed[track]) {
    return 0.0f;
  }
  return static_cast<float>(trackVolume[track]) / 127.0f;
}

// Recalcule et applique IMMEDIATEMENT le gain du mixeur de groupe pour
// une piste -- SUR pour Braids grace a trackNoteHeld[] (voir plus
// haut) : si aucune note n'est tenue, reste a 0 (repos naturel de ce
// moteur, ne "reveille" jamais une porte qui devrait etre fermee).
void applyGroupGainNow(uint8_t track) {
  if (trackEngine[track] == az2::kEngineBraids) {
    const float g = trackNoteHeld[track] ? kBraidsActiveGain * trackEffectiveGain(track) : 0.0f;
    trackGroupMixer(track).gain(trackGroupChannel(track), g);
  } else {
    trackGroupMixer(track).gain(trackGroupChannel(track), 0.5f * trackEffectiveGain(track));
  }
}

// Pas de vrai panoramique pour l'instant : la chaine est MONO de bout
// en bout (patchOutL/patchOutR dupliquent le meme mixMaster, voir plus
// haut) -- un vrai pan demanderait de refaire les bus en stereo, hors
// scope ici.

// 255 vrais patches d'usine du Yamaha DX7 original (banques ROM1-ROM4)
// -- voir az2_dexed_bank_data.h pour le detail complet (provenance,
// pourquoi 255 et pas 256). Remplace les 8 patches choisis a la main
// le 2026-09-17 (2026-09-18, "recuperer un max de patch pour tout les
// moteur"). GARDER LE MEME ORDRE que az2::kDexedPatchNames
// (AZ2_Protocol.h) -- l'index doit correspondre, les deux fichiers
// DOIVENT rester synchronises a la main.
#include "az2_dexed_bank_data.h"

// Les 43 formes UTILISABLES de Synth_Braids (voir settings.h:
// MacroOscillatorShape -- WAVETABLES/QUESTION_MARK/YOUR_ALGO restent
// commentes dans la lib elle-meme, exclus) -- 2026-09-18, "recuperer
// un max de patch pour tout les moteur", remplace les 8 choisies a la
// main le 2026-09-15. References par leur VRAI NOM d'enum (pas une
// valeur numerique devinee/comptee a la main -- l'enum a des trous
// dus aux formes commentees, compter aurait ete fragile) : disponibles
// ici sans include supplementaire, synth_braids.h (deja inclus plus
// haut) fait "using namespace braids;" a portee fichier. GARDER LE
// MEME ORDRE que az2::kBraidsPatchNames (AZ2_Protocol.h).
const int16_t kBraidsShapeValues[43] = {
    MACRO_OSC_SHAPE_CSAW, MACRO_OSC_SHAPE_MORPH, MACRO_OSC_SHAPE_SAW_SQUARE, MACRO_OSC_SHAPE_SINE_TRIANGLE, MACRO_OSC_SHAPE_BUZZ,
    MACRO_OSC_SHAPE_SQUARE_SUB, MACRO_OSC_SHAPE_SAW_SUB, MACRO_OSC_SHAPE_SQUARE_SYNC, MACRO_OSC_SHAPE_SAW_SYNC, MACRO_OSC_SHAPE_TRIPLE_SAW,
    MACRO_OSC_SHAPE_TRIPLE_SQUARE, MACRO_OSC_SHAPE_TRIPLE_TRIANGLE, MACRO_OSC_SHAPE_TRIPLE_SINE, MACRO_OSC_SHAPE_TRIPLE_RING_MOD, MACRO_OSC_SHAPE_SAW_SWARM,
    MACRO_OSC_SHAPE_SAW_COMB, MACRO_OSC_SHAPE_TOY,
    MACRO_OSC_SHAPE_DIGITAL_FILTER_LP, MACRO_OSC_SHAPE_DIGITAL_FILTER_PK, MACRO_OSC_SHAPE_DIGITAL_FILTER_BP, MACRO_OSC_SHAPE_DIGITAL_FILTER_HP, MACRO_OSC_SHAPE_VOSIM,
    MACRO_OSC_SHAPE_VOWEL, MACRO_OSC_SHAPE_VOWEL_FOF,
    MACRO_OSC_SHAPE_HARMONICS,
    MACRO_OSC_SHAPE_FM, MACRO_OSC_SHAPE_FEEDBACK_FM, MACRO_OSC_SHAPE_CHAOTIC_FEEDBACK_FM,
    MACRO_OSC_SHAPE_PLUCKED, MACRO_OSC_SHAPE_BOWED, MACRO_OSC_SHAPE_BLOWN, MACRO_OSC_SHAPE_FLUTED, MACRO_OSC_SHAPE_STRUCK_BELL, MACRO_OSC_SHAPE_STRUCK_DRUM,
    MACRO_OSC_SHAPE_KICK, MACRO_OSC_SHAPE_CYMBAL, MACRO_OSC_SHAPE_SNARE,
    MACRO_OSC_SHAPE_FILTERED_NOISE, MACRO_OSC_SHAPE_TWIN_PEAKS_NOISE, MACRO_OSC_SHAPE_CLOCKED_NOISE, MACRO_OSC_SHAPE_GRANULAR_CLOUD, MACRO_OSC_SHAPE_PARTICLE_NOISE,
    MACRO_OSC_SHAPE_DIGITAL_MODULATION,
};

// Formes AudioSynthWaveform choisies pour le moteur ANALOG (voir
// synth_waveform.h: WAVEFORM_*) -- GARDER LE MEME ORDRE que
// az2::kAnalogPatchNames (AZ2_Protocol.h).
// 11 depuis le 2026-09-18 ("recuperer un max de patch pour tout les
// moteur") -- voir kAnalogPatchNames (AZ2_Protocol.h) pour le detail,
// GARDER LE MEME ORDRE.
const short kAnalogWaveformValues[11] = {
    WAVEFORM_SINE, WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE, WAVEFORM_PULSE,
    WAVEFORM_SAWTOOTH_REVERSE, WAVEFORM_SAMPLE_HOLD, WAVEFORM_TRIANGLE_VARIABLE,
    WAVEFORM_BANDLIMIT_SAWTOOTH, WAVEFORM_BANDLIMIT_SQUARE, WAVEFORM_BANDLIMIT_PULSE,
};

// Charge le patch courant (trackPatch[track]) dans le moteur actuellement
// actif de la piste (trackEngine[track]). Partagee avec liveVoice (voir
// setup()) qui n'a pas de "piste" mais profite des memes patchs nommes.
void relayLine(const String &line);  // definie plus bas, voir son commentaire

void loadDexedPatch(AudioSynthDexed &engine, uint8_t patch) {
  uint8_t packed[128];
  memcpy_P(packed, kDexedFullBank[patch % az2::kDexedPatchCount], sizeof(packed));
  uint8_t unpacked[156];
  engine.decodeVoice(unpacked, packed);
  engine.loadVoiceParameters(unpacked);
}

// Previens l'UI ESP32 de l'algo/feedback DX7 embarques dans CE patch --
// sans ca, apres un changement de patch/moteur, la page PATCH afficherait
// un algo/feedback perimes (ceux du reglage DXP: precedent) tant que
// l'utilisateur n'a pas manuellement retouche les +/-. Meme esprit que
// printPatchSelect() apres setTrackEngine().
void announceDexedParams(uint8_t track) {
  char msg[24];
  snprintf(msg, sizeof(msg), "DXP:%d:0:%d", track,
           trackDexedEngine[track].getVoiceDataElement(DEXED_VOICE_OFFSET + DEXED_ALGORITHM));
  relayLine(String(msg));
  snprintf(msg, sizeof(msg), "DXP:%d:1:%d", track,
           trackDexedEngine[track].getVoiceDataElement(DEXED_VOICE_OFFSET + DEXED_FEEDBACK));
  relayLine(String(msg));
}

// Banque de samples SAMPLER (voir kSamplerPatchCount/kSamplerPatchNames
// dans AZ2_Protocol.h -- MEME ORDRE requis) -- pointe vers les
// tableaux PROGMEM de az2_sampler_data.h, aucune copie (setSample() ne
// fait que retenir le pointeur/la longueur).
struct SamplerBankEntry {
  const int16_t *data;
  uint32_t len;
  uint8_t rootNote;
};
const SamplerBankEntry kSamplerBank[az2::kSamplerPatchCount] = {
    {kSampleKick, kSampleKickLen, kSampleKickRoot},
    {kSampleSnare, kSampleSnareLen, kSampleSnareRoot},
};

void applyTrackPatch(uint8_t track) {
  const uint8_t patch = trackPatch[track];
  switch (trackEngine[track]) {
    case az2::kEngineDexed:
      loadDexedPatch(trackDexedEngine[track], patch);
      announceDexedParams(track);
      break;
    case az2::kEngineEPiano:
      trackEPianoEngine[track].setProgram(patch % az2::kEPianoPatchCount);
      break;
    case az2::kEngineBraids:
      trackBraidsEngine[track].set_braids_shape(kBraidsShapeValues[patch % az2::kBraidsPatchCount]);
      break;
    case az2::kEngineKarplus:
      break;  // AudioSynthKarplusStrong n'a pas de parametre de forme, rien a faire
    case az2::kEngineAnalog:
      trackAnalogWave[track].begin(kAnalogWaveformValues[patch % az2::kAnalogPatchCount]);
      break;
    case az2::kEngineSampler: {
      const SamplerBankEntry &entry = kSamplerBank[patch % az2::kSamplerPatchCount];
      trackSamplerEngine[track].setSample(entry.data, entry.len, entry.rootNote);
      break;
    }
  }
}

// Change le moteur actif d'une piste : coupe proprement la note en cours,
// rebranche le graphe audio (disconnect/connect -- API officielle de
// patch runtime de la lib Audio, sans danger appelee depuis loop(), voir
// AudioStream.cpp), recharge le patch 0 du nouveau moteur, et remet le
// gain mixeur de la piste a son niveau normal (Braids gere le sien tout
// seul note par note, voir trackNoteOn/trackNoteOff).
void allTrackNotesOff();  // definie plus bas, utilisee ici

void setTrackEngine(uint8_t track, uint8_t engine) {
  if (track >= kTrackCount || engine >= az2::kEngineCount) {
    return;
  }

  allTrackNotesOff();

  trackEngine[track] = engine;
  trackPatch[track] = 0;

  // patchTrackIn[] rebranche l'ENTREE de l'enveloppe partagee de la
  // piste (2026-09-18, voir le commentaire de trackAnalogEnv[]/
  // patchEnvToFilter[] plus haut -- CHAQUE moteur passe maintenant par
  // cette enveloppe avant le filtre, pas seulement ANALOG). Le filtre
  // et le groupe restent branches en permanence en aval.
  patchTrackIn[track].disconnect();
  switch (engine) {
    case az2::kEngineDexed:
      patchTrackIn[track].connect(trackDexedEngine[track], 0, trackAnalogEnv[track], 0);
      break;
    case az2::kEngineEPiano:
      patchTrackIn[track].connect(trackEPianoEngine[track], 0, trackAnalogEnv[track], 0);
      break;
    case az2::kEngineBraids:
      patchTrackIn[track].connect(trackBraidsEngine[track], 0, trackAnalogEnv[track], 0);
      break;
    case az2::kEngineKarplus:
      patchTrackIn[track].connect(trackKarplusEngine[track], 0, trackAnalogEnv[track], 0);
      break;
    case az2::kEngineAnalog:
      // L'oscillateur alimente maintenant l'enveloppe partagee via
      // patchTrackIn[] (comme les 4 autres moteurs) -- plus de
      // connexion fixe dediee (ancien patchAnalogEnv[], supprime).
      patchTrackIn[track].connect(trackAnalogWave[track], 0, trackAnalogEnv[track], 0);
      break;
    case az2::kEngineSampler:
      // Meme enveloppe partagee que les 5 autres (coherence
      // d'architecture) -- a surveiller a l'oreille : un release ADSR
      // trop court par rapport a la duree du sample (240-380ms pour
      // Kick/Snare) pourrait le couper avant la fin naturelle. Pas
      // encore un souci constate, les valeurs par defaut (voir
      // trackAnalogEnv[] au boot) laissent une marge confortable.
      patchTrackIn[track].connect(trackSamplerEngine[track], 0, trackAnalogEnv[track], 0);
      break;
  }

  applyTrackPatch(track);
  // Un changement de moteur invalide toute note precedemment tenue
  // (voir trackNoteHeld[]) -- applyGroupGainNow() calculera donc 0.0f
  // pour un nouveau moteur Braids, correct (silence tant qu'aucune
  // note n'est jouee, voir trackNoteOn()).
  trackNoteHeld[track] = false;
  applyGroupGainNow(track);
}

// Deux entrees possibles pour le protocole AZ2 (architecture a 2 cerveaux
// depuis l'abandon du Pico le 2026-09-14 -- voir
// AZ2_ARCHITECTURE_FIRMWARE_DOUBLE.md, a corriger) :
// - Serial  (USB)     : moniteur serie humain, pratique pour tester le
//   Teensy seul (AZ2_CABLAGE_BASE.md, "Tests de cablage v0", etape 2).
// - Serial1 (pins 0/1)  : lien UART vers l'ESP32_CONTROL (menu/commandes UI).
// Les deux sont lues et les reponses sont recopiees sur les deux, pour ne
// pas casser le test manuel USB quand l'UART est branchee.
// Pins 14/15 (ex-Serial3, ex-lien Pico) sont maintenant reutilisees en
// entrees analogiques pour les potentiometres, voir updateLocalControls().
String usbLine;
String espLine;
uint32_t lastStatusMs = 0;
bool playing = false;

// Gamme chromatique sur les 16 pads (voix live) -- kPadBaseNote
// PARTAGE avec l'ESP32 depuis le 2026-09-17 (voir AZ2_Protocol.h,
// utilise aussi pour "poser" une note de pad sur un pas du sequenceur).
// Transpose ajustable via MACRO: (protocole toujours la, voir
// handleMacroCommand()) mais plus aucune source ne l'envoie depuis
// l'abandon du Pico (2026-09-14) -- reste a 0 tant qu'un futur controle
// (encodeur/gachette C-D) n'est pas branche dessus.
int8_t transposeSemitones = 0;

uint8_t padToMidiNote(uint8_t pad) {
  return static_cast<uint8_t>(az2::kPadBaseNote + pad + transposeSemitones);
}

void announceLed(uint8_t pad, const char *state) {
  az2::printLedEvent(Serial, pad, state);
  az2::printLedEvent(Serial1, pad, state);
}

void announceStatus(const char *state) {
  az2::printStatus(Serial, "TEENSY_AUDIO", state);
  az2::printStatus(Serial1, "TEENSY_AUDIO", state);
}

void announceHello();  // definie plus bas (a besoin de bpm/stepsPerBeat/trackEngine)

void relayLine(const String &line) {
  Serial.println(line);
  Serial1.println(line);
}

// Point note lors de l'audit du code du 2026-09-17 : une commande mal
// formee ou hors bornes etait jusqu'ici juste ignoree en silence (return
// sans rien dire), aucun moyen de savoir cote ESP32/USB qu'une commande
// a ete rejetee. Reprend la convention deja utilisee par REC:ERROR:
// (gbRecStart()) : <COMMANDE>:ERROR:<raison>, envoye sur Serial (debug
// USB) ET Serial1 (vers l'ESP32, qui peut choisir de l'afficher ou de
// l'ignorer -- ne casse rien pour l'UI actuelle qui ignore deja les
// lignes non reconnues).
void sendCommandError(const char *command, const char *reason) {
  Serial.print(command);
  Serial.print(":ERROR:");
  Serial.println(reason);
  Serial1.print(command);
  Serial1.print(":ERROR:");
  Serial1.println(reason);
}

// ---------------------------------------------------------------------
// Sequenceur : 16 pas x 4 pistes, une note fixe par piste pour l'instant.
// ---------------------------------------------------------------------
// Note PAR PAS (comme MicroDexed-touch: seq.note_data[pattern][step],
// voir sequencer.cpp) -- demande le 2026-09-15 ("prend le sequenceur du
// dexed touch"), remplace l'ancienne note unique fixe par piste (v0).
// Colonnes tracker (etude LSDJ/M8/Polyend, voir AZ2_TRACKER_ETUDE.md) :
// NOTE (stepNote), INST (stepPatch -- 0xFF = patch par defaut de la
// piste ; stocke/transmis mais PAS ENCORE applique en temps reel, voir
// note dans triggerStepFx()/advanceTick() plus bas -- changer un patch
// Dexed coute trop cher pour une ISR), FX+VAL (stepFx/stepFxVal).
enum StepFx : uint8_t { kStepFxNone = 0, kStepFxArp = 1, kStepFxCut = 2, kStepFxRetrig = 3 };
constexpr uint8_t kStepFxCount = 4;  // kStepFxNone..kStepFxRetrig

struct SequencerTrack {
  bool stepOn[kStepCount] = {};
  uint8_t stepNote[kStepCount] = {};    // seede par seedDefaultNotes()
  uint8_t stepPatch[kStepCount];        // 0xFF par defaut, voir seedDefaultNotes()
  uint8_t stepFx[kStepCount] = {};      // StepFx, 0 = aucun
  uint8_t stepFxVal[kStepCount] = {};   // ARP: xy (nibbles, demi-tons) ; CUT/RETRIG: nb de ticks
  // Probabilite/condition par pas (2026-09-17, "on travaille le tracker on
  // fait un truc qui eclate tout" -- fonction la plus citee dans l'etude
  // concurrence face a Elektron). Values par defaut = comportement
  // ORIGINAL exact (100 = joue toujours, 0/kStepCondAlways = aucune
  // condition) : un pattern deja sauvegarde avant cet ajout continue de
  // jouer identique tant qu'on n'y touche pas.
  uint8_t stepProb[kStepCount];         // 0-100 (%), 100 par defaut -- voir seedDefaultNotes()
  uint8_t stepCondition[kStepCount] = {};  // encode az2::stepConditionEncode()/kStepCond*, voir AZ2_Protocol.h
  uint8_t playingNote = 0;  // note reellement tenue, pour l'extinction correcte
  bool stepPlaying = false;
  // Etat d'effet du pas EN COURS (recalcule a chaque declenchement,
  // consulte/avance par l'ISR de tick -- voir advanceTick()).
  uint8_t activeFx = kStepFxNone;
  uint8_t activeFxVal = 0;
  uint8_t baseNote = 0;          // note de reference du pas (l'ARP applique ses offsets dessus)
  uint8_t ticksSinceTrigger = 0;
};
// Plusieurs patterns + chainage en "song", demande le 2026-09-16 ("il
// faut un tracker complet ... plus qu'un sequenceur qui permet
// d'assembler des patterns"). Modele le plus simple des 3 references
// etudiees (voir AZ2_TRACKER_ETUDE.md) -- comme Polyend Tracker : un
// pas de song = UN pattern complet (8 pistes ensemble), pas de chaines
// par piste independantes comme LSDJ/M8 (bien plus de travail pour peu
// de gain a ce stade).
constexpr uint8_t kPatternCount = 8;
SequencerTrack patterns[kPatternCount][kTrackCount];
// Pattern en cours d'EDITION -- STEP:/NOTE:/INST:/SFX: modifient
// toujours celui-ci, qu'il soit ou non celui qui joue reellement (on
// peut preparer un pattern pendant qu'un autre tourne, comme dans un
// vrai tracker).
uint8_t currentPattern = 0;
// Pattern reellement JOUE par l'ISR (voir advanceTick()) -- suit
// currentPattern en mode boucle simple ; suit songPatterns[songPos] en
// mode song.
uint8_t playingPattern = 0;

constexpr uint8_t kSongLength = 16;
uint8_t songPatterns[kSongLength] = {};
uint8_t songLen = 0;    // 0 = pas de song definie
bool songMode = false;  // false = boucle simple sur currentPattern (comportement d'origine)
uint8_t songPos = 0;

// Sous-decoupage du pas en "ticks" (fondation commune a ARP/CUT/RETRIG
// dans LSDJ/M8/Polyend, voir AZ2_TRACKER_ETUDE.md) -- 4 ticks/pas, assez
// pour un arpege a 3 notes (root/x/y) et un retrig audible sans
// surcharger l'IntervalTimer (deja a stepIntervalUs()/4, reste tres
// raisonnable meme aux tempos les plus rapides geres par ailleurs).
constexpr uint8_t kTicksPerStep = 4;
volatile uint8_t currentTick = 0;

// Swing/shuffle (SWING:, priorite #3 de la liste indispensable,
// AZ2_BENCHMARK_CONCURRENCE.md). Piege trouve le 2026-09-16 (voir
// AZ2_FEUILLE_DE_ROUTE.md) : reconfigurer sequencerTimer depuis sa
// propre ISR est delicat (jitter, securite). Evite completement ici --
// sequencerTimer garde sa periode FIXE pour toujours (jamais
// d'appel a .update() pour le swing) : seul le nombre de ticks qui
// composent le pas EN COURS varie (ticksForCurrentStep, recalcule a
// chaque debut de pas dans advanceTick()) -- pas pairs raccourcis, pas
// impairs allonges d'autant, tempo moyen exact sur toute paire de pas.
// 0-2 (kTicksPerStep-2 max, jamais moins de 2 ticks pour un pas -- voir
// pourquoi juste en dessous). 0 = pas de swing (comportement d'origine,
// tous les pas a kTicksPerStep pile).
//
// BUG REEL trouve lors de l'audit du code du 2026-09-17 : le max
// autorisait avant kTicksPerStep-1 (=3), ce qui pouvait raccourcir un
// pas pair a 1 SEUL tick (voir ticksForCurrentStep dans advanceTick()).
// Or triggerStepFx() (ARP/CUT/RETRIG) n'est appelee QUE dans la branche
// "sinon" (currentTick != 0) d'advanceTick() -- avec 1 seul tick,
// currentTick vaut toujours 0 pendant tout le pas, cette branche n'etait
// donc JAMAIS executee : un CUT ou un RETRIG pose sur un pas qui tombe
// raccourci au maximum ne se declenchait plus du tout ce tour-la (le
// son du pas jouait entier -- coupe seulement par l'allTrackNotesOff()
// normal du pas suivant -- au lieu d'etre coupe/redeclenche comme
// prevu). Fix : plafonner swingAmount a kTicksPerStep-2 (jamais moins de
// 2 ticks par pas) garantit au moins UN passage dans la branche "sinon"
// par pas, donc au moins une chance pour CUT/RETRIG de s'appliquer meme
// sur le pas le plus raccourci. Perd le tout dernier cran de swing le
// plus extreme (etait deja celui qui cassait CUT/RETRIG) ; a confirmer
// a l'oreille sur materiel reel (jouer un pattern avec CUT/RETRIG au
// swing maximum).
uint8_t swingAmount = 0;
uint8_t ticksForCurrentStep = kTicksPerStep;

// Horloge du sequenceur : sur IntervalTimer (interruption materielle),
// comme MicroDexed-touch (voir src_teensy/microdexed-touch/MicroDexed-touch/
// MicroDexed-touch.ino, "PeriodicTimer sequencer_timer" + dexed_sd.cpp:4823
// "sequencer_timer.begin(sequencer, seq.tempo_ms/(seq.ticks_max+1))") --
// c'est ce qui manquait cote AZ-2 : avant, le pas etait avance depuis
// loop() via un simple test millis(), donc soumis a la duree de tout ce
// que loop() fait dans le meme tour (lecture des 3 UART, etc.) -- source
// du "temps mort" ressenti. Avec IntervalTimer, l'avancement du pas (et le
// declenchement des notes) se fait dans une vraie interruption, a l'heure
// pile, quoi qu'il arrive par ailleurs dans loop().
IntervalTimer sequencerTimer;
float bpm = 120.0f;
// Division du pas (voir AZ2_Protocol.h: kDivisionOptions) -- 4 = double-
// croche (comportement d'origine, 1/16), reglable par DIV: depuis l'ecran.
uint8_t stepsPerBeat = 4;
volatile uint8_t currentStep = 0;
volatile uint16_t currentBar = 1;
// Compteur de passages du pattern (2026-09-17, PROB:/COND:) -- voir le
// commentaire dans advanceTick(), incremente uniquement dans l'ISR.
volatile uint32_t patternLoopCount = 0;
// Etat "fill" (2026-09-17, condition kStepCondFill/kStepCondNotFill) --
// pas de bouton dedie pour l'instant : pilote uniquement par FILL:0/1
// depuis l'ESP32/serie (voir handleFillCommand()). Meme convention que
// `playing` (bool simple, pas de section critique -- lecture/ecriture
// d'un bool est atomique sur Cortex-M7).
bool fillActive = false;
// L'ISR ne fait AUCUN Serial.print (trop lent/imprevisible en interruption) :
// elle se contente de positionner ce drapeau, et announceClock() reste
// appele depuis loop() (voir updateSequencer()).
volatile bool clockPending = false;

void seedDefaultNotes() {
  // Accord de depart different par piste (tous les pas a la meme note au
  // demarrage), juste pour avoir quelque chose d'audible avant edition --
  // chaque pas est ensuite modifiable individuellement via NOTE:, voir
  // handleNoteCommand(). C3,G3,C4,E4 puis une octave au-dessus pour 4-7.
  static const uint8_t kDefaultNotes[kTrackCount] = {48, 55, 60, 64, 60, 67, 72, 76};
  for (uint8_t p = 0; p < kPatternCount; ++p) {
    for (uint8_t t = 0; t < kTrackCount; ++t) {
      for (uint8_t s = 0; s < kStepCount; ++s) {
        patterns[p][t].stepNote[s] = kDefaultNotes[t];
        patterns[p][t].stepPatch[s] = 0xFF;  // 0xFF = patch par defaut de la piste (voir INST, plus haut)
        patterns[p][t].stepProb[s] = 100;    // 100% = joue toujours, comportement d'origine (voir PROB:)
      }
    }
  }
}

float stepIntervalUs() {
  // stepsPerBeat pas par temps (4 = double-croche/1/16 par defaut) ;
  // bpm = noires/minute. En microsecondes pour IntervalTimer.
  return 60000000.0f / bpm / static_cast<float>(stepsPerBeat);
}

// L'ISR (advanceTick()) tourne a kTicksPerStep fois le rythme d'un pas
// -- voir SequencerTrack plus haut pour le pourquoi (ARP/CUT/RETRIG).
float tickIntervalUs() {
  return stepIntervalUs() / static_cast<float>(kTicksPerStep);
}

void announceClock() {
  az2::printClock(Serial, currentBar, currentStep);
  az2::printClock(Serial1, currentBar, currentStep);
}

// Definie ici (et non avec les autres announce*) car elle a besoin de
// bpm/stepsPerBeat/trackEngine/trackPatch, declares plus haut dans ce
// fichier mais apres l'ancien emplacement de cette fonction.
void announceHello() {
  Serial.println(az2::kHelloAudio);
  Serial1.println(az2::kHelloAudio);

  // Reenvoie l'etat courant a la connexion/reconnexion de l'ESP32 -- sans
  // ca, l'ecran redemarre sur des valeurs par defaut fausses alors que le
  // Teensy, lui, garde son etat (tempo, division, moteur+patch par
  // piste) tant qu'il n'est pas lui-meme redemarre.
  az2::printBpm(Serial, bpm);
  az2::printBpm(Serial1, bpm);
  az2::printDivision(Serial, stepsPerBeat);
  az2::printDivision(Serial1, stepsPerBeat);
  {
    // Reconstruit une valeur 0-127 representative depuis swingAmount
    // (0-(kTicksPerStep-2) en interne, voir handleSwingCommand()) --
    // conversion avec perte (127 valeurs -> 3 crans) comme a l'aller,
    // suffisant pour reafficher un cran coherent a la reconnexion.
    char swingMsg[16];
    snprintf(swingMsg, sizeof(swingMsg), "SWING:%d", (swingAmount * 127) / (kTicksPerStep - 2));
    relayLine(swingMsg);
  }
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    az2::printEngineSelect(Serial, t, trackEngine[t]);
    az2::printEngineSelect(Serial1, t, trackEngine[t]);
    az2::printPatchSelect(Serial, t, trackPatch[t]);
    az2::printPatchSelect(Serial1, t, trackPatch[t]);
  }

  // Pattern/song (voir patterns[]/songPatterns[] plus haut) -- pas les
  // 8x8x16 pas complets (bien trop de lignes), juste l'etat de
  // navigation/song, comme pour bpm/stepsPerBeat ci-dessus.
  char msg[16];
  snprintf(msg, sizeof(msg), "PATTERN:%d", currentPattern);
  relayLine(msg);
  snprintf(msg, sizeof(msg), "SONGMODE:%d", songMode ? 1 : 0);
  relayLine(msg);
  snprintf(msg, sizeof(msg), "SONGLEN:%d", songLen);
  relayLine(msg);
  for (uint8_t i = 0; i < songLen; ++i) {
    snprintf(msg, sizeof(msg), "SONGSET:%d:%d", i, songPatterns[i]);
    relayLine(msg);
  }
}

// Un moteur different par piste (voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md) :
// Dexed et EPiano ont un vrai "note on/off", Braids est un oscillateur
// continu qu'on "frappe" (Strike interne a set_braids_pitch) -- on simule
// son extinction en coupant son canal du mixeur plutot qu'un vrai
// relachement de note. Le moteur utilise depend de trackEngine[track],
// choisi dynamiquement (voir setTrackEngine()) -- plus fixe par numero
// de piste.
void trackNoteOn(uint8_t track, uint8_t note, uint8_t velocity) {
  switch (trackEngine[track]) {
    case az2::kEngineDexed: trackDexedEngine[track].keydown(note, velocity); break;
    case az2::kEngineEPiano: trackEPianoEngine[track].noteOn(note, velocity); break;
    case az2::kEngineBraids:
      trackBraidsEngine[track].set_braids_pitch(static_cast<int16_t>(note) << 7);
      trackNoteHeld[track] = true;
      applyGroupGainNow(track);
      break;
    case az2::kEngineKarplus:
      trackKarplusEngine[track].noteOn(midiNoteToFreq(note), static_cast<float>(velocity) / 127.0f);
      break;
    case az2::kEngineAnalog:
      trackAnalogWave[track].frequency(midiNoteToFreq(note));
      trackAnalogWave[track].amplitude(0.8f);
      break;
    case az2::kEngineSampler:
      trackSamplerEngine[track].noteOn(note, velocity);
      break;
  }
  // Enveloppe PARTAGEE par les 6 moteurs (2026-09-18, voir le
  // commentaire de trackAnalogEnv[] plus haut) -- declenchee ici pour
  // TOUS, pas seulement ANALOG (qui l'utilisait deja seul avant ce
  // fix).
  trackAnalogEnv[track].noteOn();
}

void trackNoteOff(uint8_t track, uint8_t note) {
  switch (trackEngine[track]) {
    case az2::kEngineDexed: trackDexedEngine[track].keyup(note); break;
    case az2::kEngineEPiano: trackEPianoEngine[track].noteOff(note); break;
    case az2::kEngineBraids:
      trackNoteHeld[track] = false;
      trackGroupMixer(track).gain(trackGroupChannel(track), 0.0f);
      break;
    case az2::kEngineKarplus: trackKarplusEngine[track].noteOff(1.0f); break;
    case az2::kEngineAnalog: break;
    case az2::kEngineSampler: trackSamplerEngine[track].noteOff(); break;  // one-shot, ne fait rien (voir sa definition)
  }
  // Meme enveloppe partagee qu'a l'allumage ci-dessus -- coupe TOUS les
  // moteurs, pas seulement ANALOG.
  trackAnalogEnv[track].noteOff();
}

void allTrackNotesOff() {
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    SequencerTrack &tr = patterns[playingPattern][t];
    if (tr.stepPlaying) {
      trackNoteOff(t, tr.playingNote);
      tr.stepPlaying = false;
    }
  }
}

// Applique l'effet actif d'une piste sur le tick courant (ticksSinceTrigger
// deja positionne par l'appelant) -- ARP/RETRIG reutilisent directement
// trackNoteOn()/trackNoteOff() (note-off puis note-on, comme un vrai
// changement de pas), donc marchent uniformement sur les 5 moteurs sans
// rien connaitre de leurs specificites. Note : le patch (colonne INST,
// stepPatch) n'est PAS applique ici -- changer de patch Dexed en cours de
// route est trop couteux pour une ISR (voir SequencerTrack plus haut),
// laisse pour une iteration future.
void triggerStepFx(uint8_t t) {
  SequencerTrack &tr = patterns[playingPattern][t];
  if (!tr.stepPlaying) {
    return;
  }
  switch (tr.activeFx) {
    case kStepFxArp: {
      // xy = 2 decalages en demi-tons (nibbles haut/bas de activeFxVal) --
      // cycle racine -> +x -> +y a chaque tick, comme LSDJ "C"/M8 "ARP".
      const uint8_t x = static_cast<uint8_t>(tr.activeFxVal >> 4);
      const uint8_t y = static_cast<uint8_t>(tr.activeFxVal & 0x0F);
      const uint8_t phase = static_cast<uint8_t>(tr.ticksSinceTrigger % 3);
      const uint8_t offset = (phase == 0) ? 0 : (phase == 1) ? x : y;
      const uint8_t newNote = static_cast<uint8_t>(constrain(static_cast<int>(tr.baseNote) + offset, 0, 127));
      trackNoteOff(t, tr.playingNote);
      trackNoteOn(t, newNote, 100);
      tr.playingNote = newNote;
      break;
    }
    case kStepFxCut: {
      // Coupe la note avant la fin naturelle du pas (LSDJ "K", Polyend
      // "gate length") -- activeFxVal = nombre de ticks avant coupure.
      if (tr.ticksSinceTrigger >= tr.activeFxVal) {
        trackNoteOff(t, tr.playingNote);
        tr.stepPlaying = false;
      }
      break;
    }
    case kStepFxRetrig: {
      // Redeclenche la meme note a intervalle regulier (LSDJ "R", M8
      // "RET") -- activeFxVal = ticks entre 2 declenchements.
      if (tr.activeFxVal > 0 && (tr.ticksSinceTrigger % tr.activeFxVal) == 0) {
        trackNoteOff(t, tr.playingNote);
        trackNoteOn(t, tr.baseNote, 100);
        tr.playingNote = tr.baseNote;
      }
      break;
    }
    default:
      break;
  }
}

// Appelee directement par sequencerTimer (interruption materielle), a
// kTicksPerStep fois le rythme d'un pas -- donc a l'heure pile, pas de
// jitter du a loop(). Tick 0 = declenchement du pas (comme l'ancien
// advanceSequencer()) ; ticks suivants = traitement d'effet (ARP/CUT/
// RETRIG, voir triggerStepFx()). Reste volontairement minimale : aucun
// Serial.print ici (voir clockPending / updateSequencer()).
void advanceTick() {
  if (currentTick == 0) {
    allTrackNotesOff();

    currentStep = static_cast<uint8_t>((currentStep + 1) % kStepCount);
    // Swing : voir le commentaire de swingAmount plus haut -- pas pairs
    // raccourcis, impairs allonges (classique "shuffle" de boite a
    // rythme, delai des "contretemps"). swingAmount est deja borne a
    // kTicksPerStep-2 max par handleSwingCommand(), jamais moins de 2
    // ticks ici (voir le commentaire de swingAmount pour le bug que ca
    // evite sur CUT/RETRIG).
    ticksForCurrentStep = (currentStep % 2 == 0)
                              ? static_cast<uint8_t>(kTicksPerStep - swingAmount)
                              : static_cast<uint8_t>(kTicksPerStep + swingAmount);
    // Metronome (voir triggerMetronome()) -- un temps commence tous les
    // stepsPerBeat pas ; accentue (plus aigu) sur le tout premier temps
    // du pattern. no-op silencieux si metronomeEnabled est faux.
    if (currentStep % stepsPerBeat == 0) {
      triggerMetronome(currentStep == 0);
    }
    if (currentStep == 0) {
      ++currentBar;
      // Compteur de passages du pattern (2026-09-17, PROB:/COND:) -- avance
      // de 1 a chaque redemarrage, COMMUN a tous les pas/pistes (pas un
      // compteur par pas). Utilise par az2::stepConditionMet() pour les
      // conditions "K sur N" (ex: 1:2 = un pas sur deux). Simplification
      // assumee : ne se remet PAS a zero quand le pattern JOUE change (song
      // mode) -- un compteur par pattern aurait demande kPatternCount
      // compteurs pour un gain marginal a ce stade.
      ++patternLoopCount;
      // Fin du pattern joue : avance dans la song si le mode song est
      // actif, sinon la piste jouee reste alignee sur celle en cours
      // d'edition (comportement d'origine, boucle simple -- voir
      // patterns[]/currentPattern/playingPattern plus haut).
      if (songMode && songLen > 0) {
        songPos = static_cast<uint8_t>((songPos + 1) % songLen);
        playingPattern = songPatterns[songPos];
      } else {
        playingPattern = currentPattern;
      }
    }

    for (uint8_t t = 0; t < kTrackCount; ++t) {
      SequencerTrack &tr = patterns[playingPattern][t];
      // Probabilite + condition (2026-09-17) : un pas ON peut quand meme ne
      // pas jouer CE passage-ci -- prob=100 (defaut) et cond=kStepCondAlways
      // (defaut) reproduisent exactement le comportement d'origine (voir
      // seedDefaultNotes()). Court-circuite random() sur le chemin le plus
      // frequent (prob=100) plutot que d'appeler random(100)<100 a chaque
      // pas de chaque piste pour rien.
      const uint8_t prob = tr.stepProb[currentStep];
      const bool probPass = (prob >= 100) || (static_cast<uint8_t>(random(100)) < prob);
      const bool shouldTrigger = tr.stepOn[currentStep] && probPass &&
                                  az2::stepConditionMet(tr.stepCondition[currentStep], patternLoopCount, fillActive);
      if (shouldTrigger) {
        const uint8_t note = tr.stepNote[currentStep];
        trackNoteOn(t, note, 100);
        tr.playingNote = note;
        tr.baseNote = note;
        tr.stepPlaying = true;
        tr.activeFx = tr.stepFx[currentStep];
        tr.activeFxVal = tr.stepFxVal[currentStep];
        tr.ticksSinceTrigger = 0;
      } else {
        tr.stepPlaying = false;
        tr.activeFx = kStepFxNone;
      }
    }

    clockPending = true;
  } else {
    for (uint8_t t = 0; t < kTrackCount; ++t) {
      SequencerTrack &tr = patterns[playingPattern][t];
      if (tr.stepPlaying && tr.activeFx != kStepFxNone) {
        tr.ticksSinceTrigger = currentTick;
        triggerStepFx(t);
      }
    }
  }

  currentTick = static_cast<uint8_t>((currentTick + 1) % ticksForCurrentStep);
}

// Appelee depuis loop() : se contente d'imprimer le CLOCK: en attente
// (le pas lui-meme a deja ete avance par l'ISR sequencerTimer, voir
// advanceSequencer()). Le Serial.print couteux reste hors de l'ISR.
void updateSequencer() {
  if (clockPending) {
    clockPending = false;
    announceClock();
  }
}

void startSequencer() {
  playing = true;
  currentStep = kStepCount - 1;  // le prochain tick 0 (voir advanceTick()) ira au pas 0
  currentTick = 0;
  songPos = 0;
  playingPattern = (songMode && songLen > 0) ? songPatterns[0] : currentPattern;
  sequencerTimer.begin(advanceTick, tickIntervalUs());
}

void stopSequencer() {
  playing = false;
  sequencerTimer.end();
  allTrackNotesOff();
}

// STEP:<piste 0-3>:<pas 0-15>:<0 ou 1>
void handleStepCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("STEP", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const bool on = line.substring(idx3 + 1).toInt() != 0;

  if (track >= kTrackCount || step >= kStepCount) {
    sendCommandError("STEP", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepOn[step] = on;
  relayLine(line);  // confirme tel quel, utile pour que l'UI ESP32 se resynchronise
}

// NOTE:<piste>:<pas>:<note MIDI 0-127> -- edition de note par pas (voir
// SequencerTrack::stepNote), independante de STEP: (on/off). Portee de
// MicroDexed-touch (seq.note_data[pattern][step], voir sequencer.cpp).
void handleNoteCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("NOTE", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const int note = line.substring(idx3 + 1).toInt();

  if (track >= kTrackCount || step >= kStepCount || note < 0 || note > 127) {
    sendCommandError("NOTE", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepNote[step] = static_cast<uint8_t>(note);
  relayLine(line);
}

// INST:<piste>:<pas>:<patch 0-N ou 255 pour "defaut piste"> -- colonne
// INST du tracker (voir AZ2_TRACKER_ETUDE.md). Stocke/transmet pour
// l'instant ; PAS ENCORE applique en temps reel au declenchement (voir
// commentaire sur SequencerTrack::stepPatch plus haut).
void handleInstCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("INST", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const int patch = line.substring(idx3 + 1).toInt();

  if (track >= kTrackCount || step >= kStepCount || patch < 0 || patch > 255) {
    sendCommandError("INST", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepPatch[step] = static_cast<uint8_t>(patch);
  relayLine(line);
}

// SFX:<piste>:<pas>:<effet 0-3, voir StepFx>:<valeur 0-255> -- colonne
// FX+VAL du tracker. Prefixe "SFX" (Step FX) et non "FX" -- deja pris
// par les effets du bus maitre (FX:reverb:/FX:delay:, voir
// handleFxCommand()).
void handleStepFxCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  const int idx4 = line.indexOf(':', idx3 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0 || idx4 < 0) {
    sendCommandError("SFX", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const int fx = line.substring(idx3 + 1, idx4).toInt();
  const int val = line.substring(idx4 + 1).toInt();

  if (track >= kTrackCount || step >= kStepCount || fx < 0 || fx >= kStepFxCount || val < 0 || val > 255) {
    sendCommandError("SFX", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepFx[step] = static_cast<uint8_t>(fx);
  patterns[currentPattern][track].stepFxVal[step] = static_cast<uint8_t>(val);
  relayLine(line);
}

// PROB:<piste>:<pas>:<0-100> -- probabilite de declenchement du pas (%),
// voir le commentaire de SequencerTrack::stepProb et advanceTick(). 100
// (defaut) = joue toujours, comportement d'origine.
void handleProbCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("PROB", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const int prob = line.substring(idx3 + 1).toInt();

  if (track >= kTrackCount || step >= kStepCount || prob < 0 || prob > 100) {
    sendCommandError("PROB", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepProb[step] = static_cast<uint8_t>(prob);
  relayLine(line);
}

// COND:<piste>:<pas>:<0-255> -- condition de declenchement du pas, voir
// az2::stepConditionEncode()/stepConditionMet() (AZ2_Protocol.h) pour
// l'encodage. 0/kStepCondAlways (defaut) = aucune condition.
void handleCondCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("COND", "MALFORMED");
    return;
  }

  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t step = static_cast<uint8_t>(line.substring(idx2 + 1, idx3).toInt());
  const int cond = line.substring(idx3 + 1).toInt();

  if (track >= kTrackCount || step >= kStepCount || cond < 0 || cond > 255) {
    sendCommandError("COND", "OUT_OF_RANGE");
    return;
  }

  patterns[currentPattern][track].stepCondition[step] = static_cast<uint8_t>(cond);
  relayLine(line);
}

// FILL:<0|1> -- active/desactive l'etat "fill" global, consulte par les
// pas en condition kStepCondFill/kStepCondNotFill (voir le commentaire de
// fillActive plus haut). Pas de piste/pas ici, un seul etat global --
// meme convention que PLAY/STOP.
void handleFillCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("FILL", "MALFORMED");
    return;
  }
  fillActive = line.substring(idx + 1).toInt() != 0;
  relayLine(line);
}

// PATTERN:<0-7> -- choisit le pattern EDITE (STEP:/NOTE:/INST:/SFX:
// s'appliquent a celui-ci). En mode boucle simple (songMode false),
// c'est aussi celui qui joue -- mais PAS au prochain pas : advanceTick()
// ne recopie playingPattern = currentPattern que quand currentStep
// revient a 0, donc au prochain REDEMARRAGE du pattern (jusqu'a 16 pas
// plus tard selon ou on en est), pas au pas suivant. Comportement
// voulu (pas de saut audible en plein pattern), commentaire corrige le
// 2026-09-17 (trouve trop optimiste lors d'un audit de code).
void handlePatternCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("PATTERN", "MALFORMED");
    return;
  }
  const int pattern = line.substring(idx + 1).toInt();
  if (pattern < 0 || pattern >= kPatternCount) {
    sendCommandError("PATTERN", "OUT_OF_RANGE");
    return;
  }
  currentPattern = static_cast<uint8_t>(pattern);
  relayLine(line);
}

// SONGSET:<position 0-15>:<pattern 0-7> -- assigne un pattern a une
// case de la song (voir songPatterns[] plus haut). Etend songLen si la
// position depasse la longueur actuelle (pas de trou possible pour
// l'instant -- v1 simple, toujours une song contigue depuis 0).
void handleSongSetCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("SONGSET", "MALFORMED");
    return;
  }
  const int pos = line.substring(idx1 + 1, idx2).toInt();
  const int pattern = line.substring(idx2 + 1).toInt();
  if (pos < 0 || pos >= kSongLength || pattern < 0 || pattern >= kPatternCount) {
    sendCommandError("SONGSET", "OUT_OF_RANGE");
    return;
  }
  songPatterns[pos] = static_cast<uint8_t>(pattern);
  if (static_cast<uint8_t>(pos + 1) > songLen) {
    songLen = static_cast<uint8_t>(pos + 1);
  }
  relayLine(line);
}

// SONGLEN:<0-16> -- longueur active de la song (0 = pas de song
// definie). Raccourcir ne perd pas les cases au-dela -- elles restent
// en memoire, juste ignorees par la lecture tant que songLen ne les
// couvre pas.
void handleSongLenCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("SONGLEN", "MALFORMED");
    return;
  }
  const int len = line.substring(idx + 1).toInt();
  if (len < 0 || len > kSongLength) {
    sendCommandError("SONGLEN", "OUT_OF_RANGE");
    return;
  }
  songLen = static_cast<uint8_t>(len);
  relayLine(line);
}

// SONGMODE:<0|1> -- 0 = boucle simple sur currentPattern (comportement
// d'origine), 1 = enchaine songPatterns[0..songLen-1] en boucle.
void handleSongModeCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("SONGMODE", "MALFORMED");
    return;
  }
  songMode = line.substring(idx + 1).toInt() != 0;
  relayLine(line);
}

// BPM:<valeur, 30-300>
void handleBpmCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("BPM", "MALFORMED");
    return;
  }
  const float value = line.substring(idx + 1).toFloat();
  if (value < 30.0f || value > 300.0f) {
    sendCommandError("BPM", "OUT_OF_RANGE");
    return;
  }
  bpm = value;
  if (playing) {
    // Reajuste la periode de l'ISR tout de suite, sans couper/reprendre
    // la lecture (IntervalTimer::update() change juste l'intervalle).
    sequencerTimer.update(tickIntervalUs());
  }
  relayLine(line);  // confirme tel quel, l'UI ESP32/Pico affiche le tempo reel
}

// DIV:<pas par temps -- doit etre une des valeurs de az2::kDivisionOptions>
void handleDivCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("DIV", "MALFORMED");
    return;
  }
  const uint8_t value = static_cast<uint8_t>(line.substring(idx + 1).toInt());

  bool valid = false;
  for (uint8_t i = 0; i < az2::kDivisionOptionCount; ++i) {
    if (az2::kDivisionOptions[i].stepsPerBeat == value) {
      valid = true;
      break;
    }
  }
  if (!valid) {
    sendCommandError("DIV", "OUT_OF_RANGE");
    return;
  }

  stepsPerBeat = value;
  if (playing) {
    sequencerTimer.update(tickIntervalUs());
  }
  relayLine(line);
}

// SWING:<0-127> -- voir le commentaire de swingAmount plus haut. Mappe
// 0-127 (convention UI, meme echelle que FILT:/ENV:) vers 0-(kTicksPerStep-2)
// en interne (pas kTicksPerStep-1 : voir le BUG REEL du 2026-09-17 dans
// le commentaire de swingAmount -- un pas raccourci a 1 seul tick
// empechait CUT/RETRIG de se declencher). Ne touche PAS sequencerTimer
// -- prend effet des le prochain debut de pas (ticksForCurrentStep
// n'est recalcule qu'a ce moment-la), jamais en cours de pas.
void handleSwingCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("SWING", "MALFORMED");
    return;
  }
  const int raw = constrain(line.substring(idx + 1).toInt(), 0, 127);
  swingAmount = static_cast<uint8_t>((raw * (kTicksPerStep - 2)) / 127);
  relayLine(line);
}

// ENGINE:<piste 0-3>:<moteur, voir az2::kEngine*>
void handleEngineCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("ENGINE", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const uint8_t engine = static_cast<uint8_t>(line.substring(idx2 + 1).toInt());
  if (track >= kTrackCount || engine >= az2::kEngineCount) {
    sendCommandError("ENGINE", "OUT_OF_RANGE");
    return;
  }

  setTrackEngine(track, engine);
  relayLine(line);
  // setTrackEngine() remet toujours le patch a 0 -- previens l'UI tout de
  // suite, sinon elle resterait affichee sur l'ancien patch jusqu'au
  // prochain changement.
  az2::printPatchSelect(Serial, track, 0);
  az2::printPatchSelect(Serial1, track, 0);
}

// PATCH:<piste 0-3>:<index de patch, voir az2::enginePatchCount(moteur actif)>
void handlePatchCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("PATCH", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  if (track >= kTrackCount) {
    sendCommandError("PATCH", "OUT_OF_RANGE");
    return;
  }
  const uint8_t patch = static_cast<uint8_t>(line.substring(idx2 + 1).toInt());
  if (patch >= az2::enginePatchCount(trackEngine[track])) {
    sendCommandError("PATCH", "OUT_OF_RANGE");
    return;
  }

  trackPatch[track] = patch;
  applyTrackPatch(track);
  relayLine(line);
}

// Etat du bus d'effets maitre -- source commune pour FX:reverb:/FX:delay:
// (serie) ET les potards 2/3 cables directement sur le Teensy (voir
// updateLocalControls()), pour que les deux chemins restent coherents.
float masterVolume = 1.0f;  // potard 1
float reverbWet = 0.0f;     // potard 2 ou FX:reverb:
float delayWet = 0.0f;      // potard 3 ou FX:delay:

void applyMasterMix() {
  mixMaster.gain(0, masterVolume);
  mixMaster.gain(1, reverbWet * masterVolume);
  mixMaster.gain(2, delayWet * masterVolume);
  mixMaster.gain(3, masterVolume);  // son GB (voir gbAudioQueue) -- suit le potard 1 comme le signal sec
}

// FX:reverb:<0-100> ou FX:delay:<0-100> -- bus d'effets maitre (voir
// mixMaster/reverbUnit/delayUnit plus haut). Piste par piste reste a
// faire (cf feuille de route etape 4, "mixeur vrai").
void handleFxCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("FX", "MALFORMED");
    return;
  }
  const String param = line.substring(idx1 + 1, idx2);
  const int amount = constrain(line.substring(idx2 + 1).toInt(), 0, 100);
  const float wet = static_cast<float>(amount) / 100.0f;

  if (param == "reverb") {
    reverbWet = wet;
  } else if (param == "delay") {
    delayWet = wet;
  } else {
    sendCommandError("FX", "UNKNOWN_PARAM");
    return;
  }
  applyMasterMix();
  relayLine(line);
}

// FILT:<piste 0-7>:<coupure 0-127>:<resonance 0-127> -- filtre resonant
// par piste (voir trackFilter[] plus haut), demande le 2026-09-15 ("on
// fait un max de code ... tracker/M8 killer"). Coupure mappee en
// exponentiel (20Hz-18kHz, sensation "synthe" standard -- un potentio-
// metre/encodeur lineaire sur une echelle logarithmique) ; resonance
// lineaire sur la plage acceptee par AudioFilterStateVariable (0.7-5.0,
// voir filter_variable.h). 127/0 = grand ouvert/neutre = aucun filtrage
// audible, comportement par defaut avant tout FILT:.
// VOL:<piste 0-7>:<volume 0-127> -- voir le commentaire de trackVolume[]
// plus haut pour le piege Braids (gain de groupe = porte note-on/off
// pour ce moteur specifiquement).
void handleVolCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("VOL", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  if (track >= kTrackCount) {
    sendCommandError("VOL", "OUT_OF_RANGE");
    return;
  }
  trackVolume[track] = static_cast<uint8_t>(constrain(line.substring(idx2 + 1).toInt(), 0, 127));
  applyGroupGainNow(track);
  relayLine(line);
}

// MUTE:<piste 0-7>:<0|1> et SOLO:<piste 0-7>:<0|1> -- voir
// trackEffectiveGain()/applyGroupGainNow() plus haut pour le calcul
// (mute gagne toujours ; un solo actif rend muettes toutes les pistes
// non soloed). Un changement de solo affecte potentiellement TOUTES
// les pistes (une piste peut devenir muette parce qu'une AUTRE vient
// d'etre soloed) -- on recalcule donc le gain de TOUTES les pistes a
// chaque fois, pas seulement celle nommee dans la commande.
void handleMuteCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("MUTE", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  if (track >= kTrackCount) {
    sendCommandError("MUTE", "OUT_OF_RANGE");
    return;
  }
  trackMuted[track] = line.substring(idx2 + 1).toInt() != 0;
  applyGroupGainNow(track);
  relayLine(line);
}

void handleSoloCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  if (idx1 < 0 || idx2 < 0) {
    sendCommandError("SOLO", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  if (track >= kTrackCount) {
    sendCommandError("SOLO", "OUT_OF_RANGE");
    return;
  }
  trackSoloed[track] = line.substring(idx2 + 1).toInt() != 0;
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    applyGroupGainNow(t);
  }
  relayLine(line);
}

void handleFiltCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("FILT", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const int cutoff = constrain(line.substring(idx2 + 1, idx3).toInt(), 0, 127);
  const int res = constrain(line.substring(idx3 + 1).toInt(), 0, 127);
  if (track >= kTrackCount) {
    sendCommandError("FILT", "OUT_OF_RANGE");
    return;
  }

  // 20Hz a 15000Hz exponentiel : freq = 20 * (15000/20)^(cutoff/127).
  //
  // [2026-09-18] CAUSE REELLE du "bruit blanc" qui semblait toucher
  // 4 (puis 5) moteurs sur 5 (voir AZ2_ETAT_DES_LIEUX.md) : le plafond
  // etait a 18000Hz, et LE DEFAUT AU BOOT (voir setup()) etait DEJA a
  // ce plafond -- donc TOUS les tests "moteur X sonne mal" depuis le
  // debut de l'investigation ont ete faits filtre grand ouvert a
  // 18kHz. AudioFilterStateVariable (Chamberlin SVF, voir
  // filter_variable.h dans la lib Audio Teensy) est connu pour devenir
  // instable/auto-osciller pres de sa limite haute -- meme a resonance
  // MINIMALE (0.7, la plus "plate" possible). Confirme sur le vrai
  // materiel le 2026-09-18 (soir) : le MEME test (meme moteur, meme
  // note) est BRUIT BLANC a 18000Hz mais une VRAIE note propre a
  // 8000/12000/15000Hz -- reproductible sur DEXED, EPIANO, BRAIDS,
  // KARPLUS et ANALOG identiquement. Ce n'etait donc PAS un bug par
  // moteur (aucune bibliotheque de synthese en cause) mais un reglage
  // par defaut du filtre COMMUN a toutes les pistes, dans la zone
  // d'instabilite du filtre resonant partage. 15000Hz choisi comme
  // nouveau plafond -- dernier point teste propre sur le vrai
  // materiel (18000Hz ne l'etait pas) ; garde une bonne marge sous la
  // frequence de Nyquist/2.5 (~17647Hz) que la lib elle-meme autorise
  // en theorie (voir AudioFilterStateVariable::frequency()) mais qui
  // s'est reveille bruyante en pratique sur cette carte.
  const float ratio = static_cast<float>(cutoff) / 127.0f;
  const float freq = 20.0f * powf(15000.0f / 20.0f, ratio);
  trackFilter[track].frequency(freq);
  trackFilter[track].resonance(0.7f + (static_cast<float>(res) / 127.0f) * (5.0f - 0.7f));

  relayLine(line);
}

// ENV:<piste 0-7>:<attaque 0-127>:<chute 0-127>:<maintien 0-127>:
// <relachement 0-127> -- ADSR editable, demande le 2026-09-15
// ("modifiant les patch adsr"). S'applique a trackAnalogEnv[] : SEUL
// moteur de la palette (Dexed/EPiano/Braids/Karplus/Analog) avec une
// vraie enveloppe ADSR generique exposee ici -- les autres ont leur
// propre comportement interne au patch (Dexed/EPiano) ou pas
// d'enveloppe du tout (Braids/Karplus). Accepte quelle que soit la
// piste (stocke dans l'objet AudioEffectEnvelope de la piste, utilise
// des que/si la piste bascule sur ANALOG) -- pas besoin que le moteur
// actif soit deja ANALOG pour regler l'enveloppe a l'avance.
// Temps mappes en quadratique 0-2000ms (plus de resolution pres de 0,
// utile pour une attaque tres courte) ; maintien lineaire 0.0-1.0.
void handleEnvCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  const int idx4 = line.indexOf(':', idx3 + 1);
  const int idx5 = line.indexOf(':', idx4 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0 || idx4 < 0 || idx5 < 0) {
    sendCommandError("ENV", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  if (track >= kTrackCount) {
    sendCommandError("ENV", "OUT_OF_RANGE");
    return;
  }
  const int a = constrain(line.substring(idx2 + 1, idx3).toInt(), 0, 127);
  const int d = constrain(line.substring(idx3 + 1, idx4).toInt(), 0, 127);
  const int s = constrain(line.substring(idx4 + 1, idx5).toInt(), 0, 127);
  const int r = constrain(line.substring(idx5 + 1).toInt(), 0, 127);

  auto quadMs = [](int v) { return (static_cast<float>(v) / 127.0f) * (static_cast<float>(v) / 127.0f) * 2000.0f; };
  trackAnalogEnv[track].attack(quadMs(a));
  trackAnalogEnv[track].decay(quadMs(d));
  trackAnalogEnv[track].sustain(static_cast<float>(s) / 127.0f);
  trackAnalogEnv[track].release(quadMs(r));

  relayLine(line);
}

// DXP:<piste 0-7>:<index 0=algo,1=feedback>:<valeur> -- reglages
// propres au moteur Dexed, demande le 2026-09-16 ("il faut des
// reglages, on a pas de reglages dans la fenetre dexed du tracker") :
// FILT:/ENV: pilotent le filtre resonant et l'AudioEffectEnvelope
// generiques, mais l'ADSR generique n'est PAS ecoutee par Dexed (sa
// propre EG interne au patch DX7 la remplace, voir commentaire de
// handleEnvCommand) -- une piste Dexed n'avait donc AUCUN reglage
// audible depuis la page PATCH. Algorithme (0-31) et feedback (0-7)
// sont les 2 parametres DX7 les plus identifiants et les plus simples
// a exposer (adresse globale unique dans le voice data, pas par
// operateur comme les niveaux/EG des 6 operateurs -- garde pour plus
// tard si besoin). Accepte quelle que soit la piste/le moteur actif
// (ecrit dans l'objet Dexed de la piste, audible des que/si elle
// bascule sur DEXED), meme principe que ENV:.
void handleDexedParamCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("DXP", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const int index = line.substring(idx2 + 1, idx3).toInt();
  if (track >= kTrackCount) {
    sendCommandError("DXP", "OUT_OF_RANGE");
    return;
  }

  if (index == 0) {
    const int algo = constrain(line.substring(idx3 + 1).toInt(), 0, 31);
    trackDexedEngine[track].setVoiceDataElement(DEXED_VOICE_OFFSET + DEXED_ALGORITHM, static_cast<uint8_t>(algo));
  } else if (index == 1) {
    const int fb = constrain(line.substring(idx3 + 1).toInt(), 0, 7);
    trackDexedEngine[track].setVoiceDataElement(DEXED_VOICE_OFFSET + DEXED_FEEDBACK, static_cast<uint8_t>(fb));
  } else {
    sendCommandError("DXP", "UNKNOWN_INDEX");
    return;
  }

  relayLine(line);
}

// Borne max valide pour l'octet `globalIndex` du buffer de voix DX7
// deballe (voir DexedVoiceOPParameters/DexedVoiceParameters dans
// dexed.h -- 6 operateurs de 21 octets [0-125], puis 19 parametres
// globaux [126-144], le nom occupant les octets suivants -- exclu de
// DXR:, voir handleDexedRawCommand()). Callee a l'ecriture (clamp) ET
// exposee cote UI via la meme table statique/reponse -- pas de
// duplication a maintenir a la main de chaque cote.
uint8_t dxpParamMax(uint16_t globalIndex) {
  if (globalIndex < DEXED_VOICE_OFFSET) {
    switch (globalIndex % 21) {
      case DEXED_OP_SCL_LEFT_CURVE:
      case DEXED_OP_SCL_RGHT_CURVE: return 3;
      case DEXED_OP_OSC_RATE_SCALE: return 7;
      case DEXED_OP_AMP_MOD_SENS: return 3;
      case DEXED_OP_KEY_VEL_SENS: return 7;
      case DEXED_OP_OSC_MODE: return 1;
      case DEXED_OP_FREQ_COARSE: return 31;
      case DEXED_OP_OSC_DETUNE: return 14;
      default: return 99;  // EG R1-4/L1-4, break point, scl depth, output lev, freq fine
    }
  }
  switch (globalIndex - DEXED_VOICE_OFFSET) {
    case DEXED_ALGORITHM: return 31;
    case DEXED_FEEDBACK: return 7;
    case DEXED_OSC_KEY_SYNC: return 1;
    case DEXED_LFO_SYNC: return 1;
    case DEXED_LFO_WAVE: return 5;
    case DEXED_LFO_PITCH_MOD_SENS: return 7;
    case DEXED_TRANSPOSE: return 48;
    default: return 99;  // pitch EG R1-4/L1-4, LFO speed/delay/PMD/AMD
  }
}

// DXR:<piste>:<octet brut 0-144>:<valeur> -- editeur DX7 COMPLET
// (2026-09-18, "un editeur de patch complet et completement reglable"),
// en plus de DXP: (algo/feedback seuls, garde tel quel pour ne rien
// casser cote UI existante). Acces direct a n'importe quel octet du
// buffer de voix deballe (Dexed::setVoiceDataElement()/
// getVoiceDataElement(), voir dexed.h) -- 0-125 = les 6 operateurs
// (21 octets chacun, meme disposition, voir DexedVoiceOPParameters),
// 126-144 = parametres globaux (voir DexedVoiceParameters). Le nom
// (10 caracteres ASCII a partir de l'octet 145) est EXCLU ici -- pas
// un reglage numerique, une future commande dediee si besoin.
// DXR?<piste>:<octet> lit la valeur actuelle (meme format en retour,
// indistinguable d'une confirmation d'ecriture pour l'UI).
void handleDexedRawCommand(const String &line) {
  // "DXR:" (ecriture, 2 ':' apres le prefixe) et "DXR?" (lecture, 1
  // seul ':' apres le prefixe -- pas de 3e champ valeur) n'ont PAS la
  // meme forme : les 2 prefixes font 4 caracteres, mais compter les
  // ':' depuis le DEBUT de la ligne (comme les autres handlers de ce
  // fichier) tombe juste pour "DXR:" (le ':' du prefixe lui-meme sert
  // de 1er separateur) et FAUX pour "DXR?" (le prefixe n'a pas de ':').
  // On cherche donc le ':' a partir de la fin du prefixe (4 car.), pas
  // depuis 0.
  const bool isQuery = line.startsWith("DXR?");
  const int idx1 = line.indexOf(':', 4);
  if (idx1 < 0) {
    sendCommandError("DXR", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(4, idx1).toInt());

  int rawIndex;
  int value = 0;
  if (isQuery) {
    rawIndex = line.substring(idx1 + 1).toInt();
  } else {
    const int idx2 = line.indexOf(':', idx1 + 1);
    if (idx2 < 0) {
      sendCommandError("DXR", "MALFORMED");
      return;
    }
    rawIndex = line.substring(idx1 + 1, idx2).toInt();
    value = line.substring(idx2 + 1).toInt();
  }

  if (track >= kTrackCount || rawIndex < 0 || rawIndex >= DEXED_VOICE_OFFSET + DEXED_NAME) {
    sendCommandError("DXR", "OUT_OF_RANGE");
    return;
  }

  if (isQuery) {
    const uint8_t v = trackDexedEngine[track].getVoiceDataElement(static_cast<uint8_t>(rawIndex));
    char msg[24];
    snprintf(msg, sizeof(msg), "DXR:%d:%d:%d", track, rawIndex, v);
    relayLine(msg);
    return;
  }

  value = constrain(value, 0, static_cast<int>(dxpParamMax(static_cast<uint16_t>(rawIndex))));
  trackDexedEngine[track].setVoiceDataElement(static_cast<uint8_t>(rawIndex), static_cast<uint8_t>(value));
  relayLine(line);
}

// EXP:<piste>:<0-11>:<0-127> -- editeur EPIANO complet (2026-09-18,
// "un editeur de patch complet"). mdaEPiano (voir mdaEPiano.h) expose
// 12 vrais parametres continus, chacun un flottant 0.0-1.0 normalise
// (convention standard des plugins mda -- aucun ne stocke une plage
// differente, voir setParameter()/getParameter() dans mdaEPiano.cpp) :
// avant cette commande, AUCUN n'etait reglable depuis l'ecran (seuls
// les 5 "patches" figes de setProgram() l'etaient). AudioSynthEPiano
// herite publiquement de mdaEPiano (voir synth_mda_epiano.h) -- les
// setters/getters s'appellent donc directement sur
// trackEPianoEngine[track]. EXP?<piste>:<index> lit la valeur
// actuelle (meme format en retour).
void handleEPianoParamCommand(const String &line) {
  // Meme piege que DXR: ci-dessus -- "EXP?" (lecture) n'a pas de ':'
  // dans son prefixe, contrairement a "EXP:" (ecriture) : chercher le
  // 1er ':' depuis la fin du prefixe (4 car.), pas depuis le debut de
  // la ligne.
  const bool isQuery = line.startsWith("EXP?");
  const int idx1 = line.indexOf(':', 4);
  if (idx1 < 0) {
    sendCommandError("EXP", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(4, idx1).toInt());

  int index;
  int rawValue = 0;
  if (isQuery) {
    index = line.substring(idx1 + 1).toInt();
  } else {
    const int idx2 = line.indexOf(':', idx1 + 1);
    if (idx2 < 0) {
      sendCommandError("EXP", "MALFORMED");
      return;
    }
    index = line.substring(idx1 + 1, idx2).toInt();
    rawValue = line.substring(idx2 + 1).toInt();
  }

  if (track >= kTrackCount || index < 0 || index > 11) {
    sendCommandError("EXP", "OUT_OF_RANGE");
    return;
  }
  AudioSynthEPiano &eng = trackEPianoEngine[track];

  if (isQuery) {
    float v = 0.0f;
    switch (index) {
      case 0: v = eng.getDecay(); break;
      case 1: v = eng.getRelease(); break;
      case 2: v = eng.getHardness(); break;
      case 3: v = eng.getTreble(); break;
      case 4: v = eng.getPanTremolo(); break;
      case 5: v = eng.getPanLFO(); break;
      case 6: v = eng.getVelocitySense(); break;
      case 7: v = eng.getStereo(); break;
      case 8: v = eng.getTune(); break;
      case 9: v = eng.getDetune(); break;
      case 10: v = eng.getOverdrive(); break;
      case 11: v = eng.getVolume(); break;
    }
    char msg[24];
    snprintf(msg, sizeof(msg), "EXP:%d:%d:%d", track, index, static_cast<int>(v * 127.0f + 0.5f));
    relayLine(msg);
    return;
  }

  const float v = static_cast<float>(constrain(rawValue, 0, 127)) / 127.0f;
  switch (index) {
    case 0: eng.setDecay(v); break;
    case 1: eng.setRelease(v); break;
    case 2: eng.setHardness(v); break;
    case 3: eng.setTreble(v); break;
    case 4: eng.setPanTremolo(v); break;
    case 5: eng.setPanLFO(v); break;
    case 6: eng.setVelocitySense(v); break;
    case 7: eng.setStereo(v); break;
    case 8: eng.setTune(v); break;
    case 9: eng.setDetune(v); break;
    case 10: eng.setOverdrive(v); break;
    case 11: eng.setVolume(v); break;
  }
  relayLine(line);
}

// BXP:<piste>:<0=color|1=timbre>:<0-127> -- BRAIDS n'exposait jusqu'ici
// que la forme (set_braids_shape(), voir kBraidsShapeValues) ; color/
// timbre existent dans synth_braids.h (set_braids_color()/
// set_braids_timbre(), int16_t) mais n'etaient jamais appeles.
// 0-127 mappe lineairement sur 0-32767 (16 bits signes, mais Braids
// n'utilise en pratique que la moitie positive pour ces 2 controles).
void handleBraidsParamCommand(const String &line) {
  const int idx1 = line.indexOf(':');
  const int idx2 = line.indexOf(':', idx1 + 1);
  const int idx3 = line.indexOf(':', idx2 + 1);
  if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
    sendCommandError("BXP", "MALFORMED");
    return;
  }
  const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
  const int index = line.substring(idx2 + 1, idx3).toInt();
  const int raw127 = constrain(line.substring(idx3 + 1).toInt(), 0, 127);
  if (track >= kTrackCount || (index != 0 && index != 1)) {
    sendCommandError("BXP", "OUT_OF_RANGE");
    return;
  }
  const int16_t scaled = static_cast<int16_t>((raw127 * 32767) / 127);
  if (index == 0) {
    trackBraidsEngine[track].set_braids_color(scaled);
  } else {
    trackBraidsEngine[track].set_braids_timbre(scaled);
  }
  relayLine(line);
}

// SCOPE:<piste 0-7> pour observer cette piste (sortie post-filtre, voir
// trackFilter[]), SCOPE:OFF pour arreter -- voir updateScope() plus bas
// pour l'envoi effectif des paquets. Rebranche patchScopeTap a chaque
// appel (disconnect() d'abord, comme patchTrackIn[]).
void handleScopeCommand(const String &line) {
  const int idx = line.indexOf(':');
  if (idx < 0) {
    sendCommandError("SCOPE", "MALFORMED");
    return;
  }
  const String param = line.substring(idx + 1);

  patchScopeTap.disconnect();
  scopeQueue.end();
  scopeQueue.clear();

  if (param == "OFF") {
    scopeTrack = -1;
  } else {
    const int track = param.toInt();
    if (track < 0 || track >= kTrackCount) {
      sendCommandError("SCOPE", "OUT_OF_RANGE");
      return;
    }
    scopeTrack = static_cast<int8_t>(track);
    patchScopeTap.connect(trackFilter[track], 0, scopeQueue, 0);
    scopeQueue.begin();
  }
  relayLine(line);
}

// Appelee depuis loop() -- draine scopeQueue et envoie un paquet toutes
// les kScopeSendIntervalMs. Etait 20ms (~50 images/s) -- l'ecran
// clignotait trop (chaque paquet efface/redessine toute la zone du
// tracer, voir drawPatchScope() cote ESP32), remonte a 66ms (~15
// images/s, largement suffisant pour un tracer visuel, bien moins
// agressif a l'oeil) suite au retour ("la fenetre patch ... elle
// scintille un peu trop"). Les blocs arrivent bien plus vite (~2.9ms/
// bloc a 44.1kHz) -- on jette les blocs intermediaires pour ne garder
// que le plus recent, pas de latence qui s'accumule.
uint32_t lastScopeSendMs = 0;
constexpr uint32_t kScopeSendIntervalMs = 66;
constexpr uint8_t kScopeDecimate = 128 / az2::kScopeSamplesPerPacket;

void updateScope() {
  if (scopeTrack < 0) {
    return;
  }
  // BUG REEL trouve et corrige le 2026-09-15 : freeBuffer() ne libere
  // QUE le dernier bloc obtenu via readBuffer() (voir userblock dans
  // record_queue.cpp) -- l'appeler seul, sans readBuffer() d'abord, est
  // un NO-OP (userblock reste NULL). L'ancienne version de cette boucle
  // ("while (available()>1) freeBuffer();") ne faisait donc RIEN,
  // available() ne redescendait jamais, et la boucle tournait a
  // l'infini des que de l'audio arrivait reellement (PLAY + SCOPE actif
  // en meme temps) -- gele tout loop() (plus de serie, plus de
  // sequenceur, rien). Reproduit et isole en plusieurs etapes (connect
  // seul OK, begin() seul OK, les deux ensemble + PLAY = gel garanti).
  // Fix : readBuffer() PUIS freeBuffer() pour vraiment avancer et jeter
  // les blocs intermediaires.
  while (scopeQueue.available() > 1) {
    scopeQueue.readBuffer();
    scopeQueue.freeBuffer();
  }
  if (scopeQueue.available() < 1) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastScopeSendMs < kScopeSendIntervalMs) {
    return;  // le bloc reste dans la queue, sera toujours "le plus recent" au prochain tour
  }
  lastScopeSendMs = now;

  int16_t *block = scopeQueue.readBuffer();
  static uint8_t scopeBuf[az2::kScopeSamplesPerPacket];
  for (uint8_t i = 0; i < az2::kScopeSamplesPerPacket; ++i) {
    const int16_t sample = block[i * kScopeDecimate];
    scopeBuf[i] = static_cast<uint8_t>((sample >> 8) + 128);  // 16 bits signe -> 8 bits non signe
  }
  scopeQueue.freeBuffer();

  Serial1.write(az2::kScopePacketMagic);
  Serial1.write(az2::kScopeSamplesPerPacket);
  Serial1.write(scopeBuf, az2::kScopeSamplesPerPacket);
}

// CPU? -- charge processeur et memoire audio reelles, demande le
// 2026-09-14 ("cote performance on est comment") avant d'augmenter
// pistes/moteurs. AudioProcessorUsage() = charge instantanee (%),
// ...Max() = pic depuis le dernier reset ; idem pour la memoire (en
// blocs, sur les 200 alloues par AudioMemory(), voir setup()).
void reportCpuUsage() {
  Serial.print("AZ2:CPU:usage=");
  Serial.print(AudioProcessorUsage(), 1);
  Serial.print("%:max=");
  Serial.print(AudioProcessorUsageMax(), 1);
  Serial.println('%');
  Serial.print("AZ2:MEM:blocks=");
  Serial.print(AudioMemoryUsage());
  Serial.print(":max=");
  Serial.print(AudioMemoryUsageMax());
  Serial.println("/200");
}

// ---------------------------------------------------------------------
// Croix + 4 boutons + 3 potentiometres, cables DIRECTEMENT sur le Teensy
// -- remplace le Pico/la matrice SparkFun, abandonnes le 2026-09-14
// ("ca m'a soule, on fait sans la matrice de bouton") apres un mux LED
// impossible a faire fonctionner malgre un long diagnostic (voir
// AZ2_CABLAGE_PICO.md). Cablage simple, pas de scan matriciel : chaque
// switch a sa propre broche (INPUT_PULLUP, l'autre patte au GND commun),
// voir AZ2_CABLAGE_MASTER.md pour le tableau complet. Ces memes croix+
// boutons serviront plus tard de manette pour le mode JEUX (voir
// AZ2_EMULATION_JEUX.md).
// ---------------------------------------------------------------------
constexpr int kNavUpPin = 2, kNavDownPin = 3, kNavLeftPin = 4, kNavRightPin = 5;
constexpr int kBtnAPin = 6, kBtnBPin = 8, kBtnCPin = 9, kBtnDPin = 23;

constexpr uint32_t kLocalDebounceMs = 15;

struct DigitalControl {
  const char *label;  // direction ("UP".."RIGHT") ou nom de bouton ("A".."D")
  int pin;
  bool isNav;  // true = croix (NAV:), false = bouton (BTN:)
  bool state = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;
};

DigitalControl localControls[] = {
    {"UP", kNavUpPin, true},
    {"DOWN", kNavDownPin, true},
    {"LEFT", kNavLeftPin, true},
    {"RIGHT", kNavRightPin, true},
    {"A", kBtnAPin, false},
    {"B", kBtnBPin, false},
    {"C", kBtnCPin, false},
    {"D", kBtnDPin, false},
};
constexpr uint8_t kLocalControlCount = sizeof(localControls) / sizeof(localControls[0]);

void setupEncoderButtons();  // definie plus bas avec le reste des encodeurs

void setupLocalControls() {
  for (DigitalControl &c : localControls) {
    pinMode(c.pin, INPUT_PULLUP);
  }
  setupEncoderButtons();
}

void updateDigitalControls() {
  const uint32_t now = millis();
  for (DigitalControl &c : localControls) {
    const bool raw = digitalRead(c.pin) == LOW;  // pull-up : appuye = LOW
    if (raw != c.lastRaw) {
      c.lastRaw = raw;
      c.lastChangeMs = now;
    }
    if ((now - c.lastChangeMs) >= kLocalDebounceMs && raw != c.state) {
      c.state = raw;
      if (c.isNav) {
        az2::printNav(Serial, c.label, raw);
        az2::printNav(Serial1, c.label, raw);
      } else {
        az2::printBtn(Serial, c.label[0], raw);
        az2::printBtn(Serial1, c.label[0], raw);
      }
    }
  }
}

// Les 3 "potards" sont en realite des encodeurs rotatifs incrementaux
// avec bouton poussoir integre (type EC11/KY-040), pas de simples
// potentiometres lineaires -- precise le 2026-09-15 ("c'est des
// encodeurs rotatifs avec un bouton"). Chaque encodeur = 2 broches en
// quadrature (CLK/DT, decodees par la lib PJRC Encoder deja fournie
// par le coeur Teensy -- interruptions materielles, disponibles sur
// TOUTES les broches du Teensy 4.x, aucune restriction comme sur AVR)
// + 1 broche SW (bouton, meme debounce digital que la croix/A-D
// ci-dessus). Voir AZ2_CABLAGE_MASTER.md pour le tableau de brochage
// complet.
constexpr int kEncClkPins[3] = {14, 17, 22};  // A0, A3, A8
constexpr int kEncDtPins[3] = {15, 18, 24};   // A1, A4, A10
constexpr int kEncSwPins[3] = {16, 19, 25};   // A2, A5, A11

Encoder rotaryEncoders[3] = {
    Encoder(kEncClkPins[0], kEncDtPins[0]),
    Encoder(kEncClkPins[1], kEncDtPins[1]),
    Encoder(kEncClkPins[2], kEncDtPins[2]),
};

struct EncoderButton {
  uint8_t index;
  int pin;
  bool state = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;
};

EncoderButton encoderButtons[3] = {
    {0, kEncSwPins[0]},
    {1, kEncSwPins[1]},
    {2, kEncSwPins[2]},
};

void setupEncoderButtons() {
  for (EncoderButton &b : encoderButtons) {
    pinMode(b.pin, INPUT_PULLUP);
  }
}

// Le clic n'a pas de fonction musicale assignee pour l'instant -- juste
// transmis (ENC:<0-2>:DOWN/UP, voir AZ2_Protocol.h), meme principe que
// BTN:C/BTN:D deja libres pour un usage futur.
void updateEncoderButtons() {
  const uint32_t now = millis();
  for (EncoderButton &b : encoderButtons) {
    const bool raw = digitalRead(b.pin) == LOW;  // pull-up : appuye = LOW
    if (raw != b.lastRaw) {
      b.lastRaw = raw;
      b.lastChangeMs = now;
    }
    if ((now - b.lastChangeMs) >= kLocalDebounceMs && raw != b.state) {
      b.state = raw;
      az2::printEnc(Serial, b.index, raw);
      az2::printEnc(Serial1, b.index, raw);
    }
  }
}

// La lib Encoder compte generalement 4 transitions par cran mecanique
// sur les modules EC11/KY-040 courants -- kEncCountsPerDetent divise ce
// brut en "crans" ; kEncStepPerDetent est l'amplitude (sur 0-127)
// ajoutee/retiree par cran. kEncDirection inverse le sens si besoin --
// premier test reel le 2026-09-15 ("les potentiometres doivent
// fonctionner dans l'autre sens") : tourner a droite faisait baisser la
// valeur avec kEncClkPins/kEncDtPins tels que cables -- corrige ici en
// logiciel plutot que d'inverser CLK/DT au fer a souder.
constexpr int32_t kEncCountsPerDetent = 4;
constexpr int32_t kEncStepPerDetent = 2;
constexpr int32_t kEncDirection = -1;

// BUG REEL trouve le 2026-09-15 (encodeurs cables, premier test) : sans
// toucher aux encodeurs, POT:0-2 derivait en continu de +/-1-2 unites
// (rebond electrique/mecanique typique des encodeurs bon marche --
// chaque micro-rebond sur CLK/DT est une "vraie" transition de
// quadrature pour la lib Encoder, donc compte comme un mini-mouvement).
// Fix : n'accepter une nouvelle position que si le brut est reste
// STABLE au moins kEncSettleMs -- un vrai cran humain reste en place
// bien plus longtemps que ca, un rebond electrique non.
int32_t encLastDetents[3] = {0, 0, 0};       // derniere position ACCEPTEE
int32_t encPendingDetents[3] = {0, 0, 0};    // derniere position BRUTE vue
uint32_t encPendingSinceMs[3] = {0, 0, 0};   // depuis quand le brut est a cette valeur
constexpr uint32_t kEncSettleMs = 5;

uint8_t encValue[3] = {100, 0, 0};  // valeurs de depart : volume audible, effets a 0
uint8_t potLastSent[3] = {255, 255, 255};  // 255 = jamais envoye
uint32_t potLastSentMs[3] = {};
// Evenements discrets (un cran = un delta net), pas de bruit ADC a
// lisser comme avec de vrais potards -- limite courte, juste pour eviter
// de saturer le port serie sur une rotation tres rapide.
constexpr uint32_t kPotMinIntervalMs = 20;

void updateEncoders() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 3; ++i) {
    const int32_t rawDetents = rotaryEncoders[i].read() / kEncCountsPerDetent;
    if (rawDetents != encPendingDetents[i]) {
      encPendingDetents[i] = rawDetents;
      encPendingSinceMs[i] = now;
    }
    if (rawDetents != encLastDetents[i] && (now - encPendingSinceMs[i]) >= kEncSettleMs) {
      const int32_t delta = (rawDetents - encLastDetents[i]) * kEncDirection;
      encLastDetents[i] = rawDetents;
      const int32_t next = static_cast<int32_t>(encValue[i]) + delta * kEncStepPerDetent;
      encValue[i] = static_cast<uint8_t>(constrain(next, 0, 127));
    }

    if (encValue[i] == potLastSent[i]) {
      continue;
    }
    if (now - potLastSentMs[i] < kPotMinIntervalMs) {
      continue;
    }
    potLastSent[i] = encValue[i];
    potLastSentMs[i] = now;

    az2::printPot(Serial, i, encValue[i]);
    az2::printPot(Serial1, i, encValue[i]);

    const float unit = static_cast<float>(encValue[i]) / 127.0f;
    switch (i) {
      case 0: masterVolume = unit; break;
      case 1: reverbWet = unit; break;
      case 2: delayWet = unit; break;
    }
    applyMasterMix();
  }
}

void updateLocalControls() {
  updateDigitalControls();
  updateEncoders();
  updateEncoderButtons();
}

// ---------------------------------------------------------------------
// Voix live (jeu au clavier depuis les pads / la page AUDIO de l'ecran) --
// separee des pistes du sequenceur.
// ---------------------------------------------------------------------
void handlePadCommand(const String &line) {
  const int firstColon = line.indexOf(':');
  const int secondColon = line.indexOf(':', firstColon + 1);
  if (firstColon < 0 || secondColon < 0) {
    sendCommandError("PAD", "MALFORMED");
    return;
  }

  const uint8_t pad = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
  if (!az2::validPad(pad)) {
    sendCommandError("PAD", "OUT_OF_RANGE");
    return;
  }

  const uint8_t note = padToMidiNote(pad);
  const bool pressed = line.indexOf(":DOWN") > 0;

  if (pressed) {
    uint8_t velocity = 100;
    const int velIdx = line.indexOf("vel=");
    if (velIdx >= 0) {
      velocity = static_cast<uint8_t>(line.substring(velIdx + 4).toInt());
    }
    liveVoice.keydown(note, velocity);
    announceLed(pad, "ON");
  } else {
    liveVoice.keyup(note);
    announceLed(pad, "OFF");
  }
}

// MIDI IN (priorite #6 de la liste indispensable,
// AZ2_BENCHMARK_CONCURRENCE.md) -- USB_MIDI_SERIAL deja actif dans
// platformio.ini (usbtype du Teensy), rien a cabler, `usbMIDI` est
// fourni par le core des que ce mode USB est choisi. MVP volontairement
// modeste : notes MIDI (n'importe quel canal) declenchent la voix live
// `liveVoice`, meme chemin que les pads tactiles/ecran -- pas de
// synchro d'horloge MIDI, pas de MIDI OUT, pas de routage vers une
// piste du sequenceur pour l'instant (voir AZ2_FEUILLE_DE_ROUTE.md).
void updateMidiIn() {
  while (usbMIDI.read()) {
    const uint8_t type = usbMIDI.getType();
    const uint8_t note = usbMIDI.getData1();
    const uint8_t velocity = usbMIDI.getData2();
    if (type == usbMIDI.NoteOn && velocity > 0) {
      liveVoice.keydown(note, velocity);
    } else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && velocity == 0)) {
      // velocity 0 sur un NoteOn = note-off (convention MIDI standard,
      // beaucoup de controleurs l'envoient ainsi plutot qu'un vrai
      // message NoteOff).
      liveVoice.keyup(note);
    }
  }
}

void handleMacroCommand(const String &line) {
  const int firstColon = line.indexOf(':');
  const int secondColon = line.indexOf(':', firstColon + 1);
  if (firstColon < 0 || secondColon < 0) {
    sendCommandError("MACRO", "MALFORMED");
    return;
  }

  const uint8_t index = static_cast<uint8_t>(line.substring(firstColon + 1, secondColon).toInt());
  const int32_t delta = line.substring(secondColon + 1).toInt();

  if (index == 1) {
    const int32_t updated = constrain(static_cast<int32_t>(transposeSemitones) + delta, -24, 24);
    transposeSemitones = static_cast<int8_t>(updated);
  }
}

void handleRecCommand(const String &line);  // definie plus bas, pres de handleGbAudioPacket()

void handleCommand(const String &line) {
  if (line == az2::kPlay) {
    startSequencer();
    announceStatus(az2::kStatusPlaying);
    return;
  }

  if (line == az2::kStop) {
    stopSequencer();
    liveVoice.notesOff();
    announceStatus(az2::kStatusStopped);
    return;
  }

  if (line.startsWith("PAD:")) {
    handlePadCommand(line);
    return;
  }

  if (line == az2::kHelloControl) {
    announceHello();
    return;
  }

  if (line.startsWith("MACRO:")) {
    handleMacroCommand(line);
    relayLine(line);
    return;
  }

  if (line.startsWith("ENC:")) {
    relayLine(line);
    return;
  }

  if (line.startsWith("STEP:")) {
    handleStepCommand(line);
    return;
  }

  if (line.startsWith("NOTE:")) {
    handleNoteCommand(line);
    return;
  }

  if (line.startsWith("INST:")) {
    handleInstCommand(line);
    return;
  }

  if (line.startsWith("SFX:")) {
    handleStepFxCommand(line);
    return;
  }

  if (line.startsWith("PROB:")) {
    handleProbCommand(line);
    return;
  }

  if (line.startsWith("COND:")) {
    handleCondCommand(line);
    return;
  }

  if (line.startsWith("FILL:")) {
    handleFillCommand(line);
    return;
  }

  if (line.startsWith("PATTERN:")) {
    handlePatternCommand(line);
    return;
  }

  if (line.startsWith("SONGSET:")) {
    handleSongSetCommand(line);
    return;
  }

  if (line.startsWith("SONGLEN:")) {
    handleSongLenCommand(line);
    return;
  }

  if (line.startsWith("SONGMODE:")) {
    handleSongModeCommand(line);
    return;
  }

  if (line.startsWith("BPM:")) {
    handleBpmCommand(line);
    return;
  }

  if (line.startsWith("DIV:")) {
    handleDivCommand(line);
    return;
  }

  if (line.startsWith("SWING:")) {
    handleSwingCommand(line);
    return;
  }

  if (line.startsWith("METRO:")) {
    metronomeEnabled = line.substring(6).toInt() != 0;
    relayLine(line);
    return;
  }

  // TEST:<piste>:<note MIDI>:<0|1> -- declenche/coupe une note
  // DIRECTEMENT sur une piste, HORS sequenceur (2026-09-19, "il faut
  // utiliser le bouton B pour jouer une note qu'on entende les
  // modifications" -- la page PATCH n'avait aucun moyen d'entendre le
  // patch en cours de reglage sans passer par le sequenceur/PLAY).
  // Meme trackNoteOn()/trackNoteOff() que le sequenceur, appeles
  // directement -- fonctionne que PLAY tourne ou non.
  if (line.startsWith("TEST:")) {
    const int idx1 = line.indexOf(':');
    const int idx2 = line.indexOf(':', idx1 + 1);
    const int idx3 = line.indexOf(':', idx2 + 1);
    if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
      sendCommandError("TEST", "MALFORMED");
      return;
    }
    const uint8_t track = static_cast<uint8_t>(line.substring(idx1 + 1, idx2).toInt());
    const int note = line.substring(idx2 + 1, idx3).toInt();
    const bool on = line.substring(idx3 + 1).toInt() != 0;
    if (track >= kTrackCount || note < 0 || note > 127) {
      sendCommandError("TEST", "OUT_OF_RANGE");
      return;
    }
    if (on) {
      trackNoteOn(track, static_cast<uint8_t>(note), 100);
    } else {
      trackNoteOff(track, static_cast<uint8_t>(note));
    }
    return;
  }

  if (line.startsWith("ENGINE:")) {
    handleEngineCommand(line);
    return;
  }

  if (line.startsWith("PATCH:")) {
    handlePatchCommand(line);
    return;
  }

  if (line == "PSRAM?") {
    checkPsram();
    return;
  }

  if (line == "HEAP?") {
    checkHeap();
    return;
  }

  if (line.startsWith("HEAPTEST:")) {
    const uint32_t kb = static_cast<uint32_t>(line.substring(line.indexOf(':') + 1).toInt());
    checkHeapTest(kb * 1024);
    return;
  }

  if (line.startsWith("FX:")) {
    handleFxCommand(line);
    return;
  }

  if (line.startsWith("VOL:")) {
    handleVolCommand(line);
    return;
  }

  if (line.startsWith("MUTE:")) {
    handleMuteCommand(line);
    return;
  }

  if (line.startsWith("SOLO:")) {
    handleSoloCommand(line);
    return;
  }

  if (line.startsWith("FILT:")) {
    handleFiltCommand(line);
    return;
  }

  if (line.startsWith("ENV:")) {
    handleEnvCommand(line);
    return;
  }

  if (line.startsWith("DXP:")) {
    handleDexedParamCommand(line);
    return;
  }

  if (line.startsWith("DXR:") || line.startsWith("DXR?")) {
    handleDexedRawCommand(line);
    return;
  }

  if (line.startsWith("EXP:") || line.startsWith("EXP?")) {
    handleEPianoParamCommand(line);
    return;
  }

  if (line.startsWith("BXP:")) {
    handleBraidsParamCommand(line);
    return;
  }

  if (line.startsWith("SCOPE:")) {
    handleScopeCommand(line);
    return;
  }

  if (line.startsWith("REC:")) {
    handleRecCommand(line);
    return;
  }

  if (line == "CPU?") {
    reportCpuUsage();
    return;
  }

  // SIMNAV:<UP|DOWN|LEFT|RIGHT>:<0|1> et SIMBTN:<A|B|C|D>:<0|1> --
  // injectent un evenement croix/bouton EXACTEMENT comme
  // updateDigitalControls() le fait pour un vrai appui physique (meme
  // az2::printNav()/printBtn(), meme cible Serial1, seul chemin par
  // lequel l'ESP32 recoit ces evenements -- voir kNavUpPin etc. plus
  // haut). Outil de test ajoute le 2026-09-18 ("tu peut tester toi") --
  // permet de piloter/valider le tracker (navigation, edition de
  // valeur) via la liaison USB du Teensy, sans avoir les mains sur le
  // vrai clavier physique. Ne passe PAS par le debounce ni l'etat local
  // (localControls[]) -- une injection directe, pas un remplacement
  // permanent du vrai cablage.
  if (line.startsWith("SIMNAV:")) {
    const int idx1 = line.indexOf(':');
    const int idx2 = line.indexOf(':', idx1 + 1);
    if (idx1 < 0 || idx2 < 0) {
      sendCommandError("SIMNAV", "MALFORMED");
      return;
    }
    const String dir = line.substring(idx1 + 1, idx2);
    const bool pressed = line.substring(idx2 + 1).toInt() != 0;
    az2::printNav(Serial1, dir.c_str(), pressed);
    return;
  }

  if (line.startsWith("SIMBTN:")) {
    const int idx1 = line.indexOf(':');
    const int idx2 = line.indexOf(':', idx1 + 1);
    if (idx1 < 0 || idx2 < 0 || line.substring(idx1 + 1, idx2).length() != 1) {
      sendCommandError("SIMBTN", "MALFORMED");
      return;
    }
    const char letter = line.charAt(idx1 + 1);
    const bool pressed = line.substring(idx2 + 1).toInt() != 0;
    az2::printBtn(Serial1, letter, pressed);
    return;
  }
}

// ---------------------------------------------------------------------
// Son de l'emulateur GB (ESP32 -> Teensy, voir AZ2_Protocol.h et
// gb_emulator.cpp cote ESP32) -- demande le 2026-09-15 ("il faut un
// emulateur complet classe ... envoyer sous forme de paquet ... pour
// que le DAC le joue"). Paquets binaires [magic][longueur][PCM mono 8
// bits non signe a kGbAudioSampleRate Hz] mele au flux texte habituel
// sur Serial1 -- voir AudioRxState/readStream() plus bas pour la
// reconnaissance de l'octet magique AVANT accumulation de ligne texte.
//
// Re-echantillonnage simple (repetition ponderee par un accumulateur de
// phase, pas d'interpolation fine -- suffisant pour des formes d'onde
// aussi simples que celles du GB) de 8kHz vers 44.1kHz (frequence native
// de la lib Audio Teensy), 8 bits non signe -> 16 bits signe, pousse
// dans un anneau puis servi a gbAudioQueue (voir plus haut) par blocs de
// AUDIO_BLOCK_SAMPLES (128, definis par la lib Audio).
constexpr size_t kGbRingCapacity = 2048;  // ~46ms a 44.1kHz, large marge face a la gigue serie
int16_t gbRing[kGbRingCapacity];
size_t gbRingHead = 0;
size_t gbRingTail = 0;

void gbRingPush(int16_t sample) {
  const size_t next = (gbRingHead + 1) % kGbRingCapacity;
  if (next == gbRingTail) {
    return;  // anneau plein -- echantillon perdu plutot que bloquer (micro-glitch tolere)
  }
  gbRing[gbRingHead] = sample;
  gbRingHead = next;
}

// ---------------------------------------------------------------------
// Sampler : enregistrer le son de l'emulateur GB dans un fichier .wav
// sur la carte SD DEDIEE du Teensy (BUILTIN_SDCARD, voir
// setupSampleSd() -- distincte de celle de l'ESP32 qui garde les ROM).
// Demande d'origine (2026-09-15, "la possibilite de sampler la Game Boy
// avec un des boutons des encodeurs et les samples se rangent direct
// dans la SD du Teensy") + precisee le 2026-09-17 ("REC/STOP, ... une
// routine pour capter les sons de l'emulateur"). Capture au format
// NATIF de la source (8kHz mono 16 bits signe, avant le
// sur-echantillonnage vers 44.1kHz fait pour gbRing/gbAudioQueue plus
// haut) -- fichiers plus petits, honnete sur la vraie qualite de la
// source, pas de perte a upsampler puis re-downsampler plus tard.
//
// Phase 1 (2026-09-17) : demarrer/arreter + ecrire un .wav nomme
// automatiquement. PAS FAIT ICI (demande plus large, pas encore
// implementee, voir AZ2_FEUILLE_DE_ROUTE.md) : decoupage tactile de
// l'onde, decoupage automatique, clavier de nom personnalise, vue
// d'onde en direct pendant l'enregistrement -- tout ca cote ESP32,
// viendra une fois cette base testee en reel.
// ---------------------------------------------------------------------
bool gbRecording = false;
File gbRecFile;
uint32_t gbRecSampleCount = 0;
int16_t gbRecBuf[512];
size_t gbRecBufLen = 0;
// Garde-fou phase 1 -- arrete tout seul plutot que de remplir la carte
// ou de tourner indefiniment si jamais l'utilisateur oublie STOP.
constexpr uint32_t kGbRecMaxSamples = az2::kGbAudioSampleRate * 30;  // 30s

void gbRecFlushBuf() {
  if (gbRecBufLen == 0 || !gbRecFile) {
    return;
  }
  gbRecFile.write(reinterpret_cast<const uint8_t *>(gbRecBuf), gbRecBufLen * sizeof(int16_t));
  gbRecSampleCount += static_cast<uint32_t>(gbRecBufLen);
  gbRecBufLen = 0;
}

// En-tete WAV PCM standard (44 octets), mono 16 bits, kGbAudioSampleRate
// -- ecrit deux fois : un placeholder a l'ouverture (tailles a 0, pour
// que le fichier ait deja la bonne forme si jamais on plante avant
// STOP), puis reecrit avec les vraies tailles a la fermeture (seek(0)).
void writeWavHeader(File &f, uint32_t dataBytes) {
  uint8_t h[44] = {};
  const uint32_t riffSize = 36 + dataBytes;
  const uint32_t sampleRate = az2::kGbAudioSampleRate;
  const uint32_t byteRate = sampleRate * 2;  // mono, 16 bits = 2 octets/echantillon
  memcpy(h, "RIFF", 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  h[16] = 16;                  // taille du sous-bloc fmt
  h[20] = 1;                   // format = PCM
  h[22] = 1;                   // 1 canal (mono)
  memcpy(h + 24, &sampleRate, 4);
  memcpy(h + 28, &byteRate, 4);
  h[32] = 2;                   // block align (octets par trame)
  h[34] = 16;                  // bits par echantillon
  memcpy(h + 36, "data", 4);
  memcpy(h + 4, &riffSize, 4);
  memcpy(h + 40, &dataBytes, 4);
  f.seek(0);
  f.write(h, sizeof(h));
}

// "SAMPLE_001.wav", "SAMPLE_002.wav"... premier numero libre dans
// /samples (cree si besoin).
String nextSampleName() {
  SD.mkdir("/samples");
  for (int n = 1; n <= 999; ++n) {
    char path[24];
    snprintf(path, sizeof(path), "/samples/SAMPLE_%03d.wav", n);
    if (!SD.exists(path)) {
      return String(path);
    }
  }
  return String("/samples/SAMPLE_999.wav");  // improbable (999 samples), ecrase plutot que planter
}

void gbRecStop();  // definie juste apres -- gbRecPush() l'appelle si le garde-fou est atteint

void gbRecPush(int16_t sample) {
  if (!gbRecording) {
    return;
  }
  gbRecBuf[gbRecBufLen++] = sample;
  if (gbRecBufLen >= sizeof(gbRecBuf) / sizeof(gbRecBuf[0])) {
    gbRecFlushBuf();
  }
  if (gbRecSampleCount + gbRecBufLen >= kGbRecMaxSamples) {
    gbRecStop();
  }
}

void gbRecStart() {
  if (gbRecording) {
    return;
  }
  const String path = nextSampleName();
  // SD.remove() avant open() -- meme convention que savePatchSlot()/
  // saveProject(). No-op si le fichier n'existe pas encore (cas normal,
  // nextSampleName() a trouve un nom libre) ; INDISPENSABLE dans le cas
  // improbable ou les 999 noms sont pris (nextSampleName() renvoie alors
  // SAMPLE_999.wav en le sachant deja pris) -- BUG REEL potentiel note
  // lors de l'audit du 2026-09-17, corrige ici : FILE_WRITE sur un
  // fichier EXISTANT ouvre en AJOUT (pas en ecrasement) sur SdFat, donc
  // sans ce remove() la nouvelle capture se serait ajoutee APRES
  // l'ancien contenu au lieu de le remplacer -- wav corrompu/demesure.
  SD.remove(path.c_str());
  gbRecFile = SD.open(path.c_str(), FILE_WRITE);
  if (!gbRecFile) {
    Serial.print("REC:ERROR:");
    Serial.println(path);
    Serial1.print("REC:ERROR:");
    Serial1.println(path);
    return;
  }
  uint8_t placeholder[44] = {};
  gbRecFile.write(placeholder, sizeof(placeholder));
  gbRecSampleCount = 0;
  gbRecBufLen = 0;
  gbRecording = true;
  Serial.print("REC:STARTED:");
  Serial.println(path);
  Serial1.print("REC:STARTED:");
  Serial1.println(path);
}

void gbRecStop() {
  if (!gbRecording) {
    return;
  }
  gbRecFlushBuf();
  writeWavHeader(gbRecFile, gbRecSampleCount * sizeof(int16_t));
  gbRecFile.close();
  gbRecording = false;
  Serial.print("REC:STOPPED:samples=");
  Serial.println(gbRecSampleCount);
  Serial1.print("REC:STOPPED:samples=");
  Serial1.println(gbRecSampleCount);
}

void handleRecCommand(const String &line) {
  if (line == "REC:START") {
    gbRecStart();
  } else if (line == "REC:STOP") {
    gbRecStop();
  }
}

float gbUpsamplePhase = 0.0f;
const float kGbSamplesOutPerIn = 44100.0f / static_cast<float>(az2::kGbAudioSampleRate);

void handleGbAudioPacket(const uint8_t *pcm, uint8_t len) {
  for (uint8_t i = 0; i < len; ++i) {
    const int16_t sample16 = static_cast<int16_t>((static_cast<int>(pcm[i]) - 128) << 8);
    gbRecPush(sample16);  // capture native 8kHz, avant le sur-echantillonnage ci-dessous
    gbUpsamplePhase += kGbSamplesOutPerIn;
    while (gbUpsamplePhase >= 1.0f) {
      gbRingPush(sample16);
      gbUpsamplePhase -= 1.0f;
    }
  }
}

// Vide l'anneau vers gbAudioQueue par blocs complets -- appelee depuis
// loop(), independamment du rythme d'arrivee des paquets serie.
void feedGbAudioQueue() {
  while (gbAudioQueue.available()) {
    const size_t ready = (gbRingHead >= gbRingTail) ? (gbRingHead - gbRingTail)
                                                     : (kGbRingCapacity - gbRingTail + gbRingHead);
    if (ready < AUDIO_BLOCK_SAMPLES) {
      break;
    }
    int16_t *buf = gbAudioQueue.getBuffer();
    if (buf == nullptr) {
      break;
    }
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
      buf[i] = gbRing[gbRingTail];
      gbRingTail = (gbRingTail + 1) % kGbRingCapacity;
    }
    gbAudioQueue.playBuffer();
  }
}

// Etat de reception binaire (paquets audio GB), UNIQUEMENT pour Serial1
// (ESP32) -- Serial (USB) n'en recoit jamais, voir readSerialCommands().
//
// [2026-09-18] Meme classe de bug que le tracer SCOPE cote ESP32 (voir
// ScopeRxState dans src_esp32/az2_screen/main.cpp et AZ2_ETAT_DES_LIEUX.md
// "bruit blanc...") : sans verification de longueur ni delai d'abandon,
// un octet perdu quelque part decale durablement la lecture "longueur"
// sur un octet de charge utile arbitraire, qui peut a son tour valoir
// par hasard kGbAudioPacketMagic et relancer un faux paquet -- corrige
// ici en verifiant que la longueur recue correspond EXACTEMENT a
// kGbAudioSamplesPerPacket (le paquet envoye par gb_emulator.cpp cote
// ESP32 a toujours cette taille fixe) et en abandonnant un paquet reste
// "ouvert" trop longtemps.
struct AudioRxState {
  bool inPacket = false;
  bool haveLen = false;
  uint8_t len = 0;
  uint8_t pos = 0;
  uint32_t lastByteMs = 0;
  uint8_t buf[255];
};
AudioRxState gbAudioRx;

// Un paquet complet (magic+longueur+kGbAudioSamplesPerPacket octets)
// tient en <1 ms a kControlBaud -- 20 ms est tres large, ne se declenche
// que si le lien est vraiment bloque/coupe au milieu d'un paquet.
constexpr uint32_t kAudioRxTimeoutMs = 20;

void readStream(Stream &in, String &lineBuffer, AudioRxState *audioState) {
  if (audioState != nullptr && audioState->inPacket &&
      (millis() - audioState->lastByteMs) > kAudioRxTimeoutMs) {
    audioState->inPacket = false;
  }

  while (in.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(in.read());

    if (audioState != nullptr) {
      if (audioState->inPacket) {
        audioState->lastByteMs = millis();
        if (!audioState->haveLen) {
          audioState->len = b;
          audioState->pos = 0;
          audioState->haveLen = true;
          // Longueur toujours fixe en pratique (voir
          // kGbAudioSamplesPerPacket) -- toute autre valeur signifie
          // qu'on a perdu l'alignement (magic tombe par hasard dans de
          // la charge utile bidon) : on rejette tout de suite au lieu
          // d'avaler N octets arbitraires en payload, ce qui ne ferait
          // qu'aggraver le desalignement.
          if (audioState->len != az2::kGbAudioSamplesPerPacket || audioState->len == 0) {
            audioState->inPacket = false;
          }
          continue;
        }
        audioState->buf[audioState->pos++] = b;
        if (audioState->pos >= audioState->len) {
          handleGbAudioPacket(audioState->buf, audioState->len);
          audioState->inPacket = false;
        }
        continue;
      }
      if (b == az2::kGbAudioPacketMagic) {
        audioState->inPacket = true;
        audioState->haveLen = false;
        audioState->lastByteMs = millis();
        continue;
      }
    }

    const char c = static_cast<char>(b);
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      lineBuffer.trim();
      if (lineBuffer.length() > 0) {
        handleCommand(lineBuffer);
      }
      lineBuffer = "";
      continue;
    }

    if (lineBuffer.length() < 96) {
      lineBuffer += c;
    }
  }
}

void readSerialCommands() {
  readStream(Serial, usbLine, nullptr);
  readStream(Serial1, espLine, &gbAudioRx);
}

// Verifie que la/les puce(s) PSRAM soudees sont bien detectees et
// utilisables (ecriture/lecture reelle, pas juste la taille annoncee au
// boot). Imprime le resultat sur les 3 flux -- voir docs/AZ2_SAMPLEUR.md.
// Carte SD integree au Teensy 4.1 (slot physique sur la carte, pas de
// cablage requis -- voir docs/AZ2_FEUILLE_DE_ROUTE_MOTEUR.md etape 6,
// "sampleur reel, une fois une carte SD presente sur le Teensy").
// Utilisee pour stocker les samples captures depuis l'emulateur GB
// (demande 2026-09-15, "les samples se rangent direct dans la sd du
// teensy") -- distincte de la carte SD de l'ESP32 (qui garde les ROM).
// Echec propre attendu tant qu'aucune carte n'est inseree.
void setupSampleSd() {
  if (SD.begin(BUILTIN_SDCARD)) {
    const uint32_t sizeMb = static_cast<uint32_t>(SD.totalSize() / (1024ULL * 1024ULL));
    Serial.print("SDTEENSY:READY:size_mb=");
    Serial.println(sizeMb);
    Serial1.print("SDTEENSY:READY:size_mb=");
    Serial1.println(sizeMb);
  } else {
    Serial.println("SDTEENSY:NOT_PRESENT");
    Serial1.println("SDTEENSY:NOT_PRESENT");
  }
}

void checkPsram() {
  Serial.print("AZ2:PSRAM:detected_mb=");
  Serial.println(external_psram_size);

  if (external_psram_size == 0) {
    Serial.println("AZ2:PSRAM:ERROR_NOT_DETECTED");
    return;
  }

  for (size_t i = 0; i < sizeof(psramTestBuffer); ++i) {
    psramTestBuffer[i] = static_cast<uint8_t>(i & 0xFF);
  }
  bool ok = true;
  for (size_t i = 0; i < sizeof(psramTestBuffer); ++i) {
    if (psramTestBuffer[i] != static_cast<uint8_t>(i & 0xFF)) {
      ok = false;
      break;
    }
  }
  Serial.println(ok ? "AZ2:PSRAM:READ_WRITE_OK" : "AZ2:PSRAM:READ_WRITE_FAILED");
}

// Diagnostic tas C++ (2026-09-18, piste sur le souffle DEXED) : chaque
// AudioSynthDexed alloue ses voix (Dx7Note, ~692 octets chacune) via `new`
// SANS AUCUNE verification d'echec (voir Dexed::Dexed() dans
// src_teensy/microdexed-touch/third-party/Synth_Dexed/src/dexed.cpp) -- si
// `new` echoue silencieusement (tas sature), le pointeur qui en resulte
// pointe n'importe ou, et la lecture qui suit peut produire n'importe quoi
// -- coherent avec "bruit au lieu d'une note". AZ-2 cree 9 instances
// completes de Dexed (8 pistes + liveVoice), plus gourmand que
// l'architecture MicroDexed-touch d'origine. mallinfo() donne uordblks
// (tas utilise) / fordblks (libre dans les blocs deja obtenus du systeme)
// -- pas une preuve definitive (l'allocateur newlib peut aussi juste
// demander plus de RAM au systeme), mais un fordblks proche de 0 au boot,
// APRES construction des 9 Dexed, serait un signal fort.
void checkHeap() {
  struct mallinfo mi = mallinfo();
  Serial.print("AZ2:HEAP:used=");
  Serial.print(mi.uordblks);
  Serial.print(":free_in_arena=");
  Serial.print(mi.fordblks);
  Serial.print(":arena=");
  Serial.println(mi.arena);
}

// HEAPTEST:<Ko> -- alloue puis libere N Ko pour VERIFIER EMPIRIQUEMENT
// si le tas peut vraiment grandir au-dela de l'arene actuelle
// (mi.arena ci-dessus, ~36 Ko avant cette allocation), plutot que de
// deviner. Ajoute le 2026-09-18 ("on peut en ajouter [des moteurs]") --
// le rapport de taille du linker (voir teensy_size dans la sortie de
// build) annonce ~454 Ko libres en RAM2 pour malloc/new, largement plus
// que l'arene mallinfo() actuelle : ceci confirme si ce chiffre est
// reel (le tas grandit via _sbrk() jusqu'a _heap_end, voir startup.c
// cote framework Teensy -- mi.arena ne reflete que ce qui a deja ete
// demande, PAS un plafond dur) ou s'il y a un vrai mur avant.
void checkHeapTest(uint32_t bytes) {
  Serial.print("AZ2:HEAPTEST:requested=");
  Serial.println(bytes);
  checkHeap();
  void *p = malloc(bytes);
  if (p == nullptr) {
    Serial.println("AZ2:HEAPTEST:MALLOC_FAILED");
    return;
  }
  // Touche vraiment la memoire (pas juste reservee) -- ecrit puis relit
  // un motif, comme checkPsram() ci-dessus, pour exclure une page
  // "promise" mais pas utilisable.
  memset(p, 0xA5, bytes);
  bool ok = true;
  for (uint32_t i = 0; i < bytes; i += 4096) {
    if (static_cast<uint8_t *>(p)[i] != 0xA5) {
      ok = false;
      break;
    }
  }
  Serial.println(ok ? "AZ2:HEAPTEST:WRITE_READ_OK" : "AZ2:HEAPTEST:WRITE_READ_FAILED");
  checkHeap();
  free(p);
  checkHeap();
}

void sendStatus() {
  const uint32_t now = millis();
  if (now - lastStatusMs < 1000) {
    return;
  }

  lastStatusMs = now;
  announceStatus(playing ? az2::kStatusPlaying : az2::kStatusReady);
}

} // namespace

void setup() {
  Serial.begin(az2::kControlBaud);
  // Tampon RX agrandi AVANT begin() (sans effet apres) -- le defaut du
  // core Teensy est petit face a 921600 bauds (kControlBaud) et aux
  // paquets audio GB binaires qui arrivent en rafale sur Serial1 ; un
  // depassement perd des octets EN SILENCE et desynchronise le parseur
  // (voir AudioRxState plus haut, meme classe de bug que kScopeSamplesPerPacket
  // cote ESP32). static = duree de vie du programme, pas de la pile.
  static uint8_t serial1RxBuf[2048];
  Serial1.addMemoryForRead(serial1RxBuf, sizeof(serial1RxBuf));
  Serial1.begin(az2::kControlBaud);
  // Graine pour random() (PROB:, voir advanceTick()) -- micros() au boot
  // varie assez d'un demarrage a l'autre (delais SD/audio/etc. avant ici)
  // pour eviter de rejouer EXACTEMENT le meme motif "aleatoire" a chaque
  // mise sous tension. Pas une vraie source d'entropie, suffisant pour
  // une probabilite de declenchement (pas un usage cryptographique).
  randomSeed(micros());
  // 200 (au lieu de 48) depuis le passage a 8 pistes + le bus d'effets
  // maitre : AudioEffectDelay retient ses blocs dans ce pool partage,
  // proportionnellement au temps de delay configure (350ms ~= 121 blocs a
  // 44.1kHz/128) -- PAS dans une memoire dediee. Marge mesuree via CPU?
  // (voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md, "performance reelle").
  AudioMemory(200);

  // Son GB (voir gbAudioQueue plus haut) : NON_STALLING -- si l'anneau
  // envoie plus vite que la queue ne se vide (ne devrait pas arriver,
  // 80 blocs de marge sur Teensy 4.x), on ignore plutot que de bloquer
  // tout loop() (sequenceur, controles...) en attendant de la place.
  gbAudioQueue.setBehaviour(AudioPlayQueue::NON_STALLING);

  // init_braids() = init materielle obligatoire de la lib (osc.Init()),
  // sur LES 4 instances par piste (pas seulement celle active au boot) --
  // sinon une piste basculee sur Braids plus tard partirait non initialisee.
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    trackBraidsEngine[t].init_braids();
  }
  // Coeur FM explicite et reproductible. Le constructeur choisit MKI,
  // alors que MSFA est le coeur de reference historique de Dexed. Ce
  // changement fournit un test A/B propre pour le souffle constate sur
  // le materiel; DEXED reste hors des moteurs de boot jusqu'a validation.
  liveVoice.setEngineType(MSFA);
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    trackDexedEngine[t].setEngineType(MSFA);
  }
  // Enveloppe ADSR par defaut du moteur ANALOG (attaque/chute rapides,
  // maintien franc -- profil "synthe" standard, pas percussif).
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    trackAnalogEnv[t].attack(5.0f);
    trackAnalogEnv[t].decay(50.0f);
    trackAnalogEnv[t].sustain(0.7f);
    trackAnalogEnv[t].release(150.0f);
  }
  // Filtre par piste (voir trackFilter[]) grand ouvert par defaut --
  // le constructeur AudioFilterStateVariable part a 1000Hz (voir
  // filter_variable.h), ce qui couperait audiblement le son de TOUTES
  // les pistes des le premier boot si on ne le corrige pas ici.
  // 15000Hz (pas 18000Hz) : voir le commentaire de handleFiltCommand()
  // -- 18000Hz etait la VRAIE cause du "bruit blanc" attribue a tort
  // aux moteurs de synthese (2026-09-18), confirme sur le vrai
  // materiel identiquement sur les 5 moteurs.
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    trackFilter[t].frequency(15000.0f);
    trackFilter[t].resonance(0.7f);
  }
  loadDexedPatch(liveVoice, 0);  // "FM-Rhodes" plutot qu'un init_voice vide

  // Branche chaque piste sur son moteur/patch par defaut (voir
  // trackEngine[]/trackPatch[] plus haut : DEXED est volontairement
  // absent des moteurs de boot tant que le souffle n'est pas valide --
  // mais tout ceci est maintenant
  // changeable en direct via ENGINE:/PATCH:, voir handleEngineCommand()).
  for (uint8_t t = 0; t < kTrackCount; ++t) {
    setTrackEngine(t, trackEngine[t]);
  }

  seedDefaultNotes();

  mixFinal.gain(0, 0.8f);  // groupe pistes 0-3 (deja attenuees par groupMixer, voir setTrackEngine())
  mixFinal.gain(1, 0.8f);  // groupe pistes 4-7
  mixFinal.gain(2, 0.5f);  // voix live
  mixFinal.gain(3, 0.6f);  // metronome (voir triggerMetronome())

  // Enveloppe "clic" du metronome : pas de sustain, decay seul ramene
  // a zero -- une seule noteOn() par temps suffit, pas de noteOff() a
  // gerer (voir triggerMetronome()).
  metroClick.begin(WAVEFORM_SINE);
  metroEnv.attack(1.0f);
  metroEnv.decay(30.0f);
  metroEnv.sustain(0.0f);
  metroEnv.release(5.0f);

  // Bus d'effets maitre : sec a fond, reverb/delay a 0 par defaut tant
  // que les potards n'ont pas ete lus une premiere fois (voir
  // updateLocalControls()) -- pour ne pas surprendre au premier boot
  // avec un effet impose avant meme la premiere lecture ADC.
  reverbUnit.roomsize(0.6f);
  reverbUnit.damping(0.4f);
  delayUnit.delay(0, 350.0f);  // temps fixe en v1, cf feuille de route pour le rendre reglable
  applyMasterMix();

  setupLocalControls();
  setupSampleSd();

  checkPsram();
  checkHeap();  // 2026-09-18 -- piste sur le souffle DEXED, voir le commentaire de checkHeap()

  delay(300);
  Serial.println("AZ2:ENGINE:SYNTH_DEXED_MULTIVOICE");
  Serial.print("AZ2:SEQUENCER:tracks=");
  Serial.print(kTrackCount);
  Serial.print(":steps=");
  Serial.println(kStepCount);
  announceHello();
}

void loop() {
  readSerialCommands();
  updateMidiIn();
  feedGbAudioQueue();
  updateScope();
  updateSequencer();
  updateLocalControls();
  sendStatus();
}
