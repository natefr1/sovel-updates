# Sovel 0.1.0-10 — Dual Environment

Normal Mode is the computer everyone sees: a city wallpaper, everyday apps,
ordinary files. Private Mode is the laboratory: JARED, projects, engineering
tools and the Vault, in a dark violet workspace with the white spider emblem.
Every lock covers every display first and returns to the same city lock screen.

This document is the design, the threat model, the setup procedure and the
acceptance checklist. It says plainly which parts were verified where.

## 1. Architecture

| Layer | Normal Mode | Private Mode |
|---|---|---|
| Linux account | Your existing account (for example `nate`) | A separate systemd-homed account (for example `nate-lab`), member of `sovel-private` |
| Storage | Ordinary home directory | LUKS2 image `/home/<user>.home`, **AES-256-XTS (512-bit volume key)**, argon2id, umask 077 |
| Keys | Never holds private keys | Derived from the private password; dropped when the home deactivates |
| Virtual terminal | VT 7 (existing greetd) | VT 8 (`sovel-private-greetd.service`, its own greetd with `switch = false`) |
| Compositor, clipboard, notifications, screenshots | Its own labwc instance | Its own labwc instance |
| Display profiles, wallpapers, widget layouts | `~/.config/Sovel` of the Normal account | `~/.config/Sovel` of the private account |
| JARED | Not present: no bar button, dock entry, shortcut, widget, Settings page; `sovel --app jared` exits 3 | Fully integrated, history inside the encrypted home |
| Vault | Not present | gocryptfs folder (**AES-256-GCM**) at `~/Vault` with its own passphrase |

Mode is decided by **real group membership** (`sovel-private`), checked with
`getgroups()` in C++ and `os.getgroups()` in Python. No environment variable,
preference or command-line flag can switch a Normal account into Private Mode.
`SOVEL_MODE` is exported for information only and is never trusted.

Because the two modes are different Unix users running different compositors,
the separation of clipboard, search indexes, thumbnails, recent files,
notifications, screen capture and display profiles comes from the operating
system, not from the UI hiding things.

## 2. Transitions

All transitions go through `/usr/bin/sovel-mode`. A lock is only considered
established when gtklock's `--lock-command` has run, which happens after the
ext-session-lock protocol reports that every output is covered.
`sovel-lock --wait` exits 0 only then.

| Action | Order of operations |
|---|---|
| **Enter Private** (`Super+Shift+P` in Normal, configurable at setup) | Lock Normal and wait for confirmation, then switch to VT 8. If the lock is not confirmed, nothing switches. |
| **Lock** (Super+L, bar lock button, Sovel menu, Settings, idle, sleep, lid, logind `Lock`) | Private: lock and confirm; quiet JARED (voice-stop, cancel); clear the clipboard; close the Vault if it is idle; record the event; switch to the Normal VT, where the city lock is already up. Normal: lock and confirm. |
| **Unlock Normal** | Always opens Normal Mode. Private stays locked on its own VT. |
| **Return to Private** | Press the shortcut in Normal, then type the private password on the city lock (VT 8). |
| **Exit Private Workspace** | Ask every window to close. If any remain, or the Vault is busy, stop and say so. Otherwise clear the clipboard, record the event, switch to Normal and end the session. homed then deactivates the encrypted home and drops its key. |
| **Seal while locked** (off by default; 5, 15, 30 or 60 min) | After the delay, while still locked, try the same close. If any window stays open (for example, an editor asking to save), stay locked and record `seal-deferred`. |
| **VT changed any other way** | `sovel-mode watch` locks the private session as soon as logind reports it inactive. Ctrl+Alt+F-key switching is disabled inside Private (`srvrkeys:none`). |

Idle locking uses swayidle. In Normal Mode it is configurable (0 to 60 minutes,
with lock-before-sleep on by default). Private Mode always locks before sleep and
never goes without an idle lock: 0 becomes 5 minutes.

## 3. Threat model

**Protected against:** someone using the Normal account, or the computer while
it is locked; someone who unlocks Normal Mode; theft of a powered-off or
sealed computer (private data at rest is AES-256-XTS); casual screen-sharing
and screenshots from Normal Mode; Normal Mode search, recent files, thumbnails,
clipboard history and notifications.

**Not protected against, by design:**

- **Root or an administrator.** Anyone with `sudo` can read an *open* private
  home and can install software that records it. If the Normal account is an
  administrator, Normal Mode can defeat Private Mode. The isolation check
  reports this. The recommended setup is a Normal account without `sudo`, and
  administration from a separate admin account.
- **A compromised kernel or firmware**, or physical attacks on RAM while the
  private home is open.
- **Data at rest while the private session is open or locked.** Locking hides
  and protects the session, but the key stays in memory until you exit or seal.
  homed's own suspend locking (`pam_systemd_home suspend=true`) is **not used**:
  it needs re-authentication from outside the user's session, and a gtklock
  running inside the frozen private session cannot provide that, so the
  session would hang. Use *Exit Private Workspace* or a seal delay when the
  computer leaves your control.
