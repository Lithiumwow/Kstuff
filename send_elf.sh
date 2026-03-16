#!/bin/bash
# Send an ELF payload to the PS5 over the network (port 9021).
# Usage:
#   ./send_elf.sh <PS5_IP> [path/to/payload.elf]
#   PS5_IP=192.168.1.100 ./send_elf.sh kstuff.elf
#
# If no ELF path is given, uses ps5-kstuff-ldr/kstuff.elf if it exists.

set -e
PS5_PORT="${PS5_PORT:-9021}"

if [ -n "$PS5_IP" ]; then
  IP="$PS5_IP"
  ELF="${1:-ps5-kstuff-ldr/kstuff.elf}"
else
  if [ -z "$1" ]; then
    echo "Usage: $0 <PS5_IP> [path/to/payload.elf]"
    echo "   or: PS5_IP=192.168.1.100 $0 [path/to/payload.elf]"
    exit 1
  fi
  IP="$1"
  ELF="${2:-ps5-kstuff-ldr/kstuff.elf}"
fi

if [ ! -f "$ELF" ]; then
  echo "ELF not found: $ELF"
  echo "Download the Payload artifact from CI (contains kstuff.elf) or pass a path."
  exit 1
fi

echo "Sending $ELF to $IP:$PS5_PORT ..."
nc "$IP" "$PS5_PORT" < "$ELF"
echo "Done."
