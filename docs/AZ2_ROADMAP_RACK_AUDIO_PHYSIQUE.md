# AZ-2 — Feuille de route du rack audio physique

**Statut au 23 septembre 2026 :** architecture intégrée au prototype. Le
Teensy reçoit le mix GRANULAR + SPECTRAL du S3 agrégateur et conserve le mixage
final vers le PCM5102A. Le brochage réel est figé dans `AZ2_RACK_PINOUT.md`.
La qualification longue durée reste à faire ; ce document conserve aussi les
étapes d'étude antérieures pour expliquer les choix.

## 1. Base de départ à préserver

Le Teensy 4.1 reste maître du temps musical, du mixage final et du DAC
PCM5102A. L'ESP32-S3 de façade reste responsable de l'interface. Les moteurs
audio actuels continuent de fonctionner localement sur le Teensy.

Un module audio externe est un coprocesseur optionnel. Son absence, son
redémarrage ou son plantage ne doit jamais arrêter le séquenceur, les moteurs
locaux ou la sortie du DAC.

Le test production du 21 septembre 2026 donne la référence suivante :

- CPU courant 6,4 à 6,7 %, pic 7,8 % ;
- mémoire audio 134 blocs, pic 137 sur 700 ;
- tick séquenceur maximal 1 us pour un budget de 31 250 us à 120 BPM ;
- PLAY, STOP et PANIC validés par le port USB ;
- rack actif : 4 Analog, 2 EPiano, 2 Braids.

Ces chiffres décrivent le projet chargé lors du test, pas un pire cas avec
huit pistes lourdes.

## 2. Inventaire des broches Teensy 4.1

### Broches utilisées

| Fonction | Broches |
|---|---|
| UART façade ESP32, Serial1 | 0, 1 |
| Croix | 2, 3, 4, 5 |
| Boutons A/B/C/D | 6, 8, 9, 23 |
| Sortie I2S PCM5102A | 7, 20, 21 |
| Encodeur 1 | 14, 15, 16 |
| Encodeur 2 | 17, 18, 19 |
| Encodeur 3 | 22, 24, 25 |

### Broches accessibles actuellement libres

- 10, 11, 12, 13 : bus SPI principal complet, actuellement réservé ;
- 26, 27, 28, 29, 30, 31, 32 ;
- 33, 34, 35, 36, 37, 38, 39, 40, 41.

Cela représente **20 GPIO accessibles libres** sur les rangées principales.
Les broches 42 à 47 appartiennent au lecteur microSD natif et ne doivent pas
être récupérées pour le rack.

Deux UART matériels complets sont immédiatement disponibles sans recâblage :

- Serial7 : RX 28, TX 29 ;
- Serial8 : RX 34, TX 35.

Les autres paires UART principales entrent en conflit avec les contrôles ou
l'I2S existants.

## 3. Verdict sur un ESP dédié à un gros moteur

Oui, un ESP32-S3 supplémentaire peut héberger un moteur lourd isolé, à
condition de séparer deux flux :

1. **contrôle** : notes, paramètres, horloge et état ;
2. **audio** : échantillons produits par le module vers le Teensy.

Une liaison UART seule convient au contrôle, mais n'est pas le meilleur bus
pour plusieurs flux PCM haute fidélité. Un flux stéréo 16 bits à 44,1 kHz
représente déjà environ 1,41 Mbit/s avant framing, CRC et télémétrie.

Le module doit posséder sa propre alimentation régulée. La sortie 3,3 V du
Teensy est limitée et ne doit pas alimenter plusieurs ESP. Toutes les logiques
restent en 3,3 V avec masse commune ; aucune GPIO Teensy n'est tolérante au 5 V.

## 4. Architecture recommandée du rack

```text
ESP32 façade ── UART ── Teensy maître
                           │
                           ├── moteurs locaux
                           ├── SPI/CAN : contrôle des slots
                           ├── BCLK/LRCLK communs : horloge audio
                           ├── données audio slot 1
                           ├── données audio slot 2
                           └── mix final I2S → PCM5102A
```

