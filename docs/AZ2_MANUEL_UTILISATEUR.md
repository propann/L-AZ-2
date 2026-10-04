# AZ-2 — Manuel d'utilisation

Ce guide explique comment **jouer avec la machine**, pas comment la construire ou la programmer. Pour le câblage et le flash des firmwares, voir [Premier démarrage](AZ2_DEMARRAGE.md). Pour l'état d'avancement technique détaillé, voir [État des lieux](AZ2_ETAT_DES_LIEUX.md).

> Document vivant : l'AZ-2 est un prototype en développement actif, certains réglages ou pages peuvent encore changer. Ce manuel décrit l'état visé le plus récent de l'interface — en cas de différence avec la machine devant vous, l'interface réelle a raison.

## 1. Vue d'ensemble

L'AZ-2 est une groovebox distribuée sur quatre cartes programmables :

- **Teensy 4.1** (le master audio) : tracker, moteurs locaux, chaîne d'effets par piste, mixage et sortie PCM5102A.
- **ESP32-S3 écran** : interface tactile 480×480 et console de jeu (Game Boy, Game Boy Color, NES).
- **ESP32-S3 N16R8** : moteur GRANULAR et agrégation du rack audio.
- **ESP-WROOM-32D** : moteur SPECTRAL.

Vous interagissez avec la machine via :

- une **croix directionnelle** (HAUT / BAS / GAUCHE / DROITE),
- **4 boutons** A / B / C / D,
- **3 encodeurs rotatifs** avec bouton poussoir intégré,
- l'**écran tactile**.

La plupart des pages se pilotent aussi bien à la croix qu'au doigt — un tap tactile sélectionne toujours la même chose que ce que la croix aurait sélectionné.

## 2. Conventions de contrôle (à retenir une fois pour toutes)

Ces règles reviennent sur presque toutes les pages :

- **C = RETOUR.** Toujours un seul niveau en arrière (jamais un saut direct au menu principal), sauf si vous êtes déjà au menu. Pensez-y comme "la page précédente", pas "l'accueil".
- **Un tap = sélectionner.** Toucher une ligne, une piste ou un réglage le sélectionne (et l'applique immédiatement s'il s'agit d'un choix simple comme un moteur ou un patch).
- **A maintenu + HAUT/BAS (ou GAUCHE/DROITE selon la page) = régler une valeur.** Le réglage sélectionné (en surbrillance) change tant que A reste enfoncé.
- **Sans A, HAUT/BAS/GAUCHE/DROITE = se déplacer** entre les réglages, pistes ou pas, sans rien modifier.
- **La ligne "PISTE" en haut d'une page** (quand elle existe) se distingue par un contour blanc quand elle a le focus — GAUCHE/DROITE y change alors de piste. On y accède en remontant (HAUT) depuis la toute première ligne de la page.
- **Les encodeurs 2 et 3 sont contextuels** (voir §4) : ce qu'ils règlent change selon la page affichée, indiqué par 2 pastilles de couleur (orange et cyan) en haut à droite de l'écran avec un petit libellé. **Le clic de l'encodeur 3 ouvre le mode EFFETS** depuis presque toutes les pages.

## 3. Le menu principal

4 catégories, organisées en grille :

| Catégorie | Contient |
| :-- | :-- |
| **Musique** | Séquenceur, Moteurs, Patch, Mixer, Song, Projet, Audio |
| **Jeux** | Game Boy, Game Boy Color, NES |
| **Configuration** | Écran de veille, réglages généraux |
| **Doc** | Journal de liaison série, À propos |

Naviguez à la croix jusqu'à une catégorie, **A** pour l'ouvrir, puis à nouveau à la croix + **A** pour choisir une page dans la sous-liste (ou touchez directement une tuile/un nom).

## 4. Les encodeurs

3 encodeurs rotatifs, chacun avec un bouton poussoir intégré (clic) :

- **Encodeur 1 (VOLUME)** : rôle **fixe**, toujours le volume général de sortie, quelle que soit la page affichée. Câblé directement — il répond même si l'écran est figé.
- **Encodeurs 2 et 3** : **contextuels**, leur rôle change selon la page. Repérables par leur couleur affichée à l'écran (pastille + libellé, coin supérieur droit) :
  - 🟠 **Orange = encodeur 2**
  - 🔵 **Cyan = encodeur 3**
  - Un libellé gris "--" veut dire que cet encodeur n'a rien à faire sur la page actuelle.

