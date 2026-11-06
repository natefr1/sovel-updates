# Sovel clean desktop rebuild · 0.1.0 engineering source

**0.1.0-10 Dual Environment:** Normal Mode and an encrypted Private Mode
(separate account, AES-256-XTS LUKS2 home, own VT) with privacy-first locking.
Start with `docs/DUAL-ENVIRONMENT.md`; release notes are in `docs/UPDATE_10.md`;
the browser preview is `preview/dual-environment.html`.

**Work Mode continuation:** the current source now requires Qt 6.8 or newer.
Read `docs/WORK-MODE-PROGRESS.md` for the newer implementation and target-VM
evidence. The original foundation notes below describe the starting package;
their Store, lockscreen and target-access limitations have since changed.
Additional runtime dependencies are `flatpak` and `gtklock`; native builds also
require `libwayland-dev` and `wayland-scanner`. This is still not a finished OS.

This is a new C++/Qt Quick desktop foundation based on your supplied desktop,
spatial-glass reference, answered design workbook, and interaction prototypes.
It does not load the old HTML, GNOME extensions, Vitreus, or Pleamar. The finished
product direction is one Sovel session with Sovel Files, Settings and Store.

**This package is source code, not a finished distribution or bootable ISO.**
No changes have been made to your VirtualBox VM from this workspace.

## What works in this source

| Component | Implemented behavior |
|---|---|
| Native UI | Qt Quick scene graph; no embedded browser or HTML overlay |
| Desktop | Independent background/widgets, top bar, bottom dock layer-shell surfaces |
| Dock | Neighbor distance falloff, spring magnification, expanding icon spacing, launch bounce, real launch actions |
| Widgets | Clock, real CPU/memory/storage readings, honest JARED/GPU unavailable states, drag/edit and saved normalized positions |
| Files | Real directory browsing, folder search, parent navigation, create folder, open files with registered apps |
| Settings | Persistent reduced motion and low effects preferences; local APT upgrade metadata inspection |
| Login UI | Animated ambient background, spatial glass panels, native challenge fields, no simulated authentication success |
| Authentication code | greetd framed JSON transport; PAM prompts, retries/cancellation, fixed Sovel session command, wait for start acknowledgement |
| Lock command | Uses the real swaylock session-lock client, with the Sovel palette |
| Retirement tool | Exact installed-package inventory and simulated purge; refuses additional unexpected removals |

The dock's Store and JARED actions explicitly report their unavailable status.
There are no pretend installed apps, fake performance graphs, project completion
notifications, encryption claims, or fabricated catalog data.

## Visual and motion direction

Your first image is the desktop target. Your second image defines spatial,
floating glass for login/lock: thin illuminated edges, layered depth, a warm
room behind the panels, and calm ambient movement. Purple-black remains the
desktop palette. The new built-in wallpapers are project assets, separate from
the actual interactive UI.

The interface uses native springs for dock and drag feedback, interruptible
state changes, and an ambient background animation that pauses with reduced
motion or low effects. This is not yet a measured Apple-level performance claim.
General application-window opening, minimizing and workspace animations need
compositor work beyond these shell animations.

The fragment shader refracts the wallpaper behind a panel, including rounded
edge displacement, a short bounded filter and a directional highlight. It
**does not capture other apps behind a window**, and does not constitute a
complete compositor glass pipeline. Missing GPU effects fall back to opaque
surfaces. The generated shader is baked by Qt's shader tools.

## Build on the Sovel Ubuntu 26.04 development VM

First install build/runtime dependencies. If installing greetd prompts for a
display-manager choice, keep the current manager while testing the new binary;
the old desktop is removed in the explicit retirement stage below.

```bash
sudo apt-get install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-shadertools-dev qt6-wayland \
  liblayershellqtinterface-dev layer-shell-qt \
  qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  labwc cage greetd swaylock foot

sh tools/build.sh
```

The full desktop requires the Qt 6 version of LayerShellQt (6.x), not the older
Qt 5 build from Ubuntu 24.04. CMake rejects an incompatible major version.
This package uses labwc for Wayland window/input management; it is not a newly
implemented kernel or compositor. Keeping core Linux services is necessary to
run apps, access disks, authenticate, connect to networks and render graphics.

