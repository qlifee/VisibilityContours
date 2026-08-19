#include "VisibilityContours.hpp"

#include "Planet.hpp"
#include "SolarSystem.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelUtils.hpp"

#include <QDebug>
#include <QFont>
#include <QString>

#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;

// Standard geometric altitude of the Sun's center at conventional sunrise/sunset.
// Change to 0.0 if you want the mathematical center crossing instead.
constexpr double SUNSET_CENTER_ALT_DEG = -0.8333;

// The user's requested inversion interval.
constexpr double ARCV_MIN_DEG = -20.0;
constexpr double ARCV_MAX_DEG =  40.0;

// Contours are sampled in delta-azimuth. Roots naturally disappear near |DAZ|~50 deg.
constexpr double DAZ_MIN_DEG = -55.0;
constexpr double DAZ_MAX_DEG =  55.0;
constexpr double DAZ_STEP_DEG = 0.25;

struct ContourStyle
{
    double v;
    float r;
    float g;
    float b;
};

constexpr std::array<ContourStyle, 4> CONTOURS = {{
    {1.30, 0.00f, 0.45f, 1.00f}, // blue
    {2.00, 1.00f, 0.00f, 1.00f}, // magenta
    {3.50, 1.00f, 1.00f, 0.00f}, // yellow
    {5.65, 0.00f, 1.00f, 0.00f}  // green
}};

double wrapPi(double x)
{
    while (x <= -PI) x += 2.0 * PI;
    while (x >   PI) x -= 2.0 * PI;
    return x;
}

// Odeh visibility value in the form requested for this overlay:
//   V = ARCV - (-0.1018 W^3 + 0.7319 W^2 - 6.3226 W + 7.1651)
//   W = 15 * (1 - cos(ARCV) * cos(DAZ))   [arcmin]
// ARCV and DAZ are degrees; trig functions use radians.
double odehV(double arcvDeg, double dazDeg)
{
    const double arcv = arcvDeg * DEG2RAD;
    const double daz  = dazDeg  * DEG2RAD;
    const double W = 15.0 * (1.0 - std::cos(arcv) * std::cos(daz));
    const double threshold = -0.1018 * W * W * W
                           +  0.7319 * W * W
                           -  6.3226 * W
                           +  7.1651;
    return arcvDeg - threshold;
}

bool solveArcv(double targetV, double dazDeg, double& arcvDeg)
{
    double lo = ARCV_MIN_DEG;
    double hi = ARCV_MAX_DEG;
    double flo = odehV(lo, dazDeg) - targetV;
    double fhi = odehV(hi, dazDeg) - targetV;

    if (std::abs(flo) < 1e-12) { arcvDeg = lo; return true; }
    if (std::abs(fhi) < 1e-12) { arcvDeg = hi; return true; }
    if (flo * fhi > 0.0)
        return false; // No root in the requested interval: do not force an endpoint.

    for (int i = 0; i < 70; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        const double fm = odehV(mid, dazDeg) - targetV;
        if (std::abs(fm) < 1e-11)
        {
            arcvDeg = mid;
            return true;
        }
        if (flo * fm <= 0.0)
        {
            hi = mid;
            fhi = fm;
        }
        else
        {
            lo = mid;
            flo = fm;
        }
    }

    arcvDeg = 0.5 * (lo + hi);
    return true;
}

// Geocentric Moon-Sun ecliptic-longitude difference at arbitrary JDE, in [-pi,+pi].
// Moon position is Earth-centered; the geocentric Sun direction is opposite the
// heliocentric Earth vector. Both are in Stellarium's VSOP87/J2000-like ecliptic frame.
double moonSunLongitudeDifference(const PlanetP& moon, const PlanetP& earth, double jde)
{
    const Vec3d moonGeo = moon->getEclipticPos(jde);
    const Vec3d earthHelio = earth->getEclipticPos(jde);

    const double lambdaMoon = std::atan2(moonGeo[1], moonGeo[0]);
    const double lambdaSun  = std::atan2(-earthHelio[1], -earthHelio[0]);
    return wrapPi(lambdaMoon - lambdaSun);
}

