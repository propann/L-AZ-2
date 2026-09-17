# Etude de concurrence des trackers et grooveboxes

Date de reference : 17 septembre 2026  
Projet : AZ-2  
But : identifier les fonctions qui definissent le marche actuel et construire un avantage produit credible

## Resume strategique

Le marche est deja tres fort dans quatre directions :

1. Dirtywave M8 domine le tracker portable profond.
2. Polyend rend le tracker plus lisible et plus proche d'une workstation.
3. Elektron domine le sequenceur a locks, conditions et performance.
4. Ableton, Akai et 1010music rendent la production plus visuelle et tactile.

AZ-2 ne gagnera pas en copiant une de ces machines. Son angle le plus fort est de reunir plusieurs manieres d'editer le meme morceau sans convertir ni dupliquer les donnees :

- vue Tracker pour la densite et la precision ;
- vue Step pour l'immediatete ;
- vue Piano Roll pour les melodies ;
- vue Arranger pour finir un morceau ;
- ecran tactile carre lisible ;
- commandes physiques minimales et constantes ;
- fichiers ouverts ;
- architecture reparable et documentee.

La proposition gagnante n'est donc pas « plus de fonctions ». C'est « la profondeur d'un tracker sans emprisonner l'utilisateur dans le tracker ».

## Categories du marche

| Categorie | Produits de reference | Force principale |
| --- | --- | --- |
| Tracker portable | Dirtywave M8, Polyend Tracker Mini | Densite, rapidite, autonomie |
| Tracker de table | Polyend Tracker+ | Lisibilite, sampling stereo, synthese |
| Groovebox a sequenceur profond | Elektron Digitakt II, Synthstrom Deluge | Modulation et performance |
| Sampler tactile | 1010music Blackbox, Akai MPC | Edition visuelle et sampling |
| Groovebox immediate | Ableton Move, Circuit Tracks, EP-133 | Demarrage rapide et jeu |
| Micro-workstation | Woovebox | Rapport taille/fonctions extreme |
| Plateforme ouverte | Deluge Community Firmware, Zynthian, MicroDexed-touch | Evolutivite et communaute |

## Dirtywave M8 Model 02

### Position

Le M8 est la reference la plus proche de l'esprit AZ-2 : appareil compact, Teensy, microSD, controles simples et moteur musical tres profond.

### Fonctions fortes

- sequenceur tracker 8 pistes ;
- structure Song, Chain et Phrase ;
- position de lecture independante par piste ;
- mode Live ;
- jusqu'a 128 instruments par morceau ;
- Wavsynth ;
- Macrosynth base sur Mutable Instruments Braids ;
- sampler et editeur de samples ;
- synthese FM ;
- Hypersynth ;
- instruments externes et MIDI ;
- tables de modulation ;
- trois colonnes de commandes par step ;
- grooves, echelles, mixer, EQ et limiteur ;
- rendu du morceau ;
- selection transformable en sample ;
- streaming des samples directement depuis microSD ;
- entree audio stereo ;
- sortie casque ;
- USB audio, MIDI et affichage distant ;
- microphone integre sur Model 02 ;
- ecran IPS 3,5 pouces ;
- autonomie annoncee jusqu'a 12 heures ;
- prix officiel observe : 699 USD.

### Ce que M8 fait mieux que le depot AZ-2 actuel

- produit complet et jouable ;
- moteur multi-synthese mature ;
- sampling reel avec entree audio ;
- sequenceur profond ;
- autonomie ;
- format de projet et workflow stables ;
- documentation de pres de 100 pages ;
- mises a jour actives, firmware 6.6.3C publie le 14 septembre 2026.

### Faiblesses exploitables

- interface tracker obligatoire pour l'essentiel du travail ;
- valeurs souvent hexadecimales ;
- nombreuses combinaisons de boutons ;
- ecran plus petit que celui d'AZ-2 ;
- seulement huit pistes ;
- edition graphique limitee ;
- pas de grand tactile ;
- produit souvent en rupture de stock.

### Reponse AZ-2

