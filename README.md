# Minimum

A performance mod for Geometry Dash 2.2081 (Geode 5.10+). Works on Windows, macOS, Android and iOS.

Pause a level and tap the logo. You get a live FPS card, five presets (Custom, Balanced, Performance, Extreme, Super Performance), six quick toggles and a link to the full settings.

## What it does

| | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Pause menu popup, FPS counter (with 1% low) | yes | yes | yes | yes |
| Skip idle particle draws, particle cap, adaptive cap | yes | yes | yes | yes |
| Low Latency Mode | yes | yes | yes | yes |
| Low Detail Mode | yes | yes | yes | yes |
| CPU speed hint (Android 13+) | - | - | yes | - |
| Hotkeys | yes | yes | - | - |
| 1 ms timer, no power throttling, process priority | yes | - | - | - |
| Background throttle, tab-out volume, fast alt-tab, Draw Divide | yes | - | - | - |

Fast alt-tab is left out of mobile on purpose. The save that happens when the app goes to the background is what protects your progress if the OS closes it.

## What to expect

- A level that already holds your FPS cap will not run faster. Nothing beats the cap.
- The gains are in particle heavy levels, on weaker phones and laptops, in Low Detail Mode, and while the game is in the background on PC.
- Low Latency Mode makes the CPU wait for the GPU after each frame, which can get a click on screen a frame or two sooner. If the GPU is already the slow part it costs FPS, so Minimum only runs it while the game holds its frame rate and turns it off for 10 s, then 20 s, 40 s and so on if FPS drops. On a phone that is CPU bound it may do nothing.
- To check it is running: turn on Stats Detail > Detailed and look for `sync on`, or open the Minimum popup, the frame line shows `sync on` / `sync waiting` / `sync n/a`. `n/a` means this device has no glFinish we could find (it is also written to the Geode log).
- It is not sub-frame click timing. The *Click Between Frames* mod does that on Windows and works next to Minimum.
- For the lowest latency also turn V-Sync off in the game's video settings and use the highest FPS cap you can hold.

## Quick check that it works

1. Start a level. The FPS counter shows your real FPS.
2. Open the pause menu, tap the logo, turn Minimum off and on and watch the number and how the game feels. Try the Extreme preset in a heavy level.
3. Windows: alt-tab away. The counter shows `(background)`, GPU use drops and audio goes quiet. Come back and the audio is back.

## Building

You need the Geode SDK and CLI (https://docs.geode-sdk.org/getting-started/). With `GEODE_SDK` set:

```
geode build
```

or with plain CMake:

```
cmake -B build
cmake --build build --config RelWithDebInfo
```

The GitHub Actions workflow builds all five targets and merges them into one `.geode` file (artifact `Minimum-all-platforms`).

Tests for the layout and frame statistics code need no SDK:

```
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Layout

- `include/minimum_logic.hpp` pure logic (frame statistics, presets, popup layout, latency guard). Unit tested.
- `src/frame_gate.cpp` the `CCDirector::drawScene` hook: timing, FPS counter, background throttle, Draw Divide
- `src/pause_menu.cpp` pause menu button and popup
- `src/particles.cpp` idle draw skipping, particle cap, adaptive cap
- `src/latency.cpp`, `src/game_quality.cpp`, `src/perf_hint.cpp` low latency, low detail, Android CPU hint
- `resources/logo_round.png` the round logo used for the pause menu button (`logo.png` stays the mod icon)
- `src/system_tuning.cpp`, `src/background.cpp`, `src/app_delegate.cpp` Windows extras

## Credits

Developer: ziyyhhk. Helper: Rafa. Testers: Rafa, Broken Team, ziyyhhk, Valicc, L4ZY.

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) (qimiko / mat, MIT) and [Algebra Dash](https://github.com/cgytrus/AlgebraDash) (ConfiG, MIT).
