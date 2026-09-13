# AZ-2 - DAC PCM5102A

Objectif: fixer la sortie audio AZ-2 autour du module PCM5102A.

## Module retenu

| Point | Valeur |
| --- | --- |
| Puce DAC | Texas Instruments PCM5102A |
| Interface | I2S, compatible left-justified selon module |
| Resolution | 16 / 24 / 32 bits |
| Taux echantillonnage | 8 kHz a 384 kHz |
| SNR | 112 dB |
| Plage dynamique | 112 dB |
| THD+N | -93 dB |
| Niveau sortie | 2.1 VRMS, niveau ligne |
| Sortie | Jack stereo 3.5 mm + pads |
| Charge | 1 kOhm minimum |
| Alimentation | 3.3 V |
| MCLK | Non requis |

## Role dans AZ-2

Le PCM5102A est le DAC audio principal. Il transforme la sortie I2S du Teensy en sortie ligne stereo.

Il ne doit pas etre confondu avec:

- `MCP4728`: DAC 4 canaux pour CV/controle, option plus tard;
- audio ESP32: reserve aux fonctions systeme/retro, pas au moteur groovebox.

## Cablage Teensy 4.1 cible

Base avec `AudioOutputI2S` de la Teensy Audio Library.

| PCM5102A | Teensy 4.1 | Note |
| --- | --- | --- |
| VCC / VIN | 3.3 V | Le module indique logique et alimentation 3.3 V |
| GND | GND | Masse commune obligatoire |
| BCK / BCLK | Pin 21 | Bit clock I2S Teensy 4.x |
| LCK / LRCK / WS | Pin 20 | Word select / left-right clock |
| DIN | Pin 7 | Data out I2S depuis Teensy vers DAC |
| SCK / MCLK | Non connecte | PCM5102A peut fonctionner sans MCLK |
| XSMT / MUTE | GPIO optionnel | A cabler plus tard pour mute propre |

Important: les lignes de donnees sont en logique 3.3 V. Ne pas injecter de 5 V.

## Sortie audio

La sortie jack est une sortie ligne. Elle doit aller vers:

- enceintes amplifiees;
- ampli casque;
- mixette;
- entree ligne;
- ampli audio.

Ne pas brancher un casque passif directement comme sortie finale: le module n'est pas un ampli casque.

## Firmware

La base PlatformIO AZ-2 utilise:

```cpp
#define I2S_AUDIO_ONLY
AudioOutputI2S i2sOut;
```

Fichier de depart: `src_teensy/az2_audio/main.cpp`.

## Tests conseilles

1. Alimenter le PCM5102A en 3.3 V.
2. Relier GND, BCLK, LRCLK et DIN.
3. Brancher la sortie jack vers une entree ligne avec volume bas.
4. Flasher `master_teensy`.
5. Envoyer `PLAY` sur le port serie Teensy.
6. Verifier qu'un son sinus sort proprement.
7. Envoyer `STOP` et verifier le silence.
