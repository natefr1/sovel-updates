# The next implementation pass

The main problem is now the compositor boundary, not a lack of themes.

1. Validate LayerShellQt 6.x + labwc + Qt Quick on the actual SovelDev VM.
   Check panel anchoring, background input, app focus, maximized-window reserved
   space, VT switching, app exit behavior, full login and real locking. Debug
   the existing native components before adding more shell features.
2. Replace fixed six-entry dock data with a real XDG application catalog and
   foreign-toplevel management, running-state indicators, pin persistence and
   activation of existing windows. Add application-owned menu/action IPC.
3. Build a dedicated animated session-lock client implementing
   ext-session-lock-v1. The compositor must own the locked state, cover every
   output, reject ordinary client input, and remain locked if the UI crashes.
   Share the visual components from Greeter.qml, but do not re-use a normal
   fullscreen window as a security boundary.
4. For glass over other apps and native app/window motion, evaluate a maintained
   compositor fork with explicit scene sampling and damage tracking. The
   included wallpaper shader is intentionally limited. Add a shared cached
   backdrop chain, per-surface lens masks, contrast protection and reduced
   effects. Measure 60 Hz and 120 Hz frame budgets on actual hardware.
5. Expand Files and Settings using mature backend libraries. Keep filesystem
   operations, polkit, networking, sound, accessibility, portals and accounts
   behind the shared Sovel design components. Provide process isolation.
6. Implement Store with a real catalog and per-user Flatpak installation; add
   permission review, cancellation, progress, installed-state reconciliation,
   and errors. Do not draw an install button that cannot complete a transaction.
7. Implement signed APT distribution packages and a bounded privileged update
   service. Add real online metadata refresh, verification, installation,
   restart requirements and recovery. Never put a password into a shell string.
8. Create a minimal Ubuntu-based live image with Sovel as its only desktop.
   Retire legacy components by explicit manifests. Test clean install,
   upgrade, recovery, UEFI, and Windows dual boot on disposable disks first.

No GPU performance claims, government-grade security claims or complete
feature status should be made before those tests exist. Keep dependency
license notices even when the user-facing product is branded solely Sovel.
