# AZ-2 — Audit complet du dépôt — 22 septembre 2026

## Verdict

**AZ-2 continue de progresser vite, mais le chantier en cours (moteurs de rack
externes exposés comme moteurs de piste) part avec un défaut de conception
qu'il vaut mieux corriger avant de committer.** Le reste du dépôt — protocole,
tracker, tests natifs, configuration PlatformIO — reste dans l'état sain déjà
documenté par les audits précédents.

- Aucun défaut bloquant (P0) trouvé : rien ne crashe, rien ne corrompt de
  données.
- Un défaut d'architecture important affecte les changements non committés
  (§2) : GRANULAR/SPECTRAL sont présentés comme des moteurs *par piste* alors
  que l'audio reste un flux stéréo *global*, sans identifiant de piste dans le
  protocole série.
- Une asymétrie de compilation (ESP32 toujours compilé, Teensy conditionnel)
  fait que sélectionner ces moteurs sur le firmware de production par défaut
  ne produira **aucun son et aucune erreur**.
- La CI ne construit ni la cible qui active ce chantier
  (`master_teensy_rack_lab`) ni aucun des sept environnements de laboratoire
  ESP32/Teensy : tout ce travail est aujourd'hui invisible pour la CI.
- `tools/check_project_examples.py` valide les moteurs de piste avec une
  liste figée à 6 entrées, périmée depuis l'ajout de DRUM (moteur 6) et
  maintenant de GRANULAR/SPECTRAL (7/8) — la CI est verte seulement parce
  qu'aucun projet fourni n'utilise encore ces moteurs.
- Les deux fichiers `main.cpp` (le principal point de dette identifié le
  19 septembre) ont grossi de 45 à 60 % en trois jours au lieu de se réduire.

## Périmètre et méthode

Révision auditée : branche `rack-moteurs-externes`, HEAD `b01ad65`, plus les
modifications non committées du répertoire de travail sur
`lib/AZ2_Protocol/AZ2_Protocol.h`, `src_esp32/az2_screen/main.cpp` et
`src_teensy/az2_audio/main.cpp`.

Audit statique (pas de reflash matériel) réalisé sur :

- le protocole partagé (`lib/AZ2_Protocol`) et sa cohérence avec les deux
  firmwares qui l'incluent ;
- le diff non committé, ligne par ligne ;
- le firmware écran ESP32-S3 et le firmware audio Teensy (zones modifiées et
  zones adjacentes) ;
- les moteurs de laboratoire ESP32 (granulaire S3, spectral S2) et Teensy
  (FM2, Dexed isolé) ;
- le pilote RGB (`lib/AZ2_RGB_Direct`) ;
- les scripts `tools/*.py` et le fichier CI `.github/workflows/ci.yml` ;
- les tests natifs (`test/test_protocol`, `test/test_sequencer`) ;
- `platformio.ini` face aux dossiers `src_esp32/` et `src_teensy/` réellement
  présents ;
- comparaison avec les audits des 17, 18, 19, 20 et 21 septembre.

## 1. Vue d'ensemble : ce que le code fait vs ce que la doc décrit

L'architecture à deux cerveaux reste inchangée et saine : Teensy 4.1 maître
audio/séquenceur, ESP32-S3 écran/tactile/émulation, protocole commun dans
`lib/AZ2_Protocol/AZ2_Protocol.h` (fichier unique inclus des deux côtés — pas
de copie dupliquée à resynchroniser, donc pas de risque de désync d'enum
entre firmwares).

`docs/AZ2_ARCHITECTURE_MIDI_ET_MOTEURS_EXTERNES.md` (21/09) et
`docs/AZ2_BUS_RACK_MOTEURS.md` décrivent un rack externe **global** : le S3
agrège GRANULAR MAX et SPECTRAL SWARM, renvoie un seul flux stéréo au Teensy,
qui le mélange *après* le bus maître (`AZ2_ARCHITECTURE_MIDI_ET_MOTEURS_EXTERNES.md`,
§7 : « mélange le rack en stéréo après le bus maître »). Le code correspond
exactement à cette description côté audio :

```cpp
// src_teensy/az2_audio/main.cpp:172
AudioInputI2S rackAudioIn;
// src_teensy/az2_audio/main.cpp:382-385
AudioConnection patchRackToOutputL(rackAudioIn, 0, mixOutputL, 1);
AudioConnection patchRackToOutputR(rackAudioIn, 1, mixOutputR, 1);
// src_teensy/az2_audio/main.cpp:2180-2181
mixOutputL.gain(1, 0.6f * masterVolume);
mixOutputR.gain(1, 0.6f * masterVolume);
```

Un seul flux, un seul gain fixe (0,6 × volume maître), branché après le bus
maître — donc **hors** du chemin par piste (pas de filtre, pas de
bitcrusher, pas de delay court, pas de mixeur de groupe A/B, pas de
volume/mute/pan individuel).

