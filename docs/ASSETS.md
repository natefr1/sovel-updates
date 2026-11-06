# Project imagery

The built-in image generation tool produced the following two raster assets.
They are copied into the project and compiled into Qt resources. They are
background images only; all interactive panels are implemented in QML.

- `assets/landscape.png`: obsidian alien mountains and reflective lake at night,
  purple planet in the upper right, restrained magenta mineral veins, dark
  central negative space, no UI, text or people. This follows the first visual
  reference's landscape direction.
- `assets/lockroom.png`: cinematic home architecture studio at blue hour,
  warm strip lighting, wood desk, notebook, ceramic mug, plants, dark chair and
  dusk window, no people or UI. This follows the second reference's spatial
  desktop direction.
- `assets/landscape.svg`: original code-native low-detail fallback concept;
  retained as editable source but not the default wallpaper.

The images remain still textures. `Wallpaper.qml` animates a translucent light
field above them; it does not claim the whole scene is a video. Low effects
and reduced motion stop that animation.
