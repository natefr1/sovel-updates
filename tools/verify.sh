#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
for f in tools/*.sh packaging/sovel-session packaging/sovel-autostart packaging/sovel-lock packaging/sovel-greeter packaging/labwc/autostart; do sh -n "$f"; done
for f in packaging/sovel-mode packaging/sovel-private packaging/sovel-vault; do python3 -m py_compile "$f"; done
python3 -m py_compile tools/retire-desktop.py
sh tests/session-startup.sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
echo 'Inspect skipped IPC tests too: Unix sockets must work on the release target.'
