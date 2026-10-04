# v3.2.0

- Now built for Windows, macOS, Android (32 and 64 bit) and iOS as one .geode file.
- Settings are per platform. Windows-only features (frame pacing tweaks, background throttle, tab-out volume, Draw Divide, Fast Alt Tab) are hidden on the other platforms.
- Fast Alt Tab is no longer built for Android / iOS: skipping the background save there could lose progress if the OS closes the app.
- Tab-out volume now follows the same focus check as the background throttle, so it can not get stuck low.
- New: Adaptive Particle Cap (opt-in). Lowers the particle cap automatically when the game misses your target FPS.
- New: hotkeys on desktop to toggle Minimum and the stats line (rebindable).
- Settings that depend on another setting are greyed out when that setting is off.
- Stats line keeps clear of the notch on phones.
