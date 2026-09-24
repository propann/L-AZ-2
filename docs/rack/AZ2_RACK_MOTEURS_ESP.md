# AZ-2 — Rack de moteurs ESP

> **Statut : ROADMAP / architecture cible.** Ce document sépare volontairement le prototype physique actuel des extensions futures. Rien dans la partie ROADMAP ne doit être présenté comme déjà validé.

## Philosophie

**AZ-2 n'est pas seulement un instrument open source : c'est une base pour inventer le vôtre.**

Le projet privilégie d'abord des cartes de développement, un câblage compréhensible et un boîtier bricolable. La miniaturisation viendra après la validation électrique, audio et logicielle.

**Build it. Modify it. Measure it. Share it.**

## Prototype physique actuel

Le prototype utilise actuellement **deux ESP de développement** :

- un **ESP32-WROOM-32 DevKit** ;
- un **ESP32-S3** ;
- le **Teensy 4.1** reste le cœur tracker/audio de l'architecture actuelle ;
- chaîne audio avec DAC, amplification et **sortie casque** ;
- boîtier construit à partir de **LEGO**, dont certaines pièces sont adaptées à la Dremel.

Ce choix est volontaire : les modules sont faciles à trouver, flasher, câbler, remplacer et reproduire par d'autres makers. Le format compact définitif n'est pas encore une priorité.

## Extension rack

La cible à long terme est un rack pouvant aller jusqu'à **quatre emplacements ESP**, avec **un gros moteur principal par module**.

**Important : deux modules seulement sont prévus pour la première phase physique.** Les emplacements 3 et 4 ne seront ajoutés qu'après validation de la machine en charge.

```text
                         AZ-2
                  ┌─────────────────┐
                  │ ESP32-S3 / UI   │
                  │ écran · SD · GB │
                  └────────┬────────┘
                           │ contrôle
                  ┌────────▼────────┐
                  │   TEENSY 4.1    │
                  │ tracker · audio │
                  └────────┬────────┘
                           │ interface rack à définir
                ┌──────────┴──────────┐
                │                     │
           ┌────▼────┐           ┌────▼────┐
           │ SLOT 1  │           │ SLOT 2  │
           │ ESP     │           │ ESP     │
           │ 1 moteur│           │ 1 moteur│
           └─────────┘           └─────────┘

              PHASE SUIVANTE, SI VALIDÉE
           ┌─────────┐           ┌─────────┐
           │ SLOT 3  │           │ SLOT 4  │
           │ option  │           │ option  │
           └─────────┘           └─────────┘
```

Le bus, le brochage, le transport audio et le mécanisme de flash ne sont **pas encore figés**.

## Validation avant passage de 2 à 4 modules

La décision d'ajouter les modules 3 et 4 doit reposer sur des mesures. Les essais devront charger simultanément l'écran, le tracker, les moteurs, le mixage, les effets et les fonctions annexes pertinentes.

À mesurer au minimum :

- charge CPU de chaque processeur et marge restante ;
- RAM/PSRAM/flash et fragmentation éventuelle ;
- stabilité et latence audio ;
- xruns, clics, décrochages et dérive temporelle ;
- débit et latence des liaisons inter-processeurs ;
- consommation moyenne et pointes au démarrage ;
- tension d'alimentation sous charge ;
- température après test prolongé ;
- bruit de fond, parasites numériques et comportement de la sortie casque ;
- démarrage, arrêt et récupération après erreur d'un module.

Le passage à quatre modules sera une **décision issue des tests**, pas une promesse de la documentation.

## Fiche standard d'un module moteur

Chaque moteur devra avoir une fiche reproductible dans `docs/rack/engines/` :

