# AZ-2 — Guía del proyecto (español)

[🇫🇷 Français](README.fr.md) · [🇬🇧 English](README.en.md) · [Inicio en GitHub](../../README.md)

> **Prototipo alfa.** «Implementado» significa que la función existe en el código; que «la compilación CI sea correcta» no demuestra que funcione en el hardware real. Consulta el [registro de pruebas de hardware (en francés)](../AZ2_ETAT_DES_LIEUX.md) antes de considerar validada una función.

## Construir un instrumento

AZ-2 combina una groovebox, un tracker de ocho pistas y nueve motores de audio. El repositorio contiene prototipos de Game Boy / Game Boy Color, pero **actualmente no hay ningún emulador funcional y validado en la máquina**. El prototipo activo utiliza Teensy, el ESP32-S3 de pantalla, un ESP32-S3 granular y un ESP-WROOM-32D espectral.

| Placa | Funciones | Almacenamiento |
| --- | --- | --- |
| Teensy 4.1 | Temporización del tracker, motores de audio, MIDI, grabación WAV, reproducción de samples y salida I²S al DAC PCM5102A | Su propia tarjeta SD para grabaciones; PSRAM para el sample dinámico |
| ESP32-S3 VIEWE UEDX48480040E-WB | Pantalla táctil 480×480, menús y prototipos GB/GBC sin validar | Su propia tarjeta SD para datos de interfaz y pruebas de ROM |
| ESP32-S3 N16R8 | GRANULAR, PSRAM y agregación de audio | Samples transferidos desde la SD del Teensy |
| ESP-WROOM-32D | SPECTRAL | No necesita almacenamiento local |

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

Compila los dos binarios desde el **mismo commit de Git**. No actualices una sola placa después de un cambio incompatible del protocolo. El repositorio no incluye ROMs comerciales ni archivos privados del usuario.

## Emulación, partidas y música

- **GB/GBC:** Walnut-CGB y GNUBOY son prototipos de desarrollo. Actualmente ningún núcleo se considera funcional en AZ-2; carga, vídeo, controles, audio y guardado deben validarse juntos antes de anunciar compatibilidad.
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

La información técnica actual está en el [estado verificado (francés)](../AZ2_ETAT_ACTUEL.md), la [hoja de ruta GB/LSDJ](../AZ2_GB_ROADMAP_IMPLEMENTATION.md), la [documentación del sampler](../AZ2_SAMPLEUR.md) y el [manual](../AZ2_MANUEL_UTILISATEUR.md). Los documentos antiguos pueden describir hardware obsoleto.

## Trabajo pendiente

Validar guardados y RTC frente a cortes eléctricos, medir FPS/audio reales en el prototipo, comprobar juegos y LSDJ con resultados reproducibles, mantener el estéreo hasta el DAC sin agotar la memoria de audio del Teensy y desarrollar navegación, edición y asignación de varias capturas. Que la CI sea correcta no significa que estas tareas estén terminadas.

## Contribuciones y licencias

Un informe de error útil incluye el SHA del commit, la placa, pasos de reproducción, registros y ROMs de prueba de distribución libre. No publiques ROMs comerciales, partidas personales ni secretos. Consulta [CONTRIBUTING.md](../../CONTRIBUTING.md), [LICENSE](../../LICENSE) y [licencias de terceros (francés)](../AZ2_LICENCES.md).

## Salvapantallas — estilos y controles

En **CONFIGURATION**, elige **Matrix** (caracteres clásicos), **Lluvia de notas** (notas de los pasos del patrón actual), **8 pistas** (una columna por pista del tracker) o **Dashboard** (BPM, patrón, paso, estado PLAY/STOP, motores y notas de las ocho pistas). Los eventos proceden del estado del tracker que ya conserva el ESP32, sin añadir tráfico UART a la animación.

**Cruceta:** ARRIBA/ABAJO selecciona la fila; IZQUIERDA/DERECHA cambia el estilo, el tiempo de espera, la escala o el swing. **A:** confirma pasando al siguiente valor. **Pantalla táctil:** toca las flechas de la misma fila para realizar la misma operación. Un botón despierta la pantalla. El estilo y el tiempo de espera se guardan en la memoria NVS del ESP32; un tiempo de cero desactiva el salvapantallas.

El salvapantallas no detiene el tracker ni el audio. Queda pendiente comprobar en la pantalla real la fluidez, la legibilidad y la ausencia de interferencias con Game Boy.