### Bus de contrôle

Pour un premier prototype à un ou deux modules :

- Serial7 et Serial8 sont acceptables ;
- protocole binaire avec version, slot, type, longueur, séquence et CRC ;
- watchdog et message de présence périodique ;
- file bornée, aucune attente bloquante dans le Teensy.

Pour un rack de plusieurs slots :

- préférer CAN/CAN-FD avec transceiver, ou SPI maître avec un CS par slot ;
- ne pas chaîner plusieurs UART point à point sur les mêmes fils ;
- prévoir découverte, identification du moteur et négociation de version.

### Bus audio

La cible propre est un I2S/TDM synchrone dont le Teensy fournit BCLK et LRCLK.
Chaque module travaille ainsi sur la même horloge audio et aucun drift ne
s'accumule entre producteurs.

Le câblage actuel pose cependant un conflit : les entrées I2S usuelles sont
occupées par les commandes physiques. La première révision de rack devra soit :

1. déplacer quelques boutons vers un expander GPIO afin de libérer l'entrée
   I2S1 ;
2. réserver le second port I2S à un futur backplane et déplacer la croix ;
3. utiliser un codec/bridge TDM dédié sur le backplane.

SPI peut transporter des blocs audio pour un prototype, mais exige un anneau
tampon et une correction de dérive entre l'horloge du module et celle du DAC.
Il ne faut pas le présenter comme équivalent à un bus audio synchrone.

## 5. Contrat logiciel d'un moteur distant

Chaque slot expose au minimum :

- identifiant et version du moteur ;
- nombre maximal de voix ;
- liste de patches et paramètres ;
- NOTE_ON, NOTE_OFF, PARAM, PATCH, PANIC ;
- fréquence d'échantillonnage et nombre de canaux ;
- charge CPU, profondeur de file, underruns et redémarrages ;
- état READY/FAULT/MUTED.

Le Teensy conserve un adaptateur par slot. Pour le séquenceur, un moteur local
et un moteur distant doivent présenter la même interface logique.

En cas de perte du module, le Teensy met uniquement ce slot en sourdine et
continue de jouer le reste du morceau.

## 6. Phases de réalisation

### Phase 0 — référence production

- conserver les binaires production connus ;
- enregistrer CPU, mémoire, ISR et comportement audio ;
- ne pas modifier le câblage existant.

### Phase 1 — abstraction logicielle locale

- extraire l'interface commune Engine ;
- encapsuler les sept moteurs existants ;
- supprimer progressivement les allocations par huit inutiles ;
- ajouter des tests de changement de moteur et PANIC.

Critère : aucune différence audible ni régression de projet.

### Phase 2 — slot distant simulé

- simuler un moteur distant depuis un second port série de l'ordinateur ;
- tester déconnexion, CRC faux, retard, saturation de file et reconnexion ;
- mesurer la latence note → bloc audio.

Critère : aucune panne du rack principal si le slot disparaît.

### Phase 3 — premier ESP audio, contrôle seulement

- connecter un ESP32-S3 sur Serial7, broches 28/29 ;
- faire tourner FM2 ou un oscillateur simple ;
- valider commandes, watchdog et télémétrie sans transporter le son.

Critère : contrôle stable pendant une heure sans perte de séquence.

### Phase 4 — premier chemin audio

- construire un prototype I2S/TDM à horloge Teensy commune ;
- déplacer seulement les commandes nécessaires après validation du schéma ;
- entrer le moteur distant dans un canal séparé du mixeur ;
- mesurer bruit, jitter, latence et underruns.

Critère : silence numérique propre et aucune dérive pendant une heure.

### Phase 5 — backplane deux à quatre slots

- alimentation séparée dimensionnée avec marge ;
- protection, masse et découplage par slot ;
- connecteur polarisé avec 3,3 V logique, horloges, données et détection ;
- reset et mute indépendants par slot ;
- test à chaud interdit pour la première version.

### Phase 6 — intégration produit

