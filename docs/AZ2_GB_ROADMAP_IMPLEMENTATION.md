# AZ-2 — Feuille de route Game Boy / LSDJ

Date : 2026-09-19. Branche de travail : main. Cible : ESP32-S3 (GB/GBC, écran, SD) + Teensy 4.1 (audio et DAC). Pas de module supplémentaire dans AZ-2. L'AZ-3 et son rack d'ESP ne font pas partie de cette livraison.

## Décision produit — MVP CONSOLE + CAPTURE (19 septembre 2026)

**Étude de faisabilité :** [audit des racks logiciels, gains réalistes, tests A/B et transition à étapes réversibles](AZ2_AUDIT_RACKS_CONSOLE_CAPTURE.md). Commencer par mesurer puis corriger jeu + REC ; la refonte globale est conditionnelle, pas une dépendance du MVP.

**Priorité actuelle : jouer à une ROM GB/GBC à sa cadence native, entendre le son sur le DAC et capturer ce son en WAV pendant la partie.** Le tracker et l'émulateur n'ont pas besoin d'être synchronisés pour atteindre cet objectif. L'approche « racks logiciels » est conservée comme **optimisation ciblée**, pas comme condition préalable ni comme refonte de toute la machine.

### Parcours utilisateur minimal

1. Depuis **JEUX**, choisir une ROM sur la SD de l'ESP32-S3 et démarrer la partie.
2. Jouer avec la croix, A/B et les commandes START/SELECT ; le son GB arrive au Teensy puis au PCM5102A.
3. Appuyer sur le **bouton poussoir de l'encodeur 0** pour démarrer la capture ; afficher REC seulement après l'accusé `REC:STARTED` du Teensy. Appuyer de nouveau pour arrêter ; le garde-fou existant arrête aussi la prise à **30 secondes**.
4. Le Teensy ferme et vérifie le fichier `/samples/SAMPLE_###.wav` sur **sa propre SD** (WAV PCM mono 16 bits, 14 kHz). Il signale le chemin, la durée en échantillons et si le patch dynamique `SAMPLER / GB Capture` a bien été chargé en PSRAM.
5. Continuer à jouer, relancer une prise ou quitter proprement. **D** sauvegarde la cartouche ; **C** quitte sous réserve d'une sauvegarde réussie. La capture ne doit jamais écraser un WAV existant.

Le bouton REC doit rester accessible pendant le jeu et **ne doit pas remplacer A/B, START/SELECT, C ou D**. L'entrée tactile peut proposer REC/STOP également, mais ne doit pas recouvrir l'image ou intercepter les commandes du jeu sans test sur écran réel.

### Ce qui est déjà dans le code, mais reste à qualifier sur matériel

- [x] Chargement ROM, image, contrôles, audio GB V1 mono PCM8/14 kHz ESP32 → Teensy → DAC.
- [x] `REC:START`/`REC:STOP`, indicateur confirmé par le Teensy, capture WAV de 30 s maximum, noms sans écrasement et signalement des erreurs.
- [x] Lecture du dernier WAV valide dans un **seul** slot GB Capture en PSRAM ; c'est utile après la prise, **pas requis pour jouer et enregistrer**.
- [ ] Tester sur le vrai prototype le cycle complet jeu → REC → jeu sans arrêt → STOP → lecture du WAV sur ordinateur, et comparer la sortie DAC au fichier.
- [ ] Mesurer la cadence GB pendant enregistrement, la durée réelle du WAV, les underruns et pertes UART, les pics de charge CPU, les accès SD et la réactivité des boutons.
- [ ] Vérifier les erreurs SD absente/pleine, arrêt automatique à 30 s, réenregistrement et sauvegardes cartouche pendant/après capture.
- [ ] Ne déclarer les jeux compatibles qu'après tests reproductibles par ROM ; **LSDJ n'est pas une condition de sortie du MVP**.

### Optimisation ciblée, uniquement si les mesures le justifient

Pendant une partie, suspendre les rendus d'interface non visibles (scope, animations, écran de veille) et réduire les tâches non indispensables **sans interrompre** l'horloge GB, les commandes, la réception audio, le DAC ni l'enregistrement. Le Teensy possède déjà une connexion dynamique des moteurs sélectionnés ; ne pas refondre son graphe ou créer un nouveau rack système sans profil CPU/mémoire montrant un bénéfice. Garder le profil V1 fiable comme référence ; V2 stéréo et sortie DAC stéréo sont hors MVP.

Implémentation AZ-2 : pendant une ROM active, le scope, les redraws sampleur,
la lecture tactile I²C et la veille sont suspendus ; `readTeensyStatus()` et
les commandes série restent actifs.

### Idées ultérieures — volontairement différées

- Étudier finement **LSDJ** (fonctionnement, sauvegardes, timing et éventuelle synchronisation) après stabilisation jeu + capture. Ne pas conditionner la capture à la synchronisation tracker/GB.
- Mode HYBRID jeu + tracker, déclenchement de samples depuis un pattern, enregistrement multipiste et édition de banque.
- Racks logiciels configurables à grande échelle, priorités de charge automatiques, interface d'édition complète du sample et qualité audio supérieure.

