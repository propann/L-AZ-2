# AZ-2 — Feuille de route Game Boy / LSDJ

Date : 2026-09-19. Branche de travail : main. Cible : ESP32-S3 (GB/GBC, écran, SD) + Teensy 4.1 (audio et DAC). Pas de module supplémentaire dans AZ-2. L'AZ-3 et son rack d'ESP ne font pas partie de cette livraison.

## État et critère d'achèvement

Ce document distingue **code intégré**, **à implémenter**, et **à valider sur matériel**. Ne pas annoncer une ROM « compatible » parce qu'elle démarre ou qu'un firmware compile. Le mode Console doit fonctionner à la vitesse matérielle Game Boy indépendamment du BPM du tracker.

### Lot 0 — Protections immédiates (intégré, contrôle matériel restant)

- [x] Refuser les noms de ROM vides ou contenant un séparateur de chemin.
- [x] Refuser un chemin tronqué, une ROM trop courte ou dépassant le plafond mémoire actuel (8 Mio).
- [x] Vérifier l'échec de suppression/renommage des fichiers temporaires et backups, et conserver le .bak en cas d'échec de restauration.
- [x] Refuser le lancement si la taille de la RAM cartouche est inconnue ou si son allocation échoue.
- [ ] Valider sur ESP32-S3 la compilation, puis une série de sauvegardes/restaurations, avec coupures simulées à chaque transition .sav/.tmp/.bak.
- [x] Ne plus libérer une cartouche modifiée si son enregistrement échoue : gbUnload() retourne false, le changement de page et de ROM est bloqué, message SD SAVE ERROR et nouvel essai possible par C. À qualifier matériel.
- [ ] Remplacer la rotation à deux noms .sav/.bak par un journal de génération vérifié (taille + CRC/version), récupération du fichier le plus récent valide ; éviter la perte de l'ancien backup avant confirmation de la nouvelle version.
- [ ] Corriger les chemins des noms de ROM affichés tronqués (40 caractères) : stocker un identifiant/chemin non tronqué, afficher seulement un libellé raccourci, gérer pagination et homonymes.
- [ ] Charger et valider une nouvelle ROM sans interrompre le jeu courant en cas d'ouverture/lecture/allocation échouée (staging et séparation état candidat/état actif).
- [ ] Sauvegarder et restaurer le RTC MBC3 ; ne pas confondre sauvegarde SRAM et état complet de l'émulateur.

### Lot 1 — Instrumenter et qualifier la cadence (à implémenter/valider)

- [ ] Enregistrer par ROM/firmware FPS logique, FPS affiché, temps moyen/max/p99 de frame, LCD, APU, SD et UART, ainsi que frames en retard.
- [ ] Vérifier la cadence 59,7275 Hz sur machine pendant 30 minutes ; capturer aussi les ralentissements, pas seulement la moyenne.
- [ ] Bench A/B gb_run_frame() et gb_run_frame_dualfetch() avec tests CPU/timers/interruptions/DMA.
- [ ] Vérifier le vrai rendu écran 60 Hz et les transactions par bande ; ajouter un double buffer uniquement après contrôle des échanges avec le panneau.
- [ ] Tester Mooneye, dmg-acid2, jeux DMG/CGB et documenter versions, limites de licence et résultats reproductibles.

### Lot 2 — Audio LSDJ (architecture à développer)

- [ ] Caractériser le MiniGB APU présent : timing des écritures, latence, stéréo, dérive de pitch, charge CPU.
- [ ] Prototyper Gb_Snd_Emu (écritures APU horodatées) dans une branche d'essai ; comparer avec SameBoy, vérifier licences et budget ESP32.
- [ ] Définir un protocole binaire V2 versionné : longueur >= 16 bits, séquence, CRC, format/rate/canaux, resynchronisation et compteurs ; ne pas casser les commandes existantes.
- [ ] Réserver bande passante UART aux commandes et tester les profils : stéréo PCM8 32 kHz (~64 ko/s sans overhead) et codec léger si 16 bits/44,1 kHz nécessaire. Le PCM stéréo 16 bits/44,1 kHz brut (~176,4 ko/s) NE tient PAS dans l'UART 921600 (~92,16 ko/s brut).
- [ ] Tampon circulaire borné côté Teensy, resampling avec interpolation, compteurs underrun/overflow/perte/latence ; traitement audio en priorité sur les animations UI.
- [ ] Mesurer l'audio pendant lecture LSDJ + 8 pistes AZ-2 + effets ; ne pas déclarer la qualité haute fidélité avant essai d'écoute et mesures.

### Lot 3 — Sauvegarde sans gel de frame

- [ ] Copier un snapshot SRAM cohérent et l'écrire hors boucle critique (tâche basse priorité, accès SD sérialisé avec le rendu/lecteur de ROM).
- [ ] Séquencer les snapshots avec génération/dirty flag : si la SRAM change pendant une écriture, ne pas effacer l'indicateur dirty correspondant.
- [ ] Ajouter sauvegarde manuelle, retour d'erreur visible, retry, tests 100 cycles et récupération après coupure au pire moment.

### Lot 4 — Console / LSDJ / Sampler

- [ ] Menu Console : ROMs paginées, noms distincts, boutons configurables, sauvegarde explicite, palette, volume, stats.
- [ ] Mode LSDJ Studio : combos Start/Select ergonomiques, sortie sûre, mixage stéréo, lecture simultanée.
- [ ] Capture audio GB depuis le mixeur AVANT effets si choix dry et APRÈS effets si choix wet ; enregistrer WAV valide avec durée/échantillonnage/canaux.
- [ ] Envoi au sampleur utilisateur (le sampleur actuel ne dispose pas encore du parcours complet de banque/capture).
- [ ] Synchronisation LSDJ via émulation précise du port série GB et pont horodaté vers clock/MIDI ; définir maître/esclave ; ne jamais asservir directement la cadence CPU GB au BPM.

## Matrice d'acceptation matériel

| Scénario | Durée minimum | Critère |
| --- | --- | --- |
| Tetris DMG | 30 min | vitesse normale, commandes stables, image sans saccades |
| Super Mario Land | 30 min | scroll, sprites, physique, son |
| Tetris DX / Mario Bros Deluxe | 30 min chacun | mode CGB, couleurs, scènes exigeantes |
| LSDJ édition et lecture | 2 h | sauvegarde restaurée bit à bit, pas d'interruption audio |
| LSDJ + pistes et effets AZ-2 | 30 min | aucun underrun/perte UART en conditions testées |
| SD save/load, extinctions simulées | 100 cycles | au moins un .sav ou .bak valide et identifié, jamais de perte silencieuse |

Pour chaque test : SHA firmware, modèle exact ESP/PSRAM, carte SD, ROM de test (empreinte sans redistribuer de ROM commerciale), logs, mesures et statut PASS/FAIL/NOT RUN.

## Définition des releases

- **GB alpha** : Lot 0 compilé, tests locaux réussis et absence de pertes silencieuses.
- **GB beta** : Lot 1 qualifié, sauvegardes robustes, menu et matrice de jeux reproductible.
- **LSDJ Studio beta** : Lot 2 + 3 réussis sur matériel, son stéréo et restauration validés.
- **GB Sampler** : Lot 4, capture WAV et lecture des banques utilisateur vérifiées.

Le commit de documentation n'est pas une preuve de réussite des tests physiques. Les lots 1–4 sont planifiés, non livrés.
