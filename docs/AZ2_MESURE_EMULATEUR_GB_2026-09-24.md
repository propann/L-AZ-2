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

## 6. Comparaison A/B sur matériel réel : premier essai, PAS comparable

Prototype construit sur `research/peanut-gb-prototype` (nouveau backend
`gb_emulator_peanut.cpp`, même interface publique que `gb_emulator.cpp`) et flashé
sur le même ESP32-S3, même ROM (*Zelda: Link's Awakening*), mais `frame_skip` était
encore a `true` côté Peanut-GB (valeur par defaut portee depuis gb_emulator.cpp)
alors que la mesure Walnut-CGB du §1 avait ete faite avec `frame_skip=false` --
**comparaison pas equitable**, le "core_avg_us ~15-20k, jusqu'a 2x plus rapide"
observe ici melangeait deux modes de rendu differents (un rendu sur deux vs un
rendu complet). Chiffres corriges en §7 ci-dessous, meme reglage des deux cotes.

## 7. Comparaison A/B corrigee (frame_skip=false des deux cotes)

`gb.direct.frame_skip` repasse a `false` dans `gb_emulator_peanut.cpp`, meme ROM,
regime stable :

| Champ | Walnut-CGB (§1, frame_skip=false) | Peanut-GB (frame_skip=false) |
|---|---|---|
| `core_avg_us` | ~37 000 µs | ~31 300 µs |
| `frame_us_avg` | ~38 700 µs | ~33 200 µs |
| fps estimé | ~21-22 | ~24,5-27 |
| `display_avg_us` | ~16 500 µs | ~16 450 µs (quasi identique -- meme ecran, meme pipeline de blit) |
| frames manquées | ~20-24 en continu | ~19-23 |

Gain reel mais modeste en comparaison equitable : **~15% de reduction sur
`core_avg_us`, ~15% de fps en plus** -- PAS le x2 annonce en §6 (artefact d'une
comparaison a reglages differents). `display_avg_us` identique confirme a nouveau
que le cœur est bien la source de l'ecart, pas un autre facteur commun.

Point important pour la suite ("le mettre en plein regime", 2026-09-24, apres-midi) :
`display_avg_us` (~16 450 µs) represente maintenant **environ la moitie** du temps
de frame total -- toute optimisation supplementaire vers le plein regime devra donc
aussi s'attaquer au pipeline d'affichage (blit vers le panneau RGB, `blit_scale_us`/
`blit_copy_us`/`blit_flush_us`), pas seulement continuer a alleger le cœur.

Pas encore fait : LSDJ/compatibilite cartouche non testee sur ce prototype,
Tetris/Super Mario Land non testes (Zelda seulement pour l'instant), l'ecart de
vitesse observe en §6 entre deux chargements successifs de la meme ROM (sous
frame_skip=true) n'est pas explique -- moins pertinent maintenant que la mesure de
reference se fait a frame_skip=false.

## 8. Sur le retrait de Walnut-CGB

Demande le 2026-09-24 (apres-midi) : "si c'est le nouvel emulateur on peut
officiellement degager l'autre". **Pas encore** -- les garde-fous de migration
poses en §4 (et dans `docs/AZ2_ETUDE_EMULATEUR_DMG_2026-09-24.md`) ne sont pas
remplis : une seule ROM testee (Zelda), LSDJ jamais essaye sur Peanut-GB,
sauvegarde/RTC portes mais jamais reellement exerces (pas de vrai cycle
sauvegarde-coupure-rechargement teste), le gain de vitesse est reel mais modeste
(~15%, voir §7) donc pas evident tant que le pipeline d'affichage n'est pas aussi
retravaille. Walnut-CGB reste le cœur de production tant que cette validation
croisee n'est pas faite.

## 9. Piste concrete pour le pipeline d'affichage (pas encore touchee)

`gbBlitLine()` (`src_esp32/az2_screen/main.cpp`, ~ligne 7207) est **partagee entre
les deux cœurs** (production Walnut-CGB ET le prototype Peanut-GB) -- pas touchee
ici par prudence (pas de validation materielle prudente possible a cette heure sur
du code qui affecte aussi la production). Deux pistes identifiees par lecture de
code, `display_avg_us` (~16 450µs, environ la moitie du temps de frame total) :

- **`blit_copy_us` (~8 700µs, le plus gros poste)** : la copie se fait ligne par
  ligne (18 bandes de 8 lignes source × jusqu'a 24 lignes de sortie), un `memcpy`
  separe par ligne a cause du flip 180° (`dstY = kScreenSize-1-(y+srcY)`, les
  lignes destination ne sont pas contigues dans le meme ordre que la source) --
  structurel, pas un oubli, mais peut-etre un candidat pour le PPA (Pixel
  Processing Accelerator) de l'ESP32-S3 si disponible, qui sait faire une rotation
  materielle sans passer par des memcpy CPU un par un.
- **`blit_flush_us` (~7 700µs)** : `esp_cache_msync()` est appele **18 fois par
  frame** (une fois par bande de 8 lignes source, voir `if (bandComplete)`).
  Regrouper en un seul flush pour toute la frame (accumuler les bandes, flush une
  fois a la fin) reduirait le cout FIXE par appel (meme volume total de donnees a
  synchroniser, moins d'appels) -- piste la plus simple/sure des deux a essayer en
  premier.
- `blit_scale_us` (~1 500µs) deja bon, pas prioritaire.

A faire avec validation materielle prudente (pas en fin de session tardive) avant
tout changement, puisque ce code sert la production.
