#!/bin/bash
set -euo pipefail
test "$(id -u)" = 0 || exit 1
build_dir=$(realpath "${1:?Image build directory required}")
source_dir=$(realpath "${2:?Sovel source directory required}")
package_path=$(realpath "${3:?Built public desktop package required}")
case "$build_dir" in */iso-build) ;; *) exit 2;; esac
test -f "$build_dir/runtime.complete"
root="$build_dir/rootfs"
test -x "$source_dir/build/sovel"
test -x "$root/usr/bin/calamares"
for point in dev proc sys; do
    mountpoint -q "$root/$point" && { echo 'Runtime stage must unmount its temporary filesystems first.' >&2; exit 3; }
done
install -d "$root/usr/bin" "$root/usr/libexec" "$root/usr/share/sovel" "$root/etc/sovel" "$root/etc/greetd"
test -f "$package_path"
test "$(dpkg-deb -f "$package_path" Package)" = sovel-desktop
install -m 0644 "$package_path" "$root/tmp/sovel-desktop.deb"
chroot "$root" dpkg --install /tmp/sovel-desktop.deb
rm -f "$root/tmp/sovel-desktop.deb"
rm -f "$root/run/sovel-desktop-restart-required"
# Retire only the earlier unmanaged Sovel files inside this dedicated image.
for program in sovel sovel-session sovel-autostart sovel-lock sovel-greeter sovel-setup; do rm -f "$root/usr/local/bin/$program"; done
rm -f "$root/usr/local/libexec/sovel-setup-display" "$root/usr/local/libexec/sovel-finalize-install"
rm -rf -- "$root/usr/local/share/sovel" "$root/usr/local/lib/sovel"
cp -a "$source_dir/installer/calamares" "$root/etc/"
chown -R 0:0 "$root/etc/calamares"
brand="$root/etc/calamares/branding/sovel"
install -d "$brand/radios"
for component in WifiPanel BluetoothPanel ConnectionGlyph; do
    install -m 0644 "$source_dir/qml/$component.qml" "$brand/radios/"
done
printf 'import ".."\nSetupButton {}\n' > "$brand/radios/SovelButton.qml"
install -m 0644 "$source_dir/assets/landscape.png" "$brand/setup-wallpaper.png"
install -m 0644 "$brand/sovel.svg" "$root/usr/share/icons/hicolor/scalable/apps/sovel-setup.svg"
# QML variants use the same validated backend configuration as their widget peers.
for module in welcome locale keyboard users finished; do
    cp "$root/etc/calamares/modules/$module.conf" "$root/etc/calamares/modules/${module}q.conf"
    printf '\nqmlSearch: branding\nqmlFilename: %sq\n' "$module" >> "$root/etc/calamares/modules/${module}q.conf"
done
install -d "$root/etc/NetworkManager/conf.d"
cat > "$root/etc/NetworkManager/conf.d/90-sovel-managed.conf" <<'CONF'
[ifupdown]
managed=true
[keyfile]
unmanaged-devices=
CONF
# Use the installed resolver, rather than copying the development VM's DNS file.
test -x "$root/usr/lib/systemd/systemd-resolved"
ln -sfn ../run/systemd/resolve/stub-resolv.conf "$root/etc/resolv.conf"
install -m 0755 "$source_dir/installer/finalize-install.sh" "$root/usr/libexec/sovel-finalize-install"
bash "$source_dir/installer/apply-identity.sh" "$root" "$source_dir/installer/identity"
chroot "$root" getent passwd sovel-greeter >/dev/null || chroot "$root" useradd --system --create-home --home-dir /var/lib/sovel-greeter --shell /usr/sbin/nologin sovel-greeter
cat > "$root/usr/share/sovel/greetd-installed.toml" <<'CONF'
[terminal]
# Match the packaged greetd service's getty@tty7 conflict.
vt = 7
[default_session]
command = "/usr/bin/sovel-greeter"
user = "sovel-greeter"
CONF
cp "$root/usr/share/sovel/greetd-installed.toml" "$root/etc/greetd/config.toml"
cat >> "$root/etc/greetd/config.toml" <<'CONF'
[initial_session]
command = "/usr/bin/sovel-session"
user = "sovel"
CONF
cat > "$root/etc/casper.conf" <<'CONF'
USERNAME=sovel
USERFULLNAME="Sovel Live"
HOST=sovel-live
# Casper's internal compatibility selector; visible product identity is Sovel.
BUILD_SYSTEM=Ubuntu
CONF
# Casper temporarily filters sources.list.d while making its offline live cache.
# Keep network repositories there so early boot doesn't attempt an online update.
truncate -s 0 "$root/etc/apt/sources.list"
cat > "$root/etc/apt/sources.list.d/sovel.sources" <<'SOURCES'
Types: deb
URIs: https://archive.ubuntu.com/ubuntu
Suites: resolute resolute-updates
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://security.ubuntu.com/ubuntu
Suites: resolute-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SOURCES
install -m 0644 "$source_dir/installer/updates/sovel-public.sources" "$root/etc/apt/sources.list.d/"
install -m 0644 "$source_dir/installer/updates/sovel-updates-archive-keyring.gpg" "$root/usr/share/keyrings/"
cat > "$root/etc/apt/preferences.d/sovel-public" <<'PREFERENCES'
Package: sovel-*
Pin: release o=Sovel
Pin-Priority: 700
PREFERENCES
printf 'public\n' > "$root/etc/sovel/live-image"
printf 'auto\n' > "$root/etc/sovel/rendering"
install -d "$root/etc/sudoers.d"
printf 'sovel ALL=(root) NOPASSWD:SETENV: /usr/bin/calamares\n' > "$root/etc/sudoers.d/sovel-live"
chmod 0440 "$root/etc/sudoers.d/sovel-live"
cat > "$root/usr/share/applications/sovel-install.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Sovel Setup Assistant
Comment=Set up Sovel on this computer
Exec=/usr/bin/sovel-setup
Icon=sovel-setup
Terminal=false
Categories=System;
DESKTOP
chroot "$root" systemctl enable greetd.service NetworkManager.service bluetooth.service systemd-resolved.service systemd-timesyncd.service
chroot "$root" systemctl set-default graphical.target
chroot "$root" locale-gen en_US.UTF-8
printf 'LANG=en_US.UTF-8\n' > "$root/etc/default/locale"
chroot "$root" dpkg-query -W -f='${Package} ${Version}\n' > "$build_dir/packages.manifest"
# These files must be unique on the installed machine or generated at live boot.
truncate -s 0 "$root/etc/machine-id"
rm -f "$root/var/lib/dbus/machine-id" "$root/usr/sbin/policy-rc.d"
# Some packages create an empty root SSH directory. Never include credentials.
if test -d "$root/root/.ssh"; then rmdir "$root/root/.ssh"; fi
test ! -e "$root/home/nate"
test ! -e "$root/home/sovel"
chroot "$root" ldconfig
touch "$build_dir/configuration.complete"
echo 'Configured live Sovel and installer. Initramfs refresh, ISO creation, and installation tests remain.'
