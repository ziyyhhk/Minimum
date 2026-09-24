# Minimum

**Algebra Dash mod sequel** — a modern general optimization mod for Geometry Dash.

This is a spiritual successor / updated continuation of [cgytrus/AlgebraDash](https://github.com/cgytrus/AlgebraDash) (the original "Algebra is simpler and faster than geometry" optimization mod). The old mod no longer works on current Geometry Dash + Geode, so this repo modernizes the idea and structure for newer versions.

## Features (goals / inherited)

- Faster / near-instant loading
- Reduced lagspikes when enabling/disabling large numbers of objects
- Rendering multithreading / batch improvements
- Quality-of-life improvements (anti-aliasing option, fast alt-tab, force RGBA8888)
- Tracy profiler integration (for further optimization work)
- More optimizations planned / being ported to current Geode + GD 2.208+

> **Note:** Many of the original low-level hooks and OpenGL/path hacks were written for much older Geode and GD versions. They need address/binding updates and testing on current 2.2081 + Geode 5.x before they fully work. This repo provides the updated project structure, mod.json, and base so the community (or you) can continue the work.

## Requirements

- Geometry Dash 2.208 / 2.2081 (or newer when supported)
- [Geode](https://geode-sdk.org/) mod loader (v4+ / v5 recommended)

## Installation

1. Install Geode for your platform from https://geode-sdk.org
2. Build this mod (see below) or download a release `.geode` when available
3. Place the `.geode` file in your Geode mods folder
4. Launch Geometry Dash

## Building

```bash
# Make sure GEODE_SDK points to your Geode SDK clone
export GEODE_SDK=/path/to/geode

# Configure & build (example)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the official Geode CLI / GitHub Actions workflow.

## Credits

- Original AlgebraDash by **ConfiG / cgytrus** — https://github.com/cgytrus/AlgebraDash
- Geode team for the modern modding framework
- Everyone who keeps GD modding alive

## License

MIT (same as the original AlgebraDash)

---

*algebra was simpler... minimum aims to be even faster.*