| Champ | Contenu |
| --- | --- |
| Identité | nom, version, rôle musical |
| MCU | ESP exact, mémoire, format du module |
| Firmware | environnement PlatformIO et version |
| Protocole | version et capacités annoncées |
| Audio | format, fréquence, canaux, latence mesurée |
| Contrôle | paramètres, notes et événements |
| UI | paramètres et représentation à l'écran |
| Ressources | CPU, RAM, flash mesurés |
| I/O | alimentation, contrôle et audio |
| Presets | format et stockage |
| Mise à jour | flash, vérification et récupération |
| Validation | tests matériels, limites et statut |

## Backplane : exigences avant schéma

Avant de figer un connecteur ou un PCB, documenter tension et courant par slot, courant de démarrage, masses, bruit, transport audio, bus de commande, adressage, boot/flash, détection de présence et comportement d'un module absent ou planté.

Le premier objectif n'est donc pas de dessiner immédiatement un backplane quatre slots : c'est de faire fonctionner et mesurer proprement **deux modules de développement**.

## Flash depuis l'écran

**ROADMAP — non livré.**

La cible est de pouvoir identifier un module, sélectionner un firmware présent sur SD, vérifier sa compatibilité, flasher le slot choisi, vérifier l'écriture puis redémarrer ou récupérer le module en cas d'échec.

```text
CONFIG
  └─ RACK
      ├─ SLOT 1 — moteur / version
      ├─ SLOT 2 — moteur / version
      ├─ SLOT 3 — option future
      ├─ SLOT 4 — option future
      └─ FIRMWARE
          ├─ fichier SD
          ├─ compatibilité
          ├─ flash
          ├─ vérification
          └─ récupération
```

Le mécanisme exact dépendra du module retenu et de son bootloader. Aucun binaire ne devra être envoyé aveuglément.

## MIDI

**ROADMAP.** Une prise **jack MIDI** est prévue plus tard. Le choix TRS, le brochage, l'isolation/interface électrique et le routage logiciel seront documentés lorsqu'ils seront arrêtés et testés.

## Contrôleurs expérimentaux

Ces idées sont volontairement ouvertes et ne font pas partie du hardware CURRENT :

1. **Matrice SparkFun 4×4 + Raspberry Pi Pico** : le Pico pourrait scanner les boutons, piloter les LED et transmettre les événements au Teensy. Il faudra valider la liaison, la latence, l'alimentation et le firmware.
2. **Clavier LMN-3 + Raspberry Pi Pico** : piste de réutilisation du clavier LMN-3 avec un Pico à la place du contrôleur d'origine, sous réserve de valider les E/S et la connexion au Teensy.
3. **Miniaturisation** : remplacer plus tard les DevKit par des modules plus petits seulement après stabilisation de l'architecture.

## Contributions matérielles

Les variantes sont encouragées. Une proposition communautaire peut concerner un contrôleur, un moteur, un module rack, un boîtier, une interface ou une autre façon de jouer.

Pour rester exploitable, une proposition devrait préciser : objectif, composants, schéma/câblage, firmware, connexion à AZ-2, alimentation, avantages, contraintes, état des tests et photos si un prototype existe.

Une idée communautaire reste **EXPERIMENTAL** tant qu'elle n'a pas été reproduite ou validée. Elle peut ensuite devenir une variante documentée sans remplacer automatiquement l'architecture de référence.

## Ordre de réalisation

1. Stabiliser le prototype actuel WROOM-32 + S3 + Teensy et documenter le câblage réel.
2. Documenter la chaîne DAC/ampli/casque.
3. Préparer deux modules moteur de développement.
4. Mesurer la machine en charge avec deux modules.
5. Stabiliser découverte, contrôle et transport audio.
6. Prototyper le flash sûr d'un module.
7. Décider, à partir des mesures, si les slots 3 et 4 sont pertinents.
8. Ajouter plus tard le jack MIDI.
9. Étudier seulement ensuite la miniaturisation et les variantes de contrôleurs.

---

Le châssis de référence donne un point de départ ; les interfaces documentées doivent permettre à la communauté de construire d'autres AZ-2 sans transformer les idées futures en fausses fonctions déjà livrées.
