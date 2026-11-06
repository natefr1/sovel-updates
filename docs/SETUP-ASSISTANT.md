# Sovel Setup Assistant

The live session starts the assistant automatically. An installed session does
not. The optional live-desktop action and the App Library entry reopen the same
assistant rather than an unrelated Try/Install splash screen.

The sequence is welcome, setup language, nearby Wi-Fi, optional Bluetooth,
time zone and regional formats, keyboard and typing test, local account,
drive/partition selection, review, installation, and restart. There is no cloud
account, migration, voice assistant, family, advertising, or subscription step.

The QML pages bind directly to the packaged Calamares configuration objects.
Calamares retains account validation, partition operations, final review,
installation jobs, failure reporting, and bootloader installation. No account or
partition is written by an introductory QML page. The real disk step and summary
remain the maintained widget implementations, styled with the Sovel palette.

The shared `Sovel.Platform` QML extension uses the same NetworkManager and BlueZ
controller as Settings. Networks are discovered and deduplicated by SSID and
security. Passwords travel over the system bus, not command-line arguments.
Bluetooth discovery stops when its page closes; the session pairing agent
handles passkey confirmation. Hardware availability is reported explicitly.

The standard networkcfg job transfers network profiles. The subsequent
sovelconnections job corrects live-account restrictions and carries Bluetooth
pairing state into the chosen target, with private file modes and link checks.
This happens during installation; pairing keys are never baked into the ISO.

Glass uses a cached wallpaper sample, blur, tint, and an edge highlight. Page
entrances animate opacity and translation. Software rendering uses an opaque
fallback and skips the entrance animation. There is no continuous background
render loop in the assistant.

## Verification

- Native executable and QML extension build successfully in the builder VM.
- Core and private-bus radio tests pass without skips. Radio tests cover scan
  deduplication, raw SSID/password transport, activation state, authentication
  failure, and discovery lifetime, including delayed discovery replies.
- Connection-state tests check private modes and reject source/destination
  links on Linux.
- Host previews load all assistant pages in software and Direct3D 11 rendering
  without QML warnings. Settings passes all eleven pages at normal/minimum
  sizes, keyboard audio controls, and missing-device presentation.
- The actual image's Calamares loads the new QML pages, radio extension, custom
  job, and branding. Its mandatory disk checks remain enforced.
- A private UI-only Calamares configuration verifies real language/time-zone
  values, keyboard choices, and local-account inputs. Empty and mismatched
  credentials disable Next; valid matching credentials enable it. No account
  or disk is created by this UI test, and its temporary configuration is removed
  before image assembly.
- ISO cold boot, complete installation, first login, existing-OS preservation,
  and real Wi-Fi/Bluetooth hardware tests remain release gates. Preview or unit
  tests alone do not prove these.

The ISO remains a development artifact until its installation gates pass.
Signed public online updates are separate unfinished work.
