#!/bin/bash
set -euo pipefail
test "$(id -u)" = 0 || exit 1
build_dir=$(realpath "${1:?Image build directory required}")
case "$build_dir" in */iso-build) ;; *) exit 2;; esac
test -f "$build_dir/configuration.complete"
root="$build_dir/rootfs"
iso_tree="$build_dir/iso-tree"
revision=${2:-}
[[ -z "$revision" || "$revision" =~ ^[a-z0-9][a-z0-9-]*$ ]] || exit 2
output="$build_dir/Sovel-0.1.0${revision:+-$revision}-amd64.iso"
test ! -e "$output" || { echo 'An ISO already exists; inspect it before rebuilding.' >&2; exit 3; }
for point in dev proc sys; do
    mountpoint -q "$root/$point" && { echo 'Unexpected temporary mount.' >&2; exit 4; }
done
cleanup(){
    for point in sys proc dev; do mountpoint -q "$root/$point" && umount -R "$root/$point" || true; done
}
trap cleanup EXIT
mount --rbind /dev "$root/dev"
mount --make-rslave "$root/dev"
mount -t proc proc "$root/proc"
mount --rbind /sys "$root/sys"
mount --make-rslave "$root/sys"
for module in overlay squashfs loop; do
    grep -qxF "$module" "$root/etc/initramfs-tools/modules" || printf '%s\n' "$module" >> "$root/etc/initramfs-tools/modules"
done
chroot "$root" update-initramfs -u -k all
cleanup
trap - EXIT
# Fail if a mount survived cleanup; never include host pseudo-filesystems.
if findmnt -rn -o TARGET | grep -F "$root/" >/dev/null; then
    echo 'A filesystem is still mounted inside the image root.' >&2; exit 5
fi
kernel=$(find "$root/boot" -maxdepth 1 -type f -name 'vmlinuz-*' | sort -V | tail -1)
test -n "$kernel"
release=${kernel##*/vmlinuz-}
test -f "$root/boot/initrd.img-$release"
lsinitramfs "$root/boot/initrd.img-$release" > "$build_dir/initrd.manifest"
grep -qx 'usr/bin/plymouth' "$build_dir/initrd.manifest"
grep -qx 'usr/sbin/plymouthd' "$build_dir/initrd.manifest"
grep -qx 'usr/share/plymouth/themes/sovel/sovel.script' "$build_dir/initrd.manifest"
mkdir -p "$iso_tree/casper" "$iso_tree/boot/grub"
cp "$kernel" "$iso_tree/casper/vmlinuz"
cp "$root/boot/initrd.img-$release" "$iso_tree/casper/initrd"
cp "$build_dir/packages.manifest" "$iso_tree/casper/filesystem.manifest"
du -sx --block-size=1 "$root" | cut -f1 > "$iso_tree/casper/filesystem.size"
grep -q '^CONFIG_SQUASHFS_ZSTD=y' "$root/boot/config-$release"
mksquashfs "$root" "$iso_tree/casper/filesystem.squashfs" -noappend -comp zstd -Xcompression-level 6 -processors 1 -mem 512M -no-progress
cat > "$iso_tree/boot/grub/grub.cfg" <<'GRUB'
set default=0
set timeout=12
set color_normal=light-gray/black
set color_highlight=white/magenta
search --file /casper/filesystem.squashfs --set=root
menuentry 'Start Sovel Setup Assistant' {
    linux /casper/vmlinuz boot=casper username=sovel hostname=sovel-live quiet splash ---
    initrd /casper/initrd
}
menuentry 'Sovel â€” software graphics for virtual machines' {
    linux /casper/vmlinuz boot=casper username=sovel hostname=sovel-live sovel.renderer=software-gl quiet splash ---
    initrd /casper/initrd
}
menuentry 'Sovel â€” reduced effects recovery' {
    linux /casper/vmlinuz boot=casper username=sovel hostname=sovel-live sovel.renderer=software quiet ---
    initrd /casper/initrd
}
GRUB
printf 'Sovel 0.1.0\nEdition: Public\nRelease channel: public\nKernel: %s\n' "$release" > "$iso_tree/BUILD-INFO.txt"
# Casper verifies this manifest at live startup. Keep the manifest out of itself.
(cd "$iso_tree" && find . -type f ! -path './md5sum.txt' -print0 | sort -z | xargs -0 md5sum > md5sum.txt)
grub-mkrescue -o "$output" "$iso_tree"
(cd "$build_dir" && sha256sum "${output##*/}" > "${output##*/}.sha256")
echo "Public-edition ISO assembled: $output"
echo 'This is a test artifact. Boot, installation, dual-boot, driver, kernel-branding, and Secure Boot gates are not yet passed.'
