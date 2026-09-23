# AZ-2 — éditeurs de patch GRANULAR et SPECTRAL

## Architecture retenue

GRANULAR et SPECTRAL apparaissent dans la palette de moteurs de l'écran au
même titre que les moteurs internes. Chaque carte reste une instance DSP
matérielle unique : plusieurs pistes peuvent l'adresser, mais elles partagent
le patch courant du moteur externe. Le Teensy route les notes et paramètres ;
le S3 agrège les deux sorties et reste le coupe-circuit général du rack.

Préfixes réservés, sans collision avec `ENGINE:<piste>:<moteur>` :

- `RACK_ENGINE:<GRANULAR|SPECTRAL>:<ON|OFF>` ;
- `RACK_NOTE_ON:<GRANULAR|SPECTRAL>:<note>:<velocity>` ;
- `RACK_NOTE_OFF:<GRANULAR|SPECTRAL>:<note>` ;
- `RACK_PARAM:<GRANULAR|SPECTRAL>:<id>:<0-127>` ;
- `RACK_PATCH:<GRANULAR|SPECTRAL>:<slot>` ;
- `RACK_PATCH_SAVE:<GRANULAR|SPECTRAL>:<slot>` ;
- `RACK_STATUS` et `RACK_PANIC`.

La liaison Teensy ↔ S3 reste à 115200 bauds. Le S3 relaie les commandes
SPECTRAL vers le WROOM par GPIO4 TX → GPIO16 RX ; le retour utilise GPIO17 TX
→ GPIO5 RX.

## GRANULAR — fenêtre de patch

| ID | Paramètre | Plage/effet |
|---:|---|---|
| 0 | POSITION | centre de lecture dans la source |
| 1 | SIZE | durée des grains |
| 2 | DENSITY | nombre/taux de grains actifs |
| 3 | PITCH | hauteur, centre 64 = unisson |
| 4 | PITCH SPRAY | dispersion de hauteur |
| 5 | POSITION SPRAY | dispersion de position |
| 6 | PAN SPREAD | largeur stéréo |
| 7 | REVERSE | probabilité de lecture inversée |
| 8 | WINDOW | forme/fondu de fenêtre |
| 9 | FREEZE | gel de la position/source |
| 10 | ATTACK | attaque de la voix |
| 11 | DECAY | chute de la voix |
| 12 | SUSTAIN | maintien de la voix |
| 13 | RELEASE | relâchement de la voix |
| 14 | CUTOFF | filtre de sortie |
| 15 | RESONANCE | résonance du filtre |
| 16 | DRIVE | saturation douce |
| 17 | LEVEL | niveau moteur |

Presets initiaux : CLOUD, DRONE, DUST, SHIMMER, REVERSE, TEXTURE, FREEZE,
PERCUSSIVE.

## SPECTRAL — fenêtre de patch

| ID | Paramètre | Plage/effet |
|---:|---|---|
| 0 | PARTIALS | 1 à 16 partiels en production stable |
| 1 | MORPH | pente spectrale |
| 2 | STRETCH | inharmonicité des partiels |
| 3 | TILT | grave/aigu |
| 4 | ODD/EVEN | équilibre harmoniques paires/impaires |
| 5 | DETUNE | désaccordage des voix |
| 6 | SPREAD | largeur stéréo |
| 7 | MOTION | vitesse de modulation |
| 8 | CHARACTER | mélange direct/résonateur |
| 9 | DRIVE | saturation douce |
| 10 | ATTACK | attaque de la voix |
| 11 | DECAY | chute de la voix |
| 12 | SUSTAIN | maintien de la voix |
| 13 | RELEASE | relâchement de la voix |
| 14 | CUTOFF | filtre de sortie |
| 15 | RESONANCE | résonance du filtre |
| 16 | LEVEL | niveau moteur |

Presets initiaux : AIR, WAVES, GLASS, CHOIR, METAL, SWARM, ORGAN, ABYSS.

## Comportement de la page PATCH

- même navigation deux paramètres par ligne que les moteurs existants ;
- liste de presets, TEST note, oscilloscope et volume conservés ;
- chaque changement est audible immédiatement et confirmé par le rack ;
- SAVE/LOAD stocke les valeurs du moteur externe avec le projet ;
- `NOTE_OFF` déclenche réellement le release ; aucune tonalité permanente au
  repos ;
- `PANIC` ferme les enveloppes et remet la sortie du rack à zéro.

## Mémoire GRANULAR et affichage dans PATCH

Le S3 N16R8 dispose de 8 Mio de PSRAM. Le moteur utilise deux banques PCM
fixes de **2,5 Mio chacune** : une banque ACTIVE lue par le DSP et une banque
STAGING remplie pendant le transfert. Cela représente environ 29,7 secondes
mono 16 bits à 44,1 kHz par banque. Le reste de la PSRAM demeure réservé aux
structures DSP, aux futures tables et à la marge système.

