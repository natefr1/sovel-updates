# Sovel Desktop 0.1.0-7

This update covers Sound, Mini JARED, shell activation, Bluetooth names and the
monthly calendar. The larger Settings roadmap is deferred. The Qt/Labwc session,
greeter, existing Store catalog, pinning, window restoration and installer remain.

## Use

- Open **Settings → Sound** for outputs, microphones, individual app streams,
  device profiles and explicit PipeWire rate/buffer preferences. The music-note
  button in the top bar opens quick controls. Speaker tests are quiet left/right
  tones; microphone tests record five seconds locally. EasyEffects remains an
  external optional app.
- Open **Mini JARED** from App Library, the top-bar star, or **Super+J**. Basic
  arithmetic, time, memory and reviewed system actions work without downloading
  a model. General conversation needs the explicit **Download local model**
  action: the pinned official Qwen3-1.7B Q8_0 download is 1.83 GB. Its size and
  SHA-256 are verified before installation. Conversations use localhost only.
  CPU mode is the conservative default; the model unloads after two idle minutes.
  Actions are allowlisted and separately approved. Model replies cannot execute
  commands. Locking always requires approval. Optional text-to-speech is provided
  by espeak-ng. Voice input and wake phrases are not implemented in this release.
- Open **Settings → Desktop & Dock** to adjust automatic hiding, activation
  edge/sensitivity, reveal/hide delay and animation duration. The menu bar uses
  the top edge; a top-positioned dock shares activation with it.
- Paired Bluetooth devices have a Rename action and remember custom names.
  Battery percentages appear when the device exposes BlueZ Battery1. Hardware
  addresses are kept in Advanced Details instead of the normal display name.
- Click the clock for the current month, previous/next month and Today. The grid
  follows the locale's first weekday and highlights today's date.

Install the signed public update through **Settings → Software Update**, then
restart Sovel. The model is downloaded on demand, not embedded in the package.
This desktop update does not replace the USB ISO.

## Validation

Run `cmake --build build --parallel 2` and
`ctest --test-dir build --output-on-failure`. The 13 suites include authentication,
radio discovery/name persistence, Store preservation, local packages, public
updates, calendar leap years and shell input regions.

The manual `sovel-audio-live` target requires an isolated PipeWire/Pulse service
and checks real null-device routing, volume/mute/balance, microphone capture,
recording and captured left/right channel energy. `sovel-jared-live` exercises
the real local model; `--actions-only` checks approval/cancellation against an
isolated audio service, and `--download-only` tests the pinned HTTPS downloader.
Use separate XDG runtime, config, state, cache and data directories for these
manual checks. Never use test null devices as a user's persistent audio defaults.

Real compositor validation and release-specific evidence are recorded with the
public release notes. Physical HDMI/DisplayPort audio, Bluetooth hardware,
NVIDIA stability and refresh-rate performance require the actual PC.

The compositor's existing translucency limitations remain: this release does
not add live backdrop blur/refraction to Labwc or change the optional GNOME
package. No private signing keys or test credentials belong in release files.
