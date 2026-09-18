# AZ-2 — Cartouche ESP8266 AZ-CHIP

**Statut : conception matérielle et logicielle. Cible initiale : modules nus ESP-12F possédés par le projet.**

## Rôle

Les ESP8266 deviennent des cartouches spécialisées dans les moteurs sonores de consoles et les synthèses lo-fi. Les ESP32 restent réservés aux moteurs plus lourds comme AZ-VA1.

| Famille | Rôle |
| --- | --- |
| ESP8266 / ESP-12F | GB APU, PSG, pulse/noise, chiptune, moteurs mono ou peu polyphoniques |
| ESP32 classique/S3 | VA, wavetable, granular, effets, polyphonie plus élevée |
| Teensy 4.1 maître | séquenceur, horloge, mixage, sampler, effets finaux et DAC |

## Catalogue AZ-CHIP envisagé

1. **AZ-CHIP GB** : deux canaux pulse, wavetable 4 bits, bruit/LFSR, sweep et enveloppes.
2. **AZ-CHIP PSG** : inspiration SN76489, trois carrés et un bruit.
3. **AZ-CHIP AY** : trois tons, bruit et enveloppe globale.
4. **AZ-CHIP NES** : pulse, triangle et bruit ; DPCM seulement après validation du débit.
5. **AZ-CHIP SID-Lite** : synthèse inspirée du SID, sans prétendre à l’émulation électrique exacte de son filtre analogique.

Premier firmware recommandé : **AZ-CHIP GB**, basé sur le minigb_apu MIT déjà présent dans le dépôt. Il permet de réutiliser un composant connu tout en séparant son interface console de son usage comme instrument.

## Pourquoi le module nu est intéressant

- carte porteuse identique pour toutes les cartouches ;
- alimentation et protections choisies pour l’audio ;
- connecteur AZ-BUS placé au même endroit ;
- RESET et BOOT pilotables par la machine ;
- port de programmation accessible ;
- aucun connecteur ou composant de devboard inutile ;
- possibilité de flasher plusieurs moteurs sur la même cartouche.

Le module nu n’est cependant pas une carte prête à alimenter : la porteuse est obligatoire.

## Carte porteuse ESP-12F

### Alimentation

- entrée +5 V depuis AZ-BUS ;
- régulateur 3,3 V propre, capable d’au moins 500 mA en pointe ;
- 100 nF céramique au plus près de VCC ;
- 10 µF près du module ;
- capacité réservoir supplémentaire à valider à l’oscilloscope ;
- masse continue, mais séparation raisonnable entre retour alimentation et signaux audio ;
- aucune tension de 5 V sur un GPIO.

### Résistances de démarrage

| Signal ESP-12F | État normal | Fonction |
| --- | --- | --- |
| EN / CH_PD | pull-up 10 kΩ vers 3,3 V | activation du module |
| RST | pull-up 10 kΩ vers 3,3 V | reset actif bas |
| GPIO0 | pull-up 10 kΩ | bas au reset pour le chargeur série |
| GPIO2 | pull-up 10 kΩ | strap de boot |
| GPIO15 | pull-down 10 kΩ | strap de boot |

RESET et GPIO0 doivent aussi pouvoir être pilotés par l’hôte. Utiliser des transistors/open-drain ou vérifier que les sorties de l’hôte ne combattent jamais les résistances/boutons.

### Programmation et secours

Prévoir un header ou des pastilles :

- 3V3 ;
- GND ;
- TX0 / GPIO1 ;
- RX0 / GPIO3 ;
- GPIO0 / BOOT ;
- RST.

Le premier flash se fait avec le programmateur ESP8266/ESP32 déjà disponible. Les mises à jour suivantes pourront passer par le chargeur AZ-BUS.

### Antenne

- placer l’antenne du module au bord de la carte ;
- aucune piste, plan de cuivre, vis ou plaque métallique sous et devant l’antenne ;
- si le boîtier final est métallique, orienter l’antenne vers une ouverture ou désactiver totalement la radio et tester la stabilité malgré tout.

Le Wi-Fi doit être désactivé pendant la production audio afin d’éviter consommation variable et interruptions inutiles.

