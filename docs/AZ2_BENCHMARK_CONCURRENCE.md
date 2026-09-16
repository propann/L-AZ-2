# AZ-2 - Benchmark concurrence et angle d'attaque

Objectif: comprendre les machines qui occupent deja le terrain, puis definir ce qu'AZ-2 doit faire mieux, autrement, ou plus librement.

## Positionnement AZ-2

AZ-2 n'essaie pas d'etre une copie low-cost d'une groovebox connue. Le bon axe est:

- immediat comme une petite groovebox;
- lisible comme une workstation tactile;
- rapide comme un instrument dedie;
- ouvert comme un projet DIY documente;
- separe en deux cerveaux: ESP32 pour controle, Teensy pour audio.

La promesse: une machine de scene et d'atelier, pas une boite noire.

## Concurrents directs

| Machine | Forces | Limites observees | Lecon pour AZ-2 |
| --- | --- | --- | --- |
| Elektron Digitakt II | 16 pistes samples stereo/MIDI, sequencer tres profond, parameter locks, performance solide | Prix eleve, logique dense, ecran compact, ecosysteme ferme | Reprendre l'idee de locks/conditions, mais avec UI plus visuelle et fichiers ouverts |
| Synthstrom Deluge | Synth, sampler, sequencer autonome, grille lumineuse, firmware open source/community | Interface puissante mais abstraite, apprentissage long, pas d'ecran tactile riche | Grille + autonomie + ouverture sont des armes; AZ-2 doit etre plus explicite a l'ecran |
| Dirtywave M8 Model:02 | Tracker portable, 8 pistes flexibles, moteurs synth/sampler/FM, batterie longue, microSD | Workflow tracker exigeant, ecran 3.5", tres menu-driven | Garder la puissance compacte, mais offrir un mode pad/sequencer plus direct |
| Polyend Tracker Mini | Tracker moderne, sampling, synth engines, sequencer 16 pistes | Ergonomie orientee tracker, edition live moins tactile que pads + ecran | AZ-2 peut proposer un mode tracker plus tard, pas comme navigation principale |
| Novation Circuit Tracks | Tres immediat, 2 synths + 4 drums + 2 MIDI, batterie, workflow rapide | Edition sonore limitee sans logiciel, peu d'ecran, structure fermee | AZ-2 doit garder l'immediatete mais afficher clairement ce qui se passe |
| Teenage Engineering EP-133 K.O. II | Sampler/composer compact, fun, 128 MB, MIDI/sync, fonctionne sur piles | Memoire limitee, UI cryptique, construction tres orientee produit ferme | AZ-2 peut battre la lisibilite, le stockage SD, la reparabilite et l'extension |
| Roland MC-101 | 4 pistes, clips, sons Roland, portable | Seulement 4 pistes, edition profonde moins agreable sur petit format | Eviter le piege des sous-menus: ecran carre = tableau de bord permanent |
| Sonicware LIVEN | Prix accessible, specialisations fun, sequencer 4 pistes | Machines specialisees, ecran/edition limites | AZ-2 peut etre specialise par firmware/profil, pas par achat d'une nouvelle boite |
| Akai MPC Live II / III | Standalone complet, grand tactile, sampling serieux, Wi-Fi/Bluetooth, batterie | Gros, cher, OS lourd, moins "instrument DIY" | Garder l'idee workstation, refuser la lourdeur: AZ-2 doit booter vite et rester jouable |
| 1010music Blackbox | 4" tactile, microSD, sampler compact, scenes/mix/effects | Tres sampler, controle physique limite | Confirme le bon choix: 4" tactile + SD est un excellent noyau |

Sources principales consultees:

- Elektron Digitakt II: https://www.elektron.se/
- Synthstrom Deluge: https://synthstrom.com/product/deluge/
- Dirtywave M8: https://dirtywave.com/products/m8-tracker-model-02
- Polyend Tracker Mini: https://polyend.com/
- Novation Circuit Tracks: https://us.novationmusic.com/
- Teenage Engineering EP-133: https://teenage.engineering/
- Roland MC-101: https://www.roland.com/
- Sonicware LIVEN: https://sonicware.jp/
- Akai MPC Live: https://www.akaipro.com/mpc-live-2
- 1010music Blackbox: https://1010music.com/