AZ-2 doit garder la vitesse D-pad plus boutons du M8, mais afficher les memes donnees en Tracker, Step, Piano Roll et Arranger. Le tactile doit servir a voir, selectionner, deplacer et dessiner ; les encodeurs doivent servir a regler avec precision.

Source principale : https://dirtywave.com/pages/resources-downloads  
Manuel 2026 : https://cdn.shopify.com/s/files/1/0455/0485/6229/files/m8_operation_manual_v20260421.pdf  
Produit : https://dirtywave.com/products/m8-tracker-model-02

## Polyend Tracker Plus

### Fonctions fortes

- sequenceur 16 pistes ;
- huit pistes universelles pour sample, MIDI ou synthese ;
- huit pistes supplementaires pour MIDI ou synthese ;
- sampling et lecture stereo ;
- moteurs de synthese internes ;
- moteur de percussion ;
- song mode ;
- audio USB multipiste ;
- stockage de masse USB ;
- resampling ;
- workflow tracker plus large et plus visuel que le M8 ;
- patterns jusqu'a 128 pas selon la documentation produit.

### Ce que Polyend enseigne

Le public accepte le tracker si l'ecran, les commandes et le navigateur de fichiers reduisent la friction. L'ajout de synthese interne et d'audio USB rapproche le tracker d'une workstation complete.

### Faiblesses exploitables

- separation entre pistes universelles et pistes MIDI ou synthese ;
- architecture tracker toujours dominante ;
- nombre de moteurs simultanes et polyphonie limites ;
- appareil moins compact que M8 ;
- passage entre creation de patterns et arrangement encore specifique au tracker.

### Reponse AZ-2

Unifier toutes les pistes autour d'un type de clip et rendre le moteur interchangeable par piste. Ne jamais creer une « piste pauvre » qui ne peut pas recevoir certains evenements. Si une limite materielle existe, elle doit etre visible et comprehensible.

Source : https://polyend.com/tracker-plus/  
Annonce technique : https://backstage.polyend.com/t/announcing-tracker/11031

## Polyend Tracker Mini

### Fonctions fortes

- approche portable ;
- batterie et microphone ;
- sampling stereo ;
- audio USB ;
- compatibilite de projets avec Tracker+ ;
- fonctions de synthese et sequenceur 16 pistes apportees par les mises a jour recentes.

### Lecon pour AZ-2

La compatibilite des projets entre plusieurs formats de machine est un avantage puissant. AZ-2 et une future AZ-3 devraient partager exactement le meme format de projet, les memes patterns et les memes moteurs supportes.

Source : https://polyend.com/tracker-mini/

## Elektron Digitakt II

### Fonctions fortes

- 16 pistes configurables en audio ou MIDI ;
- samples mono ou stereo ;
- 128 pas par pattern et par piste ;
- longueur et echelle temporelle independantes par piste ;
- parameter locks ;
- trig conditions ;
- microtiming ;
- sequence euclidienne ;
- plusieurs machines de lecture et transformation de samples ;
- trois LFO par piste ;
- song mode et outils de performance ;
- 400 Mo de RAM de projet et 20 Go de stockage annonces ;
- sequence MIDI externe polyphonique.

### Pourquoi il est dangereux

Elektron a transforme le sequenceur en instrument. Les locks et conditions produisent de la variation sans multiplier les pistes. La machine est fiable sur scene et chaque geste physique a une consequence musicale immediate.

### Faiblesses exploitables

- apprentissage exigeant ;
- ecran compact ;
- logique Elektron specifique ;
- fichiers et ecosysteme moins ouverts ;
- prix haut ;
- edition melodique moins visuelle qu'un piano roll tactile.

### Reponse AZ-2

Les fonctions minimales pour etre pris au serieux face a Elektron sont :

- locks de parametres par evenement ;
- probabilite ;
- conditions ;
- microtiming ;
- retrig et ratchet ;
- longueur et resolution par piste ;
- polymetre ;
- automation ;
- snapshots de performance recuperables.

Source : https://www.elektron.se/explore/digitakt-ii  
Manuel : https://www.elektron.se/wp-content/uploads/2025/07/Digitakt-2-User-Manual_ENG_OS1.15A_250708.pdf

## Synthstrom Deluge

### Fonctions fortes