Exemples :

- **MIXER** : encodeur 2 = choisir la piste, encodeur 3 = régler son volume.
- **PATCH** : encodeur 2 = le **cadre animé du haut** (clic = réglage visuel suivant, rotation = valeur), encodeur 3 = la **grille du bas** (clic = case suivante, rotation = valeur). Voir §7.
- **AUDIO (pads)** : clic de l'encodeur 2 = menu des pads ; clic de l'encodeur 1 (volume) = enregistrement ON/OFF.
- **SEQ. PAS** : encodeur 2 = piste, encodeur 3 = note du pas au curseur.
- **Encodeur 2** garde aussi ses fonctions du pad virtuel et de l'arpégiateur.

### Mode EFFETS (clic de l'encodeur 3)

Depuis le tracker, SEQ. PAS, MOTEURS, MIXER et la plupart des pages :

1. **Clic sur l'encodeur 3** : un bandeau s'ouvre en bas, par exemple `P3  PRESET  DUB`, pour la piste sélectionnée sur la page.
2. **Tourner l'encodeur 3** : change la valeur en direct (sur PRESET, charge le preset suivant).
3. **Re-clic** : passe à l'effet suivant (PRESET → DRIVE → CRUSH → … → SYNC DLY).
4. **4 secondes sans y toucher** : le bandeau disparaît, l'encodeur 3 reprend son rôle normal.

Le mode n'existe pas en jeu (le clic y fait START), sur la page AUDIO, ni sur la page PATCH (les effets y ont leurs propres lignes).

## 5. Le tracker (page SÉQUENCEUR)

Le cœur de la composition : un tracker 8 pistes, 16 pas par pattern, inspiré des trackers "à la LSDJ/M8".

### Vue d'ensemble

