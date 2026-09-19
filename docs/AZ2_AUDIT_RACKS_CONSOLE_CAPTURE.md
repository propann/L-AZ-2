# AZ-2 — Audit de faisabilité des racks logiciels et transition mesurée

**Date : 19 septembre 2026 · Branche : main · Périmètre : AZ-2 à deux cartes.**

**Décision : OUI à un *profil d'exécution CONSOLE + CAPTURE* léger et réversible ; NON, à ce stade, à une refonte générale en racks, à un ordonnanceur réparti entre les deux cœurs ESP32 ou à la synchronisation tracker/LSDJ.** Le bénéfice de performance n'est **pas chiffré** : aucune mesure A/B sur le prototype réel n'est disponible dans cet audit. La transition ci-dessous ne devient plus ambitieuse que si les tests démontrent un gain. La priorité produit reste **jouer une ROM normalement, entendre son audio et en extraire un WAV pendant qu'on joue**.

## 1. Constats vérifiés dans le code actuel

| Zone et chemin | État réel | Ce qu'un rack peut faire / ne peut pas faire |
| --- | --- | --- |
| ESP32 `src_esp32/az2_screen/main.cpp::loop()` | Reçoit tout le trafic série Teensy, sonde le tactile, gère veille/toast et ordonnance une frame GB ~16 742/16 743 µs. Mesure FPS/temps moyen/max et frames manquées. | Une politique d'exécution peut **éviter du travail périphérique** pendant le jeu. Elle n'accélère pas intrinsèquement le cœur GB. Isoler des fichiers/classes sans modifier la charge d'exécution ne fait rien gagner. |
| ESP32 `gb_emulator.cpp::gbRunFrame()` | `gb_run_frame_dualfetch()` puis `sendGbAudioPacket()`; les lignes LCD sont rendues via callback ; autosave SRAM/RTC toutes les 30 s. | Le cœur, l'APU, le LCD, l'UART et les sauvegardes restent obligatoires. Le coût du transfert écran et de la SD peut créer des pics ; **instrumenter avant de déplacer/paralleliser**. |
| Veille ESP32 | Le test d'inactivité et l'appel `screensaverStep()` sont encore actifs lorsque la page Jeux est affichée ; l'exécution des frames est conditionnée par `!screensaverActive`. | **Défaut prioritaire : la veille peut arrêter le jeu à l'issue du délai sans interaction**, même si une ROM est chargée. La désactiver tant que le jeu est chargé doit précéder toute architecture de racks ; une veille décorative ne doit ni figer le jeu ni écraser l'affichage GB. |
| Lecture série ESP32 | `readTeensyStatus()` draine `Serial1` avant la frame ; `handleTeensyLine()` copie chaque ligne vers USB/log. | Un arriéré série, des logs répétés ou un long callback retardent la frame. La lecture des commandes/états REC et l'anti-perte restent **obligatoires**. Mesurer la profondeur du backlog et le temps de vidange avant tout plafonnement. |
| Teensy `main.cpp::loop()` | Appelle RX série, télémétrie audio, MIDI, file GB, scope, statut, contrôles. `updateSequencer()` relaie surtout les ticks de l'ISR ; `updateScope()` retourne immédiatement si inactif. | Le scope est **déjà** à activation dynamique et les moteurs non sélectionnés sont **déjà** déconnectés du graphe audio. Les envelopper dans des racks ne garantit donc pas un gain. Maintenir les contrôles, le parseur audio et le DAC actifs. |
| Capture Teensy | `handleGbAudioPacket()` capture les échantillons reçus **avant** le rééchantillonnage ; `gbRecPush()` flush périodiquement un tampon de 512 samples vers la SD ; `gbRecStop()` finalise le WAV puis appelle synchroniquement le chargement du dernier sample en PSRAM. | Un enregistrement WAV peut prendre du temps SD au milieu du traitement série ; la finalisation puis le rechargement PSRAM peuvent bloquer le loop Teensy alors que le jeu/flux audio continuent. **C'est une cible d'optimisation plus directe qu'un rack complet.** |
| Flux GB et mémoire audio | UART à 921600 bauds ; V1 mono PCM8 / 14 kHz actif par défaut ; file audio Teensy de 2048 échantillons de sortie (~46 ms à 44,1 kHz), compteur `gbRingDrops`, compteurs RX V1/V2. `AudioMemory(700)` partagé. | La carte Teensy ne peut pas « éteindre le son » pour accélérer le jeu : elle doit lire les paquets, produire l'audio et sauvegarder le WAV. Arrêter un moteur peut économiser du calcul mais n'augmente pas automatiquement les tampons audio ni la bande passante UART. |
| Interface physique | La croix, A/B/C/D, START/SELECT et le poussoir REC arrivent via Teensy/liaison vers ESP32 ; retour tactile également disponible sur l'écran. | Le profil CONSOLE doit préserver **les deux modes de contrôle** et les accusés REC. Un changement de mode ne doit pas consommer un appui A/B destiné au jeu. |

