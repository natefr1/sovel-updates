# Sovel public updates

Desktop update `0.1.0-12` **Stable Displays** keeps windows on their own display when you use the menu bar, rebuilds the display arranger, makes GTK dialogs and dropdowns readable, and adds the Sovel file picker, a Downloads hub and drawing tablet mapping. See the [Stable Displays release notes](releases/0.1.0-12.md).

Desktop update `0.1.0-11` **Private Access** adds a visible way into Private Mode from the Sovel menu, Settings and Super+Shift+P, keeps JARED available until Private Mode is set up, and starts Normal Mode on the city wallpaper. See the [Private Access release notes](releases/0.1.0-11.md).

Desktop update `0.1.0-10` **Dual Environment** separates the everyday city desktop from an encrypted Private Workspace with JARED and Vault. See the [Dual Environment release notes](releases/0.1.0-10.md).

Desktop update `0.1.0-9` **Workspace** adds multi-display controls, Sovel Files improvements and
JARED Notes, Projects, Research and local voice. See the [Workspace release notes](releases/0.1.0-9.md).

Desktop update `0.1.0-8` **Horizon** adds 29 Settings pages, Wi-Fi discovery and saved profiles,
configurable menu-bar controls, signed release history, 14 additional Store apps
and 24 configurable widgets. All previous Store listings and the greeter are preserved.
See [Horizon release notes](releases/0.1.0-8.md) for features, validation and limits.

Desktop update `0.1.0-7` adds the Sound revamp, local Mini JARED,
reliable bar/dock activation controls, Bluetooth friendly names and a monthly
calendar. All 23 Store listings and the greeter are preserved. See
[release notes](releases/0.1.0-7.md) for model download, usage, validation and limits.

Desktop update `0.1.0-6` adds coordinated translucent menus, a fluid dock,
improved app icons, multilingual fonts/input, Win+Shift+S screenshots, a local
wallpaper image picker and PipeWire capture/voice configuration. Store keeps all
previous listings and adds Discord, Vesktop and Hidamari. See
[release notes](releases/0.1.0-6.md) for usage, validation and current compositor limits.

Desktop update `0.1.0-5` adds the redesigned Files app, persistent dock pinning
and drag reordering, same-window minimize/restore, and a reviewed installer for
downloaded `.deb` packages. External Waypaper and mpvpaper replace the built-in
Motion app. See [release notes](releases/0.1.0-5.md) for usage and validation.

Desktop update `0.1.0-4` added the redesigned Store with browsable collections,
publisher screenshots, app detail pages and Install/Open actions. It also
coalesced Wi-Fi scan notifications and handled desktop output reconnection.
The reported GTX 1070 hard lockup and physical USB Wi-Fi operation still need
hardware verification; this release does not claim those are resolved.

Desktop update `0.1.0-3` added the AIC8800 driver as a dependency, so updating an
older Sovel installation also installs the driver and matching firmware.

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
