# AZ-2 — Architecture multi-moteurs audio

**Date :** 18 septembre 2026  
**Cible principale :** Teensy 4.1 + ESP32-S3 écran  
**Principe :** le Teensy reste le maître audio; l'ESP32-S3 actuel reste l'interface, le stockage de projets et la console GB/GBC.

## Décision centrale

AZ-2 ne doit plus dépendre de DEXED. DEXED devient un moteur expérimental en quarantaine jusqu'à validation matérielle. Le produit repose sur un catalogue de moteurs interchangeables, avec le sampler comme moteur principal au même niveau que les synthétiseurs.

Tous les candidats sont d'abord portés dans un environnement **Engine Lab Teensy** distinct. Un moteur n'entre dans le firmware principal que s'il satisfait les tests CPU, RAM, silence, notes bloquées, changement de patch et endurance.

Il ne faut pas construire toutes les instances au démarrage. C'est précisément ce qui consomme inutilement le tas aujourd'hui avec les instances DEXED. « Disponible dans le dépôt » ne doit plus signifier « alloué en permanence dans le firmware ».

## Répartition des deux microcontrôleurs

| Fonction | Teensy 4.1 | ESP32-S3 écran |
|---|---:|---:|
| Horloge et séquenceur | Maître | Miroir/commandes |
| Synthèse et sampler | Oui | Non |
| Mixage, effets, DAC PCM5102A | Oui | Non |
| Contrôles physiques | Lecture directe | Affichage |
| Interface tactile | Non | Oui |
| Projets, patches, ROMs | État actif + commandes | Stockage SD |
| Émulation GB/GBC | Audio reçu et mixé | Émulation et vidéo |
| Diagnostic | CPU/RAM/audio | UI/FPS/SD/UART |

Le protocole doit transporter des intentions musicales et des états, pas déplacer la synthèse principale sur l'ESP32 d'interface.

## Moteurs actuellement présents

| Moteur | Famille | État | Décision |
|---|---|---|---|
| ANALOG | VA simple, Teensy Audio | Confirmé propre sur matériel | Garder, améliorer |
| BRAIDS | Macro-oscillateur | Compilé et actif | Garder |
| KARPLUS | Modélisation de corde | Compilé et actif | Garder, mesurer |
| MDA EPIANO | Piano électrique | Compilé et actif | Garder, mesurer |
| DEXED | FM DX7 six opérateurs | Souffle confirmé | Quarantaine |
| GB/GBC AUDIO | Chiptune externe | Fonctionne via ESP32→UART→Teensy | Garder |
| SAMPLE REC | Enregistrement WAV | Capture seulement | Incomplet |
| SAMPLE PLAY | Lecture/slicing | Absent | Priorité absolue |

## Catalogue cible complet

### Niveau A — à intégrer en premier sur le Teensy

1. **Sampler / multisampler AZ-Sample**
   - one-shot, gate, loop et reverse;
   - lecture WAV 16 bits depuis SD;
   - pitch indépendant, enveloppe, filtre, volume et choke groups;
   - marqueurs de début/fin, découpe en slices et assignation à une piste;
   - cache en PSRAM pour petits échantillons, streaming SD pour les longs;
   - bounce/freeze pour libérer le CPU.

2. **Plaits / Macro Engine**
   - successeur naturel de BRAIDS;
   - moteur VA, wavetable, chords, FM, formant, additive, physical modeling et drums dans une même famille;
   - excellent rapport diversité/quantité de code;
   - source Mutable Instruments disponible dans le dépôt officiel `pichenettes/eurorack`;
   - DaisySP fournit également une adaptation Plaits sous licence MIT.

3. **Drum Synth**
   - kick analogique et synthétique;
   - snare analogique et synthétique;
   - hi-hat métallique;
   - tom/clap/noise ensuite;
   - DaisySP possède déjà ces briques sous licence MIT.

4. **Wavetable**
   - tables internes légères;
   - import de tables utilisateur depuis SD;
   - morphing entre tables;
   - unison limité et anti-aliasing mesuré.

5. **Physical Modeling**
   - corde, modal/resonator et percussion physique;
   - DaisySP propose KarplusString, StringVoice, ModalVoice et Resonator;
   - peut remplacer/compléter le Karplus actuel.

### Niveau B — à tester avant promotion

