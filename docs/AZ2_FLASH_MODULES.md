# AZ-2 — Flash des modules moteurs depuis la machine

**Statut : étude d’architecture. Aucun firmware de production n’est encore flashé automatiquement.**

## Objectif

Brancher une carte moteur au rack AZ-BUS, laisser l’AZ-2 l’identifier, choisir un moteur sur l’écran puis installer et vérifier son firmware sans ordinateur.

L’ESP32-S3 de l’écran orchestre l’installation. Le Teensy 4.1 reste maître de l’audio et doit être arrêté et muté pendant toute mise à jour.

## Décision d’architecture

Une méthode unique dans l’interface, avec deux chemins techniques :

1. **Mise à jour AZ-BUS normale** : chaque module possède au départ un petit chargeur AZ-BUS. L’ESP32 envoie le firmware par UART avec CRC, vérification et redémarrage.
2. **Mode usine/secours** : BOOT, RESET et éventuellement USB/SWD restent accessibles. Ils servent au premier flash et à récupérer un module corrompu.

Cette séparation est nécessaire : ESP32, AVR, RP2040/RP2350 et Teensy n’ont pas le même chargeur ROM. Une seule séquence BOOT/RESET ne peut pas les flasher tous de façon fiable.

## Fenêtre « Moteurs > Installer »

1. Détecter le module et lire son identifiant de carte, MCU, taille flash, version du chargeur et moteur installé.
2. Montrer seulement les images compatibles présentes sur la SD.
3. Afficher version, licence, taille, SHA-256 et mémoire nécessaire.
4. Arrêter le transport, couper les notes et muter le slot.
5. Envoyer l’image par blocs numérotés avec CRC.
6. Vérifier l’image complète, redémarrer et refaire le handshake.
7. Restaurer le son seulement après confirmation ENGINE_READY.

États affichés : Détecté, Compatible, Effacement, Écriture 0–100 %, Vérification, Redémarrage, Prêt, Échec/récupération.

Arborescence SD proposée :

~~~text
/az2/engines/
  index.json
  esp32/
  rp2040/
  rp2350/
  avr/
  teensy41/
~~~

Chaque image est accompagnée d’un manifeste : carte, moteur, version, taille, SHA-256, version minimale du chargeur et ressources audio attendues.

## Compatibilité par famille

| Module | Premier flash | Mise à jour dans l’AZ-2 | Difficulté |
| --- | --- | --- | --- |
| ESP32/ESP32-S3 | UART ROM : TX/RX + EN + GPIO0 | Chargeur AZ-BUS ou protocole ROM | Facile |
| ESP8266/ESP-12F | UART ROM : TX/RX + RST + GPIO0 | Chargeur AZ-BUS ; porteuse avec straps obligatoire | Facile |
| Arduino AVR avec Optiboot | USB-série ou ISP | UART + impulsion RESET | Facile |
| RP2040/RP2350 | USB BOOTSEL ou SWD | Chargeur AZ-BUS résident recommandé | Moyen |
| Teensy 4.x | USB + PROGRAM/HalfKay | Chargeur AZ-BUS à développer ; USB secours obligatoire | Difficile |
| Daisy/STM32 | DFU/USB ou SWD | Chargeur AZ-BUS spécifique | Moyen à difficile |

### Cas Teensy externe

HalfKay se pilote par USB HID, pas par un simple UART. En V1, un module Teensy reçoit donc son premier firmware par USB. Ensuite, un chargeur AZ-BUS applicatif pourra accepter les mises à jour depuis la machine. PROGRAM/USB doit rester accessible si l’application ne démarre plus.

## Protocole minimal

- FW_QUERY : identité et capacités ;
- FW_BEGIN : taille, version et SHA-256 ;
- FW_CHUNK : offset, données et CRC ;
- FW_END : vérification complète ;
- FW_COMMIT : activation ;
- FW_ABORT : abandon propre ;
- FW_STATUS : progression ou erreur.

