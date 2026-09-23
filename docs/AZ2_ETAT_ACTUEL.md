# AZ-2 — état actuel vérifié au 23 septembre 2026

Ce document est la source de vérité courte du projet actif. Les audits plus
anciens décrivent leur révision et ne doivent pas servir à annoncer l'état
présent.

## Révision et matériel actifs

- branche : `rack-moteurs-externes` ;
- Teensy 4.1 : séquenceur, moteurs locaux, maître I2S, mixage, DAC, SD et MIDI ;
- ESP32-S3 VIEWE : écran 480×480 et interface ;
- ESP32-S3 N16R8 : moteur GRANULAR, PSRAM et agrégation audio ;
- ESP-WROOM-32D : moteur SPECTRAL ;
- PCM5102A : sortie audio finale du Teensy.

Le prototype actif comporte quatre cartes programmables, plus le DAC.

## Neuf moteurs sélectionnables

| ID | Moteur | Emplacement | État |
|---:|---|---|---|
| 0 | DEXED | Teensy | intégré ; qualité à continuer de qualifier |
| 1 | EPIANO | Teensy | intégré |
| 2 | BRAIDS | Teensy | intégré |
| 3 | KARPLUS | Teensy | intégré |
| 4 | ANALOG | Teensy | intégré |
| 5 | SAMPLER | Teensy/SD/PSRAM | intégré, navigateur à approfondir |
| 6 | DRUM | Teensy | intégré |
| 7 | GRANULAR | ESP32-S3 N16R8 | intégré au rack, presets et PATCH |
| 8 | SPECTRAL | ESP-WROOM-32D | intégré au rack, presets et PATCH |

GRANULAR et SPECTRAL partagent l'horloge I2S du Teensy. Le S3 agrège leur
audio puis le Teensy le mélange avant le PCM5102A. Le sample granulaire est lu
sur la SD du Teensy et transféré dans la PSRAM du S3 ; le granulaire n'a pas
de carte SD.

## Interface et contrôles

- tracker huit pistes, MOTEURS, PATCH, MIXER, SONG et projets présents ;
- les deux moteurs externes ont huit presets et leurs paramètres ;
- l'oscilloscope PATCH observe l'entrée audio du rack ;
- la liste de presets suit la convention A pour entrer/sortir de l'édition ;
- le rendu d'un écho PATCH est différé afin de ne plus retarder BTN/NAV.

## MIDI

- USB MIDI présent ;
- firmware MIDI DIN prêt sur Serial8 RX pin 34 à 31 250 bit/s via 6N138 ;
- canaux 1–8 vers pistes 1–8, Note On/Off, vélocité et running status ;
- CC120/123, Start, Stop et PANIC pris en charge ;
- câblage 6N138 et MIDI Clock 24 PPQN non encore validés sur banc.

## Émulation GB/GBC : statut exact

Walnut-CGB, GNUBOY et plusieurs briques de frontend sont présents dans le
dépôt. Cela ne constitue pas un émulateur utilisable de bout en bout.

- aucun émulateur GB ou GBC n'est déclaré fonctionnel sur l'AZ-2 ;
- aucune matrice ROM vidéo + commandes + audio + sauvegarde n'est validée ;
- cadence, fidélité APU, RTC et sauvegardes restent à qualifier ;
- la fonction JEUX et la capture depuis un jeu sont expérimentales.

Toute phrase plus optimiste dans un audit antérieur doit être lue comme une
observation ou une ambition propre à cette ancienne révision.

## Prochain ordre de travail

1. valider la réactivité des contrôles après le rendu PATCH différé ;
2. tester GRANULAR et SPECTRAL séparément puis ensemble dans un morceau ;
3. finaliser sélection, mémoire et transfert des samples granulaires ;
4. tester le MIDI DIN avec le 6N138, puis seulement la clock 24 PPQN ;
5. lancer un test rack prolongé avec métriques CPU, I2S et niveaux ;
6. reprendre l'émulation séparément sans la déclarer fonctionnelle avant une
   validation complète.
