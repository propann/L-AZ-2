# Cible produit et feuille de route AZ-2

Date : 17 septembre 2026  
Statut : specification de travail issue de l'audit et de l'etude concurrentielle  
Regle : ce document ne modifie aucun firmware

## Decision produit

AZ-2 devient une groovebox tracker tactile sans pads physiques.

La machine conserve :

- ESP32-S3 avec ecran tactile 4 pouces 480 x 480 ;
- Teensy 4.1 pour le temps reel musical ;
- microSD ;
- sortie audio dediee ;
- architecture double cerveau ;
- commandes physiques simples.

La machine abandonne pour cette version :

- matrice physique 4x4 ;
- multiplexeurs de pads ;
- LEDs individuelles de pads ;
- extension Pico dans le chemin principal.

Les pads physiques sont reserves a une future AZ-3.

## Promesse

AZ-2 permet de construire un morceau complet avec quatre vues synchronisees du meme sequenceur :

1. Tracker ;
2. Step ;
3. Piano Roll ;
4. Arranger.

Le musicien choisit la representation adaptee a son geste. Le morceau ne change pas de format lorsque l'on change de vue.

## Principes non negociables

1. Le son continue meme si l'interface ralentit.
2. Aucune fonction reseau ne doit perturber le temps reel.
3. Une action importante doit etre accessible au tactile et aux commandes physiques.
4. Les limites CPU, memoire et polyphonie doivent etre visibles.
5. Les projets restent ouverts, versionnes et reparables.
6. Le demarrage doit mener a un son en quelques secondes.
7. L'Arranger fait partie du noyau, pas d'une extension tardive.
8. Chaque moteur doit etre mesure avant d'etre annonce.
9. AZ-2 doit finir des morceaux, pas seulement fabriquer des boucles.
10. Le mode Retro reste separe du produit musical.

## Facade physique cible

AZ-2 ne cherche pas a reproduire une grille de pads. Sa facade minimale doit offrir une navigation rapide sans masquer l'ecran.

| Controle | Fonction principale | Fonction avec Shift |
| --- | --- | --- |
| D-pad | Deplacer le curseur et naviguer | Changer de vue ou zoomer |
| Encodeur 1 pousse | Valeur principale et validation | Valeur fine ou remise a zero |
| Encodeur 2 pousse | Parametre secondaire et ouverture | Selection multiple |
| Bouton Shift | Couche secondaire | Maintien |
| Bouton Back | Retour et annulation | Undo |
| Bouton View | Tracker, Step, Piano, Arranger | Mixer, Sound, Project |
| Play | Lecture | Lecture depuis le curseur |
| Stop | Arret | Retour debut |
| Record | Armement ou enregistrement | Automation |
| Bouton Pattern | Pattern et variation | Scene |
| Bouton Track | Selection de piste | Mute et solo |

Cette liste est une base fonctionnelle. Le nombre final de boutons dependra de la facade mecanique, mais les concepts doivent rester stables.

## Modele de sequence unique

### Objet Project

Un projet contient :

- version de format ;
- tempo ;
- signature ;
- gamme ;
- pistes ;
- patterns ;
- clips ;
- scenes ;
- arrangement ;
- presets ;
- mappings ;
- references vers samples ;
- reglage du mixer ;
- configuration d'effets ;
- metadonnees de compatibilite.

### Objet Track

Une piste contient :

- identifiant stable ;
- nom et couleur ;
- moteur ;
- polyphonie demandee ;
- volume, panoramique et mute ;
- sends ;
- longueur ;
- division temporelle ;
- routage MIDI ;
- clips ;
- lanes d'automation ;
- macros.

### Objet Event

L'evenement est le coeur commun aux quatre vues :

- position temporelle ;
- duree ;
- note ou action ;
- velocite ;
- probabilite ;
- condition ;
- microdecalage ;
- retrig ;
- instrument ;
- locks de parametres ;
- identifiant stable.

### Projection vers les vues

| Vue | Representation du meme Event |
| --- | --- |
| Tracker | Ligne avec note, instrument, velocite et commandes |
| Step | Case active avec sous-parametres |
| Piano Roll | Bloc hauteur par temps avec longueur |
| Arranger | Contenu du clip replace dans la timeline |

