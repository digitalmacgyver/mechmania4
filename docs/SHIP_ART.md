Ship Art Customization
======================

The observer can now display themed ship sprites per team. This page explains
how to prepare the assets, enable them at runtime, and package the files for
distribution.

Directory layout
----------------

Custom PNG strips live under:

```
assets/star_control/graphics/<FNAME>/<SNAME>/<SNAME>.big.<0-15>.png
```

* `<FNAME>` groups related ships (faction, mod family, etc.).
* `<SNAME>` is the short identifier referenced from the CLI (e.g. `arilou`).
* Each strip contains sixteen 32×32 (recommended) PNGs covering evenly spaced
  orientations. `*.big.0.png` should correspond to the ship facing “up”
  (π/2 radians). Frames continue counter-clockwise.

All sixteen frames must exist. If any are missing or fail to load the observer
logs a warning and falls back to the legacy Team 1/Team 2 sprite sheet so
matches are never blocked by broken art.

Enabling a sprite set
---------------------

Every team binary, including `mm4team` and the compiled variants in `teams/`,
accepts a new flag:

```
./mm4team --ship-art <SNAME>
./mm4team --ship-art <FNAME:SNAME>   # Explicit faction directory
```

* `SNAME` alone looks for `assets/star_control/graphics/<SNAME>/<SNAME>/...`.
* `FNAME:SNAME` lets you reuse a sprite housed under a different faction.
* The observer mixes art per team; one side can use default sprites while the
  other uses a custom pack.
* Custom art is applied to the base ship sprite for all thrust/turn states. The
  existing damage/shield overlays still render on top.
* When no flag is supplied the client randomly picks from the installed packs,
  including the two original legacy sprite sets. The special
  `yehat/shield` helper art is excluded from random rotation because it is used
  exclusively for shield overlays.

Runtime search order
--------------------

The observer resolves art relative to several roots, in this order:

1. The build tree (symlinked during `cmake --build`).
2. The source tree (`assets/star_control/graphics`).
3. An installed prefix (`<prefix>/share/mm4/assets/star_control/graphics`).

This mirrors how `graphics.reg` and fonts are located, keeping portable builds
and packages working without extra environment variables.

Packaging guidelines
--------------------

* When creating a release bundle, include the full
  `assets/star_control/graphics` directory (or at minimum the sub-folders your
  tournament uses). CMake’s `install` target now copies the graphics tree to
  `share/mm4/assets/star_control/graphics`, so `cpack`/`make install` users get
  the assets automatically.
* Third-party teams can ship their sprites by adding the appropriate directory
  tree beneath `assets/star_control/graphics` in their submission archive.
* Files should use standard PNG encoding with transparency (RGBA). Avoid
  premultiplied alpha—SDL handles straight alpha well.

Troubleshooting
---------------

* A missing sprite prints (once per team) a message similar to:
  `SpriteManager: custom ship art not found or incomplete for arilou`.
* Run the observer with `--verbose` to see additional loader output.
* If assets are installed in a non-standard location, set `SDL_GetBasePath`
  by launching from the directory that contains the binaries plus the
  `assets/` tree, or extend the source tree search path.
