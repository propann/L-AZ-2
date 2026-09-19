# AZ-3 — Firmware du panneau Pico : conception

**Version :** V2, 19 septembre 2026
**Branche :** `az3` · **Environnement :** `ctrl_pico`
**Statut :** compile, **rien de vérifié sur le vrai matériel**.

Le **câblage** est dans [AZ3_CABLAGE_PANNEAU.md](AZ3_CABLAGE_PANNEAU.md). Ce
document-ci ne traite que des choix logiciels.

---

## 1. Structure

Quatre fichiers, une responsabilité chacun :

| Fichier | Contient | Ne contient pas |
|---|---|---|
| `az3_panel_config.h` | **tout le brochage**, les rôles, les temporisations | aucune logique |
| `az3_panel_io.h` | lecture des contacts et des broches de quadrature | ni protocole, ni rôles |
| `az3_led_driver.h` | pilote I2C IS31FL3731, image locale | rien du panneau |
| `main.cpp` | logique, protocole, modes, diagnostics | aucun numéro de broche |

Changer un fil ne touche qu'à `az3_panel_config.h`. Changer de puce LED ne
touche qu'à `az3_led_driver.h`.

---

## 2. Le choix qui a évité de réécrire l'interface

La croix et les boutons A/B/C/D n'existent plus en matériel. Leurs messages
`NAV:` et `BTN:` **continuent d'exister** : le Pico les synthétise depuis les
encodeurs et, en mode manette, depuis les pads.

L'UI de l'ESP32 — 4680 lignes — consomme déjà ce vocabulaire pour naviguer
dans les menus, déplacer le curseur du tracker et piloter l'émulateur. En
gardant le protocole identique, **elle n'a eu besoin d'aucune modification**.

C'est de loin la décision qui a économisé le plus de travail sur ce chantier.

---

## 3. Décisions logicielles

### 3.1 La quadrature est sondée souvent, pas une fois par tour