Le diff non committé ajoute pourtant GRANULAR et SPECTRAL au catalogue
`kEngineNames[]` **par piste**, avec page PATCH dédiée, patch par piste et
paramètres par piste (voir §2). C'est la divergence la plus importante de cet
audit : la doc d'architecture décrit fidèlement un rack global, mais le
nouveau code UI/protocole véhicule un modèle « par piste » qui ne correspond
à aucune réalité côté audio. Ce n'est pas documenté comme un compromis
temporaire — rien dans les commentaires du diff ne signale ce choix.

## 2. Revue des changements non committés

### 2.1 `lib/AZ2_Protocol/AZ2_Protocol.h`

Propre. `kEngineGranular = 7`, `kEngineSpectral = 8`, `kEngineCount` passe à
9, `kEngineNames[]` mis à jour dans le même ordre, `enginePatchCount()` et
`enginePatchName()` étendus avec vérification de borne (`patch <
kRackPatchCount`). Le déplacement de `kRackPatchCount` avant son usage
(ligne ~595) et les tableaux de noms de patch rack ne cassent rien : simple
réordonnancement, pas de redéfinition.

Point notable et **positif** : le commentaire ligne 662-665 explique
explicitement pourquoi `kRackEngineGranular`/`kRackEngineSpectral` (0/1)
restent des identifiants séparés de `kEngineGranular`/`kEngineSpectral`
(7/8) — pour qu'un écran plus récent ne puisse pas sélectionner un moteur
absent d'un Teensy/S3 plus ancien. C'est une vraie précaution de
compatibilité, pas un oubli de nettoyage.

### 2.2 `src_teensy/az2_audio/main.cpp`

- `setTrackEngine()` (ligne 1181-1184) : le cas GRANULAR/SPECTRAL ne connecte
  rien (« leur audio revient déjà par l'entrée I2S globale du rack »).
  `patchTrackIn[track].disconnect()` est bien appelé juste avant (ligne
  1149), donc pas de connexion fantôme laissée par l'ancien moteur — correct.
- `trackNoteOn()`/`trackNoteOff()`/`applyTrackPatch()` : nouveaux cas
  `az2::kEngineGranular`/`kEngineSpectral` qui envoient `RACK_NOTE_ON:`,
  `RACK_NOTE_OFF:` et `RACK_PATCH:` sur `Serial7`, **sans identifiant de
  piste** dans le message. Comme il n'existe qu'une seule instance physique
  GRANULAR et une seule SPECTRAL (pas huit), si deux pistes du tracker
  sélectionnent le même moteur rack, elles pilotent la **même** voix
  externe : un `noteOff()` déclenché par la piste A peut couper une note
  tenue par la piste B, et changer le patch depuis la piste A change aussi
  le son entendu par la piste B. Rien dans le code ne détecte ni n'empêche
  cette collision.
- Le partage d'enveloppe est correctement contourné : `trackAnalogEnv[track]`
  n'est plus déclenché pour ces deux moteurs (ligne ~1500), cohérent avec le
  fait qu'ils ne passent pas par cette enveloppe. Mais cela confirme aussi
  qu'aucune enveloppe interne Teensy n'agit sur ces pistes : l'ADSR affichée
  sur leur page PATCH (héritée de `patchRowLabel`, voir §2.3) ne correspond à
  rien côté Teensy pour ces deux moteurs — les seuls paramètres qui comptent
  vraiment sont ceux envoyés en `RACK_PARAM:` vers le rack externe.
- Tout ce bloc est protégé par `#ifdef AZ2_EXTERNAL_RACK` (grep : lignes 1102,
  1489, 1530). Ce define n'existe que dans `env:master_teensy_rack_lab`
  (`platformio.ini:56`) — **pas** dans `env:master_teensy`, qui est
  l'environnement de production et fait partie de `default_envs`. Conséquence
  directe : sur le couple de firmwares par défaut (`master_teensy` +
  `screen_esp`), sélectionner GRANULAR ou SPECTRAL sur une piste depuis
  l'écran ne fait strictement rien côté Teensy — aucun son, aucun message
  d'erreur, aucune trace. Voir §3.3 pour l'angle CI de ce même problème.

### 2.3 `src_esp32/az2_screen/main.cpp`

- Suppression cohérente de l'ancienne page dédiée `Screen::Rack` (menu,
  `drawScreen()`, gestion tactile, raccourcis B/D) au profit de
  GRANULAR/SPECTRAL comme moteurs de piste ordinaires. Le remplacement
  lui-même est propre : `isRackTrack()`, `rackEngineForTrack()`,
  `patchRowLabel()`, `patchParamRef()`, `patchExtraCount()`,
  `patchExtraLabel()`, `sendPatchExtra()` et `patchApplyDelta()` sont tous mis
  à jour de façon cohérente pour ce nouveau chemin.