- synthese, sampling, MIDI et sequencing ;
- vue clip et arrangement ;
- grande grille ;
- batterie ;
- creation de morceaux complets ;
- firmware devenu open source ;
- communaute capable d'ajouter des fonctions.

### Faiblesses exploitables

- grille abstraite pour les nouveaux utilisateurs ;
- forte dependance a une facade dense ;
- navigation profonde malgre la puissance ;
- prix et disponibilite.

### Reponse AZ-2

AZ-2 peut reprendre le principe d'un morceau que l'on voit a plusieurs echelles, mais le faire sur l'ecran carre avec des labels explicites. La machine doit etre ouverte sans exiger que l'utilisateur lise le code pour comprendre ses fonctions.

Sources : https://synthstrom.com/product/deluge/  
Firmware communautaire : https://github.com/SynthstromAudible/DelugeFirmware

## 1010music Blackbox

### Fonctions fortes

- ecran tactile 4 pouces ;
- sampling, edition, resampling et sequencing ;
- streaming de longs samples depuis microSD jusqu'a 4 Go ;
- granular ;
- multisampling, jusqu'a 576 samples dans le pool ;
- 16 samples, 16 sequences et 16 sections par preset ;
- piano roll avec multiselection ;
- probabilite par note ;
- live looping ;
- trois sorties stereo plus casque ;
- entree audio stereo ;
- 16 voix partagees ;
- synchronisation et MIDI.

### Pourquoi elle valide AZ-2

Blackbox prouve qu'un ecran tactile 4 pouces peut porter un studio compact. Elle prouve aussi que le streaming SD, l'entree audio et le multisampling sont des attentes credibles dans ce format.

### Faiblesses exploitables

- forte orientation sampler ;
- controle physique limite ;
- sequenceur moins profond que M8 ou Elektron ;
- absence de plusieurs moteurs de synthese avances.

### Reponse AZ-2

Combiner la lisibilite et le tactile de Blackbox avec la vitesse physique du M8 et les locks d'Elektron.

Source : https://1010music.com/product/blackbox

## Ableton Move

### Fonctions fortes

- prise en main immediate ;
- 32 pads retroeclaires sensibles a la pression avec aftertouch polyphonique ;
- drum sampler et melodic sampler ;
- Drift et Wavetable ;
- step sequencer ;
- sampling, slicing et resampling ;
- pistes audio, bounce et warp ;
- microphone, entree ligne, haut-parleur et batterie ;
- effets Ableton ;
- MIDI bidirectionnel ;
- Wi-Fi, Ableton Link, Link Audio et transfert vers Live ;
- 64 Go internes ;
- architecture ARM quatre coeurs et 2 Go de RAM ;
- autonomie annoncee jusqu'a quatre heures.

### Menace

Move ne cherche pas a tout montrer. Il cherche a reduire le temps entre l'idee et le premier groove. Cette vitesse compte davantage qu'une longue liste de moteurs.

### Reponse AZ-2

AZ-2 doit avoir un demarrage direct :

1. nouveau projet ;
2. choix d'un moteur ou kit ;
3. enregistrement ;
4. variation ;
5. arrangement.

Aucun assistant de configuration, reseau ou menu technique ne doit bloquer ce trajet.

Source : https://www.ableton.com/en/move/

## Woovebox

### Fonctions fortes

- taille inferieure a la paume ;
- synthese, sampler, sequenceur et drum machine ;
- 16 pistes ;
- conditions d'evenements et de pistes ;
- reutilisation du tempo, du rythme et des accords ;
- construction de morceau mise au centre ;
- MIDI et sync physiques ;
- MIDI BLE ;
- entree audio ;
- export WAV, MIDI et stems secs ou traites ;
- autonomie annoncee superieure a neuf heures ;
- optimisation DSP tres poussee.

### Lecon

Une petite machine peut finir des morceaux si l'architecture musicale est pensee avant l'interface. Woovebox optimise le chemin vers le Song Mode plutot que d'accumuler des pages.

### Reponse AZ-2

L'Arranger ne doit pas etre une fonction tardive. Il doit exister dans le modele de donnees des la premiere version du sequenceur.

