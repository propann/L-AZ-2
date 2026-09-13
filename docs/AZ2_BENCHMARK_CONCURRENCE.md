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
| Retro-Go | Emulation portable ESP32, SD, Wi-Fi file manager | Bonus fun separe: utile pour apprendre l'ESP32, jamais prioritaire sur l'instrument |

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

## Avantage AZ-2 a construire

AZ-2 doit devenir une machine ou l'on voit tout, ou l'on peut tout reparer, et ou chaque fonction a une place nette. Les concurrents brillent par finition. Nous devons briller par intelligence d'architecture, rapidite de workflow et liberte.

