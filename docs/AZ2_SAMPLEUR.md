# AZ-2 - Sampleur

**[2026-09-18]** Document cree en meme temps que le premier moteur
SAMPLER fonctionnel ("on va mettre en route le sampleur"). Reference
depuis le debut du projet (commentaires `external_psram_size`/
`psramTestBuffer` dans `main.cpp`) mais jamais ecrit avant ce soir.

## Etat actuel (2026-09-18)

Un 6e moteur, `SAMPLER` (`az2::kEngineSampler`, voir
`lib/AZ2_Protocol/AZ2_Protocol.h`), a rejoint DEXED/EPIANO/BRAIDS/
KARPLUS/ANALOG. Lecture PCM 16 bits mono avec suivi de note
(resampling lineaire, pas de bouclage -- un coup, comme un vrai
instrument de percussion) via `AudioPlaySampler`
(`src_teensy/az2_audio/az2_sampler.h`).

2 patches de départ restent **embarqués en FLASH** ; un troisième patch dynamique `GB Capture` est désormais chargé depuis la carte SD vers la PSRAM (voir mise à jour 2026-09-19 plus bas) :

| Index | Nom | Source | Duree | Note racine |
| --- | --- | --- | --- | --- |
| 0 | Kick | `addon/SD/CUSTOM/Kick_1_Simple.wav` (MicroDexed-touch, vendored) | 0.24s | MIDI 36 (C1, convention kick) |
| 1 | Snare | `addon/SD/CUSTOM/Snare_1_Simple.wav` (idem) | 0.38s | MIDI 38 (D1, convention snare) |

Converti en tableaux `const int16_t ... PROGMEM` par un script Python
ponctuel (voir l'historique git, pas versionne comme outil -- lecture
directe du WAV, extraction du PCM, generation du `.h`), dans
`src_teensy/az2_audio/az2_sampler_data.h`. Licence : GPLv3, meme
couverture globale que le reste de MicroDexed-touch vendored dans ce
depot (voir `docs/AZ2_LICENCES.md`).

**Teste sur le vrai materiel** (piste isolee, MUTE sur les 7 autres) :
Kick et Snare confirmes propres par l'utilisateur a la note racine, et
le suivi de pitch confirme (Kick joue une octave au-dessus -> "pareil
plus court", coherent avec un sample qui joue 2x plus vite).

## Pourquoi embarque en flash plutot que charge depuis la carte SD

Le plan d'origine (voir le commentaire `external_psram_size` cite plus
haut, et le readme MicroDexed-touch : "Sample Management from
SD-CARD and PSRAM, samples can be loaded from SD-CARD to PSRAM during
runtime") est de charger les samples depuis la carte SD du Teensy (
slot physique dedie, voir `AZ2_CABLAGE_MASTER.md`) vers la PSRAM au
demarrage ou a la demande. **La première implémentation SD → PSRAM est désormais présente pour le slot `GB Capture`**. Le texte historique ci-dessous décrit l'état initial avant cette intégration :

deposer physiquement des fichiers sur la carte SD reelle inseree dans
le Teensy, une etape materielle que je ne peux pas faire a distance --
seul l'utilisateur peut copier des fichiers sur cette carte depuis un
ordinateur.

Pour livrer un sampleur qui sonne DES CE SOIR sans dependre de cette
etape, les 2 premiers sons (Kick/Snare, petits -- 21 Ko et 34 Ko de
PCM brut) sont a la place compiles directement dans le firmware
(flash Teensy, 7 Mo libres sur 7.75 Mo total -- large marge, voir
`teensy_size` dans la sortie de build). Ca marche immediatement, mais
ne passe pas a l'echelle pour beaucoup de gros samples (chaque Mo de
PCM = un Mo de flash en plus, et un reflash a chaque ajout).

## Prochaine etape (pas faite) : chargement reel depuis la carte SD

Pour aller au-dela de 2 samples fixes :

1. **Cote materiel** : l'utilisateur copie des fichiers `.wav` (16
   bits, mono, 44100 Hz -- meme format que Kick/Snare ci-dessus, pas
   de conversion cote firmware pour l'instant) sur la carte SD du
   Teensy, dans un dossier dedie (ex. `/samples/`).
2. **Cote firmware** : une commande `SAMPLE:<piste>:<nom de fichier>`
   qui lit le WAV depuis `SD.open()` (deja utilisee pour `REC:`, voir
   `gbRecStart()`), verifie le format (16 bits/mono/44100 Hz, sinon
   `sendCommandError`), copie le PCM en PSRAM (`EXTMEM`, 16 Mo
   disponibles sur cette carte -- voir `external_psram_size`), puis
   appelle `AudioPlaySampler::setSample()` avec ce buffer PSRAM plutot
   qu'un tableau flash -- l'API ne fait deja aucune difference entre
   les deux (voir le commentaire en tete de `az2_sampler.h`).
3. **Cote ecran** : une page/liste pour parcourir les fichiers
   `/samples/` de la carte SD (meme principe que la liste de ROM GB,
   `gbScanRoms()`/`drawRetroPage()`) et assigner un fichier a une piste
   -- remplace `PATCH:<piste>:<index>` (qui reste valable pour les 2
   samples embarques par defaut) par un choix de fichier.

Aucun de ces 3 points n'est fait ce soir -- l'objectif etait "mettre en
route le sampleur" avec quelque chose qui sonne reellement d'abord,
pas l'architecture complete de gestion de bibliotheque de samples.

## Reglages non exposes (pas de "patch complet" pour ce moteur)

