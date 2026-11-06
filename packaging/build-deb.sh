#!/bin/bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/.." && pwd)
version=${1:?Provide the Debian package version}
output=$(realpath -m "${2:?Provide the package output directory}")
[[ "$version" =~ ^[0-9][0-9a-zA-Z.+~:-]*$ ]] || exit 2
mkdir -p "$output"
stage=$(mktemp -d "$output/.sovel-package.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT
DESTDIR="$stage" cmake --install "$source_dir/build" --prefix /usr
strip --strip-unneeded "$stage/usr/bin/sovel" "$stage/usr/lib/sovel/qml/Sovel/Platform/libsovelplatform.so"
mkdir -p "$stage/DEBIAN" "$stage/usr/share/doc/sovel-desktop"
mkdir -p "$stage/usr/share/sovel"
python3 - "$source_dir/assets/release.json" "$stage/usr/share/sovel/release.json" "$version" <<'PY'
import datetime,json,sys
from pathlib import Path
data=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'));data['version']=sys.argv[3];data['builtAt']=datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
Path(sys.argv[2]).write_text(json.dumps(data,ensure_ascii=False)+'\n',encoding='utf-8')
PY
release_info=$(base64 -w0 "$stage/usr/share/sovel/release.json")
printf 'Sovel Desktop %s\nPublic release channel.\n' "$version" > "$stage/usr/share/doc/sovel-desktop/release.txt"
mkdir -p "$stage/.shlib-work/debian"
printf 'Source: sovel-desktop\nSection: x11\nPriority: optional\nMaintainer: Sovel Release Team\n\nPackage: sovel-desktop\nArchitecture: any\nDescription: Sovel desktop\n' > "$stage/.shlib-work/debian/control"
cd "$stage/.shlib-work"
libraries=$(dpkg-shlibdeps --ignore-missing-info -O -e"$stage/usr/bin/sovel" -e"$stage/usr/lib/sovel/qml/Sovel/Platform/libsovelplatform.so" | sed -n 's/^shlibs:Depends=//p')
test -n "$libraries"
libraries="$libraries, libarchive-tools, wlr-randr, udisks2, whisper.cpp, curl, python3-venv, qml6-module-qtquick-pdf, qml6-module-qtmultimedia, qml6-module-qt-labs-folderlistmodel"
cd "$source_dir"
rm -rf -- "$stage/.shlib-work"
cat > "$stage/DEBIAN/control" <<EOF
Package: sovel-desktop
X-Sovel-Release-Info: $release_info
Version: $version
Architecture: amd64
Maintainer: Sovel Release Team
Section: x11
Priority: optional
Installed-Size: $(du -sk "$stage/usr" | cut -f1)
Depends: $libraries, sovel-wallpaper-tools (>= 2.9+mpv1.9-1), sovel-aic8800-dkms (>= 1.0.0+git2e75f888-1), labwc, cage, greetd, gtklock, foot, resources, packagekit, network-manager, network-manager-applet, bluez, blueman, lxqt-policykit, xwayland, python3, flatpak, python3-gi, gir1.2-flatpak-1.0, ca-certificates, qml6-module-qtquick-dialogs, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qml6-module-qtquick-effects, qt6-wayland, qt6-svg-plugins, qt6-image-formats-plugins, coreutils, libglib2.0-bin, xdg-utils, xdg-user-dirs, grim, slurp, wl-clipboard, libnotify-bin, mako-notifier, pipewire, pipewire-pulse, pulseaudio-utils, wireplumber, xdg-desktop-portal, xdg-desktop-portal-wlr, xdg-desktop-portal-gtk, fonts-noto-core, fonts-noto-extra, fonts-noto-cjk, fonts-noto-color-emoji, fcitx5, fcitx5-frontend-qt6, fcitx5-frontend-gtk3, fcitx5-frontend-gtk4, fcitx5-mozc, fcitx5-chinese-addons, fcitx5-hangul, fcitx5-m17n, fcitx5-config-qt, llama.cpp-tools (>= 8681), espeak-ng, swayidle, gocryptfs, fuse3, systemd-homed
Description: Sovel desktop, Settings and system integration
 The public Sovel desktop with signed system updates in Settings.
EOF
cat > "$stage/DEBIAN/postinst" <<'POSTINST'
#!/bin/sh
set -eu
if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true; fi
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database -q /usr/share/applications || true; fi
# Running sessions keep their current binaries until next sign-in.
if test "${1:-}" = configure && test -n "${2:-}"; then
    mkdir -p /run
    touch /run/sovel-desktop-restart-required
fi
if test "${1:-}" = configure; then
    /usr/libexec/sovel-record-install "$(dpkg-query -W -f='${Version}' sovel-desktop)"
fi
POSTINST
chmod 0755 "$stage/DEBIAN/postinst"
find "$stage" -type d -exec chmod 0755 {} +
dpkg-deb --root-owner-group --build "$stage" "$output/sovel-desktop_${version}_amd64.deb"
