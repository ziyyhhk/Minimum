# Minimum

A performance mod for Geometry Dash 2.2081 (Geode 5.11.2). It works on Windows, macOS, Android and iOS, and in the level editor.

Pause a level (or the level editor) and tap the logo. You get a live FPS card, five presets (Custom, Balanced, Performance, Extreme, Super Performance), six quick toggles and a link to the full settings.

## What it does

| | Windows | macOS | Android | iOS |
|---|:-:|:-:|:-:|:-:|
| Pause menu popup, FPS counter (with 1% low) | yes | yes | yes | yes |
| Skip idle particle draws, particle cap, adaptive cap | yes | yes | yes | yes |
| Offscreen particle culling, spawn budget | yes | yes | yes | yes |
| Low Latency Mode | yes | yes | yes | yes |
| Low Detail Mode | yes | yes | yes | yes |
| Real FPS unlock (FPS Unlock, FPS Limit 30 to 240) | no | no | yes | yes |
| CPU speed hint (Android 13+) | no | no | yes | no |
| Thermal response (particle cap follows the phone's heat state) | no | no | yes | no |
| Precise frame limiter | yes | yes | no | no |
| Hidden benchmark (five quick taps on the live FPS card) | yes | yes | yes | yes |
| Hotkeys | yes | yes | no | no |
| 1 ms timer, no power throttling, process priority | yes | no | no | no |
| Background throttle, tab out volume, fast alt tab, Draw Divide | yes | no | no | no |

UNVERIFIED: the "Android 13+" minimum for the CPU speed hint and the API level of the thermal functions are not confirmed against the NDK headers. The mod looks these functions up at runtime and does nothing when they are missing, so older phones are not affected.

Fast alt tab is left out of mobile on purpose. The save that happens when the app goes to the background is what protects your progress if the OS closes it.

## What the FPS number means

The big number is frames drawn per second, measured with the real clock. The game can run its logic at a different rate than it draws (for example 240 updates and 60 frames). When the two are clearly apart the counter adds `| logic N`. Other FPS counters often show that logic rate, so if a different mod shows a different number, compare it with this one.

## Detailed stats and benchmark

Set Stats Detail to Detailed to see more lines under the FPS number:

* Line 1: FPS, 1% low, frame time, CPU time, logic rate, spike count, the sync state, the bottleneck (`CPU bound` or `GPU bound`, from the last 60 frames) and the thermal state when it is hot.
* Line 2: skips per second, capped draws per second, logic only frames per second, the current cap step and latency trips.
* Line 3: running totals of frames drawn, logic only frames, idle skips, offscreen skips and capped particle pools, and `particles N% of cpu`: the share of the CPU work of the last half second that went to particle update and draw. Use it to see whether particles are worth tuning in a level at all. Logic only frames can add particle time without drawn work, so the number is approximate.

The live card in the popup draws the newest 120 frames as a faint graph behind the numbers: one bar per frame, colored by how far it ran over budget, with a line at the frame budget of your Target FPS.

Benchmark: open the Minimum popup and tap the live FPS card five times quickly. Minimum records 60 seconds of frame times and saves them as `benchmark_<unix time>.csv` in the mod's save folder, with the columns `elapsed_s,frame_ms,cpu_ms`. The popup shows the time left while it records. Tap the card again to stop early and save what has been recorded.

## What to expect

* A level that already holds your FPS cap will not run faster. Nothing beats the cap.
* The gains are in particle heavy levels, on weaker phones and laptops, in Low Detail Mode, and while the game is in the background on PC.
* Offscreen culling only skips drawing. Particle updates still run, so an effect looks the same when it comes back on screen.
* Low Latency Mode makes the CPU wait for the GPU after each frame, which can get a click on screen a frame or two sooner. If the GPU is already the slow part it costs FPS, so Minimum only runs it while the game holds its frame rate and turns it off for 10 s, then 20 s, 40 s and so on if FPS drops. On a phone that is CPU bound it may do nothing.
* To check it is running: turn on Stats Detail > Detailed and look for `sync on`, or open the Minimum popup, the frame line shows `sync on` / `sync waiting` / `sync n/a`. `n/a` means this device has no glFinish we could find (it is also written to the Geode log).
* It is not subframe click timing. The *Click Between Frames* mod does that on Windows and works next to Minimum.
* For the lowest latency also turn VSync off in the game's video settings and use the highest FPS cap you can hold.

## Quick check that it works

1. Start a level. The FPS counter shows your real FPS.
2. Open the pause menu, tap the logo, turn Minimum off and on and watch the number and how the game feels. Try the Extreme preset in a heavy level.
3. Windows: alt tab away. The counter shows `(background)`, GPU use drops and audio goes quiet. Come back and the audio is back.

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

The GitHub Actions workflow builds all five targets and merges them into one `.geode` file (artifact `Minimum-all-platforms`). It pins `geode-sdk/build-geode-mod` to a commit SHA rather than `@main`, so an upstream change cannot break the build without an edit here.

Tests for the layout, frame statistics and particle culling code need no SDK:

```
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The workflow runs the same three commands on Ubuntu as its `tests` job.

## Layout

* `include/minimum_logic.hpp` pure logic (frame statistics, presets, popup layout, latency guard, particle culling bounds, governors, frame limit, benchmark recorder). Free of Geode and cocos types. Unit tested.
* `src/config.cpp` settings snapshot, refreshed every 500 ms. Per frame hooks read this, never the settings directly.
* `src/frame_gate.cpp` the `CCDirector::drawScene` hook: timing, FPS counter, detailed stats, background throttle, Draw Divide, precise limiter call
* `src/frame_rate.cpp` FPS Unlock (Android / iOS) through a `CCDirector::setAnimationInterval` hook that passes everything through when the unlock is off, and the precise frame limiter (Windows / macOS)
* `src/benchmark.cpp` hidden 60 s benchmark recording to CSV
* `src/pause_menu.cpp` pause menu button and popup, including the live card and the benchmark tap
* `src/particles.cpp` idle draw skipping, offscreen culling, spawn budget, particle cap, adaptive cap and thermal cap
* `src/latency.cpp`, `src/game_quality.cpp`, `src/perf_hint.cpp` low latency, low detail, Android CPU hint and thermal polling
* `src/hotkeys.cpp` keybinds (Windows / macOS)
* `resources/logo_round.png` the round logo used for the pause menu button, listed in `mod.json` under `resources`. `src/logo_data.cpp` holds a built in copy of it as a backup, regenerated by `tools/embed_logo.py` (standard library only). `logo.png` stays the mod icon.
* `src/scheduler_count.cpp` counts game logic steps, so the FPS counter can show the logic rate next to the FPS
* `src/system_tuning.cpp`, `src/background.cpp`, `src/app_delegate.cpp` Windows extras

## Credits

Developer: ziyyhhk. Helper: Rafa. Testers: Rafa, Broken Team, ziyyhhk, Valicc, L4ZY.

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) (qimiko / mat, MIT) and [Algebra Dash](https://github.com/cgytrus/AlgebraDash) (ConfiG, MIT).
