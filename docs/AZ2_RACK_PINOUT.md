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