Aucune vue ne possede ses propres notes. Les vues editent les memes objets.

## Les quatre vues

### Tracker

But : precision maximale et edition rapide au D-pad.

Fonctions :

- lignes verticales ;
- colonnes configurables ;
- note, instrument, velocite, gate ;
- trois ou quatre colonnes de commandes ;
- locks visibles ;
- selection multiple ;
- edition relative ;
- duplication ;
- transposition ;
- aide contextuelle ;
- affichage decimal par defaut, hexadecimal optionnel.

### Step

But : rythme, drums et edition immediate.

Fonctions :

- 16 pas visibles ;
- pages jusqu'a 64 ou 128 pas ;
- velocite par couleur ou hauteur ;
- probabilite ;
- ratchet ;
- microtiming ;
- locks ;
- longueur et resolution par piste ;
- Euclidien ;
- mutation controlee.

Les pads sont tactiles a l'ecran, pas physiques.

### Piano Roll

But : melodies, accords et durees.

Fonctions :

- notes de longueur variable ;
- zoom ;
- selection multiple ;
- quantification ;
- gamme ;
- transposition ;
- velocite ;
- automation en lanes ;
- dessin tactile ;
- correction precise a l'encodeur.

### Arranger

But : finir le morceau.

Fonctions :

- clips par piste ;
- longueur libre ;
- duplication ;
- scenes ;
- transitions ;
- automation globale ;
- changements de tempo ;
- marqueurs ;
- rendu ;
- export ;
- mode performance avec lancement quantifie.

## Architecture double cerveau cible

### ESP32-S3

Responsable de :

- ecran et tactile ;
- controles physiques ;
- rendu des quatre vues ;
- navigateur de fichiers ;
- projets et sauvegardes ;
- index des samples ;
- Wi-Fi de maintenance ;
- mise a jour ;
- diagnostic ;
- miroir de l'etat musical.

Il ne doit pas etre responsable du declenchement temporel de chaque step.

### Teensy 4.1

Responsable de :

- horloge ;
- transport ;
- sequenceur d'execution ;
- ordonnanceur d'evenements ;
- moteurs audio ;
- allocation de voix ;
- effets temps reel ;
- mixage ;
- MIDI critique ;
- monitoring CPU et memoire ;
- securite audio.

### Protocole

Le protocole doit envoyer des intentions et des changements d'etat, pas des images de l'interface.

Familles de messages :

- session ;
- transport ;
- tempo et clock ;
- edition ;
- projet ;
- moteur ;
- parametre ;
- mixer ;
- metriques ;
- erreur ;
- transfert.

Fonctions obligatoires :

- version ;
- handshake ;
- capacites ;
- numero de sequence ;
- longueur ;
- type ;
- payload ;
- CRC ;
- acquittement selectif ;
- timeout ;
- snapshot apres reconnexion.

## Strategie moteurs audio

Vouloir tous les moteurs en meme temps conduirait a un firmware enorme et impossible a stabiliser. La bonne strategie est progressive.

### Niveau 1 Noyau

| Moteur | Raison |
| --- | --- |
| Sampler SD | Base de toute groovebox moderne |
| FM MicroDexed | Force naturelle de la base Teensy |
| VA simple | Basses, leads et accords accessibles |
| Drum synth | Percussions sans charger de samples |

### Niveau 2 Differenciation

| Moteur | Raison |
| --- | --- |
| Wavetable | Large palette avec cout controle |
| Multisampler | Instruments realistes et velocity layers |
| Granular | Creation sonore et textures |
| Chiptune | Identite AZ-2 et pont avec la culture tracker |

### Niveau 3 Recherche

| Moteur | Condition |
| --- | --- |
| Modelisation physique | Seulement apres mesures CPU |
| Spectral | Seulement si la memoire et la FFT le permettent |
| Looper multipiste | Seulement avec entree audio et stockage valide |
| Emulation Game Boy | Firmware ou mode separe |

## Decision audio materielle

### Probleme actuel

Le PCM5102A est un excellent DAC de sortie, mais il n'a pas d'entree audio. Il ne permet ni sampling direct, ni resampling analogique, ni looper.

### Deux chemins possibles

#### AZ-2 Player

