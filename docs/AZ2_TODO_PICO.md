# AZ-2 - TODO Pico -- CLOS (Pico abandonne le 2026-09-15)

Cette page suivait ce qu'il faudrait adapter dans le firmware Pico une
fois rebranche. **Le Pico est abandonne** (voir
[AZ2_CABLAGE_PICO.md](AZ2_CABLAGE_PICO.md) et
[AZ2_CABLAGE_MASTER.md](AZ2_CABLAGE_MASTER.md)) : le mux LED n'a jamais
fonctionne malgre un long diagnostic, remplace par une croix + 4 boutons
+ 3 potentiometres cables directement sur le Teensy.

Ce qui etait note ici reste valable EN ESPRIT, juste porte sur le Teensy
au lieu du Pico :

- **Mode joystick/manette pour le mode JEUX** : la croix + boutons A/B/C/D
  cables sur le Teensy (`NAV:`/`BTN:`, voir AZ2_CABLAGE_MASTER.md section
  3) servent deja a la fois de navigation ET de futurs boutons de jeu
  (voir [AZ2_EMULATION_JEUX.md](AZ2_EMULATION_JEUX.md)) -- pas besoin de
  bascule de mode complexe, ce sont les memes broches pour les deux usages
  au niveau protocole, a l'ESP32 de decider comment les interpreter selon
  l'ecran affiche.
- **Choix de moteur par piste** : deja fait cote ESP32/Teensy
  (`ENGINE:`/`PATCH:`, voir AZ2_FEUILLE_DE_ROUTE_MOTEUR.md) --
  independant du Pico, rien a refaire.
- **Retour LED colore** : sans matrice LED, plus d'affichage physique par
  pad -- l'ecran ESP32 reste la seule source de retour visuel pour
  l'instant.