- **Mais contrairement au Teensy, ce code n'est protégé par aucune
  macro** : `screen_esp` fait partie de `default_envs` et compile ce chemin
  sans condition. C'est l'asymétrie évoquée en §2.2 — l'écran de production
  expose une fonctionnalité que le Teensy de production n'implémente pas.
- Code mort laissé en place : `drawRackPage()`, `rackChangeEngine()`,
  `rackChangePatch()`, `rackChangeParam()`, `sendRackEngineState()` et les
  globales `rackUiEngine`, `rackUiPatch[]`, `rackUiPage[]`, `rackUiEnabled[]`,
  `rackUiParams[]` (lignes 3241-3340, ~100 lignes) ne sont plus appelés nulle
  part après la suppression de `Screen::Rack`. Ça compile (les variables sont
  utilisées à l'intérieur de ces mêmes fonctions mortes), mais c'est de la
  dette immédiate à supprimer avant de committer.
- `kEngListRowH` réduit de 36 à 28 px (ligne 1564) pour que la liste
  MOTEURS tienne à 9 lignes au lieu de 7 — dérivé de `az2::kEngineCount`
  (ligne 1570 et 1661), donc s'adapte automatiquement à un futur changement
  de nombre de moteurs. Le commentaire juste au-dessus (ligne 1562-1563,
  « Sept moteurs tiennent dans la page ») n'a en revanche pas été mis à
  jour — mineur, mais c'est exactement le type de dérive documentée par
  l'audit du 19 septembre (§ « Documentation embarquée devenue
  incohérente »).
- `sendPatchExtra()` : le buffer `msg` est agrandi de 24 à 48 octets pour
  accueillir `RACK_PARAM:<nom moteur>:<idx>:<val>` — vérifié suffisant
  (le nom le plus long est `"SPECTRAL"`, message max ≈ 24 caractères).

## 3. Revue de code par zone

### 3.1 Moteurs de laboratoire ESP32/Teensy

`engine_lab_granular_s3_max`, `engine_lab_spectral_s2`,
`engine_lab_granular_s2`, `engine_lab_fm2`, `engine_lab_dexed` : tailles
raisonnables (110 à 616 lignes), aucun `TODO`/`FIXME`/`HACK`, correspondent
chacun à un environnement PlatformIO dédié (§4). Non revus ligne à ligne dans
le détail — hors du chemin critique de production et déjà mesurés/documentés
dans `AZ2_SESSION_RACK_AUDIO_2026-09-21.md`.

### 3.2 `lib/AZ2_RGB_Direct`

149 + 60 lignes, pas de changement dans ce diff, pas de marqueur de dette.
Non re-audité en détail (déjà couvert par
`docs/AZ2_AUDIT_PILOTE_RGB_2026-09-20.md`).

### 3.3 Outils et CI

- `tools/check_firmware_contract.py` : garde statique bien ciblée sur le
  contrat audio GB V1 (débit, fréquence, tailles de paquet). Il ne connaît
  pas la table des moteurs — ce n'est pas son rôle, mais ça confirme qu'aucun
  outil du dépôt ne vérifie automatiquement la cohérence
  `kEngineCount`/`kEngineNames[]`/menu ESP32 en dehors de la compilation
  elle-même.
