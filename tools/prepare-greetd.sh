#!/bin/sh
set -eu
test "$(id -u)" = 0 || { echo 'Run with sudo.' >&2; exit 1; }
test -x /usr/bin/sovel || { echo 'Install the compiled Sovel first.' >&2; exit 1; }
test -x /usr/bin/sovel-session
test -x /usr/bin/sovel-greeter
command -v cage >/dev/null
command -v greetd >/dev/null
command -v labwc >/dev/null
getent passwd sovel-greeter >/dev/null || useradd --system --create-home --home-dir /var/lib/sovel-greeter --shell /usr/sbin/nologin sovel-greeter
install -d -m 0755 /etc/greetd
if test -e /etc/greetd/config.toml; then
  cp -a /etc/greetd/config.toml "/etc/greetd/config.toml.before-sovel-$(date +%s)"
fi
cat > /etc/greetd/config.toml <<'CONF'
[terminal]
vt = 1

[default_session]
command = "/usr/bin/sovel-greeter"
user = "sovel-greeter"
CONF
echo 'Prepared the greeter. No services were stopped or switched.'
echo 'After console testing, activate for next boot with:'
echo '  sudo systemctl disable gdm3.service'
echo '  sudo systemctl enable --force greetd.service'
echo 'Save your work before rebooting. Authentication needs a real VT test.'
