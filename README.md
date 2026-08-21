[English](README.md) | [العربية](README.ar.md)

# VisibilityContours for Stellarium

Standalone Stellarium plugin that draws selectable crescent-visibility bands
around the Sun and adds observational visibility information when the Moon is
selected.

- Current version: **0.5.2**

The Odeh scheme uses contiguous categories:

- blue: `-0.96 ≤ V < 2.00`
- magenta: `2.00 ≤ V < 5.65`
- green: `V ≥ 5.65`

The Yallop scheme converts the original q boundaries `-0.232`, `-0.160`,
`-0.014`, and `0.216` to equivalent Odeh V values using the Moon's current
horizontal parallax. This makes its blue, magenta, yellow, and green categories
date-dependent and contiguous.

For both criteria, the rendered green fill extends to `V = 27.00`. This upper
drawing limit gives the filled region a contour-shaped outer edge instead of
ending against the rectangular ARCV/DAZ sampling domain. It does not introduce
an additional visibility category or labeled criterion boundary.

Yallop is selected on every plugin startup, regardless of a previously saved
criterion. Users can switch to Odeh for the current session. All contour labels
and information-panel values are equivalent Odeh V values, including when
Yallop is selected.

Open **Configuration window → Plugins → Visibility Contours → Configure** to
select Odeh or Yallop contours, control translucent band fills, and show the
floating Moon Navigator. Band fills are enabled for new profiles; an explicitly
saved fill setting and the navigator setting persist across restarts. The
Moon Navigator event filter also persists and defaults to Both when it has not
been saved. The criterion resets to Yallop each time the plugin starts. The main
plugin page contains the calculation note and reference links. Select the Moon
to see `V now`, the nearest valid morning/evening best time, and `V at best
time` in Stellarium's normal object-information panel.

When Stellarium's program language is Arabic, plugin-owned dialogs, tooltips,
status text, Moon information, conjunction labels, name, description, and
displayed author use Arabic. Every other program language uses English. Runtime
plugin UI follows language changes immediately; Stellarium's cached Plugins-page
metadata requires a restart after changing the program language. Names and titles within the reference section,
links, Western numerals, and scientific notation remain unchanged.

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
3. The time belongs to one of the seven nearest-integer 24-hour bins around the nearest geocentric longitude conjunction:
   `day -3, -2, -1, 0, +1, +2, +3`.

The conjunction is found numerically from Stellarium's own Moon/Earth ecliptic position functions at arbitrary JDE; the plugin does not change the Stellarium clock while searching.

The on-sky `Conjunction day` status label shows the phase-day bin and the
continuous time difference from conjunction in days. Contour V labels are
displayed to two decimal places.

## Best time and observational V

The information-panel V uses geometric topocentric Sun/Moon altitudes and the
actual illuminated crescent width derived from Stellarium's illuminated
fraction and apparent lunar diameter. Atmospheric refraction is not used.

Evening best time is sunset plus `4/9` of the positive Moon-set lag. Morning
best time is sunrise minus `4/9` of the positive Moon-rise lag. The plugin
calculates both valid candidates and displays the one nearest the current
Stellarium clock, in the observer's local civil time.
The information panel shows time only, formatted like `17h55m15s`.

If the selected Moon is at or below the airless geometric horizon, or the
current time is outside conjunction-day bins `-3` through `+3`, all three
plugin rows display `-` instead of observational values or a best time.

Sunrise and sunset use a geometric Sun-center altitude of `-0.8333°`, the
conventional upper-limb sunrise/sunset threshold. Moonrise, Moonset, and the
Moon-up test use the airless geometric center horizon at `0°`. The contour
gate uses the same `-0.8333°` solar threshold, so contours are available at
valid calculated best times even when the Moon-set or Moon-rise lag is short.

## Moon Navigator

Enable **Show Moon navigator** in the plugin configuration to open a movable
panel with two previous/next button rows. The configuration window is also
movable, and both windows remember their positions.

The global **Navigate: Both | Morning | Evening** radio selector applies to
both button rows. **Both** retains chronological morning/evening traversal,
while **Morning** or **Evening** skips every event of the other kind. Changing
the selector does not move Stellarium immediately; the next arrow click begins
from the current Stellarium time. The selection is saved as
`event_filter=both|morning|evening`, with **Both** used for a missing or invalid
setting.

- **Only Moon up** visits valid `4/9`-lag best times for which the Moon is
  strictly above the airless geometric horizon.
- **Moon up or down** prefers that same best time when it is valid and the Moon
  is up; otherwise it visits the corresponding conventional sunrise or sunset.

Forward selects the first qualifying event strictly after the current
Stellarium time; Back selects the first strictly before it. Candidates are
checked chronologically—morning, then evening, then the next morning—within
conjunction days `-3` through `+3`. Invalid rise/set pairs, nonpositive Moon
lags, polar failures, and missing sunrise/sunset events are skipped as
appropriate for the selected row.

If no qualifying event remains in the current conjunction window, navigation
continues into the adjacent lunation. At a selected event Stellarium pauses,
selects and centers the Moon, enables tracking, and preserves the current field
of view. The panel reports Morning or Evening, the conjunction-day bin, and
the observer-local date and time, and whether the instant is a Best time,
Sunrise, or Sunset. A successful jump also enables and saves Stellarium's
standard selected-object marker and Solar System planet pointers, so the
four-part rotating marker appears around the selected Moon. Navigation is
available only for observers on Earth. Closing the panel disables its saved
configuration checkbox. These are calculated Moon events; visiting one does
not assert that the Moon has already reached a visible-crescent threshold.

