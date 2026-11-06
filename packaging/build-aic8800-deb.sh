#!/bin/bash
# Package the matched driver, firmware and DKMS source for AX900 USB adapters.
set -euo pipefail
output=$(realpath -m "${1:?Output directory required}")
mkdir -p "$output"
commit=2e75f8885a053914c1b0328fdf4950f52691495d
checksum=279d0b6dd819507d1d4ca14d72f170c8267ecd3dd5caef9c4d4ce383427c4ee3
version=1.0.0+git2e75f888
archive="$output/aic8800-$commit.tar.gz"
if ! test -f "$archive"; then
    curl --fail --location --proto '=https' --tlsv1.2 \
        "https://codeload.github.com/shenmintao/aic8800d80/tar.gz/$commit" -o "$archive"
fi
printf '%s  %s\n' "$checksum" "$archive" | sha256sum -c -
stage=$(mktemp -d "$output/aic8800-package.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT
mkdir "$stage/source" "$stage/package"
tar -xzf "$archive" --strip-components=1 -C "$stage/source"
root="$stage/package"
source="$stage/source"
install -d "$root/DEBIAN" "$root/usr/src/sovel-aic8800-$version" \
    "$root/usr/lib/firmware" "$root/usr/lib/udev/rules.d" \
    "$root/etc/usb_modeswitch.d" "$root/usr/share/doc/sovel-aic8800-dkms"
cp -a "$source/drivers" "$root/usr/src/sovel-aic8800-$version/"
install -m 0644 "$source/LICENSE" "$root/usr/src/sovel-aic8800-$version/LICENSE"
sed -e 's/PACKAGE_NAME="aic8800"/PACKAGE_NAME="sovel-aic8800"/' \
    -e "s/PACKAGE_VERSION=\"1.0.0\"/PACKAGE_VERSION=\"$version\"/" \
    "$source/dkms.conf" > "$root/usr/src/sovel-aic8800-$version/dkms.conf"
cp -a "$source/fw/"* "$root/usr/lib/firmware/"
install -m 0644 "$source/aic.rules" "$root/usr/lib/udev/rules.d/60-sovel-aic8800.rules"
install -m 0644 "$source/usb_modeswitch/1111_1111" "$root/etc/usb_modeswitch.d/1111:1111"
install -m 0644 "$source/LICENSE" "$root/usr/share/doc/sovel-aic8800-dkms/copyright"
cat > "$root/usr/share/doc/sovel-aic8800-dkms/SOURCE" <<EOF
Source: https://github.com/shenmintao/aic8800d80
Commit: $commit
Archive SHA256: $checksum
Driver source and copyright notices are included in /usr/src/sovel-aic8800-$version.
Target reported by the user: AIC8800D80, USB 368b:8d81.
Physical Wi-Fi and Bluetooth operation requires testing on that adapter.
EOF
cat > "$root/DEBIAN/control" <<EOF
Package: sovel-aic8800-dkms
Version: $version-1
Architecture: all
Maintainer: Sovel Public Updates
Depends: dkms (>= 3), linux-headers-generic, gcc, make, eject, usb-modeswitch, kmod
Conflicts: aic8800-dkms
Section: kernel
Priority: optional
Description: AIC8800 USB Wi-Fi driver and matching firmware for Sovel
 Includes source for automatic rebuilds after kernel updates, USB mode-switch
 rules, and the device-scoped 368b:8d81 Bluetooth companion module.
EOF
cat > "$root/DEBIAN/postinst" <<EOF
#!/bin/sh
set -e
if [ "\$1" = configure ]; then
    if [ ! -d /var/lib/dkms/sovel-aic8800/$version ]; then
        dkms add -m sovel-aic8800 -v $version
    fi
    built=0
    for path in /lib/modules/*; do
        [ -f "\$path/build/Makefile" ] || continue
        dkms install -m sovel-aic8800 -v $version -k "\${path##*/}"
        built=1
    done
    [ "\$built" = 1 ] || { echo 'Matching kernel headers are required for the Wi-Fi driver.' >&2; exit 1; }
    if command -v udevadm >/dev/null 2>&1; then udevadm control --reload-rules || true; fi
fi
EOF
cat > "$root/DEBIAN/prerm" <<EOF
#!/bin/sh
set -e
case "\$1" in remove|upgrade|deconfigure)
    if [ -d /var/lib/dkms/sovel-aic8800/$version ]; then
        dkms remove -m sovel-aic8800 -v $version --all
    fi
esac
EOF
chmod 0755 "$root/DEBIAN/postinst" "$root/DEBIAN/prerm"
find "$root" -type d -exec chmod 0755 {} +
dpkg-deb --root-owner-group --build "$root" "$output/sovel-aic8800-dkms_${version}-1_all.deb"
