#!/bin/bash
set -euo pipefail
test "$(id -u)" = 0 || exit 1
root=$(realpath "${1:?Clean image root required}")
identity=$(realpath "${2:?Identity source directory required}")
case "$root" in */iso-build/rootfs) ;; *) echo 'Expected the dedicated ISO root filesystem.' >&2; exit 2;; esac
test -f "$root/etc/debian_version"
test -f "$identity/os-release"
install -m 0644 "$identity/os-release" "$root/usr/lib/os-release"
ln -sfn ../usr/lib/os-release "$root/etc/os-release"
install -m 0644 "$identity/lsb-release" "$root/etc/lsb-release"
install -m 0644 "$identity/issue" "$root/etc/issue"
printf 'Sovel 0.1.0\n' > "$root/etc/issue.net"
install -d "$root/etc/default/grub.d"
install -m 0644 "$identity/grub.cfg" "$root/etc/default/grub.d/90-sovel.cfg"
# Disable vendor promotional text in console logins in the clean image.
for entry in 10-help-text 50-motd-news 91-contract-ua-esm-status 91-release-upgrade; do
    if test -f "$root/etc/update-motd.d/$entry"; then chmod -x "$root/etc/update-motd.d/$entry"; fi
done
install -d "$root/var/crash"
install -d "$root/usr/share/plymouth/themes/sovel" "$root/etc/plymouth"
cp -a "$identity/plymouth/sovel/." "$root/usr/share/plymouth/themes/sovel/"
chown -R 0:0 "$root/usr/share/plymouth/themes/sovel"
printf '[Daemon]\nTheme=sovel\nShowDelay=0\n' > "$root/etc/plymouth/plymouthd.conf"
# The initramfs hook follows this alternative when deciding which theme and
# Plymouth binaries to include; the daemon configuration alone is insufficient.
chroot "$root" update-alternatives --install /usr/share/plymouth/themes/default.plymouth default.plymouth /usr/share/plymouth/themes/sovel/sovel.plymouth 200
chroot "$root" update-alternatives --set default.plymouth /usr/share/plymouth/themes/sovel/sovel.plymouth
install -m 0644 "$identity/plymouth/sovel/mark.png" "$root/usr/share/pixmaps/ubuntu-logo-text-dark.png"
# These optional Casper hooks assume vendor desktop packages which Sovel omits.
# Leave user creation, networking, storage, authentication and boot hooks intact.
hooks="$root/usr/share/initramfs-tools/scripts/casper-bottom"
for hook in 52gnome_initial_setup 59disable_mozc_autosetup; do
    test ! -f "$hooks/$hook" || chmod -x "$hooks/$hook"
done
if test ! -d "$root/usr/lib/update-notifier" && test ! -d "$root/usr/lib/ubuntu-release-upgrader"; then
    test ! -f "$hooks/31disable_update_notifier" || chmod -x "$hooks/31disable_update_notifier"
fi
if test ! -f "$root/etc/default/apport"; then
    test ! -f "$hooks/33enable_apport_crashes" || chmod -x "$hooks/33enable_apport_crashes"
fi
if test ! -f "$root/etc/apt/apt.conf.d/50unattended-upgrades"; then
    test ! -f "$hooks/53disable_unattended_upgrades" || chmod -x "$hooks/53disable_unattended_upgrades"
fi
echo 'Sovel identity applied to image root; kernel must still be built with the Sovel configuration.'
