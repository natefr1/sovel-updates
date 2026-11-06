#!/bin/sh
# Exercise launcher environment without taking a graphics seat.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/bin"
cat > "$tmp/bin/labwc" <<'EOF'
#!/bin/sh
printf '%s\n' "$WLR_RENDERER" "$QT_QUICK_BACKEND" "$XDG_CURRENT_DESKTOP" "$@"
EOF
chmod +x "$tmp/bin/labwc"
export PATH="$tmp/bin:$PATH"
export XDG_RUNTIME_DIR="$tmp"
export XDG_CONFIG_HOME="$tmp/config"
export DBUS_SESSION_BUS_ADDRESS='unix:path=/test-only'
result=$(sh packaging/sovel-session --software)
configuration=/usr/share/sovel/labwc
if test -r "$XDG_CONFIG_HOME/Sovel/labwc/rc.xml"; then configuration="$XDG_CONFIG_HOME/Sovel/labwc"; fi
expected=$(printf '%s\n' pixman software Sovel -C "$configuration")
test "$result" = "$expected"
if sh packaging/sovel-session --invalid >/dev/null 2>&1; then
    echo 'Invalid option was accepted' >&2; exit 1
fi
if sh packaging/sovel-session --software extra >/dev/null 2>&1; then
    echo 'Extra argument was accepted' >&2; exit 1
fi
unset XDG_RUNTIME_DIR
if sh packaging/sovel-session --software >/dev/null 2>&1; then
    echo 'Missing login runtime was accepted' >&2; exit 1
fi
echo 'Session startup contract checks passed'