You can test the design in an ordinary window without any layer-shell install:

```bash
sh tools/build.sh -DSOVEL_LAYER_SHELL=OFF
build/sovel --preview
build/sovel --app files
build/sovel --app settings
build/sovel --greeter
```

The last command is a **visual login test** when GREETD_SOCK is absent. It cannot
unlock a session and cannot authenticate. It is not a lockscreen.

## Install the actual desktop session

Rebuild with the native Wayland integration enabled before installing:

```bash
sh tools/build.sh -DSOVEL_LAYER_SHELL=ON
sh tools/verify.sh
sudo cmake --install build --prefix /usr/local
```

From the VM's actual text console, after saving open work and stopping its
current graphical session, run `/usr/bin/sovel-session` as your normal
user. SSH is not an active graphics seat. Do not run the desktop as root.

Super+E opens Files, Super+I opens Settings, Super+Enter opens Terminal,
Super+L invokes a real secure lock. Alt+Tab switches apps. Right-click an empty
desktop area to arrange widgets. The new launcher does not start the old
Vitreus/GNOME autostart components.

The initial native backend targets the primary display. Monitor hotplug,
mixed scaling, application focus/menu IPC and running-window indicators are
not finished.

## Replace the login manager and retire the old interface

The old `sovel-greeter-v06` ZIP is a GDM/GNOME extension. It is intentionally
not installed by this project. The replacement uses an unprivileged greeter
process, cage for its display, greetd/PAM for authentication, and labwc for
the user session.

After installing the binary:

```bash
sudo sh tools/prepare-greetd.sh
```

That prepares configuration but does not stop services. Test greetd on the VM's
console before removing the only installed login path. The preparation tool
prints the commands to select greetd at the next boot. Before retirement,
verify a real successful login, wrong-password retry, secure lock/unlock,
Files, terminal access, audio and networking. The IPC tests must run without
skips on the target.

Then inventory the concrete GNOME package removal:

```bash
python3 tools/retire-desktop.py
sudo python3 tools/retire-desktop.py --apply
```

The first command is read-only. The second performs an exact-package purge
only when greetd is active and the replacement binaries exist. It never
wildcard-purges `gnome*` or runs autoremove. If APT would remove anything outside
the explicit list, it exits before changing packages. This is not a guarantee
that every old file, user preference or app has been removed: the installed VM
still needs its own inventory. Old Pleamar/Vitreus source and user settings
are not deleted by this tool.

The desired final image contains no GNOME Shell, GNOME Control Center, GDM,
Ubuntu desktop session, previous Sovel GNOME extensions or Vitreus autostart.
Preserve the Linux kernel, boot chain, graphics/input stack, PAM, D-Bus,
PipeWire, NetworkManager, BlueZ and package engine, then expose their functions
through Sovel's interfaces. Do not erase home data or essential services to
remove old branding.

## Current validation and remaining release gates

See `docs/VALIDATION.md` for checks actually executed here. Native Wayland
session startup and real greetd/PAM authentication still require target testing.
Do not mistake windowed screenshots for proof of a working installed OS.

Still to implement: custom animated **secure** session-lock surfaces;
full Files operations; comprehensive Settings; Store/AppStream/Flatpak catalog
and transactions; signed online updates; branded live ISO and installer;
Windows dual-boot testing; full accessibility; all-display support; crash
recovery; compositor-level cross-window glass; and complete OS rebranding.
APT metadata inspection is not an online updater. A colored swaylock ring is
not the requested final lockscreen.

`docs/design-workbook.json` preserves the extracted worksheet content, including
your chosen rules, defaults and alternatives. The latest direct request takes
precedence where it differs. Its 495 decisions are requirements, not 495
completed features.

## Reference documentation

- Qt Quick shader effects: https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html
- Qt Quick texture sources: https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html
- LayerShellQt: https://github.com/KDE/layer-shell-qt
- labwc configuration: https://labwc.github.io/labwc-config.5.html
- greetd source and protocol: https://git.sr.ht/~kennylevinsen/greetd
- Ubuntu 26.04 LayerShellQt package: https://packages.ubuntu.com/resolute/liblayershellqtinterface-dev
