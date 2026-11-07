# Sovel Desktop 0.1.0-11 Private Access

A fix update for Dual Environment. After 0.1.0-10, an account that had not run
the one-time setup had no JARED and no visible way into Private Mode, and
Normal Mode kept the violet 0.1.0-9 wallpaper.

## Changes

- **JARED stays until setup.** JARED, its widgets, shortcut (Super+J) and
  Settings page remain in Normal Mode until Private Mode is set up on the
  computer. After setup and one sign-out, JARED lives only in Private Mode.
- **A way in.** The Sovel menu shows **Set Up Private Workspace…** before setup
  and **Open Private Workspace…** after it. Settings → Private Workspace now
  exists in Normal Mode too, with *Open Private Workspace*, *Set up Private
  Workspace* (a guided terminal: preview, confirm, create, move JARED data)
  and *Run isolation check*. It never shows private settings.
- **Super+Shift+P always does something.** Before setup it opens the setup
  page. If the private sign-in service is not running, it says so and does
  not lock. After switching, it confirms that the active VT really changed.
- **City wallpaper in Normal.** On first start of this update, Normal Mode
  resets to the city wallpaper once (clearing an old photo or live
  wallpaper). The violet landscape, spider emblem and laboratory are offered
  only in Private Mode.
- **Fixes.** The isolation check and private audit log were refused by the
  Settings service whitelist; they work again. A settings test had been
  partly commented out by accident; it runs in full again.
- **Diagnostics.** `sovel-mode doctor` reports setup, the private greeter
  service, sessions per VT, the active VT and recent `sovel-mode` journal
  messages, without private content.

## Validation and limits

- `tests/dual-environment.py`: 21 cases, 20 pass and 1 skips as root. New cases
  cover the missing-greeter refusal, the before-setup path and before-setup
  keybinds. `tests/mode.cpp`: 8 cases including the Normal-before-setup page
  set and the wallpaper upgrade. Settings and workspace suites pass. Built
  against Qt 6.4 offscreen.
- Not run on the VM: the guided setup terminal, VT switching after setup, and
  the Sovel menu item. The 0.1.0-10 VM note that the shortcut did not confirm
  arrival on the private VT still needs a VM run; `sovel-mode doctor` and
  `journalctl -t sovel-mode` now show where it stops.