- navigateur de moteurs et paramètres sur l'ESP32 de façade ;
- sauvegarde de l'identifiant du moteur et de son patch dans les projets ;
- fallback clair lorsqu'un module requis est absent ;
- matrice de compatibilité par firmware de module.

### Phase 7 — MIDI DIN du maître

- 6N138 vers Serial8 RX pin 34, 31 250 bit/s ;
- canaux 1–8 vers pistes 1–8, vélocité et running status ;
- CC120/123, Start, Stop et PANIC ;
- validation électrique du 6N138 puis test de stabilité ;
- asservissement MIDI Clock 24 PPQN seulement après mesure du tempo et de la
  phase sur banc.

## 7. Ordre des moteurs candidats

1. FM2 léger : validation du contrat et de la latence.
2. Synthèse wavetable : charge moyenne et patches volumineux.
3. Granulaire : bon candidat pour PSRAM locale au module.
4. Plaits/Braids étendu : moteur autonome par slot.
5. APU/chiptune : seulement après disponibilité d'une source d'événements
   fiable ; ne dépend pas de l'émulateur GB actuel.

DEXED ne justifie pas immédiatement un ESP externe : il faut d'abord terminer
le diagnostic MSFA/MKI sur le Teensy et déterminer si son problème est DSP,
routage, alimentation ou conversion analogique.

## 8. Décision

Le rack est **techniquement valide**, mais pas comme simple empilement d'ESP
reliés en UART. La version robuste nécessite :

- Teensy maître de l'horloge et du mix final ;
- bus de contrôle versionné et tolérant aux pannes ;
- bus audio synchrone séparé ;
- alimentation indépendante ;
- abstraction logicielle des moteurs avant le backplane physique.

Le premier prototype peut utiliser Serial7 sur 28/29 pour le contrôle. Le
câblage audio ne doit être décidé qu'après un schéma détaillé des entrées I2S
à libérer et un test sur un seul module.

## 9. Révision future de la carte écran et du câblage

Une future révision pourra revoir le faisceau complet sans remettre en cause
le prototype actuel :

- conserver un accès physique simple à BOOT, RESET et USB/UART de l'ESP32-S3
  de l'écran afin de pouvoir toujours le reflasher et le récupérer ;
- documenter les autres puces programmables présentes sur le module avant de
  prévoir leur mise à jour ; ne jamais supposer qu'un contrôleur de dalle ou
  de tactile possède un firmware utilisateur reflashable ;
- déplacer les commandes physiques qui bloquent les entrées I2S du Teensy
  vers un expander GPIO ou un petit contrôleur dédié ;
- réserver dès le schéma une horloge audio commune, des lignes TDM/I2S et un
  bus de contrôle pour les slots ;
- garder les connecteurs de programmation accessibles une fois la machine
  assemblée ;
- séparer alimentation écran, alimentation modules audio et logique 3,3 V,
  avec masse commune et protections adaptées.

Cette révision appartient à une nouvelle version matérielle. Le câblage AZ-2
actuel, déjà testé, reste figé jusqu'à ce qu'un prototype de backplane à un
seul module ait démontré un avantage audio mesurable.

## 10. Mesure de référence du firmware de production — 2026-09-21

Mesure faite sur le vrai Teensy 4.1, sortie de chaque piste coupée, avec une
note active par piste. Les moteurs à échantillon ont été redéclenchés pendant
la fenêtre de mesure. Le script reproductible est
`tools/measure_audio_rack.py`. Il remet le rack initial en place même en cas
d'erreur.

