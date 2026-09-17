# Audit technique complet du depot AZ-2

Date de reference : 17 septembre 2026  
Depot : https://github.com/propann/L-AZ-2  
Branche auditee : main  
Commit audite : b3eeae8f6d776dbb2d0b21d8dc2100749dad6aff  
Nature de l'audit : lecture statique du depot, sans modification du code

## Conclusion generale

AZ-2 possede une bonne intuition d'architecture : separer l'interface et les services sur ESP32-S3 du moteur audio temps reel sur Teensy 4.1. Cette separation est pertinente pour une groovebox ambitieuse. En revanche, le depot n'est pas encore un produit logiciel coherent ni une base firmware integrable.

Le code AZ-2 reel est un prototype de communication et de test audio. Il ne contient encore ni sequenceur musical, ni moteur MicroDexed integre, ni interface LVGL fonctionnelle, ni gestion tactile, ni stockage de projets, ni controle physique correspondant a la nouvelle facade. La majorite du depot est composee de copies brutes de projets tiers.

Evaluation globale :

| Domaine | Etat | Verdict |
| --- | --- | --- |
| Vision produit | Prometteuse | Bon axe, mais documentation devenue obsolete |
| Architecture double cerveau | Bonne sur le principe | A conserver |
| Liaison ESP32 vers Teensy | Bloquante | Non fonctionnelle telle qu'ecrite |
| Moteur audio | Demonstrateur sinus | Tres loin d'une groovebox |
| Sequenceur | Absent | Blocage produit |
| Interface ecran et tactile | Absente | Blocage produit |
| Materiel de controle | Ancienne matrice 4x4 | Incompatible avec la cible actuelle |
| Sampling | Import non implemente, enregistrement impossible avec le DAC actuel | Blocage de positionnement |
| Qualite du depot | Tres encombre | Dette majeure |
| Tests et integration continue | Absents pour AZ-2 | Risque eleve |
| Documentation | Riche mais contradictoire | A realigner avant developpement |
| Maturite estimee | Pre-alpha / preuve de concept | Pas encore un firmware de groovebox |

## Perimetre examine

L'audit couvre :

- la structure complete du depot ;
- le fichier PlatformIO principal ;
- le protocole partage ESP32 et Teensy ;
- le firmware ESP32 AZ-2 ;
- le firmware Teensy AZ-2 ;
- la cible Pico ;
- la documentation d'architecture, de cablage et de portage ;
- l'integration prevue de MicroDexed-touch ;
- les copies de Retro-Go et du launcher LVGL ;
- la reproductibilite, les tests, les licences et la maintenabilite.

L'audit ne pretend pas remplacer des essais sur le materiel reel. Aucun resultat de compilation ou mesure de latence n'est publie dans le depot. Les conclusions de compilation sont donc classees comme confirmees par lecture ou a verifier sur banc.

## Inventaire du depot

Le depot contient environ :

| Mesure | Valeur |
| --- | ---: |
| Fichiers | 3 774 |
| Repertoires | 482 |
| Taille logique des fichiers | 345 970 809 octets |
| Fichiers sous src_esp32 | 1 688 |
| Fichiers sous src_teensy | 2 073 |
| Fichiers AZ-2 de premiere main hors copies tierces | 17 |
| Fichiers dupliques par contenu au-dela du premier exemplaire | 1 098 |
| Volume duplique estime | 69 213 236 octets |
| Fichiers suffixes par _1 | 868 |
| Tests propres a AZ-2 | 0 |
| Workflow CI a la racine | 0 |
| Licence a la racine | 0 |

Le contraste est important : le depot pese environ 346 Mo, alors que les trois fichiers de code AZ-2 actifs representent moins de 11 Ko, hors configuration et documentation.

## Architecture actuelle

### Ce qui est sain

La separation cible est pertinente :

- ESP32-S3 : ecran 480 x 480, tactile, commandes physiques, SD, Wi-Fi et interface ;
- Teensy 4.1 : horloge musicale, sequenceur critique, synthese, lecture de samples et effets ;
- PCM5102A : conversion I2S vers sortie ligne ;
- protocole partage : contrat explicite entre les deux processeurs.

Cette architecture limite les risques de jitter causes par l'affichage, le tactile, la SD et le Wi-Fi. Elle permet aussi de redemarrer l'interface sans necessairement interrompre tout le moteur audio, a condition de definir un comportement de perte de liaison.

