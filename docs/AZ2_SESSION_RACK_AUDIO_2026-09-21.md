# Session rack audio — 21 septembre 2026

## Résultat

La branche de travail est `rack-moteurs-externes`. Le premier module du rack
est désormais réservé au moteur **GRANULAR MAX** sur ESP32-S3 N16R8. La limite
retenue est **64 grains stéréo à 44,1 kHz**.

Le firmware de production Teensy n'a pas été modifié pour l'intégration
physique durant cette phase. Le moteur externe reste un laboratoire autonome
jusqu'au câblage Teensy prévu ultérieurement.

## Matériel réellement identifié

### ESP32-S3 retenu

| Propriété | Valeur mesurée |
|---|---|
| Puce | ESP32-S3 QFN56, révision 0.2 |
| CPU | 2 cœurs, 240 MHz |
| Flash | 16 Mo |
| PSRAM | 8 Mo intégrés |
| USB-série | QinHeng `1a86:55d3`, `/dev/ttyACM0` pendant la session |
| MAC | `9c:13:9e:b7:a5:3c` |
| Affectation | GRANULAR MAX, 64 grains |

### ESP-WROOM-32D fonctionnel

| Propriété | Valeur mesurée |
|---|---|
| Puce | ESP32-D0WD-V3, révision 3.1 |
| CPU | 2 cœurs, 240 MHz |
| Flash | 4 Mo |
| PSRAM | absente |
| MAC | `08:b6:1f:bc:8e:30` |
| Résultat | SPECTRAL SWARM retenu à 4 voix × 16 partiels |

D'autres cartes à convertisseur CH340 ont été détectées mais n'ont renvoyé
aucun octet depuis leur bootloader, y compris avec BOOT maintenu et GPIO0 à
GND. Elles ne sont pas retenues tant que leur panne matérielle n'est pas
diagnostiquée.

## Mesures du Teensy de production

Le firmware de production a été testé sur le vrai Teensy 4.1, volume des
pistes coupé pendant les charges synthétiques. Le rack initial a été restauré
après chaque banc.

| Moteur, huit pistes | CPU |
|---|---:|
| DEXED | 18,0 % |
| EPIANO | 15,0 % |
| BRAIDS | 9,7 % |
| DRUM | 8,3 % |
| KARPLUS | 7,6 % |
| ANALOG | 7,2 % |
| SAMPLER | 6,7 % |

Stress maximal effectué pendant 60 secondes : huit pistes DEXED, deux notes
par piste, délai et réverbération à 100 %. Résultat stable : 26,5–26,6 % CPU,
pic 26,9 %, mémoire 158 blocs, pic 166/700, aucune fuite ni coupure de
télémétrie.

Scripts reproductibles :

- `tools/measure_audio_rack.py` ;
- `tools/stress_audio_production.py`.

## Mesures GRANULAR MAX

Le moteur charge 30 secondes mono/16 bits/44,1 kHz en PSRAM, utilise une
fenêtre de Hann précalculée, interpolation linéaire, pitch, position et
panoramique indépendants. Chaque bloc est partagé entre les deux cœurs.

| Grains | Charge murale double cœur |
|---:|---:|
| 64 | 60,4 % |
| 80 | 75,1 % |
| 96 | 89,7 % |
| 112 | 104,4 % |
| 128 | 119,1 % |
| 160 | 148,6 % |
| 192 | 178,6 % |

Décision : 64 grains est la limite normale. Les ressources restantes sont
réservées à l'I2S, aux enveloppes, aux modulations, au protocole de contrôle et
aux protections. Le palier 80 reste expérimental et n'est pas une cible de
production.

## Mesures SPECTRAL SWARM

Le deuxième moteur a été flashé sur l'ESP-WROOM-32D. Il fait de la synthèse
additive/spectrale avec morphing, étirement des harmoniques et deux étages
résonants non linéaires. Ses quatre voix sont réparties entre les deux cœurs ;
les coefficients coûteux sont calculés une fois par bloc.