Contrairement aux 5 autres moteurs (voir DXR:/EXP:/BXP:, meme soiree),
SAMPLER n'a pour l'instant AUCUN parametre reglable au-dela du choix
du sample (PATCH:) -- pas de point de bouclage, pas de reglage de
niveau/pan par sample, pas de reverse. Le filtre (FILT:) et
l'enveloppe partagee (ENV:) s'appliquent deja (memes lignes 0-5 de la
page PATCH, voir `patchRowActive()` -- aucun garde-fou special pour
SAMPLER, se comporte comme KARPLUS/ANALOG de ce point de vue). A
enrichir si le besoin s'en fait sentir (voir la structure `EXP:`/`BXP:`
pour le patron a suivre : `SXP:<piste>:<index>:<valeur>` serait le nom
naturel).


## Mise à jour 2026-09-19 — GB Capture dynamique

Le moteur SAMPLER expose désormais **3 patches** :

| Index | Nom | Source | Stockage | Sample rate |
| --- | --- | --- | --- | --- |
| 0 | Kick | embarqué | Flash | 44,1 kHz |
| 1 | Snare | embarqué | Flash | 44,1 kHz |
| 2 | GB Capture | dernier WAV Game Boy valide | PSRAM Teensy | lu depuis le WAV |

Le chemin complet est maintenant intégré dans le code :

1. Le son Game Boy peut être enregistré en WAV mono 16 bits sur la SD du Teensy.
2. À l'arrêt d'un enregistrement réussi, le WAV est relu et validé.
3. Son PCM est chargé dans une zone PSRAM dédiée.
4. Le patch partagé `SAMPLER / GB Capture` pointe vers ce buffer.
5. Les pistes déjà positionnées sur ce patch sont rafraîchies.
6. Au redémarrage, le dernier `/samples/SAMPLE_*.wav` disponible est recherché et rechargé automatiquement.

La zone dynamique est dimensionnée pour **30 secondes à 14 kHz mono 16 bits**, soit environ **840 Ko**. Si la PSRAM n'est pas détectée, le WAV reste enregistré sur SD mais n'est pas annoncé comme patch jouable.

`AudioPlaySampler` prend maintenant le sample rate source en paramètre. Une capture 14 kHz rejouée à sa note racine conserve donc sa vitesse/hauteur attendue au lieu d'être interprétée à tort comme du 44,1 kHz.

### Limites restantes

Cette intégration fournit un slot dynamique unique, « dernier GB Capture ». Elle ne remplace pas encore un gestionnaire de banque complet : navigation de dizaines/centaines de WAV, renommage, suppression, découpage, trim/normalisation et affectation de plusieurs captures restent des évolutions.

Le code doit encore être qualifié sur le Teensy réel avec PSRAM et carte SD avant d'être considéré comme une fonction matérielle validée.

## Mise à jour 2026-09-19 — éditeur de kits de pads

Dans **AUDIO > SAMPLEUR**, la grille compacte de 16 pads sert à choisir et écouter un pad. La liste de droite respecte l'arborescence réelle de `/samples` sur la carte SD du Teensy : elle affiche six fichiers ou dossiers du niveau courant. Toucher un dossier ou appuyer sur A l'ouvre ; C ou « < » remonte au parent, puis revient à AUDIO depuis la racine. Dans un dossier, choisir un WAV puis **AFFECTER**, ou le glisser vers un pad, l'assigne. La croix haut/bas parcourt la liste, gauche/droite choisit le pad, A ouvre/affecte et B joue. Le choix devient jouable dès que le Teensy confirme `PADSAMPLE:...:READY`.

Quatre kits indépendants sont enregistrables sur la carte SD de l'écran dans `/kits/0.kit` à `/kits/3.kit`. **SAUVER** écrit le kit choisi avec version et CRC via la sauvegarde temporaire ; **CHARGER** remet les 16 pads à zéro puis recharge les affectations. Le projet global sauvegarde aussi ces affectations. La page **SONG** regroupe le chaînage ; la page **PROJETS** présente séparément les quatre slots avec **CHARGER** et **SAUVER**.

Formats affichés : WAV mono PCM 16 bits, 8–48 kHz, dans la capacité du buffer de chaque pad. Les chemins de 63 caractères ou plus ne sont pas présentés par la liste. La navigation ouvre autant de niveaux de sous-dossiers que le chemin le permet. Les essais tactiles et le cycle réel de sauvegarde/chargement restent à vérifier sur le prototype.

La nouvelle navigation hiérarchique est vérifiée par le protocole sur le Teensy réel : la racine retourne deux WAV puis les dossiers `BASS`, `CYMBALS`, `KICKS`, `LOOPS` ; `/samples/BASS` retourne ses WAV. Le firmware écran correspondant est compilé, mais son flash attend une nouvelle entrée manuelle en mode BOOT après l'échec automatique `0x14`.

Validation du protocole sur le prototype : le Teensy flashé renvoie six vrais chemins pour `SAMPLELIST:0`, puis `SAMPLELIST:DONE:7:6` sans parcourir toute la bibliothèque. `PADSAMPLE?` renvoie les huit samples du kit de démarrage et confirme que les pads 8–15 sont vides. Après entrée manuelle en mode BOOT par l'utilisateur, l'écran ESP32-S3 a été flashé avec succès (hachage vérifié). Navigation simulée par boutons jusqu'à AUDIO > SAMPLEUR : l'écran demande et reçoit bien les deux réponses. Le fichier `/samples/BASS/Bass_26.wav` a été affecté au pad 8 vide ; le Teensy a répondu `PADSAMPLE:8:READY`. B déclenche `PAD:08:DOWN` puis `UP`, et le Teensy confirme `LED:08:ON/OFF`. L'écoute et le rendu tactile restent à confirmer humainement ; aucun kit ni projet n'a été écrasé pour cet essai.