### Ce qui doit changer dans la documentation

La cible produit validee le 17 septembre 2026 est differente de la documentation actuelle :

- ecran tactile 4 pouces 480 x 480 conserve ;
- aucun pad physique sur AZ-2 ;
- matrice SparkFun 4x4 et multiplexeurs abandonnes ;
- pads physiques reserves a une future AZ-3 ;
- controle physique AZ-2 par croix directionnelle, boutons, encodeurs poussoirs et transport ;
- un meme sequenceur doit pouvoir etre edite dans plusieurs vues : Tracker, Step, Piano Roll et Arranger.

Le README, le protocole, le firmware ESP32, le cablage et la feuille de route sont encore structures autour de 16 pads physiques et de multiplexeurs. La documentation decrit donc aujourd'hui un produit qui n'est plus celui que l'on veut construire.

## Audit du fichier platformio.ini

### Points positifs

- trois environnements clairement nommes ;
- separation Teensy, ESP32 et Pico ;
- deux environnements principaux selectionnes par defaut ;
- definitions de roles par drapeaux de compilation ;
- moniteur serie fixe a 230400 bauds.

### Risques

#### P0 A verifier immediatement : selection des sources

PlatformIO utilise par defaut un repertoire source nomme src. Le depot ne contient pas ce repertoire. Les filtres utilisent des chemins comme :

- ../src_teensy/az2_audio/
- ../src_esp32/az2_control/

La documentation officielle precise que build_src_filter est relatif a src_dir. La configuration peut donc etre fragile ou ne rien compiler selon la resolution effective des motifs. Le depot ne contient ni trace de build reussi ni CI permettant de le prouver.

Reference : https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_src_filter.html

#### P1 : plateformes non figees

Les plateformes teensy et espressif32 ne sont pas epinglees a une version. Une mise a jour de PlatformIO peut changer le compilateur, le framework ou les definitions de carte et casser le build.

#### P1 : dependance Git non figee

Arduino_GFX est reference par l'URL du depot sans tag ni commit. Un futur changement amont peut modifier le resultat de compilation.

#### P1 : mauvaise cible materielle potentielle

L'environnement ESP32 utilise esp32-s3-devkitc-1 alors que la carte reelle est un module ESP32-4848S040C_I avec ecran ST7701, PSRAM, flash, tactile et carte SD integres. Cette cible generique ne porte pas automatiquement le pinout, le bus RGB de l'ecran ni le tactile de la carte reelle.

#### P2 : dependances declarees mais non exercees

LVGL et Arduino_GFX sont declares, mais le firmware AZ-2 actif ne les utilise pas. Le depot donne l'impression que l'ecran est pris en charge, alors qu'aucune initialisation graphique n'existe.

## Audit du protocole AZ2_Protocol.h

### Forces

- constantes centralisees ;
- format texte lisible pendant le prototypage ;
- helpers d'emission simples ;
- messages de transport et de statut faciles a observer au moniteur serie.

### Defauts critiques

#### P0 : protocole construit autour du materiel abandonne

Le coeur du protocole declare kPadCount, kPadRows, kPadCols, padId, validPad, printPadEvent et printLedEvent. Il encode le produit autour d'une matrice 4x4 qui n'existe plus sur AZ-2.

Le futur contrat doit etre oriente intention et non facade. Il doit transporter des actions comme :

- navigation ;
- validation et retour ;
- rotation et pression d'encodeur ;
- transport ;
- edition de cellule ;
- selection de piste, clip, pattern et moteur ;
- changement de parametre ;
- chargement atomique d'un projet.

#### P0 : aucun versionnage reel

Le document evoque un protocole v0 puis v1, mais les messages actuels n'incluent ni version, ni capacites negociees, ni identifiant de requete.

#### P0 : aucune protection contre les pertes et desynchronisations

Il manque :

- checksum ou CRC ;
- numero de sequence ;
- acquittement pour les commandes importantes ;
- timeouts ;
- politique de retransmission ;
- snapshot d'etat complet apres reconnexion ;
- message d'erreur structure ;
- taille maximale commune ;
- echappement ou encodage des donnees.

#### P1 : protocole incomplet

Les helpers existent pour BPM, pattern et clock, mais le firmware Teensy ne traite pas BPM, PATTERN ni REC. Aucun format n'est defini pour les notes polyphoniques, les locks, l'automation, le mixage, les moteurs ou les projets.

