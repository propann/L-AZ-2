# AZ-2 — Étude de migration vers un émulateur Game Boy DMG uniquement

Date : 2026-09-24. Statut : étude et protocole de validation, **aucune migration de cœur ni performance matérielle validée**.

## Décision produit

Le mode JEUX de l'AZ-2 cible désormais la **Game Boy monochrome (DMG)**. Le support Game Boy Color (GBC/CGB), les ROM `.gbc`, les palettes CGB et le mode double vitesse CGB sortent du périmètre. Conserver les MBC nécessaires aux jeux DMG et à LSDJ, les sauvegardes SRAM et l'audio. Ne pas confondre « DMG-only » avec suppression de MBC5, de la SRAM ou des fonctions audio nécessaires à LSDJ.

Objectif : cadence logique Game Boy de ~59,7275 Hz, **sans frame-skip**, avec rendu visible, audio vers le Teensy, commandes et sauvegardes fiables. Une compilation réussie ou un compteur FPS moyen ne prouve pas la pleine vitesse.

## État du code à la date de l'étude

- Matériel écran identifié dans `platformio.ini` et `src_esp32/az2_screen/main.cpp` : module VIEWE UEDX48480040E-WB-V1.3, ESP32-S3 N16R8, GC9503V, écran RGB 480×480, tactile FT6336U, PSRAM 8 Mo. Le panneau est configuré à 12 MHz de pixel clock, avec `bounce_buffer_size_px=0`.
- `src_esp32/az2_screen/gb_emulator.cpp` utilise Walnut-CGB, charge la ROM en PSRAM, appelle **déjà** `gb_run_frame_dualfetch(&gb)` et configure **déjà** `gb.direct.frame_skip=false`. Ne pas présenter ces optimisations comme encore à implémenter.
- `gbBlitLine()` dans `main.cpp` agrandit chaque ligne 160 pixels en 480 pixels, réplique chaque ligne sur trois rangées, puis envoie des bandes de 8 lignes GB (24 lignes LCD) avec `draw16bitRGBBitmap`. Il faut mesurer la copie/PSRAM et l'interaction avec le framebuffer RGB, pas supposer que le cœur CPU est seul responsable.
- L'APU MiniGB génère l'audio puis le pont le transmet au Teensy. `gbRunFrame()` mesure un temps global comprenant exécution GB et envoi du paquet audio ; la mesure actuelle ne sépare pas CPU, PPU, LCD et UART.
- L'autosave SRAM/RTC synchrone périodique peut produire des pics de latence. Les anciennes études GB/GBC du dépôt sont historiques et ne constituent pas une mesure du firmware actuel.

## Trois variantes à comparer sans détruire le code existant

**A — Walnut-CGB actuel, référence.** Mesurer d'abord le firmware actuel sans modifier sa logique ; établir la même suite de ROMs et de scènes pour les trois variantes.

**B — Walnut conservé, mode DMG-only.** Refuser les ROM GBC-only dès le chargement et exclure `.gbc` du navigateur. Étudier les chemins CGB du cœur avant de les désactiver par compilation : vérifier les dépendances du PPU, du mapping mémoire et de l'initialisation. La suppression du support GBC ne garantit pas un gain CPU si les branches CGB ne sont jamais exécutées en mode DMG.

**C — Peanut-GB DMG pur.** Prototyper dans une branche distincte un adaptateur compatible avec l'interface AZ-2 : chargement ROM, MBC et RAM cartouche, joypad, lignes vidéo RGB565, registres APU, sauvegarde et télémétrie. Comparer la compatibilité de LSDJ et la charge avant de décider une migration. Vérifier la licence et les différences de fonctionnalités de la version amont choisie. Ne pas remplacer le cœur sur `main` avant validation.

Référence amont à étudier : https://github.com/deltabeard/Peanut-GB ; cœur actuel : https://github.com/Mr-PauI/Walnut-CGB.

## Profilage reproductible

1. Baseline « émulation sans affichage et sans audio » ; mesurer CPU/PPU et distribution des temps de frame (moyenne, maximum, p95/p99, frames >16 742 µs).
2. Affichage seul, puis émulation + LCD. Chronométrer conversion ×3, copies, transactions de bandes et éventuelles attentes du contrôleur RGB ; comparer un tampon 160×144 natif et un rendu agrandi, sans changer plusieurs variables à la fois.
3. Réactiver APU, envoi UART 921600, réception Teensy et DAC ; relever charge, dérive et underruns audio.
4. Réactiver commandes, SD et autosave ; observer les pics, notamment à la sauvegarde.
5. Répéter sur le vrai écran avec au moins Tetris DMG, Super Mario Land et LSDJ (version, scénario et configuration consignés). Enregistrer les résultats par ROM, sur 30 minutes, avec et sans capture audio. Conserver les logs et le hash du firmware.

Cible temporelle : une frame logique doit progresser selon l'horloge DMG ; ~16 742 µs est le budget moyen par frame, **pas** une garantie que chaque appel doit toujours durer exactement cette valeur. Distinguer temps de travail CPU et cadence réelle observée, frames affichées et audio sans coupure. Définir des seuils de tolérance sur mesures réelles avant d'annoncer « full speed ».

## Garde-fous de migration

- Sauvegarder et relire les SRAM existantes ; vérifier les formats et proposer une conversion explicite si les cœurs diffèrent.
- Refuser les ROMs GBC-only avec un message clair, sans les démarrer dans un mode incorrect.
- Vérifier que LSDJ et ses cartouches DMG/MBC restent pris en charge avant toute suppression de code CGB.
- Conserver la version actuelle sur une branche de référence et un mécanisme de retour arrière.
- Ne pas supprimer immédiatement `walnut_cgb/` ni réécrire l'audio, le LCD et le cœur en une seule PR.
- Ne publier aucun gain de performance chiffré sans mesure A/B sur le prototype physique.

## Ordre de réalisation

**Étape 1 :** instrumentation et baseline sur firmware existant. **Étape 2 :** filtrage DMG-only dans le navigateur/chargeur et tests de non-régression. **Étape 3 :** prototype Peanut-GB sur branche séparée et benchmarks identiques. **Étape 4 :** choisir le cœur sur résultats de cadence, compatibilité, mémoire et audio. **Étape 5 :** optimiser LCD/PSRAM/UART selon les temps mesurés ; fusionner seulement après tests matériel.
