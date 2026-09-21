# AZ-2 — câblage du rack de moteurs externes

État : proposition de banc, branche `rack-moteurs-externes`. Aucun fil du
prototype AZ-2 actuel ne doit être déplacé avant validation autonome du
module ESP32-S3 N16R8.

Le tableau court faisant foi pour les broches est
[`AZ2_RACK_PINOUT.md`](AZ2_RACK_PINOUT.md). Le présent document conserve les
raisons et l'ordre des essais.

## Tableau des modifications envisagées

Ce tableau décrit seulement les impacts possibles. **Aucune de ces
modifications matérielles n'est faite pendant les benchmarks USB.**

| Élément | Maintenant | Modification future possible | Raison |
|---|---|---|---|
| Teensy 4.1 | aucune modification, firmware de production conservé | libérer l'entrée I2S pin 8, ajouter une entrée audio au mixeur et un protocole de slot | recevoir et mixer le moteur externe |
| Commandes Teensy | aucune | déplacer le bouton B de la pin 8 vers une GPIO libre, probablement 10 | la pin 8 est l'entrée audio I2S standard |
| ESP1 ESP32-S3 N16R8 | moteur granulaire MAX validé, I2S DMA maître sur GPIO7/9/11 | passage futur en I2S esclave et UART de contrôle | moteur granulaire dédié 64 grains |
| ESP2 ESP-WROOM-32D | SPECTRAL SWARM 4×16 compilé, sans PSRAM | valider son DMA I2S autonome puis étudier WROOM → S3 | second moteur avancé |
| ESP écran AZ-2 | aucune modification | ajouter plus tard l'interface de choix et les paramètres des moteurs externes | contrôle utilisateur |
| Alimentation | USB séparé pendant les tests | alimentation 5 V séparée et protégée pour les slots, masse commune | stabilité et bruit |
| Projets sauvegardés | aucun changement | enregistrer identifiant du slot, moteur et patch | rappel des morceaux |

## 1. Étape immédiate : mesure du moteur granulaire

Le premier ESP32-S3 N16R8 reste totalement indépendant :

| ESP32-S3 | Connexion |
|---|---|
| USB-C | ordinateur, alimentation et console série |
| Toutes les GPIO | non connectées |

Cette étape mesure le processeur, la PSRAM et le moteur granulaire. Elle ne
teste pas encore l'I2S et ne nécessite ni Teensy, ni DAC, ni soudure.

## 2. Écoute autonome avant intégration

Pour écouter le moteur sans toucher à AZ-2, utiliser un second PCM5102A :

| ESP32-S3 | PCM5102A | Rôle |
|---:|---|---|
| GPIO7 | BCK | horloge bits I2S |
| GPIO9 | LCK / LRCK | horloge gauche/droite |
| GPIO11 | DIN | audio du S2 vers le DAC |
| GND | GND | masse commune |
| 5 V USB/VBUS | VIN | alimentation du module DAC, si son module l'accepte |

Ne jamais injecter 5 V sur une GPIO. Vérifier le marquage exact du module
PCM5102A avant alimentation : certains breakout acceptent 5 V sur VIN grâce à
leur régulateur, d'autres attendent 3,3 V.

## 3. Liaison de contrôle future ESP32-S3 ↔ Teensy

Serial7 du Teensy est libre et doit être réservé au premier slot :

| Teensy 4.1 | ESP32-S3 | Rôle |
|---:|---:|---|
| 28 RX7 | GPIO16 TX | commandes/réponses S2 → Teensy |
| 29 TX7 | GPIO18 RX | commandes Teensy → S2 |
| GND | GND | référence logique commune |

Les deux cartes travaillent en logique 3,3 V : aucun convertisseur de niveau
n'est requis. Le S3 doit garder une alimentation séparée dimensionnée ; ne pas
alimenter le rack complet depuis la sortie 3,3 V du Teensy.

## 4. Audio ESP32-S3 → mixeur Teensy, après validation autonome

Le Teensy reste maître des horloges. Les horloges déjà envoyées au PCM5102A
peuvent être distribuées au S3 configuré en esclave I2S :

| Teensy 4.1 | ESP32-S3 | Rôle |
|---:|---:|---|
| 21 BCLK | GPIO7 BCLK | horloge bits fournie par le Teensy |
| 20 LRCLK | GPIO9 WS | fréquence d'échantillonnage fournie par le Teensy |
| 8 RX DATA | GPIO11 DOUT | audio calculé par le S2 |
| GND | GND | masse commune obligatoire |

Le PCM5102A actuel reste câblé au Teensy : DATA 7, BCLK 21, LRCLK 20.

### Conflit à résoudre

La broche Teensy 8 est actuellement le bouton B. Elle doit être libérée avant
le premier test audio intégré. Solution minimale : déplacer uniquement ce
bouton vers une GPIO libre, par exemple 10, puis modifier `kBtnBPin`. Ne faire
ce déplacement qu'après réussite du test S3 + DAC séparé.

## 5. Deuxième module

Ne pas câbler deux sorties audio en parallèle. Le deuxième module sera d'abord
testé seul en USB avec le moteur spectral. Pour deux slots simultanés, il
faudra soit une seconde entrée audio Teensy/SAI, soit un bus TDM ou un petit
agrégateur audio. Cette décision est reportée après les benchmarks.