## Audit du firmware ESP32

Fichier : src_esp32/az2_control/main.cpp

### Etat reel

Le firmware :

- ouvre le port serie USB ;
- coupe le Wi-Fi ;
- prepare conditionnellement une SD ;
- prepare conditionnellement deux multiplexeurs ;
- simule un scan de 16 pads ;
- emet des lignes texte ;
- lit des lignes venant du Teensy et les recopie sur le port de debug ;
- emet un heartbeat local.

### Bloquants

#### P0 : toutes les broches utiles valent -1

UART, SD, multiplexeurs et signaux associes sont desactives. Le firmware demarre mais ne communique avec aucun materiel AZ-2.

#### P0 : aucune interface ecran

Il n'existe :

- aucune initialisation ST7701 ;
- aucun bus RGB ;
- aucun pilote tactile ;
- aucun affichage LVGL ;
- aucune page Tracker, Step, Piano Roll ou Arranger ;
- aucun rafraichissement d'etat.

Le message AZ2:FEATURE:UI_480X480_ST7701 annonce donc une fonction absente.

#### P0 : absence des nouveaux controles physiques

La croix directionnelle, les boutons, les encodeurs poussoirs et le transport ne sont ni modelises ni scannes.

#### P0 : communication asymetrique

L'ESP32 envoie au Teensy sur Serial1 lorsqu'il est configure. Le firmware Teensy ne lit que Serial, qui correspond au port USB dans cette configuration. La liaison physique ESP32 vers Teensy ne peut donc pas fonctionner telle qu'elle est ecrite.

#### P1 : heartbeat incomplet

Le heartbeat ESP32 est imprime uniquement sur le port de debug. Il n'est pas envoye au Teensy. Il ne permet donc pas au moteur audio de detecter la perte de l'interface.

#### P1 : reception sans modele d'etat

Les messages du Teensy sont simplement prefixes par TEENSY puis imprimes. Aucun parser ne met a jour un etat local, un ecran, un indicateur de connexion ou une file d'evenements.

#### P1 : usage intensif de String

La construction ligne par ligne dans un objet String provoque des allocations dynamiques. Ce n'est pas le plus gros risque sur ESP32-S3, mais un protocole temps reel durable devrait utiliser un buffer borne et un parseur sans allocation dans la boucle.

#### P2 : SD non exploitee

Le code cree seulement /az2. Il ne charge ni configuration, ni projet, ni preset, ni sample, et n'effectue aucune ecriture atomique.

## Audit du firmware Teensy

Fichier : src_teensy/az2_audio/main.cpp

### Etat reel

Le code est un demonstrateur minimal :

- un oscillateur sinus monophonique ;
- un amplificateur ;
- une sortie I2S stereo dupliquee ;
- lecture de commandes texte ;
- correspondance pad vers frequence ;
- statut et pseudo-horloge envoyes une fois par seconde.

### Bloquants

#### P0 : mauvais port de communication

Le Teensy lit et ecrit Serial, alors que l'ESP32 utilise Serial1 pour le lien materiel. Il faut distinguer le port USB de debug du port UART inter-processeurs.

#### P0 : aucun sequenceur

La variable currentStep avance une fois par seconde seulement quand playing est vrai. Elle n'est liee ni au BPM, ni a une resolution PPQN, ni a un timer materiel, ni au moteur audio. Elle ne constitue pas une horloge musicale.

#### P0 : aucun moteur promis

MicroDexed, sampler, wavetable, VA, granular, drum synth, chiptune, modelisation physique, spectral et looper ne sont pas integres. Le moteur actif est un unique sinus.

#### P0 : aucune entree audio

Le PCM5102A est uniquement un DAC de sortie. Il ne peut pas enregistrer. Sans ADC ou codec audio, AZ-2 peut importer et lire des samples depuis un stockage, mais ne peut pas concurrencer M8, Tracker+, Blackbox, Move ou MPC sur le sampling direct.

#### P1 : monophonie fragile

Une pression change la frequence du seul oscillateur. Le relachement de n'importe quel pad coupe la voix, meme si une autre touche devrait rester active. Il n'existe ni allocation de voix, ni enveloppe, ni suivi des notes maintenues.

#### P1 : parseur permissif

