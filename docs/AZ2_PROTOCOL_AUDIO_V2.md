# AZ-2 — Protocole audio Game Boy V2

**Statut : préparé dans le code partagé, non activé sur le fil.**  
Le transport utilisé aujourd'hui reste V1 : mono PCM8 / 14 kHz, paquet `0x01 + len8 + payload`.

Le but du V2 est de permettre une évolution coordonnée vers une meilleure qualité audio sans casser les commandes texte ni flasher une seule carte avec un format incompatible.

## Objectifs

- longueur 16 bits,
- numéro de séquence,
- CRC,
- sample rate explicite,
- mono ou stéréo,
- PCM8 ou PCM16 little-endian,
- resynchronisation après paquet corrompu,
- compatibilité avec les deux firmwares depuis un même SHA.

## Trame proposée

```text
offset  taille  champ
0       1       magic = 0x03
1       1       version = 2
2       1       flags (bit0 = stereo)
3       1       format (1=PCM_U8, 2=PCM_S16LE)
4       2       sequence, little-endian
6       2       payload length, little-endian
8       2       sample rate, little-endian
10      N       payload
10+N    2       CRC16/CCITT-FALSE, little-endian
```

Le CRC couvre les octets **version jusqu'à la fin du payload**. Le magic et les deux octets du CRC sont exclus.

Les constantes et helpers sont déjà définis dans `lib/AZ2_Protocol/AZ2_Protocol.h` et couverts par les tests natifs. Aucun émetteur ou récepteur V2 n'est encore activé.

## Profil recommandé pour le premier essai

Premier palier envisagé :

- PCM8,
- stéréo,
- 32 kHz,
- environ 64 ko/s de payload avant overhead,
- UART 921600 : plafond brut théorique ~92,16 ko/s en 8N1.

Ce profil laisse de la marge aux commandes. Le PCM16 stéréo 44,1 kHz brut est exclu sur l'UART actuel : ~176,4 ko/s de payload, avant même le framing.

## Migration sûre

1. Implémenter le parser V2 côté Teensy **sans retirer V1**.
2. Tester le parser sur trames synthétiques, CRC faux, longueur fausse, séquence manquante et timeout.
3. Implémenter l'émetteur V2 côté ESP32 derrière une option compile-time désactivée par défaut.
4. Compiler les deux firmwares et vérifier le protocole natif.
5. Tester sur câble réel à 921600 bauds.
6. Ajouter une négociation explicite de capacité.
7. Basculer uniquement lorsque les deux côtés confirment V2.
8. Conserver V1 comme fallback pendant la phase beta.

## Télémétrie nécessaire

Le Teensy doit relever :

- paquets V2 reçus,
- CRC invalides,
- gaps de séquence,
- longueurs invalides,
- timeouts,
- ring drops,
- underruns de sortie,
- latence estimée.

L'ESP32 doit relever :

- paquets envoyés,
- temps APU,
- temps de sérialisation,
- retard de frame,
- bytes en attente TX.

## Critère de validation

Un profil V2 n'est considéré utilisable que si, pendant au moins 30 minutes avec LSDJ + pistes AZ-2 actives :

- aucun décrochage durable de l'UI,
- aucune corruption de commandes,
- zéro CRC invalide en conditions normales,
- zéro perte de séquence en conditions normales,
- pas d'underrun audible,
- cadence GB stable.

Le passage au V2 ne doit jamais être fait en modifiant seulement un des deux firmwares.
