# AZ-BUS — Rack modulaire de moteurs audio

**Version :** concept V1, 18 septembre 2026  
**Projet :** AZ-2  
**But :** raccorder au Teensy maître des moteurs locaux ou des cartes audio interchangeables utilisant toutes le même contrat.

## Vision

AZ-2 possède un rack général de moteurs. Une piste ne parle plus directement à DEXED, BRAIDS, ANALOG ou à une carte précise. Elle parle à un **Engine Slot**.

Un slot peut contenir :

- un moteur compilé localement sur le Teensy maître;
- un ESP32-S3 audio;
- un autre Teensy 4.1;
- un RP2040/RP2350;
- un Arduino pour un moteur lo-fi léger;
- plus tard, une carte DSP dédiée.

Le séquenceur et l'interface utilisent exactement les mêmes commandes dans tous les cas.

## Principe fondamental

Deux chemins distincts :

1. **Contrôle UART** : identité, notes, paramètres, patches, transport et diagnostic.
2. **Audio I2S/TDM** : flux audio numérique vers le Teensy maître.

L'UART ne transporte jamais l'audio musical normal. Le flux PCM8 de l'émulateur GB actuel est une exception de prototype à faible débit, pas le modèle du rack.

## Architecture

```
ESP32-S3 écran / projets / émulateur
                |
                | AZ2 Control Protocol
                v
       Teensy 4.1 — Engine Rack Host
          |       |       |
          |       |       +-- moteur local Sampler
          |       +---------- moteur local Plaits
          +------------------ moteur distant AZ-BUS
                                  |
                         UART contrôle + I2S audio
                                  |
                    ESP32 / Teensy / RP2350 / Arduino
```

Le Teensy reste :

- maître de l'horloge;
- propriétaire du séquenceur;
- propriétaire de l'état des pistes;
- mixeur final;
- processeur des effets critiques;
- sortie vers le PCM5102A;
- autorité qui accepte, refuse ou coupe un moteur.

## Rack logiciel commun

### EngineDescriptor

Chaque moteur publie un manifeste :

- identifiant stable;
- nom et fournisseur;
- version du firmware;
- famille : sampler, VA, FM, wavetable, drum, physical, granular, chip;
- local ou distant;
- nombre de voix;
- mono/stéréo;
- fréquence et format audio;
- nombre de paramètres;
- nombre de patches;
- consommation CPU/RAM déclarée;
- besoin PSRAM/SD/entrée audio;
- état : expérimental, test, stable, quarantaine.

### Interface Engine

Tous les moteurs répondent aux mêmes opérations :

```cpp
prepare()
activate()
deactivate()
noteOn(note, velocity)
noteOff(note)
allNotesOff()
setParameter(id, value)
getParameter(id)
loadPatch(id)
savePatch(id)
setTempo(bpm)
setTransport(running, songPosition)
getMetrics()
getDescriptor()
```

### Adaptateurs

- `LocalEngineAdapter` : appelle directement un moteur dans le Teensy.
- `RemoteEngineAdapter` : transforme les mêmes opérations en paquets UART.
- `SamplerEngineAdapter` : ajoute fichiers, slices et streaming.
- `LegacyEngineAdapter` : enveloppe les moteurs actuels pendant la migration.

Le séquenceur ne contient plus de grands `switch (engine)` répétés. Il appelle l'adaptateur du slot.

## Suppression des doublons actuels

Les fonctions suivantes doivent être centralisées une seule fois dans le rack :

- note-on/note-off/panic;
- sélection du moteur;
- gestion du patch;
- paramètres normalisés;
- enveloppe et filtre communs si le moteur n'en fournit pas;
- volume/mute/solo;
- métriques CPU/RAM;
- sérialisation projet;
- annonces vers l'ESP32;
- activation/désactivation du graphe audio;
- validation des bornes.

Chaque moteur ne conserve que son DSP particulier et la traduction de ses paramètres.

## AZ-BUS Control V1

### Transport

- UART 3,3 V point-à-point;
- débit initial : 1 Mbaud;
- un port série matériel du Teensy par slot pour le prototype;
- commandes binaires encadrées, pas des chaînes `String`;
- CRC16, numéro de séquence et accusé de réception;
- timeout et reconnexion automatique.

