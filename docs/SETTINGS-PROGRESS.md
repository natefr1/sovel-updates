# Settings implementation checkpoint — 2026-11-01

Settings now uses a persistent sidebar with search, separate page components, Back navigation, scrolling content, glass surfaces, and reduced-motion-aware transitions.

Pages: General, Appearance, Wallpaper, Accessibility, Lock Screen, Applications, Software Update, Sound, Network, Power.

Working integrations:
- General reads actual machine, kernel, architecture, storage, and memory information.
- Appearance and accessibility persist reduced motion and transparency preferences.
- Wallpaper selects either bundled image and propagates through QSettings polling to the desktop and glass surfaces. Unknown wallpaper identifiers are rejected.
- Lock invokes the installed secure session lock service.
- Applications opens App Library.
- Software Update inspects cached APT metadata; it does not install updates or refresh the online catalog.
- Sound reads and changes the default WirePlumber output volume and mute state asynchronously. Volume is capped at 100%.
- Network reads actual NetworkManager connection details asynchronously. Connection editing and Wi-Fi setup are not implemented yet.
- Power lists profiles reported by power-profiles-daemon and changes the selected profile. The system authorizes this for an active local session; remote SSH tests are correctly denied. No authorization policy was weakened.
- Audio and network commands have a ten-second process timeout. Search recognizes related terms, Enter opens a match, Escape clears the search, and Ctrl+F focuses it. The sidebar scrolls when needed.

Evidence:
- Native core tests: 10 passed, zero failed or skipped, including authentication IPC and preference persistence.
- All ten page selectors exercised with actual Qt Quick mouse events at 1000×680 and 640×438 using a recording backend. Keyboard volume/mute, disabled controls for unavailable audio, related search terms, and empty results passed.
- A native optional audio integration executable changed volume and mute using the actual Backend methods and verified restoration of the original state.
- General page rendered in the active Wayland session with the actual wallpaper shader and no runtime log errors.
- The combined native service check passed from a process launched through the compositor's terminal: volume/mute changes, original audio state restoration, power-profile change/readback/restoration, and NetworkManager report parsing. The audio check waits for actual service state after asynchronous propagation rather than accepting command exit as proof of a change. The corrected build was installed in the development VM.

Still required before Settings is considered complete: live interaction checks for all pages, network editing/Wi-Fi, display configuration, power, Bluetooth, user/password management, keyboard/input controls, notification preferences, update installation, consistent icons, and broader accessibility validation. The existing small test set does not establish complete OS readiness.

Installer target is recorded in INSTALLER-REQUIREMENTS.md. User explicitly requested finishing Settings before creating the downloadable dual-boot installer ISO.
