# AZ-2 — Audit du pilote RGB et pistes de performance

## Mesures de référence

- ESP32-S3, framebuffer RGB565 en PSRAM, écran 480×480.
- Walnut-CGB avec audio actif et mise à l’échelle GB 3×.
- Le chemin de copie directe du framebuffer a réduit `display_avg_us` d’environ
  24 ms à 19 ms.
- Le réglage 12 lignes par bande a scintillé ; le réglage stable reste à 8
  lignes par bande.
- Les pics de frame restent autour de 41–43 ms : le débit moyen est amélioré,
  mais la latence n’est pas encore régulière.

## Paramètres du pilote actuellement utilisés

Dans `src_esp32/az2_screen/main.cpp` :

- `pclk = 12 MHz` ; le test 16 MHz n’a pas apporté de gain visible.
- framebuffer en PSRAM (`fb_in_psram = true` dans Arduino_GFX).
- un seul framebuffer (`num_fbs = 1`).
- `bounce_buffer_size_px = 0`.
- `refresh_on_demand = false`.
- rotation logicielle 180° contournée pour le rendu GB par copie directe et
  invalidation ciblée du cache.

## Pistes classées

### 1. Bounce buffer RGB — prochain test

Espressif recommande un bounce buffer de plusieurs lignes, typiquement autour
de `10 * largeur`, pour améliorer le transfert RGB depuis la PSRAM. Pour
480×480, `4800` pixels représentent environ 9,6 Ko de RAM interne. Cela peut
réduire les pics de lecture PSRAM et lisser le flux vers le panneau. Test à
faire seul, avec retour immédiat à `0` si l’image scintille ou si l’audio
change.

### 2. Double framebuffer — à réserver à un test séparé

Deux framebuffers peuvent réduire le tearing si l’échange est synchronisé sur
VSYNC, mais coûtent environ 900 Ko de PSRAM supplémentaires en RGB565 480×480.
Il ne faut pas l’activer en même temps que le bounce buffer : le résultat serait
difficile à attribuer et l’échange actuel n’est pas encore synchronisé VSYNC.

### 3. `refresh_on_demand` — non prioritaire

Le mode de rafraîchissement à la demande pourrait éviter des scans inutiles,
mais il faut appeler explicitement l’API de refresh du pilote. Sans synchroni-
sation VSYNC, il risque de produire un écran incomplet ou des flashs. À tester
seulement après une voie de synchronisation propre.

### 4. Timing PCLK — déjà exploré

16 MHz n’a pas amélioré les mesures et peut réduire la marge de stabilité du
panneau. Garder 12 MHz jusqu’à ce qu’un test de timing complet mesure la
fréquence réelle, le blanking et les erreurs visuelles.

### 5. Rotation — déjà traitée côté GB

La rotation générique Arduino_GFX ajoutait une copie coûteuse. Le chemin GB
écrit maintenant directement la zone inversée du framebuffer et vide le cache
sur cette zone uniquement. Les autres pages gardent l’API graphique normale.

## Ordre des essais

1. Revenir à la base stable 8 lignes et vérifier l’image.
2. Tester uniquement `bounce_buffer_size_px = 4800`.
3. Mesurer `display_avg_us`, `frame_us_max`, `fps` et les sauts visibles.
4. Si le résultat est mauvais, revenir à zéro et conserver le chemin direct.
5. Étudier ensuite une synchronisation VSYNC avant tout double framebuffer.

## Résultat du profilage du cœur

Les maxima détaillés ont séparé le coût du cœur, de l’affichage et de l’audio.
L’affichage reste stable autour de 19 ms et l’audio autour de 2 ms ; les pics
venaient du cœur. Les lecteurs ROM 8/16/32 bits ont été placés en IRAM : les
fenêtres mesurées passent alors ponctuellement de 39–40 ms à 31–33 ms et le
débit monte jusqu’à 41–49 FPS selon la scène. Les pics ne disparaissent pas
dans tous les écrans, mais cette optimisation est conservée.

Les callbacks SRAM de la cartouche (`cartRamRead` et `cartRamWrite`) sont aussi
placés en IRAM. Sur une scène stable de Zelda, les mesures récentes donnent
`core_avg_us` autour de 20,9 ms, `core_max_us` autour de 36–37 ms et 35–38 FPS,
avec l'affichage stable autour de 19,2 ms. Cette optimisation est conservée.

Un essai de rendu de chaque frame (`frame_skip=false`) a été fait pour vérifier
si le saut d'une frame venait du cœur. Le cœur monte alors à 35,8 ms et le jeu
tombe à environ 21,7 FPS, alors que l'affichage reste à 19,2 ms. Le mode
`frame_skip=true` est donc rétabli : la logique et l'audio restent à cadence
complète, avec un rendu LCD autour de 30 Hz plus fluide sur ce panneau.

Le profilage détaillé a ensuite isolé le coût du chemin direct : environ
1,5 ms pour l'expansion, 11,5 ms pour les écritures PSRAM pixel par pixel et
7,7 ms pour le writeback du cache par bandes. La copie horizontale est
maintenant préparée dans le tampon interne puis envoyée par `memcpy` contigu.
Sur Zelda, les écritures descendent à 8,6–8,8 ms, l'affichage à 16,4 ms et le
cœur à 19,3–20,0 ms ; la cadence monte à 37–40 FPS. Cette optimisation est
conservée. Le coût restant principal est le writeback cache, autour de 7,6 ms.