Sources : https://www.woovebox.com/about  
Modeles : https://www.woovebox.com/models

## Teenage Engineering EP-133 KO II

### Fonctions fortes

- sampling immediat ;
- microphone et entree ligne ;
- jeu expressif ;
- effets de performance ;
- alimentation portable ;
- identite visuelle forte ;
- workflow amusant ;
- 128 Mo sur la version actuelle presentee par le constructeur ;
- patterns et groupes orientes performance.

### Faiblesses exploitables

- affichage tres limite ;
- memorisation des fonctions ;
- stockage modeste face a une microSD ;
- format ferme ;
- edition detaillee moins lisible.

### Reponse AZ-2

Ne pas sous-estimer le plaisir. AZ-2 doit etre techniquement profond sans ressembler a un logiciel de comptabilite. Un mode performance, des macros et des effets immediats sont indispensables.

Source : https://teenage.engineering/guides/ep-133

## Akai MPC One G2 et gamme MPC

### Fonctions fortes

- production autonome complete ;
- sampling et chopping ;
- pistes audio ;
- plugins de synthese ;
- arrangement lineaire ;
- effets ;
- pilotage MIDI ;
- grand tactile ;
- integration ordinateur ;
- traitements lourds comme time-stretch et separation de stems selon modele et options.

### Faiblesses exploitables

- approche plus proche d'un ordinateur ;
- temps de demarrage et complexite superieurs ;
- encombrement ;
- menus nombreux ;
- cout.

### Reponse AZ-2

AZ-2 ne doit pas essayer de battre une MPC sur la quantite de logiciels. Elle doit gagner sur la concentration, le demarrage, la transparence du projet et la sensation d'instrument.

Source : https://www.akaipro.com/

## Matrice comparative

Legende : fort = fonction centrale et mature ; moyen = presente avec limites ; faible = secondaire ou absente.

| Produit | Tracker | Step | Piano roll | Arrangement | Sampling direct | Synthese | Locks et conditions | Tactile | Ouverture |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Dirtywave M8 | Fort | Faible | Faible | Fort | Fort | Fort | Fort | Non | Moyen |
| Polyend Tracker+ | Fort | Moyen | Faible | Fort | Fort | Fort | Fort | Non | Faible |
| Digitakt II | Faible | Fort | Moyen | Fort | Fort | Faible | Tres fort | Non | Faible |
| Deluge | Moyen | Fort | Moyen | Tres fort | Fort | Fort | Fort | Non | Tres fort |
| Blackbox | Faible | Moyen | Fort | Moyen | Fort | Faible | Moyen | Fort | Moyen |
| Ableton Move | Faible | Fort | Moyen | Moyen | Fort | Fort | Moyen | Non | Faible |
| Woovebox | Faible | Fort | Faible | Fort | Fort | Fort | Fort | Non | Faible |
| EP-133 | Faible | Fort | Faible | Moyen | Fort | Faible | Moyen | Non | Faible |
| MPC | Faible | Fort | Fort | Tres fort | Tres fort | Tres fort | Fort | Fort | Faible |
| AZ-2 cible | Fort | Fort | Fort | Fort | A decider materiellement | Multi-moteur | Fort | Fort | Tres fort |

## Fonctions devenues obligatoires

Pour concurrencer serieusement ces machines, AZ-2 doit proposer au minimum :

### Sequenceur

- 8 pistes minimum pour le prototype, architecture extensible a 16 ;
- patterns de 64 pas minimum, extensibles a 128 ;
- longueur et resolution independantes par piste ;
- microtiming ;
- swing global et par piste ;
- probabilite ;
- conditions ;
- ratchets ;
- locks de parametres ;
- automation ;
- polymetre ;
- gammes et quantification ;
- duplication, variation et mutation controlee ;
- chaines, scenes et arrangement.

### Edition

- Tracker ;
- Step ;
- Piano Roll ;
- Arranger ;
- selection multiple ;
- copier, coller, dupliquer et transposer ;
- undo et redo ;
- zoom temporel ;
- meme evenement visible dans toutes les vues.

### Son