## Projets ouverts / DIY a surveiller

| Projet | Ce qui compte | Lecon pour AZ-2 |
| --- | --- | --- |
| MicroDexed-touch | Teensy 4.1 + PCM5102A + synth/groovebox deja proche de notre base | Recuperer le moteur audio, pas l'UI complete |
| Zynthian | Plateforme open synth Linux, multi-engine, web config, audio/MIDI | Bonne reference pour config web et philosophie ouverte |
| Monome Norns | Petit ordinateur musical open, scripts, communaute forte | La communaute aime les machines ouvertes si elles sont poetiques et jouables |
| LMN-3 | DAW-in-a-box open source sur Raspberry Pi | Reference pour workflow DAW autonome, mais AZ-2 reste microcontroleurs en v0 |
| Retro-Go | Emulation portable ESP32, SD, Wi-Fi file manager | Bonus fun separe: utile pour apprendre l'ESP32, jamais prioritaire sur l'instrument. **Etudie et ecarte pour le mode JEUX v0 (2026-09-14)** : ESP-IDF natif (pas Arduino) + ecrans SPI ILI9341-only officiellement supportes, incompatible avec notre ecran RGB parallele sans un vrai chantier (double demarrage OTA + pilote ecran a ecrire) -- voir [AZ2_EMULATION_JEUX.md](AZ2_EMULATION_JEUX.md) pour l'alternative retenue (Walnut-CGB, GB/GBC, Arduino-natif, licence MIT) |

Sources principales consultees:

- MicroDexed-touch sur PJRC: https://www.pjrc.com/microdexed-touch-diy-synth-groovebox/
- Zynthian: https://zynthian.org/
- Monome Norns docs: https://monome.org/docs/norns/
- LMN-3: https://github.com/FundamentalFrequency/LMN-3-DAW
- Retro-Go: https://github.com/ducalex/retro-go

## Ce qu'il faut exploser

### 1. Lisibilite

Beaucoup de grooveboxes sont puissantes mais obligent a deviner. AZ-2 doit montrer:

- piste selectionnee;
- pad / step actif;
- moteur sonore;
- parametres principaux;
- etat Teensy;
- etat SD/Wi-Fi;
- latence de communication;
- niveau audio et mute.

L'ecran ESP32 n'est pas une decoration: c'est la tour de controle.

### 2. Vitesse de jeu

La machine doit repondre vite aux pads:

| Action | Cible AZ-2 |
| --- | --- |
| Pad down -> commande envoyee | < 2 ms apres scan stable |
| ESP32 -> Teensy UART | 230400 baud v0, protocole compact |
| Reception Teensy -> note audio | dans la boucle audio, sans allocation dynamique |
| Retour LED | immediat cote ESP32, confirme ensuite par Teensy si besoin |

Pour le musicien, la LED peut reagir tout de suite. Le Teensy confirme l'audio. C'est plus nerveux.

### 3. Firmware divise intelligemment

AZ-2 doit refuser le gros firmware monolithique. Deux cerveaux:

- ESP32 = humain, ecran, pads, SD, Wi-Fi, web config;
- Teensy = temps reel audio, synthese, samples, sequenceur critique.

Si l'ESP32 plante, le Teensy doit pouvoir muter proprement ou continuer selon le mode. Si le Teensy reboot, l'ESP32 doit l'afficher et resynchroniser.

### 4. Fichiers ouverts

Les projets doivent etre lisibles sur SD:

```text
/az2/projects/nom_du_projet/project.json
/az2/projects/nom_du_projet/patterns/
/az2/projects/nom_du_projet/presets/
/az2/projects/nom_du_projet/samples/
```

Pas de format opaque en v0. Un futur outil desktop/web pourra comprendre les fichiers.

### 5. Modes qui servent vraiment

AZ-2 doit separer les modes:

| Mode | But |
| --- | --- |
| Performance | Jouer, muter, lancer patterns, modifier macro |
| Sequencer | Programmer steps, locks, variations |
| Sound | Editer moteur synth/sample |
| Mixer | Niveaux, panoramique, effets, mute |
| Project | Charger/sauver/exporter |
| Hardware Test | Tester pads, LEDs, UART, SD, DAC |
| Retro | Bonus Game Boy separe |

## Decisions produit

