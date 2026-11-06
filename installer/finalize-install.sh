#!/bin/bash
set -euo pipefail
# Calamares runs this inside the mounted target, never against the live session.
test "$(id -u)" = 0
systemd-detect-virt --quiet --chroot || { echo 'Refusing to finalize outside the installation target chroot.' >&2; exit 2; }
test -f /etc/sovel/live-image
test -f /usr/share/sovel/greetd-installed.toml
install -m 0644 /usr/share/sovel/greetd-installed.toml /etc/greetd/config.toml
# Preserve the graphics fallback selected in the live boot menu for first reboot.
test -r /proc/cmdline || { echo 'The target /proc filesystem must be mounted before finalizing.' >&2; exit 4; }
for option in $(cat /proc/cmdline); do
    case "$option" in
        sovel.renderer=auto|sovel.renderer=software|sovel.renderer=software-gl)
            printf '%s\n' "${option#sovel.renderer=}" > /etc/sovel/rendering ;;
    esac
done
rm -f /etc/sudoers.d/sovel-live /usr/share/applications/sovel-install.desktop /usr/sbin/policy-rc.d
# The squashfs has no live user's account or home; Casper creates it only in RAM.
test ! -d /home/sovel || { echo 'Unexpected live account in target image.' >&2; exit 3; }
dpkg --remove casper calamares calamares-data
rm -f /etc/sovel/live-image
systemctl set-default graphical.target
echo 'Installed Sovel now requires the account created in Setup.'
