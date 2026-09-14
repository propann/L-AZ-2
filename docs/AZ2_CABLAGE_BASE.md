# AZ-2 - Cablage complet v0

Objectif: demarrer la groovebox avec un cablage clair, deux firmwares PlatformIO, et des points a completer seulement la ou il manque encore une reference exacte.

## Architecture generale

```mermaid
flowchart TD
    SD[Carte TF / microSD] --> ESP[ESP32-S3 ecran 480x480]
    PAD[Matrice SparkFun 4x4 RGB] --> MUX[Multiplexeurs / drivers facade]
    MUX --> ESP
    ESP --> UART[UART commandes]
    UART --> TEENSY[Teensy 4.1 moteur son]
    TEENSY --> I2S[I2S audio]
    I2S --> DAC[PCM5102A sortie ligne]
```

## Roles figes

| Bloc | Role | Firmware |
| --- | --- | --- |
| ESP32-S3 ecran 4 pouces | Tete de controle, UI, scan matrice, LEDs, Wi-Fi, SD | `src_esp32/az2_control` |
| Matrice SparkFun 4x4 RGB | Pads, steps, mutes, scenes, feedback LED | Geree par ESP32 |
| Multiplexeurs / drivers | Economie de GPIO, scan boutons, pilotage LEDs | Pins a renseigner |
| Teensy 4.1 | Moteur son, sequenceur, synthese MicroDexed adaptee | `src_teensy/az2_audio` |
| PCM5102A | DAC audio stereo I2S, sortie ligne | Sortie du Teensy |
| MIDI / CV | Hors scope v0 | Plus tard |

## Alimentation et masses

| Rail | Alimente | Note |
| --- | --- | --- |
| 5 V | Module ecran ESP32 si entree USB-C/module | Selon carte ecran |
| 3.3 V | PCM5102A, logique multiplexeurs, signaux | Ne pas injecter 5 V sur GPIO |
| GND commun | ESP32, Teensy, PCM5102A, matrice, mux | Obligatoire pour UART/I2S/scan |

Regle dure: tous les signaux logiques doivent rester en 3.3 V.

## ESP32 vers Teensy

Le lien ESP32 -> Teensy commence en UART serie simple. USB MIDI viendra plus tard si besoin.

| Signal | ESP32 | Teensy | Note |
| --- | --- | --- | --- |
| TX ESP32 | GPIO17 | RX1 (pin 0) | Commandes `PAD`, `PLAY`, `STOP` |
| RX ESP32 | GPIO18 | TX1 (pin 1) | Etat audio, LED, pattern |
| GND | GND | GND | Masse commune obligatoire |

Valide sur ESP32-S3-DevKitC-1 nu (bring-up), teste le 2026-09-13 : boot serie OK
(`WIFI:OFF:BOOT`, `SD:SKIP:PINS_NOT_SET`, heartbeat `STATUS:ESP32_CONTROL:READY`).
A reverifier si on passe au module ecran ESP32-4848S040C_I: son bus RGB +
tactile + SD occupe la quasi-totalite des GPIO, ces pins devront peut-etre
changer.

Debit cible: `230400`.

## PCM5102A vers Teensy 4.1

Le PCM5102A est le DAC audio principal. Il sort un niveau ligne 2.1 VRMS et ne demande pas de MCLK.

| PCM5102A | Teensy 4.1 | Note |
| --- | --- | --- |
| VCC / VIN | 3.3 V | Module annonce en 3.3 V |
| GND | GND | Masse commune |
| BCK / BCLK | Pin 21 | Bit clock I2S Teensy 4.x |
| LCK / LRCK / WS | Pin 20 | Word select / left-right clock |
| DIN | Pin 7 | Data out I2S Teensy |
| SCK / MCLK | Non connecte | PCM5102A: MCLK non requis |
| XSMT / MUTE | GPIO optionnel | Mute logiciel plus tard |

Sortie jack: vers entree ligne, enceinte amplifiee, mixette ou ampli. Pas un ampli casque direct.

## Matrice SparkFun 4x4 RGB

Le guide SparkFun explique que la carte 4x4 est une matrice: les boutons utilisent lignes/colonnes, et les LEDs RGB sont trois matrices superposees avec cathodes communes par colonnes. Les connexions du bas sont plutot des colonnes que de vrais grounds.