Le chargement est atomique :

1. le Teensy lit et valide le WAV depuis sa carte SD ;
2. les blocs numérotés sont envoyés avec CRC vers STAGING ;
3. le S3 refuse tout bloc hors séquence ou dépassant la capacité ;
4. après validation du nombre d'échantillons et du CRC global, ACTIVE et
   STAGING sont échangées à une frontière de bloc audio ;
5. en cas d'annulation, déconnexion ou erreur, ACTIVE continue de jouer et le
   contenu incomplet de STAGING est abandonné.

Le patch/projet sauvegarde le **chemin SD**, la note racine, les points de
début/fin et les paramètres, jamais les octets PCM. Au chargement du projet,
le Teensy recharge le WAV correspondant. Si le fichier manque, l'ancien
buffer actif reste intact et l'écran affiche `FICHIER ABSENT`.

Formats d'entrée prévus : WAV PCM 16 bits, mono ou stéréo, 8 à 48 kHz. Le
Teensy downmixe le stéréo en mono ; le S3 conserve le sample rate source et
calcule le pas de lecture afin de sortir à 44,1 kHz. Les formats compressés
ou flottants sont refusés explicitement au lieu d'être mal interprétés.

La fenêtre PATCH GRANULAR affiche en permanence :

| Information | Exemple |
|---|---|
| SOURCE | `Bass_26.wav` |
| FORMAT | `PCM16 MONO 24 kHz` |
| DURÉE | `1,84 s` |
| MÉMOIRE | `86 Ko / 2,5 Mio` avec jauge |
| BANQUE | `A ACTIVE / B LIBRE` |
| CHARGEMENT | barre 0–100 %, débit et temps estimé |
| DSP | grains actifs, temps max du bloc et marge temps réel |
| ÉTAT | PRÊT, CHARGEMENT, CRC ERREUR, TROP GRAND, FICHIER ABSENT |

Commandes de gestion disponibles dans cette même fenêtre :

- `PARCOURIR` : ouvre la bibliothèque `/samples` déjà utilisée par les pads ;
- `CHARGER` et `ANNULER` ;
- `DÉCHARGER` : libère les deux banques après confirmation ;
- `DÉBUT` / `FIN` : trim non destructif dans le buffer ;
- `NORMALISER` : gain calculé depuis le pic, sans réécrire le WAV ;
- `ROOT NOTE` : note racine du sample ;
- `RECHARGER` : relit le chemin sauvegardé ;
- `SOURCE TEST` : écoute courte du PCM avant granulation ;
- `GRAIN TEST` : joue le moteur avec le patch courant.

Protections obligatoires : taille maximale vérifiée avant transfert, CRC par
bloc et global, compteur de séquence, timeout, annulation propre, aucune
allocation dynamique dans la boucle audio, aucun remplacement du buffer actif
avant validation complète, et `RACK:OFF` indépendant toujours disponible.

## Mémoire SPECTRAL

SPECTRAL ne charge pas de PCM. Sa fenêtre affiche seulement l'utilisation
RAM, le nombre de voix/partiels, le temps maximal d'un bloc, la marge temps
réel et l'état de la liaison WROOM → S3. Ses presets ne stockent que les 17
paramètres. Cette séparation évite de présenter une fausse jauge de samples
sur un moteur purement synthétique.

## Ordre d'intégration

1. protocole sans collision et routage UART ;
2. notes/enveloppes et paramètres DSP GRANULAR ;
3. notes/enveloppes et paramètres DSP SPECTRAL ;
4. ajout des deux moteurs à la palette et à la page PATCH ;
5. presets et sauvegarde projet ;
6. tests audio séparés, combinés, séquenceur, panic et redémarrage.

## État d'intégration — 22 septembre 2026

- GRANULAR : moteur, paramètres, enveloppe, double banque PCM et transfert
  non bloquant implémentés ; S3 agrégateur flashé.
- SPECTRAL : 4 voix réparties sur les deux cœurs du WROOM, 1–16 partiels,
  stéréo, enveloppe, filtre, drive, 17 paramètres et 8 presets implémentés.
- Le S3 relaie à 115200 bauds les notes, paramètres, presets, status et panic
  vers le WROOM sur GPIO4/GPIO5 ; son firmware est flashé.
- Le Teensy relaie maintenant toutes les commandes `RACK_*` reçues de
  l'écran ; son firmware est flashé.
- GRANULAR et SPECTRAL sont intégrés à la liste **MOTEURS** existante. Leurs
  huit presets apparaissent dans la colonne PATCH ; la fenêtre **PATCH**
  existante expose tous leurs paramètres, la note de test B et les slots
  SAVE/LOAD. Aucune page RACK supplémentaire n'est conservée.