- **`tools/check_project_examples.py:14`** :
  `ENGINE_NAMES = ("Dexed", "EPiano", "Braids", "Karplus", "Analog",
  "Sampler")` — 6 entrées, alors que `az2::kEngineCount` vaut maintenant 9
  (DRUM ajouté le 19/09 n'y figurait déjà pas ; GRANULAR/SPECTRAL ajoutés
  aujourd'hui non plus). La vérification de borne ligne 31-32
  (`0 <= engine < len(ENGINE_PATCH_COUNTS)`) utilise donc un plafond figé à
  6 au lieu de 9. Vérifié en exécutant le script : il passe aujourd'hui
  (`PASS` sur les 6 projets fournis) uniquement parce qu'aucun projet
  d'exemple n'utilise un moteur ≥ 6 — pas parce que la vérification est
  correcte. Deux conséquences concrètes : (1) un futur projet d'exemple
  légitime utilisant DRUM, GRANULAR ou SPECTRAL ferait échouer cette
  assertion à tort ; (2) les indices de patch de ces trois moteurs ne sont
  aujourd'hui validés par aucun outil. Ce script est exécuté par la CI
  (`.github/workflows/ci.yml:25`), donc ce plafond obsolète a une
  conséquence réelle sur ce qui est réellement vérifié à chaque push.
- `.github/workflows/ci.yml` : deux jobs, l'un pour les tests natifs +
  scripts Python, l'autre en matrice `[master_teensy, screen_esp]`. **Aucun**
  des neuf autres environnements (`master_teensy_rack_lab`,
  `engine_lab_fm2`, `engine_lab_dexed`, `engine_lab_granular_s2`,
  `engine_lab_spectral_s2`, `engine_lab_granular_esp32`,
  `engine_lab_spectral_esp32`, `engine_rack_spectral_esp32`,
  `engine_lab_granular_s3_max`, `engine_rack_granular_s3`,
  `engine_rack_granular_s3_teensy_slave`) n'est construit par la CI. C'est
  cohérent avec leur statut de « banc de laboratoire », mais ça veut dire
  concrètement que **le bloc `#ifdef AZ2_EXTERNAL_RACK`** ajouté aujourd'hui
  dans `src_teensy/az2_audio/main.cpp` (§2.2) n'a jamais été compilé par la
  CI et ne le sera pas au prochain push — une erreur de syntaxe dedans
  resterait invisible jusqu'à une compilation manuelle de
  `master_teensy_rack_lab`.
- `tools/measure_audio_rack.py` / `tools/stress_audio_production.py` :
  scripts de mesure manuelle sur matériel réel (utilisent `pyserial`,
  `/dev/ttyACM0`), cohérents avec les mesures publiées dans les audits du
  20 et 21 septembre. `measure_audio_rack.py:11` liste encore les 7 moteurs
  pré-GRANULAR/SPECTRAL (`DEXED`..`DRUM`) — cohérent avec son usage (mesure
  du rack Teensy interne), pas un défaut.
- `tools/build_teensy_loader.py` : script de contournement d'un bug connu du
  loader PlatformIO, bien documenté en tête de fichier, inchangé.

### 3.4 Tests natifs

- `test/test_protocol/test_main.cpp` (271 lignes) couvre `enginePatchCount`/
  `enginePatchName` pour DEXED, KARPLUS et SAMPLER (lignes 210-215), mais
  **aucune assertion n'existe pour `kEngineGranular`/`kEngineSpectral`**
  (ni pour `kEngineDrum`, déjà absent avant ce diff), ni pour
  `kRackPatchCount`/`rackParamName`/`rackParamCount`. Le diff du jour ajoute
  du code au cœur du protocole sans ajouter le test correspondant.
- `test/test_sequencer/test_main.cpp` fait toujours 17 lignes et un seul
  test (`testSequencerTrackInit`) — inchangé depuis les audits précédents,
  toujours une couverture symbolique plutôt que réelle du séquenceur.

## 4. Cohérence `platformio.ini`

Vérification croisée dossiers ↔ environnements : chaque dossier sous
`src_esp32/` et `src_teensy/` (hors `microdexed-touch/`, vendored) a au moins
un environnement PlatformIO qui le construit (`build_src_filter`), et aucun
environnement ne pointe vers un dossier absent. Pas d'environnement orphelin,
pas de dossier source oublié. `master_teensy_rack_lab` étend correctement
`master_teensy` (mêmes `build_flags` plus `AZ2_EXTERNAL_RACK`) sans dupliquer
la configuration — bonne pratique.

## 5. Dette technique et risques, par sévérité

### Important

1. **Modèle « par piste » sans identifiant de piste dans le protocole rack**
   (`src_teensy/az2_audio/main.cpp:1489-1540`, `AZ2_Protocol.h`) — deux
   pistes sur le même moteur rack se marchent dessus (§2.2). À trancher avant
   de committer : soit une piste "propriétaire" exclusive par moteur rack
   (verrouillage UI), soit ajouter l'identifiant de piste au protocole
   `RACK_NOTE_ON/OFF/PATCH/PARAM`.
2. **Asymétrie de compilation ESP32 (inconditionnel) / Teensy (`#ifdef
   AZ2_EXTERNAL_RACK`, absent de `master_teensy`)** — le couple de firmwares
   de production par défaut laisse sélectionner un moteur qui ne fait
   aucun son, sans diagnostic (§2.2, §2.3).
3. **CI aveugle sur le nouveau chemin** — ni `master_teensy_rack_lab` ni
   aucun environnement de laboratoire n'est construit par
   `.github/workflows/ci.yml` (§3.3).
4. **`tools/check_project_examples.py` valide les moteurs de piste avec un
   plafond figé à 6 au lieu de 9** — CI verte par absence de cas de test, pas
   par correction (§3.3).

### Mineur

5. **Code mort** : ~100 lignes de l'ancienne page Rack toujours compilées
   mais plus jamais appelées (`src_esp32/az2_screen/main.cpp:3241-3340`).
6. **Documentation/commentaires périmés** : `README.md:9` et `README.md:27`
   (« sept moteurs »), commentaire `src_esp32/az2_screen/main.cpp:1562-1563`
   (« Sept moteurs tiennent dans la page ») — même défaut récurrent que
   l'audit du 19 septembre.
7. **Absence de test natif** pour `kEngineGranular`/`kEngineSpectral`/
   `kRackPatchCount` dans `test/test_protocol/test_main.cpp`.
8. **Croissance des deux monolithes** : voir §6 — pas un risque immédiat
   mais une tendance qui va à l'encontre de la recommandation du 19/09.

Aucun défaut classé bloquant (P0) : pas de dépassement de tampon identifié,
pas de division par zéro nouvelle, pas de corruption de sauvegarde liée à ce
diff.

## 6. Progression depuis le dernier audit (21 septembre)

Ce qui a avancé, confirmé dans le code et les docs :

- Les deux moteurs de rack externes (GRANULAR MAX 64 grains, SPECTRAL SWARM
  4×16 partiels) sont passés de « laboratoires isolés » à un banc ESP↔ESP
  fonctionnel puis à une intégration ESP↔ESP↔Teensy validée le 22 septembre
  (`AZ2_ARCHITECTURE_MIDI_ET_MOTEURS_EXTERNES.md`, §5-7) : c'est un vrai
  progrès matériel, cohérent avec les commits `b01ad65`, `fc4c2ee`, `70fca84`.
- Le firmware `master_teensy_rack_lab` compile, avec `AudioInputI2S`, le
  mixage post-bus-maître et l'ouverture de `Serial7` — exactement ce que
  §1/§2 constatent dans le code.
- Nouveauté du jour (non committée) : tentative de rendre GRANULAR/SPECTRAL
  sélectionnables comme moteur de piste depuis l'écran, avec une page PATCH
  dédiée — au prix des défauts d'architecture et de compilation détaillés
  en §2 et §5, qui restent à corriger avant de committer.

Ce qui reste ouvert (inchangé depuis le 21/09, voir
`AZ2_AUDIT_MOTEURS_2026-09-21.md`) :

- allocation trop large des instances de moteur par piste ;
- ADSR partagée pour le sampleur (pas de mode one-shot/gate explicite) ;
- Karplus toujours sans paramètres decay/damping/brightness ;
- validation à l'écoute incomplète pour EPiano/Braids/Karplus/Sampler.

Nouveau depuis le 21/09, pas encore documenté ailleurs :

- le passage du bouton B de la pin 8 à la pin 10, prérequis matériel avant
  de flasher `master_teensy_rack_lab` sur la machine réelle (rappelé dans
  `AZ2_SESSION_RACK_AUDIO_2026-09-21.md`), n'est pas encore reflété dans
  `docs/AZ2_CABLAGE_MASTER.md` à ma lecture — à vérifier avant tout flash
  physique.

## 7. Recommandations, dans l'ordre

1. **Trancher le modèle piste/rack avant de committer** : piste propriétaire
   exclusive avec verrouillage UI, ou identifiant de piste ajouté au
   protocole `RACK_*`. C'est le changement le plus risqué du diff actuel.
2. **Aligner ESP32 et Teensy sur la même condition de compilation**, ou à
   défaut griser/désactiver GRANULAR/SPECTRAL dans l'UI écran tant que le
   Teensy en face ne les supporte pas (ex. capacité annoncée au handshake).
3. **Ajouter `master_teensy_rack_lab` à la CI** (au moins `pio run`, sans
   test matériel), pour que ce bloc `#ifdef` cesse d'être invisible.
4. **Corriger `tools/check_project_examples.py`** : dériver `ENGINE_NAMES`/
   `ENGINE_PATCH_COUNTS` de `az2::kEngineNames[]`/`enginePatchCount()` (ou a
   minima les compléter à la main) pour retrouver une vérification réelle.
5. **Supprimer le code mort** de l'ancienne page Rack
   (`src_esp32/az2_screen/main.cpp:3241-3340`) et mettre à jour
   `README.md`/le commentaire ligne 1562-1563 avant de committer.
6. **Ajouter des tests natifs** pour `enginePatchCount`/`enginePatchName`/
   `engineName` sur GRANULAR/SPECTRAL et pour `kRackPatchCount`.
7. **Vérifier `docs/AZ2_CABLAGE_MASTER.md`** contre le déplacement du bouton
   B (pin 8 → pin 10) avant tout flash physique de `master_teensy_rack_lab`.
8. **Reprendre le découpage des deux `main.cpp`** recommandé le 19/09 — la
   dette a augmenté de 45 à 60 % en trois jours au lieu de se réduire ; plus
   l'attente est longue, plus chaque nouveau moteur (comme celui de ce diff)
   coûte cher à intégrer proprement.

## Suite du 22 septembre : corrections appliquées et validation matérielle

Toutes les recommandations §7 (1, 2, 4, 6) ont été implémentées et validées
sur le matériel réel (Teensy 4.1 + écran ESP32-S3 branchés, `/dev/ttyACM1`
et `/dev/ttyUSB0`) :

- **Propriété exclusive des moteurs de rack** (`rackOwnerTrack[]` côté
  Teensy) : une piste qui sélectionne GRANULAR/SPECTRAL en devient
  propriétaire ; une autre piste qui sélectionne le même moteur le lui vole
  proprement (silence de sécurité envoyé au rack). Les trois chemins qui
  parlent au rack sont couverts : note on/off et patch complet
  (`trackNoteOn`/`trackNoteOff`/`applyTrackPatch`), ET les réglages fins en
  direct (`RACK_PARAM:`, qui passait par un forward brut sans aucune notion
  de piste — angle mort de l'audit initial, fermé côté écran via un miroir
  `RACK_OWNER:` renvoyé par le Teensy). **Validé en direct** :
  `ENGINE:0:7` puis `ENGINE:1:7` sur `master_teensy_rack_lab` produisent
  bien `RACK_OWNER:GRANULAR:0` puis `RACK_OWNER:GRANULAR:1` (vol confirmé),
  sans erreur ni blocage sur `TEST:0:60:0` envoyé par l'ancienne
  propriétaire.
- **Asymétrie de compilation** : `master_teensy` (production) refuse
  désormais `ENGINE:<piste>:7/8` avec `ENGINE:ERROR:RACK_UNAVAILABLE`
  plutôt que d'accepter silencieusement une sélection muette — **validé en
  direct** sur le vrai `master_teensy`. Poignée de main `RACK_CAP:0/1`
  ajoutée à `announceHello()`, grise GRANULAR/SPECTRAL sur l'écran tant que
  le Teensy en face n'a pas confirmé — **validé en direct** :
  `RACK_CAP:0` reçu sur `master_teensy`, `RACK_CAP:1` sur
  `master_teensy_rack_lab`.
- **`tools/check_project_examples.py`** dérive maintenant son plafond de
  validation (`ENGINE_PATCH_COUNT_NAMES`) directement des constantes de
  `AZ2_Protocol.h` avec une assertion de cohérence sur `kEngineCount`, au
  lieu d'une liste figée à 6 — re-testé, toujours `PASS` sur les 6 projets
  fournis, mais avec un plafond réel de 9 désormais.
- **Tests natifs** ajoutés pour `enginePatchCount`/`enginePatchName` sur
  GRANULAR/SPECTRAL et pour `rackParamCount`/`rackParamName` — 21/21 tests
  passent (`pio test -e native`).
- **Bug de compilation trouvé en vérifiant l'ensemble du dépôt** (hors
  scope de cet audit mais bloquant) : `engine_lab_granular_s3_max/main.cpp`
  ne compilait plus (`sampleRxRate` non déclarée hors
  `AZ2_RACK_AGGREGATOR`, utilisée pourtant sans condition dans
  `resetGrain()`) — corrigé, les 3 environnements qui utilisent ce fichier
  compilent à nouveau.
