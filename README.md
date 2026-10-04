# Minimum v4.0.0

A small performance mod for Geometry Dash 2.2081 (Geode 5.10+) for **Windows, macOS, Android and iOS**.

Open the pause menu and tap the logo in the top right corner: quick toggles, a button to all settings, and the credits.

## What it does

| Feature | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Pause menu logo button, quick settings, credits | yes | yes | yes | yes |
| Real FPS counter (green / yellow / red) | yes | yes | yes | yes |
| Skip idle particle draws, particle cap, adaptive cap | yes | yes | yes | yes |
| Frame spike logger | yes | yes | yes | yes |
| Low Latency Mode (cuts frames queued in the driver) | yes | yes | - | - |
| Hotkeys | yes | yes | - | - |
| 1 ms timer, no power throttling, process priority | yes | - | - | - |
| Background throttle, tab-out volume, Fast Alt Tab, Draw Divide | yes | - | - | - |

Why some things are Windows only: they need a reliable "is the window focused" check or an OS feature that only
Windows has. Fast Alt Tab is left out of mobile on purpose: the save on focus loss is what protects your progress
when the OS closes the app.

## Input and latency (honest version)

* **Low Latency Mode** (Windows, macOS, on by default) empties the queue of frames the graphics driver keeps in
  flight, so a click reaches the screen one to two frames sooner. It only runs while the game is holding its frame
  rate. If your FPS feels worse, switch it off in the pause menu popup.
* It does **not** do sub-frame click timing. That is a different system (a separate input thread that timestamps
  clicks and splits physics steps). The *Click Between Frames* mod does exactly that on Windows, and it works next to Minimum.
* For the lowest latency also turn off V-Sync in the game video settings and use the highest FPS cap you can hold.
  Minimum never changes physics, timing or any gameplay value.

## Check that it works (30 seconds)

1. Start a level. The top-left line shows your real FPS.
2. Open the pause menu, tap the logo, switch **Minimum** off and on while watching FPS and how the game feels.
3. Windows: alt-tab away. The line shows `(background)`, GPU use drops, and audio goes quiet. Come back and the audio is back.

## Does it change gameplay?

No. Nothing here touches physics, player movement, level timing, input handling, or any gameplay value.
Particle changes are visual only. Draw Divide and the background throttle only skip drawing; game logic still
runs every frame with the same delta time. System tweaks only change how Windows schedules the process.

Still in development.