Source de reference: `https://learn.sparkfun.com/tutorials/button-pad-hookup-guide/all`

### Logique de scan

| Element | Principe |
| --- | --- |
| Boutons | Selectionner une colonne, lire les lignes |
| LEDs rouges | Selectionner une colonne cathode, piloter les lignes rouges |
| LEDs vertes | Meme logique avec lignes vertes |
| LEDs bleues | Meme logique avec lignes bleues |
| Anti-rebond | Cote ESP32 avant envoi au Teensy |
| Multi-appuis | Prevoir diodes ou filtrage si necessaire |

### Nommage AZ-2

Pour eviter les confusions du silkscreen SparkFun, AZ-2 nomme les axes comme ceci:

| Axe | Nom AZ-2 | Pads |
| --- | --- | --- |
| Colonne A | `COL_0` | 0, 4, 8, 12 |
| Colonne B | `COL_1` | 1, 5, 9, 13 |
| Colonne C | `COL_2` | 2, 6, 10, 14 |
| Colonne D | `COL_3` | 3, 7, 11, 15 |
| Ligne 1 | `ROW_0` | 0, 1, 2, 3 |
| Ligne 2 | `ROW_1` | 4, 5, 6, 7 |
| Ligne 3 | `ROW_2` | 8, 9, 10, 11 |
| Ligne 4 | `ROW_3` | 12, 13, 14, 15 |

### Mapping pad_id

| Ligne | Pads |
| --- | --- |
| Ligne 1 | 0, 1, 2, 3 |
| Ligne 2 | 4, 5, 6, 7 |
| Ligne 3 | 8, 9, 10, 11 |
| Ligne 4 | 12, 13, 14, 15 |

## Multiplexeurs / drivers facade

La reference exacte des multiplexeurs manque encore. La base firmware utilise donc des pins `-1` tant que le composant n'est pas confirme.

| Usage | Minimum logique | Option propre |
| --- | --- | --- |
| Scan boutons | 4 colonnes sorties + 4 lignes entrees | Mux 16 voies ou GPIO expander |
| LEDs RGB | 4 colonnes cathodes + 12 lignes couleurs | Driver LED ou transistors recommandes |
| LEDs monochrome v0 | 4 colonnes + 4 lignes couleur rouge | Plus simple pour premier test |

Attention: un multiplexeur analogique type CD74HC4067 peut aider a lire/selectionner, mais il n'est pas forcement adapte pour fournir le courant des LEDs. Pour les LEDs RGB, prevoir drivers/transistors si la luminosite ou le courant deviennent serieux.

### Pins renseignes dans le firmware ESP32 (bring-up DevKitC-1)

Fichier: `src_esp32/az2_control/main.cpp`. UART Teensy fixee (voir plus haut).
Mux boutons/LED fixes sur un CD74HC4067 par fonction, adresses S0-S3
partagees entre les deux (memes GPIO), car bouton et LED d'un pad_id sont
toujours adresses ensemble:

```cpp
constexpr int kTeensyTxPin = 17;
constexpr int kTeensyRxPin = 18;

constexpr int kButtonMuxS0 = 4;
constexpr int kButtonMuxS1 = 5;
constexpr int kButtonMuxS2 = 6;
constexpr int kButtonMuxS3 = 7;
constexpr int kButtonMuxSignal = 8;

constexpr int kLedMuxS0 = kButtonMuxS0;  // memes GPIO que le mux boutons
constexpr int kLedMuxS1 = kButtonMuxS1;
constexpr int kLedMuxS2 = kButtonMuxS2;
constexpr int kLedMuxS3 = kButtonMuxS3;
constexpr int kLedMuxSignal = 9;
```

Ces pins sont valides pour un ESP32-S3-DevKitC-1 nu. A reverifier avant de
souder sur le module ecran final (voir note plus haut).

## Encodeurs rotatifs 1-4