Le texte reste disponible uniquement pour la console de diagnostic.

### Types de messages

| Type | Direction | Rôle |
|---|---|---|
| HELLO | module → maître | Présence et version |
| DESCRIBE | module → maître | Capacités |
| CONFIG_AUDIO | maître → module | Format I2S/TDM |
| NOTE_ON/OFF | maître → module | Jeu |
| PARAM_SET/GET | bidirectionnel | Réglages |
| PATCH_LOAD/SAVE | bidirectionnel | Patches |
| CLOCK | maître → module | Tempo/position |
| TRANSPORT | maître → module | Play/stop |
| PANIC | maître → module | Silence immédiat |
| METRICS | module → maître | CPU/RAM/xruns |
| ERROR | bidirectionnel | Erreur structurée |
| HEARTBEAT | bidirectionnel | Surveillance |
| BOOTLOADER | maître → module | Préparation au flash |

### Découverte

Au démarrage :

1. alimentation du slot;
2. reset du module;
3. attente de `HELLO`;
4. lecture du manifeste;
5. négociation audio;
6. test de silence;
7. affectation à un Engine Slot;
8. publication du moteur vers l'ESP32 écran.

Si la carte ne répond pas, le rack reste utilisable sans elle.

## AZ-BUS Audio V1

### Premier prototype

Un seul slot externe audio :

- Teensy = maître I2S;
- carte moteur = esclave I2S;
- BCLK, LRCLK et éventuellement MCLK fournis par le Teensy;
- DATA va du module vers l'entrée I2S du Teensy;
- 44,1 kHz ou 48 kHz fixé pour tout le système;
- stéréo 16 ou 24 bits;
- masse commune obligatoire.

C'est la version réellement prototypable immédiatement.

### Plusieurs slots

Plusieurs sorties I2S ne doivent jamais conduire la même ligne DATA en même temps.

Évolutions possibles :

1. deux lignes DATA séparées avec entrée I2S quad;
2. TDM avec créneaux attribués aux modules capables de mettre leur sortie en haute impédance;
3. petit agrégateur audio dédié;
4. plusieurs entrées d'un codec/mixeur numérique.

La V1 ne promet donc pas huit cartes simultanées. Elle valide d'abord un slot externe propre.

## Connecteur physique proposé

Connecteur **2×10 broches détrompé**, niveau logique 3,3 V.

| Broche | Signal | Sens vu du Teensy |
|---:|---|---|
| 1–2 | +5 V module | Sortie |
| 3–4 | GND | — |
| 5 | +3,3 V auxiliaire limitée | Sortie |
| 6 | SLOT_PRESENT | Entrée |
| 7 | UART_TX | Sortie |
| 8 | UART_RX | Entrée |
| 9 | I2S_BCLK | Sortie |
| 10 | I2S_LRCLK | Sortie |
| 11 | I2S_MCLK | Sortie |
| 12 | I2S_DATA_IN | Entrée |
| 13 | I2S_DATA_OUT réserve | Sortie |
| 14 | RESET | Sortie |
| 15 | BOOT | Sortie |
| 16 | I2C_SDA réserve | Bidirectionnel |
| 17 | I2C_SCL réserve | Sortie |
| 18 | ID0 | Entrée |
| 19 | ID1 | Entrée |
| 20 | FAULT/IRQ | Entrée |

Ce tableau définit les fonctions du bus, pas encore les numéros GPIO définitifs du Teensy. Le pinout GPIO sera fixé après vérification des broches SAI/I2S réellement disponibles et du câblage actuel.

### Adaptateurs

Chaque famille de carte reçoit un petit adaptateur au même format mécanique :

- AZ-BUS → ESP32-S3;
- AZ-BUS → Teensy 4.1;
- AZ-BUS → RP2040/RP2350;
- AZ-BUS → Arduino 5 V avec conversion de niveau;
- AZ-BUS → Daisy Seed.

Une carte 5 V ne doit jamais envoyer directement 5 V dans une entrée du Teensy/ESP32. Son adaptateur contient les convertisseurs nécessaires.

## Format mécanique

À figer après le prototype électrique :

