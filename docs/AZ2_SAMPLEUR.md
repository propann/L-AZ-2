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

2 patches de depart, **embarques en FLASH** (pas encore charges depuis
la carte SD -- voir plus bas) :

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
demarrage ou a la demande. **Pas fait ce soir** : ca demande de
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
