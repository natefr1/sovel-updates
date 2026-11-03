# Sovel public updates

The repository also includes `sovel-aic8800-dkms`, with matched firmware and
source for automatic rebuilds after kernel updates. It targets AIC8800 USB
adapters, including the user's Foxtec AX900 (`368b:8d81`). The pinned source is
https://github.com/shenmintao/aic8800d80/commit/2e75f8885a053914c1b0328fdf4950f52691495d.
Compilation for the image's kernel passed; physical Wi-Fi and Bluetooth testing
on that adapter is pending. Driver source and license notices ship in the package.

This repository distributes signed Sovel desktop packages for the public release channel. On an installed Sovel system, open **Settings → Software Update** to check for updates, review available packages, install them, and restart when needed.

The ISO includes the repository's public signing key and a `Signed-By` source entry. Updates are verified by APT and PackageKit; unknown signing keys and unsigned packages are not accepted automatically. System dependencies continue to use their independently signed upstream repositories.

Public signing key fingerprint:

`BE91 DC80 E6C7 049D B9DC 4C01 3BE7 EDB7 FDD2 7B2C`

The private signing key is not stored in this repository or the ISO. Published package versions are immutable. Repository metadata expires after 30 days and must be refreshed by the release maintainer before that date; expired metadata is rejected.

This repository does not yet contain a downloadable ISO. A public ISO will be added after its installation checks pass.
