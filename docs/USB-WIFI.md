# USB Wi-Fi in the public image

The runtime includes the distribution's Linux wireless modules and signed firmware,
NetworkManager, BlueZ, USB mode-switch tools and USB identification tools.

The user's Foxtec AX900 + BT5.4 adapter identifies in Windows as
`USB\VID_368B&PID_8D81&REV_0100&MI_02` (AIC8800D80 USB WiFi).
The additional `sovel-aic8800-dkms` package contains the matched firmware and
driver source pinned to `shenmintao/aic8800d80` commit
`2e75f8885a053914c1b0328fdf4950f52691495d`. The build script verifies the
source archive's SHA256 before packaging it. Source and original license notices
ship under `/usr/src/sovel-aic8800-*`.

The image build compiles the modules for its installed kernel. DKMS and matching
kernel headers remain installed so subsequent kernel updates can rebuild them.
Normal USB device matching loads the driver. The Bluetooth companion's alias and
quirk are scoped to `368b:8d81`; the standard kernel `btusb` driver is retained.

Successful compilation and USB ID matching do not prove radio operation.
Acceptance on the actual adapter still requires a cold plug-in, nearby-network
scan, connection, reboot/reconnection, and Bluetooth discovery and pairing.
Different AIC MCU revisions may need a different matched firmware branch.
