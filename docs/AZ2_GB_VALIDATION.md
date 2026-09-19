# AZ-2 — Validation Game Boy / LSDJ sur matériel

Ce document est la fiche de test officielle du sous-système GB/GBC. Une fonction n'est considérée comme validée qu'avec un résultat reproductible sur la machine réelle.

## Informations de session

- Date :
- SHA Git :
- Firmware ESP32-S3 :
- Firmware Teensy 4.1 :
- Carte écran :
- PSRAM détectée :
- Carte SD ESP32 (marque / taille / FAT32) :
- Carte SD Teensy :
- DAC :
- Alimentation :
- ROM de test / version :
- SHA-256 de la ROM de test :
- Durée :

Ne pas ajouter de ROM commerciale au dépôt. Une empreinte et le nom/version suffisent.

## Priorité de la campagne actuelle : jouer + enregistrer, sans tracker/LSDJ

**Parcours de validation MVP :** [décision CONSOLE + CAPTURE](AZ2_GB_ROADMAP_IMPLEMENTATION.md#décision-produit--mvp-console--capture-19-septembre-2026). Les tests LSDJ et la coexistence avec le tracker sont différés ; ils ne bloquent pas la qualification du jeu et de la capture.

| Étape sur prototype réel | Résultat attendu | Statut |
| --- | --- | --- |
| Démarrer une ROM de test GB puis une GBC | Contrôles et image stables ; son audible au DAC | NOT RUN |
| Jouer 5 min sans enregistrer | Cadence, latence des boutons et son de référence notés | NOT RUN |
| Pendant une partie, appuyer sur l'encodeur 0 (REC) | Accusé `REC:STARTED`, indicateur REC ; le jeu ne s'arrête pas | NOT RUN |
| Jouer et enregistrer 10 à 20 s, puis appuyer de nouveau | `REC:STOPPED` avec chemin sur la SD Teensy, fichier WAV lisible sur ordinateur | NOT RUN |
| Écouter le WAV et comparer à la sortie DAC | Capture du son de la ROM sans blancs, distorsions ou décalages anormaux | NOT RUN |
| Laisser une deuxième prise atteindre 30 s | Arrêt automatique propre, fichier valide, pas d'écrasement de la première prise | NOT RUN |
| Reprendre le jeu, faire D (SAVE), puis C (quitter) | Sauvegarde cartouche restaurable, partie/ROM conservée en cas d'erreur SD | NOT RUN |
| SD Teensy absente/pleine, coupure pendant capture | Erreur REC explicite ; les anciens WAV et sauvegardes restent intacts | NOT RUN |

**À consigner pour chaque prise :** SHA des deux firmwares, ROM et empreinte, mode GB/GB Color, durée jouée et enregistrée, FPS/missed frames, diagnostics UART/audio, taille du WAV, statut `sampler_patch`, observations à l'écoute et éventuelles erreurs SD. Un test `PASS` exige la preuve sur carte réelle : la compilation CI ne suffit pas.

## Test 1 — Boot et liaison double firmware

1. Compiler `master_teensy` et `screen_esp` depuis le même SHA.
2. Flasher les deux cartes.
3. Vérifier que l'écran démarre, que les commandes physiques répondent et que les logs ne contiennent pas d'erreur de protocole.
4. Vérifier les moteurs audio et le DAC avant d'ouvrir le mode Jeux.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 2 — Cadence GB

Lancer une ROM libre ou de test et laisser tourner au moins 30 minutes.

À relever dans les logs `GB:PERF` et dans la bande supérieure :

- FPS moyen :
- FPS minimum observé :
- frame_us_avg :
- frame_us_max :
- missed frames :
- autosave failures :
- température / comportement anormal éventuel :

Cible : cadence proche de 59,7275 Hz, aucun retard durable, aucun blocage de l'UI.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 3 — Commandes

Vérifier :

- Croix : haut / bas / gauche / droite.
- A / B.
- Encodeur SELECT.
- Encodeur START.
- D : sauvegarde manuelle sans quitter le jeu.
- C : sortie propre avec sauvegarde.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 4 — Sauvegarde SRAM

Créer une progression de test, appuyer sur D, quitter avec C puis redémarrer.

Vérifier :

- présence du `.sav`,
- taille attendue,
- restauration correcte,
- absence de `GB:SAVE_*_ERROR`,
- compteur autosave failure à zéro.

Faire ensuite 100 cycles sauvegarde / sortie / rechargement sur une copie de test.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 5 — Injection de pannes SD

Utiliser uniquement une sauvegarde de test.

Scénarios :

1. SD retirée / indisponible au moment d'un SAVE NOW.
2. Échec de sauvegarde au moment de quitter.
3. `.sav` tronqué avec `.bak` valide.
4. `.sav` absent avec `.bak` valide.
5. fichier temporaire incomplet.
6. coupure contrôlée pendant la rotation `.tmp/.sav/.bak`.

Attendu : aucune perte silencieuse ; si la sauvegarde échoue, la ROM reste chargée et l'UI affiche l'erreur.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 6 — Jeux DMG/CGB

| Test | Durée | Image | Contrôles | Audio | Save | Perf | Résultat |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| Mooneye / test libre | 30 min |  |  |  |  |  |  |
| dmg-acid2 | 10 min |  |  |  | n/a |  |  |
| Tetris DMG | 30 min |  |  |  |  |  |  |
| Super Mario Land | 30 min |  |  |  |  |  |  |
| Tetris DX | 30 min |  |  |  |  |  |  |
| Super Mario Bros Deluxe | 30 min |  |  |  |  |  |  |

## Test 7 — LSDJ

Durée cible : 2 h.

Vérifier :

- édition,
- lecture,
- changement de patterns,
- sauvegarde D,
- autosave,
- sortie C,
- restauration bit à bit du morceau,
- stabilité audio,
- coexistence avec le moteur audio AZ-2.

Le son actuel GB est encore le profil V1 mono PCM8 / 14 kHz. La qualité stéréo future ne doit pas être évaluée comme déjà livrée.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Test 8 — Capture WAV

Vérifier une capture courte et une capture longue :

- fichier créé,
- taille cohérente,
- header WAV valide,
- lecture sur ordinateur,
- aucun écrasement si la banque est pleine,
- pas de corruption après STOP.

Résultat : PASS / FAIL / NOT RUN  
Notes :

## Rapport final

- PASS :
- FAIL :
- NOT RUN :
- Régressions :
- Logs joints :
- Vidéo / photos :
- Prochaine action :

Un test PASS doit toujours indiquer le SHA exact des deux firmwares.
