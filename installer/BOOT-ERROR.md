# Initial live-boot failure and correction

The first development ISO reached the live initramfs but stopped with `/cow format specified as 'overlay' and no support found`.

The overlay module was present. Running `modprobe overlay` manually succeeded and allowed boot to continue. Inspecting the installed Casper scripts showed that `casper-helpers` sets `MP_QUIET=-q` for its supported build-system selectors and otherwise sets it to an empty string. Casper then calls `modprobe "${MP_QUIET}" -b overlay`. The initial generated configuration omitted `BUILD_SYSTEM`, so its default `Custom` selector caused an empty argument to be passed.

A native dry-run reproduced the distinction against the image's actual module tree:

- Empty option: exit 1, `Module  not found`.
- Configured quiet option: exit 0.

The revised live configuration sets Casper's internal build compatibility selector explicitly. This does not change Sovel's product identity. The ISO builder also force-loads overlay, squashfs, and loop in the early image, and the image validator checks the real helper/module-loader invocation.

The initial image's plain network sources.list also caused Casper's offline CD cache builder to attempt network updates before networking was ready. Sources now use a Deb822 file in sources.list.d, which that cache builder temporarily filters.

These changes require a rebuilt ISO and a successful cold-boot test. File preflight and module dry-runs alone do not establish boot or installer success. The initial ISO checksum was b311d60cf4ae366a3d00dca71b2b1f918a45e7df5db32936f169bc02361c314a and it must not be treated as a release artifact.

## Rebuild recovery, 2026-11-02

The first rebuild stalled after the development VM's 7.0.0-31 kernel fault killed a compression worker. The remaining threads waited without CPU or output progress. The failed build was stopped, its kernel log retained in the workspace, and the builder was updated and cold-booted into 7.0.0-42-generic.

The subsequent `sovel-image-r2-cold` build succeeded with one CPU allowed, lower priority, and a 1800 MiB memory limit. It consumed 6 minutes 13 seconds elapsed time and peaked at 1.7 GiB. The resulting R2 image is 1,558,323,200 bytes, SHA-256 `a6c7190a8f5c60e249d379cc300c8155c39de3eacb61937a34a0ba72cc8f1194`. Transfer verification and cold-boot/installation tests are the next gates.

## R2 live and setup testing

R2 passed the initial overlay mount. Its packaged greetd service conflicts with getty on tty7, while the generated configuration incorrectly selected tty1. Matching the configuration to tty7 brought up the desktop. The Qt SVG decoder is a separate `qt6-svg-plugins` package; adding it restored the dock icons. Both checks are now part of image preflight.

Calamares required a top-level slideshow reference in the branding file. A Sovel QML progress screen is now supplied, following the [upstream branding schema](https://github.com/calamares/calamares/blob/calamares/src/branding/default/branding.desc). Setup also needs an available X display for its keyboard support; Xwayland and display-environment forwarding are explicit dependencies.

The welcome, location, keyboard, manual partition selection, account creation, summary, and final confirmation screens were exercised in the disposable VM. Its 32 GiB disk contains a separate 1 GiB data fixture and an existing EFI directory fixture for preservation checks. The initial install attempts exposed missing `unsquashfs` and `rsync`; both are now required by the runtime manifest and preflight. No successful installation or preservation result is claimed yet.

The two-CPU validation VM then stopped responding. Its VirtualBox log records repeated timing catch-up failures and missing guest heartbeats while using the Windows hypervisor backend. It was powered off after failing to respond to shutdown. The next validation run uses one virtual CPU, VBoxSVGA, HPET, and the SATA host I/O cache. These are changes to the disposable VM, not Windows security settings.
