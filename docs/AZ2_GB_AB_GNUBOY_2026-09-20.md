# AZ-2 — A/B Walnut-CGB / GNUBOY

Date : 20 septembre 2026

## Constat matériel

Sur l’ESP32-S3 et le panneau RGB 480×480, la version Walnut-CGB mesurée sur
`Legend of Zelda` prend environ 44 ms par frame en rendu 3×, puis environ
30 ms en rendu 2×. La génération et l’envoi audio prennent environ 1,9 ms.
Le goulet est donc le chemin cœur + framebuffer PSRAM, pas le transport audio.

## Ce que fournit GNUBOY Retro-Go

Le dépôt Retro-Go officiel contient un composant `gnuboy` autonome d’environ
5 000 lignes C. Son contrat est adapté à une intégration contrôlée :

- `gnuboy_run(bool draw)` exécute une frame et permet de sauter le dessin sans
  sauter la logique ni l’audio ;
- `gnuboy_set_framebuffer()` reçoit un framebuffer complet ;
- `gnuboy_set_soundbuffer()` sépare le tampon audio ;
- le callback vidéo est appelé une fois par frame, après la génération de
  l’image, au lieu d’être appelé pour chaque ligne LCD.

La source se compile avec le compilateur Xtensa ESP32-S3 utilisé par
PlatformIO. Elle est sous **GPLv2** (`retro-core/components/gnuboy/COPYING`).
Elle ne doit donc pas être copiée dans le firmware audio Teensy GPLv3. Le
firmware écran et le firmware audio sont deux binaires séparés : la piste
possible est un firmware écran explicitement GPLv2 (si les auteurs du code
écran l'autorisent), avec les notices GNUBOY conservées, tandis que le
firmware Teensy reste GPLv3. Le dépôt global ne sera pas abaissé en GPLv2,
car `Synth_MDA_EPiano` est GPLv3 uniquement.

## Premier test matériel

Le probe `screen_esp_gnuboy_probe` a été compilé puis flashé sur l'ESP32-S3.
Avec `tobu.gb` trouvé sur la carte SD, rendu 2× et sans audio (mesure CPU/vidéo
isolée), la cadence stable observée est de **35–36 FPS**, pour environ
**28,5 ms par frame**. La mémoire libre reste à environ **328 Ko SRAM** et
**7 155 Ko PSRAM**. Walnut-CGB mesurait environ 30 ms par frame en 2× avec
audio actif : le gain GNUBOY est réel mais limité, et le test n'est pas encore
comparable fonctionnellement tant que le callback audio n'est pas raccordé.

Le callback audio a ensuite été raccordé au paquet PCM V1 du Teensy. La
mesure passe à **32–33 FPS**, environ **31 ms par frame**, avec environ
**30 paquets audio par seconde** et 234 échantillons par paquet. Le coût
supplémentaire est donc proche de 2,5 ms par frame, comparable au chemin
Walnut-CGB. Le probe confirme ainsi que GNUBOY peut remplacer le cœur sans
aggraver la charge, mais il reste à valider les contrôles, les sauvegardes et
les ROM CGB avant une intégration dans `az2_screen`.

## Décision d’intégration

Ne pas remplacer Walnut-CGB à l’aveugle. Le prochain lot est un backend A/B
isolé : même navigateur ROM, mêmes contrôles, mêmes sauvegardes et même pont
audio, avec un choix de cœur à la compilation. Le résultat sera accepté si
GNUBOY apporte une cadence mesurée supérieure sans régression sur les ROMs
DMG/CGB et les sauvegardes.

Le premier port doit garder le cœur GNUBOY hors du chemin par défaut tant que
son adaptation PSRAM, son audio et sa licence ne sont pas validés. Le code
Retro-Go complet (launcher, ESP-IDF et pilotes d’écran) ne sera pas importé :
seul le composant cœur et l’adaptateur nécessaire seront étudiés.