- Les firmwares WROOM spectral et écran compilent, mais restent à flasher sur
  leurs cartes respectives avant le test audio complet.

## Validation matérielle du chargement PCM — 22 septembre 2026

Premier transfert réel validé avec
`/samples/BASS/Bass_26.wav` depuis la SD du Teensy :

- PCM16 mono, 24 kHz, 5 195 échantillons / 10 390 octets ;
- progression 0–100 % remontée par le Teensy ;
- CRC32 `33F9A4AF` identique à chaque transfert ;
- réception dans la banque inactive du S3 puis activation atomique ;
- correction spécifique aux samples courts (aucune marge fixe de 8 192
  échantillons au-delà de la vraie fin du fichier) ;
- compensation du sample rate 24 kHz vers la sortie 44,1 kHz ;
- normalisation non destructive plafonnée à ×8 ;
- gain de sommation des grains relevé avant limiteur ;
- écoute utilisateur confirmée « plus claire » après correction ;
- charge maximale observée environ 2,15 ms pour un budget de 2,90 ms, sans
  nouvelle erreur I2S.

Le transfert Teensy est désormais non bloquant : une machine d'état dans
`loop()` sépare le calcul CRC, l'en-tête et les données. Elle ne lit qu'un bloc
SD de 512 octets par passage et n'écrit que l'espace annoncé disponible par
l'UART. Le test matériel de `Bass_26.wav` a conservé le CRC `33F9A4AF`, atteint
100 % et maintenu `ring_drop=0` pendant toute l'opération. Le flux GB était
inactif (`packets=0`) pendant ce premier contrôle : un essai simultané avec la
console active reste nécessaire. Un nouveau chargement interrompt proprement
l'ancien ; une commande d'annulation explicite reste à ajouter dans la fenêtre
PATCH existante.

Révision de stabilité : le S3 annonce désormais son redémarrage au Teensy,
qui recharge automatiquement `/samples/BASS/Bass_26.wav` dans la PSRAM. Le
WROOM replie correctement les phases des partiels très aigus afin d'éviter les
lectures hors table responsables de bips/craquements. Dans MOTEURS, un choix ne
redessine plus l'écran complet : seules les lignes, presets et l'aperçu touchés
sont actualisés.

## Passe musicalité, niveau et affichage — 23 septembre 2026

- Correction de la page PATCH : `patchScroll` désigne maintenant une ligne
  visuelle contenant jusqu'à deux paramètres. Tous les paramètres GRANULAR et
  SPECTRAL sont donc accessibles sans doublon ni saut pendant le défilement.
- Ajout d'un indicateur compact de position (`1-8/11`, par exemple) afin de
  rendre immédiatement visible la présence de réglages sous l'écran.
- Refonte des huit presets GRANULAR : densités plus basses, grains plus courts
  pour les percussions, drones et gels mieux contrôlés, niveau preset à 127.
- Refonte des huit presets SPECTRAL : moins de partiels agressifs, étirement et
  drive contenus, niveau preset à 127.
- Gain du flux granulaire porté à 1,35 avant le limiteur ; flux spectral reçu
  par le S3 porté à 0,85 × son gain au lieu de 0,5 × son gain. La conversion
  finale reste saturée en int16 afin de protéger la sortie.
- Validation matérielle : S3 granulaire (MAC `9c:13:9e:b7:a5:3c`) sans lecture
  ni écriture I2S courte et rendu autour de 2,08 ms ; WROOM spectral (MAC
  `08:b6:1f:bc:8e:30`) avec `short=0` ; écran S3 (MAC
  `fc:01:2c:d6:04:8c`) vivant et dialogue Teensy confirmé par
  `DISPLAY:ALIVE:TICK` et `TEENSY_AUDIO:READY`.

Le granulaire ne possède pas de carte SD : les fichiers audio restent lus sur
la SD du Teensy, transférés par UART puis conservés dans la PSRAM du S3.

## Correctif sélection et oscilloscope — 23 septembre 2026

- La liste placée à droite de l'oscilloscope possède désormais un vrai focus
  croix : la page PATCH s'ouvre dessus, HAUT/BAS sélectionnent le preset
  (CLOUD, AIR, ORGAN, etc.), DROITE ouvre la grille de paramètres et GAUCHE
  remonte au choix de piste. Le tactile conserve la sélection directe.
- Le scope des pistes GRANULAR/SPECTRAL est branché sur `rackAudioIn` au lieu
  de `trackFx[]`. Les moteurs externes entrent directement dans le mixeur de
  sortie et ne traversent pas la chaîne audio interne par piste ; l'ancien
  branchement ne pouvait donc afficher qu'une ligne plate.
- Firmware écran et firmware Teensy rack compilés puis flashés sur le matériel.
