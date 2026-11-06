# Sovel Desktop 0.1.0-10 Dual Environment

Install the update, then run the one-time setup from Normal Mode:
`sudo sovel-private setup --user nate-lab --normal-user nate`. Full design,
threat model and checklist: `docs/DUAL-ENVIRONMENT.md`.

## Changes

- **Two environments.** Normal Mode is an ordinary city desktop with everyday apps. Private Mode is a separate Linux account on its own virtual terminal with JARED, projects, engineering tools and the spider-emblem workspace. Clipboard, notifications, search, thumbnails, screenshots, display profiles and widget layouts are separate because the accounts and compositors are separate.
- **Encrypted private storage.** The private home is a systemd-homed LUKS2 image using AES-256-XTS with a 512-bit volume key, argon2id and a recovery key. It is unlocked only by the private password and deactivated when you exit.
- **Privacy-first locking.** Super+L, the menu-bar lock, idle, sleep and lid close all cover every display first and wait for the locker's confirmation. Only then do they quiet JARED, clear the clipboard, close the Vault and switch to the city lock. A failed lock never switches. Unlocking always opens Normal Mode.
- **Entering and leaving.** Super+Shift+P in Normal Mode locks Normal, then opens the private sign-in. Exit Private Workspace closes private windows and seals the account; it refuses while any window or Vault file is still open. An optional seal-while-locked delay is off by default.
- **Vault.** A second encrypted folder (gocryptfs, AES-256-GCM) inside Private Mode with its own passphrase. It closes after 15 idle minutes and whenever Sovel locks.
- **Command center.** Private Mode turns the secondary display into a JARED, projects, tasks and system-monitoring layout. Engineering and Research layouts are also available. Every widget stays movable.
- **JARED is private.** Normal Mode has no JARED bar item, dock entry, shortcut, widget or Settings page, and the binary refuses to start it there. Existing JARED data moves with `sudo sovel-private migrate-jared --from nate`.
- **New look.** A city sign-in and lock screen shared by both modes, a neutral graphite Normal palette, a violet laboratory palette in Private, glass panels with a specular rim, and a glass shader whose edge light follows the panel edge on wide bars and docks.
- **Fixes.** `sovel-lock` no longer counts a stale marker as a lock. Settings no longer fails when its autostart link is dangling.

## Validation and limits

- New automated suites: `tests/dual-environment.py` (18 cases: lock ordering, refusal rules, seal policy, keybinds, confirmed-lock timing with a simulated locker) and `tests/mode.cpp` (Normal/Private pages, palettes, widget registry, layout migration, presets). Changed C++ and QML were compile- and render-checked against Qt 6.4 offscreen. The existing settings and workspace suites pass with their counts updated for the private pages and widgets.
- Not yet run on the VM: live greetd on two VTs, homed creation and deactivation, ext-session-lock covering two outputs, VT switching through logind, monitor hotplug while locked, sleep and resume, and the Vault mount. Run the checklist in `docs/DUAL-ENVIRONMENT.md` before publishing.
- An administrator account can read an open private workspace. The isolation check warns when the Normal account has `sudo`, `adm` or journal access.
