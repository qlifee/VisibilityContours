[English](README.md) | [العربية](README.ar.md)

# Crescent Visibility & Hijri Date for Stellarium

Standalone Stellarium plugin that draws selectable crescent-visibility bands
around the Sun and adds observational visibility information when the Moon is
selected.

- Current version: **0.7.0**

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

Open **Configuration window → Plugins → Crescent Visibility & Hijri Date →
Configure** to
select Odeh or Yallop contours, control translucent band fills, and show the
floating Moon Navigator. Band fills are enabled for new profiles; an explicitly
saved fill setting and the navigator setting persist across restarts. The
Moon Navigator event filter also persists and defaults to Both when it has not
been saved. The criterion resets to Yallop each time the plugin starts. The main
plugin page contains the calculation note and reference links. Select the Moon
to see `V now`, the nearest valid morning/evening best time, `V at best time`,
and the observational `Hijri date` in Stellarium's normal object-information
panel.

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
3. The time belongs to one of the seven nearest-integer 24-hour bins around the nearest apparent geocentric longitude conjunction:
   `day -3, -2, -1, 0, +1, +2, +3`.

The conjunction is the instant when the apparent Sun and Moon ecliptic
longitudes are equal, in the true ecliptic and equinox of date. It is not the
instant of minimum angular separation. The calculation includes light-time,
aberration, and—only for the separately reported topocentric
conjunction—topocentric parallax and diurnal aberration. Atmospheric refraction
is excluded. The plugin evaluates Stellarium positions at arbitrary JDE and
restores the active clock and coordinate settings after each search.

The on-sky `Age from (Geo) Conjunction` label shows the signed continuous time
from conjunction, rounded to the nearest minute as total hours and minutes,
for example `+18h29m` or `-42h07m`. Contour V labels are displayed to two
decimal places.

## Best time and observational V

The information-panel V uses geometric topocentric Sun/Moon altitudes and the
actual illuminated crescent width derived from the Sun-Moon-observer geometry
and topocentric apparent lunar diameter. Atmospheric refraction is not used.

Evening best time is sunset plus `4/9` of the positive Moon-set lag. Morning
best time is sunrise minus `4/9` of the positive Moon-rise lag. The plugin
calculates both valid candidates and displays the one nearest the current
Stellarium clock, in the observer's local civil time.
The information panel shows time only, formatted like `17h55m15s`.
Its seconds follow the same containing-second convention as Stellarium's
bottom clock. `V now` and `V at best time` use the same airless geometric
topocentric calculation, so they agree when evaluated at the same instant.

If the selected Moon is at or below the airless geometric horizon, or the
current time is outside conjunction-day bins `-3` through `+3`, all three
plugin rows display `-` instead of observational values or a best time.

The Hijri-date row is calculated independently of the three `-3…+3`-gated
observational rows. It advances at local sunset and follows two visibility
tracks: a calculated track using `V ≥ 1.35` and an observed track using
`V ≥ 5.83`. When both tracks agree it displays one date, for example
`Hijri date: 01/04/1448`. When they differ it displays the calculated date
first, for example `Hijri date: 01/04/1448 - 30/03/1448`.

For each numerical lunation and each threshold, the first qualifying valid
evening best-time event is used. The Hijri-only post-conjunction search window
adapts to absolute observer latitude:

- `|latitude| ≤ 45°`: bins `0…+3`;
- `45° < |latitude| < 59°`: bins `0…+4`;
- `59° ≤ |latitude| ≤ 60°`: bins `0…+5`.

These wider windows do not change the plugin's separate `-3…+3` contour and
Moon-parameter windows. The Navigator's transition-only search is described
below. Each displayed Hijri month is constrained
to 29 or 30 days. A qualifying crossing after 29 or 30 completed days begins
the new month at that sunset. An earlier crossing is recorded, but the start is
deferred until the sunset completing 29 days. When this happens on the lower
`V ≥ 1.35` track, the resulting month carries the warning
`Possible premature start`. If no qualifying event occurs, the track is forced
to the next month after 30 completed days. Missing Moonset,
nonpositive lag, a Moon-down best time, or invalid geometry counts as an unmet
criterion rather than making the calendar unavailable.