- sampler ;
- multisampler ;
- synthese FM issue de MicroDexed ;
- synthese VA ;
- wavetable ;
- moteur drums ;
- granular en phase ulterieure ;
- effets par piste et master ;
- limiteur de securite ;
- resampling si une entree audio est ajoutee.

### Integration

- MIDI USB et DIN ou TRS ;
- MIDI clock ;
- import et export MIDI ;
- export WAV et stems ;
- audio USB si la chaine materielle le permet ;
- gestion microSD ;
- sauvegarde atomique ;
- projets lisibles et versionnes ;
- mises a jour sures ;
- Wi-Fi desactive en performance mais disponible en maintenance.

## Fonctions qui peuvent differencier AZ-2

### 1. Quatre vues un seul morceau

C'est la fonction phare. Une note creee dans Tracker apparait instantanement dans Piano Roll, Step et Arranger. Aucun export ni conversion.

### 2. Mode morphologie

Chaque piste peut afficher seulement les controles pertinents pour son moteur, tout en conservant les memes gestes physiques : D-pad pour naviguer, encodeur pour modifier, pression pour ouvrir, boutons pour changer de vue.

### 3. Fichiers ouverts

Projet, patterns, presets et mappings documentes. Un utilisateur peut sauvegarder, versionner et reparer ses morceaux sans logiciel proprietaire.

### 4. Diagnostic embarque

Page de test pour ecran, tactile, controles, SD, UART, audio, latence, charge CPU et memoire. Les concurrents cachent souvent la machine ; AZ-2 peut la rendre lisible.

### 5. Profils de puissance

- Eco : autonomie et moteurs legers ;
- Studio : polyphonie et effets ;
- Live : Wi-Fi coupe, allocations gelees, securite maximale ;
- Render : export plus lent mais qualite maximale.

### 6. Moteurs modulaires realistes

Ne pas charger dix moteurs simultanement. Un projet declare ses moteurs. Le firmware reserve CPU et memoire de facon explicite, puis refuse proprement une configuration impossible.

## Ce qu'il ne faut pas copier

- les combinaisons de touches obscures du M8 ;
- la dependance au tracker de Polyend ;
- les menus profonds d'une MPC ;
- le manque d'affichage de l'EP-133 ;
- la facade massive du Deluge ;
- les formats fermes ;
- les limitations cachees de polyphonie ;
- les fonctions reseau actives pendant le temps reel ;
- la promesse de moteurs non mesures.

## Positionnement recommande

AZ-2 doit etre presente comme :

Une workstation tracker tactile et ouverte, pilotee comme un instrument, capable de montrer le meme morceau en Tracker, Step, Piano Roll et Arranger.

Elle ne doit pas etre presentee comme :

- une MPC moins chere ;
- un M8 avec un plus grand ecran ;
- un clone de MicroDexed-touch ;
- une console Retro-Go qui fait aussi du son ;
- une liste de moteurs sans workflow.

## Cible concurrentielle realiste

### Version AZ-2 essentielle

- 8 pistes ;
- Tracker et Step ;
- sampler par import SD ;
- MicroDexed FM ;
- un moteur VA ou wavetable ;
- locks, probabilite et microtiming ;
- mixer et effets ;
- song mode ;
- projets ouverts ;
- ecran tactile et controles physiques ;
- MIDI.

### Version AZ-2 complete

- 16 pistes logiques selon budget ;
- Piano Roll et Arranger ;
- sampling direct avec nouvelle chaine audio ;
- resampling ;
- multisampling ;
- export stems ;
- audio USB ;
- moteurs supplementaires mesures ;
- Wi-Fi de maintenance ;
- performance macros et scenes.

## Verdict concurrence

Les concurrents sont puissants, mais chacun force l'utilisateur a penser selon une seule philosophie. AZ-2 peut ouvrir une autre voie : un noyau musical unique, plusieurs langages visuels, et une architecture transparente.

Le terrain a prendre se situe entre M8 et Blackbox, avec la profondeur d'Elektron et l'ouverture du Deluge. C'est ambitieux, mais coherent avec l'ecran 480 x 480 et le double cerveau. Le danger serait de poursuivre dix moteurs avant d'avoir construit ce noyau commun.
