# Minimum v2.0.6

Advanced live toggle optimization mod for Geometry Dash 2.2081.

## Highlights

- Optional Draw Divide style render FPS (OFF by default)
- Background volume when tabbed out
- Particle and streak culling
- Skip empty batches + faster batch sort
- Fast level enter and exit
- Live Enable Minimum switch

Inspired by [Draw Divide](https://geode-sdk.org/mods/mat.draw-divide) and [Algebra Dash](https://github.com/cgytrus/AlgebraDash).

## Important

**Do not run Draw Divide together with `mat.draw-divide`.** Both hook `CCDirector::drawScene` and will conflict.

Parallel batch transforms were removed in 2.0.6 (they caused crashes because Cocos is not thread-safe).

## Crash fix (2.0.6)

Access violation / Mtx_lock crashes in worker threads are fixed by removing unsafe parallel transforms.
