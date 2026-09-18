# AZ-2 — Audit technique de progression

**Date :** 18 septembre 2026  
**Révision auditée :** `88f304a5967bbf2dfafc0c7cd97a1bc7e21c531c` (`main`)  
**Référence précédente :** `ff58b967b1d6f15c568d8b20d2ae1b4e74121aaf`  
**Méthode :** revue statique du dépôt et comparaison des 88 commits. Aucun firmware ni matériel modifié par cet audit.

## Verdict

AZ-2 n'est plus une simple preuve de concept. Le dépôt contient désormais une groovebox pré-alpha cohérente et essayable : écran/tactile réel, liaison ESP32–Teensy validée, séquenceur 8 pistes, tracker, sauvegarde de projets et de patches, cinq moteurs, effets maîtres, contrôles physiques et émulation GB/GBC avec audio routé vers le Teensy.

La progression est forte, mais l'ensemble n'est pas encore fiable pour une démonstration publique sans précautions. Le moteur DEXED est défaillant sur le matériel, la marge de tas Teensy mesurée est critique, les sauvegardes et le protocole inter-cartes ne sont pas transactionnels, les deux firmwares actifs sont devenus monolithiques et la couverture automatisée reste minime.

**Maturité estimée : pré-alpha avancée / prototype fonctionnel.**

## Progression depuis le premier audit

| Domaine | Premier audit | État actuel | Verdict |
|---|---|---|---|
| Liaison ESP32–Teensy | Incohérence Serial/Serial1 | UART 230400 validé sur les deux cartes | Corrigé |
| Écran/tactile | Absent | Écran 480×480 et FT6336U opérationnels | Corrigé |
| Horloge musicale | Tick fixe | BPM, division, swing et timer musical | Corrigé |
| Séquenceur | Absent | 8 pistes × 16 pas, 8 patterns, song mode | Forte progression |
| Tracker | Absent | NOTE/INST/FX/VAL/PRB/CND | Forte progression |
| Son | Sinusoïde de test | DEXED, EPIANO, BRAIDS, KARPLUS, ANALOG | Progression, DEXED bloquant |
| Sauvegarde | Absente | Patches et projets sur SD | Progression, robustesse à faire |
| Émulation | Vendoring sans intégration | GB/GBC, image, commandes, sauvegarde RAM, audio | Forte progression |
| Contrôles | Architecture incertaine | Croix, boutons et encodeurs sur Teensy | Clarifié |
| Tests | Aucun | 9 tests natifs du protocole pur | Début seulement |
| CI | Absente | Toujours absente | Non corrigé |
| Licence | Absente à la racine | GPLv3 + inventaire des dépendances | Corrigé en grande partie |
| Taille dépôt | ~346 Mo / 3 774 entrées | ~39,9 Mo de blobs / 2 480 entrées | Nettoyage réussi |

## Points positifs confirmés

- Rôles matériels désormais nets : Teensy pour audio/séquenceur/contrôles, ESP32-S3 pour UI/SD/émulation.
- Les environnements PlatformIO actifs et les plateformes sont figés.
- Le dépôt documente honnêtement ce qui est compilé, flashé et réellement testé.
- Le code vendored inutile a été fortement réduit.
- Les commandes invalides importantes renvoient des erreurs explicites.
- Probabilité, conditions de déclenchement et fill donnent au tracker une vraie différenciation.
- La charge audio mesurée (environ 6–12 % selon les essais consignés) laisse de la marge CPU.
- Le son GB et le moteur ANALOG confirment que la chaîne DAC/I2S générale fonctionne.
- Une licence racine et un inventaire des licences tierces existent.

## Constats critiques

### P0 — DEXED produit du bruit au lieu d'un son

Le défaut est confirmé sur matériel et isolé au moteur DEXED. ANALOG et l'audio GB sont propres sur la même chaîne de sortie. DEXED a été retiré des moteurs par défaut, ce qui réduit l'impact, mais ne répare pas la fonction annoncée.

**Risque :** démonstration instable, bruit persistant, perte de confiance dans le moteur phare FM.

