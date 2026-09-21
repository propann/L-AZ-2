# AZ-2 — Refonte interface musique : MOTEURS vivants + MIXER dynamique

**Date : 21 septembre 2026 — proposition issue de l'audit du code réel.**

## Décision d'architecture UI

La page **PATCH ne doit plus rester une entrée indépendante du menu Musique**. Son contenu appartient conceptuellement au moteur choisi sur une piste. La transition doit cependant être progressive : dans une première passe, conserver `Screen::Patch` et tout le protocole actuel en interne, mais retirer l'entrée PATCH du menu et y accéder comme **sous-vue MOTEURS > EDIT**. Une fois la nouvelle surface validée sur matériel, supprimer le code d'interface historique devenu inutile.

Le tracker reste le point d'ancrage. La chaîne utilisateur devient :

`TRACKER → MOTEUR → EDIT SON → MIXER → SONG → PROJET`

Aucune modification du format des patterns n'est nécessaire pour cette refonte.

## 1. Ce que le code possède déjà

### MOTEURS
- sélection de piste 1–8 ;
- six moteurs : DEXED, EPIANO, BRAIDS, KARPLUS, ANALOG, SAMPLER ;
- browser moteur à gauche / patches à droite ;
- aperçu de réglages en bas ;
- bouton A / tactile ouvrant déjà `Screen::Patch`.

### PATCH
- filtre cutoff/résonance ;
- ADSR générique hors Dexed ;
- paramètres spécifiques DEXED (algo, feedback + 17 paramètres DX7) ;
- 12 paramètres EPIANO ;
- COLOR/TIMBRE pour BRAIDS ;
- bitcrusher/delay et autres lignes supplémentaires déjà branchées dans le moteur audio ;
- volume, sauvegarde/chargement de patch ;
- oscilloscope audio réel à ~15 rafraîchissements/s ;
- encodeurs contextuels 1/2 ;
- tactile pour sélectionner un paramètre ;
- bouton B pour tenir une note test et entendre la modification.

Cela signifie que la majorité du **backend d'un éditeur vivant est déjà présente**. Le chantier porte surtout sur le modèle visuel et la navigation.

### MIXER
Le code actuel affiche huit grands faders dont la hauteur correspond à `trackVolume[]`. Ce ne sont **pas des VU-mètres**. Il existe mute/solo, piste sélectionnée et encodeurs contextuels PISTE/VOLUME. Aucun `AudioAnalyzePeak`/RMS par piste n'est présent actuellement.

## 2. Nouvelle page MOTEURS

### Vue A — BROWSER
Partie haute : piste active + nom/couleur du moteur.

Partie centrale :
- liste moteurs compacte ;
- patches du moteur ;
- aperçu graphique du moteur sélectionné à la place du bandeau textuel actuel.

Taper l'aperçu ou appuyer sur **A** ouvre la vue EDIT.

### Vue B — EDIT
Une page plein écran appartenant à MOTEURS, pas un onglet principal.

Organisation proposée :