Le writeback a été migré vers `esp_cache_msync()` comme dans le pilote RGB
ESP-IDF officiel. Retirer le flag `UNALIGNED` n'a pas changé le coût (~7,7 ms),
et ajouter `INVALIDATE` a provoqué davantage de scintillement avec le panneau
en rafraîchissement continu. La version stable est donc writeback seul, sans
invalidation.

Un essai du compilateur `-O2` (avec `-Os` réellement retiré via
`build_unflags`) a augmenté le code de 377 à 413 KiB, mais a dégradé Zelda à
32–34 FPS avec un cœur autour de 22,5 ms. Le réglage Arduino `-Os` est donc
rétabli et conservé.

Un test de plafond en échelle 2× (320×288 centrés) a atteint 59,7–59,8 FPS
avec zéro image manquée, `core_avg_us` autour de 12,2 ms et affichage autour
de 7,5 ms. Le 3× plein écran est rétabli par défaut ; le 2× est validé comme
futur mode performance, car il confirme que la limite restante vient du débit
du framebuffer 480×432 et non du CPU d'émulation.

Un essai X3 avec une seule synchronisation cache en fin d'image a réduit
`blit_flush_us` de 7,7 ms à 0,5–0,6 ms et `display_avg_us` d'environ 16,5 ms à
14,2 ms. La cadence observée est montée de 32–34 à 39–43 FPS. Le coût de copie
augmente toutefois à environ 14–15 ms ; l'essai reste donc à valider
visuellement avant de devenir la base permanente.

Après retrait de cet essai scintillant, la combinaison stable (mise à l'échelle
séquentielle + flush par bande) a mesuré 53,0–53,6 FPS sur une séquence Zelda
peu chargée, avec `core_avg_us` 15,4 ms, `display_avg_us` 16,4 ms,
`blit_scale_us` 1,5 ms et 4 images manquées. Une scène de jeu active doit encore
confirmer cette cadence avant de la considérer comme le nouveau niveau X3.

## Sources techniques

- ESP-IDF RGB panel :
  https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
- Guide Espressif :
  https://documentation.espressif.com/api/resource/path/docs/projects/esp-iot-solution/en/latest/esp-iot-solution-en-master.pdf
- Arduino_GFX, pilote RGB ESP32-S3 :
  https://github.com/moononournation/Arduino_GFX
Essai suivant : DMA Walnut-CGB 16 bits à la place du DMA 32 bits. Sur Zelda
actif, le résultat est resté à 31–33 FPS avec `core_avg_us` autour de 23,3 ms,
`blit_copy_us` autour de 8,7 ms et `blit_flush_us` autour de 7,7 ms. Aucun gain
mesurable ni différence visuelle utile ; le DMA 32 bits est rétabli.

## Pistes restantes avant changement de sujet

Les optimisations locales sûres sont épuisées. Les prochaines étapes sont des
expériences d'architecture à isoler :

1. double framebuffer PSRAM et bascule synchronisée sur VSYNC ;
2. tâche d'affichage séparée sur l'autre cœur de l'ESP32-S3 ;
3. transfert PSRAM par DMA dédié, avec attente explicite avant lecture par le
   panneau RGB ;
4. comparaison d'un pilote RGB direct avec Arduino_GFX pour réduire les coûts
   de copie et de synchronisation.

La base conservée avant ces essais est X3 avec mise à l'échelle séquentielle,
flush par bande, DMA Walnut 32 bits et frame skip activé.

Vérification du pilote : `Arduino_ESP32RGBPanel::getFrameBuffer()` configure
actuellement `num_fbs=1` et `double_fb=false`, sans méthode publique de bascule
VSYNC. Le double framebuffer nécessite donc un fork local d'Arduino_GFX ou un
passage direct à l'API ESP-IDF RGB ; aucune modification de ce type n'est
introduite dans la base stable.

## Essai PCLK 20 MHz — rejeté

Le PCLK RGB a été porté de 12 à 20 MHz et flashé sur l'ESP. Dès le démarrage,
l'interface complète bougeait et l'image de jeu était brouillée et mal alignée.
Le test est donc rejeté sans mesure FPS exploitable. PCLK rétabli à 12 MHz puis
firmware reflasché avec succès ; cette valeur reste la base stable.

## Décision pour la suite

La piste double framebuffer ne peut pas être activée par un simple réglage :
Arduino_GFX demande un seul buffer et n'expose pas de bascule VSYNC. Le prochain
prototype doit donc être un pilote RGB local basé sur `esp_lcd`, avec deux
buffers PSRAM et un échange synchronisé. Aucun changement expérimental n'est
laissé dans la base flashée tant que ce prototype n'est pas compilé et vérifié.

## Référence avant probe RGB direct — Zelda actif

Mesure relevée sur le firmware stable à 12 MHz pendant le jeu lancé :
`fps_x100=3118..3275` (31,18–32,75 FPS), `core_avg_us=22978..23945`,
`display_avg_us=16470..16515`, `blit_copy_us=8649..8810`,
`blit_flush_us=7725..7796`. Le compteur `missed` est resté à 16 après la
première fenêtre de mesure. Cette série sert de référence pour le probe direct.