- **Running work during a seal.** Sealing closes windows the way their close
  buttons do. Apps that ask to save keep the workspace open. A terminal
  running a simulation closes without asking, which is why sealing is off by
  default.

## 4. Setup (on the Sovel VM or PC)

```bash
# 1. Install 0.1.0-10 (adds swayidle, gocryptfs, fuse3, systemd-homed).
sudo apt install ./sovel-desktop_0.1.0-10_amd64.deb

# 2. Review the plan without changing anything.
sudo sovel-private setup --user nate-lab --normal-user nate --dry-run

# 3. Create it. You choose the private password; homectl prints a recovery key once.
sudo sovel-private setup --user nate-lab --normal-user nate --size 64G

# 4. Move JARED's existing data out of the Normal account (prompts to unlock nate-lab).
sudo sovel-private migrate-jared --from nate
#    After checking JARED in Private Mode:
sudo sovel-private migrate-jared --from nate --remove-source

# 5. Check isolation.
sovel-private status
```

Sign out of Normal and back in once so its Settings helper writes the new
keybinds (Super+Shift+P, Super+L through `sovel-mode`).

**Recovery and backups.** The recovery key unlocks the private account if the
password is lost. With the workspace exited,
`sudo sovel-private backup /media/backup-drive` copies the sealed image. The
copy is still encrypted and opens only with the password or recovery key.
`sudo sovel-private disable` turns off the private greeter without touching the
account or its data.

**Full-disk encryption** is an installer-level choice and is reported, not
changed. Without it, Normal files and system logs are unencrypted at rest.
Private data stays encrypted either way. **Swap**: the isolation check warns
about an unencrypted swap file or partition. zram, or encrypted swap, is
recommended. **Hibernation** is disabled by setup.

## 5. Acceptance checklist

Status key: **Verified here** means checked by the automated tests in this
source tree (`tests/dual-environment.py`, `tests/mode.cpp`). **Needs VM** means
it depends on live greetd, PAM, homed, logind or labwc behaviour and has not been
run yet.

| # | Requirement | Status |
|---|---|---|
| 1 | Both monitors work as an extended desktop in Normal Mode | Unchanged from 0.1.0-9; **needs VM** re-check |
| 2 | Normal Mode cannot open or search private files | Unix permissions + LUKS by design; **needs VM** (`ls /home/nate-lab` as nate must fail; image inactive when sealed) |
| 3 | Private authentication activates the correct wallpapers and layouts | Mode defaults and presets **verified here**; **needs VM** for the two-display first run |
| 4 | JARED is inaccessible from Normal Mode | Settings pages, widgets, layouts, keybind, presets and binary refusal **verified here** |
| 5 | Lock immediately hides all private content on both monitors | Lock-before-switch ordering and confirmed-lock **verified here** with a fake locker; real ext-session-lock on two outputs **needs VM** |
| 6 | Normal unlock never resumes Private Mode | By construction: Normal unlock unlocks only VT 7. **Needs VM** |
| 7 | Private documents and clipboard do not leak into Normal Mode | Separate users and compositors by design; clipboard cleared on lock and exit. **Needs VM** |
| 8 | Vault remains encrypted and separately protected | Refused outside Private **verified here**; gocryptfs init, unlock and idle close **need VM** |
| 9 | Unplugging or reconnecting a monitor reveals no private windows | ext-session-lock must cover new outputs. **Needs VM** with hotplug |
| 10 | Sleep, wake, reboot and crashes preserve privacy | before-sleep lock and the VT watcher are written; **needs VM** |
| 11 | Unsaved private work is handled without silent data loss | Exit and seal refuse while windows remain (**verified here**); terminal caveat above |
| 12 | Encrypted backups restore with documented recovery credentials | `backup` command written; restore **needs VM** |

**The acceptance test:** with JARED and both monitors active in Private Mode,
press Super+L and hand the computer to someone else. They can unlock Normal
Mode and use both monitors, but cannot see, switch to or read the private
workspace.

## 6. Files

- `packaging/sovel-mode`: session transitions, idle and sleep locking, VT watcher, audit log
- `packaging/sovel-private`: root setup, status/isolation check, JARED migration, backup
- `packaging/sovel-vault`: gocryptfs Vault (status, init, unlock, lock)
- `packaging/sovel-lock`: `--wait` confirms the lock through gtklock's lock command
- `src/sovelmode.h`: the C++ mode check shared by Backend, Settings and Workspace
- `qml/Greeter.qml`: city sign-in on VT 7; password-only private greeter on VT 8
- `qml/Vault.qml`, `qml/TopBar.qml`, `qml/ShellGlass.qml`: Private UI and the glass material
- `assets/city.png` (generated by `tools/make-wallpapers.py`), `assets/emblem.png`, `assets/lab.png`
- `preview/dual-environment.html`: browser preview of the lock, Normal and Private states