After navigation, the panel also shows a compact Hijri month/year heading for
the selected event. Morning events use the format
`End of Rabi' al-Awwal 1448 AH`, while evening events use
`Beginning of Rabi' al-Akhir 1448 AH`. In Arabic, the corresponding formats are
`آخر ربيع الأول 1448 هـ` and `غرة ربيع الآخر 1448 هـ`.

## Calculation references

All contours and displayed V values use the Odeh visibility equation, with
airless geometric topocentric ARCV and topocentric crescent width W. When
Yallop is selected, its q boundaries are converted to equivalent Odeh V
boundaries using the Moon's current horizontal parallax.

- Mohammad Sh. Odeh, [*New Criterion for Lunar Crescent Visibility*](https://doi.org/10.1007/s10686-005-9002-5), *Experimental Astronomy* 18, 39–64.
- B. D. Yallop, [*A Method for Predicting the First Sighting of the New Crescent Moon*](https://assets.admiralty.co.uk/public/documents/2025-08/HMNAO%20Technical%20Notes%20Index.pdf?VersionId=DfKow0usAp5ANPUBA_pWq4.DJ6nbkX2q), NAO Technical Note No. 69, 1997, updated 1998.

## Why this is a plugin, not an .ssc script

Stellarium's public scripting API does not expose the arbitrary celestial-sphere polyline drawing needed for these contours. A compiled plugin can use `StelPainter` and the Alt/Az projector directly.

## Install a prebuilt plugin

Stellarium plugins are ABI-sensitive. Use a binary built for the same
Stellarium release, Qt major/minor version, operating system, and processor
architecture as the host application. A package for another Stellarium build,
including a Qt5 or Homebrew build, may not load even on the same computer.

### Linux

The prebuilt Linux plugin has been tested with Stellarium 26.2 on Fedora 44
x86_64 using Qt 6.11.1. If Stellarium cannot load this binary, use the
source-build instructions below against your exact Stellarium installation.

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

### macOS 12+ — official Stellarium 26.2 Qt6 package

The macOS v0.5.2 workflow targets only the unmodified official universal
`Stellarium-26.2-qt6-macOS.zip` application from stellarium.org. Its matching
build inputs are Qt 6.9.3, Apple Clang 21, a macOS 12.0 deployment target, and
both `arm64` and `x86_64` slices. Qt5, Homebrew, and other third-party
Stellarium packages are not supported by this binary.

The workflow verifies both architecture slices, deployment target, Qt plugin
metadata, `@rpath` dependencies, unresolved Stellarium symbols, the official
host's exported symbols, and its code-signing entitlements. Apple Silicon must
also pass a real runtime test with the official application before the asset is
published. The Intel slice is universal-binary and CI inspected, but is not
runtime tested unless an Intel Mac tester is available.

The ad-hoc-signed GitHub Actions artifact is for acceptance testing only. A
stable macOS download will not be published until it is Developer ID signed,
notarized by Apple, and passes a clean browser-download Gatekeeper test. Do not
treat the workflow artifact as a normal end-user release.

After the signed and notarized asset appears on the Releases page, install it
for the current user with:

```bash
download_dir="$HOME/Downloads/VisibilityContours-0.5.2-macOS"
mkdir -p "$download_dir"
curl -fL \
  https://github.com/qlifee/VisibilityContours/releases/download/v0.5.2/VisibilityContours-0.5.2-Stellarium-26.2-macOS-universal.zip \
  -o "$download_dir/VisibilityContours-0.5.2-Stellarium-26.2-macOS-universal.zip"
ditto -x -k \
  "$download_dir/VisibilityContours-0.5.2-Stellarium-26.2-macOS-universal.zip" \
  "$download_dir/unpacked"
mkdir -p "$HOME/Library/Application Support/Stellarium/modules/VisibilityContours"
install -m 755 \
  "$download_dir/unpacked/VisibilityContours/libVisibilityContours.dylib" \
  "$HOME/Library/Application Support/Stellarium/modules/VisibilityContours/libVisibilityContours.dylib"
```

Restart Stellarium after installation. To remove the macOS plugin:

```bash
rm "$HOME/Library/Application Support/Stellarium/modules/VisibilityContours/libVisibilityContours.dylib"
rmdir "$HOME/Library/Application Support/Stellarium/modules/VisibilityContours"
```

## Build from source

VisibilityContours is a standalone dynamic plugin. It requires the source and a
configured build tree for the exact Stellarium 26.2 / Qt 6 installation with
which it will be used, plus Qt 6 LinguistTools to compile the embedded Arabic
catalog. It does not modify or rebuild Stellarium, and the installed plugin
remains a single shared library.

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

For a universal macOS build, use a universal Qt 6.9.3 SDK and add:

```bash
-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
```

The staged macOS library is
`modules/VisibilityContours/libVisibilityContours.dylib`. Building from source
does not by itself provide Developer ID signing or Apple notarization.

## Easy constants to change

The main drawing constants are easy to adjust:

- `CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG` in `src/VisibilityMath.hpp`
- `ARCV_MIN_DEG`, `ARCV_MAX_DEG` in `src/VisibilityContours.cpp`
- `DAZ_MIN_DEG`, `DAZ_MAX_DEG`, `DAZ_STEP_DEG` in `src/VisibilityContours.cpp`
- the criterion boundary values and RGB colors
- line width

## Version target

Source written against the Stellarium 26.x / 26.2-era plugin APIs (StelPainter, SolarSystem, Planet, StelCore). Build it against the same source tag as your installed Stellarium.