**Défauts connexes à examiner indépendamment des racks :** dans `handleGbAudioPacket()`, le produit signé `delta * gbResamplePhaseQ16` est calculé en 32 bits avant le décalage ; son amplitude peut déborder. Utiliser/valider une multiplication 64 bits et tester les échantillons extrêmes avant d'attribuer une distorsion à une surcharge. Dans `gbRecStop()`, le rechargement synchronisé du dernier WAV dans un buffer PSRAM partagé est à séparer de l'achèvement de la capture ; tester les lectures simultanées par le sampleur avant un remplacement à chaud.

## 2. Ce qu'on peut gagner — sans inventer des chiffres

| Mesure | Gain envisageable | Limite / coût |
| --- | --- | --- |
| Temps de boucle ESP32 et retard de frame | Éviter redraws, logs/rafraîchissements inutiles, veille et traitements non visibles pendant CONSOLE | Si le temps est déjà dominé par `gb_run_frame_dualfetch` + LCD, le gain sera faible ; mesurer séparément cœur/LCD/APU/UART/SD. |
| Risque de décrochage audio pendant REC/STOP | Échelonner les tâches SD et différer la mise à jour du slot sampleur hors chemin audio critique | Requiert une gestion explicite des états, des buffers et des erreurs ; **ne jamais** supprimer un échantillon ou annoncer REC réussi avant fermeture/validation WAV. |
| Charge Teensy et `AudioMemory` | Seul le travail audio effectivement actif peut être diminué ; scope et moteurs sont déjà à connexion dynamique | Un profil CONSOLE peut arrêter le séquenceur **uniquement avec accord explicite** de l'utilisateur ; ne pas perdre un morceau, ses réglages ni l'état des effets ; éviter la reconfiguration fréquente du graphe. |
| PSRAM ESP32 / Teensy | ROM staging et slot GB Capture utilisent déjà des allocations dédiées ; un mode peut limiter les usages simultanés | Aucun gain de mémoire garanti sans allocations/libérations réellement mesurées ; la PSRAM des deux cartes est **physiquement séparée** et ne se mutualise pas. |
| Maintenabilité | Une table de profils et un contrat minimal rendent les transitions testables et réversibles | Une hiérarchie générale de racks, plugins, threads et messages introduirait du code, de la latence et des états de panne pour un bénéfice encore inconnu. |

**Conclusion technique conditionnelle :** les politiques « n'exécuter que ce qui est nécessaire » valent la peine **par petites modifications mesurables** ; construire d'emblée deux systèmes complets de racks ne se justifie pas pour le MVP. Le profil CONSOLE + CAPTURE n'exige **aucune nouvelle carte** ni changement du protocole audio de production.

## 3. Contrat minimal des profils

Ne pas confondre *racks matériels* (ESP32 / Teensy) et *racks logiciels*. Pour le MVP, trois profils suffisent :

- **STUDIO (existant)** : UI tracker/synthés et services musicaux actuels ; ROM non active.
- **CONSOLE (cible MVP)** : jeu GB/GBC, tactile/commandes physiques, LCD GB, audio GB → DAC, SRAM/RTC et commande REC maintenus ; animations/veille et scope hors page suspendus. Le tracker n'est **pas synchronisé** au jeu.
- **CAPTURE (sous-état de CONSOLE)** : mêmes services obligatoires + écriture WAV ; état `IDLE → START_REQUESTED → RECORDING → STOP_REQUESTED → FINALIZING → IDLE/ERROR`. Les accusés `REC:STARTED/STOPPED/ERROR` de Teensy sont source d'autorité ; l'ESP32 n'affiche pas une réussite supposée.

**Invariants de sécurité :** garder le DAC, la lecture série, le décodage audio, le poussoir REC, A/B, la croix, START/SELECT, C (sortie) et D (save). Ne jamais désactiver la gestion des sauvegardes ROM ou l'émission des erreurs SD. Ne pas décharger une ROM si `gbUnload()` refuse une sauvegarde. Avant un retour au STUDIO, finaliser ou annuler proprement une capture sans perdre le WAV existant. Désactiver l'affichage décoratif pendant une ROM chargée ; ne pas couper l'alimentation du LCD pour libérer un prétendu « rack ». Garder V1 comme profil audio de référence tant que V2 n'est pas qualifié sur matériel.

