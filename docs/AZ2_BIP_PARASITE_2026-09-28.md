# Bip parasite périodique — journal de diagnostic du 28 septembre 2026

**Statut : NON RÉSOLU À LA RACINE.** Le bip est absent sur la version figée
ici, après une coupure d'alimentation. La cause n'est pas identifiée. Ce
document existe pour que la prochaine session ne recommence pas les
tentatives déjà épuisées.

## Symptôme

- Un bip périodique, environ une fois par seconde, dans la sortie audio.
- Présent **dans toute l'application**, pas seulement dans les émulateurs.
- Survit à STOP et à PANIC.
- **Seule une coupure d'alimentation complète le fait disparaître.**

## Le fait le plus important

Un artefact qui survit à STOP mais que seul un redémarrage nettoie est un
**état verrouillé**, pas un signal généré en continu.

C'est la clé, et elle explique pourquoi trois correctifs successifs ont
échoué : ils coupaient des **générateurs**, alors que le problème est un état
qui reste accroché en aval.

La même signature avait déjà été observée le 18 septembre 2026
(`AZ2_ETAT_DES_LIEUX.md`, vers la ligne 1275) : « Seul un power-cycle complet
a nettoyé l'état — pas rejouable à la demande avec juste STOP/PLAY. »
Attribuée alors à une hypothèse de nombres dénormaux dans la boucle de
feedback FM de Synth_Dexed. Jamais confirmée.

## Aucun chemin logiciel ne remet le bus audio à zéro

`panicAllAudio()` ne coupe que les **voix** : moteurs Dexed / EPiano /
Karplus / Sampler / Analog, pads, voix live. Tout ce qui est en aval conserve
son état jusqu'à la coupure d'alimentation :

| État | Remis à zéro ? |
| --- | --- |
| `gbRingHead` / `gbRingTail` | **jamais** — seules écritures : l'init à la déclaration et l'avancement |
| `gbResampleHavePrev` / `gbResamplePhaseQ16` | **jamais** — phase et échantillon précédent persistent |
| `reverbUnit` | configuré dans `setup()` uniquement, jamais vidé |
| `delayUnit` (350 ms) | configuré dans `setup()` uniquement, jamais vidé |
| retour rack `rackAudioIn` | jamais coupé (`kMuteExternalRackAudio = false`) |

De plus, l'anneau audio GB est branché sur la sortie **au volume général et
en permanence**, sans aucune condition d'émulateur :

```
main.cpp   AudioConnection patchGbAudioToMaster(gbAudioQueue, 0, mixMaster, 3);
main.cpp   mixMaster.gain(3, masterVolume);   // jamais mis à 0
```

**PANIC ne peut donc pas nettoyer ce bip, par construction.** C'est la raison
structurelle pour laquelle ce bug « revient » : il se verrouille, et rien
dans le firmware ne sait le déverrouiller.

## Tentatives épuisées — NE PAS RECOMMENCER

| Tentative | Commit | Pourquoi ça n'a rien changé |
| --- | --- | --- |
| Couper le métronome | `a33fe50` | Le métronome était déjà triple-muté ; il est désormais entièrement supprimé, le bip persistait |
| Désactiver le MIDI | `47f92dd` | Le MIDI était **déjà inerte** avant ce commit (voir ci-dessous) |
| Couper le retour audio du rack | `kMuteExternalRackAudio` | Remis à `false` depuis ; jamais conclu par une mesure |

Le point commun des trois : aucune mesure n'a jamais été prise avant ou
après. Le bip a été chassé par suppression de sources suspectes, jamais par
observation.

## Le MIDI est définitivement écarté

Le composant MIDI (6N138) n'est pas monté. L'hypothèse « c'est le MIDI qui
est codé mais la puce n'est pas installée » a été vérifiée ligne par ligne et
elle est **fausse** : quatre verrous indépendants l'empêchaient déjà.

| Verrou | État |
| --- | --- |
| Code USB MIDI | supprimé (avant : corps vide via `AZ2_DISABLE_MIDI`) |
| Parseur MIDI DIN | supprimé (avant : appel commenté dans `loop()`) |
| `Serial8` | **jamais ouvert** — l'UART n'échantillonne même pas la broche |
| Pins 34 (RX8) et 35 (TX8) | `INPUT_PULLDOWN` dans `setup()` |

Aucun octet ne peut devenir une note, même si la broche du 6N138 absent capte
du bruit électrique : il n'y a aucun code en face pour la lire.

**Ne pas retirer les pull-downs des pins 34 et 35.** Celui de la pin 34
existait, il a été supprimé par accident en même temps que le code MIDI, puis
rétabli le 28 septembre. Une entrée CMOS flottante oscille au gré du bruit et
injecte du courant de commutation dans l'alimentation partagée avec le
PCM5102A.

## Angle mort à connaître

L'élimination binaire du 18 septembre 2026 concluait « toutes les pistes
coupées = silence total ». **Cette conclusion est antérieure au rack audio
externe** (commits `4ef226c`, `70fca84`, `b01ad65`). Le retour
`rackAudioIn` → `mixOutputL/R` n'existait pas quand ce silence a été validé,
et il est mixé **en aval de `mixMaster`** : il contourne les mutes de pistes,
le mute du métronome et le choix des moteurs.

