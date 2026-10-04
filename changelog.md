# v3.0.0

- Rewrite. Removed the thread pool, parallel batch transforms, batch sort hook, streak culling, fast level exit, anti aliasing log, Force RGBA8888 and early-load.
- Particle draw hook now targets `CCParticleSystemQuad::draw` (the old hook was never applied).
- Particle cap is applied when a system is created instead of patching live systems every frame.
- New: throttle drawing while the game window is not focused.
- Draw Divide now times frames with a steady clock.
- Settings are cached instead of looked up on every hook call.
- New on-screen stats line.