| Partiels par voix | Oscillateurs simultanés | Charge murale double cœur |
|---:|---:|---:|
| 16 | 64 | 64,0 % |
| 24 | 96 | 80,1 % |
| 32 | 128 | 96,1 % |
| 48 | 192 | 128,2 % |
| 64 | 256 | 160,4 % |

Décision : **4 voix × 16 partiels**, soit 64 oscillateurs simultanés. Cette
limite garde 36 % sur le calcul moyen et environ 29 % sur le pire bloc DMA
mesuré, pour les enveloppes, les commandes et les sécurités. Le palier 24
reste expérimental ; 32 et plus sont hors temps
réel. Après le test, 334568 octets de heap interne restaient libres, sans
PSRAM. Le diagnostic USB est fixé à 115200 bauds pour la fiabilité du CH340.
Le test DMA I2S 4×16 a ensuite produit 10335 blocs pendant 30 secondes, deux
fois, sans retard ni écriture courte. Le pire rendu était de 2051 µs pour un
budget de 2902 µs.

### Rappel : validation DMA I2S du GRANULAR MAX

| Mesure | Résultat |
|---|---:|
| Durée | 30 s |
| Blocs stéréo de 128 échantillons | 10335 |
| Blocs en retard | 0 |
| Écritures DMA incomplètes | 0 |
| Pire rendu | 1797 µs |
| Budget par bloc | 2902 µs |
| Marge du pire bloc | environ 38 % |
| Heap interne libre | environ 332 Ko |
| PSRAM libre | environ 5,70 Mo |

## Pinout figé côté ESP32-S3

La source de vérité logicielle est
`src_esp32/engine_lab_granular_s3_max/rack_pins.h`. Le tableau de câblage
complet est `docs/AZ2_RACK_PINOUT.md`.

| Fonction | ESP32-S3 | Teensy futur |
|---|---:|---:|
| I2S BCLK | GPIO7 | pin 21 |
| I2S WS/LRCLK | GPIO9 | pin 20 |
| I2S DATA OUT | GPIO11 | pin 8 / I2S RX |
| UART TX contrôle | GPIO16 | pin 28 / RX7 |
| UART RX contrôle | GPIO18 | pin 29 / TX7 |
| Masse | GND | GND |

Le banc actuel fait fonctionner le S3 en maître I2S. L'intégration directe
fera du Teensy le maître d'horloge et du S3 l'esclave I2S.

## Conflit matériel avant soudure définitive

La pin 8 du Teensy est actuellement utilisée par le bouton B et doit devenir
l'entrée audio I2S du moteur externe. Avant l'intégration :

1. déplacer le bouton B de la pin 8 vers la pin 10 ;
2. modifier `kBtnBPin` dans le firmware Teensy ;
3. ajouter l'entrée I2S au graphe et au mixeur Teensy ;
4. passer le S3 en esclave I2S ;
5. ajouter le protocole UART Serial7 sur les pins 28/29 ;
6. alimenter le module séparément en 5 V avec masse commune.

Ne pas alimenter le rack depuis le 3,3 V du Teensy et ne jamais appliquer 5 V
sur une GPIO.

## État logiciel en fin de session

- moteur GRANULAR MAX compilé et flashé sur le S3 ;
- 64 grains et DMA I2S validés ;
- moteur SPECTRAL SWARM double cœur compilé et flashé sur le WROOM-32D ;
- limite spectrale fixée à 4 voix × 16 partiels ;
- pinout centralisé dans le code ;
- tests natifs AZ-2 : 19/19 réussis ;
- firmwares production Teensy et écran compilés durant la session ;
- aucun émulateur GB/GBC déclaré fonctionnel ;
- intégration Teensy/S3 et contrôle musical encore à faire ;
- le moteur doit encore recevoir NOTE_ON/OFF, position, taille, densité,
  pitch, dispersion, panoramique, enveloppe et chargement d'échantillons.

## Historique Git de la session

| Commit | Contenu |
|---|---|
| `83745cc` | audits, corrections audio, PANIC, télémétrie et premiers laboratoires |
| `f43873b` | moteur granulaire S3 double cœur et benchmarks jusqu'à 192 grains |
| `ff1d191` | validation 64 grains, I2S DMA et pinout canonique |
