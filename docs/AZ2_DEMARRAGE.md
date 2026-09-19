# AZ-2 — Premier démarrage et mise à jour des deux firmwares

Ce guide décrit la procédure de préparation, pas un résultat de test matériel. **Les deux firmwares forment un système couplé.** Ils partagent `lib/AZ2_Protocol/AZ2_Protocol.h` et échangent commandes, scope et audio GB sur UART.

## 1. Avant de brancher

- Vérifier le modèle exact de l'écran ESP32-S3 (VIEWE UEDX48480040E-WB), Teensy 4.1 et DAC PCM5102A avec [le câblage maître](AZ2_CABLAGE_MASTER.md).
- Vérifier tensions, masse commune, brochage TX↔RX et absence de court-circuit, **alimentation hors tension**. Ne jamais raccorder une sortie 5 V sur une entrée non tolérante 5 V.
- Préparer des cartes SD FAT32 adaptées aux fonctions de chaque carte et une copie séparée de tous les projets, patches, ROMs et `.sav` existants. Ne pas écraser ces données lors d'un flash.
- Conserver les deux anciens binaires fonctionnels et le SHA Git exact correspondant, afin de pouvoir revenir à une paire compatible.

## 2. Obtenir et tester une révision unique

```bash
git clone https://github.com/propann/L-AZ-2.git
cd L-AZ-2
git rev-parse HEAD
python -m pip install platformio==6.1.19
python tools/check_firmware_contract.py
pio test -e native
pio run -e master_teensy -e screen_esp
```

`platformio.ini` épingle Teensy et pioarduino pour les deux cibles. Ne modifier ni `AUDIO_SAMPLE_RATE` ni `kGbAudioSampleRate` séparément ; le `static_assert` côté émulateur doit faire échouer la compilation si leurs valeurs divergent.

## 3. Flasher la paire cohérente

```bash
pio run -e master_teensy -t upload
pio run -e screen_esp -t upload
```

Adapter les ports et l'alimentation à chaque carte, et ne pas supposer que le port d'upload Teensy est le port de l'ESP32. Éviter de laisser le master et l'écran actifs avec deux versions incompatibles entre les deux uploads. Ne pas lancer un enregistrement/sauvegarde pendant une mise à jour. Ne flasher qu'après vérification du câblage et de la compilation des deux cibles.

## 4. Contrôles après flash

1. Vérifier démarrage et connexion des deux cartes sans bruit anormal ni surchauffe.
2. Vérifier croix, A/B/C/D, encodeurs, tactilité et affichage.
3. Vérifier un patch de chaque moteur, le volume du DAC, le tracker et le MIDI.
4. Vérifier que le mode Jeux démarre une ROM de test libre et que les commandes atteignent l'émulateur.
5. Vérifier que le son GB arrive au Teensy sans corrompre les messages de contrôle et sans blocage de la navigation.
6. Sauvegarder, quitter, redémarrer et restaurer un fichier de test. Vérifier sa taille et son contenu ; ne pas utiliser la seule copie d'un morceau LSDJ important.
7. Consigner SHA, versions des deux firmwares, SD, ROM de test, durée, logs d'erreur et observations.

**Si une étape échoue :** ne pas flasher au hasard un seul firmware, couper proprement l'alimentation, préserver les SD et revenir à la dernière paire de binaires fonctionnelle après diagnostic.

## Règle de développement

Toute évolution de message ou d'audio qui touche `AZ2_Protocol.h` doit inclure (1) l'émetteur, (2) le récepteur, (3) les tests natifs de framing, (4) une compilation des deux environnements et (5) un plan de rollback. Les changements de format incompatibles nécessitent une négociation de version ou un déploiement atomique explicite. Le profil actuel du son GB reste **V1 mono 8 bits / 14 kHz**, pas la future version stéréo.
