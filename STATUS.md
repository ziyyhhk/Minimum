# Status

## Done

Project structure for Geode 5.x and GD 2.2081
Skip empty sprite batch draws
Offscreen particle culling
Offscreen streak culling
Fast Alt Tab
Force RGBA8888 (Windows)
Anti aliasing setting hook (log only)
CI builds for Windows and macOS with artifact upload
Crash fix 2.0.6: removed parallel batch transforms (thread-safety)
Hardened Draw Divide (still OFF by default)

## Still limited

Deep Algebra Dash ports (parallel move actions, full visibility rewrite, OpenGL path hacks) need current bindings and careful testing. They are not safe to drop in as is.

Parallel batch transforms are intentionally disabled forever unless a proven main-thread-only alternative is found.

## Next

More move and rotation path optimization when bindings are confirmed stable
Optional Tracy integration for profiling
Android CI matrix if requested
