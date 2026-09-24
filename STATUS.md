# Current Status of Minimum (Algebra Dash Sequel)

## What is done

- New GitHub repository: https://github.com/ziyyhhk/Minimum
- Modern `mod.json` targeting Geode 4+ / GD 2.2081
- Updated README describing this as the Algebra Dash mod sequel
- CMakeLists.txt skeleton for current Geode SDK
- Basic improvement stubs (fast alt-tab, anti-aliasing setting, force RGBA8888)
- Project structure matching the original AlgebraDash
- MIT license with credit to original author

## What still needs work (important)

The original AlgebraDash contained deep, version-specific optimizations:

- `parallel_loading.cpp` (~10k lines of texture loading hacks)
- `update_visibility_rewrite.cpp` (~21k)
- `opengl_update.cpp` (~30k)
- `move_actions.cpp` (~13k)
- Tracy profiler integration
- QOI cache, smooth particles, main loop rewrite, etc.

These were written against **old Geode (v1.x)** and **older GD versions**. They will **not** compile or work on current Geode 5.x + GD 2.2081 without:

1. Updating every `geode::Modify` / hook to modern `$modify` or current binding style
2. Re-finding addresses / using current Broma bindings
3. Fixing OpenGL / Cocos changes
4. Testing extensively (many of these touch rendering and loading paths that changed)

## Recommended next steps

1. Clone this repo + the original AlgebraDash side-by-side
2. Port one feature at a time (start with `fast_alt_tab` which is already partially modern)
3. Use Geode docs + current bindings: https://docs.geode-sdk.org
4. Enable Tracy again only after basic hooks work
5. Publish a working `.geode` when at least a few optimizations are stable

Credits remain with **cgytrus / ConfiG** for the original work.
