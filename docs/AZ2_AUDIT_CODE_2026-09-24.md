# AZ-2 — Audit code daté du 24 septembre 2026

## Périmètre

Audit du dépôt `propann/L-AZ-2` sur la branche `main`, basé sur le code et la documentation présents au 24 septembre 2026.

Zones examinées en priorité :
- `README.md`
- `platformio.ini`
- `.github/workflows/ci.yml`
- `src_esp32/az2_screen/main.cpp`
- `src_esp32/az2_screen/gb_emulator.cpp/.h`
- `src_teensy/az2_audio/main.cpp`
- `src_teensy/az2_audio/az2_sampler.h`
- `lib/AZ2_Protocol/`
- documentation architecture, câblage, émulation, UI et manuel utilisateur.

## Résumé

Le projet a atteint un vrai stade de prototype logiciel cohérent : architecture double firmware claire, séparation ESP32-S3 / Teensy, protocole partagé, CI, tracker 8 pistes, six moteurs audio, sampleur, projet/song/mixer, interface tactile, émulation GB/GBC et chaîne de capture audio.

Le risque principal n'est plus l'absence de fonctions. Il est désormais la **concentration de trop de responsabilités dans quelques fichiers géants**, surtout les deux `main.cpp`. La suite logique est donc de stabiliser et découper, pas de continuer à empiler des fonctions dans les mêmes unités.

## Points solides

### 1. Architecture matériel/firmware cohérente

La séparation est saine :
- ESP32-S3 : écran, tactile, SD, interface, navigateur ROM, émulation GB/GBC ;
- Teensy 4.1 : tracker, synthèse, sampleur, mixage, effets, DAC I²S ;
- protocole commun dans `lib/AZ2_Protocol/AZ2_Protocol.h`.

C'est une bonne base pour garder l'audio temps réel hors de la charge graphique.

### 2. CI utile et ciblée

Le workflow `.github/workflows/ci.yml` compile les deux firmwares séparément et lance les tests natifs du protocole. C'est exactement le bon minimum pour une machine à deux cerveaux.

Point à conserver absolument : toute évolution du protocole doit continuer à être testée avec les deux firmwares dans le même changement.

### 3. Tracker et moteur audio déjà riches

Le Teensy gère 8 pistes et six moteurs :
- DEXED
- EPIANO
- BRAIDS
- KARPLUS
- ANALOG
- SAMPLER

Le graphe audio est déjà plus avancé qu'une simple preuve de concept : enveloppe, filtre, mixage 8 pistes, effets master, audio Game Boy, oscilloscope, sampleur et sauvegarde projet.

### 4. Bonne gestion de plusieurs problèmes réels

Le code contient des corrections issues de tests matériels réels : bruit résiduel avant filtre, encodeurs, tactile, SD, parser WAV, ordre d'initialisation, UART, etc. Cela montre que le firmware n'est pas purement théorique.

### 5. Documentation technique abondante

Le dépôt contient déjà une base de documentation rare pour un prototype DIY : câblage, architecture double firmware, audits, émulation, sampleur, roadmap, UI et manuel.

Le problème n'est plus le manque de documentation, mais sa **hiérarchisation**.

---

## Problèmes à traiter

### P0 — Les deux `main.cpp` sont devenus trop gros

Ordre de grandeur observé :
- `src_esp32/az2_screen/main.cpp` : ~266 Ko ;
- `src_teensy/az2_audio/main.cpp` : ~155 Ko.

Ils contiennent chacun plusieurs sous-systèmes complets.

#### Risques
- régressions lors de petites modifications ;
- temps de compréhension élevé ;
- conflits Git fréquents ;
- navigation difficile ;
- tests unitaires presque impossibles ;
- duplication progressive des états et handlers.

#### Action recommandée

Découper sans changer le comportement.

ESP32 :
```
src_esp32/az2_screen/
  main.cpp
  ui/
    screen_router.*
    home_screen.*
    tracker_screen.*
    mixer_screen.*
    engines_screen.*
    patch_screen.*
    song_screen.*
    project_screen.*
    pads_screen.*
    config_screen.*
  input/
    touch.*
    navigation.*
  transport/
    teensy_link.*
  storage/
    projects.*
  gb/
    ...
```

Teensy :
```
src_teensy/az2_audio/
  main.cpp
  audio/
    engine_manager.*
    mixer.*
    master_fx.*
    track_fx.*
    gb_audio_bridge.*
  sequencer/
    sequencer.*
    song.*
  controls/
    encoders.*
    buttons.*
  storage/
    projects.*
    patches.*
  sampler/
    ...
```

Le premier objectif n'est pas de réécrire le code : seulement déplacer proprement des blocs déjà fonctionnels.

### P0 — Documentation de statut parfois contradictoire

Certains documents historiques décrivent encore des états dépassés. Exemple typique : anciennes mentions de potentiomètres/Pico alors que l'architecture actuelle utilise trois encodeurs et deux cartes.

Il faut distinguer explicitement :
- **CURRENT** : état actuel ;
- **HISTORY** : ancien choix conservé pour mémoire ;
- **ROADMAP** : idée non livrée.

Sinon un nouveau contributeur ne sait pas ce qui est réellement monté.

### P1 — Navigation écran encore trop centralisée

L'enum `Screen`, l'historique de navigation, les handlers tactile, les redraws et les commandes série vivent dans la même grande unité.

Créer une interface minimale par écran :
```cpp
struct ScreenController {
  void enter();
  void exit();
  void draw();
  void onTouch(...);
  void onNav(...);
  void tick();
};
```

Cela permettra ensuite d'améliorer l'UI sans risquer l'émulation ou la couche série.

### P1 — Mixer visuel ≠ niveau audio réel

