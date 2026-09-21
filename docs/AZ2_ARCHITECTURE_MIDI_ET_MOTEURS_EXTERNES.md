# AZ-2 — architecture figée MIDI et moteurs externes

**Décision du 21 septembre 2026.** Ce document sépare les choix validés des
éléments qui doivent encore être mesurés. Aucun câblage définitif ne doit être
fait à partir d'une simple hypothèse.

## 1. Responsabilités figées

Le **Teensy 4.1 reste le maître musical et audio** : réception du MIDI USB et
du futur MIDI DIN, séquenceur, routage des pistes, moteurs internes, commandes
des moteurs externes, mixage final, effets et sortie PCM5102A.

Les ESP32 ne reçoivent pas directement le MIDI DIN. Ils reçoivent NOTE_ON,
NOTE_OFF, PANIC et paramètres depuis le Teensy, puis renvoient du PCM.

```text
MIDI DIN -> 6N138 -> UART MIDI Teensy (31250 bit/s)
                         |-> moteurs internes
                         |-> commandes GRANULAR MAX
                         `-> commandes SPECTRAL SWARM

SPECTRAL -> S3 agrégateur envisagé -> I2S Teensy
                                      -> mixage/effets -> PCM5102A
```

## 2. MIDI DIN avec 6N138

Le 6N138 est réservé à une **entrée MIDI DIN isolée du Teensy**. Il ne doit
pas être raccordé au S3. Le schéma devra garantir : UART 31250 bit/s 8-N-1,
pull-up de sortie au 3,3 V, résistance base-émetteur assurant des fronts assez
rapides, diode antiparallèle sur la LED et aucune masse MIDI reliée directement
à la masse logique. Les valeurs finales et le numéro des broches DIN seront
vérifiés avant soudure.

Le port UART Teensy n'est pas encore choisi. Serial7 reste réservé au rack et
ne doit pas être pris automatiquement pour le MIDI DIN.

## 3. Moteurs préparés séparément

### GRANULAR MAX — ESP32-S3 N16R8

- 64 grains stéréo, double cœur, 44,1 kHz, PSRAM obligatoire ;
- DMA I2S autonome validé 30 secondes sans retard ;
- banc : GPIO7 BCLK, GPIO9 LRCLK, GPIO11 DATA OUT ;
- contrôle futur : GPIO16/18 à 921600 bit/s ;
- passage I2S maître vers esclave uniquement lors du banc Teensy.

### SPECTRAL SWARM — ESP-WROOM-32D

- 4 voix × 16 partiels, soit 64 oscillateurs, double cœur, 44,1 kHz ;
- banc : GPIO26 BCLK, GPIO25 LRCLK, GPIO22 DATA OUT ;
- GPIO16 RX et GPIO17 TX réservées au futur contrôle/agrégateur ;
- test DMA I2S 30 secondes ajouté et compilation validée ;
- test matériel DMA à refaire dès reconnexion USB.

## 4. Agrégation audio envisagée

Pour ne souder qu'un moteur au Teensy : le WROOM envoie son PCM au S3 ; le S3
le mélange à GRANULAR MAX puis renvoie un seul flux stéréo au Teensy. Le
Teensy l'ajoute à ses moteurs internes et sort sur le PCM5102A.

Cette solution est possible en principe grâce aux deux contrôleurs I2S du S3,
mais elle n'est pas encore prête à souder. Il reste à prouver : réception I2S
WROOM, émission I2S esclave Teensy, horloge commune, charge GRANULAR 64 plus
agrégation sans underrun, et silence propre au démarrage/PANIC.

Le WROOM ne devra pas conserver une horloge indépendante dans le montage
final. Les deux moteurs devront suivre l'horloge audio du Teensy.

## 5. Ordre de validation

1. GRANULAR MAX seul sur DMA I2S : **fait** ;
2. SPECTRAL SWARM seul sur DMA I2S : **binaire prêt, matériel à tester** ;
3. NOTE_ON/OFF, paramètres, PANIC et métriques sur chaque moteur ;
4. WROOM vers S3 avec une tonalité avant de réunir les deux DSP ;
5. S3 agrégateur pendant 30 minutes sans erreur ;
6. entrée I2S et contrôle dans un firmware Teensy de laboratoire ;
7. MIDI DIN 6N138 sur un banc séparé ;
8. fusion production seulement après chaque validation.

Le firmware Teensy de production ne doit pas être modifié pendant les étapes
1 à 5.
