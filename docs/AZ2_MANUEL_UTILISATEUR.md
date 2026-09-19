# AZ-2 — Manuel d'utilisation

Ce guide explique comment **jouer avec la machine**, pas comment la construire ou la programmer. Pour le câblage et le flash des firmwares, voir [Premier démarrage](AZ2_DEMARRAGE.md). Pour l'état d'avancement technique détaillé, voir [État des lieux](AZ2_ETAT_DES_LIEUX.md).

> Document vivant : l'AZ-2 est un prototype en développement actif, certains réglages ou pages peuvent encore changer. Ce manuel décrit l'état visé le plus récent de l'interface — en cas de différence avec la machine devant vous, l'interface réelle a raison.

## 1. Vue d'ensemble

L'AZ-2 est une groovebox à deux cerveaux :

- **Teensy 4.1** (le "master audio") : fait tourner le tracker (séquenceur), les 6 moteurs de synthèse, le mixage et la sortie audio (DAC PCM5102A).
- **ESP32-S3** (l'"écran") : affiche l'interface tactile 480×480, lit les ROM Game Boy / Game Boy Color depuis la carte SD, et relaie vos actions au Teensy.

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
- **2 des 3 encodeurs sont contextuels** (voir §4) : ce qu'ils règlent change selon la page affichée, indiqué par 2 pastilles de couleur (orange et cyan) en haut à droite de l'écran avec un petit libellé.

## 3. Le menu principal

4 catégories, organisées en grille :

| Catégorie | Contient |
| :-- | :-- |
| **Musique** | Séquenceur, Moteurs, Patch, Mixer, Song, Projet, Audio |
| **Jeux** | Game Boy / GBC |
| **Configuration** | Écran de veille, réglages généraux |
| **Doc** | Journal de liaison série, À propos |

Naviguez à la croix jusqu'à une catégorie, **A** pour l'ouvrir, puis à nouveau à la croix + **A** pour choisir une page dans la sous-liste (ou touchez directement une tuile/un nom).

## 4. Les encodeurs

3 encodeurs rotatifs, chacun avec un bouton poussoir intégré (clic) :

- **Encodeur 0 (VOLUME)** : rôle **fixe**, toujours le volume général de sortie, quelle que soit la page affichée. Câblé directement — il répond même si l'écran est figé.
- **Encodeur 1** et **Encodeur 2** : **contextuels**, leur rôle change selon la page. Repérables par leur couleur affichée à l'écran (pastille + libellé, coin supérieur droit) :
  - 🟠 **Orange = encodeur 1**
  - 🔵 **Cyan = encodeur 2**
  - Un libellé gris "--" veut dire que cet encodeur n'a rien à faire sur la page actuelle.

Pages où les encodeurs sont déjà câblés :

- **MIXER** : encodeur 1 = choisir la piste, encodeur 2 = régler son volume.
- **PATCH** : encodeur 1 = valeur de gauche de la ligne actuellement sélectionnée, encodeur 2 = valeur de droite — accès direct aux 2 réglages d'une même ligne sans avoir à les sélectionner un par un.

D'autres pages seront câblées progressivement — l'objectif est qu'ils servent à quelque chose sur chaque écran musical.

## 5. Le tracker (page SÉQUENCEUR)

Le cœur de la composition : un tracker 8 pistes, 16 pas par pattern, inspiré des trackers "à la LSDJ/M8".

### Vue d'ensemble

- Sélecteur de **PISTE** en haut (1 à 8).
- 16 lignes, une par pas, colonnes **NOTE / INST / FX / VAL / PROB / COND**.
- **Panneau latéral droit** avec 6 boutons tactiles :
  - **MOTEUR** — ouvre la page Moteurs pour cette piste.
  - **PATCH** — ouvre la page Patch complète (filtre, ADSR, forme d'onde) pour cette piste.
  - **EFFET** — reste dans le tracker, place le focus croix directement sur la colonne FX du pas sélectionné.
  - **CLAVIER** — ouvre la page Audio (pads tactiles), en mode "pose la note sur le pas sélectionné" (pratique pour composer en jouant en direct).
  - **METRO** — active/désactive le métronome.
  - **SAUVER** — sauvegarde tout le morceau (patterns, chaînage song, tempo, et le moteur/patch/réglages de chaque piste) dans l'emplacement PROJET actuellement choisi, sans quitter le tracker.

### Éditer un pas

- HAUT/BAS (sans A) : déplace le pas sélectionné.
- GAUCHE/DROITE : change de colonne (NOTE/INST/FX/VAL/PROB/COND).
- A maintenu + HAUT/BAS : édite la valeur de la colonne sélectionnée.
  - **NOTE** : poser une note allume automatiquement le pas.
  - **INST** : choisit un patch différent du patch par défaut de la piste, juste pour ce pas.
  - **FX/VAL** : effet et sa valeur pour ce pas (voir la liste des effets ci-dessous).
  - **PROB** : probabilité de déclenchement du pas (permet des variations aléatoires).
  - **COND** : condition de déclenchement (ex. uniquement 1 fois sur 2, ou en "fill").
- Toucher directement une ligne fait la même chose qu'y naviguer à la croix.

### Transport

Barre du bas : PLAY/STOP, BPM (+/- 5 par tap gauche/droite de la case), division rythmique. Bouton D = déclenche un "fill" temporaire pendant qu'il est maintenu (variation de motif).

## 6. Page MOTEURS

Assigner un moteur de synthèse et un patch à chaque piste.

- Ligne **PISTE** en haut (GAUCHE/DROITE pour changer, atteinte en remontant depuis le haut des listes).
- **Liste MOTEUR** à gauche (6 moteurs, voir §9), **liste PATCH** à droite (patchs disponibles pour le moteur choisi — jusqu'à 255 pour Dexed).
- GAUCHE/DROITE bascule le focus entre les 2 listes ; HAUT/BAS s'y déplace.
- Toucher ou sélectionner un moteur/patch l'applique **immédiatement**.
- Bandeau du bas : aperçu du patch actuel, touchez-le (ou appuyez **A** quand la liste PATCH a le focus) pour ouvrir directement la page PATCH complète.

## 7. Page PATCH

Le réglage fin du son de la piste affichée : filtre, enveloppe, effets propres à la piste, et un oscilloscope qui trace en direct ce qui est réellement entendu.

- Ligne **PISTE** en haut (même convention qu'ailleurs).
- Grille de réglages, **2 par ligne** pour gagner de la place — GAUCHE/DROITE choisit le réglage, HAUT/BAS monte/descend d'une ligne en gardant la colonne.
- **A maintenu + HAUT/BAS *ou* A maintenu + GAUCHE/DROITE** éditent tous les deux la valeur sélectionnée (au choix, selon ce qui est le plus confortable à tenir).
- Réglages disponibles selon le moteur : coupure/résonance du filtre, ADSR (attaque/chute/maintien/relâchement) — remplacé par ALGO/FEEDBACK pour Dexed — puis des réglages spécifiques au moteur (voir §9), **CRUSH** (bitcrusher) et **DELAY** (écho court, un seul répétition) propres à la piste, VOLUME, puis la ligne **SLOT** (sauvegarde/chargement de patch, voir §10).
- **Bouton B** joue/coupe une note de test directement sur la piste affichée — pratique pour entendre l'effet de chaque réglage en le modifiant.
- L'**oscilloscope** en haut de la page trace la forme d'onde réellement jouée (après filtre, bitcrusher et delay).
- **Encodeurs contextuels** : voir §4.

## 8. Page MIXER

Vue d'ensemble du volume de toutes les pistes à la fois.

- 8 barres verticales, une par piste (numérotées 1 à 8), hauteur = volume.
- GAUCHE/DROITE (ou toucher une barre) choisit la piste ; HAUT/BAS règle directement son volume (pas besoin de maintenir A — c'est un fader, le geste le plus fréquent).
- Boutons **MUTE** et **SOLO** (tactiles, ou boutons physiques B/D) pour la piste sélectionnée — indicateur M/S affiché sous sa barre.
- Encodeur 1 = choisir la piste, encodeur 2 = son volume (mêmes actions qu'à la croix, en plus rapide).

## 9. Les 6 moteurs de synthèse

| Moteur | Type | Patchs disponibles |
| :-- | :-- | :-- |
| **DEXED** | Synthèse FM (compatible DX7) | 255 (banques ROM Yamaha officielles) |
| **EPIANO** | Piano électrique (mda ePiano) | 5 presets réels |
| **BRAIDS** | Oscillateur macro (Mutable Instruments) | 43 formes d'onde |
| **KARPLUS** | Corde pincée (Karplus-Strong) | — |
| **ANALOG** | Oscillateur + ADSR classique | 11 formes d'onde |
| **SAMPLER** | Lecture d'échantillons PCM (pitché selon la note) | 2 samples embarqués (Kick, Snare) pour l'instant — chargement depuis la carte SD à venir |

Chaque piste peut utiliser n'importe lequel des 6, changé à tout moment depuis la page MOTEURS.

## 10. Sauvegarder / charger

Deux systèmes de sauvegarde distincts, tous deux sur la carte SD de l'écran :

- **Patch** (page PATCH, ligne SLOT) : moteur + patch + filtre + ADSR + algo/feedback Dexed d'**une seule piste**, dans l'un des 8 emplacements. A maintenu + GAUCHE (charge) ou DROITE (sauve) sur la ligne SLOT ; ou touchez directement SLOT/SAVE/LOAD.
- **Projet** (page PROJET, ou bouton SAUVER du tracker) : **tout le morceau** — patterns, chaînage song, tempo, gamme, et le moteur/patch/réglages de chaque piste — dans l'un des 4 emplacements.

Les deux utilisent une écriture atomique (fichier temporaire puis renommage) : une coupure de courant en cours de sauvegarde ne peut pas corrompre le dernier fichier valide.

## 11. Page SONG

Chaîner plusieurs patterns pour construire un morceau complet (intro / couplet / refrain...). Bascule entre boucle simple (comportement par défaut) et mode song ; une grille de 16 cases où chaque case pointe vers un pattern (0-7).

## 12. Page AUDIO (pads)

4×4 pads tactiles colorés pour jouer en direct, indépendamment du tracker. Bouton D bascule le mode "pose la note sur le pas actuellement sélectionné du tracker" (pratique pour enregistrer une performance live directement dans un pattern).

## 13. Jeux (Game Boy / Game Boy Color)

Émulateur GB/GBC intégré, ROM lues depuis la carte SD de l'écran.

- Liste de ROM naviguable à la croix, **A** pour lancer.
- En jeu : A/B = boutons Game Boy A/B, START/SELECT sur 2 des encodeurs, **C** quitte proprement (sauvegarde la RAM cartouche avant de fermer).
- Sauvegarde automatique périodique de la RAM cartouche en plus de la sauvegarde à la fermeture (perte maximale ~30 secondes en cas de coupure).
- Le son du jeu passe par le même bus d'effets maître (reverb/delay/volume) que la musique.

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
5. Ouvrez **PATCH** depuis le panneau latéral pour affiner le son (filtre, ADSR, delay/bitcrusher) — bouton **B** pour entendre chaque réglage.
6. Une fois satisfait, **SAUVER** depuis le tracker (ou page PROJET) pour ne rien perdre.

## Voir aussi

- [Premier démarrage](AZ2_DEMARRAGE.md) — préparer et flasher les 2 cartes.
- [Le sampleur](AZ2_SAMPLEUR.md) — état actuel et limites.
- [Licences](AZ2_LICENCES.md) — provenance des composants tiers (banques DX7, samples, cœur d'émulation GB).
- [État des lieux](AZ2_ETAT_DES_LIEUX.md) — journal détaillé de chaque évolution, avec tests matériel.
