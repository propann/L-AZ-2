# AZ-2 - ESP32 controle, Wi-Fi, SD et mode retro

Objectif: utiliser au mieux l'ESP32-S3 de l'ecran. Il ne sert pas seulement a afficher: il devient la tete de controle de la groovebox.

## Capacites ESP32 retenues

| Ressource | Usage AZ-2 |
| --- | --- |
| ESP32-S3 double coeur | UI, scan matrice, stockage, Wi-Fi, outils |
| Ecran 480x480 ST7701 | Interface principale de la groovebox |
| Tactile capacitif | Menus, editions lentes, selection de fichiers |
| PSRAM 8 MB | Buffers UI, assets, eventuellement emulateurs |
| Flash 16 MB | Firmware UI plus large, partitions plus confortables |
| TF / microSD | Presets, projets, assets, captures, ROMs legales, config Wi-Fi |
| Wi-Fi | File manager, sync, debug, updates, remote control |
| Bluetooth | Reserve MIDI BLE ou controle plus tard |

## Regle de priorite

La groovebox reste prioritaire. Les fonctions Wi-Fi, SD et retro sont utiles, mais ne doivent jamais bloquer le son du Teensy.

| Priorite | Fonction |
| --- | --- |
| 1 | Scan pads + commandes vers Teensy |
| 2 | UI performance, mixer, sequenceur |
| 3 | SD: sauvegarde/chargement local |
| 4 | Wi-Fi: file manager, debug, sync |
| 5 | Mode retro/Game Boy pour bonus |

## SD: arborescence proposee

La carte SD de l'ecran doit servir de stockage local principal cote ESP32.

```text
/az2/
  config/
    wifi.json
    hardware.json
    ui.json
  projects/
  presets/
  samples/
  screenshots/
  logs/
  firmware/
/retro-go/
  bios/
  saves/
  states/
/roms/
  gb/
  gbc/
/romart/
  gb/
  gbc/
```

Notes:

- `/az2` = donnees groovebox.
- `/retro-go` = donnees compatibles avec Retro-Go si on porte le mode retro.
- `/roms/gb` et `/roms/gbc` = uniquement des ROMs legales/personnelles.
- `wifi.json` ne doit jamais etre commite dans Git.

## Wi-Fi

Le Wi-Fi est prevu, mais il doit rester desactive au boot par defaut pendant les premiers tests hardware.

Usages cibles:

| Mode | Usage |
| --- | --- |
| OFF | Mode performance, priorite stabilite |
| AP local | Configurer AZ-2 depuis un telephone/PC |
| STA | Rejoindre le reseau maison/atelier |
| File manager | Envoyer presets, samples, ROMs legales, captures |
| Debug | Voir logs et etat machine sans ouvrir le boitier |

Retro-Go utilise un fichier `/retro-go/config/wifi.json` avec plusieurs reseaux. Pour AZ-2, on gardera plutot `/az2/config/wifi.json`, puis on pourra generer une copie compatible Retro-Go si besoin.

## Game Boy / Retro-Go

Le repo contient deja `src_esp32/retro-go-master`. Retro-Go supporte notamment:

- Game Boy;
- Game Boy Color;
- NES;
- Game & Watch;
- Master System / Game Gear;
- Mega Drive / Genesis;
- PC Engine;
- Lynx;
- DOOM.

Pour AZ-2, la piste utile est Game Boy / Game Boy Color, pour un mode bonus propre et leger.

### Attention technique

Retro-Go n'est pas une simple librairie Arduino. C'est un firmware ESP-IDF complet avec:

- targets materiels dans `components/retro-go/targets/`;
- drivers ecran propres;
- gestion SD/FATFS;
- gestion Wi-Fi;
- launcher;
- apps/emulateurs.

Notre firmware principal ESP32 est en Arduino/PlatformIO avec LVGL. Donc il y a deux pistes:

| Piste | Avantage | Risque |
| --- | --- | --- |
| Mode AZ-2 principal avec quelques idees Retro-Go | Simple, garde la groovebox stable | Pas de vrai emulateur au debut |
| Port Retro-Go cible `az2-esp32-4848s040` | Vrai Game Boy/GBC | Travail driver ST7701 + input + SD + build ESP-IDF |
| Dual firmware / partition outil | Separation propre | Complexite de flash et maintenance |

Decision v0: ne pas melanger Retro-Go dans le firmware principal. On documente, on garde le code dans le repo, puis on creera un target dedie si la groovebox est stable.

## Infos Retro-Go utiles deja trouvees

| Sujet | Info |
| --- | --- |
| PSRAM | Requise/recommandee pour les ports ESP32 |
| Game Boy | Supporte par Retro-Go via `gnuboy` |
| Wi-Fi | File manager accessible par IP |
| BIOS GB | `/retro-go/bios/gb_bios.bin` |
| BIOS GBC | `/retro-go/bios/gbc_bios.bin` |
| Covers | `/romart/<system>/<game>.png` ou CRC32 |
| Portage | Creer un target dans `components/retro-go/targets/` |
| Ecran | Retro-Go supporte surtout ILI9341/ST7789 par defaut; ST7701 demandera un driver/adaptation |

## Firmware ESP32 v0

Fichier: `src_esp32/az2_control/main.cpp`.

La base actuelle:

- coupe le Wi-Fi au boot pour stabilite;
- prepare le montage SD, desactive tant que les pins ne sont pas renseignees;
- prepare scan matrice 4x4 via multiplexeurs;
- envoie les commandes au Teensy;
- annonce les features au port serie.

Pins encore a renseigner:

| Bloc | Pins |
| --- | --- |
| UART ESP32 -> Teensy | RX, TX |
| Mux boutons | S0, S1, S2, S3, SIG |
| Mux LEDs | S0, S1, S2, S3, SIG |
| SD SPI si non cablage interne | SCK, MISO, MOSI, CS |

## Prochaines recherches utiles

1. Trouver le pinout complet du module `ESP32-4848S040C_I`, surtout TF/microSD.
2. Identifier les multiplexeurs exacts disponibles.
3. Verifier si la matrice SparkFun 4x4 LED demande un driver de courant.
4. Tester le PCM5102A avec le Teensy seul.
5. Creer un target Retro-Go `az2-esp32-4848s040` seulement apres validation de l'ecran ST7701.
