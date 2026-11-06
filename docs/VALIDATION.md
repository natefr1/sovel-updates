# Validation actually performed

This package was checked in an Ubuntu 24.04 container with Qt 6.4.2. Your
SovelDev Ubuntu 26.04 VM was not connected and was not modified.

| Check | Result | Limit |
|---|---|---|
| C++ application build | Passed, GCC 13.3, CMake, Ninja | Built with SOVEL_LAYER_SHELL=OFF locally |
| Fragment shader baking | Passed with Qt Shader Tools | Does not establish physical GPU performance |
| Native layer integration syntax | Passed using actual LayerShellQt 6.0.5 interface headers | Full Qt 6 LayerShellQt linking and Wayland runtime not available locally |
| Desktop render | Passed on Qt Quick OpenGL under Xvfb/Mesa | Windowed composition, not real layer-shell placement |
| Greeter render | Passed on Qt Quick OpenGL under Xvfb/Mesa | GREETD_SOCK absent; cannot authenticate |
| Files render | Passed with actual Backend directory data | Folder operations are basic; not a complete Files suite |
| Settings render | Passed after fixing a scroll-layout binding loop | Online refresh/install is not implemented |
| Core test execution | QtTest: 5 passed, 0 failed, 3 skipped | Pass count includes init and cleanup fixtures |
| Substantive passing core tests | Missing-auth preview fails; actual folder creation/listing and invalid-name rejection; layout validation/persistence | No real PAM authentication test |
| IPC integration tests | 3 skipped | This host prohibits AF_UNIX sockets; run on VM |
| Shell scripts | Syntax passed | Target service activation not executed |
| Python tools | Syntax passed | No package removal performed |
| Compositor XML | Parsed successfully | labwc has not loaded it on target hardware |

The three IPC test cases cover fragmented/multiple PAM prompts and the fixed
session command, failed-auth cancellation/retry, and oversized frame rejection.
They are present in `tests/core.cpp` but were **not exercised** here because
the local socket server cannot be created. A zero-failure CTest run with skips
is not a release authentication pass.

The four screens were rendered from the compiled native executable, not
reconstructed as HTML. Their shader path used OpenGL/Mesa in a virtual display.
This validates the QML loads and renders, not Apple-level smoothness, real
hardware frame budgets, secure locking, native seat ownership or bootability.

Run `sh tools/verify.sh` and inspect the test output on the target. Before
retiring GNOME, complete all target session/login/locking checks described in
README.md. The finished design is intentionally more ambitious than this
first engineering source package.

## 0.1.0-10 Dual Environment (container checks, 2026-11-06)

Run in a cloud container (Ubuntu 24.04, Qt 6.4.2, no display server):

- `python3 tests/dual-environment.py`: 18 cases, 17 passed, 1 skipped (the non-root check skips when run as root).
- `tests/mode.cpp`, `tests/settings.cpp` and `tests/workspace.cpp` built and passed against Qt 6.4 with the 6.8 version gate relaxed.
- `main.cpp`, `windows.cpp` and `auth.cpp` compiled with a PackageKit stub (PackageKit-Qt6 is not packaged for 24.04). `shaders/glass.frag` baked with `qsb`.
- Greeter (Normal and Private) and Vault rendered offscreen with the real Backend, Auth and SettingsService, with no QML errors. All changed QML parses with `qmlformat`.
- `shellcheck` on the session scripts reports only existing informational notes.

Not verified here: live greetd/PAM on two VTs, systemd-homed, logind VT switching,
gtklock on real outputs, swayidle, gocryptfs and labwc. See the checklist in
`docs/DUAL-ENVIRONMENT.md`.
