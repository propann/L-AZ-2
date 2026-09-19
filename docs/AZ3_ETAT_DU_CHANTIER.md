# AZ-3 — État du chantier

**Branche :** `az3` · **Dernière mise à jour :** 19 septembre 2026

Point d'entrée unique de la branche AZ-3. À lire en premier en reprenant le
projet.

---

## Ce qu'est l'AZ-3

L'AZ-2 est une groovebox à **deux** cerveaux, dont le boîtier est plein
(périmètre gelé le 2026-09-18). L'AZ-3 passe à **trois**, avec des rôles nets :

| Carte | Rôle | Sources |
|---|---|---|
| **ESP32-S3** | écran 480×480, Wi-Fi, carte SD, projets, émulateur GB/GBC | `src_esp32/az2_screen/` |
| **Teensy 4.1** | moteurs audio, séquenceur, DAC PCM5102A, **rack AZ-BUS** | `src_teensy/az2_audio/` |
| **Pico RP2040** | **toute la façade** : matrice, encodeurs, boutons, LED | `src_pico/` |

Deux changements de fond par rapport à l'AZ-2 :

1. **Le Teensy n'a plus aucune commande.** Ses 17 broches de croix, boutons
   A/B/C/D et encodeurs (GPIO 2-6, 8, 9, 14-19, 22-25) sont libérées — et
   c'est le but : le rack de moteurs en a besoin.
2. **Un rack physique de moteurs audio** (ESP32, Teensy, RP2350…) se branche
   sur le Teensy maître, chaque piste parlant à un « Engine Slot » plutôt
   qu'à un moteur précis.

---

## Où on en est

### ✅ Fait

| | |
|---|---|
| **Firmware du panneau Pico** | écrit, structuré en 4 fichiers, **compile** |
| **Teensy vidé de ses commandes** | 17 broches libérées, `Serial7` ouvert pour le Pico, handlers `NAV:`/`BTN:`/`POT:` entrants |
| **Plan de câblage** | 🔒 **figé** le 2026-09-19 — [AZ3_CABLAGE_PANNEAU.md](AZ3_CABLAGE_PANNEAU.md) |
| **Module LED choisi** | IS31FL3731 « 2946 », référence figée, **à commander en 2 exemplaires** |
| **Builds** | `ctrl_pico`, `master_teensy`, `screen_esp` SUCCESS · tests natifs 9/9 |

### ❌ Pas fait — et c'est là que ça reprend

| | |
|---|---|
| **Rien n'est soudé.** | Aucun composant du panneau n'est câblé. |
| **Le Pico n'a jamais été branché.** | Jamais détecté sur la machine de dev : aucun VID `2e8a`, aucun `/dev/ttyACM*`, aucun volume `RPI-RP2`. |
| **Le module LED n'est pas commandé.** | Voir [§9.1 de la feuille de câblage](AZ3_CABLAGE_PANNEAU.md). |
| **Le rack AZ-BUS** | reste à l'état d'étude — [AZ2_BUS_RACK_MOTEURS.md](AZ2_BUS_RACK_MOTEURS.md). |

