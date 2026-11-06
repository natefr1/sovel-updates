# Sovel installation target

User confirmed on 2026-11-01: install into an empty partition on the secondary PC and retain a dual-boot system. Finish Settings before producing the installer ISO.

## Required installer behavior

- Show physical disk model, capacity, partition size, filesystem, and existing OS detection before selection.
- Default to explicit partition selection. Never default to erasing a disk or resizing the existing OS.
- Format only the selected installation partition after a final summary and explicit confirmation in the installer.
- An empty partition is not the same as unallocated disk space. Handle both explicitly without guessing which existing partition is disposable.
- Preserve the existing OS partitions and any existing EFI system partition. Reuse an EFI partition without formatting it when appropriate; keep existing boot entries.
- Detect firmware boot mode and reject incompatible installation combinations with an actionable explanation.
- Set up the Sovel boot entry and a working choice of installed operating systems.
- Provide account creation, timezone, keyboard layout, language, storage review, installation progress, and first-login setup.
- Do not carry development VM credentials, SSH keys, user files, machine IDs, or host-specific network configuration into the ISO.

## Release evidence needed

- ISO checksum and reproducible build manifest.
- Live boot, installer launch, partition selection, successful installation, reboot without ISO, account login, networking, audio, graphics, secure lock/unlock.
- Dual-boot test with an existing OS and a separate empty target partition; verify existing partition contents and boot path remain intact.
- Test cancel/back paths before disk writes and clear reporting of installation failures.
- Record firmware and hardware actually tested. Secondary PC model, CPU/GPU, existing OS, disk layout, encryption, and Secure Boot state are not known yet.

An initial development ISO was built, but failed its first live-boot test. A corrected image is being rebuilt; neither live boot nor installation is validated yet. The running development VM is not a distributable installer image. See `installer/BOOT-ERROR.md` for the reproduced failure and correction.