- **Les 17 environnements PlatformIO du dépôt compilent** (les 15 listés
  dans le tableau ci-dessous, plus `native`) — première vérification
  complète du genre, la CI n'en couvrant que 2/15 (voir §3.3).

Point d'architecture vérifié et **écarté** : l'allocation d'une instance de
chaque moteur par piste (7 × 8 = 56 objets audio, item ouvert depuis le
21/09, "allocation trop large des instances de moteur par piste") a été
étudiée en détail dans le code du Teensy Audio Library
(`AudioStream.cpp`) : chaque `AudioConnection` marque son objet source ET
destination `active = true`, et `update_all()` ne traite QUE les objets
`active` (`if (p->active) p->update();`). `setTrackEngine()` appelle déjà
`patchTrackIn[track].disconnect()` avant de reconnecter le nouveau moteur,
et chaque moteur n'a qu'UNE seule connexion (vers `patchTrackIn[]`) —
donc les 48 instances inutilisées à un instant donné (6 moteurs sur 7 par
piste) sont réellement inactives cote CPU, pas seulement débranchées en
apparence. Le coût réel est de la RAM statique, pas du CPU — et la RAM
actuellement libre (`master_teensy` : 86 688 o RAM1, 324 192 o RAM2, 4 Mo
PSRAM quasi vides) ne justifie pas une refonte risquée du modèle
d'allocation pour l'instant. Pas d'action prise sur ce point.