6. **FM légère**
   - moteur FM2 à deux opérateurs comme solution fiable et légère;
   - sert de moteur FM de secours si DEXED reste instable;
   - ne remplace pas le six-op DX7, mais couvre beaucoup de basses, cloches et sons métalliques.

7. **Granular**
   - grains depuis échantillon en PSRAM;
   - densité, taille, position, pitch et spray;
   - DaisySP fournit un GranularPlayer de départ;
   - moteur gourmand : une ou deux pistes maximum avant mesures.

8. **Additif / Harmonic**
   - banque d'oscillateurs harmoniques;
   - utile pour orgues, timbres spectraux simples et sons évolutifs;
   - candidat DaisySP OscillatorBank/HarmonicOscillator.

9. **Formant / voix synthétique**
   - moteur léger et très identifiable;
   - bon complément créatif au tracker;
   - candidat DaisySP FormantOscillator / VosimOscillator.

10. **Chiptune natif**
    - pulse 12,5/25/50/75 %, wavetable 4-bit, noise LFSR;
    - enveloppes et pitch sweep de style Game Boy;
    - doit rester distinct du son provenant réellement de l'émulateur.

11. **Looper**
    - boucles synchronisées au tempo;
    - overdub, undo du dernier overdub, reverse et half-speed;
    - PSRAM/SD selon longueur;
    - moteur séparé du sampler pour garder une logique claire.

### Niveau C — expérimental ou futur coprocesseur

12. **Spectral/FFT** — freeze, vocoder, resynthèse; gourmand en RAM et latence.
13. **Granular polyphonique** — à déplacer si plusieurs pistes sont nécessaires.
14. **FM six-op polyphonique** — DEXED réparé ou moteur alternatif; candidat au coprocesseur.
15. **Wavetable unison massif** — trop coûteux pour huit pistes simultanées.
16. **Convolution/réverbération lourde** — plutôt futur nœud d'effets.
17. **Streaming audio haute qualité d'un coprocesseur** — I2S/TDM, jamais UART PCM 8-bit.

## Bibliothèques et licences retenues

| Source | Intérêt | Licence | Décision |
|---|---|---|---|
| Teensy Audio Library | sampler, granular simple, oscillateurs, filtres, mixage, I2S | Licence PJRC/MIT selon fichiers | Base native |
| DaisySP | synthèse, drums, physical modeling, granular, effets | MIT | Meilleur laboratoire commun |
| Mutable Instruments eurorack | Plaits, Rings et DSP originaux | Entêtes/licences à vérifier module par module | Port ciblé seulement |
| Synth_Braids | moteur actuel | MIT | Déjà intégré |
| Synth_MDA_EPiano | EPiano | GPLv3 | Déjà intégré |
| Synth_Dexed | DX7 | GPLv3 | Quarantaine |
| Faust | génération de DSP C++ | GPL pour l'outil, licence du DSP généré selon source | Outil de prototypage, pas moteur embarqué entier |

Références :
- https://github.com/electro-smith/DaisySP
- https://github.com/pichenettes/eurorack
- https://github.com/PaulStoffregen/Audio
- https://github.com/grame-cncm/faust

## Engine Lab Teensy

Chaque moteur doit avoir son propre environnement PlatformIO et son propre adaptateur. Il ne doit pas être ajouté directement au gros `main.cpp`.

Structure cible :

```
src_teensy/
  az2_audio/                 firmware stable
  engine_lab/
    common/                  banc de mesure et commandes
    sampler/
    plaits/
    drums/
    wavetable/
    physical/
    fm2/
    granular/
    chip/
    looper/
```

Chaque laboratoire accepte les mêmes commandes minimales :

```
LAB:ENGINE?
LAB:NOTE:<note>:<velocity>
LAB:OFF:<note>
LAB:PARAM:<index>:<value>
LAB:PATCH:<index>
LAB:CPU?
LAB:RAM?
LAB:PANIC
```

## Contrat commun d'un moteur

Un moteur promu doit exposer :

- `prepare(sampleRate, blockSize)`;
- `noteOn(note, velocity)`;
- `noteOff(note)`;
- `allNotesOff()`;
- `setParameter(id, value)`;
- `loadPatch(id)`;
- `render(output, frames)`;
- `estimatedRam()`;
- `capabilities()`.

