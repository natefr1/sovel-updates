# Work Mode checkpoint — November 1, 2026

This remains an incomplete development source package, not an OS release.

## Changes in this checkpoint

- Disconnect and abort the authentication socket before Auth member teardown.
- Add a regression test requiring connected-client destruction to emit no state
  changes and to close its peer connection.
- Add `sovel-session --software`, selecting pixman and Qt Quick software rendering.
- Add isolated launcher checks for renderer propagation, compositor arguments,
  invalid arguments and missing login runtime. Include these in tools/verify.sh.

## Evidence

- Session startup contract checks passed using Git Bash on the Windows host.
- Launcher shell syntax check passed.
- Running VM: Sovel Dev 60gb. VMSVGA graphics, 3D acceleration disabled.
- SSH endpoint is forwarded on localhost port 2222; noninteractive authentication
  for nate was rejected. No guest files or installed desktop were changed.
- C++ compilation, regression execution, PAM login, real Wayland startup,
  graphics performance and installed-session verification have not yet run.
- The supplied ANSWERED.xlsx was independently extracted for specification review.

All release gaps in NEXT_IMPLEMENTATION.md remain open. The previous validation
document describes historical checks, not checks performed on this checkpoint.
