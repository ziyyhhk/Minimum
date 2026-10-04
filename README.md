# Minimum v3.2.0

A small performance mod for Geometry Dash 2.2081 (Geode 5.10+) for Windows, macOS, Android and iOS. v3 is a rewrite: everything that did
nothing, or could crash the game, was removed. What is left is what has a technical reason to help.

## Platforms

Built and tested for:
- Windows
- macOS
- Android (32-bit and 64-bit)
- iOS

One `.geode` file works on all of them.

Particle changes are visual only. Draw Divide and the background throttle only skip draws; the scheduler
runs every frame with the same delta time. The system tweaks only change how Windows schedules the process.
So Minimum is not a cheat mod and is not tagged as one.

## Platform support

| Feature | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Skip idle particle draws, particle cap, presets | yes | yes | yes | yes |
| Adaptive particle cap | yes | yes | yes | yes |
| Stats line, frame spike logger | yes | yes | yes | yes |
| Hotkeys | yes | yes | - | - |
| 1 ms timer, no power throttling, process priority | yes | - | - | - |
| Background throttle, tab-out volume, Fast Alt Tab, Draw Divide | yes | - | - | - |

Why some things are Windows only: they depend on a reliable "is the window focused" check (Windows has one, the others are not
verified), or on OS features that only exist on Windows. Fast Alt Tab is left out of mobile on purpose, because the save on
focus loss is what protects your progress when the OS closes the app.

## Fair play

Minimum only changes drawing and OS-level pacing. It has no hooks on `PlayLayer`, `GJBaseGameLayer` or `PlayerObject`, does not
change delta time, and does not alter what the game simulates. It is not tagged as a cheat and does not need Cheat API.
Draw Divide is the one feature that touches the frame loop: it only skips *rendering* on some frames, the scheduler still gets
the unchanged delta time. It is off by default.

## What it does

| Feature | Default | What it really does |
|---|---|---|
| Skip Idle Particle Draws | on | No draw call for particle systems with zero live particles. Dozens of those exist in a level. No visual change. |
| Cap Particle Pool Size | on (128) | Limits particles per system when the system is created. Helps in particle-heavy levels, thins extreme particle spam. Re-enter the level after changing it. |
| Throttle Drawing In Background | on (20 fps) | While the game window is not focused, the scene is drawn at 20 fps. Logic and audio keep running. Lower GPU/CPU use while you are tabbed out. |
| Draw Divide | off | Draws at "Visual FPS" while logic runs at full frame rate. Only useful when your FPS is above your monitor refresh rate. |
| Lower Volume When Tabbed Out | on | Dims music and SFX on focus loss, restores them on return. |
| Fast Alt Tab | on | Skips the game save on focus loss. Saves on level exit and on close are untouched. |
| Show Stats Line | on | Top-left line with drawn frames/s, logic-only frames/s and skipped particle draws/s. Turn it off when you are done testing. |
| 1 ms Timer Resolution (Windows) | on | Asks Windows for 1 ms timer resolution so frame pacing is less jittery. |
| Disable Power Throttling (Windows) | on | Opts the game out of Windows efficiency mode so it is not put on slow cores / low clocks. |
| Process Priority (Windows) | Normal | Optional Above Normal / High. High can starve OBS and Discord. |
| Frame Spike Logger | on (40 ms) | Logs every frame slower than the threshold to the Geode log with player x position and particle systems created in that frame. |
| Adaptive Particle Cap | off | Lowers the particle cap by itself (down to 1/8) when the game keeps missing Target FPS. Raises it again after 10 smooth seconds. |
| Preset | Custom | Balanced / Performance / Extreme overwrite the particle cap and background FPS in one click. |

## Tracking down lag spikes

No mod can remove a spike without knowing what causes it. Play the level where it stutters, then open the newest file in the Geode log folder
(Windows / macOS: `geode/logs` inside your Geometry Dash folder, Android: `Android/media/com.geode.launcher/game/geode/logs`)
and search for `Frame spike`. You can also watch them live in the Geode console (Geode settings). Each line has the frame time, the player x position and how many particle systems were
created during that frame. If the same x position shows up again and again, that spot in the level is the cause
(usually a particle or shader trigger). If the particle count is high, try the Performance preset. If spikes happen
everywhere with no pattern, it is usually the system (background apps, laptop power mode, vsync).