- Sélecteur de **PISTE** en haut (1 à 8).
- 16 lignes, une par pas, colonnes **NOTE / INST / FX / VAL / PROB / COND**.
- **Panneau latéral droit** avec 6 boutons tactiles :
  - **MOTEUR** — ouvre la page Moteurs pour cette piste.
  - **PATCH** — ouvre la page Patch complète (filtre, ADSR, forme d'onde) pour cette piste.
  - **EFFET** — reste dans le tracker, place le focus croix directement sur la colonne FX du pas sélectionné.
  - **PAD 4X4** — ouvre le clavier tactile 4×4 configuré sur le moteur/patch de la piste sélectionnée, en mode "pose la note sur le pas sélectionné".
  - **METRO** — active/désactive le métronome.
  - **SAUVER** — sauvegarde tout le morceau (patterns, chaînage song, tempo, et le moteur/patch/réglages de chaque piste) dans l'emplacement PROJET actuellement choisi, sans quitter le tracker.

### Éditer un pas

- HAUT/BAS (sans A) : déplace le pas sélectionné.
- GAUCHE/DROITE : change de colonne (NOTE/INST/FX/VAL/PROB/COND).
- A maintenu + HAUT/BAS : édite la valeur de la colonne sélectionnée.
  - **NOTE** : poser une note allume automatiquement le pas.
  - **INST** : choisit un patch différent du patch par défaut de la piste, juste pour ce pas.
  - **FX/VAL** : effet et sa valeur pour ce pas (voir §7 bis, « Effets de pas »).
  - **PROB** : probabilité de déclenchement du pas (permet des variations aléatoires).
  - **COND** : condition de déclenchement (ex. uniquement 1 fois sur 2, ou en "fill").
- Toucher directement une ligne fait la même chose qu'y naviguer à la croix.

### En-tête

Deux boutons en haut du tracker : **MENU** (retour au menu principal) et **PATTERN** (A + GAUCHE/DROITE, ou toucher la moitié gauche/droite, pour changer de pattern). On y monte avec HAUT depuis le premier pas.

### Transport

Barre du bas : PLAY/STOP, BPM (+/- 5 par tap gauche/droite de la case), division rythmique. Les flèches peuvent entrer dans les boutons latéraux : depuis la dernière colonne, DROITE ouvre le focus du panneau, HAUT/BAS choisit MOTEUR/PATCH/EFFET/CLAVIER/METRO/SAUVER, puis A confirme. Bouton **D** déclenche un "fill" temporaire pendant qu'il est maintenu (variation de motif). **C court** = PLAY/STOP, **C maintenu** (0,7 s) = retour au menu.

### SEQ. PAS (vue pas à pas, style OP-1)

- GAUCHE/DROITE : déplace le curseur de pas ; HAUT/BAS : change de piste.
- **A tapé** : allume/éteint le pas. **A maintenu + HAUT/BAS** : monte/descend la note du pas dans la gamme (et l'allume).
- Encodeur 2 : piste ; encodeur 3 : note ; **C court** : PLAY/STOP, **C maintenu** : retour au menu.

## 6. Page MOTEURS

Assigner un moteur de synthèse et un patch à chaque piste.

- Ligne **PISTE** en haut (GAUCHE/DROITE pour changer, atteinte en remontant depuis le haut des listes).
- **Liste MOTEUR** à gauche (9 moteurs, voir §9), **liste PATCH** à droite (patchs disponibles pour le moteur choisi — jusqu'à 255 pour Dexed).
- GAUCHE/DROITE bascule le focus entre les 2 listes ; HAUT/BAS s'y déplace.
- Toucher ou sélectionner un moteur/patch l'applique **immédiatement**.
- Bandeau du bas : aperçu du patch actuel, touchez-le (ou appuyez **A** quand la liste PATCH a le focus) pour ouvrir directement la page PATCH complète.

## 7. Page PATCH

Le réglage fin du son de la piste affichée : filtre, enveloppe, réglages propres au moteur, effets de la piste et volume.

- Ligne **PISTE** en haut (même convention qu'ailleurs). Bouton **MOT>** au bout : un toucher passe la piste au moteur suivant sans quitter la page (la section EFFETS est conservée).
- **Cadre animé** (en haut à gauche, bordure orange) — domaine de l'**encodeur 2**. Il montre ce que fait le réglage choisi :
  - CUTOFF / RESO : la courbe du filtre et les harmoniques coupées au-delà ;
  - ATTACK, DECAY, SUSTAIN, RELEASE : l'enveloppe, parcourue par un point, segment réglé en blanc ;
  - réglages du moteur : une animation propre à chaque moteur, pilotée par ses vrais réglages (opérateurs FM de DEXED selon l'algorithme, lame et marteau d'EPIANO, pavé COLOR/TIMBRE de BRAIDS, corde de KARPLUS, onde d'ANALOG, pads de DRUM, tête de lecture du SAMPLER, grains, spectre).
  - Clic encodeur 2 = réglage visuel suivant (CUTOFF → RESO → ADSR ou ALGO/FDBK → COLOR/TIMBRE ou HARDNESS/TREMOLO/LFO RATE), rotation = valeur. L'étiquette orange « ENC2 … » rappelle le réglage et sa valeur. Le son réel (bouton **B**) anime l'intensité et s'affiche en filigrane.
- **Cadre des presets** (en haut à droite) : **A** entre en édition, HAUT/BAS choisit le preset, **A** ressort. Choisir un patch le fait repartir de **ses propres réglages** (filtre ouvert, enveloppe par défaut, valeurs du preset relues) ; un projet ou un SLOT rechargé garde les siens.
- **Grille de réglages** (en bas, **3 cases par ligne**) — domaine de l'**encodeur 3** : clic = case suivante, rotation = valeur. La case active a un fond coloré, un double cadre blanc et une languette cyan ; la case réglée par l'encodeur 2 porte un coin orange.
  - À la croix : GAUCHE/DROITE choisit la case, HAUT/BAS change de ligne en gardant la colonne ; **A maintenu + une direction** édite la valeur. Le toucher sélectionne une case.
  - Contenu : filtre et ADSR (ALGO/FEEDBACK pour Dexed), réglages spécifiques au moteur (voir §9), **section EFFETS** (voir §7 bis), VOLUME, puis la ligne **SLOT / SAVE / LOAD** (voir §10).
- **Bouton B** joue/coupe une note de test sur la piste affichée.

## 7 bis. Les effets

Chaque piste a sa **propre chaîne d'effets**, indépendante des autres pistes et du moteur choisi :

`moteur → filtre (+ LFO) → drive → crush → tremolo / ring mod → flanger → delay avec feedback → volume → envoi reverb`

### Section EFFETS (page PATCH)

Lignes à nom coloré, sous les réglages du moteur :

| Ligne | Effet |
| :-- | :-- |
| PRESET FX | charge d'un coup les 14 réglages ci-dessous (voir les presets plus bas) |
| DRIVE | saturation douce, le volume reste le même |
| CRUSH | lo-fi, de 16 bits (propre) à 2 bits |
| LFO RATE / LFO DEPTH | LFO sur le filtre (wah) : vitesse 0,05–20 Hz, profondeur jusqu'à 4 octaves |
| DELAY / FEEDBACK / DLY MIX | écho par piste jusqu'à 500 ms, répétitions, niveau |
| FILTRE | type de filtre : LP (passe-bas), BP (passe-bande), HP (passe-haut) |
| TREMOLO | volume qui pulse au rythme du LFO |
| RING MOD | modulation en anneau (métallique, cloche, robot) |
| FLANGER | effet « avion » |
| REVERB | envoi de la piste vers la reverb générale |
| SYNC LFO / SYNC DLY | LFO et écho calés sur le tempo (4 MES … 1/32, triolets et pointés) |

En **BP** ou **HP**, baissez le CUTOFF : filtre grand ouvert, le son disparaît.

### Presets d'effets

PROPRE, DUB, LOFI, WAH, ESPACE, ROBOT, TREMOL, JET, SATURE, CHIP, PING, CATHED. Un preset est un point de départ : retouchez ensuite chaque effet librement. Chargement par la ligne PRESET FX (page PATCH) ou par le mode EFFETS de l'encodeur 3 (§4).

### Effets de pas (colonne FX du tracker)

| Effet | Pendant le pas |
| :-- | :-- |
| ARP, CUT, RET | arpège, coupure, re-déclenchement |
| CRUSH, DELAY | preset de bitcrusher / d'écho, qui reste actif ensuite |
| DRIVE, WAH, REVRB, RING, TREM, FLANG, DMIX | **verrou de paramètre** : la valeur (0–127, par pas de 8) remplace le réglage EFFETS de la piste le temps du pas, puis la piste revient à son réglage |

### Bus général (ligne MASTER du MIXER)

REV (reverb), DLY (delay de 350 ms) et CHO (chorus) s'appliquent à tout le mix. Un toucher sur une case avance de 20 (0 → 100 → 0).

Tous ces réglages sont **sauvegardés avec le projet**.

## 8. Page MIXER

Vue d'ensemble du volume de toutes les pistes à la fois.

- 8 barres verticales, une par piste (numérotées 1 à 8), hauteur = volume.
- GAUCHE/DROITE (ou toucher une barre) choisit la piste ; HAUT/BAS règle directement son volume (pas besoin de maintenir A — c'est un fader, le geste le plus fréquent).
- Boutons **MUTE** et **SOLO** (tactiles, ou boutons physiques B/D) pour la piste sélectionnée — indicateur M/S affiché sous sa barre.
- Encodeur 2 = choisir la piste, encodeur 3 = son volume (mêmes actions qu'à la croix, en plus rapide).
- Ligne **MASTER** tout en bas : reverb, delay et chorus du bus général (voir §7 bis).
- **VU-mètres** : chaque barre affiche en direct le niveau réel de sa piste (après effets), plus un VU du bus MASTER.

## 9. Les 9 moteurs audio

| Moteur | Type | Patchs disponibles |
| :-- | :-- | :-- |
| **DEXED** | Synthèse FM (compatible DX7) | 255 (banques ROM Yamaha officielles) |
| **EPIANO** | Piano électrique (mda ePiano) | 105 (5 d'origine + 100 variations) |
| **BRAIDS** | Oscillateur macro (Mutable Instruments) | 43 formes d'onde |
| **KARPLUS** | Corde pincée (Karplus-Strong) | 100 presets (nylon, acier, harpe, basse, pluck, drone, FX…) |
| **ANALOG** | Oscillateur + ADSR classique | 11 formes d'onde |
| **SAMPLER** | Lecture d'échantillons PCM | Kick, Snare et GB Capture expérimental |
| **DRUM** | Percussions synthétiques Teensy | 6 programmes |
| **GRANULAR** | Granulaire externe sur ESP32-S3 avec PSRAM | 8 presets |
| **SPECTRAL** | Synthèse additive/spectrale externe sur WROOM-32D | 8 presets |

Chaque piste peut sélectionner l'un des neuf moteurs. Les niveaux sont **équilibrés** entre moteurs et entre patches (mesure automatique de tous les patches, `tools/engine_bench.py`) : changer de moteur ou de patch ne fait plus bondir le volume. GRANULAR et SPECTRAL sont chacun une ressource physique unique du rack externe : une seule piste à la fois en possède le contrôle.

## 10. Sauvegarder / charger

Deux systèmes de sauvegarde distincts, tous deux sur la carte SD de l'écran :

- **Patch** (page PATCH, ligne SLOT) : moteur + patch + filtre + ADSR + algo/feedback Dexed d'**une seule piste**, dans l'un des 8 emplacements. A maintenu + GAUCHE (charge) ou DROITE (sauve) sur la ligne SLOT ; ou touchez directement SLOT/SAVE/LOAD.
- **Projet** (page PROJET, ou bouton SAUVER du tracker) : **tout le morceau** — patterns, chaînage song, tempo, gamme, et le moteur/patch/réglages de chaque piste — dans l'un des 4 emplacements.

La page **PROJETS** affiche les quatre fichiers de la SD ESP32 sous forme de liste. **HAUT/BAS** choisit un projet, **A** le charge, **D** lance la sauvegarde (un deuxième appui confirme si le slot contient déjà un fichier), **B** ouvre SONG et **C** revient. Les deux grands boutons tactiles CHARGER/SAUVER font les mêmes actions ; toucher une ligne la sélectionne. Un message en bas confirme le résultat ou indique l'erreur.

Les deux utilisent une écriture atomique (fichier temporaire puis renommage) : une coupure de courant en cours de sauvegarde ne peut pas corrompre le dernier fichier valide.

## 11. Page SONG

Chaîner plusieurs patterns pour construire un morceau complet (intro / couplet / refrain...). Bascule entre boucle simple (comportement par défaut) et mode song ; une grille de 16 cases où chaque case pointe vers un pattern (0-7).

Les pages SONG et PROJETS sont séparées pour que la grille ne recouvre plus les commandes de sauvegarde. Dans SONG, **HAUT/BAS** déplace la case choisie, **GAUCHE/DROITE** change son pattern, **A** bascule le mode, **D** règle la longueur, **B** ouvre PROJETS et **C** revient. Les cases, le mode, la longueur et le bouton PROJETS restent tactiles.

## 12. Page AUDIO (pads)

4×4 pads tactiles colorés pour jouer en direct. Bouton D bascule le mode "pose la note sur le pas actuellement sélectionné du tracker" (pratique pour enregistrer une performance live directement dans un pattern) — en mode pose, le son se fait aussi entendre en direct.

Deux façons d'y arriver, avec un comportement différent :

- **Depuis le menu (Musique → Audio)** : pads génériques, jouent une voix de synthèse fixe — pour noodler sans dépendre d'une piste précise.
- **Depuis le bouton CLAVIER du panneau latéral du tracker** : les pads jouent le **vrai moteur/patch de la piste** actuellement sélectionnée (n'importe quel pattern, n'importe quelle piste) — si cette piste utilise le SAMPLER, ce sont ses échantillons qui sonnent.

**Menu des pads** (clic de l'encodeur 2, ou bouton tactile) : tout est dedans pour laisser la place au jeu — MODE (jeu libre / pose sur le pas), PISTE jouée (ou VOIX LIVE), **ARPÉGIATEUR** (mode, vitesse, octaves), **MÉTRONOME**, **ENREGISTREMENT**, raccourcis SAMPLER / MOTEURS / SÉQUENCEUR / MENU PRINCIPAL. Les réglages gardent le menu ouvert.

- **Enregistrement live** : clic de l'encodeur 1 (volume) ou menu ; pendant la lecture, chaque pad joué s'écrit sur le pas en cours de la piste.
- **Multipoint** : deux doigts à la fois. Sur un moteur polyphonique (DEXED, EPIANO, VOIX LIVE) ça joue un accord ; sur un moteur monophonique (ANALOG, BRAIDS, KARPLUS) la dernière note tenue sonne et la précédente reprend quand on la relâche — le son tient tant qu'un doigt appuie.
- **Arpégiateur** : les pads tenus sont joués en motif (haut, bas, aller-retour, aléatoire…) au tempo, sur 1 à 4 octaves.

**Kit de batterie** : indépendamment de tout ça, chaque pad peut avoir son **propre échantillon dédié** assigné directement (jusqu'à 2 secondes chacun, les 16 pads sonnent en même temps si besoin) — un pad avec un échantillon assigné sonne toujours ce son-là en priorité, quel que soit le mode. Un premier kit de 8 sons (grosse caisse, caisse claire, charley fermé/ouvert, clap, rim, cowbell, ride) est déjà prêt sur les pads 1 à 8 dès que la bibliothèque de samples est présente sur la carte SD du Teensy. Ces assignations se sauvegardent avec le reste du morceau (page PROJET).

## 13. Jeux (Game Boy / Game Boy Color / NES)

Menu **Jeux** → page ÉMULATEURS, trois cartes. Chaque carte ne liste que ses ROM, rangées dans `/games` sur la SD de l'écran (sous-dossiers acceptés).

| Carte | ROM | Cadence mesurée |
| :-- | :-- | :-- |
| GAME BOY | `.gb` | 59,7 fps en X2 et X3 |
| GAME BOY COLOR | `.gbc` | 59,7 fps en X2 et X3 |
| NES | `.nes` | 49–50 fps (en cours d'optimisation) |

- Liste de ROM à la croix, **A** pour lancer ; X2 / X3 choisit la taille de l'image.
- En jeu : A/B = boutons A/B, SELECT/START sur les clics des encodeurs 2 et 3, **C** quitte proprement (sauvegarde la RAM cartouche avant de fermer).
- Sauvegarde automatique toutes les 30 s, seulement si la partie a changé (pas d'à-coup).
- Le son du jeu passe par le bus général (volume, reverb, delay) ; la capture GB → WAV → SAMPLER est disponible.

## 14. Pages annexes

- **CONFIGURATION** : écran de veille, réglages généraux.
- **CONTROLES** : visualise en direct l'état de la croix, des boutons et des encodeurs — utile pour vérifier que le câblage répond bien.
- **LIENS SÉRIE** : journal des échanges entre les 2 cartes — utile en cas de comportement inattendu, à consulter avant de signaler un bug.
- **A PROPOS** : version, rôle de chaque carte, informations de build.

## 15. Premier beat, pas à pas

1. **Menu → Musique → Moteurs.** Choisissez un moteur (ex. ANALOG, le plus simple pour commencer) et un patch pour la piste 1.
2. **Menu → Musique → Séquenceur.** Sur la piste 1, posez quelques notes sur les pas 1, 5, 9, 13 (croix, A maintenu + HAUT/BAS sur la colonne NOTE).
3. Appuyez **PLAY** en bas du tracker. Ajustez le tempo (BPM) si besoin.
4. Passez à la piste 2 (GAUCHE/DROITE sur la ligne PISTE), changez de moteur si besoin, posez d'autres notes.
5. Ouvrez **PATCH** depuis le panneau latéral pour affiner le son (filtre, ADSR, section EFFETS) — bouton **B** pour entendre chaque réglage. Ou, séquence en route, **cliquez l'encodeur 3** et tournez : les presets d'effets défilent sur la piste.
6. Une fois satisfait, **SAUVER** depuis le tracker (ou page PROJET) pour ne rien perdre.

## Voir aussi

- [Premier démarrage](AZ2_DEMARRAGE.md) — préparer et flasher les 2 cartes.
- [Le sampleur](AZ2_SAMPLEUR.md) — état actuel et limites.
- [Licences](AZ2_LICENCES.md) — provenance des composants tiers (banques DX7, samples, cœur d'émulation GB).
- [État des lieux](AZ2_ETAT_DES_LIEUX.md) — journal détaillé de chaque évolution, avec tests matériel.