**Action recommandée :** reproduire avec une seule instance DEXED et un patch minimal, capturer la sortie avant mixage, comparer les options de compilation et l'initialisation avec MicroDexed-touch, puis réintroduire les voix progressivement. DEXED ne doit pas être présenté comme fonctionnel avant validation audio longue durée.

### P0 — Marge mémoire Teensy critique

Le journal matériel indique environ 1,6 Ko libres dans l'arène du tas après démarrage. Plusieurs instances de synthèse utilisent des allocations dynamiques.

**Risque :** échec d'allocation silencieux, corruption ou panne seulement après certaines séquences d'utilisation.

**Action recommandée :** établir un budget RAM/AudioMemory par configuration, vérifier chaque allocation, mesurer démarrage/lecture/changement de moteur/GB, puis diminuer le nombre d'instances permanentes ou utiliser des allocations statiques contrôlées.

## Constats élevés

### P1 — Sauvegarde non atomique et chargement non transactionnel

Les fichiers sont supprimés avant réécriture. Une panne SD ou une coupure peut donc détruire la dernière sauvegarde valide. Le chargement d'un projet envoie ensuite un grand nombre de commandes unitaires sur l'UART, sans transaction globale ni accusé final.

**Risques :** projet partiellement chargé, divergence ESP32/Teensy, perte du fichier précédent.

**Recommandation :** format versionné, écriture temporaire + fermeture + renommage, checksum, commande BEGIN/COMMIT/ABORT et accusé final avec comptage des éléments.

### P1 — Protocole texte et binaire sur le même UART sans intégrité forte

Commandes, oscilloscope et audio GB partagent le lien. Les paquets binaires ont un marqueur et une longueur, mais pas de checksum, numéro de séquence ni mécanisme robuste de resynchronisation documenté.

**Risques :** corruption ponctuelle transformée en commande, perte audio ou désynchronisation difficile à diagnostiquer.

**Recommandation :** enveloppe unique avec type, longueur, séquence et CRC; files prioritaires séparant contrôle, scope et audio; compteurs d'erreurs visibles dans le diagnostic.

### P1 — Architecture devenue monolithique

Les principaux fichiers actifs font environ 179 Ko pour l'ESP32 et 103 Ko pour le Teensy. UI, stockage, parsing, protocole, séquenceur et orchestration sont fortement regroupés.

**Risques :** régressions lors des changements UI, tests difficiles, temps de revue élevé.

**Découpage conseillé :**

- ESP32 : navigation, pages UI, état miroir, stockage projet/patch, transport UART, émulateur;
- Teensy : séquenceur, moteurs, routage audio, contrôles, protocole;
- commun : modèle sérialisable versionné et validation pure testable sur PC.

### P1 — Validation automatique insuffisante

Le dépôt possède 9 tests utiles sur les helpers du protocole, mais aucun workflow GitHub Actions et aucun test AZ-2 pour le parser, le séquenceur, les sauvegardes, le swing, les probabilités, le song mode ou les erreurs de protocole. GitHub ne rapporte aucun run CI.

**Recommandation :** CI sur `pio test -e native`, compilation des deux environnements actifs, tests de parsing/fuzz léger, round-trip de projet et simulation déterministe du séquenceur.

## Constats moyens

### P2 — Écart entre fonctions annoncées et validées

Le README décrit largement les fonctions présentes, mais plusieurs restent seulement compilées ou partiellement essayées. La page PATCH la plus récente n'a pas encore été testée sur écran réel; DEXED est annoncé parmi les moteurs malgré son défaut; le sampler capture mais ne relit pas.

**Recommandation :** conserver dans le README une matrice « validé matériel / compilé / expérimental / indisponible ».

### P2 — Format projet coûteux

Un projet sauvegarde toutes les cases de tous les patterns sous forme textuelle. Le chargement peut générer plusieurs milliers de messages et écrit également les valeurs par défaut.

**Recommandation :** sauvegarde clairsemée, version explicite, validation avant application et commande de synchronisation en bloc.

### P2 — Usage intensif de `String` dans les boucles de commande

Le parsing et le stockage emploient fréquemment des `String` temporaires sur des microcontrôleurs à longue durée de fonctionnement.

