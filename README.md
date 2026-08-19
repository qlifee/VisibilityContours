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

## Build/install strategy

A Stellarium binary plugin must be ABI-compatible with the exact Stellarium/Qt build you run. The safest first build is therefore **in the matching Stellarium source tree**.

Check your installed version:

```bash
stellarium --version
```

Get the matching Stellarium source version, then run:

```bash
./add_to_stellarium_source.sh /path/to/stellarium-source
```

That copies this directory to `plugins/VisibilityContours` and adds:

```cmake
ADD_PLUGIN(VisibilityContours 1)
```

to Stellarium's root `CMakeLists.txt` immediately after the `SimpleDrawLine` demo plugin registration.

Then configure/build Stellarium normally. For Fedora, use the build dependencies and instructions appropriate to the exact Stellarium release/Qt version you have installed.

## Easy constants to change

At the top of `src/VisibilityContours.cpp` you can change:

- `SUNSET_CENTER_ALT_DEG`
- `ARCV_MIN_DEG`, `ARCV_MAX_DEG`
- `DAZ_MIN_DEG`, `DAZ_MAX_DEG`, `DAZ_STEP_DEG`
- the four contour V values and their RGB colors
- line width

## Version target

Source written against the Stellarium 26.x / 26.2-era plugin APIs (StelPainter, SolarSystem, Planet, StelCore). Build it against the same source tag as your installed Stellarium.
