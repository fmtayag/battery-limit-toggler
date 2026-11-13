#!/bin/sh
# Build dist/main (requires libx11-dev)
set -e
cd "$(dirname "$0")"
mkdir -p dist
gcc -Wall -O2 -o dist/main src/*.c -lX11
echo "Built: $(pwd)/dist/main"