The plugin starts from the immediately preceding apparent geocentric
conjunction and lazily searches backward for synchronization anchors for both
tracks, stopping after at most nine numerical lunations. It then replays the
cached groups chronologically to the current sunset. The cache is specific to
the active lunation, latitude, longitude, altitude, and timezone. If the
required anchors or real sunset history cannot be constructed, the row displays
`Hijri date: Not available`.

The latitude display policy is:

- `|latitude| ≤ 55°`: show the numeric observational date normally;
- `55° < |latitude| ≤ 60°`: show the numeric date followed by
  `Follow date of lower latitude.`;
- `|latitude| > 60°`: do not calculate or substitute a proxy date; show
  `Hijri date: Not available; follow date of lower latitude`.

Sunrise and sunset use a geometric Sun-center altitude of `-0.8333°`, the
conventional upper-limb sunrise/sunset threshold. Moonrise, Moonset, and the
Moon-up test use the airless geometric center horizon at `0°`. The contour
gate uses the same `-0.8333°` solar threshold, so contours are available at
valid calculated best times even when the Moon-set or Moon-rise lag is short.

## Moon visibility parameters

The observational Hijri date remains above this section. Under the **Moon
visibility parameters** heading, the selected Moon's information panel reports
`V now`, `Best time`, and `V at best time`, followed by:

- illuminated crescent `Width W` in arcseconds;
- geometric topocentric `ARCV` and signed `DAZ`;
- the one relevant signed lag for the displayed nearest best-time event:
  morning (`sunrise − Moonrise`) or evening (`Moonset − sunset`);
- signed ages from the apparent geocentric and topocentric conjunctions; and
- `ΔT (TT−UT1)` in seconds.

W remains in arcminutes internally in the Odeh equation and is converted only
for display. ARCV is geometric Moon altitude minus geometric Sun altitude; DAZ
is the signed geometric Moon–Sun azimuth difference in `−180°…+180°`.
Conventional solar events use `−0.8333°`, lunar events use the geometric center
horizon at `0°`, and neither uses atmospheric refraction. Width, both
conjunction ages, and ΔT are shown whenever calculable. ARCV, DAZ, and the
relevant lag display `-` when the Moon is down or the time is outside bins
`-3…+3`.
Stellarium's existing magnitude, altitude, elongation, and illuminated-fraction
rows are not duplicated. The displayed ΔT is Stellarium's active time-correction
model, which is also used to relate TT to UT1 for the topocentric conjunction.

## Moon Navigator

Enable **Show Moon navigator** in the plugin configuration to open a movable
panel with two previous/next button rows. The configuration window is also
movable, and both windows remember their positions.

The global **Navigate: Morning | Evening | Both** radio selector applies to
both button rows. **Both** retains chronological traversal of the selected
visibility landmarks, while **Morning** or **Evening** skips every landmark of
the other kind. Changing the selector does not move Stellarium immediately;
the next arrow click begins from the current Stellarium time. The selection is
saved as `event_filter=both|morning|evening`, with **Both** used for a missing
or invalid setting.

The Navigator uses the fixed observational Odeh thresholds `1.35` and `5.83`,
independently of the contour preset. It keeps two or three category landmarks
for each morning or evening crescent associated with a conjunction:

- Evening: the event immediately before the first upward crossing
  (`V < 1.35`), the first event at or above `1.35`, and the first event at or
  above `5.83`.
- Morning: the event immediately before the final downward crossing nearest
  conjunction (`V >= 5.83`), the first event below `5.83`, and the first event
  below `1.35`.

If one daily step crosses both thresholds, the two crossing landmarks are the
same event and are shown only once, leaving two stops instead of three.
If a complete bracketed sequence cannot be calculated, that morning or evening
crescent is skipped rather than inventing a category landmark.

- **Only Moon up** visits valid `4/9`-lag best times for which the Moon is
  strictly above the airless geometric horizon.
- **Moon up or down** also classifies a genuine positive-lag best time when the
  Moon is geometrically down. It then visits the corresponding conventional
  sunrise or sunset, and the Moon-information V rows remain `-`. An evening
  sunset fallback lands five seconds after the calculated sunset so the
  sunset-based Hijri date has already advanced.