Ce « silence total » ne prouve donc plus le silence sur le firmware actuel.

## À la prochaine occurrence : mesurer AVANT de redémarrer

Le redémarrage détruit la preuve. Deux instruments existent déjà dans le
firmware, aucun flash nécessaire :

- **`RACK:STATUS`** → `AZ2:RACK:INPUT:peak_l=…:peak_r=…:final_l=…:final_r=…`
  Crêtes mesurées sur l'entrée I2S du rack (`rackPeakL`/`rackPeakR`). Non
  nulles pendant le bip ⇒ le bip **entre par le rack**, le Teensy ne fait que
  le remixer.
- **`GB:AUDIO_RX`** (émis d'office toutes les 5 s sur l'USB série) →
  `packets`, `bad_len`, `timeouts`, `ring_drop`. Si `packets` augmente alors
  qu'aucun émulateur ne tourne, le Teensy reçoit ou mal-parse des paquets
  audio en permanence.

Procédure : ouvrir le moniteur série, noter ces deux lignes **pendant** que le
bip est audible, et seulement ensuite redémarrer.

## Étape suivante proposée

Étendre PANIC en véritable remise à zéro du bus : vider l'anneau GB,
réinitialiser l'état du rééchantillonneur, purger reverb et delay. Deux
bénéfices :

1. Le bip se récupère par un bouton au lieu d'une coupure d'alimentation.
2. C'est l'instrument qui identifie le coupable : on purge élément par
   élément, celui qui fait taire le bip est le fautif.

## Bug distinct trouvé au passage (toujours ouvert)

Ce n'est **pas** la cause du bip — ce chemin ne porte du son que quand un
émulateur tourne — mais c'est un vrai défaut, chiffré :

`lib/AZ2_Protocol/AZ2_Protocol.h` :

```cpp
constexpr uint8_t kGbAudioSamplesPerPacket =
    static_cast<uint8_t>(kGbAudioSampleRate / (4194304.0 / 70224.0));
```

`14000 / 59,7275 = 234,398` → **tronqué à 234** par le cast `uint8_t`.

L'ESP32 envoie 234 échantillons par frame émulée ; le Teensy rééchantillonne
avec un ratio fixe supposant 14000 Hz ; l'I2S consomme à 44100 Hz exactement.
**Aucun asservissement d'horloge.**

| | Débit |
| --- | --- |
| produit réellement | 234 × 59,7275 = 13 976,2 Hz → 44 025 éch/s après upsample |
| consommé par l'I2S | 44 100 éch/s |
| déficit | **75 éch/s permanent** |

Un trou = 1 bloc `AudioPlayQueue` = 128 échantillons :

- cas idéal (59,73 fps) → un trou toutes les **1,7 s**
- 59,64 fps (mesure GBC réelle) → toutes les **920 ms**
- intro GBC double-vitesse, 47–51 fps → toutes les **20 ms**
- NES à 49–50 FPS → toutes les **16 ms** (grésillement continu)

Aucun arrondi entier ne sauve ça : 235 donnerait +36 Hz d'excédent, donc un
débordement lent au lieu d'une famine lente. Il faut de l'asservissement.

Deux raisons pour lesquelles ce défaut a survécu si longtemps :

1. **L'instrumentation est aveugle.** `reportGbAudioHealth()` compte
   `ring_drop` (débordement) mais **aucun compteur de famine** n'existe dans
   le projet. Le rapport affiche `ring_drop=0` et tout paraît sain.
2. **Grossir le tampon déplace la période sans rien corriger.** Le feeder vide
   l'anneau dès que `ready >= AUDIO_BLOCK_SAMPLES`, donc l'anneau tourne en
   permanence quasi vide : la capacité n'amortit pas la dérive moyenne.

Correctif à faire : compteur de famine, pré-remplissage (~8 blocs avant
démarrage, cible ~50 %), puis rééchantillonnage asservi
(`kGbResampleStepQ16` variable, corrigé de ±0,5 % max selon l'écart au
remplissage cible). Ensuite seulement, ramener `kGbRingCapacity` de 16384 à
~4096.

## Version figée

Les deux firmwares de production compilent sur cette version :

- `pio run -e master_teensy_rack_lab` : SUCCESS
- `pio run -e screen_esp` : SUCCESS

## Mise à jour du 1er octobre 2026

Commit `ecca553` : la source audio GB est désormais tirée directement par
l'ISR audio (`AudioGbRingSource`), l'asservissement se fait sur le vrai
remplissage de l'anneau, et PANIC vide enfin le chemin audio GB
(`gbAudioResetStream()`). Les pull-downs des broches 34/35 sont rétablis.

Validé à l'écoute sur matériel après flash du Teensy : émulation GB sans bug
de son, séquenceur et moteurs fonctionnels, plus de bruit parasite.

**Prudence :** le bip disparaissait déjà après chaque coupure d'alimentation.
L'absence de bip sur cette session ne prouve donc pas que sa cause racine
est trouvée. À la prochaine occurrence, appliquer la procédure « mesurer
AVANT de redémarrer » ci-dessus, puis redémarrer les cartes une par une.
