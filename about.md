# Minimum v3.2.0

Small, honest performance mod for **Windows, macOS, Android and iOS**.

**Visual only.** Minimum does not touch gameplay, physics, player movement or game timing. It only changes what is drawn and how the game is paced by your operating system.

## All platforms
- Skips draw calls of idle particle systems (no visual change)
- Caps particle pool sizes (default 128) with Balanced / Performance / Extreme presets
- Optional Adaptive Particle Cap: lowers the cap by itself when the game misses your target FPS
- Stats line with FPS, worst frame time and spike count
- Frame spike logger: writes slow frames to the Geode log with the player x position, so you can find what causes a lag spike

## Windows only
- 1 ms timer resolution and no power throttling (smoother frame pacing)
- Optional process priority
- Throttles drawing while the game is in the background, lowers volume when tabbed out
- Fast alt-tab (skips the save on focus loss)
- Optional Draw Divide (experimental, off by default)

## Desktop only
- Hotkeys to toggle Minimum and the stats line (Ctrl+Shift+M / Ctrl+Shift+H, Cmd on Mac, rebindable)

Do not use Draw Divide together with `mat.draw-divide`.

## Credits

Draw Divide concept by qimiko / mat. Algebra Dash by ConfiG. Geode team.
