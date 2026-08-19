# VisibilityContours for Stellarium

Custom Stellarium plugin that draws four crescent-visibility contours around the Sun:

- V = 1.30
- V = 2.00
- V = 3.50
- V = 5.65

## Geometry

The plugin uses geometric (unrefracted) topocentric Alt/Az for the Sun and draws each contour in the same Alt/Az frame.

For each delta-azimuth DAZ, it solves by bisection for ARCV in `[-20°, +40°]`:

```text
V = ARCV - (-0.1018 W^3 + 0.7319 W^2 - 6.3226 W + 7.1651)
W = 15 * (1 - cos(ARCV) * cos(DAZ))   [arcmin]
```

Angles are degrees except inside trig functions.

The sky point then is:

```text
azimuth  = Sun azimuth  + DAZ
altitude = Sun altitude + ARCV
```

No-root DAZ samples are omitted rather than clamped to an endpoint.

## When contours are visible

The overlay is drawn only when:

1. Observer is on Earth.
2. The geometric altitude of the Sun's center is below `-0.8333°` (conventional sunrise/sunset approximation).
3. The time belongs to one of the five nearest-integer 24-hour bins around the nearest geocentric longitude conjunction:
   `day -2, -1, 0, +1, +2`.

The conjunction is found numerically from Stellarium's own Moon/Earth ecliptic position functions at arbitrary JDE; the plugin does not change the Stellarium clock while searching.

The on-sky status label shows the phase-day bin and the continuous time difference from conjunction in days.

## Why this is a plugin, not an .ssc script

Stellarium's public scripting API does not expose the arbitrary celestial-sphere polyline drawing needed for these contours. A compiled plugin can use `StelPainter` and the Alt/Az projector directly.

## Install the prebuilt plugin

The prebuilt Linux plugin has been tested with Stellarium 26.2 on Fedora 44
x86_64 using Qt 6.11.1. Stellarium plugins are ABI-sensitive; if Stellarium
cannot load this binary, use the source-build instructions below against your
exact Stellarium installation.

Install the latest release for your user account:

```bash
mkdir -p "$HOME/.stellarium/modules/VisibilityContours"
curl -fL \
  https://github.com/qlifee/VisibilityContours/releases/latest/download/libVisibilityContours.so \
  -o "$HOME/.stellarium/modules/VisibilityContours/libVisibilityContours.so"
```

Restart Stellarium after installation. The plugin starts automatically by
default. If it does not, open **Configuration window → Plugins → Visibility
Contours**, select **Load at startup**, and restart Stellarium again.

To check the downloaded file against the checksum published in the release:

```bash
sha256sum "$HOME/.stellarium/modules/VisibilityContours/libVisibilityContours.so"
```

To update, run the download command again. To uninstall:

```bash
rm "$HOME/.stellarium/modules/VisibilityContours/libVisibilityContours.so"
rmdir "$HOME/.stellarium/modules/VisibilityContours"
```

Release downloads and compatibility notes are available on the
[GitHub Releases page](https://github.com/qlifee/VisibilityContours/releases).

## Build from source

VisibilityContours is a standalone dynamic plugin. It requires the source and a
configured build tree for the exact Stellarium 26.2 / Qt 6 installation with
which it will be used, but it does not modify or rebuild Stellarium.

Configure and build it out of tree:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DSTELLARIUM_SOURCE_DIR=/path/to/stellarium-26.2 \
  -DSTELLARIUM_BUILD_DIR=/path/to/stellarium-26.2/build
cmake --build build --parallel
```

Stage the installation before installing it for the current user:

```bash
cmake --install build --prefix /tmp/visibility-contours-stage
cmake --install build --prefix "$HOME/.stellarium"
```

On Linux the installed plugin is
`~/.stellarium/modules/VisibilityContours/libVisibilityContours.so`.
Stellarium resolves the plugin's Stellarium API symbols when it loads the
module. The plugin must therefore be rebuilt for ABI-incompatible Stellarium,
Qt, or compiler versions.

## Easy constants to change

At the top of `src/VisibilityContours.cpp` you can change:

- `SUNSET_CENTER_ALT_DEG`
- `ARCV_MIN_DEG`, `ARCV_MAX_DEG`
- `DAZ_MIN_DEG`, `DAZ_MAX_DEG`, `DAZ_STEP_DEG`
- the four contour V values and their RGB colors
- line width

## Version target

Source written against the Stellarium 26.x / 26.2-era plugin APIs (StelPainter, SolarSystem, Planet, StelCore). Build it against the same source tag as your installed Stellarium.
