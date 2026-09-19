# AZ-2 — Guía del proyecto (español)

[🇫🇷 Français](README.fr.md) · [🇬🇧 English](README.en.md) · [Inicio en GitHub](../../README.md)

> **Prototipo alfa.** «Implementado» significa que la función existe en el código; que «la compilación CI sea correcta» no demuestra que funcione en el hardware real. Consulta el [registro de pruebas de hardware (en francés)](../AZ2_ETAT_DES_LIEUX.md) antes de considerar validada una función.

## Construir un instrumento

AZ-2 combina una groovebox, un tracker de ocho pistas, seis motores de síntesis/reproducción y una consola Game Boy / Game Boy Color. El objetivo es jugar, componer, capturar el audio del juego y convertir una grabación en un instrumento. AZ-2 utiliza **dos placas**, sin multiplexor ni rack de ESP adicional; ese rack pertenece al proyecto independiente AZ-3.

| Placa | Funciones | Almacenamiento |
| --- | --- | --- |
| Teensy 4.1 | Temporización del tracker, motores de audio, MIDI, grabación WAV, reproducción de samples y salida I²S al DAC PCM5102A | Su propia tarjeta SD para grabaciones; PSRAM para el sample dinámico |
| ESP32-S3 VIEWE UEDX48480040E-WB | Pantalla táctil 480×480, menús, emulación GB/GBC y explorador de ROMs | Su propia tarjeta SD para ROMs y partidas guardadas |

Los comandos y el audio Game Boy comparten un enlace **UART a 921600 baudios**. El contrato común se define en `lib/AZ2_Protocol/AZ2_Protocol.h`: **actualiza ambos firmwares y sus pruebas juntos**. La ruta de audio de producción sigue siendo **V1, PCM8 mono a 14 kHz**. El piloto V2 estéreo está implementado, pero desactivado.

## Instalación y actualización

Lee primero la [guía de cableado (en francés)](../AZ2_CABLAGE_MASTER.md) y la [guía de seguridad del primer arranque (en francés)](../AZ2_DEMARRAGE.md). Comprueba las tensiones, la masa común y las conexiones TX/RX con la alimentación desconectada. Haz una copia de seguridad de ambas tarjetas SD, especialmente de los archivos `.sav`, `.rtc`, ROMs y `.wav`, y conserva también la pareja anterior de binarios que funcionaba correctamente.

```bash
git clone https://github.com/propann/L-AZ-2.git
cd L-AZ-2
python -m pip install platformio==6.1.19
python tools/check_firmware_contract.py
pio test -e native
pio run -e master_teensy -e screen_esp
# Solo después de comprobar el hardware y guardar copias:
pio run -e master_teensy -t upload
pio run -e screen_esp -t upload
```

Compila los dos binarios desde el **mismo commit de Git**. No actualices una sola placa después de un cambio incompatible del protocolo. `ui_esp` es un entorno antiguo de pruebas iniciales, no el firmware de la pantalla definitiva. El repositorio no incluye ROMs comerciales ni archivos privados del usuario.

## Emulación, partidas y música

- **GB/GBC:** núcleo Walnut-CGB, selección de ROMs desde la SD del ESP32 y controles físicos. La compatibilidad, los FPS reales y la ausencia de fallos deben verificarse juego por juego, incluidos LSDJ, Tetris y Mario.
- **SRAM del cartucho:** guardado periódico y manual en `.sav/.bak`. Un error de escritura impide descargar el cartucho para no perder silenciosamente los cambios en RAM.
- **RTC MBC3:** archivo `.rtc` independiente y versionado, con CRC32 y recuperación desde `.bak`. El tiempo transcurrido con la máquina apagada solo se aplica cuando el reloj del sistema ESP32 es válido; queda pendiente probar la resistencia a cortes de alimentación en el hardware.
- **Audio GB:** V1 PCM8 mono a 14 kHz hacia Teensy. En modo experimental, V2 transporta PCM8 estéreo L/R entrelazado a 14 kHz con CRC16, números de secuencia y negociación; está **desactivado de forma predeterminada**. El bus de salida del Teensy sigue siendo mono incluso al usar V2.
- **Captura → sampler:** `REC:START` / `REC:STOP` generan un WAV en la SD del Teensy. El último WAV válido puede cargarse en una zona PSRAM de unos 30 segundos y reproducirse mediante `SAMPLER / GB Capture` (patch 2). Los patches 0/1 siguen siendo Kick/Snare. La reproducción respeta la frecuencia de muestreo original. El sistema también intenta recuperar la captura al arrancar. Todavía no existe un explorador de múltiples grabaciones.

## Trabajo entregado y trazabilidad — 19 de septiembre de 2026

| Área | Modificación implementada | Evidencia / límite |
| --- | --- | --- |
| Persistencia GB | RTC MBC3 independiente con CRC32, rotación de archivos temporales/respaldo y recuperación condicional del tiempo apagado | [Commit RTC](https://github.com/propann/L-AZ-2/commit/44d56dac643593e9f499b872050680185dd9274f); pendientes las pruebas físicas de RTC/cortes |
| Audio acoplado | Transporte V2 estéreo L/R; mezcla explícita a mono en el receptor Teensy; V1 de producción sin cambios | [Emisor](https://github.com/propann/L-AZ-2/commit/e08af9c180c42ed0407f55907dedbb00361cf593) · [Receptor](https://github.com/propann/L-AZ-2/commit/9d0c8263c625264b2a9a2ba88eac7778c03a6a74); piloto sin activar |
| Flujo musical | Patch compartido GB Capture, carga WAV → PSRAM, reproducción a la frecuencia original y recarga al arrancar | [Integración del sampler](https://github.com/propann/L-AZ-2/commit/2aa8fbfc99d2483be684f5009462c3f760410ee7); falta validar audio/SD en el dispositivo |
| Verificación | Prueba del patch dinámico, compilación de Teensy y ESP32-S3 y pruebas nativas del protocolo compartido sobre la misma revisión | [Ejecución CI correcta](https://github.com/propann/L-AZ-2/actions/runs/35455889760) para `2ea9e03285814f4ebcb6844b7758ccf5f931891d`; **no es una prueba física** |

La información técnica detallada está en la [hoja de ruta GB/LSDJ (francés)](../AZ2_GB_ROADMAP_IMPLEMENTATION.md), el [protocolo de audio V2 (francés)](../AZ2_PROTOCOL_AUDIO_V2.md), la [documentación del sampler (francés)](../AZ2_SAMPLEUR.md) y el [manual de usuario (francés)](../AZ2_MANUEL_UTILISATEUR.md). Algunos documentos antiguos de planificación mencionan hardware descartado. Para la configuración actual de dos placas, usa la guía de cableado vigente, `platformio.ini` y este resumen.

## Trabajo pendiente

Validar guardados y RTC frente a cortes eléctricos, medir FPS/audio reales en el prototipo, comprobar juegos y LSDJ con resultados reproducibles, mantener el estéreo hasta el DAC sin agotar la memoria de audio del Teensy y desarrollar navegación, edición y asignación de varias capturas. Que la CI sea correcta no significa que estas tareas estén terminadas.

## Contribuciones y licencias

Un informe de error útil incluye el SHA del commit, la placa, pasos de reproducción, registros y ROMs de prueba de distribución libre. No publiques ROMs comerciales, partidas personales ni secretos. Consulta [CONTRIBUTING.md](../../CONTRIBUTING.md), [LICENSE](../../LICENSE) y [licencias de terceros (francés)](../AZ2_LICENCES.md).