EC11 classiques (2 voies quadrature A/B + bouton poussoir integre), tout en
`INPUT_PULLUP` cote ESP32 (l'autre patte de chaque contact va au GND commun).
Cablage v0 sur ESP32-S3-DevKitC-1 nu, valide au flash (voir plus bas) :

| Encodeur | A | B | Bouton |
| --- | --- | --- | --- |
| 1 | GPIO10 | GPIO11 | GPIO38 |
| 2 | GPIO12 | GPIO13 | GPIO39 |
| 3 | GPIO14 | GPIO15 | GPIO40 |
| 4 | GPIO16 | GPIO21 | GPIO41 |

Protocole envoye vers le Teensy: `MACRO:<1-4>:+N` / `MACRO:<1-4>:-N` par cran
(4 transitions quadrature = 1 cran), `ENC:<1-4>:BTN:DOWN`/`UP` pour le clic.
Tant qu'aucun encodeur n'est cable sur une broche, un etat flottant peut
declencher un evenement bouton isole au boot: normal, pas un defaut.

## SD de l'ecran

La carte TF/microSD du module ESP32 doit servir au stockage local.

| Dossier | Usage |
| --- | --- |
| `/az2/config` | Config machine, UI, Wi-Fi |
| `/az2/projects` | Projets groovebox |
| `/az2/presets` | Presets synth/mixer |
| `/az2/samples` | Samples courts |
| `/az2/logs` | Logs debug |
| `/retro-go` | Donnees compatibles Retro-Go si mode retro |
| `/roms/gb`, `/roms/gbc` | ROMs legales/personnelles |

Le pinout TF exact du module ecran reste a confirmer. Le firmware ESP32 contient deja les hooks SD, desactives tant que les pins sont a `-1`.

## Wi-Fi ESP32

Le Wi-Fi est une force de l'ESP32, mais il reste coupe au boot en v0 pour stabilite.

| Mode | Usage |
| --- | --- |
| OFF | Performance musicale |
| AP local | Configuration depuis telephone/PC |
| STA | Connexion atelier/maison |
| File manager | Envoyer presets, samples, captures |
| Debug | Lire logs et etat machine |

## Mode Game Boy / Retro-Go

Retro-Go est present dans `src_esp32/retro-go-master` et supporte Game Boy/Game Boy Color. Pour AZ-2, ce sera un mode bonus, pas la base du firmware.

Decision v0:

- garder Retro-Go comme reference;
- ne pas le melanger au firmware LVGL principal;
- creer plus tard un target `az2-esp32-4848s040` si on veut un vrai mode Game Boy;
- verifier avant tout le driver ST7701 et la SD.

## Tests de cablage v0

1. Flasher `master_teensy`. **Fait** (2026-09-13, compile OK apres correction
   `AudioSynthWaveformSine` sans `.begin()`).
2. Tester le PCM5102A seul avec `PLAY` / `STOP`. **A faire** des que le
   Teensy est branche.
3. Flasher `ui_esp`. **Fait**, sur ESP32-S3-DevKitC-1 nu.
4. Verifier le boot serie ESP32: Wi-Fi OFF, SD skip si pins non renseignees.
   **Fait**, boot serie conforme.
5. Renseigner UART ESP32/Teensy. **Fait** (GPIO17/18, voir plus haut) — cote
   ESP32; branchement physique + ecoute `Serial1` cote Teensy a faire/tester
   ensuite.
6. Envoyer `HELLO:ESP32_CONTROL`, verifier `HELLO:TEENSY_AUDIO`. **A faire**
   une fois l'UART cablee physiquement entre les deux cartes.
7. Renseigner multiplexeurs boutons. **Fait** (CD74HC4067, GPIO4-7 adresse +
   GPIO8 signal).
8. Tester `PAD:00` a `PAD:15`. **A faire** des que la matrice SparkFun et le
   CD74HC4067 boutons sont cables.
9. Renseigner pilotage LEDs. **Fait** (meme bus d'adresse que les boutons,
   GPIO9 signal, une LED a la fois — v0 mono, voir plus bas).
10. Tester un mode LED rouge simple avant RGB complet. **A faire** avec le
    CD74HC4067 LED cable.
11. Renseigner et tester les 4 encodeurs rotatifs. **Fait**, boot OK avec
    `AZ2:FEATURE:ENCODERS_4X`; test reel des crans/bouton a faire une fois
    les EC11 cables (voir "Encodeurs rotatifs 1-4" plus haut).

## Commandes PlatformIO

```bash
pio run -e ui_esp
pio run -e master_teensy
```

Le `Pico` reste dans `platformio.ini`, mais hors `default_envs`.