Threshold classification always comes from a genuine `4/9` best time with a
positive Moon lag, finite airless topocentric Odeh V, the correct side of
conjunction, and illuminated fraction no greater than one half. Invalid
rise/set pairs, nonpositive Moon lags, polar failures, and missing solar events
are skipped; no V is invented at sunrise or sunset.

Forward selects the first landmark strictly after the current Stellarium time;
Back selects the first strictly before it. To find rare high-latitude
crossings, the Navigator alone examines up to ten rounded conjunction days on
the relevant crescent side, but exposes only the two or three category
landmarks above. The `-3…+3` gates used by contours and Moon information are
unchanged.

If no qualifying landmark remains in the current lunation, navigation
continues into the adjacent lunation. At a selected event Stellarium pauses,
selects and centers the Moon, enables tracking, and preserves the current field
of view. The panel reports the observer-local Gregorian or Julian date, with
the observational Hijri date on a separate line beneath it. Both lines are
bold; the event time remains
visible in Stellarium's bottom clock. Navigator titles, labels, dates, and
radio options are white for contrast, while button arrows and tooltips retain
Stellarium's styling. A successful jump enables and saves Stellarium's
standard selected-object marker and Solar System planet pointers, so the
four-part rotating marker appears around the selected Moon. Navigation is
available only for observers on Earth. Closing the panel disables its saved
configuration checkbox. The pre-threshold landmark provides context and does
not itself assert crescent visibility.

After navigation, the panel also shows a bold Hijri month/year heading at 150%
of the application font size. Morning events use the format
`End of Rabi' al-Awwal 1448 AH`, while evening events use
`Beginning of Rabi' al-Akhir 1448 AH`. In Arabic, the corresponding formats are
`آخر ربيع الأول 1448 هـ` and `غرة ربيع الآخر 1448 هـ`.

The two bold date lines are explicitly labelled `Gregorian date:` or `Julian
date:` according to Stellarium's historical calendar switch at 1582-10-15,
and `Hijri date:`. Arabic uses right-to-left labelled lines while preserving
the numeric dates in left-to-right order. The Navigator shows the same
lower-latitude and possible-premature-start warnings as the Moon information
panel. Above 60°, its arithmetic **Beginning/End of [Hijri month]** heading
remains as event context, while the observational date is explicitly
unavailable.

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
default. If it does not, open **Configuration window → Plugins → Crescent
Visibility & Hijri Date**, select **Load at startup**, and restart Stellarium
again.

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

The macOS v0.7.0 workflow targets only the unmodified official universal
`Stellarium-26.2-qt6-macOS.zip` application from stellarium.org. Its matching
build inputs are Qt 6.9.3, Xcode 26.5 with Apple Clang 21, a macOS 12.0
deployment target, and both `arm64` and `x86_64` slices. Qt5, Homebrew, and
other third-party Stellarium packages are not supported by this binary.

The workflow verifies both architecture slices, deployment target, Qt plugin
metadata, `@rpath` dependencies, unresolved Stellarium symbols, the official
host's exported symbols, and its code-signing entitlements. The v0.7.0
universal asset is CI built and inspected, Developer ID signed, and notarized
by Apple. This version has not been runtime tested on macOS; both the `arm64`
and `x86_64` slices are universal-binary and CI inspected only. Linux is the
runtime-tested platform for v0.7.0.

GitHub Actions also produces ad-hoc-signed acceptance artifacts for
maintainers. Those test artifacts are not normal end-user downloads; install
the signed and notarized asset from the Releases page.

Install v0.7.0 for the current user with:

```bash
download_dir="$HOME/Downloads/VisibilityContours-0.7.0-macOS"
mkdir -p "$download_dir"
curl -fL \
  https://github.com/qlifee/VisibilityContours/releases/download/v0.7.0/VisibilityContours-0.7.0-Stellarium-26.2-macOS-universal.zip \
  -o "$download_dir/VisibilityContours-0.7.0-Stellarium-26.2-macOS-universal.zip"
ditto -x -k \
  "$download_dir/VisibilityContours-0.7.0-Stellarium-26.2-macOS-universal.zip" \
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