- même largeur et mêmes trous de fixation;
- connecteur au même emplacement;
- détrompeur empêchant l'inversion;
- hauteur maximale définie;
- zone interdite sous l'antenne des ESP32;
- dissipateur possible;
- port USB accessible pour le premier flash;
- étiquette moteur/version visible.

Le premier adaptateur recommandé est ESP32-S3 ou second Teensy, pas Arduino 8-bit : il permettra de tester tout le contrat à pleine vitesse.

## Alimentation

Chaque slot comporte :

- limitation de courant;
- condensateurs locaux;
- protection contre inversion;
- broche `SLOT_PRESENT`;
- signal `FAULT`;
- budget déclaré dans le manifeste.

Point de départ :

- 5 V par slot;
- 500 mA maximum pour le prototype;
- 3,3 V seulement comme auxiliaire faible courant;
- régulation 3,3 V principale sur l'adaptateur du module.

Le rack n'est pas hot-swap en V1. Insertion et retrait machine éteinte, ou slot explicitement coupé par un circuit prévu pour cela.

## Flash des moteurs

### V1

Chaque module se flashe par son USB natif. Le dépôt fournit un environnement PlatformIO par carte et moteur.

Exemples :

```
engine_esp32_plaits
engine_esp32_granular
engine_teensy_dexed
engine_teensy_sampler
engine_rp2350_chip
engine_arduino_lofi
```

### V2

Le Teensy/ESP32 écran peut commander RESET/BOOT, mais la programmation automatique universelle est difficile car ESP32, Teensy, RP2350 et Arduino n'utilisent pas le même bootloader.

La solution correcte est un outil hôte qui détecte le type de module et appelle le bon chargeur. Ne pas inventer un faux protocole de flash universel.

## Sécurité audio

- mute matériel/logique du slot pendant connexion et boot;
- fade-in après validation;
- limiteur sur le retour externe;
- détection DC et niveau impossible;
- timeout heartbeat → mute;
- `PANIC` prioritaire;
- compteur de xruns;
- moteur distant absent → piste silencieuse, jamais blocage du séquenceur.

## Migration du firmware actuel

### Étape 1 — inventaire

Repérer les doublons dans :

- `setTrackEngine()`;
- `trackNoteOn()/trackNoteOff()`;
- `applyTrackPatch()`;
- handlers ENGINE/PATCH/ENV/FILT/DXP;
- annonce d'état;
- sauvegarde de projet;
- miroir ESP32.

### Étape 2 — rack local

Créer l'interface commune et envelopper sans changer le son :

- ANALOG;
- BRAIDS;
- KARPLUS;
- EPIANO;
- DEXED en quarantaine;
- SAMPLER.

### Étape 3 — banc distant simulé

Un second microcontrôleur répond à HELLO/DESCRIBE/METRICS et produit une sinusoïde I2S. Aucun gros moteur tant que transport, silence et reconnexion ne sont pas fiables.

### Étape 4 — premier vrai module

Porter un moteur simple :

- Plaits ou Drum Synth sur ESP32-S3/second Teensy;
- mesurer latence UART→note→I2S;
- tester 1 000 notes et 30 minutes;
- débrancher/redémarrer uniquement selon la procédure V1.

### Étape 5 — catalogue

Ajouter les firmwares moteurs validés sans les charger tous dans le Teensy principal.

## Critères de réussite

- même projet avec moteur local ou distant;
- latence note-on stable;
- aucune dérive d'horloge audible;
- silence absolu au repos;
- reconnexion après reset du module;
- aucun crash si module absent;
- CPU/RAM affichés dans le diagnostic;
- patch sauvegardé avec l'identité/version du moteur;
- firmware principal compilable sans aucun module externe;
- CI séparée pour chaque firmware de moteur.

## Conclusion

Le concept est valide et puissant : AZ-2 devient une machine évolutive, pas une boîte figée. Le rack unifie d'abord les moteurs internes, puis le même contrat accueille des cartouches audio externes.

La première réalisation raisonnable est :

1. rack logiciel local;
2. un slot physique AZ-BUS;
3. UART 1 Mbaud pour le contrôle;
4. I2S pour le son;
5. adaptateur ESP32-S3 ou second Teensy;
6. moteur de test simple;
7. seulement ensuite Plaits, granular, FM ou sampler distant.
