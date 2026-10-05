# v3.2.1

- New pause menu popup. It has a live FPS card (current FPS, 1% low, frame time), four preset buttons, six quick toggles, a button to the full settings and the credits. It also adapts to short screens so nothing gets cut off on a phone.
- New: Low Detail Mode toggle. Turns on the game's own low detail option while Minimum is on, and puts your old value back when you turn it off.
- New: Low Latency Mode now works on Android and iOS too (it was desktop only).
- New: CPU Speed Hint on Android 13+. Tells the system how long each frame takes so the CPU speeds up before a heavy frame.
- New: Target FPS is now a main setting. The FPS counter colors, the adaptive particle cap and the Android hint all use it.
- Fixed: clicking Skip Idle Draws or Cap Particles in the popup while a preset was active did nothing, because the preset overwrote the click. It now leaves the preset first. Picking Custom keeps whatever the preset was doing.
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