| Sujet | Decision v0 |
| --- | --- |
| Audio | Teensy 4.1 + PCM5102A, pas ESP32 audio |
| Controle | ESP32-S3 ecran + pads + LEDs + SD + Wi-Fi |
| Protocole | UART texte compact au debut, binaire versionne ensuite si necessaire |
| MIDI/CV | Hors v0, architecture prevue mais pas bloquante |
| Samples | SD cote ESP32, transfert vers Teensy a definir |
| MicroDexed | Source d'inspiration moteur, pas firmware final tel quel |
| Retro-Go | Bonus isole, jamais melange au firmware musical principal |

## Topo 2026-09-15 : ou en est AZ-2 reellement vs le marche

Etat reel de la machine a cette date (pas une intention -- ce qui tourne
et a ete teste) : 8 pistes, sequenceur 16 pas avec note PAR PAS,
tempo+division reglables a l'ecran, 5 moteurs de synthese
selectionnables/reglables par piste (Dexed FM, mda ePiano, Braids,
Karplus-Strong corde pincee, oscillateur analogique), bus d'effets
maitre (reverb+delay) pilote par 3 potards physiques, ecran tactile
480x480 avec plusieurs pages de controle, croix+4 boutons+3 potards
cables directement sur le Teensy. PSRAM 16 Mo confirmee mais pas encore
utilisee (pas de sampleur). Pas de MIDI, pas de sauvegarde persistante,
pas de carte SD branchee.

### Ce qui manque le plus, par rapport aux concurrents ci-dessus

