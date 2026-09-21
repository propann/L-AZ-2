# AZ-2 — Audit des moteurs et du rack audio

**Date :** 21 septembre 2026  
**Cible :** Teensy 4.1 maître audio, ESP32-S3 interface/GB

## État vérifié dans le code

Le protocole partagé expose six moteurs :

| ID | Moteur | Patches exposés | État actuel |
|---:|---|---:|---|
| 0 | DEXED | 255 | quarantaine : souffle confirmé sur le matériel |
| 1 | EPIANO | 5 | intégré, à valider à l'écoute |
| 2 | BRAIDS | 43 formes | intégré, comportement note tenu simulé par coupure du canal |
| 3 | KARPLUS | 1 | intégré, aucun paramètre de patch dans `AudioSynthKarplusStrong` |
| 4 | ANALOG | 11 formes | moteur de référence confirmé propre |
| 5 | SAMPLER | Kick, Snare, GB Capture | intégré, banque embarquée et capture GB |

Les sélections sont synchronisées par `AZ2_Protocol.h`, puis envoyées par
`ENGINE:<piste>:<moteur>` et `PATCH:<piste>:<patch>`. L'écran utilise les mêmes
comptes et noms que le Teensy.

## Rack par piste

Le firmware possède huit pistes. Pour chaque piste, le chemin actif est :

```text
moteur choisi
  -> enveloppe commune
  -> filtre passe-bas
  -> bitcrusher
  -> sortie sèche + delay court réglable
  -> mixeur de piste
  -> groupe A/B (4 pistes par groupe)
  -> mix final
  -> volume sec / reverb maître / delay maître
  -> I2S -> PCM5102A
```

Le changement de moteur déconnecte l'entrée de l'enveloppe et la reconnecte
au seul moteur sélectionné. Les effets et les mixeurs en aval restent fixes.
Le rack est donc correctement branché pour une piste active à la fois.

Les seize pads ont un bus séparé : quatre groupes de quatre sampleurs vers un
bus pads, mélangé avec la voix live, puis vers le mix final. Un pad chargé en
WAV joue son propre sample ; un pad ouvert depuis le tracker avec `track=` joue
le moteur de la piste ciblée. La voix live générale reste une voix DEXED
séparée et ne change pas avec le moteur de piste.

## Points propres

- Les six moteurs sont sélectionnables par piste et le patch est remis à zéro
  lors du changement.
- Les huit pistes sont toutes raccordées aux deux groupes puis au mix final.
- Les effets dédiés par piste sont transparents au démarrage : bitcrusher à
  16 bits, delay à 0 ms et retour delay à zéro.
- La reverb et le delay longs restent sur le bus maître, ce qui évite une
  explosion de RAM avec huit pistes.
- Le sampleur de pads est indépendant du sampleur chromatique des pistes.
- Le pool `AudioMemory` est dimensionné à 700 blocs ; le pire cas documenté
  avec huit delays utilise 694/700 blocs et environ 10,4 % de CPU en pointe.

## Limites à corriger avant d'ajouter beaucoup de moteurs

1. **Allocation trop large :** les instances DEXED, EPiano, Braids, Karplus,
   Analog et Sampler sont créées pour les huit pistes, même si une seule
   instance de famille est active par piste. Le graphe évite le calcul des
   moteurs non sélectionnés, mais pas leur coût de construction et de RAM.
   C'est le principal chantier d'architecture du rack : pool d'instances ou
   adaptateurs, avec une limite de polyphonie par moteur.
2. **ADSR commune au sampleur de piste :** `trackSamplerEngine` passe par la
   même enveloppe que les synthés. Pour les one-shots, le `noteOff()` ne coupe
   pas le sample ; la durée naturelle doit être testée avec l'enveloppe et le
   delay. Il faut prévoir un mode one-shot/gate explicite.
3. **Paramètres incomplets :** Karplus n'expose encore ni decay, ni damping,
   ni brightness. Analog n'expose que la forme, tandis que filtre/ADSR sont
   génériques. Les commandes CRUSH/DELAY existent côté Teensy mais n'ont pas
   encore de panneau écran dédié.
4. **Validation audio partielle :** Analog est confirmé à l'écoute. EPiano,
   Braids, Karplus et Sampler sont compilés et routés, mais doivent encore
   passer un test reproductible de notes, changements de patch et notes
   bloquées. DEXED reste hors du démarrage par défaut.

## Ajouts proposés, dans l'ordre

### 1. Finir le rack actuel

- mesurer CPU/RAM par moteur à 1, 2, 4 et 8 pistes ;
- ajouter le mode sampler one-shot/gate et son choix dans le patch ;
- exposer les paramètres Karplus ;
- ajouter les contrôles CRUSH/DELAY à la page PATCH ;
- centraliser un `PANIC` et un test de changement de moteur répété.

### 2. Ajouter un moteur léger en laboratoire

Le meilleur prochain candidat est un **FM2** à deux opérateurs : famille sonore
complémentaire, beaucoup moins lourde que DEXED et testable dans un banc isolé.
Ensuite viennent un **drum synth** léger et un **chiptune natif**. Ils doivent
être développés dans `src_teensy/engine_lab/`, mesurés, puis seulement
ajoutés au catalogue partagé après validation.

Plaits, wavetable et granular restent des candidats de phase suivante. Ils ne
doivent pas être ajoutés directement aux huit pistes avant mesure de la RAM,
du CPU et de la latence.

## Conclusion

Le rack actuel est fonctionnel et correctement raccordé pour six familles,
huit pistes, un bus pads et les effets maître. Le gain immédiat ne vient pas
d'un septième moteur dans `main.cpp` : il vient de la réduction des instances
allouées, de la séparation sampler/ADSR et de la validation audio des moteurs
déjà présents. Le laboratoire FM2 est maintenant présent dans
`src_teensy/engine_lab_fm2/` avec une cible PlatformIO séparée
`engine_lab_fm2`. Il reste monophonique et isolé tant que les mesures CPU/RAM
et l'écoute sur le vrai DAC ne sont pas faites ; le firmware stable n'en
dépend pas.
