# AZ-2 - DAC PCM5102A

Objectif: fixer la sortie audio AZ-2 autour du module PCM5102A.

**Son confirme et teste sur le vrai montage (2026-09-13)** avec la config
ci-dessous (carte EstarDyn/GY-PCM5102, achetee sur AliExpress) -- si tu
repars d'une carte identique ou similaire (jumpers H1L-H4L au dos), applique
directement cette config, elle est verifiee par le datasheet TI ET par un
test reel.

## Module retenu

| Point | Valeur |
| --- | --- |
| Puce DAC | Texas Instruments PCM5102A |
| Carte | EstarDyn / GY-PCM5102 (jumpers H1L-H4L au dos) |
| Interface | I2S 3 fils (BCK, LRCK, DIN), pas de MCLK requis |
| Resolution | 16 / 24 / 32 bits |
| Taux echantillonnage | 8 kHz a 384 kHz |
| SNR | 112 dB |
| Plage dynamique | 112 dB |
| THD+N | -93 dB |
| Niveau sortie | 2.1 VRMS, niveau ligne |
| Sortie | Jack stereo 3.5 mm + pads |
| Charge | 1 kOhm minimum |
| Alimentation | 3.3 V |
| MCLK | Non requis (PLL interne genere l'horloge depuis BCK) |

## Role dans AZ-2

Le PCM5102A est le DAC audio principal. Il transforme la sortie I2S du Teensy en sortie ligne stereo.

Il ne doit pas etre confondu avec:

- `MCP4728`: DAC 4 canaux pour CV/controle, option plus tard;
- audio ESP32: reserve aux fonctions systeme/retro, pas au moteur groovebox.

## Cablage Teensy 4.1 (confirme fonctionnel)

Base avec `AudioOutputI2S` de la Teensy Audio Library (format I2S standard,
pas Left-Justified).

| PCM5102A | Teensy 4.1 | Note |
| --- | --- | --- |
| VCC / VIN | 3.3 V | Le module indique logique et alimentation 3.3 V |
| GND | GND | Masse commune obligatoire |
| BCK / BCLK | Pin 21 | Bit clock I2S Teensy 4.x |
| LCK / LRCK / WS | Pin 20 | Word select / left-right clock -- **pas la pin 19**, erreur frequente (decalage d'une broche) |
| DIN | Pin 7 | Data out I2S depuis Teensy vers DAC |
| SCK | **A relier a GND directement sur la carte DAC** | Voir "Jumpers" ci-dessous -- NE PAS laisser flottant, NE PAS relier a une broche Teensy |
| XSMT / MUTE | Piloté par jumper H3L, voir plus bas | Pas de fil vers le Teensy pour l'instant |

Important: les lignes de donnees sont en logique 3.3 V. Ne pas injecter de 5 V.

## Jumpers H1L-H4L (carte EstarDyn/GY-PCM5102)

Config confirmee par le schema de reference officiel TI (datasheet
PCM5100A/PCM5102A, section 10.1.1 "Simplified Schematic, Hardware-Controlled
Subsystem" -- exactement notre cas: I2S 3 fils, controle par pins/jumpers,
pas de circuit de mute externe) **et** par un test audio reel.

| Jumper | Broche pilotee | Position a souder | Pourquoi |
| --- | --- | --- | --- |
| H1L | FLT (filtre) | **Gauche (GND/L)** | "FLT pin tied low" dans le schema TI |
| H2L | DEMP (de-emphasis) | **Gauche (GND/L)** | Pas utile pour de l'audio moderne |
| H3L | XSMT (mute) | **Droite (3.3V/H)** | Seul celui-ci va a droite -- demute le DAC |
| H4L | FMT (format audio) | **Gauche (GND/L)** | LOW = format I2S (celui du Teensy). HIGH = Left-Justified -> **incompatible, silence total garanti** |

Piege verifie en conditions reelles: mettre les 4 jumpers a droite (comme
suggere par certains tutoriels generiques en ligne) casse le son a cause de
H4L/FMT -- toujours verifier contre le datasheet du chip plutot qu'un
tutoriel generique quand un reglage ne correspond pas au bon fonctionnement.

### SCK: piege frequent

Le PCM5102A peut generer son horloge interne (PLL) a partir de BCK, mais
seulement si **SCK est relie a GND** en dur. Symptomes observes selon l'etat
de SCK:

| Etat de SCK | Symptome observe |
| --- | --- |
| Relie a une broche active du micro (ex: une broche non utilisee) | Silence total |
| Flottant (fil coupe, rien branche) | "Souffle"/bruit au lieu d'un son propre |
| Relie a GND (bon) | Son propre |

## Sortie audio

La sortie jack est une sortie ligne. Elle doit aller vers:

- enceintes amplifiees;
- ampli casque;
- mixette;
- entree ligne;
- ampli audio.

Ne pas brancher un casque passif directement comme sortie finale: le module n'est pas un ampli casque (fonctionne quand meme a faible volume avec un casque, teste, mais niveau ligne prevu pour un ampli).

## Firmware

Le moteur audio Teensy utilise Synth_Dexed (FM, portage MicroDexed-touch)
plutot qu'un simple sinus de test -- voir
[AZ2_PORTAGE_MICRODEXED_TOUCH.md](AZ2_PORTAGE_MICRODEXED_TOUCH.md).
`AudioOutputI2S` reste la sortie finale vers le DAC:

```cpp
AudioSynthDexed dexedVoice(4, SAMPLE_RATE);
AudioOutputI2S i2sOut;
AudioConnection patchCord1(dexedVoice, 0, i2sOut, 0);
AudioConnection patchCord2(dexedVoice, 0, i2sOut, 1);
```

Fichier: `src_teensy/az2_audio/main.cpp`.

## Tests -- deroule reel (2026-09-13)

1. Alimenter le PCM5102A en 3.3 V, relier GND, BCLK (21), LRCLK (20), DIN (7).
2. Souder les jumpers H1L/H2L/H4L a gauche, H3L a droite (voir tableau).
3. Relier SCK a GND directement sur la carte DAC.
4. Brancher la sortie jack vers un casque ou une entree ligne, volume bas au depart.
5. Flasher `master_teensy`.
6. Envoyer `PAD:00:DOWN:vel=100` sur le port serie Teensy (ou taper un pad sur la page AUDIO de l'ecran ESP32).
7. **Son confirme** : note tenue tant que `PAD:00:UP` n'est pas envoye.
8. `PAD:00:UP` coupe la note.

Si aucun son malgre cette config exacte: verifier au multimetre la
continuite des jumpers (une soudure qui a l'air bonne peut ne pas faire
vraiment contact) et la tension VCC/GND du DAC (doit lire ~3.3V).