**Risque :** fragmentation du tas, surtout avec la marge Teensy déjà faible.

**Recommandation :** buffers bornés et parsing sans allocation dans les chemins fréquents; mesures de fragmentation sur plusieurs heures.

### P2 — Dépôt encore dominé par du code de référence vendored

Le nettoyage est remarquable, mais `src_teensy/microdexed-touch/` représente encore l'essentiel des 2 106 fichiers. Cela complique la recherche et les audits.

**Recommandation :** conserver seulement les moteurs réellement compilés ou isoler l'upstream comme sous-module/artefact versionné, après vérification du besoin hors-ligne et des licences.

### P2 — Journal d'état utile mais trop volumineux

`AZ2_ETAT_DES_LIEUX.md` mélange journal chronologique, diagnostic et spécifications. Il est précieux, mais devient difficile à utiliser comme source de vérité.

**Recommandation :** séparer statut courant, journal de tests matériels, bugs connus et décisions d'architecture.

## Position concurrentielle

Les fonctions les plus prometteuses face aux trackers/grooveboxes existants sont :

1. tracker tactile et contrôles physiques dans la même machine;
2. conditions/probabilités/fill par pas;
3. cinq familles de synthèse;
4. émulation GB/GBC intégrée et routée dans la chaîne audio;
5. architecture ouverte et documentée.

Pour réellement concurrencer M8, Polyend Tracker, Digitakt/Syntakt ou Dirtywave, la priorité n'est plus d'ajouter beaucoup de fonctions. Il faut maintenant rendre le noyau fiable et rapide :

- sauvegarde instantanée et sûre;
- undo/redo;
- copie/collage, duplication et mutations de patterns;
- effets par piste;
- MIDI clock IN/OUT et synchronisation;
- vrai sampler avec lecture, découpe et assignation;
- arpegiateur live;
- navigation cohérente entièrement utilisable sans tactile;
- démarrage et reprise après erreur prévisibles.

L'émulation GB est différenciante, mais ne doit pas retarder la stabilité de la groovebox.

## Plan recommandé

### Sprint 1 — Stabilisation

1. Réparer ou désactiver officiellement DEXED.
2. Sécuriser la mémoire Teensy et documenter le budget.
3. Rendre sauvegarde/chargement atomiques et versionnés.
4. Ajouter CI et tests du séquenceur/projet.
5. Ajouter télémétrie des erreurs UART et SD.

### Sprint 2 — Utilisabilité

1. Refaire le panneau latéral tracker en raccourcis PATCH/EFFETS.
2. Uniformiser la navigation boutons/tactile.
3. Ajouter undo/redo et opérations rapides de pattern.
4. Afficher clairement les erreurs et confirmations de sauvegarde.

### Sprint 3 — Compétitivité musicale

1. Effets par piste.
2. MIDI clock et routage MIDI par piste.
3. Lecture/découpe/assignation du sampler.
4. Arpegiateur live.
5. Patches d'usine validés pour chaque moteur.

## Critères avant une démo publique

- 2 heures de lecture sans corruption, blocage ni croissance mémoire;
- 20 cycles save/load identiques avec coupure simulée;
- changement de moteur répété sans bruit résiduel;
- tous les contrôles utilisables sans tactile;
- reconnexion UART propre après redémarrage d'une seule carte;
- projets anciens relus par une version nouvelle;
- compilation et tests automatiques verts à chaque commit;
- DEXED réparé ou clairement marqué indisponible.

## Conclusion

Le projet a franchi une étape majeure en 88 commits : l'architecture matérielle fonctionne et le produit possède désormais une identité réelle. Le risque principal a changé. Ce n'est plus l'absence de fonctionnalités, mais l'accumulation rapide de fonctions dans deux gros firmwares sans garde-fous suffisants.

La meilleure stratégie pour dépasser les concurrents est donc de geler temporairement l'expansion, fiabiliser le noyau, puis reprendre les fonctions musicales à fort impact. Avec DEXED réparé, une sauvegarde robuste, une CI minimale et une navigation cohérente, AZ-2 pourra passer de « prototype impressionnant » à « instrument crédible ».
