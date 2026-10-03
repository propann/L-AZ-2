# Journal des changements — AZ-2

Chaque entrée renvoie aux commits concernés. « Validé » signifie mesuré au
banc (`tools/engine_bench.py`, `GB:PERF`) ou confirmé à l'écoute / à l'œil sur
la machine ; « compile » seul n'est jamais présenté comme une validation.
Source de vérité de l'état courant : [`docs/AZ2_ETAT_ACTUEL.md`](docs/AZ2_ETAT_ACTUEL.md).

## Son vivant — 2 et 3 octobre 2026 (branche `son-vivant`)

### Moteurs et niveaux
- Banc de mesure automatique des moteurs par le port USB du Teensy
  ([`tools/engine_bench.py`](tools/engine_bench.py)) et balayage des 523
  patches Teensy ([`tools/bench_data/`](tools/bench_data/)). — `72d959a`, `4990027`
- EPIANO Default, Mellow, Autopan, Tremolo étaient **muets** (paramètres des
  presets mdaEPiano décalés d'un cran à la copie) — corrigés. — `72d959a`
- Niveaux **équilibrés** par moteur puis par patch (tableau généré par
  [`tools/gen_patch_trim.py`](tools/gen_patch_trim.py)) : l'écart allait
  jusqu'à ×25 entre moteurs et ×50 entre patches. — `72d959a`, `4990027`

### Effets par piste
- Section EFFETS : drive (niveau constant), crush, LFO sur le filtre,
  delay avec vrai feedback. — `4990027`
- Filtre LP/BP/HP, tremolo, ring mod, flanger, envoi reverb post-volume,
  LFO et delay synchronisés au tempo. — `9821e94`, `578faa4`
- Verrous de paramètre par pas dans le tracker (DRIVE, WAH, REVRB, RING,
  TREM, FLANG, DMIX), appliqués hors ISR. — `2829472`
- 12 presets d'effets (PROPRE, DUB, LOFI, WAH, ESPACE, ROBOT, TREMOL, JET,
  SATURE, CHIP, PING, CATHED). — `20bb1fe`
- Chorus maître modulé ; reverb, delay et chorus généraux réglables dans la
  ligne MASTER du MIXER (ils n'avaient plus de commande à l'écran). — `fa27d25`, `6e840f6`
- Charge mesurée : CPU 8–12 %, mémoire audio ≤ 310/700 blocs.

### Interface
- Mode EFFETS au clic de l'encodeur 3 depuis presque toutes les pages
  (l'encodeur 2 garde pad virtuel et arpégiateur). — `9d62357`
- Bouton MOT> dans PATCH pour changer de moteur sans quitter la page. — `5c1d63d`
- SEQ. PAS : A maintenu + HAUT/BAS change la note (jamais câblé jusque-là). — `578faa4`
- Fenêtre MOTEURS qui sautillait : seule l'icône est encore animée. — `5b64310`

### Corrections trouvées en route
- Un projet utilisant les effets de pas CRUSH ou DELAY ne se rechargeait
  plus (validateur trop strict). — `4990027`
- Pièges de la bibliothèque audio documentés dans le code : oscillateur à
  amplitude 0 muet, flanger qui n'utilise que la moitié de son tampon,
  waveshaper sans courbe muet. — `9821e94`, `fa27d25`

## Console et réparation — 1er et 2 octobre 2026 (PR #4, branche `nes-emulation`)

- **Son des émulateurs** réparé : la régression du 29/09 (latence croissante
  puis blocs perdus) est corrigée par une source audio tirée par l'ISR du
  Teensy ; PANIC vide aussi ce chemin. Plus de bruit parasite à l'écoute. — `ecca553`
- **Game Boy et Game Boy Color à 59,7 fps en X2 et X3** sur Walnut-CGB :
  pipeline d'affichage sans attente, une image sur 3 dessinée en X3,
  tampons d'envoi série, sauvegarde auto sans à-coup. — `6c952c7`, `e73adde`, `b211b4d`
- Une liste de ROM par carte (`.gb` / `.gbc`), noms de ROM lisibles. — `81d4a58`, `4885275`
- Écran RGB à 10 MHz : plus de lignes décalées. — `81132b7`
- Neo Geo Pocket (cœur RACE, GPLv2 seule) retirée pour incompatibilité de
  licence avec la GPLv3. — `8cc3e3a`
- Documentation et présentation remises à niveau. — `32baee9`

## Chasse au bip parasite — 28 et 29 septembre 2026

- MIDI retiré du code (puce non montée), métronome supprimé, asservissement
  audio GB, commande `STATUS_MS`. Le bip a disparu depuis le 1er octobre,
  mais sa cause racine n'est pas prouvée : voir
  [`docs/AZ2_BIP_PARASITE_2026-09-28.md`](docs/AZ2_BIP_PARASITE_2026-09-28.md). — `bbb584c`, `4c6d09b`, `c872885`, `779ae0e`

## Intégration NES — 27 septembre 2026

- Émulateur NES (Anemoia-ESP32) : ROM, contrôleur, audio APU, sauvegardes
  SRAM ; 49–50 fps mesurés. — `626a5bb`, `b7cdccb`

Pour l'historique antérieur, voir `git log` et les documents datés de
[`docs/`](docs/README.md).
