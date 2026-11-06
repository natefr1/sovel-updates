# Public desktop update 0.1.0-4

The Store now opens to browsable collections, with publisher screenshots and icons, search, individual app pages, permission review, installation and Open actions. Data comes from Flathub's AppStream API and Flatpak's signed remote. Twenty real listings are bundled for immediate browsing; app details refresh online. Version information in the bundled snapshot can become stale until refreshed. Ratings and download counts are not invented.

Flathub is configured for the signed-in user when first searching or installing. Installed apps are discovered from Flatpak, not inferred from successful command submission. App installation and system software updates remain separate operations.

Motion Wallpapers provides a built-in aurora animation and silent looping local video backgrounds. It does not import Steam Workshop projects. Reduced Motion and Low Effects pause playback, as does a maximized app on that display. It is included in this desktop update and opens from Store, Settings or App Library.

Wi-Fi enumerations now coalesce NetworkManager property notifications instead of cancelling the previous enumeration every time a signal arrives. Scan errors, blocked/unmanaged devices and missing scan results have bounded, visible states. Physical AIC8800D80 scanning still requires testing on the user's hardware.

Desktop layer surfaces are recreated for outputs added after sign-in and removed when an output disappears. Geometry changes update surface sizes. This corrects missing hotplug lifecycle handling. It does not establish the cause of the reported GTX 1070 hard lockup, which requires kernel/GPU evidence.
