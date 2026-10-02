# Audit des firmwares et feuille de route — 28 septembre 2026

Audit mené sur la version figée du 28 septembre 2026 (commits `910a15a`,
`bbb584c`, `207f732`).

## Méthode

Trois mesures reproductibles, pas d'appréciation à l'œil :

1. **Compilation des 22 environnements** de `platformio.ini`, un par un.
2. **Code mort par preuve** : flags utilisés en `#ifdef` mais définis dans
   aucun environnement, et fonctions définies dont le nom n'apparaît qu'une
   fois dans leur fichier.
3. **Inventaire des tests** existants, comparé au volume de code.

## État de compilation — 22 environnements, 18 OK / 4 en échec

### Production

| Environnement | Résultat |
| --- | --- |
| `master_teensy_rack_lab` | **OK** |
| `screen_esp` | **OK** (RAM 76,7 %, Flash 13,3 %) |

### Teensy — autres cibles

| Environnement | Résultat |
| --- | --- |
| `master_teensy` | OK |
| `engine_lab_fm2` | OK |
| `engine_lab_dexed` | OK |

### Bancs moteurs ESP32 — 8 cibles

`engine_lab_granular_s2`, `engine_lab_spectral_s2`,
`engine_lab_granular_esp32`, `engine_lab_spectral_esp32`,
`engine_rack_spectral_esp32`, `engine_lab_granular_s3_max`,
`engine_rack_granular_s3`, `engine_rack_granular_s3_teensy_slave` :
**toutes OK**.

### Variantes écran — 3 échecs sur 8

| Environnement | Résultat | Cause |
| --- | --- | --- |
| `screen_esp_ngp_race_lab` | OK | |
| `screen_esp_rgb_direct_probe` | OK | |
| `screen_esp_gnuboy_probe` | OK | |
| `screen_esp_gb_direct` | **ÉCHEC** | `race-memory.h:36: fatal error: z80.h: No such file or directory` |
| `screen_esp_peanut_gb_lab` | **ÉCHEC** | `gb_emulator_peanut.cpp:109: static assertion failed` |
| `screen_esp_peanut_gb_core_task` | **ÉCHEC** | idem |
| `screen_esp_walnut_gbc_core_task` | **ÉCHEC** | `dram0_0_seg` dépassé de **6 488 octets** |

### Divers

| Environnement | Résultat |
| --- | --- |
| `pico_keypad` | OK |
| `native` | OK — **21/21 tests** |

### Les trois causes, précisément

**`screen_esp_gb_direct`** — son `build_src_filter` prend tout
`az2_screen/`, ce qui inclut désormais `ngp_emulator.cpp` (intégration NGP),
mais il n'ajoute ni `+<../src_esp32/az2_ngp_race/race/>` ni `-D CZ80`.
L'intégration NGP a cassé cet environnement sans que personne ne le voie.

**Les deux cibles Peanut** — dérive entre les deux backends GB :

| | `kSavePathCapacity` |
| --- | --- |
| `gb_emulator.cpp` (Walnut, production) | **192** |
| `gb_emulator_peanut.cpp` (Peanut, labo) | **96** |

Avec `kGbRomNameLen = 160` (`gb_emulator.h:19`), l'assertion
`kGbRomNameLen + sizeof("/games/") <= kSavePathCapacity` donne `168 <= 96`,
fausse. Quand la longueur de nom de ROM est passée à 160 pour le balayage des
sous-dossiers SD, **seul le backend Walnut a été mis à jour**. Correctif :
porter Peanut à 192 et aligner `rtcPath` (une seconde assertion exige
`sizeof(rtcPath) == kSavePathCapacity`).

**`screen_esp_walnut_gbc_core_task`** — ce n'est pas une erreur de code mais
un **plafond de RAM atteint**. `screen_esp` consomme déjà 76,7 % de la DRAM ;
ajouter le double tampon de `AZ2_GB_DUAL_CORE_BLIT` dépasse de 6 488 octets.
L'intégration de NES puis de NGP dans `screen_esp` a mangé la marge.

### Conséquence sur la source de vérité

`AZ2_ETAT_ACTUEL.md` affirme dans sa section « Vérifications » que
`screen_esp_peanut_gb_lab`, `screen_esp_peanut_gb_core_task` et
`screen_esp_walnut_gbc_core_task` sont « OK », les deux derniers « flashés et
validés sur matériel réel ». **Ces trois affirmations sont fausses
aujourd'hui.**

C'est d'autant plus gênant que `screen_esp_peanut_gb_core_task` est la cible
qui a produit la mesure de référence **59,76 fps en X3**, et
`screen_esp_walnut_gbc_core_task` celle de la validation GBC. Ces deux
mesures ne sont pas reproductibles en l'état : le binaire qui les a produites
ne se reconstruit plus.