// Find the nearest new-moon longitude conjunction by bracketing a zero of Δlambda.
// This does not alter Stellarium's clock or the currently displayed ephemeris state.
bool findNearestConjunction(const PlanetP& moon, const PlanetP& earth,
                            double currentJDE, double& conjunctionJDE)
{
    constexpr double SEARCH_HALF_SPAN_DAYS = 4.0;
    constexpr double SCAN_STEP_DAYS = 0.25;

    bool found = false;
    double bestDistance = std::numeric_limits<double>::infinity();
    double bestLo = 0.0;
    double bestHi = 0.0;

    double t0 = currentJDE - SEARCH_HALF_SPAN_DAYS;
    double f0 = moonSunLongitudeDifference(moon, earth, t0);

    for (double t1 = t0 + SCAN_STEP_DAYS;
         t1 <= currentJDE + SEARCH_HALF_SPAN_DAYS + 1e-9;
         t1 += SCAN_STEP_DAYS)
    {
        const double f1 = moonSunLongitudeDifference(moon, earth, t1);

        // A genuine conjunction crosses zero smoothly. Ignore the ±pi discontinuity
        // at full moon, where the wrapped value jumps by almost 2*pi.
        if (std::abs(f1 - f0) < PI && f0 * f1 <= 0.0)
        {
            const double midpoint = 0.5 * (t0 + t1);
            const double distance = std::abs(midpoint - currentJDE);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestLo = t0;
                bestHi = t1;
                found = true;
            }
        }

        t0 = t1;
        f0 = f1;
    }

    if (!found)
        return false;

    double lo = bestLo;
    double hi = bestHi;
    double flo = moonSunLongitudeDifference(moon, earth, lo);

    for (int i = 0; i < 70; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        const double fm = moonSunLongitudeDifference(moon, earth, mid);
        if (std::abs(fm) < 1e-13)
        {
            conjunctionJDE = mid;
            return true;
        }
        if (flo * fm <= 0.0)
        {
            hi = mid;
        }
        else
        {
            lo = mid;
            flo = fm;
        }
    }

    conjunctionJDE = 0.5 * (lo + hi);
    return true;
}

Vec3d altAzVector(double azRad, double altRad)
{
    Vec3d v;
    StelUtils::spheToRect(azRad, altRad, v);
    return v;
}

} // namespace

StelModule* VisibilityContoursStelPluginInterface::getStelModule() const
{
    return new VisibilityContours();
}

StelPluginInfo VisibilityContoursStelPluginInterface::getPluginInfo() const
{
    StelPluginInfo info;
    info.id = "VisibilityContours";
    info.displayedName = "Visibility Contours";
    info.authors = "Custom plugin for Stellarium";
    info.contact = "";
    info.description = "Draws V=1.30, 2.00, 3.50 and 5.65 Odeh visibility contours around the Sun near lunar conjunction.";
    info.version = VISIBILITYCONTOURS_PLUGIN_VERSION;
    info.license = VISIBILITYCONTOURS_PLUGIN_LICENSE;
    info.startByDefault = true;
    return info;
}

VisibilityContours::VisibilityContours()
    : cachedForJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedConjunctionJDE(std::numeric_limits<double>::quiet_NaN())
{
    setObjectName("VisibilityContours");
}

VisibilityContours::~VisibilityContours() = default;

void VisibilityContours::init()
{
    qInfo() << "VisibilityContours initialized";
}

double VisibilityContours::getCallOrder(StelModuleActionName actionName) const
{
    if (actionName == StelModule::ActionDraw)
    {
        StelModule* nebula = StelApp::getInstance().getModuleMgr().getModule("NebulaMgr");
        return nebula ? nebula->getCallOrder(actionName) + 10.0 : 10.0;
    }
    return 0.0;
}