## Connexion AZ-BUS proposée

| Fonction | AZ-BUS | ESP-12F |
| --- | --- | --- |
| Alimentation | +5 V/GND | régulateur 3,3 V de la porteuse |
| Contrôle + audio V1 | UART_TX/RX | RX0 GPIO3 / TX0 GPIO1 |
| RESET | RESET | RST via open-drain |
| BOOT | BOOT | GPIO0 via open-drain |
| Présence | SLOT_PRESENT | résistance d’identification ou GPIO disponible |
| Défaut/prêt | FAULT/IRQ | GPIO disponible à fixer après inventaire |

## Choix audio V1 : PCM par UART

L’ESP8266 n’est pas retenu d’office comme esclave I2S du Teensy : cette capacité doit être démontrée sur le framework choisi avant tout PCB. Pour ne pas bloquer le rack, AZ-CHIP V1 transporte un flux audio lo-fi sur l’UART du slot.

Cible initiale :

- UART 1 Mbaud ;
- mono 8 bits non signé ;
- 16 kHz ou 22,05 kHz selon mesures ;
- paquets binaires avec séquence, longueur 16 bits et CRC ;
- file circulaire côté ESP8266 ;
- file de réception et rééchantillonnage côté Teensy ;
- compteurs underrun, overflow et paquet perdu ;
- contrôle et audio multiplexés avec priorité PANIC/NOTE_OFF.

Pour des moteurs de consoles, ce compromis est cohérent. Les moteurs ESP32 évolués utiliseront l’I2S numérique du rack.

Une V2 pourra employer I2S si un mode esclave fiable est validé, ou une petite interface audio dédiée sur la porteuse.

## Interface instrument

AZ-CHIP GB doit pouvoir fonctionner de deux façons :

1. **Mode instrument** : NOTE_ON/OFF et macros pilotent directement les quatre canaux.
2. **Mode APU** : écritures de registres Game Boy horodatées, pour rejouer fidèlement une source console.

Macros proposées : moteur, forme/duty, sweep, enveloppe, pitch, longueur, bruit, mix des canaux et bitcrush.

## Budget réaliste

- commencer avec un seul moteur actif ;
- allocations statiques ;
- aucune utilisation Wi-Fi/Bluetooth ;
- génération par blocs ;
- pas d’accès flash dans la routine audio ;
- journal série désactivé en mode performance ;
- fréquence CPU 160 MHz seulement après test thermique et alimentation ;
- retour automatique au silence si le heartbeat disparaît.

## Déroulement du prototype

### Lot 1 — Porteuse de programmation

- régulateur et straps de boot ;
- header USB-UART ;
- test boot normal/chargeur/reset ;
- mesure du 3,3 V au démarrage.

### Lot 2 — AZ-BUS sans son

- HELLO, DESCRIBE, HEARTBEAT et METRICS ;
- flash depuis la machine ;
- reconnexion après reset et mauvaise image refusée.

### Lot 3 — AZ-CHIP GB

- porter minigb_apu ;
- produire un motif de test puis NOTE_ON/OFF ;
- transporter PCM 16 kHz/8 bits ;
- recevoir, rééchantillonner et mixer dans le Teensy.

### Lot 4 — Qualité et catalogue

- tester 22,05 kHz ;
- ajouter patches et macros ;
- comparer bruit, latence et charge ;
- ajouter PSG puis AY ;
- documenter la compatibilité de chaque image avec la taille flash du module.

## Validation

- 30 minutes sans reset ni dérive ;
- silence absolu au repos ;
- aucune note bloquée ;
- latence stable ;
- aucune corruption du protocole pendant l’audio ;
- recovery après interruption de flash ;
- alimentation stable lors du boot ;
- température et courant mesurés ;
- dix changements de firmware depuis l’AZ-2 ;
- module absent ou planté : le rack principal continue de jouer.

## Informations à relever avant schéma final

- marquage exact de chaque module ;
- ESP-12E ou ESP-12F ;
- taille de flash détectée ;
- dimensions et orientation de l’antenne ;
- nombre de modules disponibles ;
- programmateur et tension logique réellement utilisés.