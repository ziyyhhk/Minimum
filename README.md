# Minimum v3.2.1

A small performance mod for Geometry Dash 2.2081 (Geode 5.10+) for **Windows, macOS, Android and iOS**.

Open the pause menu and tap the logo: quick toggles, a live FPS readout, a button to all settings, and the credits.
The logo sits in the top right by default. If another button is already there it moves to the nearest free spot by itself,
and you can pick another corner (or hide it) with the *Pause Menu Button* setting.

## What it does

| Feature | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Pause menu logo button, quick settings | yes | yes | yes | yes |
| Real FPS counter (green / yellow / red) | yes | yes | yes | yes |
| Skip idle particle draws, particle cap, adaptive cap | yes | yes | yes | yes |
| Low Latency Mode (cuts frames queued in the driver) | yes | yes | yes | yes |
| Unlock FPS past 60 | - | - | yes | yes |
| Frame spike logger | yes | yes | yes | yes |
| Hotkeys | yes | yes | - | - |
| 1 ms timer, no power throttling, process priority | yes | - | - | - |
| Background throttle, tab-out volume, Fast Alt Tab, Draw Divide | yes | - | - | - |

Why some things are Windows only: they need a reliable "is the window focused" check or an OS feature that only
Windows has. Fast Alt Tab is left out of mobile on purpose: the save on focus loss is what protects your progress
when the OS closes the app.

## Input and latency (honest version)

* **Low Latency Mode** (all platforms, **off by default**) empties the queue of frames the graphics driver keeps in
  flight, so a tap or click can reach the screen one to two frames sooner. The price: the CPU waits for the GPU every
  frame, so if your GPU is the limit it can lower your FPS. It only runs while the game is holding its frame rate,
  and if it costs frames it switches itself off for 10 s, then 20 s, 40 s and so on, instead of flickering on and off.
* **Unlock FPS** (Android / iOS, off by default) lets the game ask for frames faster than 60. On a 60 Hz phone nothing
  changes; on a 90/120 Hz screen the game can use it. Set *FPS Limit* to your screen's refresh rate.
* Minimum does **not** do sub-frame click timing. That is a different system, and it is already built into the game
  itself since 2.208 (Click Between / On Steps).
* For the lowest latency on PC also turn off V-Sync in the video settings and use the highest FPS cap you can hold.
  Minimum never changes physics, timing or any gameplay value.

## Check that it works (30 seconds)

1. Start a level. The corner shows your real FPS.
2. Open the pause menu, tap the logo, switch **Minimum** off and on while watching FPS and how the game feels.
3. Windows: alt-tab away. The line shows `(background)`, GPU use drops, and audio goes quiet. Come back and the audio is back.

## Does it change gameplay?

No. Particle changes are visual only. Draw Divide, the background throttle, Low Latency Mode and FPS unlock only change
*when and how drawing happens*; game logic keeps running every frame with the same delta time.

## Honest expectations

* A level that already holds your FPS cap will not run faster. Nothing can beat the cap.
* Gains show up in particle-heavy levels, in Draw Divide above your refresh rate, and while tabbed out.
* Do not run Draw Divide together with the `mat.draw-divide` mod. Both hook `CCDirector::drawScene`.

## Building

Push to GitHub and the workflow in `.github/workflows/build.yml` builds Windows, macOS, iOS, Android32 and Android64
and combines them into one `.geode` file (see the *Build Output* artifact of the run).

Or build locally with the Geode CLI: `geode build` (needs the SDK, and the Android NDK for mobile builds).

## Credits

* **Developer:** ziyyhhk
* **Helper:** Rafa
* **Testers:** Rafa, Broken Team, ziyyhhk

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) (qimiko / mat, MIT) and
[Algebra Dash](https://github.com/cgytrus/AlgebraDash) (ConfiG, MIT).