La conversion toInt transforme une commande mal formee en zero dans plusieurs cas. Une ligne invalide peut donc etre interpretee comme une action sur l'element zero.

#### P1 : allocations dans la boucle de commande

String, substring et trim effectuent des manipulations dynamiques dans la boucle principale. Le callback audio de la librairie reste separe, mais cette approche n'est pas ideale pour un moteur qui devra aussi gerer sequenceur, stockage et nombreux parametres.

#### P1 : absence de surveillance audio

Aucun suivi de AudioProcessorUsage, AudioMemoryUsage, underrun, clipping ou charge par moteur n'est publie.

#### P1 : AudioMemory arbitraire

AudioMemory(12) convient au test sinus, mais ne donne aucune marge ni budget pour les moteurs futurs. Le projet n'a pas encore de budget CPU, RAM, PSRAM, voix ou effets.

## Audit de la cible Pico

Le firmware Pico est vide. La cible est correctement qualifiee d'optionnelle dans le README, mais elle ajoute aujourd'hui une branche materielle sans valeur fonctionnelle.

Recommandation produit : garder le Pico hors du chemin AZ-2. Ne le reactiver que lorsqu'une extension physique AZ-3 aura une specification figee.

## Audit des composants tiers

### MicroDexed-touch

La copie fournit une base precieuse pour le moteur Teensy et inclut ses licences Apache 2.0 et GPLv3. Cependant, elle est importee comme un depot complet avec documentation, samples, outils, bibliotheques tierces et fichiers volumineux.

Risques :

- provenance exacte du commit non documentee ;
- pas de mecanisme de mise a jour ;
- melange de plusieurs licences ;
- aucune liste claire des fichiers reellement reutilises ;
- manuel PDF de pres de 69 Mo dans Git ;
- aucune preuve que la copie compile dans l'environnement AZ-2 ;
- l'architecture actuelle ne lie pas ce code au binaire master_teensy.

### Retro-Go

Retro-Go est une reference de recherche et non une bibliotheque Arduino directement integrable. Sa copie embarque un ecosysteme ESP-IDF complet, des emulateurs, des assets et de nombreuses duplications suffixees _1.

Risques :

- poids massif ;
- surface de maintenance disproportionnee ;
- licences multiples ;
- confusion entre recherche et code produit ;
- aucun target pour ESP32-4848S040C_I ;
- incompatibilite architecturale avec le firmware LVGL Arduino actuel.

### Launcher LVGL

Le launcher contient de gros assets convertis en tableaux C, dont plusieurs fichiers de 18 a 22 Mo. Il n'est pas relie au build AZ-2 et ne comporte pas de licence visible a sa racine.

Verdict : utile comme reference visuelle eventuelle, mais dangereux comme code embarque non qualifie.

## Hygiene du depot

### P0 : depot disproportionne

Le depot versionne des sources amont completes plutot que des dependances identifiees. Cela rend les recherches, les audits, les clones et les mises a jour plus difficiles.

### P0 : duplications massives

1 098 copies supplementaires partagent exactement le meme contenu qu'un autre fichier. Les 868 suffixes _1 indiquent probablement une copie ou extraction repetee.

### P1 : absence de licence racine

La licence propre a AZ-2 n'est pas declaree. La presence de licences dans les sous-projets ne definit pas la licence du projet principal.

### P1 : absence d'inventaire de licences

Un produit derive doit connaitre la licence de chaque moteur, bibliotheque, sample, police, image et firmware tiers. Certaines licences copyleft peuvent imposer des obligations sur la distribution du firmware combine.

### P1 : absence de provenance

Chaque source tierce devrait avoir :

- URL d'origine ;
- commit ou version ;
- licence ;
- modifications appliquees ;
- procedure de mise a jour ;
- liste des composants effectivement utilises.

### P1 : historique fragmente

Plusieurs fichiers d'une meme livraison ont ete pousses dans des commits separes a quelques secondes d'intervalle. Cela complique le retour atomique a un etat coherent.

## Tests, qualite et securite

Il n'existe aucun test AZ-2 automatise.

Minimum futur recommande :