Règles : jamais flasher pendant lecture/enregistrement ; alimentation stable ; timeout et reprise par bloc ; refuser une mauvaise cible ; conserver l’ancienne image quand possible ; ne jamais programmer par l’I2S ; connecteur détrompé en logique 3,3 V.

## Relevé réel des broches Teensy 4.1

Sources actuelles : src_teensy/az2_audio/main.cpp et AZ2_CABLAGE_MASTER.md.

| Groupe | Broches | État |
| --- | --- | --- |
| UART écran Serial1 | 0, 1 | Occupées |
| Croix | 2, 3, 4, 5 | Réservées dans le firmware |
| Boutons A/B/C | 6, 8, 9 | Réservées |
| I2S DAC | 7, 20, 21 | Occupées et confirmées |
| SPI futur | 10, 11, 12, 13 | Libres mais réservées |
| Encodeur 1 | 14, 15, 16 | Occupées et testées |
| Encodeur 2 | 17, 18, 19 | Occupées et testées |
| Encodeur 3 | 22, 24, 25 | Occupées et testées |
| Bouton D | 23 | Réservée |
| Extension | 26–41 | Non utilisées par le firmware actuel |
| SD intégrée Teensy | Interface dédiée | À préserver pour le sampler |

## Proposition de slot V1

Il n’est pas nécessaire de déplacer les encodeurs : les broches 26–35 offrent déjà les lignes nécessaires. La paire 28/29 est la candidate UART matérielle à confirmer sur le banc.

| Fonction | Broche proposée | Rôle |
| --- | --- | --- |
| MOD_RX | 28 | Réception contrôle/flash, paire UART candidate |
| MOD_TX | 29 | Émission contrôle/flash, paire UART candidate |
| MOD_RESET | 30 | Réinitialisation cible |
| MOD_BOOT | 31 | Sélection chargeur cible |
| MOD_DETECT | 32 | Détection/identité, optionnelle |
| MOD_READY | 33 | Sécurité audio/état, optionnelle |

**Cette affectation n’est pas encore à figer sur PCB.** Il faut vérifier que la paire choisie correspond bien au port UART matériel retenu. RESET/BOOT peuvent rester sur des GPIO ordinaires.

Ordre de récupération des lignes :

1. Utiliser 28–33 sans déplacer de commande ; garder 26/27 en réserve.
2. Garder 10–13 pour SPI/extension rapide.
3. Les broches de la croix et A-D peuvent servir seulement aux essais si elles ne sont pas encore câblées.
4. Déplacer un encodeur en dernier recours ; un expandeur GPIO peut récupérer neuf lignes mais ajoute latence et complexité.

## Audio du module

Le flash et le contrôle passent par UART. L’audio revient séparément : sortie analogique vers mixeur pour le premier prototype, puis I2S/TDM synchronisé pour le rack numérique. Ne jamais relier directement deux maîtres I2S sur le même bus.

## Plan de réalisation

1. **Banc ESP32 externe** : TX/RX/EN/GPIO0, premier flash par ordinateur, puis mise à jour test depuis la SD.
2. **Fenêtre maintenance** : manifestes, compatibilité stricte, progression, journal, verrouillage transport et mute.
3. **Moteur test** : oscillateur simple, commandes note/paramètre, mesures de latence et coupure volontaire pendant flash.
4. **Autres cartes** : AVR, puis RP2040/RP2350. Teensy externe en dernier car sa récupération est la plus exigeante.

## Validation avant PCB

- confirmer au multimètre les broches réellement non câblées ;
- vérifier le port série matériel exact ;
- tester 3,3 V, RESET, BOOT et le courant maximal du slot ;
- provoquer une coupure pendant chaque phase de flash ;
- vérifier qu’un module défaillant ne bloque ni le Teensy maître ni le DAC ;
- conserver USB/PROGRAM ou SWD accessible.