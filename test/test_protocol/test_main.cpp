// Tests unitaires natifs (tournent sur CETTE machine, pas sur du materiel
// AZ2 -- voir env:native dans platformio.ini) pour la logique PURE de
// lib/AZ2_Protocol/AZ2_Protocol.h : encodage/decodage des conditions de
// declenchement par pas (PROB:/COND:, ajoutees le 2026-09-17), et quelques
// autres helpers sans dependance materielle (division/moteur/pad).
//
// Premier test AZ2 automatise (voir "Tests et integration continue :
// absents pour AZ-2, risque eleve" dans AZ2_AUDIT_TECHNIQUE_COMPLET_
// 2026-09-17.md) -- ne remplace pas une verification sur materiel reel
// (rien ici ne touche a l'audio/l'ecran/le tactile), mais couvre au moins
// la logique de calcul qui ne depend d'aucun peripherique.
//
// ArduinoFake fournit un Arduino.h/Print factice pour que AZ2_Protocol.h
// (ecrit pour Arduino, pas pour du code natif) compile ici sans rien
// changer au header lui-meme -- ce sont les memes fonctions que celles
// utilisees par les 2 vrais firmwares, pas une reimplementation separee.
#include <ArduinoFake.h>
#include <unity.h>

#include <AZ2_Protocol.h>
#include "../../src_teensy/az2_audio/sampler_math.h"

void test_step_condition_always_is_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, az2::kStepCondAlways);
  TEST_ASSERT_TRUE(az2::stepConditionMet(az2::kStepCondAlways, 0, false));
  TEST_ASSERT_TRUE(az2::stepConditionMet(az2::kStepCondAlways, 12345, true));
}

void test_step_condition_encode_decode_ratio() {
  // 1:2 -- joue le 1er passage sur 2 (loopCount pair, 0-indexe).
  const uint8_t oneOfTwo = az2::stepConditionEncode(1, 2);
  TEST_ASSERT_TRUE(az2::stepConditionMet(oneOfTwo, 0, false));
  TEST_ASSERT_FALSE(az2::stepConditionMet(oneOfTwo, 1, false));
  TEST_ASSERT_TRUE(az2::stepConditionMet(oneOfTwo, 2, false));
  TEST_ASSERT_FALSE(az2::stepConditionMet(oneOfTwo, 3, false));

  // 2:2 -- l'inverse exact de 1:2.
  const uint8_t twoOfTwo = az2::stepConditionEncode(2, 2);
  TEST_ASSERT_FALSE(az2::stepConditionMet(twoOfTwo, 0, false));
  TEST_ASSERT_TRUE(az2::stepConditionMet(twoOfTwo, 1, false));

  // 3:4 -- ne joue qu'au 3e passage sur 4 (index 2 dans un cycle 0-3).
  const uint8_t threeOfFour = az2::stepConditionEncode(3, 4);
  for (uint32_t loop = 0; loop < 12; ++loop) {
    const bool expected = (loop % 4) == 2;
    TEST_ASSERT_EQUAL_MESSAGE(expected, az2::stepConditionMet(threeOfFour, loop, false),
                               "3:4 doit jouer uniquement quand loopCount%4==2");
  }
}

void test_step_condition_encode_rejects_out_of_range() {
  // K > N, N > 8 ou K/N == 0 -> retombe sur kStepCondAlways plutot que de
  // produire un octet invalide (voir le commentaire de stepConditionEncode()).
  TEST_ASSERT_EQUAL_UINT8(az2::kStepCondAlways, az2::stepConditionEncode(3, 2));
  TEST_ASSERT_EQUAL_UINT8(az2::kStepCondAlways, az2::stepConditionEncode(0, 4));
  TEST_ASSERT_EQUAL_UINT8(az2::kStepCondAlways, az2::stepConditionEncode(1, 0));
  TEST_ASSERT_EQUAL_UINT8(az2::kStepCondAlways, az2::stepConditionEncode(1, 9));
}

void test_step_condition_fill_and_not_fill() {
  TEST_ASSERT_TRUE(az2::stepConditionMet(az2::kStepCondFill, 0, true));
  TEST_ASSERT_FALSE(az2::stepConditionMet(az2::kStepCondFill, 0, false));
  TEST_ASSERT_TRUE(az2::stepConditionMet(az2::kStepCondNotFill, 0, false));
  TEST_ASSERT_FALSE(az2::stepConditionMet(az2::kStepCondNotFill, 0, true));
}

