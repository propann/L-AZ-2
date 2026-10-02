# Statut de la documentation

## À utiliser en priorité

1. `AZ2_ETAT_ACTUEL.md` — état matériel et firmware courant.
2. `README.md` — présentation et limites publiques.
3. `AZ2_DEMARRAGE.md`, `AZ2_REPRODUCTION.md`, `AZ2_CABLAGE_MASTER.md` — mise
   en route et câblage actif.
4. `AZ2_MANUEL_UTILISATEUR.md` — commandes utilisateur déjà intégrées.

## Historique

Les audits, mesures et sessions datés (`AZ2_AUDIT_*`, `AZ2_MESURE_*`,
`AZ2_SESSION_*`, `AZ2_ETAT_2026-09-20.md`) conservent les observations de leur
révision. Ils ne remplacent pas `AZ2_ETAT_ACTUEL.md` et leurs conclusions sur
Walnut-CGB, GNUBOY, MIDI DIN ou l’absence de capture fonctionnelle peuvent être
obsolètes.

## Archivé

Le **MIDI** est archivé depuis le 28 septembre 2026 : la puce 6N138 n'est pas
soudée et tout le code MIDI a été supprimé du firmware (commit `bbb584c`,
vérifié sur le binaire lié). Les sections MIDI de
`AZ2_ARCHITECTURE_MIDI_ET_MOTEURS_EXTERNES.md` et de `AZ2_RACK_PINOUT.md`
portent un bandeau d'archivage et ne décrivent plus le firmware réel.

Les documents Pico/LED (`AZ2_CABLAGE_PICO*.md`, `AZ2_TODO_PICO.md`) décrivent un
chantier abandonné. Ils sont conservés pour traçabilité et ne doivent pas être
utilisés comme plan de câblage de production.
