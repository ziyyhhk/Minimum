# Minimum

Modern general optimization mod for Geometry Dash.

Spiritual sequel to [Algebra Dash](https://github.com/cgytrus/AlgebraDash). The old mod no longer runs on current Geode and GD, so this project brings the idea forward for 2.2081 and Geode 5.x.

## Features

**Fast Pickup**
Faster collectible and counter handling when levels spam items.

**Skip Empty Batches**
Avoids drawing sprite batches that have nothing to show. Less GPU waste on dense layouts.

**Cull Offscreen Particles**
Pauses particle systems far outside the view. Big help against lag spikes from particle heavy levels.

**Fast Alt Tab**
Skips the save on minimize so switching windows feels instant.

**Force RGBA8888**
Loads textures at full color depth for sharper art. Windows only. Reload textures after enabling.

**Anti Aliasing**
Optional MSAA. Windows only. Restart required.

## Install

1. Install [Geode](https://geode-sdk.org)
2. Download the `.geode` from Actions artifacts or a release
3. Drop it in your Geode mods folder
4. Launch Geometry Dash

## Build

```bash
export GEODE_SDK=/path/to/geode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the GitHub Actions workflow.

## Credits

Original Algebra Dash by ConfiG / cgytrus
Geode team
Everyone keeping GD modding alive

## License

MIT
