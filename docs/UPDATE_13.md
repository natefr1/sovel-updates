# Sovel Desktop 0.1.0-13 Desktop Fix

0.1.0-12 left people at a blank screen after signing in. The shared dropdown
component used a property that Qt popups do not have, so the menu bar and
desktop failed to load and the shell exited. The dropdowns now fade and scale
instead. Everything else from 0.1.0-12 Stable Displays is unchanged.

The build now loads the desktop and the file picker as tests, so a QML error
like this fails the build instead of reaching anyone.

If you are stuck at the login screen on 0.1.0-12: press Ctrl+Alt+F3, log in,
and run `sudo apt update && sudo apt upgrade`, then `sudo reboot`.