void test_step_condition_label_matches_encoding() {
  char buf[8];

  az2::stepConditionLabel(az2::kStepCondAlways, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("---", buf);

  az2::stepConditionLabel(az2::kStepCondFill, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("FILL", buf);

  az2::stepConditionLabel(az2::kStepCondNotFill, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("!FIL", buf);

  az2::stepConditionLabel(az2::stepConditionEncode(2, 4), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("2:4", buf);
}

void test_step_condition_cycle_contains_only_valid_entries() {
  // Chaque entree du cycle propose a l'ecran (voir seqDetailCol cote
  // ESP32) doit etre soit une des 2 valeurs speciales, soit un octet que
  // stepConditionEncode() aurait vraiment pu produire (K<=N<=8) -- sinon
  // le cycle afficherait un octet que l'utilisateur ne pourrait pas
  // re-obtenir en repartant de zero.
  for (uint8_t i = 0; i < az2::kStepConditionCycleCount; ++i) {
    const uint8_t cond = az2::kStepConditionCycle[i];
    if (cond == az2::kStepCondAlways || cond == az2::kStepCondFill || cond == az2::kStepCondNotFill) {
      continue;
    }
    const uint8_t n = static_cast<uint8_t>(cond >> 4);
    const uint8_t k = static_cast<uint8_t>(cond & 0x0F);
    TEST_ASSERT_TRUE_MESSAGE(k >= 1 && k <= n && n <= 8, "octet du cycle hors du domaine valide");
    TEST_ASSERT_EQUAL_UINT8(cond, az2::stepConditionEncode(k, n));
  }
}

void test_division_label_known_values() {
  TEST_ASSERT_EQUAL_STRING("1/16", az2::divisionLabel(4));  // valeur d'origine (pas de swing)
  TEST_ASSERT_EQUAL_STRING("1/4", az2::divisionLabel(1));
  TEST_ASSERT_EQUAL_STRING("?", az2::divisionLabel(99));  // valeur non presente dans kDivisionOptions
}

void test_pad_id_and_valid_pad() {
  TEST_ASSERT_EQUAL_UINT8(0, az2::padId(0, 0));
  TEST_ASSERT_EQUAL_UINT8(5, az2::padId(1, 1));  // ligne 1, colonne 1 -> 1*4+1
  TEST_ASSERT_TRUE(az2::validPad(0));
  TEST_ASSERT_TRUE(az2::validPad(15));
  TEST_ASSERT_FALSE(az2::validPad(16));
}


void test_gb_audio_v2_stream_roundtrip() {
  az2::GbAudioV2Frame frame;
  frame.sequence = 65534;
  frame.sampleRate = 32000;
  frame.format = az2::kGbAudioV2FormatPcmU8;
  frame.flags = az2::kGbAudioV2FlagStereo;
  frame.payloadLen = 128;
  for (uint16_t i = 0; i < frame.payloadLen; ++i)
    frame.payload[i] = static_cast<uint8_t>((i * 37) & 0xff);
  uint8_t wire[az2::kGbAudioV2HeaderBytes + az2::kGbAudioV2MaxPayload +
               az2::kGbAudioV2CrcBytes] = {};
  const size_t size = az2::encodeGbAudioV2(wire, sizeof(wire), frame);
  TEST_ASSERT_EQUAL_UINT32(140, size);
  az2::GbAudioV2Decoder decoder;
  bool complete = false;
  for (size_t i = 0; i < size; ++i) {
    if (decoder.feed(wire[i])) {
      TEST_ASSERT_FALSE(complete);
      complete = true;
    }
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_UINT16(frame.sequence, decoder.frame.sequence);
  TEST_ASSERT_EQUAL_UINT16(frame.sampleRate, decoder.frame.sampleRate);
  TEST_ASSERT_EQUAL_UINT16(frame.payloadLen, decoder.frame.payloadLen);
  TEST_ASSERT_EQUAL_MEMORY(frame.payload, decoder.frame.payload, frame.payloadLen);
  TEST_ASSERT_EQUAL_UINT32(0, decoder.rejectedCrc);
}

void test_gb_audio_v2_reject_corruption_and_recover() {
  az2::GbAudioV2Frame frame;
  frame.sequence = 7;
  frame.sampleRate = 14000;
  frame.format = az2::kGbAudioV2FormatPcmU8;
  frame.payloadLen = 4;
  frame.payload[0] = 0x03; frame.payload[1] = 0xff;
  frame.payload[2] = 0x03; frame.payload[3] = 0x00;
  uint8_t wire[32] = {};
  const size_t size = az2::encodeGbAudioV2(wire, sizeof(wire), frame);
  TEST_ASSERT_EQUAL_UINT32(16, size);
  az2::GbAudioV2Decoder decoder;
  wire[11] ^= 0x80;
  for (size_t i = 0; i < size; ++i) TEST_ASSERT_FALSE(decoder.feed(wire[i]));
  TEST_ASSERT_EQUAL_UINT32(1, decoder.rejectedCrc);
  wire[11] ^= 0x80;
  bool complete = false;
  for (size_t i = 0; i < size; ++i) complete |= decoder.feed(wire[i]);
  TEST_ASSERT_TRUE(complete);
  // Broken header must be rejected before touching the payload buffer.
  wire[6] = 0xff; wire[7] = 0xff;
  for (size_t i = 0; i < az2::kGbAudioV2HeaderBytes; ++i)
    TEST_ASSERT_FALSE(decoder.feed(wire[i]));
  TEST_ASSERT_EQUAL_UINT32(1, decoder.rejectedHeaders);
  decoder.timeout();
  TEST_ASSERT_EQUAL_UINT32(0, decoder.timeouts);
}

void test_gb_audio_v2_endian_helpers() {
  uint8_t bytes[2] = {0, 0};
  az2::writeLe16(bytes, 0xBEEF);
  TEST_ASSERT_EQUAL_HEX8(0xEF, bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0xBE, bytes[1]);
  TEST_ASSERT_EQUAL_HEX16(0xBEEF, az2::readLe16(bytes));
}

void test_gb_audio_v2_crc16_known_vector() {
  const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, az2::crc16CcittFalse(data, sizeof(data)));
}

void test_gb_audio_v2_header_sanity() {
  uint8_t h[az2::kGbAudioV2HeaderBytes] = {};
  h[0] = az2::kGbAudioV2Magic;
  h[1] = az2::kGbAudioV2Version;
  h[2] = az2::kGbAudioV2FlagStereo;
  h[3] = az2::kGbAudioV2FormatPcmU8;
  az2::writeLe16(h + 4, 42);
  az2::writeLe16(h + 6, 512);
  az2::writeLe16(h + 8, 32000);
  TEST_ASSERT_TRUE(az2::gbAudioV2HeaderSane(h, sizeof(h)));

  h[3] = 99;
  TEST_ASSERT_FALSE(az2::gbAudioV2HeaderSane(h, sizeof(h)));
  h[3] = az2::kGbAudioV2FormatPcmS16Le;
  az2::writeLe16(h + 6, az2::kGbAudioV2MaxPayload + 1);
  TEST_ASSERT_FALSE(az2::gbAudioV2HeaderSane(h, sizeof(h)));
}

void test_engine_patch_count_and_name() {
  // 255 depuis le 2026-09-18 ("recuperer un max de patch") -- 255 des
  // 256 vraies voix d'usine du Yamaha DX7 original (ROM1-ROM4), pas
  // 256 : 0xFF/255 est deja le sentinel "pas d'override de patch par
  // pas" ailleurs dans le protocole (stepPatch/seqStepPatch), voir le
  // commentaire de kDexedPatchCount dans AZ2_Protocol.h.
  TEST_ASSERT_EQUAL_UINT16(255, az2::enginePatchCount(az2::kEngineDexed));
  TEST_ASSERT_EQUAL_UINT16(1, az2::enginePatchCount(az2::kEngineKarplus));  // un seul "patch" possible
  TEST_ASSERT_EQUAL_UINT16(3, az2::enginePatchCount(az2::kEngineSampler));
  TEST_ASSERT_EQUAL_UINT8(2, az2::kSamplerGbCapturePatch);
  TEST_ASSERT_EQUAL_STRING("GB Capture",
                           az2::enginePatchName(az2::kEngineSampler, az2::kSamplerGbCapturePatch));
  TEST_ASSERT_EQUAL_STRING("DEXED", az2::engineName(az2::kEngineDexed));
  TEST_ASSERT_EQUAL_STRING("?", az2::engineName(99));  // moteur invalide -> pas de crash, "?" attendu
}

// Ajoute 2026-09-22 (audit complet) : GRANULAR/SPECTRAL (kEngineGranular/
// kEngineSpectral) n'avaient aucune assertion malgre le diff qui les a
// ajoutes au coeur du protocole partage -- ni pour kRackPatchCount/
// rackParamName/rackParamCount, cote rack (kRackEngine*, identifiants
// SEPARES de kEngine*, voir le commentaire ligne 662-665 de
// AZ2_Protocol.h).
void test_rack_engine_patch_count_and_name() {
  TEST_ASSERT_EQUAL_UINT16(az2::kRackPatchCount, az2::enginePatchCount(az2::kEngineGranular));
  TEST_ASSERT_EQUAL_UINT16(az2::kRackPatchCount, az2::enginePatchCount(az2::kEngineSpectral));
  TEST_ASSERT_EQUAL_STRING("GRANULAR", az2::engineName(az2::kEngineGranular));
  TEST_ASSERT_EQUAL_STRING("SPECTRAL", az2::engineName(az2::kEngineSpectral));

  TEST_ASSERT_EQUAL_STRING("CLOUD", az2::enginePatchName(az2::kEngineGranular, 0));
  TEST_ASSERT_EQUAL_STRING("PERCUSSIVE",
                            az2::enginePatchName(az2::kEngineGranular, az2::kRackPatchCount - 1));
  TEST_ASSERT_EQUAL_STRING("?", az2::enginePatchName(az2::kEngineGranular, az2::kRackPatchCount));
  TEST_ASSERT_EQUAL_STRING("AIR", az2::enginePatchName(az2::kEngineSpectral, 0));
  TEST_ASSERT_EQUAL_STRING("ABYSS",
                            az2::enginePatchName(az2::kEngineSpectral, az2::kRackPatchCount - 1));
  TEST_ASSERT_EQUAL_STRING("?", az2::enginePatchName(az2::kEngineSpectral, az2::kRackPatchCount));
}

void test_rack_param_count_and_name() {
  TEST_ASSERT_EQUAL_UINT8(az2::kRackGranularParamCount, az2::rackParamCount(az2::kRackEngineGranular));
  TEST_ASSERT_EQUAL_UINT8(az2::kRackSpectralParamCount, az2::rackParamCount(az2::kRackEngineSpectral));
  TEST_ASSERT_EQUAL_UINT8(0, az2::rackParamCount(99));  // moteur rack invalide -> 0, pas de crash

  TEST_ASSERT_EQUAL_STRING("POSITION", az2::rackParamName(az2::kRackEngineGranular, 0));
  TEST_ASSERT_EQUAL_STRING("?",
      az2::rackParamName(az2::kRackEngineGranular, az2::kRackGranularParamCount));
  TEST_ASSERT_EQUAL_STRING("PARTIALS", az2::rackParamName(az2::kRackEngineSpectral, 0));
  TEST_ASSERT_EQUAL_STRING("?",
      az2::rackParamName(az2::kRackEngineSpectral, az2::kRackSpectralParamCount));

  // Precaution de compatibilite documentee ligne 662-665 de AZ2_Protocol.h :
  // ces deux jeux d'identifiants doivent rester numeriquement distincts,
  // sinon un ecran plus recent pourrait selectionner un moteur absent d'un
  // Teensy/S3 plus ancien (voir aussi rackEngineSlotFor() cote Teensy).
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(az2::kRackEngineGranular), static_cast<int>(az2::kEngineGranular));
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(az2::kRackEngineSpectral), static_cast<int>(az2::kEngineSpectral));
}

void test_sampler_modes_are_stable_wire_values() {
  TEST_ASSERT_EQUAL_UINT8(0, az2::kSamplerModeOneShot);
  TEST_ASSERT_EQUAL_UINT8(1, az2::kSamplerModeGate);
  TEST_ASSERT_NOT_EQUAL(az2::kSamplerModeOneShot, az2::kSamplerModeGate);
}

void test_sampler_playback_step() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f,
      az2_sampler_math::samplerPlaybackStep(44100, 44100, 440.0f, 440.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f,
      az2_sampler_math::samplerPlaybackStep(22050, 44100, 440.0f, 440.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f,
      az2_sampler_math::samplerPlaybackStep(44100, 44100, 880.0f, 440.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f,
      az2_sampler_math::samplerPlaybackStep(0, 44100, 440.0f, 440.0f));
}

void test_sampler_linear_interpolation() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1000.0f,
      az2_sampler_math::samplerLinearInterpolate(-1000, 1000, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
      az2_sampler_math::samplerLinearInterpolate(-1000, 1000, 0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f,
      az2_sampler_math::samplerLinearInterpolate(-1000, 1000, 1.0f));
}

void test_panic_command_is_stable() {
  TEST_ASSERT_EQUAL_STRING("PANIC", az2::kPanic);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_step_condition_always_is_zero);
  RUN_TEST(test_step_condition_encode_decode_ratio);
  RUN_TEST(test_step_condition_encode_rejects_out_of_range);
  RUN_TEST(test_step_condition_fill_and_not_fill);
  RUN_TEST(test_step_condition_label_matches_encoding);
  RUN_TEST(test_step_condition_cycle_contains_only_valid_entries);
  RUN_TEST(test_division_label_known_values);
  RUN_TEST(test_pad_id_and_valid_pad);
  RUN_TEST(test_engine_patch_count_and_name);
  RUN_TEST(test_rack_engine_patch_count_and_name);
  RUN_TEST(test_rack_param_count_and_name);
  RUN_TEST(test_sampler_modes_are_stable_wire_values);
  RUN_TEST(test_sampler_playback_step);
  RUN_TEST(test_sampler_linear_interpolation);
  RUN_TEST(test_panic_command_is_stable);
  RUN_TEST(test_gb_audio_v2_endian_helpers);
  RUN_TEST(test_gb_audio_v2_crc16_known_vector);
  RUN_TEST(test_gb_audio_v2_header_sanity);
  RUN_TEST(test_gb_audio_v2_stream_roundtrip);
  RUN_TEST(test_gb_audio_v2_reject_corruption_and_recover);
  return UNITY_END();
}
