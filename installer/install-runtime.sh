#!/bin/bash
# Populate only the clean image root produced by bootstrap-root.sh.
set -euo pipefail
test "$(id -u)" = 0 || exit 1
build_dir=$(realpath "${1:?Image build directory required}")
source_dir=$(realpath "${2:?Sovel source directory required}")
case "$build_dir" in */iso-build) ;; *) exit 2;; esac
test -f "$build_dir/bootstrap.complete"
root="$build_dir/rootfs"
test -d "$root/etc/apt"
test -f "$source_dir/installer/runtime-packages.txt"
# Re-running this stage is idempotent and installs any newly added manifest items.
mkdir -p "$root/dev" "$root/proc" "$root/sys"
for point in dev proc sys; do
    mountpoint -q "$root/$point" && { echo "Unexpected existing mount: $point" >&2; exit 3; }
done
cleanup(){
    for point in sys proc dev; do
        mountpoint -q "$root/$point" && umount -R "$root/$point" || true
    done
}
trap cleanup EXIT
mount --rbind /dev "$root/dev"
mount --make-rslave "$root/dev"
mount -t proc proc "$root/proc"
mount --rbind /sys "$root/sys"
mount --make-rslave "$root/sys"
printf '#!/bin/sh\nexit 101\n' > "$root/usr/sbin/policy-rc.d"
chmod 755 "$root/usr/sbin/policy-rc.d"
if test -L "$root/etc/resolv.conf"; then rm -f "$root/etc/resolv.conf"; fi
cp -L /etc/resolv.conf "$root/etc/resolv.conf"
if test -f "$root/etc/apt/sources.list.d/sovel.sources"; then
    truncate -s 0 "$root/etc/apt/sources.list"
else
cat > "$root/etc/apt/sources.list" <<'SOURCES'
deb https://archive.ubuntu.com/ubuntu resolute main restricted universe multiverse
deb https://archive.ubuntu.com/ubuntu resolute-updates main restricted universe multiverse
deb https://security.ubuntu.com/ubuntu resolute-security main restricted universe multiverse
SOURCES
fi
chroot "$root" env DEBIAN_FRONTEND=noninteractive apt-get update
mapfile -t packages < <(sed '/^[[:space:]]*#/d; /^[[:space:]]*$/d' "$source_dir/installer/runtime-packages.txt")
chroot "$root" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
# Build from a pinned, verified source archive and compile for the image's
# installed kernel. A failed driver build must stop ISO creation.
bash "$source_dir/packaging/build-aic8800-deb.sh" "$build_dir/driver-packages"
driver_package="$build_dir/driver-packages/sovel-aic8800-dkms_1.0.0+git2e75f888-1_all.deb"
install -m 0644 "$driver_package" "$root/tmp/sovel-aic8800-driver.deb"
chroot "$root" env DEBIAN_FRONTEND=noninteractive dpkg --install /tmp/sovel-aic8800-driver.deb
rm -f "$root/tmp/sovel-aic8800-driver.deb"
# DKMS creates a local module-signing key during compilation. Each installed
# machine must generate its own key; never distribute the image builder's key.
rm -f "$root/var/lib/dkms/mok.key" "$root/var/lib/dkms/mok.pub"
chroot "$root" apt-get clean
touch "$build_dir/runtime.complete"
echo 'Clean runtime packages installed. Image configuration and ISO creation remain.'
