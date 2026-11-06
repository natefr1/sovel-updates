# Sovel public installer

The public image contains Sovel's desktop and Setup Assistant on an Ubuntu 26.04
amd64 base, a local account created during setup, and signed online updates.
The clean image excludes builder accounts, credentials and private signing keys.
GNOME Shell, GDM and Ubuntu desktop metapackages are excluded.

## Setup and partitioning

Setup includes language, nearby Wi-Fi, Bluetooth, time zone, keyboard, local
account, disk selection, review, installation and completion. Manual partitioning
is the initial choice. The final disk-write confirmation remains enabled.

For installation alongside another system, format only the empty Sovel partition
as ext4 at `/`. Keep the existing FAT32 EFI System Partition at `/boot/efi`
without formatting it. The configured EFI minimum is 100 MiB.

`mount.conf` supplies target device, process, system, runtime, udev and EFI
variable mounts. `soveltarget` checks them immediately after mounting, before
unpacking files. Finalization preserves the USB's graphics fallback, configures
the installed greeter and removes live-only permissions and packages. Network
and Bluetooth preferences are copied with private permissions.

## Image stages

Run only against the dedicated `iso-build` directory:

1. `bootstrap-root.sh` creates the clean base.
2. `install-runtime.sh` installs runtime packages and compiles the pinned AIC8800
   DKMS driver. Compilation failures stop the build. Temporary private module
   signing keys are removed. See `docs/USB-WIFI.md` for driver provenance.
3. `configure-image.sh` installs the public desktop package, setup, identity and
   trusted update source.
4. `validate-image.py` checks contents, dependencies, driver modules and USB ID
   matching. File checks do not prove installation succeeds.
5. `build-iso.sh` assembles BIOS/UEFI media and writes its SHA256 checksum.

The release signing home must stay outside the image and public repository.

## Validation checkpoint

`sudo python3 tests/installer-mounts.py` uses a private Linux mount namespace to
check mounted, missing and subsequently unmounted targets.

The previous image's VM installation passed account creation and connection
copying, then failed bootloader installation because its system mounts were
missing. Applying the corrected mounts allowed initramfs and bootloader creation
to complete. Existing EFI and separate data fixture checksums were unchanged.
This is a repair test, not a clean installation pass for the replacement ISO or
a Windows boot test.

The replacement image must still complete a clean installation and installed
login test. Physical Foxtec Wi-Fi scanning, connection, reboot/reconnection and
Bluetooth pairing require testing on the user's adapter. Secure Boot and a
custom branded kernel remain unfinished; this release requires Secure Boot off
and uses the distribution's generic kernel.

References: https://github.com/calamares/calamares and
https://calamares.io/docs/partitions/.
