# AZ-2 — pinout canonique du rack audio externe

Ce fichier est la référence courte à utiliser pendant le câblage. Les mêmes
valeurs sont compilées dans
`src_esp32/engine_lab_granular_s3_max/rack_pins.h`.

## État actuel : banc ESP32-S3 autonome

Le S3 est maître I2S et peut envoyer directement le moteur granulaire vers un
DAC séparé. Le Teensy n'est pas raccordé pendant ce test.

| Signal | ESP32-S3 N16R8 | Destination DAC | Direction |
|---|---:|---|---|
| I2S BCLK | GPIO7 | BCK/BCLK | S3 → DAC |
| I2S WS/LRCLK | GPIO9 | LCK/LRCK/WS | S3 → DAC |
| I2S DATA | GPIO11 | DIN | S3 → DAC |
| Masse | GND | GND | commun |

Le firmware fonctionne et mesure le DMA même sans DAC branché. Pour une
écoute, vérifier d'abord la tension VIN acceptée par le breakout DAC. Les
GPIO du S3 restent exclusivement en logique 3,3 V.

## Réservation pour l'intégration Teensy ↔ ESP32-S3

| Fonction | Teensy 4.1 | ESP32-S3 | Direction |
|---|---:|---:|---|
| Contrôle UART TX du S3 | pin 28 / RX7 | GPIO16 / TX | S3 → Teensy |
| Contrôle UART RX du S3 | pin 29 / TX7 | GPIO18 / RX | Teensy → S3 |
| Masse logique | GND | GND | commun |
| Débit prévu | Serial7, 921600 | UART, 921600 | bidirectionnel |

## Audio intégré futur : Teensy maître d'horloge

Les broches S3 restent identiques, mais GPIO7 et GPIO9 passent d'une sortie à
une entrée lorsque le S3 est configuré en esclave I2S.

| Signal | Teensy 4.1 | ESP32-S3 | Direction |
|---|---:|---:|---|
| BCLK | pin 21 | GPIO7 | Teensy → S3 |
| LRCLK | pin 20 | GPIO9 | Teensy → S3 |
| Audio granulaire | pin 8 / I2S RX | GPIO11 / DOUT | S3 → Teensy |
| Masse | GND | GND | commun |

Conflit connu : la pin 8 du Teensy porte actuellement le bouton B. Avant
l'intégration audio, déplacer uniquement ce bouton vers la pin 10 et modifier
`kBtnBPin` dans le firmware Teensy. Ne faire aucune modification physique
avant le test autonome avec DAC séparé.

## Broches réservées

| Carte | Réservé | Ne pas réutiliser pour |
|---|---|---|
| ESP32-S3 | GPIO7, 9, 11 | boutons, SPI ou LED |
| ESP32-S3 | GPIO16, 18 | I2C, encodeurs ou sélection de slot |
| Teensy | 20, 21 | nouvelles commandes ; horloges audio existantes |
| Teensy | 28, 29 | autre périphérique série |
| Teensy | 10 | autre fonction si le bouton B doit quitter la pin 8 |

## Alimentation

- alimenter le S3/rack par un 5 V séparé et dimensionné ;
- relier toutes les masses avant les lignes de signal ;
- ne pas alimenter le rack depuis le 3,3 V du Teensy ;
- couper l'alimentation avant toute modification de fils ;
- ne jamais appliquer 5 V à une GPIO Teensy ou ESP32-S3.

## Tableau global retenu — Teensy + S3 + WROOM

Ce tableau décrit le faisceau cible. **Ne pas relier BCLK/LRCLK tant que le
firmware WROOM n'est pas passé du mode maître au mode esclave** : deux maîtres
sur les mêmes fils peuvent endommager ou bloquer les sorties.

| Signal | Source | Destination | Rôle |
|---|---|---|---|
| BCLK commun | Teensy pin 21 | S3 GPIO7 + WROOM GPIO26 | horloge bits, Teensy seul maître |
| LRCLK commun | Teensy pin 20 | S3 GPIO9 + WROOM GPIO25 | 44,1 kHz, Teensy seul maître |
| Audio spectral | WROOM GPIO22 | S3 GPIO12 | PCM stéréo WROOM vers agrégateur |
| Audio agrégé | S3 GPIO11 | Teensy pin 8 | GRANULAR + SPECTRAL vers mixeur Teensy |
| Contrôle rack TX | Teensy pin 29 / TX7 | S3 GPIO18 / RX | commandes du maître |
| Contrôle rack RX | S3 GPIO16 / TX | Teensy pin 28 / RX7 | état, métriques et erreurs |
| Contrôle spectral | S3 GPIO4 / TX | WROOM GPIO16 / RX | commandes relayées au spectral |
| Retour spectral | WROOM GPIO17 / TX | S3 GPIO5 / RX | état, métriques et erreurs |
| Masse logique | GND Teensy | GND S3 + GND WROOM | référence commune obligatoire |

Le bouton B doit quitter la pin 8 du Teensy et aller sur la pin 10 avant de
connecter `Audio agrégé`. La sortie PCM5102A ne change pas : DATA pin 7,
LRCLK pin 20 et BCLK pin 21.

## MIDI DIN IN réservé au Teensy

| Signal | Connexion |
|---|---|
| UART MIDI RX | sortie 6N138 pin 6 -> Teensy pin 34 / RX8 |
| Pull-up sortie | 6N138 pin 6 -> 3,3 V, valeur à valider au schéma |
| Alimentation logique | 6N138 pin 8 -> 3,3 V ; pin 5 -> GND logique |
| Accélération | 6N138 pin 7 -> GND par résistance, valeur à valider |
| Entrée optique | DIN MIDI pins 4/5 -> résistance + LED pins 2/3 du 6N138 |
| Protection | diode antiparallèle sur la LED du 6N138 |
| Blindage DIN | pin 2 DIN vers châssis si prévu, jamais vers la masse logique du récepteur |

Serial8 fonctionne à 31250 bit/s, 8-N-1. La pin 35 / TX8 reste libre pour un
éventuel MIDI OUT futur ; elle n'est pas nécessaire au MIDI IN.

## Alimentation globale

Le Teensy, le S3 et le WROOM doivent recevoir une alimentation correctement
dimensionnée. Pour les ESP32, utiliser le 5 V/VIN prévu par leur carte de
développement après vérification du marquage exact. Ne pas alimenter les deux
ESP32 depuis la sortie 3,3 V du Teensy. Toutes les masses logiques sont
communes, mais la boucle MIDI en amont du 6N138 reste isolée.