| Moteur | CPU 1 piste | CPU 2 pistes | CPU 4 pistes | CPU 8 pistes | Pic mémoire 8 pistes |
|---|---:|---:|---:|---:|---:|
| DEXED | 5,7 % | 7,5 % | 11,0 % | 18,0 % | 166 / 700 |
| EPIANO | 5,3 % | 6,7 % | 9,4 % | 15,0 % | 166 / 700 |
| BRAIDS | 4,7 % | 5,4 % | 6,8 % | 9,7 % | 166 / 700 |
| KARPLUS | 4,4 % | 4,9 % | 5,8 % | 7,6 % | 166 / 700 |
| ANALOG | 4,4 % | 4,8 % | 5,6 % | 7,2 % | 166 / 700 |
| SAMPLER | 4,5 % | 5,5 % | 5,4 % | 6,7 % | 174 / 700 |
| DRUM | 5,0 % | 5,5 % | 6,4 % | 8,3 % | 174 / 700 |

Conclusion : le Teensy possède encore une marge DSP et mémoire très large.
Un rack d'ESP ne doit donc pas être ajouté pour corriger une saturation qui
n'existe pas. Son intérêt sera d'apporter de nouveaux moteurs lourds ou une
isolation fonctionnelle. DEXED est le moteur local le plus coûteux observé,
mais huit instances actives restent autour de 18 % CPU dans ce scénario.

Limite : ce banc mesure une note par piste, pas la polyphonie maximale, les
effets au pire cas ni la qualité sonore analogique. Une seconde campagne devra
mesurer polyphonie, effets, séquenceur, SD et écran actifs ensemble.

### Stress polyphonique et effets

Le banc `tools/stress_audio_production.py` a ensuite tenu 60 secondes avec les
huit pistes en DEXED, deux notes par piste (16 voix, maximum configuré),
réverbération et délai à 100 %, liaison série vers l'écran maintenue :

- charge stable : 26,5–26,6 % ; pic 26,9 % ;
- mémoire stable : 158 blocs ; pic 166 / 700 ;
- 60 réponses de télémétrie sur 60 ;
- aucun blocage ni croissance mémoire ;
- rack initial restauré après le test.

Ce test ne valide pas encore la qualité audible, la lecture simultanée de WAV
depuis la SD ni un fonctionnement prolongé sur plusieurs heures.

## 11. Cartes disponibles et choix du banc

Si les cartes marquées S2 Mini sont bien des LOLIN/WEMOS basées sur
ESP32-S2FN4R2, elles disposent de 240 MHz, 4 Mo de flash, 2 Mo de PSRAM et
d'un périphérique I2S avec DMA. Elles sont adaptées à un premier moteur
granulaire dédié, avec les limites suivantes :

- un seul cœur : aucune tâche Wi-Fi ou interface lourde pendant l'audio ;
- grains et échantillons courts placés dans les 2 Mo de PSRAM ;
- buffers DMA internes, préallocation totale au démarrage ;
- sortie I2S esclave cadencée par le Teensy pour éviter la dérive ;
- commencer en mono, 44,1 kHz, 16 bits, 8 à 16 grains maximum ;
- mesurer les underruns avant d'augmenter la qualité ou la polyphonie.

Les D1 Mini classiques basées sur ESP8266 (80/160 MHz, 4 Mo de flash, sans
PSRAM annoncée) ne sont pas retenues pour le moteur granulaire. Elles restent
utiles comme contrôleurs de rack, passerelles MIDI, panneaux de commandes ou
générateurs lo-fi simples.

Attribution proposée :

1. S2 Mini n°1 : laboratoire granulaire ;
2. S2 Mini n°2 : module de référence/ secours, puis moteur spectral ou effet ;
3. D1 Mini n°1 : contrôleur et watchdog du rack ;
4. D1 Mini n°2 : MIDI/commandes ou banc de trafic.

Avant tout flash, lire par USB l'identifiant exact de chaque puce et vérifier
la PSRAM. Aucun brochage de rack ne doit être figé avant ce contrôle.

Le banc a finalement été basculé vers deux cartes ESP-WROOM-32D. La première
a été identifiée comme ESP32-D0WD-V3, double cœur 240 MHz, 4 Mo de flash et
sans PSRAM. Le granulaire 16 grains tient à environ 20,7 % d'un cœur avec un
buffer source de 64 Kio en RAM interne. Les S2 restent hors du chemin
d'intégration pour l'instant.