Le mixer actuel montre surtout la valeur de volume des pistes. Ce n'est pas un VU-mètre.

La roadmap UI du 21 septembre va dans la bonne direction : ajouter une télémétrie peak/RMS réelle venant du Teensy, à cadence modérée, puis faire l'animation côté ESP32.

Recommandation :
1. tester un seul analyseur ;
2. mesurer CPU + AudioMemory ;
3. étendre à 8 si le coût reste acceptable ;
4. ajouter le master ensuite.

### P1 — Protocole série à surveiller

Le projet transporte déjà commandes, état, audio GB, scope et potentiellement futures télémétries.

Il faut éviter de faire grossir le protocole ASCII sans limite.

Pour les messages fréquents :
- conserver l'ASCII pour les commandes humaines/debug ;
- préférer des paquets binaires versionnés pour audio, scope et niveaux ;
- ajouter un numéro de version de protocole visible au démarrage ;
- conserver CRC/sequence pour les flux sensibles.

### P1 — Émulation : séparer stabilité et ambition

L'émulateur GB/GBC est désormais un sous-projet majeur. Il faut éviter qu'il devienne un second firmware monolithique à l'intérieur de `az2_screen`.

Séparer clairement :
- core emulator ;
- frontend AZ-2 ;
- audio bridge ;
- save/RTC ;
- ROM browser ;
- performance telemetry.

La priorité doit être la reproductibilité sur une petite matrice de ROMs de test et ROMs libres, avant d'ajouter davantage de fonctions LSDJ.

### P1 — Tests insuffisants autour des sauvegardes

Les projets, patches, SRAM GB et captures sont critiques.

Ajouter des tests natifs lorsque possible pour :
- sérialisation/desérialisation projet ;
- versions de format ;
- fichiers tronqués ;
- valeurs hors plage ;
- restauration après fichier temporaire interrompu ;
- compatibilité d'une version précédente.

### P2 — Les données embarquées alourdissent fortement le firmware

Les gros headers de banque Dexed et de samples sont pratiques mais augmentent les temps de build et la taille du code source.

À terme :
- banques/samples sur SD ;
- petits presets de secours embarqués ;
- index/cache côté firmware.

Ce n'est pas urgent tant que la build et la flash restent confortables.

### P2 — Dossier documentation trop plat

Le dossier `docs/` contient beaucoup de fichiers au même niveau.

Proposition :
```
docs/
  user/
  hardware/
  firmware/
  audio/
  gameboy/
  audits/
  roadmap/
  archive/
  i18n/
```

Conserver les anciens liens via petits fichiers relais ou faire la migration en une fois avec correction des liens.

---

## Audit UI par rapport au prototype photographié

Les photos du prototype montrent une identité cohérente : fond sombre, typo pixel, palette cyan/magenta/orange/vert, blocs encadrés et forte lisibilité à distance.

Écrans confirmés visuellement dans le prototype :
- accueil AZ-2 ;
- tracker/pattern ;
- pads 4×4 tactiles ;
- mixer 8 pistes ;
- song ;
- projets SD ;
- navigateur ROM GB/GBC.

Cette identité doit devenir la référence visuelle du README et de la documentation. Les futures captures propres doivent rester fidèles à ces écrans, pas inventer une interface qui n'existe pas.

## Documentation GitHub recommandée

### README racine

Le README devrait devenir une vitrine courte :
1. photo principale réelle du prototype ;
2. phrase de concept ;
3. architecture 2 cerveaux ;
4. 6 fonctions clés ;
5. galerie de 4 à 6 écrans ;
6. état actuel ;
7. démarrage rapide ;
8. liens vers la documentation détaillée.

Éviter d'y recopier le manuel complet.

### Nouvelle galerie

Créer :
`docs/user/SCREENS.md`

Avec pour chaque écran :
- photo/capture ;
- fonction ;
- commandes physiques ;
- commandes tactiles ;
- état : testé / intégré / expérimental.

### Photos

Utiliser les photos réelles en priorité.

Traitements acceptables :
- recadrage ;
- correction exposition/contraste ;
- réduction des reflets si possible ;
- légère correction perspective ;
- netteté ;
- balance des blancs.

Ne pas modifier :
- disposition du boîtier ;
- écran affiché ;
- boutons ;
- encodeurs ;
- proportions ;
- contenu fonctionnel.

## Ordre de travail proposé

### Phase 1 — Présentation et vérité documentaire
- ajouter photos propres au dépôt ;
- refaire README autour du prototype réel ;
- créer galerie des écrans ;
- indexer les documents ;
- marquer les documents historiques.

### Phase 2 — Refactor sans changement fonctionnel
- extraire router UI ;
- extraire tactile/navigation ;
- extraire mixer/engines/tracker ;
- extraire audio engine manager côté Teensy ;
- build + tests à chaque extraction.

### Phase 3 — UI vivante
- PATCH devient sous-vue MOTEURS ;
- composant ADSR ;
- identité graphique ANALOG ;
- puis autres moteurs ;
- vrais VU du mixer.

### Phase 4 — Validation produit
- matrice de tests hardware ;
- matrice ROM GB/GBC ;
- test sauvegardes ;
- test 8 pistes stress ;
- test GB + tracker + mixer simultanés ;
- mesure CPU/RAM/AudioMemory/UART.

## Conclusion

AZ-2 n'est plus un simple assemblage expérimental : le dépôt possède déjà les briques d'une vraie groovebox DIY originale.

La prochaine marche n'est pas d'ajouter dix nouvelles fonctions. Elle consiste à rendre le système **lisible, modulaire, documenté et démontrable** sans casser ce qui fonctionne déjà.

Le chantier prioritaire est donc :

**stabiliser → découper → documenter → embellir → seulement ensuite étendre.**