Les capacités déclarent polyphonie, sampler, stéréo, entrée audio, besoin PSRAM, streaming SD et compatibilité future avec un coprocesseur.

## Règles de ressources

1. Aucun moteur non sélectionné ne doit calculer de bloc audio.
2. Aucun gros moteur expérimental ne doit être construit globalement huit fois.
3. Une piste obtient une instance depuis un pool; elle ne possède pas toutes les familles de moteurs.
4. Polyphonie adaptative selon le moteur et la charge CPU.
5. Seuil cible : CPU moyen inférieur à 65 %, pointe inférieure à 80 %.
6. Réserve mémoire obligatoire après chargement : au moins 25 % du tas utilisable.
7. Toute allocation critique est vérifiée.
8. `PANIC` coupe réellement toutes les notes et remet les états DSP à zéro.
9. Le sampler et les delays utilisent PSRAM/SD sans bloquer l'interruption audio.
10. Le moteur stable ne dépend jamais de l'ESP32 d'interface pour produire le son.

## Sampler : contrainte matérielle

Le PCM5102A est uniquement un DAC : il ne possède aucune entrée analogique. AZ-2 peut déjà capturer un flux numérique interne, mais pour sampler un micro, une cassette ou un instrument externe, il faudra ajouter un ADC ou un codec audio.

Le logiciel doit néanmoins être conçu maintenant avec trois sources :

- flux interne GB/GBC;
- import WAV depuis la SD de l'ESP32 ou la SD Teensy;
- entrée audio future quand le codec sera ajouté.

## Futur coprocesseur audio

Le troisième microcontrôleur reste optionnel. Il ne remplace pas l'ESP32-S3 écran.

Architecture future :

```
ESP32-S3 UI/SD/GB
        |
        | commandes/état
        v
Teensy 4.1 maître audio/séquenceur/mix/DAC
        |
        | I2S ou TDM audio + lien contrôle séparé
        v
Coprocesseur moteur optionnel
```

Candidats :

- autre Teensy 4.1 : compatibilité maximale et développement simple;
- Daisy Seed/STM32H7 : fort DSP et DaisySP natif;
- ESP32-S3 dédié sans Wi-Fi/Bluetooth : granular/sampler/FX;
- RP2350 : moteurs légers, chiptune ou oscillateurs, pas moteur spectral principal.

Le flux audio d'un coprocesseur doit arriver en I2S/TDM. L'UART reste réservé aux commandes et diagnostics.

## Ordre d'intégration

### Phase 0 — sécuriser

- garder DEXED désactivé par défaut;
- terminer le test MSFA sur matériel;
- corriger le pool d'instances et la stratégie d'allocation;
- conserver la CI verte.

### Phase 1 — cœur produit

1. Sample playback;
2. slicing/choke/reverse;
3. Plaits;
4. Drum Synth;
5. Wavetable.

### Phase 2 — couleur sonore

6. Physical Modeling;
7. FM2;
8. Chiptune natif;
9. Granular;
10. Looper.

### Phase 3 — moteurs lourds

11. DEXED réparé;
12. spectral/freeze;
13. effets lourds;
14. coprocesseur audio.

## Tests obligatoires par moteur

- compilation isolée et intégrée;
- silence au repos;
- 1 000 cycles note-on/note-off;
- changement de patch pendant lecture;
- `PANIC` après note bloquée;
- mesure CPU à 1, 2, 4 et 8 pistes;
- mémoire avant/après 30 minutes;
- changement répété de moteur;
- sauvegarde/chargement du patch;
- test audio subjectif au casque et enregistrement;
- absence de clic lors de l'activation/désactivation.

## Conclusion

La meilleure collection pour AZ-2 n'est pas vingt copies de synthés classiques. Le noyau gagnant est :

- sampler/multisampler;
- Plaits;
- drum synth;
- wavetable;
- physical modeling;
- FM légère;
- granular;
- chiptune;
- EPiano;
- looper;
- DEXED seulement lorsqu'il est réellement fiable.

Le Teensy peut tous les accueillir dans le dépôt et dans des laboratoires séparés, mais il ne doit pas tous les instancier dans le même firmware. C'est ainsi que la machine restera vaste sans devenir une petite centrale nucléaire qui souffle dans le casque.