Le Teensy a été laissé flashé avec `master_teensy` (production, avec les
correctifs de cette session) ; l'écran avec `screen_esp` (idem).

## Diagnostic du souffle DEXED (22 septembre, suite)

Le blocage du diagnostic du 18/09 ("Fausse alerte sur SCOPE:", voir
`AZ2_ETAT_DES_LIEUX.md`) était réel : `updateScope()` n'écrivait les
paquets binaires que sur `Serial1` (liaison UART dédiée vers l'écran),
jamais sur `Serial` (USB de debug) — aucun outil de l'époque n'y avait
accès. Corrigé en miroitant aussi sur `Serial` (coût négligeable, actif
uniquement quand `SCOPE:<piste>` est sélectionné) — **gardé en
permanence**, pas seulement pour ce diagnostic.

Capture en direct sur le vrai Teensy (piste 0, moteur DEXED, `TEST:0:60:1`
pour déclencher une note hors séquenceur) :

- Contrôle sur ANALOG (moteur confirmé propre) : onde parfaitement lisse,
  saut max entre échantillons décimés consécutifs ≤ 11 sur toute la
  capture — valide le pipeline de capture lui-même (pas d'artefact
  introduit par la décimation/le tap).
- DEXED, patch 0 ("BRASS 1", celui sur lequel `setTrackEngine()`
  réinitialisait systématiquement `trackPatch[]`) : sauts jusqu'à 54,
  restant élevés (30-36) pendant plusieurs secondes de note tenue — très
  au-dessus du bruit de fond d'ANALOG.
- Balayage de 8 patches (0, 10, 30, 50, 80, 120, 160, 200) : **patch 0 est
  un cas isolé**, nettement pire que les 7 autres (sauts max 1-25 contre
  30-54 pour le patch 0). Comme DEXED repart toujours du patch 0 à la
  sélection, c'est le premier son garanti de quiconque essaie ce moteur —
  cohérent avec le diagnostic du 18/09 ("bruit... dès qu'on déclenche une
  note").

Je n'ai pas pu trancher si le patch 0 lui-même est un bug du cœur FM ou un
patch BRASS avec un feedback DX7 volontairement agressif (caractéristique
connue de ce type de patch) — ça demande une oreille, que je n'ai pas.
Correctif appliqué sans attendre cette réponse : `kDexedDefaultPatch = 120`
("WATER GDN", le plus propre du balayage, sauts ≤ 6 sur 1,5 s de note
tenue après flash — meilleur que le bruit de fond d'ANALOG) remplace le
patch 0 comme point de départ de DEXED dans `setTrackEngine()`. Bug
connexe corrigé au passage : `handleEngineCommand()` annonçait toujours
`PATCH:<piste>:0` à l'écran après un changement de moteur, quel que soit
le patch réellement chargé par `setTrackEngine()` — désynchronisait
l'affichage pour DEXED (Teensy sur patch 120, écran affichant "BRASS 1").
Utilise maintenant `trackPatch[track]`, la vraie valeur. Validé en direct :
`PATCH:0:120` bien annoncé, `DXP:0:0:31`/`DXP:0:1:3` (algo/feedback) au
lieu des valeurs du patch 0.

## Identité visuelle par moteur sur la page PATCH (22 septembre, suite)

Vérification demandée : les fenêtres de contrôle avaient-elles chacune une
identité propre au moteur affiché ? Réponse trouvée en lisant le code :
**non** — toute la palette de couleurs de l'appli (page MOTEURS, page
PATCH, Mixer, Séquenceur) est dérivée de `kPalette[track % kPaletteCount]`,
donc de la PISTE, jamais du moteur. Conséquence concrète : DEXED sur la
piste 0 et DEXED sur la piste 3 n'avaient rien de visuellement commun,
alors que BRAIDS et DEXED sur la MEME piste se ressemblaient parfaitement
(même couleur de piste). Rien n'indiquait non plus, textuellement, quel
moteur était affiché sur la page PATCH — l'en-tête disait juste "PATCH" en
couleur fixe.

Ajout d'une palette dédiée `kEngineAccent[az2::kEngineCount]` (une couleur
stable par moteur, indépendante de la piste) et de deux fonctions
(`engineAccent()`/`patchAccent()`). Appliqué à **toute** la page PATCH
(portée volontairement limitée à cette page, comme demandé — les autres
pages restent colorées par piste pour l'instant) :

- en-tête "PATCH" et cadre de l'oscilloscope : couleur du moteur ;
- ligne PISTE : affiche maintenant `< PISTE N - NOM_MOTEUR >`, centré
  dynamiquement (la largeur du texte varie selon le nom, ex. "DEXED" vs
  "SPECTRAL") plutôt que le décalage fixe précédent (valable seulement pour
  l'ancien texte de longueur constante) ;
- liste des patches, trace de l'oscilloscope, lignes de paramètres
  (fixes/extra/VOLUME/SLOT) : toutes recolorées par moteur au lieu de la
  piste.

Compile et flashe (`screen_esp`) sans erreur, boot stable confirmé en
direct (logs `DISPLAY:ALIVE:TICK` réguliers, aucune boucle de crash). Je
n'ai en revanche **aucun moyen de voir le rendu réel** sur l'écran physique
(pas de caméra) — la largeur de texte centrée est calculée à 12 px/caractère
(police GFX par défaut, taille 2), à vérifier visuellement par l'utilisateur
une fois l'écran sous les yeux, de même que la lisibilité/le bon goût des 9
couleurs choisies.

Extension à la page MOTEURS (même session) : chaque ligne de la liste des 9
moteurs garde désormais sa propre couleur (`engineAccent(engineIdx)`),
peu importe la piste sélectionnée — avant, seule la ligne du moteur
courant de la piste était mise en avant, dans la couleur de la piste.
Liste des patches, cadre de focus, bandeau "réglages du patch" et
visualiseur (qui avait déjà des silhouettes différentes par moteur — DEXED
en rayons FM, DRUM en pulsation, SAMPLER en zigzag — mais toutes dans la
couleur de piste) recolorés pareil. Ligne PISTE alignée sur le même format
que la page PATCH (`< PISTE N - MOTEUR >`, centrage dynamique). L'en-tête
"MOTEURS" reste volontairement neutre (page de sélection parmi les 9
moteurs, pas l'édition d'un seul). Compile et flashe sans erreur, boot
stable confirmé en direct. Même limite qu'avant : pas d'yeux sur l'écran
physique pour confirmer le rendu.

Aucun commit git n'a été fait — modifications encore dans l'arbre de travail.

## Repères chiffrés

| Fichier | 19 sept. (audit) | 22 sept. (ce jour) | Évolution |
|---|---:|---:|---:|
| `src_esp32/az2_screen/main.cpp` | ≈ 4 700 lignes | 7 535 lignes | +60 % |
| `src_teensy/az2_audio/main.cpp` | ≈ 3 000 lignes | 4 369 lignes | +45 % |
| `lib/AZ2_Protocol/AZ2_Protocol.h` | — | 799 lignes | — |
| Moteurs au catalogue (`kEngineCount`) | 6 | 9 (avec le diff non committé) | +3 |
| Environnements PlatformIO | — | 15 (2 en `default_envs`) | — |
| Environnements construits par la CI | — | 2 / 15 | — |

## Fusion sampler piste/pads (23 septembre)

Demande : le moteur SAMPLER d'une piste (page PATCH) ne choisissait que
parmi 3 sons fixes (Kick/Snare/GB Capture) alors que les 16 pads (écran
SAMPLEUR dédié) peuvent déjà charger n'importe quel WAV de la SD — fusionner
les deux.

- `kSamplerPatchCount` 3→4, nouveau `kSamplerCustomPatch = 3` ("CUSTOM")
  dans `AZ2_Protocol.h`.
- Teensy : buffer PSRAM dédié par piste (`trackSampleBuffer[8]`, même
  capacité que les pads, +~1,5 Mo EXTRAM confirmé au lien — 5,59 Mo au
  total, largement dans les 16 Mo). Nouvelle commande `TRACKSAMPLE:
  <piste>:<chemin>` (charge, réutilise `readWavPcm16Mono()` déjà partagée
  avec les pads/la capture GB) et `TRACKSAMPLE:<piste>:-` (efface) ;
  bascule automatiquement `trackPatch[piste]` sur CUSTOM après un
  chargement réussi. `applyTrackPatch()` sait revenir sur ce patch sans
  recharger le fichier (buffer conservé tant qu'aucun autre n'est chargé).
- Écran : l'écran SAMPLEUR (jusqu'ici pads uniquement) accepte un mode
  cible "piste" (`samplerTargetIsTrack`) — même navigateur SD, mais
  affiche un panneau piste (couleur d'identité du moteur, nom de piste,
  sample actuel) à la place de la grille de 16 pads, et envoie
  `TRACKSAMPLE:` au lieu de `PADSAMPLE:`. Le bouton retour (C/tactile)
  ramène à la page PATCH plutôt qu'à AUDIO en mode piste. Nouvelle ligne
  "SAMPLE" sur la page PATCH du moteur SAMPLER (à côté de MODE) : la
  sélectionner puis faire le geste habituel A+croix ouvre ce navigateur au
  lieu de régler une valeur numérique.
- Persistance projet : `TRACKSAMPLE:<piste>:<chemin>` écrit/lu dans les
  fichiers projet, même convention que `PADSAMPLE:` (uniquement les pistes
  réellement chargées).
- Test natif mis à jour (`kSamplerPatchCount` était en dur à 3 dans
  `test_engine_patch_count_and_name`) — 21/21 toujours au vert.

**Validé en direct sur le vrai Teensy** : `TRACKSAMPLE:` répond
correctement à un chemin invalide (`SAMPLER:TRACK:<n>:OPEN_ERROR`), une
piste hors bornes (`TRACKSAMPLE:ERROR:OUT_OF_RANGE`), une commande mal
formée et l'effacement. **Non vérifié** : le chargement réel d'un WAV
existant — la carte SD de ce banc de test n'a pas encore la bibliothèque
de samples complète (les chemins par défaut de `kDefaultKitPaths[]`, déjà
utilisés par le kit de batterie au boot, retournent `OPEN_ERROR` ici). Le
mécanisme réutilise cependant tel quel `readWavPcm16Mono()`, déjà prouvé
par le chemin pads — confiance élevée mais pas une preuve par l'écoute.
Écran ESP32 compilé et vérifié (pas encore flashé, port non branché au
moment d'écrire ceci).
