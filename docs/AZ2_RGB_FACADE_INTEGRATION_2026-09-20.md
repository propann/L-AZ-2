# Intégration de la façade RGB directe

## Appels graphiques recensés

L'interface AZ-Tracker utilise quinze opérations Arduino_GFX : texte
(`setCursor`, `print`, `printf`, `setTextColor`, `setTextSize`), rectangles,
`fillScreen`, lignes, pixels, bitmap et `getFramebuffer`. Le rendu GB n'utilise
que `getFramebuffer`, la copie RGB565 et l'invalidation de cache.

## Découpage retenu

1. Garder Arduino_GFX pour le texte et les pages de configuration.
2. Introduire une façade de framebuffer qui expose le buffer libre au rendu GB.
3. Pendant une frame GB, écrire toutes les bandes dans le même buffer libre.
4. Publier ce buffer après `on_frame_buf_complete`.
5. Ne basculer les pages UI qu'après duplication complète du framebuffer, afin
   qu'elles ne réapparaissent pas avec un ancien buffer.

La façade `lib/AZ2_RGB_Direct` est maintenant réutilisable et validée par le
probe matériel. La prochaine modification de `main.cpp` devra remplacer la
création unique du panneau Arduino_GFX ; aucune seconde initialisation RGB ne
doit être faite sur les mêmes GPIO.
