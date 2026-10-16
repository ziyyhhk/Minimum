# Minimum

A performance mod for Geometry Dash 2.2081 (Geode 5.10+). Works on Windows, macOS, Android and iOS.

Pause a level and tap the logo. You get a live FPS card, five presets (Custom, Balanced, Performance, Extreme, Super Performance), six quick toggles and a link to the full settings.

## What it does

| | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Pause menu popup, FPS counter (with 1% low) | yes | yes | yes | yes |
| Skip idle particle draws, particle cap, adaptive cap | yes | yes | yes | yes |
| Off-screen particle culling, spawn budget | yes | yes | yes | yes |
| Low Latency Mode | yes | yes | yes | yes |
| Low Detail Mode | yes | yes | yes | yes |
| CPU speed hint (Android 13+, API 33) | - | - | yes | - |
| CPU-bound / GPU-bound readout, thermal response (Android 11+) | - | - | yes | - |
| Unlock FPS (experimental) | - | - | yes | yes |
| Precise frame limiter | yes | yes | - | - |
| Hotkeys | yes | yes | - | - |
| 1 ms timer, no power throttling, process priority | yes | - | - | - |
| Background throttle, tab-out volume, fast alt-tab, Draw Divide | yes | - | - | - |

Fast alt-tab is left out of mobile on purpose. The save that happens when the app goes to the background is what protects your progress if the OS closes it.

## What the FPS number means

The big number is frames drawn per second, measured with the real clock. The game can run its logic at a different rate than it draws (for example 240 updates and 60 frames). When the two are clearly apart the counter adds `| logic N`. Other FPS counters often show that logic rate, so if a different mod shows a different number, compare it with this one.

## The Detailed stats

*Stats Detail > Detailed* shows three lines:

1. the numbers you compare with other counters: FPS, 1% low, frame time, **worst** frame of the last half second, logic updates per second, spikes, Low Latency status (and on Android CPU time and `CPU-bound` / `GPU-bound`)
2. the **proof line**: `skipped` (idle particle draws), `culled` (off-screen draws) and `capped` (pools) per second, `cap step` and the cap in use, Low Latency `trips`, and `thermal` while the thermal response is lowering the cap
3. the totals since the game started: frames drawn, logic-only frames, idle draws skipped, culled draws, capped pools, budgeted systems

`CPU-bound` means the CPU was busy for 85% or more of the last 60 frames. `GPU-bound` means less than 50% **and** Low Latency sync is running, because only then does waiting for the GPU show up as CPU idle time. It is shown on Android only: on the other platforms the measured frame work includes the wait for the display, so the number would not mean "CPU".

## Particle features

- **Skip Idle Draws** and **Particle Cap**: see above. **Adaptive Particle Cap** lowers the cap step by step when the game misses your Target FPS (only drawn frames count), and on Android one more step while the device is severely hot (back after 30 s of normal temperature).
- **Cull Off-screen Particles** (on by default) skips the draw of systems that are completely off screen, with a margin. Updates always run, so nothing changes when they come back. Turn it off if an effect ever disappears near a screen edge.
- **Spawn Budget** (off by default) limits new particle systems per frame during a level. Extra systems get a tiny pool instead of failing. Not active while a level loads.

## Benchmark mode (hidden)

Open the Minimum popup and tap the logo five times quickly. Minimum records the frame time of every drawn frame for 60 seconds and writes `benchmark-<unix time>.csv` (`frame,time_s,frame_ms`) to the mod's save folder (`Mod::getSaveDir()`, the `minimum.optimization` folder in Geode's mods save directory). Tap five times again to cancel. A notification tells you when it starts and when the file is saved. Only frames the stats line would also count are recorded (not the first 300 frames, not while unfocused).

## Unlock FPS and the precise limiter

- **Unlock FPS** (Android and iOS, off by default) asks the system for 1 / *FPS Limit* seconds per frame when that is shorter than what the game asks for. It does not touch game logic, timing or physics. The screen refresh rate and the system can still cap you (Geode's header documents the display link director as supporting only 1/60, 1/30 and 1/15 s intervals, so iOS may stay at 60). Check that game speed feels normal after turning it on.
- **Precise Frame Limiter** (Windows and macOS, off by default) holds your Target FPS by sleeping until the frame deadline and spinning the last millisecond. It only runs while the game's own FPS cap is off and Draw Divide is off. Best with V-Sync off.

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

Tests for the pure logic (layout, frame statistics, latency guard, adaptive cap, thermal guard, bottleneck classifier, culling math, spawn budget, frame limiter, benchmark) need no SDK. The CI workflow runs them too:

```
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Layout

- `include/minimum_logic.hpp` pure logic (frame statistics, presets, popup layout and fit scale, latency guard, adaptive cap, thermal guard, bottleneck classifier, culling math, spawn budget, frame limiter, benchmark recorder). Unit tested in `tests/logic_test.cpp`.
- `src/frame_gate.cpp` the `CCDirector::drawScene` hook: timing, FPS counter, background throttle, Draw Divide
- `src/pause_menu.cpp` pause menu button and popup
- `src/particles.cpp` idle draw skipping, particle cap, adaptive cap, off-screen culling, spawn budget
- `src/thermal.cpp` Android thermal status (looked up at runtime), `src/mobile_fps.cpp` Unlock FPS (Android / iOS), `src/benchmark.cpp` benchmark mode
- `src/latency.cpp`, `src/game_quality.cpp`, `src/perf_hint.cpp` low latency, low detail, Android CPU hint
- `resources/logo_round.png` the round logo used for the pause menu button, listed in `mod.json` under `resources`. `src/logo_data.cpp` holds a built-in copy of it as a backup (128 x 128, made by `python3 tools/embed_logo.py`, check it with `--check`). `logo.png` stays the mod icon.
- `src/scheduler_count.cpp` counts game logic steps, so the FPS counter can show the logic rate next to the FPS
- `src/system_tuning.cpp`, `src/background.cpp`, `src/app_delegate.cpp` Windows extras

## Credits

Developer: ziyyhhk. Helper: Rafa. Testers: Rafa, Broken Team, ziyyhhk, Valicc, L4ZY.

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) (qimiko / mat, MIT) and [Algebra Dash](https://github.com/cgytrus/AlgebraDash) (ConfiG, MIT).
