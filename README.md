# Minimum v3.0.0

A small performance mod for Geometry Dash 2.2081 (Geode 5.10+). v3 is a rewrite: everything that did
nothing, or could crash the game, was removed. What is left is what has a technical reason to help.

## What it does

| Feature | Default | What it really does |
|---|---|---|
| Skip Idle Particle Draws | on | No draw call for particle systems with zero live particles. Dozens of those exist in a level. No visual change. |
| Cap Particle Pool Size | on (128) | Limits particles per system when the system is created. Helps in particle-heavy levels, thins extreme particle spam. Re-enter the level after changing it. |
| Throttle Drawing In Background | on (20 fps) | While the game window is not focused, the scene is drawn at 20 fps. Logic and audio keep running. Lower GPU/CPU use while you are tabbed out. |
| Draw Divide | off | Draws at "Visual FPS" while logic runs at full frame rate. Only useful when your FPS is above your monitor refresh rate. |
| Lower Volume When Tabbed Out | on | Dims music and SFX on focus loss, restores them on return. |
| Fast Alt Tab | on | Skips the game save on focus loss. Saves on level exit and on close are untouched. |
| Show Stats Line | on | Top-left line with drawn frames/s, logic-only frames/s and skipped particle draws/s. Turn it off when you are done testing. |

## Check that it works (30 seconds)

1. Start a level. The top-left line shows `Minimum ON | drawn ... | idle particle draws skipped ...`.
   A number above 0 for "idle particle draws skipped" means the hook is active.
2. Toggle **Enable Minimum** in the mod settings. The line switches to `OFF` and the skipped counter stops. Compare your FPS.
3. Alt-tab away and look at GPU use in Task Manager: it drops because drawing is throttled.

## Honest expectations

* A vanilla level that already runs at your FPS cap will not run faster. Nothing can beat the cap.
* Gains show up in levels with heavy particles, when you use Draw Divide above your refresh rate, and while tabbed out.
* Do not run Draw Divide together with the `mat.draw-divide` mod. Both hook `CCDirector::drawScene`.

## What was removed from v2 and why

* **Thread pool and "parallel batch"**: Cocos is not thread-safe and the pool did nothing. Joining threads inside a DLL at exit can hang the game.
* **Skip empty batch draws**: the game already does this, so the hook only added overhead.
* **Fast batch sort**: used `replaceObjectAtIndex(..., false)`, which retains every child on each sort (reference leak), with no speed gain.
* **Particle draw hook on `CCParticleSystem`**: that class has no `draw` binding, so the hook never ran. Now hooked on `CCParticleSystemQuad`.
* **Motion streak culling**: tested the node origin against the screen, which hides trails that live in a scrolling layer.
* **Fast level exit**: walked every node of the level on exit and skipped the save on quit (risk of lost progress).
* **Anti aliasing** (log only), **Force RGBA8888** (not an optimization), **fast pickup** (only a log line), **early-load** (not needed).

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) (qimiko / mat, MIT) and [Algebra Dash](https://github.com/cgytrus/AlgebraDash) (ConfiG, MIT).
