# AZ-2 — Community Hardware Lab

> **Statut : EXPERIMENTAL / COMMUNITY.** Ici, une idée peut être intéressante sans être encore un composant officiel de l'AZ-2.

## Une plateforme, pas une boîte fermée

AZ-2 est conçu pour être démonté, compris et transformé. Le prototype de référence donne une base reproductible ; il ne dicte pas l'unique forme que l'instrument doit prendre.

Vous pouvez proposer un nouveau contrôleur, une cartouche moteur, une disposition de boutons, un boîtier, une interface MIDI, un module d'effets ou une autre manière de jouer.

## Proposition matérielle

Copiez ce modèle dans une issue ou un document :

```text
Nom :
Auteur / contact :
Statut : IDEA | PROTOTYPE | TESTED
Objectif :

Matériel :
- MCU :
- contrôleurs :
- audio :
- alimentation :

Connexion AZ-2 :
- données :
- audio :
- alimentation :

Firmware :
- dépôt / dossier :
- environnement :
- protocole AZ-2 :

Tests réalisés :
- démarrage :
- latence :
- charge :
- audio/bruit :
- durée :
- erreurs connues :

Documentation :
- schéma/câblage :
- photos :
- instructions de reproduction :

Avantages :
Contraintes :
Prochaine étape :
```

## Idées déjà envisagées

- matrice de boutons **SparkFun 4×4** pilotée par **Raspberry Pi Pico**, avec gestion des LED et transmission vers le Teensy ;
- réutilisation du **clavier LMN-3** avec un Pico, si les E/S et la liaison au Teensy sont validées ;
- nouveaux moteurs sur ESP ;
- futurs formats de modules plus compacts ;
- autres boîtiers et surfaces de contrôle.

Aucune de ces pistes n'est annoncée comme validée avant un test réel.

## Règle du labo

**Proposer librement, mesurer sérieusement, documenter ce qui fonctionne.**

Les variantes qui deviennent reproductibles peuvent recevoir une fiche dédiée et rejoindre la documentation des configurations validées.