Ces idées ne doivent pas retarder les tests et corrections du parcours minimal ci-dessus.

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
- [x] Ne plus tronquer silencieusement les noms du navigateur : les noms complets jusqu'à 87 octets sont conservés, les noms trop longs sont ignorés avec diagnostic. [x] Navigateur porté à 100 ROMs et tri alphabétique. [ ] Ajouter si besoin un stockage dynamique pour les noms >87 octets / collections >100.
- [x] Charger la nouvelle ROM en staging : ouverture, taille, allocation PSRAM et lecture complète sont validées avant de sauvegarder/décharger le jeu courant. L'initialisation du cœur reste postérieure au basculement et doit encore être couverte par tests de cartouches invalides.
- [x] RTC MBC3 persisté séparément dans un fichier `.rtc` versionné avec CRC32, `.tmp/.bak`, récupération du backup et blocage de sortie en cas d'échec. Si l'heure système ESP32 est valide, le temps écoulé hors tension est rattrapé ; sinon l'état RTC exact est restauré sans inventer d'heure. [ ] Qualifier sur vraies cartouches/ROMs RTC et coupures.

- [x] Refuser de démarrer une cartouche dont les sauvegardes existantes sont invalides (au lieu de lancer une SRAM vierge). Après récupération .bak, le premier enregistrement conserve cette copie valide.
- [x] Empêcher la capture Game Boy d'écraser SAMPLE_999.wav quand la banque est pleine ; contrôler les écritures WAV et signaler les fichiers incomplets.

### Lot 1 — Instrumenter et qualifier la cadence (à implémenter/valider)

- [x] Instrumentation de base intégrée : FPS observé, temps moyen/max de frame, p99 du travail émulateur+audio, missed frames, total frames et échecs d'autosave, avec logs série. Le bandeau graphique est désactivé en jeu pour éviter un rafraîchissement parasite. [ ] Ajouter timings LCD/APU/SD/UART et qualification sur matériel.
- [ ] Vérifier la cadence 59,7275 Hz sur machine pendant 30 minutes ; capturer aussi les ralentissements, pas seulement la moyenne.
- [ ] Bench A/B gb_run_frame() et gb_run_frame_dualfetch() avec tests CPU/timers/interruptions/DMA.
- [ ] Vérifier le vrai rendu écran 60 Hz et les transactions par bande ; ajouter un double buffer uniquement après contrôle des échanges avec le panneau.
- [ ] Tester Mooneye, dmg-acid2, jeux DMG/CGB et documenter versions, limites de licence et résultats reproductibles.

### Lot 2 — Audio LSDJ (architecture à développer)

- [ ] Caractériser le MiniGB APU présent : timing des écritures, latence, stéréo, dérive de pitch, charge CPU.
- [ ] Prototyper Gb_Snd_Emu (écritures APU horodatées) dans une branche d'essai ; comparer avec SameBoy, vérifier licences et budget ESP32.
- [x] V2 pilote implémenté des deux côtés : encodeur ESP32, décodeur Teensy borné avec contrôle CRC/longueur/séquence, négociation QUERY/READY, tests natifs. Pilote PCM8 mono/14 kHz **désactivé par défaut** (flag partagé). [ ] Valider la bascule et le retour V1 sur matériel, puis implémenter une véritable sortie stéréo/32 kHz avec capture et mixage adaptés.
- [ ] Réserver bande passante UART aux commandes et tester les profils : stéréo PCM8 32 kHz (~64 ko/s sans overhead) et codec léger si 16 bits/44,1 kHz nécessaire. Le PCM stéréo 16 bits/44,1 kHz brut (~176,4 ko/s) NE tient PAS dans l'UART 921600 (~92,16 ko/s brut).
- [x] Tampon circulaire borné côté Teensy et resampling linéaire 14 kHz → 44,1 kHz intégrés. Compteurs paquets reçus, longueurs invalides, timeouts et ring drops ajoutés. [x] Séquence/CRC V2 disponibles en mode pilote. [ ] Ajouter mesure de latence et compteur d'underrun réel.
- [ ] Mesurer l'audio pendant lecture LSDJ + 8 pistes AZ-2 + effets ; ne pas déclarer la qualité haute fidélité avant essai d'écoute et mesures.

### Lot 3 — Sauvegarde sans gel de frame

- [ ] Copier un snapshot SRAM cohérent et l'écrire hors boucle critique (tâche basse priorité, accès SD sérialisé avec le rendu/lecteur de ROM).
- [ ] Séquencer les snapshots avec génération/dirty flag : si la SRAM change pendant une écriture, ne pas effacer l'indicateur dirty correspondant.
- [x] Sauvegarde manuelle sur D, retour d'erreur visible et retry sans quitter la partie. [ ] Tests 100 cycles et récupération après coupure au pire moment.

### Lot 4 — Console / LSDJ / Sampler

- [ ] Menu Console : ROMs paginées, noms distincts, sauvegarde explicite et stats sont désormais présents en base. Restent boutons configurables, palette/volume dédiés et polish de l'interface.
- [ ] Mode LSDJ Studio : combos Start/Select ergonomiques, sortie sûre, mixage stéréo, lecture simultanée.
- [ ] Capture audio GB depuis le mixeur AVANT effets si choix dry et APRÈS effets si choix wet ; enregistrer WAV valide avec durée/échantillonnage/canaux.
- [x] Parcours capture → sampleur intégré : le dernier WAV GB valide est chargé en PSRAM du Teensy comme patch dynamique `SAMPLER / GB Capture`, avec sample rate source respecté et restauration automatique au boot. [ ] Qualifier sur matériel et ajouter navigateur/gestion de plusieurs captures utilisateur.
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
