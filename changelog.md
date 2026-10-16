# v3.2.2

- New: off-screen particle culling. A particle effect that is fully off screen, with a margin for how far its particles can travel, is not drawn. It still updates, so it looks the same when it comes back. Only effects that use relative position mode are culled. Setting: *Cull Off-Screen Particles* (on by default).
- New: spawn budget. Limits how many particles a level may hold while it runs. 0 (the default) means no limit. Setting: *Spawn Budget*, 0 to 500.
- New: thermal response on Android. When the phone reports a severe or hotter state, the particle cap is lowered at once, and it goes back up after 30 s at a normal temperature. Works together with *Adaptive Particle Cap*.
- New: real FPS unlock on Android and iOS. *FPS Unlock* replaces the game's own frame cap with *FPS Limit* (30 to 240). Off by default.
- New: precise frame limiter on Windows and macOS. The last part of each frame wait is spun instead of slept, so frames land closer to their target time. Off by default. Game logic and timing are not changed.
- New: bottleneck readout. The popup and the Detailed stats say whether the CPU or the GPU is the limit, based on the last 60 frames.
- New: Detailed stats show three lines: frame numbers, skips and caps, and running draw counts.
- New: hidden benchmark. Five quick taps on the live FPS card in the popup record 60 s of frame times to a CSV file in the mod's save folder (`benchmark-<unix time>.csv`, columns `elapsed_s,frame_ms,cpu_ms`). Tap again to stop early.
- Fixed: the popup could open at full height on a short screen. It is now scaled to fit after it opens.
- Fixed: the adaptive cap counted frames before they were drawn. It now counts only drawn frames, after the draw, against your Target FPS.
- Fixed: Low Latency Mode never forgave its trips, so the backoff kept growing. After 120 s of running without a trip, one trip is forgiven. The 10 s to 5 minute backoff is unchanged.
- Fixed: pause menu buttons that are hidden inside a hidden parent no longer move the logo button.
- Fixed: the logo in the pause menu could be stored at 128 px while its comment said otherwise. The built-in copy is now regenerated at 256 px from `resources/logo_round.png`.
- Changed: at 0 FPS the FPS line is drawn in a neutral colour instead of a band colour.
- Removed: an unused worst-frame counter.
- Developers: the logic tests run in CI on Ubuntu. The Geode build action is pinned to a commit SHA. `mobile_fps.cpp` is replaced by `frame_rate.cpp`. `tools/embed_logo.py` regenerates the logo data. `.gitignore` ignores `build-tests/`.

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