## 6. Ordre de validation

1. benchmark granulaire ESP32 n°1, USB seul ;
2. benchmark spectral ESP32 n°2, USB seul ;
3. écoute du granulaire sur DAC séparé ;
4. test I2S esclave avec horloges Teensy ;
5. déplacement du bouton B vers la broche 10 ;
6. intégration d'un seul slot au mixeur ;
7. décision TDM ou seconde interface pour le deuxième slot.

À chaque étape : masse commune avant les signaux, alimentation coupée pendant
le câblage, continuité vérifiée avant mise sous tension.

## 7. Premier résultat ESP32 n°1

Carte mesurée : ESP32-D0WD-V3 double cœur 240 MHz, 4 Mo de flash, sans PSRAM.
Le banc granulaire utilise un échantillon de 32768 points/64 Kio en RAM
interne, 16 grains simultanés, interpolation linéaire et fenêtre de Hann par
table précalculée.

| Paramètre | Résultat |
|---|---:|
| Audio produit | 10 s à 44,1 kHz |
| Temps de calcul | 2,073727 s |
| Charge estimée d'un cœur | 20,7 % |
| Heap interne libre après le test | 280988 octets |
| PSRAM | absente |

Conclusion provisoire : le moteur 16 grains tient largement sur cet ESP32.
La prochaine validation devra produire le flux en temps réel par I2S et
compter les éventuels underruns ; le benchmark actuel mesure uniquement le
calcul DSP hors ligne.

## 8. Moteur granulaire MAX sur ESP32-S3 N16R8

La carte dédiée retenue est un ESP32-S3 double cœur 240 MHz avec 16 Mo de
flash et 8 Mo de PSRAM. Le laboratoire partage chaque bloc stéréo de 128
échantillons entre les deux cœurs : cœur 0 pour la première moitié des grains,
cœur 1 pour la seconde, puis sommation avant sortie.

Source de test : 30 secondes mono/16 bits/44,1 kHz en PSRAM, soit 2646000
octets. Interpolation linéaire, fenêtre de Hann précalculée, pitch et position
indépendants, panoramique stéréo par grain.

| Grains simultanés | Charge murale double cœur | Décision |
|---:|---:|---|
| 64 | 60,4 % | cible MAX sûre |
| 80 | 75,1 % | mode extrême, marge réduite |
| 96 | 89,7 % | trop proche de la limite avec I2S |
| 112 | 104,4 % | hors temps réel |
| 128 | 119,1 % | hors temps réel |
| 160 | 148,6 % | hors temps réel |
| 192 | 178,6 % | hors temps réel |

Après le test : 331716 octets de heap interne et 5697808 octets de PSRAM
libres. La configuration produit retenue pour la suite est donc 64 grains.

### Validation I2S DMA à 64 grains

Le flux stéréo 16 bits/44,1 kHz a ensuite tourné réellement dans le DMA I2S
pendant 30 secondes sur les broches canoniques GPIO7/GPIO9/GPIO11 :

- 10335 blocs de 128 échantillons ;
- 0 bloc rendu après son échéance ;
- 0 écriture DMA incomplète ;
- pire temps de rendu : 1797 µs ;
- budget par bloc : 2902 µs ;
- marge sur le pire bloc : environ 38 %.

La limite du module est donc **figée à 64 grains**. Le mode 80 grains reste
uniquement expérimental et ne doit pas être proposé comme mode fiable tant que
commandes, modulation et liaison Teensy ne sont pas actives.

## 9. Deuxième résultat : SPECTRAL SWARM sur ESP-WROOM-32D

La carte `ESP32-D0WD-V3`, révision 3.1, double cœur 240 MHz, 4 Mo de flash et
sans PSRAM (MAC `08:b6:1f:bc:8e:30`) est affectée au moteur spectral. Le banc
`engine_lab_spectral_esp32` partage ses quatre voix entre les deux cœurs.

| Partiels/voix | Oscillateurs | Charge temps réel | Statut |
|---:|---:|---:|---|
| 16 | 64 | 64,0 % | cible sûre retenue |
| 24 | 96 | 80,1 % | expérimental, marge insuffisante |
| 32 | 128 | 96,1 % | hors cible avec transport/contrôle |
| 48 | 192 | 128,2 % | hors temps réel |
| 64 | 256 | 160,4 % | hors temps réel |

Configuration figée pour l'intégration future : **4 voix, 16 partiels par
voix**. Le banc est validé par USB seulement. Son câblage audio et son mode
I2S attendent la décision d'architecture multi-module (TDM, seconde interface
SAI ou agrégateur) après l'intégration du premier slot granulaire.

### Validation I2S DMA du moteur spectral

Après alignement des deux cartes sur Arduino-ESP32 3.3.11, le moteur 4×16 a
tourné deux fois pendant 30 secondes sur GPIO26/GPIO25/GPIO22 :

- 10335 blocs stéréo de 128 échantillons ;
- 0 bloc en retard et 0 écriture DMA incomplète ;
- pire rendu : 2051 µs au premier passage, 2036 µs au second ;
- budget par bloc : 2902 µs, soit environ 29 % de marge au pire ;
- heap interne libre après matrice : environ 312 Ko.

Le firmware autonome du second moteur est donc validé. Cela ne valide pas
encore la future réception de son flux par le S3.
