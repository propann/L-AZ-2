#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-/dev/ttyUSB0}"
IMAGE="$ROOT/.pio/build/screen_esp/firmware.factory.bin"

if [[ ! -f "$IMAGE" ]]; then
  echo "Binaire absent : compile d'abord avec : pio run -e screen_esp" >&2
  exit 2
fi

echo "Flash rapide de $IMAGE sur $PORT"
echo "Si besoin, maintiens BOOT pendant la connexion de la carte."

exec esptool --chip esp32s3 --port "$PORT" --baud 921600 \
  write-flash --flash-size keep 0x0 "$IMAGE"
