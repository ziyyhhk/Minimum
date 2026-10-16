# v3.2.2

- New: Cull Off-screen Particles (on by default). The draw of a particle system whose particles are all clearly outside the screen is skipped. Only the drawing is skipped, never the update, so emission and lifetimes run exactly as before and effects look the same when they come back into view. If you ever see an effect vanish near the edge of the screen, turn it off in the settings.
- New: Spawn Budget (off by default). Caps how many new particle systems a level can create per frame. Systems over the budget are still created, with a tiny pool. It never applies while a level is loading, only after you have been in it for a moment.
- New: Detailed stats now show three lines. Line 1 adds the worst frame of the last half second. Line 2 is a proof line: particle draws skipped, culled and capped per second, the current particle cap step and cap, Low Latency backoff trips, and `thermal` when the thermal response is active. Line 3 shows the totals: frames drawn, logic-only frames, idle draws skipped, culled draws, capped pools and budgeted systems. These counters were collected before but never shown.
- New: CPU-bound / GPU-bound readout on Android, in the popup's frame line and in the Detailed stats. It looks at how much of the last 60 frames the CPU was busy: `CPU-bound` at 85% or more, `GPU-bound` below 50% while Low Latency sync is on, nothing otherwise. It is Android only because on the other platforms the measured frame work includes waiting for the display.
- New: Thermal response on Android 11+. While the device reports a severe thermal status or worse, the particle cap goes down one more step. It is given back after 30 seconds of normal temperature. The Detailed stats show `thermal` while it is active. (iOS is not done: it needs Objective-C.)
- New: Unlock FPS and FPS Limit (30 to 240, default 120) on Android and iOS, off by default. Asks the system for a higher frame rate than the game's usual 60. Experimental: the screen and the system still decide what you get, so check game speed after turning it on.
- New: Precise Frame Limiter on Windows and macOS, off by default. Sleeps until the frame deadline, then spins for the last millisecond, to hold your Target FPS with steadier frame times. Only runs while the game's own FPS cap is off and Draw Divide is off.
- New: Benchmark mode (hidden). Tap the logo in the Minimum popup five times quickly to record 60 seconds of frame times to a CSV (`benchmark-<time>.csv`) in the mod's save folder. Tap it five times again to cancel.
- Fixed: the popup could run off a very short screen (for example 224 high). When even the tightest layout is too tall, the whole popup is now scaled down to fit.
- Fixed: the Adaptive Particle Cap counted frames that were not drawn when it decided how many frames were slow, so with Draw Divide the slow frames were diluted and the cap never came down. It now only counts drawn frames, and takes the Draw Divide interval into account so a 30 FPS Draw Divide is not mistaken for a slow game. The decision itself moved into the tested logic file.
- Fixed: Low Latency Mode's backoff counter only ever went up, so one bad minute kept the backoff long for the rest of the session. Every 2 minutes with the sync running and no new backoff now forgives one (never below 0). The 10 s, 20 s ... 5 min backoff itself is unchanged.
- Fixed: the worst frame time was recorded but never used. It is now shown in the Detailed stats and starts over every half second.
- Fixed: the stats line could run off the screen at a high HUD Scale, and the new three line Detailed text made that more likely. It now shrinks just enough to stay on screen.
- Fixed: the FPS number showed in red before the first measurement (0 FPS), as if the game was running badly. It is now grey until there is a number, in the popup and in the stats line.
- Changed: Minimum now says it conflicts with `mat.draw-divide` (Geode shows a warning, neither mod is blocked from loading).
- Changed: the built-in copy of the logo is now made by `tools/embed_logo.py` from `resources/logo_round.png`, and its header says what it really is (a 128 x 128 downscale of the 256 x 256 logo). The unused duplicate `resources/logo.png` was removed.
- Changed: the Android CPU Speed Hint text now says Android 13 (API level 33), checked against the NDK header (`__ANDROID_API_T__`).
- Changed: `src/mobile_fps.cpp` read settings that did not exist and was never part of the build. It was rewritten as the real Unlock FPS implementation and is now built.
- Under the hood: new unit tests for all the new logic, a CI job that builds and runs them (packaging waits for it), `build-tests/` is git-ignored, version 3.2.2.

# v3.2.1

