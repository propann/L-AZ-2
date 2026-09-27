# AZ-2 — Préparation de la carte SD ROM

Mise à jour : 27 septembre 2026.

## Format attendu

La carte destinée à l’ESP32-S3 doit avoir :

- une table de partitions MBR (`msdos`) ;
- une seule partition ;
- une partition FAT32 ;
- aucun volume `ext4`, `exFAT` ou découpage `bootfs/rootfs` Raspberry Pi.

Structure cible :

```text
/games/
  GameBoy/
  GameBoyColor/
  NES/
  NeoGeoPocket/
/nes/
/projects/
/kits/
```

Le firmware parcourt maintenant récursivement `/games/`. Les ROMs peuvent
donc être rangées par console dans des sous-dossiers :

```text
/games/GameBoy/tetris.gb
/games/GameBoy/zelda.gb
/games/GameBoyColor/jeu_gbc.gbc
/games/GameBoy/zelda.sav       # sauvegarde à côté de la ROM
/games/NES/jeu.nes
/projects/0.proj       # jusqu'à 3.proj
/kits/0.kit            # jusqu'à 3.kit, créés par l'interface
```

Les extensions `.gb`, `.gbc` et `.nes` doivent rester en minuscules pour
faciliter la détection. Le code NGP cherche aussi `.ngp`, `.NGP`, `.ngc` et
`.NGC` sous `/games/`. L’intégration est encore en qualification et ne doit
pas être annoncée comme un émulateur de production validé.

Les WAV et samples ne vont pas sur cette carte : ils appartiennent à la carte
SD du Teensy, dans `/samples/`.

## Tri des sources disponibles

- GB : Tetris, Super Mario Land, Zelda français et LSDJ.
- GBC : archive de traductions et ROMs GBC ; privilégier Europe/France puis
  World/USA lorsque la version française n’existe pas.
- NES : archive `.nes` ; exclure les entrées Pirate, doublons et hacks
  multiples ; privilégier Europe/France, World puis USA.
- Neo Geo Pocket : collection NGPC ; conserver les ROMs dans `/games/` et
  valider séparément le chargement, l’audio et les sauvegardes.

Les archives originales de `Downloads/` ne doivent pas être supprimées.

## Diagnostic du lecteur SD du MacBook Pro

Le lecteur interne est un Broadcom BCM57765/57785 (`14e4:16bc`) piloté par
`sdhci-pci`. Avec le noyau courant, le mode UHS/ADMA produit des erreurs :

- `ADMA Err: 0x00000001` ;
- `Timeout waiting for hardware interrupt` ;
- `Card removed during transfer` ;
- changements de capacité et remount FAT en lecture seule.

Le correctif temporaire testé est :

```text
sdhci.debug_quirks2=4
sdhci.debug_quirks=64
```

Il désactive le mode UHS problématique et force le chemin PIO en désactivant
ADMA. Rechargement temporaire :

```bash
sudo modprobe -r sdhci_pci sdhci
sudo modprobe sdhci debug_quirks=64 debug_quirks2=4
sudo modprobe sdhci_pci
```

Le correctif a stabilisé la détection et permis le formatage, mais les copies
de fichiers ROM restent à valider : certains transferts provoquent encore un
remount en lecture seule. Ne pas annoncer la carte comme prête tant qu’une
copie complète suivie d’une relecture n’a pas réussi.

## Critère de validation

Avant de flasher ou d’utiliser la carte dans l’AZ-2 :

1. monter la partition FAT32 en écriture ;
2. créer `/games` puis les sous-dossiers `GameBoy`, `GameBoyColor` et `NES`,
   ainsi que `/projects` et `/kits` ;
3. copier un petit jeu GB, un GBC et une ROM NES ;
4. démonter proprement ;
5. remonter et relire les fichiers ;
6. vérifier leur taille et leur empreinte ;
7. seulement ensuite copier la collection complète.