Un scan complet des pads prend ~450 µs (4 colonnes × 4 lignes, chacune avec
son temps d'établissement de mux), celui des boutons ~130 µs. Lire les
encodeurs une seule fois par tour de boucle laisserait passer des crans sur
une rotation rapide.

`pollEncoders()` est donc appelé **entre chaque bloc** : avant les pads, entre
pads et boutons, après les boutons. Il ne fait que de l'arithmétique — aucune
émission série — et dépose les crans dans un compteur.

**Le défaut existait dans le firmware Pico d'origine**, où il passait
inaperçu : presque aucune LED ne s'allumait, donc la boucle était rapide.

### 3.2 L'émission série est séparée de l'échantillonnage

`drainEncoders()` traduit les crans en messages **après** tous les scans,
jamais au milieu. Envoyer sur l'UART pendant un scan allongerait le créneau en
cours de façon imprévisible.

### 3.3 Pourquoi `attachInterrupt()` n'est pas utilisé

Théoriquement plus propre, mais sur le cœur Arduino-mbed `attachInterrupt`
crée un objet `InterruptIn` qui prend la main sur la broche, et faire un
`digitalRead` de cette même broche depuis l'ISR demande une validation sur
matériel réel. Le sondage réutilise exactement le chemin déjà confirmé
fonctionnel le 2026-09-13. **Si la quadrature s'avère insuffisante une fois
tout câblé, l'interruption est le plan B.**

### 3.4 Une seule colonne pilotée à la fois, les autres en haute impédance

Jamais à HIGH. Si deux touches d'une même ligne sont enfoncées, deux colonnes
se retrouvent reliées entre elles : en haute impédance ça ne fait rien, mais
une sortie à HIGH face à une sortie à LOW est un court-circuit franc entre
deux GPIO.

### 3.5 Les LED n'ont plus aucune contrainte temps réel

Le balayage est fait **par le matériel** du IS31FL3731. Le firmware écrit une
image locale de 48 octets et ne l'envoie que lorsqu'elle a changé, au plus à
50 Hz. Rater une échéance ne fait plus scintiller quoi que ce soit — c'était
toute la fragilité du POV logiciel précédent.

Le pilote est écrit à la main plutôt que d'utiliser la bibliothèque Adafruit,
qui tire `Adafruit_GFX` et `BusIO` derrière elle pour une poignée de
registres. L'audit du 2026-09-17 avait retiré `lvgl` du projet pour la même
raison.

### 3.6 Le cœur mbed n'a pas `Wire.setSDA()`

Les broches I2C se donnent au **constructeur** : le pilote déclare sa propre
instance `arduino::MbedI2C bus(kI2cSdaPin, kI2cSclPin)` au lieu d'utiliser le
`Wire` global, qui serait câblé sur les broches par défaut.

### 3.7 Un pad pressé s'allume immédiatement, sans Teensy

L'appui physique prime sur l'état musical venant du Teensy. C'est le test de
câblage le plus rapide qui soit : brancher le Pico seul, appuyer, voir.

---

## 4. Protocole

### Émis par le Pico

| Message | Source |
|---|---|
| `PAD:NN:DOWN/UP` · `PAD:NN:HOLD` | pads, mode musique |
| `NAV:<DIR>:DOWN/UP` | encodeurs 1-2 (impulsion) ou pads (mode manette, maintenu) |
| `BTN:<A-D>:DOWN/UP` | clics d'encodeur 1-4, ou pads en mode manette |
| `ENC:<n>:DOWN/UP` | clics d'encodeur 5-6, ou SELECT/START en mode manette |
| `POT:<0-2>:<0-127>` | encodeurs 3-5, valeur absolue, débit limité |
| `MACRO:<n>:±N` | encodeur 6, delta brut |
| `PLAY` · `STOP` · `REC:START` · `REC:STOP` | boutons de transport |
| `HELLO:PICO_KEYPAD` | au démarrage |

### Reçu par le Pico

| Message | Effet |
|---|---|
| `LED:NN:ON` / `LED:NN:OFF` | forme historique — `ON` allume les trois couleurs (blanc) |
| `LED:NN:C:<0-7>` | forme étendue — un bit par couleur (1 rouge, 2 vert, 4 bleu) |
| `PANEL:MODE:GAME` / `PANEL:MODE:MUSIC` | bascule le rôle des pads |

Les deux formes de `LED:` coexistent : le Teensy actuel n'émet que la
première, et n'a donc pas besoin de changer.

---

## 5. Diagnostics

Tous pilotables au moniteur série USB (921600 bauds), **sans Teensy branché**.
C'est délibérément fourni : la panne LED de 2026-09 a coûté le projet faute de
pouvoir isoler le bloc fautif.

| Commande | Rôle |
|---|---|
| `SELFTEST` | enchaîne identité, scan I2C, mux, encodeurs, pads |
| `LEDSCAN` | quelles adresses I2C répondent |
| `MUXTEST` | les 16 canaux du mux boutons |
| `ENCTEST` | A/B bruts + compteur de crans, sans passer par les rôles |
| `PADTEST` | état de la matrice, ligne par ligne |
| `LEDTEST` / `LEDTEST:STOP` | les 48 LED une par une, ~0,3 s chacune |
| `VERSION` | identité et fonctions |

Chacune nomme la cause probable quand le résultat est vide — `MUXTEST` qui
renvoie 16 zéros figés pointe la broche EN, `LEDSCAN` sans réponse pointe
l'alimentation ou la broche AD.

---

## 6. Étendre

| Envie | Où |
|---|---|
| Changer le rôle d'un encodeur | table `kEncoders`, une ligne |
| Ajouter un bouton de façade | un canal libre du mux + un `case` dans `onButtonEdge()` |
| Changer le mapping manette | table `kGamepadMap` |
| Changer un fil | `az3_panel_config.h`, nulle part ailleurs |
| Changer de puce LED | `az3_led_driver.h`, l'interface ne bouge pas |
| Passer à 8 encodeurs | 2× 74HC165 (~0,60 €) — ils capturent 16 entrées d'un coup, ce que ni les GPIO restantes ni un mux ne savent faire |

---

## 7. Limites assumées

- **Le mode Jeux n'a plus de croix physique.** Les pads la remplacent
  (`kGamepadMap`), mais le confort d'une vraie croix est perdu.
- **6 encodeurs est un plafond dur** sur RP2040 : la quadrature coûte 2 GPIO
  chacun et il n'en reste aucune.
- **La géométrie du driver LED n'est pas tranchée** — il faut lire la
  datasheet pour savoir de quel côté sont les anodes. Les deux cas compilent
  (`kAnodeSide`), voir [AZ3_CABLAGE_PANNEAU.md §5.1](AZ3_CABLAGE_PANNEAU.md).
- **Rien n'est vérifié en réel.** Le Pico n'a jamais été détecté sur la
  machine de développement : aucun VID `2e8a`, aucun `/dev/ttyACM*`, aucun
  volume `RPI-RP2`, sur toute la durée de la session.