- New pause menu popup. It has a live FPS card (current FPS, 1% low, frame time), five preset buttons, six quick toggles, a button to the full settings and the credits. It adapts to short screens so nothing gets cut off on a phone.
- New: Super Performance preset. 16 particles, Low Detail Mode on and the lowest background frame rate. The plainest look and the fastest.
- New: the pause menu button is now a smaller round logo (new `logo_round.png`) with a bigger invisible tap area, so it matches the other round buttons and is still easy to hit on a phone.
- Fixed: the pause menu button showed a pink and black square instead of the logo. The sprite was never listed under `resources` in mod.json, so Geode did not package it and the game drew its "missing texture". It is now listed, and a copy of the logo is also built into the mod, so the button can not end up as that square even if the PNG is missing.
- Changed: the FPS counter still shows the frames drawn per second, and now also shows the game logic rate (`| logic 240`) when it is clearly different from the FPS, because other FPS counters often show that number. Logic rate is counted from the game's scheduler updates.
- Fixed: the popup hitching when it opened. It now opens in small steps, one part per frame, and it no longer re-reads every setting when it opens. The pause menu button also does less work when the pause menu opens. If a step still takes more than 10 ms it is written to the Geode log as "Minimum popup: ... took ... ms".
- New: Low Detail Mode toggle. Turns on the game's own low detail option while Minimum is on, and puts your old value back when you turn it off.
- New: Low Latency Mode works on Android and iOS too (it was desktop only). glFinish is looked up in several GL libraries and the result is logged. The popup and the Detailed FPS counter show `sync on`, `sync waiting` or `sync n/a`, so you can see if it is really running.
- New: CPU Speed Hint on Android 13+. Tells the system how long each frame takes so the CPU speeds up before a heavy frame.
- New: Target FPS is now a main setting. The FPS counter colors, the adaptive particle cap and the Android hint all use it.
- New: credits now list Valicc and L4ZY as testers.
- Fixed: clicking Skip Idle Draws, Cap Particles or Low Detail in the popup while a preset was active did nothing, because the preset overwrote the click. It now leaves the preset first. Picking Custom keeps whatever the preset was doing.
- Fixed: locking the phone, switching apps or dragging the window could show a fake FPS drop, trigger a spike log line and make Low Latency Mode back off. Long gaps are now ignored and the measurements start over when you come back.
- Fixed: FPS counter colors were fixed at 55 and 30, so a 30 FPS phone was always yellow. They now follow your Target FPS.
- Changed: Adaptive Particle Cap is on by default, Frame Spike Logger is off by default.
- Changed: shorter, plainer description and settings text.
- Version number reset to 3.2.1. Entries below are from the older numbering.

# v4.1.0

- Fixed: the pause menu logo button could sit on top of other buttons and was hard to press. It now looks at the other buttons in the pause menu and moves to the nearest free spot, and its tap is checked before theirs.
- New: *Pause Menu Button* setting. Choose the corner (Top Right, Top Left, Bottom Right, Bottom Left) or hide it.
- New: redesigned popup. Toggles are in cards, the labels are bigger, the credits have their own box, and the popup adapts to the number of toggles.
- Changed: Low Latency Mode is now off by default (the saved value of people who already turned it on is kept). It makes the CPU wait for the GPU every frame, which can lower FPS.
- Fixed: Low Latency Mode could switch itself on and off every half second, which made the FPS number and the feel of the game alternate. When it has to back off it now stays off for 10 s, then 20 s, 40 s ... up to 5 minutes.

# v4.0.0

- New: logo button in the top right of the pause menu. Opens quick toggles, a button to all settings, and the credits (Developer ziyyhhk, Helper Rafa, Testers Rafa / Broken Team / ziyyhhk). Works on every platform.
- Fixed: the FPS number. It is now measured with the wall clock inside the render hook, so it shows the real frames drawn per second, and it says `(background)` when the throttle is limiting it.
- Fixed: audio staying quiet after tabbing back in. The tab-out volume now scales FMOD's master volume instead of rewriting the game's own volume values, and it re-checks every frame, so a missed focus event can not leave it stuck.
- Fixed: focus detection can no longer report "unfocused" by mistake (two independent checks, two polls in a row), which could throttle the game or dim the audio while you were playing.
- New: Low Latency Mode (Windows / macOS), trims frames queued in the driver so input reaches the screen sooner.
- FPS counter is colored by performance and updates twice a second.

# v3.2.0

- Multi-platform builds, per-platform settings, adaptive particle cap, hotkeys, spike logger.
