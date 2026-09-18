# AZ-2 — Premier module moteur ESP32 : AZ-VA1

**Statut : conception du prototype. Aucun câblage définitif ni firmware moteur n’est encore validé sur le matériel.**

## Décision

Le premier vrai module AZ-BUS sera un ESP32 dédié à un moteur polyphonique moderne nommé provisoirement **AZ-VA1**. Il ne remplacera ni le Teensy maître ni le sampler : il devient une cartouche sonore esclave.

- Teensy 4.1 : séquenceur, horloge, rack, mixage, effets et DAC final.
- ESP32-S3 écran : interface, SD et installation des firmwares.
- ESP32 moteur : synthèse AZ-VA1 uniquement.
- ESP8266 : banc de protocole ou moteur lo-fi léger, pas moteur principal haute qualité.

## Pourquoi AZ-VA1

Le Teensy possède déjà Dexed, ePiano, Braids, Karplus et un oscillateur analogique simple. Un autre portage identique ajouterait surtout des doublons. AZ-VA1 doit apporter la zone encore faible du catalogue : nappes larges, basses modernes, leads supersaw et tables d’ondes modulables.

### Cible sonore V1

- 8 voix sur ESP32-S3 avec PSRAM ;
- 4 à 6 voix sur ESP32 classique selon mesures ;
- 2 oscillateurs par voix + sub + bruit ;
- formes VA et tables d’ondes ;
- unisson/supersaw ;
- filtre résonant multimode ;
- enveloppe amplitude et enveloppe filtre ;
- 2 LFO ;
- matrice de modulation réduite ;
- glide, mono, legato et poly ;
- saturation douce ;
- chorus stéréo optionnel ;
- 16 patches de démonstration ;
- aucune allocation mémoire dans la boucle audio.

Format cible : 44,1 kHz, stéréo, 16 bits pour le premier banc. Le passage en 24 bits ne vient qu’après mesure : davantage de bits ne répare jamais un moteur qui décroche.

## Audio numérique et broche à récupérer

Le PCM5102A utilise déjà :

- Teensy 21 : BCLK ;
- Teensy 20 : LRCLK ;
- Teensy 7 : DATA vers le DAC.

L’entrée I2S standard du Teensy utilise la ligne DATA IN sur la broche 8. Cette broche est actuellement attribuée au bouton B dans le plan AZ-2. Pour recevoir directement le moteur externe dans le mixeur numérique :

| Fonction actuelle | Modification proposée |
| --- | --- |
| Bouton B : Teensy 8 | déplacer vers Teensy 26 ou 27 |
| I2S DATA du module | connecter à Teensy 8 |
| BCLK du Teensy | partager la broche 21 vers le module |
| LRCLK du Teensy | partager la broche 20 vers le module |
| DATA Teensy vers PCM5102A | conserver la broche 7 |

Le Teensy reste maître I2S. Le module ESP32 doit fonctionner en émetteur I2S esclave, cadencé par BCLK/LRCLK du Teensy. Il ne doit jamais injecter sa propre horloge sur ces lignes.

Ce déplacement n’est pas à réaliser avant un test de continuité et un firmware de banc. La documentation de câblage principale ne sera figée qu’après validation réelle.

## Contrôle et maintenance du slot

Proposition cohérente avec le plan de flash :

| Signal | Teensy proposé |
| --- | ---: |
| UART RX module | 28 |
| UART TX module | 29 |
| RESET/EN module | 30 |
| BOOT/GPIO0 module | 31 |
| SLOT_PRESENT | 32 |
| READY/FAULT | 33 |
| I2S DATA IN | 8 |
| I2S BCLK | 21 partagé |
| I2S LRCLK | 20 partagé |

Le port UART matériel exact et le sens RX/TX doivent être confirmés par un programme boucle locale avant soudure.

## Contrat AZ-VA1

Paramètres normalisés proposés :

| ID | Paramètre | Plage |
| ---: | --- | --- |
| 0 | Oscillateur 1 forme/table | 0–127 |
| 1 | Oscillateur 2 forme/table | 0–127 |
| 2 | Accord oscillateur 2 | -24 à +24 demi-tons |
| 3 | Désaccord/unisson | 0–127 |
| 4 | Mix oscillateurs | 0–127 |
| 5 | Coupure filtre | 0–127 |
| 6 | Résonance | 0–127 |
| 7 | Enveloppe filtre | 0–127 bipolaire |
| 8–11 | ADSR amplitude | 0–127 |
| 12–15 | ADSR filtre | 0–127 |
| 16 | LFO 1 vitesse | synchronisé/libre |
| 17 | LFO 1 destination | enum |
| 18 | LFO 1 profondeur | 0–127 bipolaire |
| 19 | Saturation | 0–127 |
| 20 | Chorus | 0–127 |
| 21 | Glide | 0–127 |
| 22 | Mode voix | mono/legato/poly |
| 23 | Volume moteur | 0–127 |

