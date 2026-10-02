# Reproduire AZ-2 — pièces, câblage et firmware

Ce document décrit le prototype AZ-2 réellement utilisé au 21 septembre 2026.
Il sert de référence pour reconstruire une machine identique. Les anciens
plans Pico, matrice SparkFun et écran ILI9341 ne font pas partie de cette
version.

## 1. Nomenclature minimale

| Qté | Pièce | Référence utilisée | Remarques |
| ---: | --- | --- | --- |
| 1 | Calculateur audio | **PJRC Teensy 4.1**, i.MX RT1062, 600 MHz | Firmware `master_teensy` |
| 1 | Mémoire du Teensy | **2 puces PSRAM de 8 Mo compatibles Teensy 4.1** | 16 Mo détectés sur le prototype ; nécessaires aux samples |
| 1 | Carte écran | **VIEWE UEDX48480040E-WB-V1.3** | ESP32-S3 N16R8, écran RGB 480×480 GC9503V, tactile FT6336U, lecteur microSD |
| 1 | DAC audio | **EstarDyn / GY-PCM5102**, puce TI PCM5102A | Module avec jumpers H1L à H4L et sortie jack 3,5 mm |
| 3 | Encodeur rotatif poussoir | Type **EC11** ou module KY-040, quadrature, 5 broches | CLK, DT, SW, 3,3 V et GND |
| 1 | Croix directionnelle | 4 contacts momentanés normalement ouverts | Un contact par direction |
| 4 | Bouton poussoir | Contacts momentanés normalement ouverts | A, B, C et D |
| 2 | Carte microSD | microSD fiable, **FAT32** | Une pour l’ESP32, une pour le Teensy |
| 1 | Sortie audio | Câble jack stéréo 3,5 mm vers entrée ligne | Le PCM5102A n’est pas un ampli casque |
| — | Câblage | Fil souple, barrettes, masse commune, petite plaque à pastilles ou PCB | Signaux logiques en 3,3 V |
| 2 | Câbles USB de données | USB-C pour l’écran ; micro-USB pour le Teensy 4.1 | Nécessaires au flash et à l’alimentation du prototype |

Les références mécaniques du boîtier, des boutons et des capuchons ne sont pas
encore figées. Elles peuvent changer sans modifier l’électronique.

## 2. Architecture réelle

```text
Croix + A/B/C/D + 3 encodeurs
              │
              ▼
         Teensy 4.1 ── I2S ──> PCM5102A ──> sortie ligne stéréo
              ▲
              │ UART 921600 bauds, 3,3 V
              ▼
VIEWE ESP32-S3 + écran tactile 480×480 + microSD ROM/projets
```

Le Teensy produit tout le son et pilote le DAC. L’ESP32 affiche l’interface,
lit le tactile et exécute l’émulateur Game Boy. Le son Game Boy est envoyé au
Teensy par le même UART. Toutes les masses doivent être communes.

## 3. Câblage Teensy 4.1 vers DAC PCM5102A

| PCM5102A | Teensy 4.1 |
| --- | --- |
| VCC/VIN | 3,3 V |
| GND | GND commun |
| DIN | pin 7 |
| LRCK/LCK/WS | pin 20 |
| BCK/BCLK | pin 21 |
| SCK | GND directement sur le module DAC |

Réglage des jumpers du module testé : H1L, H2L et H4L à gauche/GND ; H3L
seul à droite/3,3 V. H4L à droite sélectionne un format incompatible et rend
la sortie silencieuse. SCK flottant produit du bruit ; il doit être relié au
GND.

## 4. Liaison ESP32-S3 vers Teensy

| Carte écran VIEWE | Teensy 4.1 |
| --- | --- |
| GPIO19 TX | pin 0 RX1 |
| GPIO20 RX | pin 1 TX1 |
| GND | GND |

Le débit courant est **921600 bauds** (`az2::kControlBaud`). Il faut croiser
TX et RX. Aucun convertisseur de niveau n’est nécessaire : les deux cartes
utilisent une logique 3,3 V.

## 5. Contrôles physiques

Tous les contacts relient la broche indiquée au GND lorsqu’ils sont pressés.
Le firmware active `INPUT_PULLUP` ; aucune résistance externe n’est requise.

