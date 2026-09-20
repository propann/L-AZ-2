# AZ-2 — Prototype RGB direct et double framebuffer

## Motif

Le rendu X3 reste limité par la copie vers le framebuffer PSRAM et par la
synchronisation du pilote RGB. Arduino_GFX configure actuellement un seul
framebuffer (`num_fbs=1`, `double_fb=false`) et ne publie pas de bascule VSYNC.
Le test PCLK 20 MHz a été rejeté car il brouillait l'image et déstabilisait toute
l'interface.

## Architecture retenue

1. Conserver le pilote Arduino_GFX pour la phase de démarrage et l'interface
   existante.
2. Créer un pilote local `esp_lcd` qui reprend les mêmes broches et timings
   stables : PCLK 12 MHz, 480x480, VSYNC GPIO17, HSYNC GPIO16, DE GPIO18.
3. Allouer deux framebuffers RGB565 en PSRAM avec `num_fbs=2`.
4. Exposer les deux adresses au rendu AZ-2 et ne jamais écrire dans le buffer
   actuellement utilisé par le DMA RGB.
5. Utiliser le callback de fin de frame pour publier l'index du buffer libre.
6. Invalider le cache uniquement pour la zone écrite avant de la publier.
7. Garder un interrupteur de compilation permettant de revenir au chemin
   Arduino_GFX tant que le prototype n'est pas qualifié.

## Critères de validation

- compilation ESP32 sans modification de la base Teensy ;
- écran de démarrage et toutes les pages tactiles inchangés ;
- image GB alignée, sans scintillement ni déchirure pendant 5 minutes ;
- X2 stable à environ 60 FPS ;
- X3 mesuré sur Zelda actif avec `missed=0` et comparaison des champs
  `display_avg_us`, `blit_copy_us` et `blit_flush_us` ;
- retour immédiat aux 12 MHz/Arduino_GFX si un seul critère visuel échoue.

## État au 20/09/2026

Le pilote actuel ne permet pas ce test directement. Aucun réglage partiel
(`num_fbs=2` seul) ne sera flashé : sans sélection du buffer libre, il peut
produire une image noire ou déchirée. Le firmware stable reste donc celui à
12 MHz, déjà reflashed après l'essai 20 MHz.

## API vérifiée

La version ESP-IDF livrée avec la carte fournit bien
`esp_lcd_rgb_panel_register_event_callbacks()` avec `on_vsync` et
`on_frame_buf_complete`, ainsi que `esp_lcd_rgb_panel_get_frame_buffer()` pour
récupérer plusieurs adresses. Le prototype peut donc rester entièrement local
et ne dépend pas d'un fork externe ; l'intégration devra seulement remplacer la
création du `Arduino_RGB_Display` par une façade compatible avec le framebuffer.

## Probe compilée

Un environnement séparé `screen_esp_rgb_direct_probe` a été ajouté avec
`src_esp32/az2_rgb_direct_probe/main.cpp`. Il initialise directement
`esp_lcd`, alloue deux buffers PSRAM, enregistre `on_frame_buf_complete` et
alterne des aplats de couleur pour vérifier la synchronisation. La compilation
PlatformIO est validée le 20/09/2026 ; le probe n'est pas flashé sur la machine
principale tant que l'on n'a pas décidé de remplacer temporairement l'interface
AZ-Tracker.

## Test matériel du probe

Le probe a été flashé après passage en mode BOOT. Le port série a confirmé les
callbacks `on_frame_buf_complete` avec `frames=232, 274, 316, 359, 401` sur les
fenêtres successives, soit environ 42 callbacks/s. Les deux adresses sont
utilisées par l'alternance `write=0/1`. Le test valide donc l'allocation et la
rotation matérielle des deux buffers à PCLK 12 MHz. Il reste à restaurer le
firmware AZ-Tracker, puis à intégrer cette mécanique dans la façade graphique.

## Résultat Canvas sur panneau réel

Le probe `Arduino_Canvas` a bien affiché l'alternance des deux buffers, mais le
panneau réel a montré une image inversée avec des aplats vert/brun. Le problème
vient de la rotation 180° et de l'ordre des composantes appliqués par la façade,
pas de l'allocation double buffer. Le probe est conservé comme test de
synchronisation, sans être promu dans le firmware principal.

## Correction de rotation validée

Le probe Canvas corrigé applique la rotation 180 degrés avant la copie dans le
buffer RGB direct. Test matériel validé : texte « RGB x2 » à l'endroit, aplats
vert et brun alternés, et callbacks des deux buffers continus. Le pilote direct
est maintenant validé pour les primitives Canvas de base à 12 MHz.

## Mesure de copie Canvas complète

Le probe instrumenté donne `flush_us=30660..36123` us pour une copie complète
480x480 avec rotation 180 degrés. Cette voie est trop lente pour rafraîchir une
image complète à 60 Hz. Le rendu GB devra donc conserver la copie par bandes et
appeler la façade uniquement sur les lignes modifiées ; le double framebuffer
reste utile pour supprimer le déchirement, pas pour accélérer une copie totale.

## Mesure par bande validée

Le probe matériel donne `band_us=1573..1597` us pour une bande 480x24 avec
rotation et invalidation cache limitée. La copie plein écran reste à
`flush_us=30482..33626` us. Le chemin par bande est donc environ 20 fois moins
coûteux et correspond à la granularité nécessaire au rendu GB X3.

## Optimisation de copie mesurée

Le probe a ensuite remplacé les écritures PSRAM 16 bits pixel par pixel par des
écritures groupées 32 bits lorsque l'adresse est alignée, avec un repli sûr pour
les coordonnées impaires. Sur la même carte, la copie plein écran est passée à
`flush_us=23299..25469` us et la bande à `band_us=1140..1380` us. Le gain est
d'environ 25 % sur le plein écran et 15 % sur la bande. Le commit de référence
est `f91e72a`.

Une variante 64 bits alignée a ensuite été testée sur le probe. Elle donne
`flush_us=22044..23623` us et `band_us=1100..1351` us sur plusieurs cycles,
sans erreur série ni redémarrage. Le gain supplémentaire est modeste mais la
voie reste compatible avec le repli 32 bits pour les adresses non alignées.

## Balayage PCLK

Le probe 16 MHz est resté stable pendant plusieurs secondes, avec des copies
à `23..26 ms` et des bandes autour de `1,33 ms`. À 18 MHz, aucune erreur série
ni redémarrage n'est apparu, mais la copie est montée à `25..29 ms` et reste
moins régulière. Le réglage 16 MHz est conservé comme meilleur compromis à ce
stade ; le firmware principal reste à 12 MHz tant que l'intégration n'est pas
validée visuellement.