- PCM5102A conserve ;
- samples importes depuis microSD ;
- pas d'enregistrement direct ;
- cout et complexite reduits ;
- concurrence M8 et Blackbox limitee.

#### AZ-2 Sampler

- ajout d'un ADC ou remplacement par un codec avec entree et sortie ;
- entree ligne ;
- microphone optionnel ;
- sortie casque avec ampli adapte ;
- sampling, resampling et looper ;
- cout, cablage et developpement superieurs.

### Recommandation

Si le mot sampler fait partie du positionnement commercial, choisir AZ-2 Sampler. Sinon, annoncer clairement un sample player par import SD. Le marche pardonne une limite assumee ; il pardonne beaucoup moins une fonction promise mais physiquement impossible.

## Nombre de pistes et budget

### Prototype credible

- 8 pistes logiques ;
- 64 pas par pattern ;
- longueurs independantes ;
- 4 voix FM ou VA partagees selon moteur ;
- samples mono ou stereo selon debit ;
- deux effets send ;
- limiteur master.

### Cible avancee

- 16 pistes logiques ;
- 128 pas ;
- budget de voix dynamique ;
- trois moteurs actifs minimum ;
- effets par piste limites ;
- bounce interne pour liberer des ressources.

Le nombre de pistes logiques peut depasser le nombre de voix simultanees. Cette distinction doit etre affichee dans l'interface.

## Sampling et stockage

Fonctions prioritaires :

- navigateur rapide ;
- audition avant chargement ;
- index en cache ;
- tags ;
- favoris ;
- slicing ;
- zero crossing ;
- normalisation non destructive ;
- loop points ;
- time-stretch en phase ulterieure ;
- streaming microSD mesure ;
- protection contre une carte trop lente ;
- consolidation du projet ;
- recherche des fichiers manquants.

Structure cible :

    /az2/
      system/
      projects/
        NomProjet/
          project.json
          patterns/
          presets/
          samples/
          renders/
      library/
        samples/
        presets/
      logs/
      updates/

## Format de projet

Exigences :

- schema versionne ;
- identifiants stables ;
- chemins relatifs ;
- nombres bornes ;
- sauvegarde temporaire puis renommage atomique ;
- copie de secours ;
- migration explicite ;
- controle d'integrite ;
- aucune cle Wi-Fi dans les projets ;
- export autonome contenant toutes les dependances.

## Experience de demarrage

### Premier son

1. Allumer.
2. Le dernier projet charge.
3. Appuyer sur Play ou creer un projet.
4. Choisir un moteur.
5. Saisir un pattern.
6. Enregistrer une variation.
7. Placer les clips dans Arranger.

Objectif futur : moins de dix secondes jusqu'a l'interface jouable, sous reserve des temps reels mesures.

### Navigation constante

- le D-pad deplace toujours ;
- l'encodeur modifie toujours la valeur en focus ;
- la pression ouvre ou confirme ;
- Back revient ;
- Shift revele la seconde couche ;
- View change de representation sans changer le morceau.

## Fonctions de performance

- mutes et solos ;
- scenes ;
- macros multi-destinations ;
- fill ;
- retrig ;
- repeat ;
- stutter ;
- filtre master ;
- delay et reverb sends ;
- capture d'automation ;
- snapshot temporaire ;
- retour instantane a l'etat sauvegarde ;
- changement quantifie de pattern ;
- chainage de projets ou setlist.

## Fonctions de production

- undo et redo multi-niveaux ;
- historique de sauvegarde ;
- duplication intelligente ;
- humanisation bornee ;
- generation euclidienne ;
- mutation avec verrouillage des parametres a proteger ;
- import MIDI ;
- export MIDI ;
- rendu WAV ;
- stems ;
- metronome et pre-roll ;
- count-in ;
- punch-in ;
- normalisation et limiteur de rendu.

## Diagnostic comme avantage produit

La page Diagnostic doit afficher :

- version ESP32 ;
- version Teensy ;
- etat du lien ;
- latence aller-retour ;
- erreurs CRC ;
- charge CPU audio ;
- pic CPU ;
- memoire audio ;
- PSRAM ;
- debit SD ;
- underruns ;
- niveau et clipping ;
- etat tactile ;
- controles physiques ;
- temperature si disponible ;
- rapport exportable.