void VisibilityContours::draw(StelCore* core)
{
    if (!core)
        return;

    // Criterion/conjunction logic here is Earth-specific.
    const PlanetP currentPlanet = core->getCurrentPlanet();
    if (!currentPlanet || currentPlanet->getEnglishName() != "Earth")
        return;

    SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
    if (!solarSystem)
        return;

    const PlanetP sun = solarSystem->getSun();
    const PlanetP moon = solarSystem->getMoon();
    const PlanetP earth = solarSystem->getEarth();
    if (!sun || !moon || !earth)
        return;

    // Geometric/airless Sun altitude. The -0.8333 deg gate corresponds approximately
    // to conventional observed sunrise/sunset of the upper solar limb.
    Vec3d sunAltAz = sun->getAltAzPosGeometric(core);
    sunAltAz.normalize();
    double sunAzRad = 0.0;
    double sunAltRad = 0.0;
    StelUtils::rectToSphe(&sunAzRad, &sunAltRad, sunAltAz);
    const double sunAltDeg = sunAltRad * RAD2DEG;
    if (sunAltDeg > SUNSET_CENTER_ALT_DEG)
        return;

    // Determine true time offset from the nearest geocentric longitude conjunction,
    // using Stellarium's position functions at arbitrary JDE.
    const double jde = core->getJDE();
    if (!std::isfinite(cachedForJDE) || std::abs(jde - cachedForJDE) > 0.20)
    {
        double conjunction = 0.0;
        if (findNearestConjunction(moon, earth, jde, conjunction))
            cachedConjunctionJDE = conjunction;
        else
            cachedConjunctionJDE = std::numeric_limits<double>::quiet_NaN();
        cachedForJDE = jde;
    }

    if (!std::isfinite(cachedConjunctionJDE))
        return;

    const double daysFromConjunction = jde - cachedConjunctionJDE;
    const int dayIndex = static_cast<int>(std::lround(daysFromConjunction));

    // Five requested phase-day bins only: -2, -1, 0, +1, +2.
    if (dayIndex < -2 || dayIndex > 2)
        return;

    const StelProjectorP projector = core->getProjection(StelCore::FrameAltAz,
                                                          StelCore::RefractionOff);
    StelPainter painter(projector);
    painter.setBlending(true);
    painter.setLineSmooth(true);
    painter.setLineWidth(2.2f);
    painter.setFont(QFont("Sans Serif", 11));

    for (const ContourStyle& contour : CONTOURS)
    {
        painter.setColor(contour.r, contour.g, contour.b, 0.95f);

        bool havePreviousSample = false;
        double previousDazDeg = 0.0;
        double previousAltDeg = 0.0;
        Vec3d previous;

        for (double dazDeg = DAZ_MIN_DEG;
             dazDeg <= DAZ_MAX_DEG + 1e-9;
             dazDeg += DAZ_STEP_DEG)
        {
            double arcvDeg = 0.0;
            if (!solveArcv(contour.v, dazDeg, arcvDeg))
            {
                havePreviousSample = false;
                continue;
            }

            // Odeh ARCV is airless/topocentric.
            // Therefore use geometric altitude, without refraction.
            const double altDeg = sunAltDeg + arcvDeg;

            if (altDeg <= -90.0 || altDeg >= 90.0)
            {
                havePreviousSample = false;
                continue;
            }

            const double azRad = sunAzRad + dazDeg * DEG2RAD;
            const Vec3d current =
                altAzVector(azRad, altDeg * DEG2RAD);

            if (havePreviousSample)
            {
                const bool previousAboveHorizon =
                    previousAltDeg >= 0.0;
                const bool currentAboveHorizon =
                    altDeg >= 0.0;

                if (previousAboveHorizon && currentAboveHorizon)
                {
                    painter.drawGreatCircleArc(previous, current);
                }
                else if (previousAboveHorizon != currentAboveHorizon)
                {
                    // Find where this short segment crosses
                    // the geometric horizon: altitude = 0 deg.
                    const double denominator =
                        altDeg - previousAltDeg;

                    if (std::abs(denominator) > 1e-12)
                    {
                        double t =
                            -previousAltDeg / denominator;

                        if (t < 0.0) t = 0.0;
                        if (t > 1.0) t = 1.0;

                        const double horizonDazDeg =
                            previousDazDeg +
                            t * (dazDeg - previousDazDeg);

                        const Vec3d horizonPoint =
                            altAzVector(
                                sunAzRad +
                                horizonDazDeg * DEG2RAD,
                                0.0);

                        if (previousAboveHorizon)
                            painter.drawGreatCircleArc(
                                previous, horizonPoint);
                        else
                            painter.drawGreatCircleArc(
                                horizonPoint, current);
                    }
                }
            }

            previous = current;
            previousDazDeg = dazDeg;
            previousAltDeg = altDeg;
            havePreviousSample = true;
        }

        // Put one label on each contour near DAZ=+20 deg.
        double labelArcvDeg = 0.0;
        if (solveArcv(contour.v, 20.0, labelArcvDeg))
        {
            const double labelAltDeg = sunAltDeg + labelArcvDeg;
            if (labelAltDeg >= 0.0 && labelAltDeg < 90.0)
            {
                const Vec3d labelPos = altAzVector(sunAzRad + 20.0 * DEG2RAD,
                                                    labelAltDeg * DEG2RAD);
                painter.drawText(labelPos,
                                 QString("V=%1").arg(contour.v, 0, 'f', 2),
                                 0.0f, 5.0f, 4.0f, true);
            }
        }
    }

    // Status label close to the Sun; useful for checking the automatic day filter.
    painter.setColor(1.0f, 1.0f, 1.0f, 0.90f);
    painter.drawText(sunAltAz,
                     QString("Odeh day %1   Δ=%2 d")
                         .arg(dayIndex >= 0 ? QString("+%1").arg(dayIndex)
                                            : QString::number(dayIndex))
                         .arg(daysFromConjunction, 0, 'f', 2),
                     0.0f, 10.0f, -18.0f, true);
}