Messages indispensables : HELLO, DESCRIBE, CONFIG_AUDIO, NOTE_ON, NOTE_OFF, ALL_NOTES_OFF, PARAM_SET, PATCH_LOAD, CLOCK, TRANSPORT, METRICS, PANIC et HEARTBEAT.

## Répartition des deux cœurs ESP32

- cœur audio : rendu DSP et remplissage DMA uniquement ;
- autre cœur : UART AZ-BUS, paramètres, diagnostic et chargeur ;
- Wi-Fi et Bluetooth désactivés pendant le jeu ;
- échange des paramètres par file bornée ou snapshot atomique ;
- aucun String, log ou accès flash dans le callback audio.

## Usage réaliste des ESP8266

Les ESP8266/ESP-12F possédés restent utiles, mais leur RAM limitée, leur cœur unique et l’absence de PSRAM les rendent inadaptés au moteur AZ-VA1 complet.

Rôles pertinents :

1. **AZ-BUS Loopback** : HELLO/DESCRIBE/HEARTBEAT, mesures de débit et reprise après reset.
2. **AZ-CHIP1** : moteur monophonique lo-fi, oscillateurs pulse/noise, arpège et filtre simple.
3. **Contrôleur auxiliaire** : boutons, LEDs, capteurs ou MIDI distant.
4. **Simulateur de panne** : coupures, mauvais CRC, heartbeat perdu, afin d’endurcir le rack.

Un ESP8266 peut donc devenir une cartouche chiptune amusante. Lui demander huit voix wavetable, deux filtres et un chorus serait transformer une mobylette en autobus.

## Choix suivant le matériel disponible

| Carte trouvée dans le stock | Usage recommandé |
| --- | --- |
| ESP32-S3 avec PSRAM | AZ-VA1 complet, priorité 1 |
| ESP32 classique WROOM/WROVER | AZ-VA1 réduit ; WROVER préférable si PSRAM |
| ESP32-C3 | protocole, mono ou petit moteur ; pas premier choix audio |
| ESP8266 ESP-12F | AZ-BUS Loopback puis AZ-CHIP1 |

Avant d’écrire le firmware définitif, relever sur chaque carte : référence exacte du module, taille flash, présence/taille PSRAM, broches accessibles et régulateur d’alimentation.

## Ordre de construction

### Banc 0 — ESP8266

- répondre à HELLO/DESCRIBE ;
- accepter NOTE_ON/OFF et PARAM_SET sans produire de son ;
- tester CRC, heartbeat, reset et reconnexion ;
- vérifier la fenêtre de détection sur l’écran.

### Banc 1 — ESP32 sinusoïde

- UART 1 Mbaud ;
- I2S esclave 44,1 kHz ;
- sinusoïde stéréo à niveau faible ;
- ajout d’AudioInputI2S dans le graphe Teensy ;
- mute, limiteur, détection silence et compteur d’erreurs.

### Banc 2 — AZ-VA1 minimal

- 4 voix ;
- 2 oscillateurs ;
- ADSR ;
- filtre ;
- huit paramètres ;
- quatre patches.

### Banc 3 — AZ-VA1 complet

- 8 voix si les mesures le permettent ;
- wavetable, unisson, deux LFO, saturation et chorus ;
- fenêtre d’édition dédiée ;
- installation du firmware depuis la SD ;
- endurance 30 minutes puis 1 000 note-on/note-off.

## Critères de validation

- aucune dérive de hauteur ou clic périodique ;
- silence au repos ;
- latence stable entre commande et audio ;
- aucun xrun pendant changement de paramètres ;
- PANIC coupe le module immédiatement ;
- perte du heartbeat mute le retour ;
- reset du module ne bloque jamais le Teensy ;
- moteur absent : AZ-2 continue avec ses moteurs locaux et son sampler ;
- flash interrompu : récupération possible par USB/BOOT.

## Décision immédiate

Ne pas coder AZ-VA1 à l’aveugle. La prochaine donnée nécessaire est une photo nette recto/verso ou la référence imprimée de chaque ESP32 disponible. Ensuite seulement seront figés l’environnement PlatformIO, le pinout de l’adaptateur et le budget de voix.