Une machine ouverte doit expliquer pourquoi elle ne va pas bien.

## Strategie d'ouverture

AZ-2 peut se distinguer par :

- documentation publique ;
- format de projet documente ;
- protocole documente ;
- schema de cablage ;
- liste de composants ;
- inventaire des licences ;
- procedure de build reproductible ;
- firmware signe ou verifie ;
- simulateur de protocole ;
- outil desktop ou web facultatif ;
- aucun cloud obligatoire.

Ouvert ne signifie pas desordonne. Les composants tiers doivent etre versions et isoles.

## Ce qui doit rester hors AZ-2

- pads physiques ;
- matrice 4x4 ;
- multiplexeurs de facade ;
- mode console melange au firmware musical ;
- fonctions CV tant que MIDI et audio ne sont pas stabilises ;
- extension Pico sans besoin precis ;
- intelligence artificielle embarquee gadget ;
- separation de stems locale non realiste pour le materiel ;
- plugin ecosystem avant le noyau musical.

## Feuille de route documentaire et technique

### Phase 0 Specification

Livrables :

- cible materielle figee ;
- facade figee ;
- choix Player ou Sampler ;
- modele Project Track Clip Event ;
- protocole v1 ;
- budget CPU et memoire ;
- licences et provenance ;
- definition des tests.

Critere de sortie : aucune contradiction entre produit, cablage, protocole et UI.

### Phase 1 Lien et son

Livrables futurs :

- build reproductible ;
- vrai UART ;
- handshake ;
- watchdog de liaison ;
- son de test ;
- mute de securite ;
- metriques audio.

Critere : le tactile ou un bouton physique declenche un son et recoit un etat confirme.

### Phase 2 Sequenceur noyau

Livrables futurs :

- horloge ;
- transport ;
- 8 pistes ;
- events ;
- patterns ;
- microtiming ;
- swing ;
- locks ;
- probabilite ;
- conditions ;
- sauvegarde et chargement.

Critere : un morceau continue de jouer correctement si l'ESP32 cesse de rafraichir l'ecran.

### Phase 3 Deux vues

- Tracker ;
- Step ;
- edition bidirectionnelle ;
- undo et redo ;
- mixer minimal.

Critere : la meme note peut etre modifiee dans les deux vues sans divergence.

### Phase 4 Premier moteur reel

- FM MicroDexed ou sampler ;
- presets ;
- polyphonie mesuree ;
- effets ;
- limites affichees.

Critere : dix minutes de lecture sans erreur, underrun ni fuite observable.

### Phase 5 Piano Roll et Arranger

- notes longues ;
- clips ;
- scenes ;
- arrangement ;
- rendu.

Critere : produire un morceau complet sans ordinateur.

### Phase 6 Sampling reel

Seulement si la chaine audio possede une entree :

- enregistrement ;
- slicing ;
- resampling ;
- multisampling ;
- streaming ;
- export stems.

### Phase 7 Ouverture et finition

- documentation utilisateur ;
- format public ;
- outils de mise a jour ;
- diagnostic ;
- boitier ;
- autonomie ;
- tests de scene.

## Critere pour battre les concurrents

AZ-2 ne battra pas un produit fini avec une liste de promesses. Elle commencera a devenir concurrentielle lorsque les cinq demonstrations suivantes seront reelles :

1. un pattern cree en Tracker apparait en Step et Piano Roll ;
2. le morceau est arrange sans conversion ;
3. le son continue pendant une charge UI ou SD ;
4. le projet reste lisible et reparable hors de la machine ;
5. une page Diagnostic explique les limites en temps reel.

## Documents associes

- Audit technique : docs/AZ2_AUDIT_TECHNIQUE_COMPLET_2026-09-17.md
- Etude de concurrence : docs/AZ2_ETUDE_CONCURRENCE_TRACKERS_GROOVEBOXES_2026.md

## Conclusion

La meilleure AZ-2 n'est pas une boite qui possede tous les moteurs du monde. C'est une machine qui laisse le musicien penser de quatre manieres sans jamais perdre le fil du morceau.

Le noyau gagnant est simple a formuler et difficile a construire : un seul sequenceur, quatre vues, un son stable, des fichiers ouverts. C'est la montagne. Tout le reste est la neige sur le sommet.
