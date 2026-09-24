# AZ-2 — Rack de moteurs ESP

> **Statut : ROADMAP / architecture cible.** Ce document décrit l'extension prévue du prototype. Le rack et le flash depuis l'écran ne doivent pas être présentés comme déjà livrés.

## Vision

AZ-2 doit pouvoir accueillir des **cartouches de calcul audio** : de petits modules ESP enfichables, chacun consacré à **un gros moteur**. L'objectif n'est pas d'empiler des moteurs dans le firmware principal, mais de donner à chaque moteur lourd son propre processeur, sa mémoire et son cycle de développement.

Le prototype actuel reste centré sur le couple **Teensy 4.1 + ESP32-S3 écran**. L'extension visée ajoute un **rack physique de quatre emplacements**.

```text
                         AZ-2
                  ┌─────────────────┐
                  │ ESP32-S3 ÉCRAN  │
                  │ UI · SD · GB    │
                  └────────┬────────┘
                           │ contrôle / gestion
                  ┌────────▼────────┐
                  │   TEENSY 4.1    │
                  │ tracker · audio │
                  │ mixer · DAC     │
                  └────────┬────────┘
                           │ bus rack
            ┌──────────────┼──────────────┐
            │              │              │
       ┌────▼────┐    ┌────▼────┐    ┌────▼────┐    ┌─────────┐
       │ SLOT 1  │    │ SLOT 2  │    │ SLOT 3  │    │ SLOT 4  │
       │ mini ESP│    │ mini ESP│    │ mini ESP│    │ mini ESP│
       │ 1 moteur│    │ 1 moteur│    │ 1 moteur│    │ 1 moteur│
       └─────────┘    └─────────┘    └─────────┘    └─────────┘
```

Un slot = un module. Un module = un firmware moteur principal.

## Fiche standard d'une cartouche

Chaque moteur devra posséder une fiche reproductible, par exemple `docs/rack/engines/<nom>.md`, contenant au minimum :

| Champ | Contenu |
| --- | --- |
| Identité | nom, version, rôle musical |
| MCU | référence exacte de l'ESP et mémoire disponible |
| Firmware | environnement PlatformIO, version du protocole |
| Audio | format, fréquence, canaux, latence cible |
| Contrôle | paramètres exposés, notes/events acceptés |
| UI | paramètres à afficher et représentation graphique |
| Ressources | CPU/RAM/flash mesurés |
| I/O rack | alimentation, masse, bus de contrôle, bus audio |
| Presets | format et emplacement |
| Mise à jour | méthode de flash et récupération |
| Validation | tests, limites connues, statut matériel |

## Backplane 4 slots : exigences avant schéma

Le rack physique doit être traité comme un **backplane**, pas comme quatre cartes câblées au hasard. Avant de figer le connecteur, mesurer et documenter :

- tension(s) d'alimentation et courant maximal par slot ;
- courant de pointe au démarrage de quatre modules ;
- masse et stratégie anti-bruit ;
- type de transport audio ;
- type de bus de commande ;
- sélection/adressage des slots ;
- lignes nécessaires au boot/flash ;
- détection de présence et identification d'une cartouche ;
- comportement d'un slot absent, planté ou incompatible ;
- possibilité de remplacer une cartouche sans endommager la machine.

**Décision volontairement non figée :** le brochage et le bus audio ne sont pas encore spécifiés. Ils doivent découler des mesures et du prototype électrique, pas de la seule documentation.

## Contrat logiciel du rack

Prévoir une couche commune indépendante du moteur :

```text
DISCOVER
IDENTIFY
CAPS
ENGINE_INFO
PARAM_GET / PARAM_SET
NOTE_ON / NOTE_OFF
PRESET_LOAD / PRESET_SAVE
STATUS
CPU / RAM / XRUN
BOOTLOADER_ENTER
FW_BEGIN / FW_DATA / FW_END
FW_VERIFY
REBOOT
```

Chaque cartouche annonce au minimum son identifiant, sa version de firmware, sa version de protocole et ses capacités. Une cartouche inconnue ne doit jamais pouvoir casser le démarrage du cœur AZ-2.

## Flash depuis l'écran

**ROADMAP — non livré.**

La cible est de pouvoir choisir sur l'écran AZ-2 :

```text
CONFIG
  └─ RACK
      ├─ SLOT 1 — moteur / version
      ├─ SLOT 2 — moteur / version
      ├─ SLOT 3 — moteur / version
      ├─ SLOT 4 — moteur / version
      └─ FIRMWARE
          ├─ fichier sur SD
          ├─ vérification
          ├─ flash
          └─ résultat / rollback
```

Le flash devra être conçu comme une opération sûre : fichier identifié par cible, manifeste/version, contrôle d'intégrité, slot explicitement sélectionné, entrée contrôlée en bootloader, progression visible, vérification après écriture et procédure de récupération après coupure.

**À ne pas faire :** envoyer aveuglément un binaire depuis l'UI. Le mécanisme exact dépendra du mini-ESP retenu et de son bootloader.

## MIDI

L'ajout/extension MIDI reste un chantier séparé de la première version du rack. Le protocole interne doit néanmoins éviter de bloquer une future passerelle MIDI : notes, vélocité, clock/transport et paramètres doivent avoir une représentation propre.

## Ordre de réalisation

1. Définir la **fiche cartouche v0** et choisir le mini-ESP de référence.
2. Faire **un slot de développement** sur table.
3. Faire fonctionner **un gros moteur autonome** et mesurer CPU/RAM/latence/bruit.
4. Stabiliser le protocole découverte/contrôle.
5. Valider le transport audio.
6. Prototyper le flash d'un seul module depuis le firmware principal.
7. Ajouter protections et récupération.
8. Concevoir le backplane 4 slots.
9. Passer à 2 puis 4 modules en stress test.
10. Intégrer l'écran RACK et la documentation utilisateur.

## Critères de réussite

Le rack sera considéré intégré seulement quand quatre emplacements pourront coexister sans dégrader le tracker principal, avec identification fiable, erreurs visibles, mesures de charge, démarrage reproductible et récupération après échec de mise à jour.

---

Cette architecture transforme le rack en plateforme : le châssis reste stable, tandis que les moteurs peuvent évoluer indépendamment.