| Niveau | Tests |
| --- | --- |
| Protocole | encode/decode, trames invalides, taille, CRC, timeout |
| Sequenceur | BPM, swing, microtiming, polymetre, conditions, changement de pattern |
| Audio | allocation de voix, niveau, clipping, charge CPU, changement de moteur |
| Stockage | coupure pendant sauvegarde, fichier corrompu, migration de version |
| ESP32 | tactile, encodeurs, boutons, perte du Teensy, perte SD |
| Integration | handshake, resynchronisation, boot dans n'importe quel ordre |
| Materiel | DAC, future entree audio, bruit, masse, latence action vers son |

Risques de securite futurs :

- ne jamais stocker les identifiants Wi-Fi dans Git ;
- signer ou verifier les mises a jour ;
- limiter le serveur web local au mode maintenance ;
- valider les chemins et tailles des fichiers charges ;
- rendre les sauvegardes atomiques ;
- ne jamais laisser le Wi-Fi degrader le temps reel.

## Incoherences documentaires principales

| Documentation actuelle | Cible actuelle | Gravite |
| --- | --- | --- |
| Matrice 4x4 organe principal | Aucun pad physique sur AZ-2 | Critique |
| Multiplexeurs a identifier | Multiplexeurs abandonnes | Critique |
| Page grille 4x4 prioritaire | Tracker, Step, Piano Roll, Arranger | Critique |
| Tactile surtout pour edition lente | Tactile conserve comme element central avec controles simples | Moyenne |
| Sampling evoque | Aucun ADC dans l'architecture | Critique |
| Moteurs multiples evoques | Seul un sinus est compile | Critique |
| UI 480x480 annoncee | Aucun pilote ecran actif | Critique |
| Bus bidirectionnel documente | Ports serie incompatibles | Critique |
| Build PlatformIO presente comme base | Aucun build reproductible publie | Elevee |

## Priorites recommandees sans coder maintenant

### P0 Produit et architecture

1. Figer AZ-2 sans pads ni multiplexeurs.
2. Figer la facade : D-pad, boutons, encodeurs poussoirs et transport.
3. Definir le modele de sequence unique partage par les quatre vues.
4. Decider si AZ-2 doit enregistrer de l'audio. Si oui, remplacer ou completer le PCM5102A par une chaine avec ADC.
5. Fixer un nombre realiste de pistes, voix, moteurs simultanes et effets.

### P0 Depot

1. Definir ce qui est code produit, reference externe et archive.
2. Documenter la provenance et les licences.
3. Etablir une strategie future de nettoyage des doublons et gros binaires.
4. Ajouter une licence racine adaptee au choix de distribution.

### P0 Liaison

1. Choisir le vrai UART Teensy et les broches.
2. Separer debug USB et bus inter-processeurs.
3. Definir boot, handshake, timeout et resynchronisation.
4. Concevoir un protocole versionne oriente actions et parametres.

### P1 Firmware

1. Creer d'abord un sequenceur fiable et independant de l'UI.
2. Faire fonctionner une UI materielle minimale.
3. Integrer un seul moteur sonore complet avant d'en annoncer dix.
4. Mesurer CPU, RAM, latence et stabilite.
5. Ajouter un second moteur seulement apres validation du premier.

## Decision Go ou No Go

### Go pour continuer

Les composants principaux sont capables de porter un produit interessant :

- Teensy 4.1 peut soutenir un moteur audio embarque serieux ;
- ESP32-S3 convient a l'ecran, au tactile, au stockage et au reseau ;
- l'ecran carre 480 x 480 est un vrai facteur differenciant ;
- la separation temps reel et interface est defendable ;
- l'idee de quatre vues sur un meme sequenceur peut etre unique.

### No Go pour empiler des fonctions maintenant

Ajouter immediatement granular, wavetable, looper, Retro-Go ou dix pages UI aggraverait la dette. Le noyau inexistant doit d'abord devenir reel : transport, horloge, pistes, evenements, projet, communication et une voix audio solide.

## Verdict final

AZ-2 n'est pas en retard parce qu'il manque des effets. Il est en retard parce que son contrat central n'est pas encore fige.

La priorite absolue est de faire converger quatre choses :

1. le produit reel sans pads physiques ;
2. le modele de sequence commun aux quatre vues ;
3. le protocole ESP32 vers Teensy ;
4. la chaine audio capable de tenir les promesses de sampling et de synthese.

Une fois ces fondations verrouillees, le double cerveau devient une force. Avant cela, le depot ressemble davantage a un hangar rempli d'excellentes pieces qu'a une machine capable de jouer un morceau complet.