## 4. Feuille de route de transition, avec vérification et retour arrière

Chaque étape est une **modification isolée**, poussée avec son SHA, tests natifs + compilations `master_teensy` et `screen_esp`, suivi d'essais réels. Ne pas enchaîner une étape si le critère de sortie précédent échoue. Préserver la dernière paire de binaires fonctionnels et les deux SD.

### R0 — Référence reproductible, AVANT optimisation

- [ ] Figer le SHA de référence, documenter écran/PSRAM/SD/câblage/ROM libre de test, sauvegarder `.sav/.rtc` et WAV.
- [ ] Mesurer sur ROM de test **jeu seul puis jeu + REC (10, 20 et 30 s) puis STOP**, même ROM/zone/contrôles, 3 répétitions ; relever FPS observé vs cadence GB cible ~59,7275 Hz, frames manquées, temps frame moyen/max, temps de traitement UART, `GB:AUDIO_RX` (timeouts, bad_len, ring_drop), usage/max CPU et AudioMemory Teensy, écoute DAC et inspection WAV sur PC.
- [ ] Chronométrer séparément `gb_run_frame_dualfetch`, LCD, APU + émission série, autosave SD ESP32, `gbRecFlushBuf`, fermeture WAV et chargement PSRAM Teensy (diagnostic léger et **hors chemin critique**).
- [ ] Mesurer temps de réponse boutons/tactile et vérifier les erreurs SD absente/pleine. Constater explicitement si le jeu s'interrompt à l'activation de l'écran de veille.
- **Passage :** un tableau `avant` chiffré + une prise WAV lisible ou une anomalie reproduite ; sinon aucune affirmation de gain. **Retour arrière :** aucune modification fonctionnelle dans cette étape.

### R1 — Corriger le comportement CONSOLE sans nouvelle architecture

- [ ] Inhiber *l'entrée en veille* et les redraws décoratifs pendant une ROM chargée ; laisser intact le temps système et reprendre le délai d'inactivité normal à la sortie.
- [ ] À l'entrée Jeux, confirmer `SCOPE:OFF` si une source scope est active ; garantir sa restauration normale lors du retour page PATCH. Ne pas envoyer de commande de contrôle pour chaque frame.
- [ ] Ne pas dessiner d'overlay/témoin REC au milieu des 432 px de l'image GB ; préserver les entrées physiques et tactiles.
- **Vérification :** jeu seul > délai veille configuré, jeu + capture 30 s, sortie C avec SRAM/RTC sauvegardés, retour STUDIO, écran de veille à nouveau actif hors jeux. Comparer R0 à R1 (moyenne **et** pics). **Retour arrière :** revenir au SHA R0 si entrées, image, sauvegardes ou audio régressent.

### R2 — Sécuriser enregistrement et chemin audio, avant tout « rack »

- [ ] Tester/corriger l'interpolation avec une multiplication intermédiaire 64 bits ; ajouter des tests de valeurs audio extrêmes et vérifier l'absence de distorsion ajoutée.
- [ ] Instrumenter les durées de `gbRecFlushBuf` et `gbRecStop`. Si les pertes RX/audio surviennent lors des écritures, séparer réception et écriture en tampon borné, avec compteurs d'overflow, **sans bloquer une frame GB ni jeter silencieusement des samples**.
- [ ] À `REC:STOP`, finaliser et fermer le WAV en priorité, puis différer l'import WAV → PSRAM : cet import est **facultatif pour le MVP** ; l'échec d'import ne doit pas transformer une prise WAV valide en erreur de capture. Empêcher tout écrasement du buffer PSRAM lu par le moteur audio (staging ou arrêt contrôlé avant échange).
- [ ] Vérifier que 2 captures successives produisent des WAV distincts ; tester limite 30 s, interruption SD, réception après STOP, 10 cycles répétés et reprise du jeu sans blocage durable.
- **Passage :** WAV valide, durée correcte, contrôles réactifs, aucun overflow silencieux ; compteurs et latences R0/R1 comparés. **Retour arrière :** rétablir l'enregistreur actuel si régression ; ne pas activer V2 pour masquer un défaut V1.

### R3 — Profil d'exécution léger et réversible, UNIQUEMENT SI R0–R2 montrent un coût significatif