C'est la démonstration la plus directe du besoin de CI : quatre
environnements se sont cassés en silence parce que seuls les deux de
`default_envs` sont jamais construits.


## 1. Bloquant juridique — à traiter avant toute diffusion

**AZ-2 est sous GPLv3** (`LICENSE`). Le firmware de **production**
`screen_esp` compile le cœur RACE :

```
platformio.ini:226   +<../src_esp32/az2_ngp_race/race/>
```

**RACE est GPLv2-only** (`AZ2_ETAT_ACTUEL.md:111`). GPLv2-only et GPLv3 sont
**incompatibles** : le binaire combiné n'est pas distribuable. Par ailleurs
`AZ2_LICENCES.md` ne mentionne pas RACE du tout.

Trois options, à trancher :

- retirer RACE de `screen_esp` et le garder uniquement dans la cible labo
  isolée `screen_esp_ngp_race_lab` (déjà prévue pour ça) ;
- remplacer RACE par un cœur NGP compatible GPLv3 ou sous licence permissive ;
- repasser AZ-2 en GPLv2-or-later, ce qui contraint tout le reste du dépôt.

C'est le point le plus grave de l'audit : il ne se voit pas à l'usage et il
bloque toute publication.

## 2. Code mort et chemins dormants

### 2.1 Code inatteignable par construction

Deux flags sont testés en `#ifdef` mais définis dans **aucun** environnement :

| Flag | Emplacements |
| --- | --- |
| `AZ2_NES_DUAL_CORE_BLIT` | `src_esp32/az2_screen/main.cpp:45` |
| `AZ2_NGP_DUAL_CORE_BLIT` | `main.cpp:45`, `:8414`, `:8504`, `:8523` |

Les blocs de `:8414`, `:8504` et `:8523` exigent en plus `AZ2_DIRECT_PANEL`,
défini seulement par `screen_esp_gb_direct` — qui ne compile plus (voir le
tableau). Ce code est triplement inatteignable.

Fonctions correspondantes jamais appelées : `nesBlitBand()`,
`ngpBlitFrame()`, `ngpBlitWaitIdle()`.

### 2.2 Protocole audio GB V2 — complet, jamais activé

`kGbAudioV2PilotEnabled = false` (`lib/AZ2_Protocol/AZ2_Protocol.h`).

**80 points de code répartis sur 5 fichiers** portent ce protocole :
en-tête 10 octets, CRC-16/CCITT-FALSE, poignée de main `GBV2:QUERY` /
`GBV2:READY`, encodeur, décodeur, compteurs de rejet. Rien n'est jamais
passé sur le fil, et le format n'a jamais été qualifié sur matériel.

Les gardes étant `constexpr false`, le compilateur les élimine : le coût est
en maintenance et en risque de dérive entre les deux firmwares, pas en cycles.

À trancher : l'activer et le qualifier, ou le retirer et le retrouver dans
l'historique git le jour où le besoin revient (passer au-dessus de 15,2 kHz
exige d'élargir le champ de longueur à 16 bits, ce que V2 apporte).

### 2.3 METRO — commande fantôme visible par l'utilisateur

Le métronome a été supprimé du Teensy, mais l'interface le propose toujours :

| Emplacement | Ce qui subsiste |
| --- | --- |
| `src_esp32/az2_screen/main.cpp:1644` | le bouton **METRO** est dessiné dans la page SEQ |
| `main.cpp:1192` | l'état `metronomeOn` est conservé |
| `main.cpp:7074`, `:9203` | l'appui envoie `METRO:0` / `METRO:1` |
| `main.cpp:7772` | l'écho `METRO:` est relu et affiché |
| `src_teensy/az2_audio/main.cpp:3864` | le Teensy répond **toujours** `METRO:0` |

Résultat : un bouton visible, pressable, qui ne peut jamais s'allumer. À
retirer de l'UI, ou à réimplémenter côté Teensy.

### 2.4 Commentaires orphelins — activement trompeurs

`src_teensy/az2_audio/main.cpp` décrit encore du code supprimé :

- `:359` — « voir metronome/patchMetroOut plus bas » : ces symboles n'existent
  plus.
- `:385-391` — un paragraphe entier décrit le clic de métronome, son
  enveloppe et son déclenchement depuis `advanceTick()`.

C'est exactement la classe de défaut qui fait « revenir » un bug : quelqu'un
lit un commentaire qui décrit un comportement absent et raisonne faux.
Le canal 3 de `mixFinal`, libéré par la suppression, n'a plus de gain réglé.

### 2.5 Constante de diagnostic restée en production

