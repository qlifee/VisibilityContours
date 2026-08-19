# VisibilityContours

## Project purpose

VisibilityContours is a Stellarium plugin for displaying lunar-crescent
visibility geometry and visibility-criterion contours around the Sun.

The project originated as a prototype named OdehContours.

The canonical project name is now:

VisibilityContours

Human-readable Stellarium name:

Visibility Contours

The old name OdehContours should eventually be completely migrated to
VisibilityContours in filenames, C++ classes, plugin IDs, CMake targets,
and documentation, except where "Odeh" refers specifically to Mohammad
Odeh or the Odeh visibility criterion.

## Repository boundaries

The canonical project source is this VisibilityContours directory.

The Stellarium 26.2 source directory that may also appear in the VS Code
workspace is reference/build material.

DO NOT modify the Stellarium source tree unless explicitly requested.

The final plugin must be a standalone DYNAMIC Stellarium plugin.
It must not require users to modify or rebuild Stellarium.

## Development environment

Current development system:

- Fedora Linux 44 KDE
- Stellarium 26.2
- Qt 6
- x86_64

Current Stellarium reference/development tree:

~/Downloads/stellarium-26.2-odeh

Current custom static-plugin test installation:

~/.local/stellarium-odeh

The existing custom Stellarium build is useful as a known-working
prototype/reference. Do not delete it.

## Distribution goal

The final project should build as a standalone dynamic plugin.

Expected Linux deployment:

~/.stellarium/modules/VisibilityContours/libVisibilityContours.so

Equivalent Windows and macOS builds should eventually produce the
appropriate DLL/dylib.

Do not require:

- patching StelApp.cpp
- Q_IMPORT_PLUGIN in Stellarium
- ADD_PLUGIN in Stellarium's root CMakeLists.txt
- recompiling the user's entire Stellarium installation

## Scientific requirements

The current implementation uses the Odeh crescent-visibility criterion:

V = ARCV - (-0.1018 W^3 + 0.7319 W^2 - 6.3226 W + 7.1651)

Current width equation:

W = 15 * (1 - cos(ARCV) * cos(DAZ))

W is in arcminutes.

ARCV used in the Odeh equation MUST be:

- topocentric
- geometric
- airless
- not corrected for atmospheric refraction

Do not silently introduce refraction into ARCV.

DAZ is the Moon-Sun azimuth difference.

ARCV is the Moon-Sun altitude difference.

Calculations and drawing currently use Stellarium geometric Alt/Az with
RefractionOff.

## Contours

Current default contours:

V = 1.30 : blue
V = 2.00 : magenta
V = 3.50 : yellow
V = 5.65 : green

Preserve these defaults unless explicitly requested otherwise.

Contours must not be drawn below the geometric horizon.

A contour crossing altitude 0 degrees should terminate at the geometric
horizon rather than simply drawing below it.

## Conjunction window

Contours are currently restricted to conjunction-day bins:

-2, -1, 0, +1, +2

The prototype finds the nearest Sun-Moon conjunction numerically using
Stellarium ephemeris functions.

Before changing conjunction logic, inspect and understand the existing
implementation.

Do not replace astronomical calculations with rough mean-motion
approximations without explicit approval.

## Ephemeris and time

DE441 and Delta-T are separate concepts.

Do not modify Delta-T as part of unrelated visibility-contour work.

Do not assume that use of DE441 determines the correct ancient Delta-T.

## Migration from OdehContours

The source currently contains names such as:

OdehContours.cpp
OdehContours.hpp
OdehContours
OdehContoursStelPluginInterface

Migrate these carefully to:

VisibilityContours.cpp
VisibilityContours.hpp
VisibilityContours
VisibilityContoursStelPluginInterface

The plugin ID and output library should become:

VisibilityContours
libVisibilityContours.so

The displayed name should be:

Visibility Contours

References specifically to the Odeh criterion should retain the word
"Odeh".

Do not perform blind global replacements of "Odeh", because many
occurrences refer to the scientific criterion rather than the old
project name.

## Development workflow

Before substantial edits:

1. Inspect the existing implementation.
2. Inspect relevant Stellarium 26.2 APIs/examples.
3. Explain the intended change.
4. Make focused changes.
5. Build after changes.
6. Fix actual compiler errors rather than guessing.
7. Preserve scientifically working behavior.
8. Review git diff before committing.

Do not modify unrelated HCal projects.

## Immediate development goal

First establish a clean working baseline.

Then:

1. rename the project from OdehContours to VisibilityContours;
2. convert the static prototype into a standalone dynamic plugin;
3. successfully build it against Stellarium 26.2 / Qt6;
4. install it into the normal Stellarium user modules directory;
5. verify that normal Fedora Stellarium can discover and load it;
6. only afterward work on multi-platform packaging and GitHub Actions.