> **Aucune ligne de cette branche n'a tourné sur du matériel réel.** Tout ce
> qui suit est du code qui compile et un plan sur papier. Le tableau détaillé
> de ce qui est confirmé (essentiellement : le scan de matrice et les
> encodeurs, validés le 2026-09-13 sur l'ancien firmware) est en
> [§12 de la feuille de câblage](AZ3_CABLAGE_PANNEAU.md).

---

## Les trois décisions qui structurent tout

### 1. Le protocole ne change pas, donc l'écran non plus

La croix et les boutons A/B/C/D disparaissent en matériel. Leurs messages
`NAV:` et `BTN:` **continuent d'exister** : le Pico les synthétise depuis les
encodeurs, et depuis les pads en mode manette.

L'UI de l'ESP32 — 4680 lignes — consomme déjà ce vocabulaire. **Elle n'a eu
besoin d'aucune modification.** C'est de loin ce qui a économisé le plus de
travail.

### 2. Chaque composant sur ce pour quoi il est fait

> Un CD74HC4067 relie **un** canal à la fois à un seul fil.

Boutons et lignes de matrice : oui, ce sont des entrées lentes. Quadrature :
non, A et B doivent être lus au même instant. LED : non — 2 % de rapport
cyclique et aucune régulation de courant.

C'est le fond de l'échec de 2026-09. La broche EN laissée en l'air n'en était
que la cause immédiate ; le mux était de toute façon le mauvais composant pour
piloter des LED. D'où le passage à un driver I2C qui balaie en matériel.

### 3. Le budget de broches est le vrai cadre

Le RP2040 a 26 broches utilisables, **toutes affectées**. La quadrature coûte
2 vraies GPIO par encodeur : c'est ce qui plafonne la façade à 6 encodeurs. Le
6ᵉ n'existe que parce que les lignes de matrice ont été déportées sur un mux —
4 GPIO libérées pour 1 broche de SIG.

---

## Reprendre le chantier

### Étape 0 — brancher le Pico

Rien ne peut être vérifié tant qu'il n'apparaît pas à l'ordinateur. Cause la
plus fréquente : **câble micro-USB charge seule**, sans les paires de données.
Contrôle : `lsusb | grep 2e8a`, ou l'apparition d'un volume `RPI-RP2` en
maintenant BOOTSEL au branchement.

### Étape 1 — flasher et faire tourner le Pico seul

```
pio run -e ctrl_pico -t upload
```

Sans outil : BOOTSEL enfoncé au branchement, puis glisser
`.pio/build/ctrl_pico/firmware.uf2` sur le volume qui apparaît.

Au moniteur série (**921600 bauds**), la commande `SELFTEST` enchaîne identité,
scan I2C, mux, encodeurs et pads en un seul compte rendu. Les diagnostics
détaillés (`LEDSCAN`, `MUXTEST`, `ENCTEST`, `PADTEST`, `LEDTEST`) fonctionnent
**sans Teensy branché** — chacun nomme la cause probable quand il ne trouve
rien.

### Étape 2 — souder, bloc par bloc

Suivre [AZ3_CABLAGE_PANNEAU.md](AZ3_CABLAGE_PANNEAU.md) §10, dans l'ordre. Les
trois vérifications au multimètre **avant** de mettre sous tension :

1. **Continuité EN → GND** sur chaque CD74HC4067 — la vérification qui aurait
   évité l'abandon de 2026-09 ;
2. pas de court-circuit 3,3 V / GND ;
3. **boutons et LED bien séparés** sur la carte SparkFun (le guide le dit,
   mais signale que la sérigraphie prête à confusion).

### Étape 3 — le module LED, à la réception

Une seule chose à trancher sur la datasheet : **de quel côté sont les anodes**.
Le pad demande 12 anodes × 4 cathodes, le driver est une matrice 16 × 9.

- anodes côté **16** → un module suffit → `kAnodeSide = AnodeSide::C16`
- anodes côté **9** → il en faut deux → `kAnodeSide = AnodeSide::A9`

Une constante dans `src_pico/az3_led_driver.h`. **Les deux cas compilent
déjà.**

### Étape 4 — la chaîne complète

Câbler GPIO 0/1 → Teensy pins 28/29 **+ la masse commune**, flasher le Teensy,
vérifier `HELLO:TEENSY_AUDIO` en retour. Puis tourner l'encodeur 3 : le volume
doit changer à l'oreille *et* à l'écran.

---

## Où trouver quoi

| Document | Contenu |
|---|---|
| [AZ3_CABLAGE_PANNEAU.md](AZ3_CABLAGE_PANNEAU.md) | 🔒 **le plan de câblage figé** — brochage, mux, LED, nomenclature, mise en service, dépannage |
| [AZ3_PANNEAU_PICO.md](AZ3_PANNEAU_PICO.md) | conception du firmware : structure, décisions logicielles, protocole, diagnostics |
| [AZ2_BUS_RACK_MOTEURS.md](AZ2_BUS_RACK_MOTEURS.md) | le rack de moteurs, qui récupère les 17 broches libérées |
| [AZ2_ETAT_DES_LIEUX.md](AZ2_ETAT_DES_LIEUX.md) | journal au fil de l'eau, entrées les plus récentes en haut |
| [AZ2_CABLAGE_PICO.md](AZ2_CABLAGE_PICO.md) | plan abandonné de 2026-09 — conservé pour le journal de la panne LED |

| Source | Contenu |
|---|---|
| `src_pico/az3_panel_config.h` | **tout le brochage** et les rôles — aucune logique |
| `src_pico/az3_panel_io.h` | accès matériel — ni protocole ni rôles |
| `src_pico/az3_led_driver.h` | pilote I2C IS31FL3731 |
| `src_pico/main.cpp` | logique, protocole, modes, diagnostics — aucun numéro de broche |

Changer un fil ne touche qu'à `az3_panel_config.h` **et** à la feuille de
câblage, en même temps.

---

## Ce qui reste ouvert

- **Le rack AZ-BUS** n'est qu'une étude. Le vrai premier jalon est le rack
  *logiciel* : envelopper les 6 moteurs existants du Teensy derrière une
  interface `Engine` commune, faisable sans aucun matériel.
- **Le mode Jeux perd sa croix physique.** Les pads la remplacent
  (`kGamepadMap`, mode `PANEL:MODE:GAME`), mais l'ESP32 n'envoie pas encore
  cette bascule — à câbler côté écran quand une partie démarre.
- **8 encodeurs** demanderaient 2× 74HC165 (~0,60 €), qui capturent 16 entrées
  d'un coup. Ni les GPIO restantes ni un mux ne savent le faire.
- **`SHIFT` est lu mais inutilisé** — le bouton existe, les rôles secondaires
  des encodeurs restent à définir.
