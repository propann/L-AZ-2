# AZ-2 — Protocole audio Game Boy V2

**Statut : émetteur ESP32 + récepteur Teensy implémentés ; le pilote V2 transporte désormais du PCM8 stéréo 14 kHz mais reste désactivé par défaut.**  
Le transport de production reste V1 : mono PCM8 / 14 kHz, paquet `0x01 + len8 + payload`. Quand le pilote V2 est activé et négocié, l'ESP32 envoie L/R entrelacé avec séquence et CRC. Le Teensy valide la trame puis downmixe actuellement vers son bus audio mono existant : la stéréo est donc préservée sur le lien, pas encore jusqu'au DAC.

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

Les constantes, l'encodeur et le décodeur à CRC sont définis dans `lib/AZ2_Protocol/AZ2_Protocol.h` et couverts par les tests natifs. L'ESP32 possède un émetteur V2 pilote, le Teensy possède un récepteur avec détection de CRC et des sauts de séquence. Le pilote reste inactif tant que `kGbAudioV2PilotEnabled` vaut `false` (valeur par défaut). Le Teensy répond à `GBV2:QUERY` par `GBV2:READY` uniquement si le pilote est activé ; l'ESP32 continue d'envoyer V1 sans cette réponse.

## Profil recommandé pour le premier essai

Premier palier matériel à qualifier :

- PCM8,
- stéréo,
- 14 kHz avec le générateur APU actuel (~28 ko/s de payload),
- séquence + CRC16 + fallback V1,
- puis seulement après validation, étude d'un passage à 32 kHz (~64 ko/s de payload),
- UART 921600 : plafond brut théorique ~92,16 ko/s en 8N1.

Ce profil laisse de la marge aux commandes. Le PCM16 stéréo 44,1 kHz brut est exclu sur l'UART actuel : ~176,4 ko/s de payload, avant même le framing.

## Migration sûre

1. [x] Décodeur V2 Teensy et émetteur ESP32 présents, avec V1 conservé.
2. [x] Tests natifs du roundtrip, CRC et longueurs invalides ; [ ] injection de pertes/timeout et mesure matérielle.
3. [x] Option compile-time partagée désactivée par défaut et échange `GBV2:QUERY` / `GBV2:READY`.
4. [ ] Confirmer la compilation CI **des deux firmwares pour le SHA de livraison**, puis tester sur câble réel à 921600 bauds.
5. [ ] Valider 30 min le pilote V2 mono 14 kHz avant toute activation par défaut.
6. [ ] Étendre réception, mixage et capture audio à stéréo/32 kHz ; le Teensy actuel utilise encore un bus GB mono.
7. [ ] Déployer profil supérieur uniquement après négociation et tests, en gardant V1 comme fallback.

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