| Manque | Present chez | Effort estime | Priorite |
| --- | --- | --- | --- |
| Mute/solo par piste | Quasi tous (Digitakt, Circuit Tracks, M8...) | Faible -- juste un gain a 0 par piste, mapping bouton a definir | **Haute** |
| Sauvegarde/rechargement (patterns+patchs survivent a une coupure) | Tous, meme les moins chers | Moyen -- PSRAM ou flash interne Teensy, format a definir | **Haute** |
| Swing/groove (decalage d'un pas sur deux) | Quasi tous | Faible -- decalage de timing dans advanceSequencer() | **Haute** |
| Volume/pan reglable PAR piste (pas juste le bus maitre) | Tous | Moyen -- deja identifie feuille de route etape 4 | Moyenne |
| MIDI (sync/notes in-out) | Digitakt, Circuit Tracks, M8, MPC... quasi tous | Moyen-eleve -- USB MIDI natif dispo sur Teensy (classe audio+MIDI) | Moyenne |
| Sons samples/drums | Digitakt, MPC, Blackbox, EP-133 (tous orientes sample) | Eleve -- demande une carte SD (voir roadmap etape 6) | Basse (bloque par le materiel) |
| Enchainement de patterns / mode song | M8, Digitakt, Circuit Tracks | Eleve -- refonte du sequenceur (patterns multiples par piste) | Basse |

### Ce qu'on supprime (nettoyage, pas de perte fonctionnelle reelle)

- ~~**Page ENCODEURS**~~ -- **[FAIT]** retiree le 2026-09-15 (plus
  aucune source de `MACRO:` depuis l'abandon du Pico).
- ~~**`src_esp32/retro-go-master/`**~~ (Retro-Go vendored, 134 Mo,
  1670 fichiers) -- **[FAIT, 2026-09-15]** supprime (`git rm -r`).
  Etudie et ECARTE le 2026-09-14 pour le mode JEUX (ESP-IDF,
  incompatible avec notre ecran RGB parallele sans un vrai double
  demarrage -- voir AZ2_EMULATION_JEUX.md), jamais le chemin retenu
  (Walnut-CGB). Reste dans l'historique git si jamais utile de le
  retrouver.
- **`kHelloKeypad`** dans AZ2_Protocol.h : plus emis par personne depuis
  l'abandon du Pico -- laisse par prudence (verifie encore le
  2026-09-15, toujours candidat a la suppression si rien ne le
  reutilise, mais lie a `src_pico/` garde volontairement pour
  reference, voir ci-dessous).
- **`src_pico/`** : deja hors des builds par defaut (voir
  `platformio.ini`), garde uniquement pour reference -- a
  supprimer completement si on est surs de ne jamais y revenir (pour
  l'instant garde par prudence, cout de stockage negligeable).

### Ce qui distingue deja AZ-2 (a ne pas perdre en avancant)

Diagnostic materiel documente en public (utile pour du DIY/reparation),
architecture ouverte en 2 cartes lisibles, 5 moteurs de synthese
radicalement differents deja en place avec beaucoup de marge CPU (~8%
de pic mesure sur 8 pistes + FX), et un vrai potentiometre physique
relie en direct au mixeur (pas juste un menu) -- rare meme chez les
concurrents cites.

## Avantage AZ-2 a construire

AZ-2 doit devenir une machine ou l'on voit tout, ou l'on peut tout reparer, et ou chaque fonction a une place nette. Les concurrents brillent par finition. Nous devons briller par intelligence d'architecture, rapidite de workflow et liberte.

## Mise a jour 2026-09-16 -- nouvelle recherche + etat reel vs marche

Recherche demandee ("on fait une recherche sur nos concurrents, liste
des ameliorations indispensables") -- 2 machines ajoutees au comparatif,
les autres reverifiees. Sources en bas de section.

| Machine | Etat 2026 | Lecon pour AZ-2 |
| --- | --- | --- |
| **Teenage Engineering OP-XY** (nouveau, $2299, remise a $1699 en promo) | 8 pistes, 8 moteurs synth + 3 sampleurs, 24 voix, ecran 480x222, gyroscope pour macro-controle par inclinaison, "Brain" auto-transpose selon la tonalite du morceau, CV/Gate + MIDI + Bluetooth | Le prix confirme que la lisibilite/l'ecran + le controle direct (pas juste des menus) sont ce qui justifie un tarif eleve -- AZ-2 doit rester lisible SANS ce budget. "Brain" (auto-transpose) est une bonne idee a retenir une fois nos gammes/accords en place |
| **nanoloop (Game Boy)** (mono/one, toujours vendu en cartouche) | Synthetiseur+sequenceur MINIMALISTE tournant sur le vrai chip son GB (carre/carre/onde 4 bits/bruit), interface reduite a une grille, sync analogique + transfert de fichiers par jack | C'est litteralement notre idee de "sampler la Game Boy" mais dans l'autre sens (composer AVEC le chip GB) -- confirme que le public pour "faire de la musique avec/depuis une Game Boy" existe et est actif. AZ-2 peut aller plus loin : ECHANTILLONNER un vrai jeu GB (musique/bruitages) puis les rejouer sur nos 5 moteurs, pas juste piloter le chip |
| **Dirtywave M8 Model:02** (maj $790, ecran 3.5" IPS, 12h batterie) | Micro integre pour echantillonner, sampleur mono/stereo 8/16/24 bits, USB-C, 64 Go de carte SD fournie avec demos/presets | Le micro integre pour sampler a la volee est une fonction tres appreciee -- notre "sampler la Game Boy" est un axe proche (source audio captive au lieu du micro) mais on pourrait aussi envisager un micro plus tard |
| **Polyend Tracker Mini** (maj) | Devenu quasi poche, batterie, micro integre, sample stereo (nouveaute vs l'original), 12 pistes audio USB-C, 48 instruments/256 patterns/128 pas par projet | Confirme stereo + micro comme standard attendu desormais chez un tracker portable ; notre pattern 8 pistes/16 pas reste modeste en comparaison -- a garder simple pour l'instant (deja plus complexe que prevu ce mois-ci) mais noter que 16 pas est petit face a 128 |

Sources additionnelles : [Teenage Engineering OP-XY](https://teenage.engineering/products/op-xy), [Sound on Sound OP-XY](https://www.soundonsound.com/reviews/teenage-engineering-op-xy), [nanoloop one](http://www.nanoloop.com/one/), [CDM -- nanoloop reborn](https://cdm.link/nanoloop-game-boy-hardware/), [Dirtywave M8 Model:02](https://dirtywave.com/products/m8-tracker-model-02), [Gearnews M8 Model:02](https://www.gearnews.com/dirtywave-m8-tracker-model-02/), [Sound on Sound Polyend Tracker Mini](https://www.soundonsound.com/reviews/polyend-tracker-mini), [AltWire Polyend Tracker Mini](https://altwire.net/polyend-tracker-mini-review/).

### Etat reel AZ-2 au 2026-09-16 (vs le topo du 2026-09-15)

Beaucoup avance depuis le dernier topo concurrence : tracker colonnes
NOTE/INST/FX/VAL (vue unique, plus de grille), 8 patterns + chainage
song basique, gammes (verrouillage a la saisie), filtre resonant +
ADSR **reellement editables par piste** (pas juste prevus), oscilloscope
temps reel, sauvegarde/chargement de patch (slots sur SD), reglages
propres au moteur Dexed (algo/feedback DX7). Voir
[AZ2_ETAT_DES_LIEUX.md](AZ2_ETAT_DES_LIEUX.md) pour le detail verifie
en reel vs seulement compile.

### Liste des ameliorations indispensables (priorisee)

Recoupe le tableau "ce qui manque le plus" du 2026-09-15 (toujours
valable dans l'ensemble) avec la recherche fraiche ci-dessus :

| # | Amelioration | Pourquoi indispensable | Effort estime |
| ---: | --- | --- | --- |
| 1 | **Mute/solo par piste** | Present chez TOUS les concurrents cites, y compris les moins chers (Circuit Tracks) ; sans ca on ne peut pas "jouer" en scene, juste programmer | Faible -- gain a 0 par piste, mapping bouton a definir (C/D libres, voir "gachette") |
| 2 | **Sauvegarde/chargement de PROJET complet** (pas juste un patch) | Tous les concurrents survivent a une coupure ; on a la sauvegarde de patch (2026-09-16) mais pas patterns+song+BPM+scale en un fichier | Moyen -- meme mecanique que savePatchSlot()/loadPatchSlot(), format a etendre (JSON ou texte simple sur SD) |
| 3 | **Swing/groove** | Present chez quasi tous (Polyend, LSDJ "groove screen", M8) ; sans lui le sequenceur sonne mecanique | **Correction 2026-09-16** : moyen, pas faible -- le tempo est un seul `IntervalTimer` a periode fixe, un vrai swing alterne 2 durees de tick et doit reconfigurer le timer depuis sa propre ISR (voir note dans AZ2_FEUILLE_DE_ROUTE.md) -- a verifier avec un analyseur logique, pas juste a l'oreille |
| 4 | **Volume/pan par piste** | Tous les concurrents, y compris les moins chers | Moyen -- deja identifie feuille de route etape 4, pas encore fait |
| 5 | **Micro/sampler integre** (ou a defaut, sampler-depuis-GB deja prevu) | M8, Polyend Tracker Mini, Blackbox, EP-133 -- tous orientes sample en 2026 | Eleve -- bloque par la carte SD Teensy (preparee le 2026-09-16, FAT32, a inserer et verifier), + nouveau moteur `AudioPlaySdWav`/`AudioPlaySdRaw` |
| 6 | **MIDI (sync/notes in-out)** | Quasi tous les concurrents cites | Moyen-eleve -- USB MIDI natif deja dispo sur le Teensy (classe audio+MIDI, `USB_MIDI_SERIAL` deja dans platformio.ini) |
| 7 | **Accords / plusieurs notes par pas** | LSDJ/M8 le permettent en partie, demande deja documentee (AZ2_TRACKER_ETUDE.md etape 5) | **Correction 2026-09-16** : plus gros que prevu -- `kNotesPerTrack=2` n'est QUE la polyphonie interne du moteur (Dexed/EPiano), pas une 2e note par pas dans les donnees du sequenceur (`stepNote[kStepCount]` est un seul octet par pas). Un vrai accord demande d'etendre `stepNote` a 2 notes partout (Teensy ET ESP32 : protocole NOTE:, tracker, sauvegarde) -- touche le meme code que le bug de gel deja rencontre cette session (`advanceTick()`). A faire avec du materiel pour verifier, pas a l'aveugle |
| 8 | **Clavier tactile comme editeur live** (poser une note sur le pas selectionne en tapant un pad) | Aucun concurrent direct ne fait exactement ca, mais c'est un gain de vitesse d'edition documente (etape 6 de l'etude tracker) | Moyen -- routage a ecrire cote ESP32 (pad -> NOTE: au lieu de -> PAD: quand un pas est selectionne) |

Le classement change peu depuis le 2026-09-15 : mute/solo et sauvegarde
de projet restent les 2 trous les plus visibles face a n'importe quel
concurrent, meme un low-cost.