- bande haute : `PISTE 3 · BRAIDS · CLOUD` ;
- **zone graphique dominante** (environ 45 % de l'écran) ;
- deux paramètres principaux affectés aux encodeurs 1 et 2 ;
- bande de paramètres secondaires tactile en dessous ;
- B = note test maintenue ;
- C = retour au browser MOTEURS ;
- toucher un paramètre secondaire l'affecte à l'un des deux encodeurs ;
- changement de valeur = animation immédiate du dessin + commande Teensy ;
- scope réel facultatif superposé ou accessible par un petit bouton `SCOPE`, pas forcément affiché en permanence.

L'encodeur 0 conserve son rôle global de volume.

## 3. Une identité graphique par moteur

Les graphismes doivent être **calculés**, pas des vidéos ni des images lourdes. Arduino_GFX suffit : lignes, cercles, polygones, courbes approximées et quelques dizaines de points. Le dessin change uniquement quand un paramètre change ou lors d'une petite animation temporisée.

### ANALOG — oscillateur vivant
Visuel principal : onde animée.

- forme de l'onde reflète le patch/waveform ;
- cutoff = ouverture d'un halo / amplitude des harmoniques dessinées ;
- resonance = pic visuel autour du point de coupure ;
- ADSR = enveloppe superposée en quatre segments ;
- encodeur 1 par défaut : CUTOFF ;
- encodeur 2 : RESONANCE ;
- toucher l'enveloppe permet de sélectionner A/D/S/R.

C'est le moteur le plus proche de l'esprit OP-1 : on voit littéralement le son que l'on façonne.

### BRAIDS — morphing macro-oscillator
Visuel principal : forme centrale abstraite qui se transforme.

- SHAPE/patch choisit la famille visuelle ;
- TIMBRE déforme la géométrie horizontalement ;
- COLOR modifie densité/structure ;
- filtre représenté par un cercle/masque autour de la forme ;
- encodeurs par défaut : TIMBRE / COLOR.

L'objectif n'est pas de reproduire l'UI Mutable Instruments, mais de donner à BRAIDS sa propre identité AZ-2.

### KARPLUS — corde pincée
Visuel principal : corde horizontale en vibration.

- attaque/note test = déplacement brutal de la corde ;
- decay/release = amortissement de l'animation ;
- cutoff/résonance modifient la finesse et la courbure ;
- éventuels futurs paramètres damping/excitation pourront être ajoutés sans changer la page.

Même avec peu de paramètres spécifiques aujourd'hui, ce moteur peut être immédiatement plus vivant.

### EPIANO — lame / tine électrique
Visuel principal : marteau + lame/barre qui vibre.

- HARDNESS = impact plus ou moins franc ;
- DECAY/RELEASE = durée visuelle de vibration ;
- TREMOLO/LFO RATE = oscillation latérale ;
- TREBLE = brillance/densité de la trace ;
- STEREO = séparation gauche/droite visuelle ;
- encodeurs par défaut : HARDNESS / TREMOLO, ou DECAY / TREMOLO selon essais.

Les 12 paramètres déjà présents donnent suffisamment de matière pour une vraie page instrument.

### DEXED — graphe FM
Visuel principal : les opérateurs et leur routage.

- ALGO sélectionne l'un des 32 graphes ;
- FEEDBACK anime/boucle l'opérateur concerné ;
- PEG est visualisée en courbe ;
- LFO peut être un petit mouvement/pulsation du graphe ;
- opérateur/branche active mise en évidence visuellement ;
- encodeurs par défaut : ALGO / FEEDBACK.

Le code expose déjà suffisamment de paramètres globaux DX7 pour commencer. Un éditeur complet des 6 opérateurs pourra venir plus tard ; ne pas bloquer cette refonte dessus.

### SAMPLER — waveform
Visuel principal : forme d'onde du sample.

- waveform simplifiée stockée en aperçu (64–128 points) ;
- nom/slot et note racine ;
- curseur de lecture animé lorsqu'un sample joue ;
- futur : START / END / PITCH / LOOP, trim tactile ;
- GB Capture doit apparaître avec sa propre identité lorsque le dernier sample Game Boy est chargé.

Aujourd'hui, le moteur ne possède pas encore tous les paramètres nécessaires au trim/loop : afficher d'abord ce qui est vrai, puis ajouter les contrôles audio progressivement.

## 4. Enveloppe ADSR : composant partagé

Créer un seul composant graphique `drawEnvelopeWidget(A,D,S,R,...)` réutilisable par ANALOG, BRAIDS, KARPLUS, EPIANO et SAMPLER lorsque l'enveloppe générique s'applique.

Fonctionnement :
- axe horizontal = temps normalisé ;
- hauteur = niveau ;
- quatre points/segments A-D-S-R ;
- paramètre sélectionné dessiné avec accent ;
- toucher une zone A/D/S/R sélectionne le paramètre ;
- encodeur actif change la valeur ;
- redessin partiel uniquement du widget.

Pour DEXED, utiliser un widget PEG séparé afin de ne pas montrer une ADSR générique qui n'agit pas réellement sur ce moteur.

## 5. MIXER 2.0

Le mixeur actuel est fonctionnel mais visuellement statique. La refonte doit afficher **volume demandé** et **niveau audio réel** comme deux informations différentes.

### Channel strip par piste
Chaque piste affiche :
- numéro 1–8 ;
- mini nom moteur : `ANA`, `BRD`, `DX7`, `EPN`, `KPL`, `SMP` ;
- VU vertical audio réel ;
- marque de fader/volume superposée ou adjacente ;
- M / S ;
- indication très compacte de FX (ex. D pour delay, C pour crush) ;
- piste sélectionnée plus claire.

Partie basse :
- MUTE / SOLO tactiles conservés ;
- valeur volume exacte de la piste sélectionnée ;
- nom moteur + patch ;
- éventuellement deux mini réglages contextuels `VOL` / `FX` aux encodeurs.

### VU-mètres réels
Ajouter côté Teensy une télémétrie légère.

Option recommandée : un détecteur de **peak** par piste après `trackFx[t]` et avant le mixage de groupe, ou un analyseur maison ultra-léger si huit `AudioAnalyzePeak` sont jugés trop coûteux.

Protocole proposé, binaire ou ASCII décimé :
`LEVELS:v0,v1,v2,v3,v4,v5,v6,v7`

Cadence cible initiale : **10 à 15 Hz**, pas 60 Hz. L'ESP32 anime ensuite la retombée du VU localement entre deux mesures. Cela limite le trafic UART et évite de redessiner huit colonnes à chaque bloc audio.

Ajouter également un niveau MASTER si son coût est raisonnable.

**Validation obligatoire** avant de garder huit analyseurs permanents :
- CPU Teensy avant/après ;
- AudioMemory avant/après ;
- aucun underrun GB ;
- aucun impact audible avec 8 pistes ;
- UART stable à 921600.

Si le coût est trop élevé, utiliser une mesure multiplexée/échantillonnée ou un tap léger plutôt que huit analyseurs permanents.

## 6. Animation et performances

Une interface vivante ne doit pas devenir un concurrent de l'audio.

Règles :
- animation visuelle ESP32 uniquement ;
- Teensy n'envoie que paramètres/mesures utiles ;
- 15–30 FPS maximum pour les animations décoratives ;
- redraw partiel des régions modifiées ;
- scope réel activé seulement dans l'éditeur moteur concerné ;
- aucune animation moteur hors de la page visible ;
- mixer VU 10–15 Hz de données, interpolation visuelle côté ESP32 ;
- mode CONSOLE/GB désactive toute cette télémétrie musicale non nécessaire.

## 7. Transition pas à pas

### UI-0 — photographie de l'existant
- [ ] captures/photos des pages MOTEURS, PATCH, MIXER sur matériel réel ;
- [ ] relever CPU/MEM Teensy au repos et pendant 8 pistes ;
- [ ] noter les contrôles physiques/tactiles réellement agréables ou gênants.

### UI-1 — PATCH devient sous-vue de MOTEURS
- [ ] retirer uniquement l'entrée PATCH de `kMenuItems` ;
- [ ] conserver `Screen::Patch` en interne ;
- [ ] A/tap depuis MOTEURS ouvre toujours cette sous-vue ;
- [ ] C retourne à MOTEURS ;
- [ ] entrée depuis panneau tracker ouvre directement l'éditeur du moteur de la piste sélectionnée ;
- [ ] tests navigation tactile + croix/A/B/C.

**Cette étape ne modifie aucun protocole audio.**

### UI-2 — composant ADSR vivant + ANALOG
- [ ] créer le composant enveloppe ;
- [ ] créer la première identité moteur ANALOG : waveform + filtre + ADSR ;
- [ ] raccorder encodeurs contextuels ;
- [ ] B note test ;
- [ ] comparer commandes et valeurs avec l'ancien PATCH avant suppression de quoi que ce soit.

ANALOG sert de prototype de l'architecture visuelle.

### UI-3 — identités moteur une par une
Ordre recommandé :
1. BRAIDS ;
2. EPIANO ;
3. KARPLUS ;
4. SAMPLER ;
5. DEXED.

Après chaque moteur :
- paramètres visibles = paramètres réellement supportés ;
- tactile + encodeurs ;
- SAVE/LOAD patch ;
- comparaison aller-retour valeurs avant/après ;
- tests matériel.

### UI-4 — MIXER avec VU
- [ ] ajouter niveau d'une seule piste pilote ;
- [ ] mesurer CPU/MEM ;
- [ ] étendre à 8 seulement si coût acceptable ;
- [ ] nouveau channel strip ;
- [ ] master meter ;
- [ ] interpolation/peak hold côté ESP32 ;
- [ ] test tracker 8 pistes + mixer affiché + GB audio inactif, puis GB actif pour vérifier absence d'impact.

### UI-5 — nettoyage
Seulement quand toutes les vues nouvelles sont validées :
- [ ] supprimer PATCH du menu définitivement (déjà caché depuis UI-1) ;
- [ ] renommer éventuellement `Screen::Patch` en `EngineEdit` ;
- [ ] factoriser l'ancien code de lignes devenu inutile ;
- [ ] mettre à jour manuel FR/EN/ES ;
- [ ] conserver un tag/commit avant suppression pour retour arrière.

## 8. Résultat visé

Le menu Musique devient plus court et plus logique :

- **TRACKER**
- **MOTEURS**
- **MIXER**
- **SONG**
- **PROJET**
- **AUDIO / LIVE**

Dans **MOTEURS**, chaque moteur n'est plus une liste de paramètres générique : c'est un instrument avec une identité graphique, une animation et deux contrôles physiques immédiats. Les paramètres avancés restent accessibles sans transformer l'écran en tableau Excel.

Le MIXER devient une page de performance : niveaux qui bougent réellement, faders, mute/solo, moteur/patch visible et activité de chaque piste.

Cette refonte réutilise au maximum le backend actuel et minimise le risque pour le tracker et l'audio.
