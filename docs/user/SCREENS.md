# AZ-2 — Galerie des écrans

> **Référence UI : prototype réel.** Les visuels de documentation doivent reproduire l'interface réellement présente dans le firmware. Une reconstruction graphique doit être explicitement légendée comme telle.

## Identité visuelle

L'AZ-2 utilise un écran carré 480×480, fond sombre, typographie compacte/pixel et accents cyan, magenta, orange, violet, jaune et vert. Les futurs visuels ne doivent pas transformer cette identité en interface générique de DAW.

## Écrans observés sur le prototype

| Écran | Fonction | État documentaire |
| --- | --- | --- |
| HOME | Entrée AZ-TRACKER, JEUX, CONFIG, DOC | Prototype réel |
| PATTERN / TRACKER | Séquence 16 pas, piste, note, instrument, FX, probabilité, accord | Prototype réel |
| PAD 4×4 | Déclenchement de 16 pads | Prototype réel |
| MIXER | 8 pistes, volume, mute/solo | Prototype réel ; les barres actuelles sont des valeurs de volume, pas encore des VU audio réels |
| SONG | Assemblage des patterns et longueur | Prototype réel |
| PROJETS | Charger/sauver les projets sur SD ESP32 | Prototype réel |
| JEUX | Navigateur de ROM GB/GBC | Prototype réel |
| PATCH / MOTEUR | Édition sonore | Présent dans le firmware ; évolution UI prévue |

## Règles pour les images GitHub

Deux catégories seulement :

- **Photo prototype** : vraie photo, recadrage/perspective/exposition/netteté autorisés ; aucun changement du boîtier, des boutons, des encodeurs ou du contenu de l'écran.
- **Reconstruction firmware** : rendu propre 480×480 reproduisant les dimensions, textes, états et palette du code. Toujours légendé « reconstruction de l'interface firmware ».

Les images de concept non implémentées doivent porter clairement la mention **CONCEPT / ROADMAP**.

## Parcours musical cible

```text
HOME
 └─ AZ-TRACKER
     ├─ TRACKER
     │   ├─ MOTEUR
     │   │   └─ EDIT SON
     │   ├─ PAD 4×4
     │   └─ METRO
     ├─ MIXER
     ├─ SONG
     └─ PROJET
```

Le rack de moteurs ESP apparaîtra plus tard dans CONFIG/RACK et dans la sélection de moteurs lorsqu'il sera réellement implémenté.

## À produire

- photo hero du prototype réel ;
- HOME ;
- TRACKER ;
- MIXER ;
- SONG ;
- JEUX ;
- un montage « hardware + écran » ;
- plus tard : page RACK, uniquement quand son état sera suffisamment défini.

Les captures propres doivent être dérivées du firmware ou du prototype, jamais inventer une fonctionnalité pour embellir le README.
