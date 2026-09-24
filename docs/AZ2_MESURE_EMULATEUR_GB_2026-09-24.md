# AZ-2 — Première mesure réelle de l'émulateur GB + décision de cœur

Date : 24 septembre 2026. Fait suite à `docs/AZ2_ETUDE_EMULATEUR_DMG_2026-09-24.md`
(branche `research/dmg-only-emulator`), qui proposait trois variantes (A : Walnut-CGB
actuel, B : Walnut-CGB en mode DMG-only, C : Peanut-GB) et une méthodologie de
profilage — mais sans aucune mesure chiffrée sur matériel réel disponible dans le
dépôt à ce moment-là (`AZ2_GB_VALIDATION.md` entièrement NON EXÉCUTÉ, les audits
précédents basés sur une lecture de code).

## 1. Mesure sur matériel réel

`gb.direct.frame_skip` passé à `false` dans `gb_emulator.cpp` (était `true`, avec un
commentaire affirmant sans preuve chiffrée que "le panneau RGB/PSRAM reste le goulet
mesuré"). Télémétrie `GB:PERF` (déjà présente dans `main.cpp`, jamais lue en
conditions réelles jusqu'ici) capturée en direct sur ESP32-S3, ROM *The Legend of
Zelda: Link's Awakening* chargée depuis la SD, ~60 s en régime stable après la
période de chargement/warm-up initiale.

Valeurs typiques en régime stable :

| Champ | Valeur | Budget/référence |
|---|---|---|
| `fps_x100` | ~2100-2200 (≈21-22 fps) | cible ~59,7 fps |
| `frame_us_avg` | ~38 700 µs | budget 16 743 µs/frame |
| `core_avg_us` (cœur GB + rendu PPU interne) | ~37 000 µs | — |
| `display_avg_us` (transfert vers panneau RGB) | ~16 500 µs | — |
| `audio_avg_us` | ~1 900 µs | — |
| `missed` (frames manquées) | ~20-24, en continu | cible 0 |

`blit_scale_us` (~1 520) + `blit_copy_us` (~8 700) + `blit_flush_us` (~7 700) ≈
17 900 µs, cohérent avec `display_avg_us`.

## 2. Interprétation

`core_avg_us` (cœur CPU + rendu PPU dans le framebuffer interne, PAS le transfert
écran) représente à lui seul l'essentiel du temps de frame observé, et dépasse à lui
seul plus de deux fois le budget d'une frame complète. `display_avg_us` est réel
(à peu près l'équivalent d'un budget de frame entier) mais nettement secondaire.

**Ceci contredit le commentaire historique du code** qui désignait le panneau RGB/
PSRAM comme goulet — la mesure réelle désigne le **cœur d'émulation** (CPU 6502-like
+ PPU) comme dominant.

## 3. Le cœur ne permet pas de gain facile en mode DMG-only (option B)

Vérification directe dans `src_esp32/az2_screen/walnut_cgb/walnut_cgb.h` (9912
lignes, un seul header unifié DMG+CGB) : `gb->cgb.doubleSpeed` et d'autres champs
`cgb.*` sont lus **à chaque cycle CPU**, dans la boucle d'exécution principale
elle-même (au moins 3 points distincts du hot path, ex. `gb->counter.lcd_count +=
(inst_cycles >> gb->cgb.doubleSpeed);`). Il n'existe pas deux chemins de code
DMG/CGB séparés qu'un simple filtrage des ROM `.gbc` ou un `#ifdef` de compilation
pourrait débrancher proprement — le mode CGB est vérifié en permanence, partout
dans le cœur.

Ceci confirme la mise en garde de l'étude DMG ("la suppression du support GBC ne
garantit pas un gain CPU si les branches CGB ne sont jamais exécutées en mode DMG")
: sur CE cœur précis, amputer le support GBC (option B) ne réduirait quasiment pas
le coût CPU mesuré — potentiellement un peu de RAM/flash, pas le goulet réel.

## 4. Décision

**Option C retenue : intégrer Peanut-GB (cœur DMG-only conçu comme tel dès le
départ) en parallèle, progressivement, sans toucher au cœur actuel.**

- Walnut-CGB reste le cœur de référence en production : tout le câblage existant
  (chargement ROM, MBC, joypad, rendu vidéo, audio, sauvegarde SRAM/RTC,
  télémétrie) fonctionne déjà avec lui et ne doit pas être perturbé.
- Peanut-GB se prototype sur une branche séparée, avec son propre adaptateur vers
  l'interface AZ-2 (ROM/MBC/RAM cartouche, joypad, lignes vidéo RGB565, registres
  APU, sauvegarde, télémétrie identique à `GB:PERF` pour comparaison directe).
- Aucune suppression de `walnut_cgb/` avant validation croisée complète (mêmes
  ROMs, même méthodologie, compatibilité LSDJ vérifiée en premier).
- Référence upstream : https://github.com/deltabeard/Peanut-GB — vérifier licence
  et écarts fonctionnels avant intégration.

## 5. Prochaine étape concrète

Prototyper l'adaptateur Peanut-GB sur une branche dédiée (ex.
`research/peanut-gb-prototype`), réutiliser exactement la même instrumentation
`GB:PERF` pour une comparaison directe A/B sur les mêmes ROMs (Tetris DMG, Super
Mario Land, LSDJ) et le même protocole (60 s en régime stable, `core_avg_us`/
`display_avg_us`/`audio_avg_us`/`missed`).
