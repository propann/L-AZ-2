# teensy_loader_cli (source verifie, recompile a la volee)

`teensy_loader_cli.c` est le code source officiel de PJRC
(https://github.com/PaulStoffregen/teensy_loader_cli, licence GPL-3.0,
Copyright PJRC.COM LLC), recupere le 2026-09-15.

## Pourquoi il est ici

Le binaire `teensy_loader_cli` fourni par le paquet PlatformIO
`tool-teensy` (version affichee : "2.2") a un vrai bug de lecture de
fichier `.hex` sur les GROS firmwares Teensy 4.1 (programme > ~1 Mo,
notre cas des qu'on a ajoute `#include <SD.h>` cote Teensy pour le
sampleur) : `error reading intel hex file ... Warning, HEX parse error
line 65556`, alors que le fichier `.hex` est en realite **valide**
(verifie octet par octet, tous les enregistrements Intel HEX et leurs
checksums corrects -- voir la discussion dans le commit qui a introduit
ce dossier). Cause identifiee en lisant le vrai code source de
`parse_hex_line()` : la logique specifique Teensy 4.x qui ramene les
adresses flash `0x60xxxxxx` dans les bornes du tampon interne
(`extended_addr -= 0x60000000`) est absente/differente dans le binaire
2.2 bundle -- la version officielle courante ("2.3", source ci-dessus)
corrige ca. Recompiler depuis la source officielle **resout le probleme
sans toucher au binaire systeme** de PlatformIO (`~/.platformio/...`,
hors du depot, pas modifie).

## Comment c'est utilise

`extra_scripts` de l'environnement `master_teensy` (voir
`platformio.ini`) charge `../../tools/build_teensy_loader.py`, qui
compile ce fichier UNE fois (`gcc -O2 -DUSE_LIBUSB -o teensy_loader_cli
teensy_loader_cli.c -lusb`, meme flags que le Makefile officiel pour
Linux) dans `.pio/local_tools/` (pas commite, recompile si absent) et
redirige l'upload Teensy vers ce binaire frais au lieu du binaire
PlatformIO bugge.

Requiert `gcc` et la lib `libusb-0.1` (`libusb-compat` sur Arch, meme
dependance que le binaire PlatformIO d'origine -- rien de plus a
installer si l'upload Teensy fonctionnait deja avant).