`kMuteExternalRackAudio` (`src_teensy/az2_audio/main.cpp:193`) est commentée
« Diagnostic temporaire » et vaut `false`. Soit elle devient une vraie
commande série (utile : elle isole le retour rack sans dessouder l'I2S), soit
elle disparaît.

### 2.6 Doublons et chantiers archivés

| Élément | Volume | Situation |
| --- | --- | --- |
| `walnut_cgb/` + `peanut_gb/` | 9 912 + 4 045 lignes | deux cœurs GB vendored, mutuellement exclusifs par filtre de build |
| `src_pico/az2_keypad` + env `pico_keypad` | 101 lignes | `AZ2_DOC_STATUS.md` déclare le chantier Pico **archivé** |
| `screen_esp_gnuboy_probe`, `screen_esp_rgb_direct_probe`, `screen_esp_gb_direct` | 3 environnements | sondes jamais promues |
| `drawPatchSamplerBrowser()`, `drawPatchSamplerPage()`, `drawEngMiniPatch()` | 3 fonctions | jamais appelées ; correspond à l'explorateur PATCH que `AZ2_ETAT_ACTUEL.md` reconnaît non finalisé |

Garder les deux cœurs GB est défendable (rollback), mais cela doit être écrit
comme une décision, pas subi.

## 3. Défauts ouverts

| # | Défaut | Gravité | Référence |
| --- | --- | --- | --- |
| D1 | Bip parasite périodique, état verrouillé, cause inconnue | haute | `AZ2_BIP_PARASITE_2026-09-28.md` |
| D2 | Aucun chemin logiciel ne remet le bus audio à zéro ; PANIC ne coupe que les voix | haute | idem |
| D3 | Pont audio GB : déficit permanent de 75 éch/s, aucun asservissement d'horloge | haute | idem |
| D4 | Aucun compteur de famine ; `reportGbAudioHealth()` ne voit que le débordement | haute | idem |
| D5 | `screen_esp_gb_direct` ne compile plus depuis l'intégration NGP | moyenne | voir tableau |
| D6 | Écritures `Serial1` hors verrou dans `gb_emulator.cpp`, `gb_emulator_peanut.cpp`, `ngp_emulator.cpp` | moyenne | commit `910a15a` |
| D7 | Licence RACE incompatible dans le build de production | bloquante | §1 |
| D8 | Dérive `kSavePathCapacity` entre les deux backends GB : 2 environnements cassés | moyenne | §2, tableau |
| D9 | DRAM de `screen_esp` saturée à 76,7 % ; la variante double cœur dépasse de 6 488 o | moyenne | idem |
| D10 | `AZ2_ETAT_ACTUEL.md` déclare « OK » trois environnements qui ne compilent pas | moyenne | idem |

## 4. Maintenabilité

| Fichier | Lignes |
| --- | --- |
| `src_esp32/az2_screen/main.cpp` | **9 884** |
| `src_teensy/az2_audio/main.cpp` | 4 891 |
| `src_esp32/az2_screen/gb_emulator.cpp` | 1 194 |
| `lib/AZ2_Protocol/AZ2_Protocol.h` | 855 |

`az2_screen/main.cpp` concentre l'UI, les menus, le raccordement des quatre
émulateurs, le scope, l'oscilloscope série et le protocole. C'est le risque
structurel principal : chaque intégration d'émulateur l'élargit, et c'est
là que vivent les six fonctions mortes et les deux flags inatteignables.

Découpage suggéré, sans réécriture : extraire d'abord `ui_pages.cpp`
(dessin), puis `serial_protocol.cpp` (parsing des lignes). Les deux sont
mécaniques et testables.

## 5. Couverture de tests

| Mesure | Valeur |
| --- | --- |
| Code firmware propriétaire | ~17 500 lignes |
| Code de test | **344 lignes** (`test_protocol` 327, `test_sequencer` 17) |
| Environnements | 22, **aucune CI** |

Les 16 tests couvrent uniquement de la logique pure : libellés de division,
encodage/décodage `PROB:`/`COND:`, identifiants de pad, comptes de patches,
interpolation du sampler.

Ce qui n'est **pas** testé, et qui est pourtant de la logique pure
parfaitement testable en natif :

- le **rééchantillonneur GB** et l'**anneau** — c'est-à-dire exactement où
  vit D3 ; un test d'égalité des débits l'aurait attrapé dès le premier jour ;
- le **parseur de paquets audio** (désynchronisation, longueur invalide,
  délai d'abandon, octet magique dans la charge utile) ;
- l'**encodage/décodage V2** et son CRC ;
- le **séquenceur** au-delà de son initialisation (swing, longueur de
  pattern, `PROB:`/`COND:`).

## 6. Feuille de route priorisée

### Palier 0 — débloquer et instrumenter

| # | Action | Pourquoi maintenant |
| --- | --- | --- |
| 0.1 | Trancher la licence RACE (§1) | bloque toute diffusion, invisible à l'usage |
| 0.2 | Compteur de famine dans `feedGbAudioQueue()`, publié dans `GB:AUDIO_RX` | sans lui on répare à l'aveugle (D4) |
| 0.3 | PANIC = vraie remise à zéro du bus (anneau GB, rééchantillonneur, reverb, delay) | transforme « débrancher la machine » en un bouton, et isole D1 élément par élément |
| 0.4 | Réparer les 4 environnements cassés (D5, D8, D9) | un environnement cassé masque les régressions suivantes |
| 0.5 | Corriger la section « Vérifications » de `AZ2_ETAT_ACTUEL.md` (D10) | une source de vérité fausse est pire que pas de source |
| 0.6 | CI minimale : construire **les 22 environnements**, pas seulement `default_envs` | c'est ce qui a laissé 4 cibles casser en silence |

### Palier 1 — fermer le pont audio GB

| # | Action |
| --- | --- |
| 1.1 | Pré-remplissage : ~8 blocs avant démarrage, cible ~50 % de l'anneau |
| 1.2 | Rééchantillonnage asservi : `kGbResampleStepQ16` variable, corrigé de ±0,5 % max selon l'écart au remplissage cible |
| 1.3 | Ramener `kGbRingCapacity` de 16384 à ~4096 une fois 1.1 et 1.2 en place |
| 1.4 | Verrouiller les écritures `Serial1` restantes (D6) |

### Palier 2 — nettoyer

| # | Action |
| --- | --- |
| 2.1 | Retirer les deux flags inatteignables et leurs trois fonctions mortes (§2.1) |
| 2.2 | Trancher le protocole V2 : activer et qualifier, ou retirer (§2.2) |
| 2.3 | METRO : retirer de l'UI ou réimplémenter (§2.3) |
| 2.4 | Corriger les commentaires orphelins du métronome (§2.4) |
| 2.5 | Décider du sort de `src_pico/`, des trois sondes et du second cœur GB (§2.6) |

### Palier 3 — structure et CI

| # | Action |
| --- | --- |
| 3.1 | Extraire `ui_pages.cpp` puis `serial_protocol.cpp` de `az2_screen/main.cpp` |
| 3.2 | CI : compiler les environnements de production + `pio test -e native` à chaque commit |
| 3.3 | Étendre les tests natifs (§7) |

## 7. Tests à faire

### Tests natifs — aucun matériel requis, à ajouter dans `test/`

| # | Test | Ce qu'il verrouille |
| --- | --- | --- |
| T1 | Débit du rééchantillonneur : sur N frames, comparer les échantillons produits à ceux consommés à 44 100 Hz | **attrape D3** ; doit échouer aujourd'hui (déficit 75 éch/s) |
| T2 | Anneau : débordement, famine, passage par zéro de l'index | D4 |
| T3 | Parseur de paquets : désynchronisation, longueur invalide, délai d'abandon, octet magique dans la charge utile | régression de 2026-09-18 |
| T4 | Encodage/décodage V2 + CRC, aller-retour et corruption d'un bit | prérequis de 2.2 |
| T5 | Séquenceur : swing, longueur de pattern, `PROB:`/`COND:`, `stepsPerBeat` | 17 lignes aujourd'hui |
| T6 | `panicAllAudio()` : après appel, anneau vide et état du rééchantillonneur réinitialisé | verrouille 0.3 |

T1 est le plus important : il transforme un bug audible et intermittent en
une assertion déterministe.

### Tests matériel — procédure écrite, résultat consigné

| # | Test | Critère de réussite |
| --- | --- | --- |
| H1 | Silence 30 min, moniteur série ouvert, `RACK:STATUS` et `GB:AUDIO_RX` relevés | aucun bip ; si bip, **relever les deux lignes avant de redémarrer** |
| H2 | Élimination binaire des moteurs au repos, **refaite avec le chemin rack actif** | l'élimination du 18 septembre est antérieure au rack, sa conclusion ne vaut plus |
| H3 | `RACK:STATUS` pendant un bip : crêtes sur l'entrée rack | non nulles ⇒ le bip entre par le rack |
| H4 | PANIC pendant un bip, après 0.3 | le bip s'arrête sans coupure d'alimentation |
| H5 | GB, GBC, NES, NGP : session de 30 min, FPS et continuité audio | pas de décrochage, `ring_drop` et famine à zéro |
| H6 | Matrice de compatibilité ROM, plusieurs titres par cœur | une seule ROM testée par cœur à ce jour |
| H7 | Aller-retour sauvegarde complet sur les quatre cœurs | non qualifié |
| H8 | Capture GB 30 s puis relecture du WAV depuis la SD | fonctionnel en X2, non requalifié depuis |

H1 et H3 sont prioritaires : ils sont les seuls à pouvoir attraper D1 en
flagrant délit, et ils ne coûtent aucun flash.
