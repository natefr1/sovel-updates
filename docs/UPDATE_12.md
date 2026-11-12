# Sovel Desktop 0.1.0-12 Stable Displays

Multi-monitor stability, a new display arranger, readable GTK dialogs and
dropdowns, the Sovel file picker, a Downloads hub and drawing tablet mapping.

## Why windows jumped to the primary display

Labwc re-arranges every window whenever the space the menu bar reserves at
the top of a display changes, or when it is reconfigured, and it put windows
onto the display under the pointer. Sovel caused this in four ways:

1. The bar's reserved strip switched between 30 px and 0 whenever the bar
   revealed or hid over a maximized window, and whenever a dropdown opened.
2. Every settings change, including the Do Not Disturb switch in the menu bar,
   ran `labwc --reconfigure`. Automatic appearance did it again every hour.
3. Identify Displays turned bars on and off on the other displays.
4. The saved display layout was re-applied on every screen event, even when
   nothing had changed.

Now the reserved strip is fixed by the menu bar settings and never changes at
runtime, Labwc is reconfigured only when its configuration file actually
changes, and a restore that matches the current layout does nothing. With
"Automatically hide for maximized apps" on, the bar floats over windows
instead of reserving space.

## Changes

- **Display arranger.** Displays are drawn at their real sizes and positions,
  numbered left to right as in Identify. Drag to arrange with edge snapping;
  displays never overlap. Resolution and refresh rate are separate; scale,
  orientation, variable refresh (where the display reports it), per-display
  wallpaper, menu bar and dock placement. Nothing applies until Apply, and an
  unconfirmed change reverts after 15 seconds. HDR is shown as unavailable
  because the compositor has no HDR output yet.
- **Readable GTK and GNOME windows.** GTK3 apps, including the portal dialogs,
  followed only the GTK theme name, which stayed light. Sovel now sets the
  dark theme for GTK3, the color scheme for GTK4 and libadwaita, and a
  palette for both: graphite in Normal Mode, near-black with violet in Private
  Mode. Your own `gtk.css` is left alone.
- **Dropdowns.** Wi-Fi, Bluetooth and Sound lists painted rows in the text
  color, so names were invisible; fixed. All menu-bar dropdowns share one
  surface: more opaque, 150 ms open and 120 ms close, Escape and an outside
  click close them, one open at a time. Combo lists are as wide as their
  longest entry.
- **Sovel file picker.** An `org.freedesktop.impl.portal.FileChooser` backend:
  Open, Save, Save As, Select Folder and saving several files; Recent, Home,
  Desktop, Documents, Downloads, Pictures, Videos, Music, GTK bookmarks and
  mounted drives; search, type filters, a typed path (Ctrl+L), new folder,
  hidden files (Ctrl+H), an overwrite warning, and each app's last folder.
  GTK apps are routed through the portal. Turn it off in Settings → Apps &
  Defaults → Downloads to go back to the GTK picker. Try it with
  `sovel --file-picker-preview save`.
- **Downloads.** A Downloads button in the menu bar (Settings → Menu Bar or the
  Downloads settings) lists files still downloading and recent downloads with
  Open and Show. A notification arrives when one completes; in Private Mode it
  leaves out the file name. Settings → Apps & Defaults → Downloads changes the
  default folder.
- **Drawing tablets.** The pen is mapped to its display through Labwc, so the
  cursor jumps straight to the pen and no window moves. A recognised pen
  display (Huion Kamvas, Wacom Cintiq, XP-Pen Artist and others) is mapped
  automatically, again when it is plugged back in. Otherwise Settings →
  Drawing Tablets asks once. It also sets cursor behavior, orientation, pen
  buttons, touch mapping and the active area. Pressure and tilt go straight to
  apps; set pressure curves in the drawing app.

## Not included

Labwc does not remember where windows were on a display that is unplugged and
plugged back in, so they stay where Labwc moved them. Organising downloads
into folders by type, per-app tablet profiles and HDR are not included.