| Fonction | Pin Teensy |
| --- | ---: |
| Croix haut / bas / gauche / droite | 2 / 3 / 4 / 5 |
| Boutons A / B / C / D | 6 / 8 / 9 / 23 |
| Encodeur 1 CLK / DT / SW | 14 / 15 / 16 |
| Encodeur 2 CLK / DT / SW | 17 / 18 / 19 |
| Encodeur 3 CLK / DT / SW | 22 / 24 / 25 |

Les encodeurs sont alimentés en 3,3 V. Le prototype utilise quatre transitions
de quadrature par cran. Le sens est inversé en logiciel par rapport au câblage
du premier montage.

## 6. Broches intégrées de la carte écran

Ces liaisons sont déjà réalisées sur la carte VIEWE et ne doivent pas être
recâblées :

| Fonction | GPIO ESP32-S3 |
| --- | --- |
| RGB DE / VSYNC / HSYNC / PCLK | 18 / 17 / 16 / 21 |
| Rouge R0–R4 | 4 / 3 / 2 / 1 / 0 |
| Vert G0–G5 | 10 / 9 / 8 / 7 / 6 / 5 |
| Bleu B0–B4 | 15 / 14 / 13 / 12 / 11 |
| Rétroéclairage | 38 |
| Commandes écran CS / CLK / SDA | 39 / 48 / 47 |
| Tactile FT6336U SDA / SCL | 40 / 41 |
| microSD CS / CLK / MOSI / MISO | 47 / 45 / 42 / 46 |

Le GPIO47 est partagé entre la commande écran au démarrage et le CS microSD.
Le firmware initialise l’écran avant la carte SD ; cet ordre est obligatoire.

## 7. Cartes microSD

### Carte de l’ESP32

Format FAT32. Répertoires utilisés :

```text
/games       ROM .gb et .gbc, sauvegardes .sav/.rtc
/projects    projets AZ-2 et sauvegardes .bak
```

Les ROM commerciales ne sont pas distribuées avec le projet. Utiliser des ROM
homebrew ou ses propres copies légales.

### Carte du Teensy

Format FAT32 dans le lecteur microSD intégré du Teensy 4.1 :

```text
/samples     fichiers WAV PCM utilisés par le sampleur et les pads
/captures    enregistrements WAV du son Game Boy
/patches     sauvegardes de patches
```

Les WAV pris en charge par le flux actuel sont PCM non compressés. Les samples
assignés sont chargés en PSRAM avant lecture.

## 8. Logiciels et compilation

Installer Git, Python 3 et PlatformIO 6.1.x. Depuis la racine du dépôt :

```bash
pio run -e master_teensy
pio run -e screen_esp
```

Flasher le Teensy :

```bash
pio run -e master_teensy -t upload
```

Mettre l’ESP32-S3 en mode BOOT puis flasher :

```bash
pio run -e screen_esp -t upload --upload-port /dev/ttyUSB0
```

Les deux binaires doivent provenir du même commit Git, car ils partagent le
protocole défini dans `lib/AZ2_Protocol/AZ2_Protocol.h`.

## 9. Contrôle après assemblage

1. Vérifier au multimètre qu’aucun rail 5 V n’arrive sur une entrée logique.
2. Vérifier la masse commune et le croisement TX/RX.
3. Flasher les deux cartes depuis le même commit.
4. Confirmer `HELLO:TEENSY_AUDIO`, `DISPLAY:READY`, `TOUCH:FT6336U:READY` et
   `SD:READY` sur le journal ESP32.
5. Tester les quatre directions, A/B/C/D et les trois encodeurs.
6. Déclencher un pad et contrôler la sortie ligne du PCM5102A à faible volume.
7. Sauvegarder puis recharger un projet de test.
8. Charger une ROM homebrew et vérifier image, commandes, audio et sauvegarde.

## 10. Éléments volontairement absents

- Raspberry Pi Pico ;
- matrice SparkFun 4×4 et multiplexeur CD74HC4067 ;
- écran ILI9341 ;
- shield audio SGTL5000 ;
- convertisseur de niveau logique ;
- Wi-Fi et Bluetooth dans le firmware actuel.

Ces éléments appartiennent à d’anciens plans et ne sont pas nécessaires pour
reproduire le prototype fonctionnel.
