#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build --parallel 2
echo 'Built build/sovel. Test the interface using: build/sovel --preview'
