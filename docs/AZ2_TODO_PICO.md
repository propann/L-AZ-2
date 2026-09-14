# AZ-2 - A faire cote firmware Pico (note de suivi)

Le Pico n'est pas branche pendant qu'on travaille sur l'ESP32 et le Teensy
(2026-09-13). Cette page accumule tout ce qui, decide ailleurs, impliquera
un changement du firmware Pico plus tard -- **pour ne rien oublier au
moment de le rebrancher**. Rappel : les 3 firmwares (ESP32, Teensy, Pico)
forment une seule application repartie sur 3 cartes.

## En attente

- **Mode joystick/manette** (decide le 2026-09-13, voir
  [AZ2_FEUILLE_DE_ROUTE_MOTEUR.md](AZ2_FEUILLE_DE_ROUTE_MOTEUR.md)) : une
  fois le mode "JEUX/HACK" (Retro-Go) pret cote ESP32, le Pico devra
  pouvoir basculer son role clavier musical <-> manette de jeu (mapper la
  matrice 4x4 sur des boutons/direction plutot que sur `PAD:NN:DOWN`).
  Reflechir a : nouveau message du Teensy/ESP32 vers le Pico pour changer
  de mode (ex: `MODE:GAME` / `MODE:MUSIC`), et un mapping clavier/manette
  qui a du sens sur une matrice 4x4 seulement.
- **Retour LED colore selon le mode/etat** : quand le mode joystick
  marchera, colorer les LED du clavier differemment du mode musical (deja
  possible avec le mux RGB deja code, juste besoin d'un signal indiquant
  le mode courant).
- **Choix de moteur par piste** (fait cote ESP32/Teensy le 2026-09-14,
  voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md) : le protocole dedie existe deja
  (`ENGINE:piste:moteur`, `PATCH:piste:patch`, pas de surcharge de
  `MACRO:`). Reste a faire cote Pico QUAND il sera rebranche : si on veut
  piloter/refleter le moteur actif depuis le clavier physique (ex: un
  encodeur qui cycle le moteur d'une piste, LED qui change de couleur
  selon le moteur), il faudra lire ces 2 messages dans
  `handleTeensyLine()` (comme deja fait pour `LED:`) et emettre
  `ENGINE:`/`PATCH:` en reponse a un appui/rotation -- rien a coder pour
  l'instant, le Pico actuel ignore juste ces lignes sans erreur.

## Deja pris en compte (rien a faire de plus pour l'instant)

- Le protocole actuel (`PAD:`, `MACRO:`, `ENC:`, `LED:`, `STEP:`, `BPM:`,
  `PLAY`/`STOP`) reste valable tel quel pour le Pico : aucune de ces
  commandes n'a change de forme depuis le dernier firmware Pico teste.