- [ ] Ajouter un état explicite `STUDIO/CONSOLE/CAPTURE` **au-dessus** des fonctions actuelles, sans les déplacer toutes ni introduire de threads ou de plugins. Un seul changement d'état à l'entrée/sortie du jeu et aux accusés REC.
- [ ] Centraliser seulement la politique des tâches facultatives : scope hors écran, veille, UI décorative, fréquence des diagnostics. Conserver en continu les chemins série/audio/inputs/sauvegardes.
- [ ] Prévoir des messages de mode partagés **uniquement si nécessaires** ; tests de protocole sur les deux firmwares, rejeu d'un ordre répété, pertes d'ACK et démarrage d'une seule carte. Ne pas casser `REC:` ni `GBV2:`.
- [ ] Le choix d'arrêter le tracker ou des moteurs existants est **explicite**, pas un arrêt automatique destructif ; les moteurs non sélectionnés sont déjà déconnectés.
- **Passage :** A/B sur le même matériel et les mêmes ROMs : gains reproductibles sur retards/pics/underruns **sans** régression commandes, WAV, SD ou restauration du mode STUDIO. **Arrêt de la refonte :** si les mesures sont identiques à l'étape R2, conserver R2 et fermer ce chantier ; le gain de maintenance seul n'autorise pas une réécriture totale.

### R4 — Options coûteuses, seulement si le profil léger ne suffit pas

- [ ] Si la lenteur vient du **LCD**, profiler le callback et les bandes de rendu, réduire les copies/transferts sans changer l'émulation ni dégrader l'image.
- [ ] Si la lenteur vient du **cœur GB/APU**, comparer optimisations localisées sur les mêmes jeux et scènes, avec tests de compatibilité/mémoire ; aucune supposition qu'un « rack » accélère le CPU émulé.
- [ ] Si la lenteur vient de **UART/SD**, tester débit et priorité des commandes, tampons bornés, découpage des écritures ; conserver la réception audio intacte. Un pinning FreeRTOS ou double cœur ESP32 n'est envisagé **qu'après** mesures des contentions PSRAM/LCD et avec stratégie de synchronisation des buffers.
- **Passage :** preuve A/B que la cause ciblée est améliorée et aucun échec de compatibilité/sauvegarde/capture. **Retour arrière :** option désactivable et ancien firmware flashable.

## 5. Matrice minimale des tests de non-régression

| Scénario | Obligatoire avant chaque passage |
| --- | --- |
| Menu → ROM → jeu 10 min → C → menu | ROM/entrée/affichage corrects ; sortie et sauvegarde sans perte |
| Jeu sans toucher pendant > délai de veille | Jeu **continue**, aucun écran de veille ni perte audio |
| Jeu → encodeur 0 REC → jeu 20 s → STOP | Témoins basés sur ACK ; WAV valide et distinct ; jeu/audio continus |
| Jeu → REC 30 s → arrêt automatique → nouvelle REC | 2 WAV distincts, banque jamais écrasée, pas de statut REC bloqué |
| Jeu → REC avec SD Teensy absente/pleine | Erreur visible, jeu et commandes préservés ; ancien WAV intact |
| Jeu → D (save) / C (sortie), erreurs SD ESP32 simulées | Aucune ROM déchargée si la sauvegarde échoue ; restauration SRAM/RTC |
| CONSOLE → STUDIO → CONSOLE | Menus, commandes, synthés et effets retrouvent leur état convenu ; pas de redémarrage obligatoire |
| Compilations double firmware + tests natifs | Tous verts pour le **même SHA**, puis vérification réelle distincte |

**Périmètre volontairement exclu :** déchiffrage de LSDJ, sync tracker/GB, multitrack DAW, nouveau hardware, vraie stéréo DAC, V2 par défaut et banque multi-samples. Ils restent dans les idées et n'entrent pas dans le critère de fin « ROM jouable + WAV capturé ».

## 6. Verdict et règle de décision

Le **profil CONSOLE + CAPTURE minimal vaut la peine**, parce qu'il élimine des comportements non désirés et rend la capture testable sans casser le reste. En revanche, une **réarchitecture globale en racks n'a pas encore de justification empirique**. Implémenter R0 → R1 → R2 ; n'ouvrir R3/R4 que si les chiffres de R0–R2 démontrent un problème encore présent, identifient son origine et montrent une réduction mesurable sans régression. Les tâches déjà activées à la demande ne doivent pas être « optimisées » une deuxième fois sans preuve.
