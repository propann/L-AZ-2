# Audit de code AZ-2 — 19 septembre 2026

## Verdict

**AZ-2 progresse très bien.** Le dépôt est passé d’un prototype avancé mais fragile à une **alpha matérielle cohérente et réellement utilisable**. Le système n’est pas encore au niveau « bêta fiable » : les fonctions essentielles existent, plusieurs tests sur le vrai matériel sont documentés, mais la protection des sauvegardes, la validation longue durée et la maintenabilité restent insuffisantes.

**Note globale indicative : 7,3 / 10.**

- Aucun défaut bloquant P0 trouvé dans la révision auditée.
- Deux défauts de données doivent être corrigés avant de considérer les projets comme sûrs.
- Le souffle attribué à Dexed est résolu : la cause était le filtre partagé à 18 kHz, pas le cœur FM.
- Les six moteurs audio sont intégrés au Teensy.
- L’émulation GB/GBC est fonctionnelle, mais sa vitesse réelle et sa compatibilité doivent encore être mesurées méthodiquement.
- Le sampleur est une bonne fondation, mais reste une démonstration à deux samples embarqués.

## Périmètre et méthode

Révision auditée : [6cdaf923](https://github.com/propann/L-AZ-2/commit/6cdaf923beb8e3cc922e3aeb48e1207d2309505e), branche principale.

Comparaison avec le premier audit : [be33c951…6cdaf923](https://github.com/propann/L-AZ-2/compare/be33c9510c7d33f4c68b2b0478a92028e9a82f9d...6cdaf923beb8e3cc922e3aeb48e1207d2309505e).

Entre ces deux points, le dépôt a avancé de **40 commits**. Les changements principaux représentent notamment :

- environ 800 lignes modifiées dans l’interface ESP32 ;
- environ 700 lignes modifiées dans le moteur Teensy ;
- une banque Dexed complète embarquée ;
- un nouveau moteur sampleur ;
- une CI, des tests natifs et plusieurs documents d’architecture.

Audit statique réalisé sur :

- le firmware Teensy ;
- le firmware écran ESP32-S3 ;
- le protocole partagé ;
- le sampleur ;
- l’émulation GB/GBC ;
- les sauvegardes ;
- la configuration PlatformIO et la CI ;
- les comptes rendus de tests réels présents dans l’historique.

Les affirmations « validé sur matériel » ci-dessous reposent sur les essais consignés dans les commits. Cet audit n’a pas reflashé physiquement la machine.

## Évolution depuis le premier audit

| Domaine | Premier audit | État actuel | Appréciation |
|---|---|---|---|
| Souffle audio | attribué à Dexed, cause inconnue | cause isolée : filtre partagé instable à 18 kHz, plafond ramené à 15 kHz | très bon progrès |
| Mémoire Teensy | marge supposée d’environ 1,6 Ko | test d’allocation de 300 Ko réussi ; environ 454 Ko de RAM2 indiqués par le linker | alerte précédente levée |
| Charge audio | non mesurée proprement | environ 7,1 %, pic 11,2 %, 125–139 blocs sur 200 consignés | bonne marge |
| Moteurs | 5 moteurs, peu de sons | 6 moteurs ; Dexed 255, Braids 43, Analog 11, ePiano 5, Karplus, Sampler 2 | gros progrès |
| Tracker | base fonctionnelle | patterns, song, probabilité, conditions, swing, FX de pas, métronome | niveau alpha solide |
| UART | 230400, désynchronisation possible | 921600, buffers 2048, longueur stricte, timeout et resynchronisation | nettement amélioré |
| Émulation | lente et instable | cadence à 16 742 µs, frame-skip, audio 14 kHz, sauvegarde cartouche | progrès réel, validation incomplète |
| Tests | absents | tests natifs du protocole + compilation CI des deux firmwares | fondation correcte |
| Architecture | gros fichiers monolithiques | fonctionnalités mieux séparées, mais les deux fichiers principaux restent énormes | dette encore élevée |

## Ce qui est bien conçu

### Architecture à deux cerveaux

La séparation des responsabilités est bonne pour l’AZ-2 actuelle :

- Teensy 4.1 : audio temps réel, tracker et commandes physiques ;
- ESP32-S3 : écran, tactile, SD principale et émulation ;
- protocole commun dans `lib/AZ2_Protocol`.

La décision de reporter les racks ESP à l’AZ-3 est saine : le boîtier AZ-2 est déjà plein et il vaut mieux stabiliser le produit existant.

### Chaîne audio

Le graphe audio est maintenant lisible et cohérent :

- deux mixeurs de quatre pistes ;
- un mixeur final pour les deux groupes, la voix live et le métronome ;
- un bus maître sec/réverbération/délai/audio Game Boy ;
- filtre et ADSR partagés par piste ;
- sortie I2S vers le PCM5102A.

Le métronome utilise bien le quatrième canal libre du mixeur final. La charge mesurée laisse une marge importante pour la stabilisation.

### Moteurs sonores

L’intégration des moteurs a beaucoup progressé :

- Dexed : 255 patches DX7 réels ;
- Braids : 43 modèles accessibles ;
- Analog : 11 formes d’onde ;
- ePiano : 5 programmes ;
- Karplus-Strong ;
- Sampler : lecture PCM one-shot avec interpolation linéaire et pitch.

Le passage de `enginePatchCount()` à `uint16_t` évite le débordement qui aurait transformé 256 en zéro. La limitation à 255 patches Dexed est cohérente avec le sentinel `0xFF` utilisé par le tracker.

### Protocole série

Les protections ajoutées sont pertinentes :

- contrôle exact de la longueur des paquets audio ;
- délai d’abandon d’un paquet incomplet ;
- resynchronisation ;
- buffers de réception agrandis ;
- débit porté à 921600 bauds ;
- dessin de l’oscilloscope différé hors du chemin critique.

Le protocole reste perfectible, mais le défaut de désynchronisation durable a été pris au sérieux.

### Tracker et interface

Le tracker possède maintenant les briques qui donnent une vraie identité à la machine :

- huit pistes ;
- huit patterns ;
- mode song ;
- note, instrument, effet et valeur par pas ;
- probabilité et conditions ;
- swing ;
- arpège, cut et retrig ;
- métronome ;
- moteur et patch accessibles depuis le panneau latéral.

Les commandes de simulation `SIMNAV` et `SIMBTN` sont une bonne idée pour automatiser les parcours sans dépendre du clavier physique.

## Défauts prioritaires

### P1 — Le chargement d’un projet ne remet pas les instruments de pas à leur valeur par défaut

Dans `loadProject()`, une valeur `INST` égale à `0xFF` n’est pas envoyée au Teensy :

```cpp
### Défauts prioritaires

#### ✅ P1 — Le chargement d’un projet ne remet pas les instruments de pas à leur valeur par défaut (RÉSOLU)

#### ✅ P1 — Les sauvegardes projet et patch ne sont pas atomiques (RÉSOLU)

#### ✅ P1 — La sauvegarde Game Boy dépend de la sortie propre du jeu (RÉSOLU)

En parallèle, la boucle principale modifie les moteurs, les patches et les structures du tracker sans section critique générale. Les accès unitaires en 8 bits sont atomiques, mais une commande composée peut être observée dans un état intermédiaire.

La charge actuelle est faible et le matériel fonctionne, mais ce point peut produire des glitches rares difficiles à reproduire.

**Recommandation :** réduire l’ISR à une file d’événements horodatés ou protéger explicitement les changements multi-champs. Ajouter des compteurs de dépassement et une mesure de durée maximale de l’ISR.

### P2 — L’émulateur n’est pas encore validé comme une console « vitesse officielle »

Le code vise 59,7275 images/s et publie `GB:FPS`, mais :

- le cœur et le rendu restent dans la boucle principale ;
- le rattrapage est volontairement abandonné après un gros retard ;
- le rendu saute une image sur deux ;
- Walnut-CGB a des limites de rendu ligne par ligne reconnues ;
- l’audio est mono 8 bits à 14 kHz puis répété vers 44,1 kHz sans interpolation fine.

Ce n’est pas mauvais pour un bonus intégré à une groovebox, mais ce n’est pas encore un émulateur de référence.

**Validation minimale à faire :** matrice de 10 ROMs GB/GBC, FPS min/moyen, glitches audio, commandes, sauvegarde, 30 minutes par ROM.

### P2 — Le sampleur est une fondation, pas encore un sampleur complet

Le moteur contient seulement Kick et Snare embarqués. Les fichiers WAV capturés depuis l’émulateur ne peuvent pas encore être sélectionnés et joués par le moteur sampleur.

Autres limites connues :

- pas de streaming SD ;
- pas de découpage ;
- pas de normalisation ;
- pas de boucle ;
- pas de filtre anti-repliement pour les fortes transpositions ;
- pas de gestion de banque utilisateur.

Le code `AudioPlaySampler` est simple et propre pour un premier one-shot, mais les changements de note et de sample arrivent depuis un autre contexte que `update()` sans verrou audio explicite.

### P2 — La CI ne couvre qu’une petite partie de la logique

La CI compile les deux firmwares et lance les tests natifs du protocole. C’est une vraie amélioration.

Limites :

- pas de tests des sauvegardes ;
- pas de tests du parser projet ;
- pas de tests du framing binaire avec octets perdus/corrompus ;
- pas de tests du scheduler, du swing ou des effets de pas ;
- pas de test du sampleur ;
- aucun statut de contrôle n’était associé au SHA audité via l’API GitHub au moment de l’audit.

**Recommandation :** extraire la logique pure des deux `main.cpp` pour la rendre testable nativement, puis rendre les contrôles CI obligatoires sur la branche principale.

## Dette technique

### Fichiers principaux trop gros

- `src_teensy/az2_audio/main.cpp` : environ 3 000 lignes ;
- `src_esp32/az2_screen/main.cpp` : environ 4 700 lignes.

Cela augmente le risque de régression et rend les modifications simultanées dangereuses.

Découpage conseillé :

- Teensy : `AudioGraph`, `EngineRack`, `Sequencer`, `ProjectState`, `SerialProtocol`, `GbAudioBridge` ;
- ESP32 : `ScreenRouter`, `TrackerView`, `PatchView`, `ProjectStore`, `InputRouter`, `GbFrontend`.

### Documentation embarquée devenue incohérente

Plusieurs commentaires et le README décrivent encore un état ancien :

- README : cinq synthétiseurs au lieu de six moteurs ;
- README : UART 230400 au lieu de 921600 ;
- écran : commentaire « pas de son » alors que l’audio GB est actif ;
- Teensy : long commentaire présentant encore Dexed comme bruyant ;
- protocole : commentaire parlant du passage à 256 patches alors que la valeur finale est 255.

Ce n’est pas un défaut d’exécution, mais c’est dangereux pour les prochains développements.

### État de projet incomplet

Le projet sauvegarde beaucoup de données utiles : tempo, division, gamme, swing, song, moteurs, patches, filtres, enveloppes, volume, mute, patterns, probabilités et conditions.

Il ne sauvegarde notamment pas :

- l’état du métronome ;
- les niveaux de réverbération et délai lorsqu’ils sont pilotés autrement que par les potentiomètres ;
- le fill courant ;
- certains états d’interface, ce qui est acceptable.

Il faut définir officiellement ce qui appartient au morceau et ce qui appartient à la session.

### Audio mono

Toute la chaîne principale est mono puis dupliquée sur gauche/droite. Il n’y a pas de panoramique par piste. C’est acceptable pour l’alpha, mais ce sera un manque face aux grooveboxes concurrentes.

## Évaluation par domaine

| Domaine | Note | Commentaire |
|---|---:|---|
| Architecture produit | 8,5/10 | bonne séparation Teensy/ESP32 et périmètre AZ-2 désormais réaliste |
| Audio et moteurs | 8/10 | six moteurs, souffle résolu, grande marge CPU ; validation longue durée encore à faire |
| Tracker | 8/10 | fonctions musicales solides ; INST par pas non appliqué et concurrence ISR à sécuriser |
| Interface | 7,5/10 | riche et cohérente ; dernières pages pas toutes validées visuellement |
| Émulation | 5,5/10 | jouable et intégrée, mais vitesse/compatibilité non qualifiées |
| Sampleur | 4,5/10 | moteur one-shot propre, seulement deux samples fixes |
| Robustesse des données | 5/10 | format riche, mais sauvegardes non atomiques et défaut INST au chargement |
| Tests/CI | 6/10 | bonne première base, couverture encore étroite |
| Maintenabilité | 5/10 | documentation abondante, mais deux gros monolithes et commentaires périmés |

## Niveau de maturité

**Classement recommandé : alpha matérielle avancée.**

Pour passer en bêta, les critères suivants devraient être remplis :

- zéro perte/corruption sur 100 cycles save/load ;
- sauvegardes atomiques ;
- correction du sentinel INST ;
- 2 heures de lecture tracker sans glitch ;
- test simultané 8 pistes + effets + écran + SD ;
- 10 ROMs testées 30 minutes avec FPS journalisé ;
- test UART prolongé avec compteurs d’erreurs ;
- CI verte et obligatoire ;
- découpage initial des deux gros fichiers ;
- inventaire clair « confirmé matériel / compilé / non testé ».

## Feuille de route recommandée

### Étape 1 — Sécuriser les données

1. Corriger le chargement `INST:255`.
2. Rendre projets, patches et sauvegardes GB atomiques.
3. Ajouter checksum/version aux formats.
4. Ajouter un test de round-trip complet.

### Étape 2 — Qualifier le temps réel

1. Mesurer durée maximale de l’ISR.
2. Compter les paquets UART perdus et les underruns audio.
3. Faire un stress test de deux heures.
4. Vérifier les six moteurs, toutes les pistes et les effets en même temps.

### Étape 3 — Finir le sampleur AZ-2

1. Indexer les WAV de la SD Teensy.
2. Charger des one-shots utilisateur en PSRAM.
3. Relier les captures Game Boy au moteur sampler.
4. Ajouter trim début/fin et normalisation simple.
5. Garder streaming long, timestretch et fonctions avancées pour une version ultérieure.

### Étape 4 — Mesurer l’émulation

1. Enregistrer `GB:FPS` par ROM.
2. Ajouter compteur d’underrun audio.
3. Sauvegarder la RAM périodiquement.
4. Documenter les ROMs compatibles et les limites Walnut-CGB.
5. Ne viser 22,05 kHz qu’après un protocole audio V2 avec longueur 16 bits.

### Étape 5 — Réduire la dette

1. Extraire les modules testables des deux `main.cpp`.
2. Corriger les commentaires et le README.
3. Ajouter des tests natifs sur projet, protocole binaire et séquenceur.
4. Geler un format de projet versionné.

## Conclusion

Oui, **le projet avance vite et dans la bonne direction**. Les progrès les plus importants sont réels : souffle résolu, six moteurs, banques complètes, tracker enrichi, communication série renforcée, mesures mémoire/CPU et début de CI.

La priorité n’est plus d’ajouter encore beaucoup de fonctions. La priorité est maintenant de rendre l’existant fiable : sauvegardes sûres, état reproductible au chargement, tests prolongés et séparation progressive des gros fichiers.

Avec ces corrections, AZ-2 peut raisonnablement passer d’une alpha démontrable à une bêta musicale solide sans changer le matériel.
