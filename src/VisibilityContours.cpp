#include "VisibilityContours.hpp"
#include "VisibilityContoursDialog.hpp"
#include "VisibilityMath.hpp"

#include "Planet.hpp"
#include "SolarSystem.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelObserver.hpp"
#include "StelLocation.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelUtils.hpp"

#include <QDebug>
#include <QFont>
#include <QSettings>
#include <QString>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

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

struct Color
{
    float r;
    float g;
    float b;
};

constexpr Color BLUE = {0.00f, 0.45f, 1.00f};
constexpr Color MAGENTA = {1.00f, 0.00f, 1.00f};
constexpr Color YELLOW = {1.00f, 1.00f, 0.00f};
constexpr Color GREEN = {0.00f, 1.00f, 0.00f};

struct VisibilityBand
{
    double lowerV;
    std::optional<double> upperV;
    Color color;
};

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
    return VisibilityMath::odehValue(
        arcvDeg, VisibilityMath::theoreticalWidth(arcvDeg, dazDeg));
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

double altitudeRad(const Vec3d& position)
{
    Vec3d normalized = position;
    normalized.normalize();
    double azimuth = 0.0;
    double altitude = 0.0;
    StelUtils::rectToSphe(&azimuth, &altitude, normalized);
    return altitude;
}

double observationalVNow(StelCore* core, const PlanetP& sun, const PlanetP& moon)
{
    const double arcvDeg = (altitudeRad(moon->getAltAzPosGeometric(core))
                            - altitudeRad(sun->getAltAzPosGeometric(core))) * RAD2DEG;
    const double phase = moon->getPhase(core->getObserverHeliocentricEclipticPos());
    const double width = VisibilityMath::illuminatedWidth(
        phase, 2.0 * moon->getAngularRadius(core));
    return VisibilityMath::odehValue(arcvDeg, width);
}

double altitudeFromEquatorial(const Vec3d& position, double latitudeRad,
                              double localSiderealRad)
{
    double ra = 0.0;
    double dec = 0.0;
    StelUtils::rectToSphe(&ra, &dec, position);
    const double hourAngle = wrapPi(localSiderealRad - ra);
    return std::asin(std::sin(latitudeRad) * std::sin(dec)
                     + std::cos(latitudeRad) * std::cos(dec) * std::cos(hourAngle));
}

double observationalVAtJd(StelCore* core, const PlanetP&, const PlanetP& moon,
                          const PlanetP& earth, double jd)
{
    const double jde = jd + (core->getJDE() - core->getJD());
    const StelLocation& location = core->getCurrentLocation();
    const double latitude = location.getLatitude() * DEG2RAD;
    const double localSidereal = (earth->getSiderealTime(jd, jde)
                                  + location.getLongitude()) * DEG2RAD;

    Vec3d moonEq = StelCore::matVsop87ToJ2000.upper3x3() * moon->getEclipticPos(jde);
    Vec3d sunEq = StelCore::matVsop87ToJ2000.upper3x3() * (-earth->getEclipticPos(jde));
    moonEq = core->j2000ToEquinoxEqu(moonEq, StelCore::RefractionOff);
    sunEq = core->j2000ToEquinoxEqu(sunEq, StelCore::RefractionOff);

    const double observerRadius = core->getCurrentObserver()->getDistanceFromCenter();
    const Vec3d observer(observerRadius * std::cos(latitude) * std::cos(localSidereal),
                         observerRadius * std::cos(latitude) * std::sin(localSidereal),
                         observerRadius * std::sin(latitude));
    const Vec3d moonTopo = moonEq - observer;
    const Vec3d sunTopo = sunEq - observer;

    const double arcv = (altitudeFromEquatorial(moonTopo, latitude, localSidereal)
                         - altitudeFromEquatorial(sunTopo, latitude, localSidereal)) * RAD2DEG;
    const double cosElongation = std::clamp((moonTopo * sunTopo)
                                           / (moonTopo.norm() * sunTopo.norm()), -1.0, 1.0);
    const double illuminatedFraction = 0.5 * (1.0 - cosElongation);
    const double diameterDeg = 2.0 * std::atan2(moon->getEquatorialRadius(), moonTopo.norm())
                               * RAD2DEG;
    return VisibilityMath::odehValue(
        arcv, VisibilityMath::illuminatedWidth(illuminatedFraction, diameterDeg));
}

bool validRise(const Vec4d& rts)
{
    return std::isfinite(rts[0]) && std::abs(rts[3]) < 100.0 && rts[3] != 30.0;
}

bool validSet(const Vec4d& rts)
{
    return std::isfinite(rts[2]) && std::abs(rts[3]) < 100.0 && rts[3] != 40.0;
}

std::vector<VisibilityBand> bandsForCriterion(const QString& criterion,
                                              const PlanetP& moon,
                                              const PlanetP& earth)
{
    if (criterion == QStringLiteral("Yallop"))
    {
        auto boundaries = VisibilityMath::fallbackYallopBoundaries();
        const double hp = VisibilityMath::horizontalParallaxDeg(
            moon->getEclipticPos().norm(), earth->getEquatorialRadius());
        if (std::isfinite(hp))
            boundaries = VisibilityMath::yallopBoundaries(hp);
        return {{boundaries[0], boundaries[1], BLUE},
                {boundaries[1], boundaries[2], MAGENTA},
                {boundaries[2], boundaries[3], YELLOW},
                {boundaries[3], std::nullopt, GREEN}};
    }
    return {{-0.96, 2.00, BLUE},
            {2.00, 5.65, MAGENTA},
            {5.65, std::nullopt, GREEN}};
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
    info.authors = "Sultan ALKHULAIFI";
    info.contact = "qlifee@gmail.com";
    info.description = "Draws selectable Odeh and Yallop lunar-crescent visibility bands and reports observational V and best time."
                       "<br/><br/><b>Calculation method</b><br/>"
                       "All contours and displayed V values use the Odeh visibility equation, with airless geometric topocentric ARCV and topocentric crescent width W."
                       "<br/><br/>When Yallop is selected, its q boundaries are converted to equivalent Odeh V boundaries using the Moon's current horizontal parallax. Contour labels and Moon information therefore show equivalent V values, not q values."
                       "<br/><br/><b>References:</b><br/>"
                       "<a href=\"https://doi.org/10.1007/s10686-005-9002-5\">Odeh — <i>New Criterion for Lunar Crescent Visibility</i></a><br/>"
                       "<a href=\"https://assets.admiralty.co.uk/public/documents/2025-08/HMNAO%20Technical%20Notes%20Index.pdf?VersionId=DfKow0usAp5ANPUBA_pWq4.DJ6nbkX2q\">Yallop — <i>A Method for Predicting the First Sighting of the New Crescent Moon</i></a>";
    info.version = VISIBILITYCONTOURS_PLUGIN_VERSION;
    info.license = VISIBILITYCONTOURS_PLUGIN_LICENSE;
    info.startByDefault = true;
    return info;
}

VisibilityContours::VisibilityContours()
    : cachedForJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedConjunctionJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedBestLocalDay(std::numeric_limits<double>::quiet_NaN())
    , cachedBestLatitude(std::numeric_limits<double>::quiet_NaN())
    , cachedBestLongitude(std::numeric_limits<double>::quiet_NaN())
    , cachedBestAltitude(std::numeric_limits<int>::min())
    , cachedEveningJd(std::numeric_limits<double>::quiet_NaN())
    , cachedEveningV(std::numeric_limits<double>::quiet_NaN())
    , cachedMorningJd(std::numeric_limits<double>::quiet_NaN())
    , cachedMorningV(std::numeric_limits<double>::quiet_NaN())
    , cachedEveningAvailable(false)
    , cachedMorningAvailable(false)
    , cachedBestJd(std::numeric_limits<double>::quiet_NaN())
    , cachedBestV(std::numeric_limits<double>::quiet_NaN())
    , cachedBestAvailable(false)
    , selectedCriterion(QStringLiteral("Yallop"))
    , bandsFilled(false)
    , settings(nullptr)
    , configDialog(new VisibilityContoursDialog(this))
{
    setObjectName("VisibilityContours");
}

VisibilityContours::~VisibilityContours()
{
    delete configDialog;
    delete settings;
}

void VisibilityContours::init()
{
    // Keep plugin settings in Stellarium's active user config. Avoid the inline
    // StelApp::getSettings() accessor here: distro builds can differ in private
    // StelApp layout even when the public Stellarium/Qt ABI is otherwise usable.
    settings = new QSettings(StelFileMgr::getUserDir() + QStringLiteral("/config.ini"),
                             QSettings::IniFormat);
    readSettings();
    qInfo() << "VisibilityContours initialized";
}

bool VisibilityContours::configureGui(bool show)
{
    if (show && configDialog)
        configDialog->setVisible(true);
    return configDialog != nullptr;
}

QString VisibilityContours::criterion() const
{
    return selectedCriterion;
}

bool VisibilityContours::fillBands() const
{
    return bandsFilled;
}

void VisibilityContours::setCriterion(const QString& value)
{
    const QString normalized = value.compare(QStringLiteral("Yallop"), Qt::CaseInsensitive) == 0
                                   ? QStringLiteral("Yallop") : QStringLiteral("Odeh");
    if (selectedCriterion == normalized)
        return;
    selectedCriterion = normalized;
    saveSettings();
    emit criterionChanged(selectedCriterion);
}

void VisibilityContours::setFillBands(bool enabled)
{
    if (bandsFilled == enabled)
        return;
    bandsFilled = enabled;
    saveSettings();
    emit fillBandsChanged(enabled);
}

void VisibilityContours::readSettings()
{
    if (!settings)
    {
        qWarning() << "VisibilityContours: settings are not available";
        return;
    }
    settings->beginGroup(QStringLiteral("VisibilityContours"));
    selectedCriterion = QStringLiteral("Yallop");
    settings->setValue(QStringLiteral("criterion"), selectedCriterion);
    bandsFilled = settings->value(QStringLiteral("fill_bands"), false).toBool();
    settings->endGroup();
    settings->sync();
}

void VisibilityContours::saveSettings() const
{
    if (!settings)
        return;
    settings->beginGroup(QStringLiteral("VisibilityContours"));
    settings->setValue(QStringLiteral("criterion"), selectedCriterion);
    settings->setValue(QStringLiteral("fill_bands"), bandsFilled);
    settings->endGroup();
    settings->sync();
}

void VisibilityContours::addMoonInformation(StelCore* core)
{
    SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
    if (!solarSystem)
        return;
    const PlanetP sun = solarSystem->getSun();
    const PlanetP moon = solarSystem->getMoon();
    const PlanetP earth = solarSystem->getEarth();
    if (!sun || !moon || !earth)
        return;

    const QList<StelObjectP>& selected = StelApp::getInstance().getStelObjectMgr().getSelectedObject();
    if (selected.isEmpty() || selected.first()->getEnglishName() != QStringLiteral("Moon"))
        return;

    const double vNow = observationalVNow(core, sun, moon);
    moon->addToExtraInfoString(StelObject::OtherCoord,
        tr("V now: %1<br/>").arg(vNow, 0, 'f', 2));

    const StelLocation& location = core->getCurrentLocation();
    const double localDay = std::floor(core->getJD() + core->getUTCOffset(core->getJD()) / 24.0);
    if (!std::isfinite(cachedBestLocalDay) || localDay != cachedBestLocalDay
        || location.getLatitude() != cachedBestLatitude
        || location.getLongitude() != cachedBestLongitude
        || location.altitude != cachedBestAltitude)
    {
        // A non-zero altitude bypasses StelObject::getRTSTime's atmosphere-dependent
        // canonical refraction correction while remaining the mathematical horizon.
        constexpr double GEOMETRIC_HORIZON_EPSILON_DEG = 1e-9;
        const Vec4d sunRts = sun->getRTSTime(core, GEOMETRIC_HORIZON_EPSILON_DEG);
        const Vec4d moonRts = moon->getRTSTime(core, GEOMETRIC_HORIZON_EPSILON_DEG);

        std::optional<double> evening;
        std::optional<double> morning;
        if (validSet(sunRts) && validSet(moonRts))
            evening = VisibilityMath::eveningBestTime(sunRts[2], moonRts[2]);
        if (validRise(sunRts) && validRise(moonRts))
            morning = VisibilityMath::morningBestTime(sunRts[0], moonRts[0]);

        cachedEveningAvailable = evening.has_value();
        cachedMorningAvailable = morning.has_value();
        if (evening)
        {
            cachedEveningJd = *evening;
            cachedEveningV = observationalVAtJd(core, sun, moon, earth, *evening);
        }
        if (morning)
        {
            cachedMorningJd = *morning;
            cachedMorningV = observationalVAtJd(core, sun, moon, earth, *morning);
        }
        cachedBestLocalDay = localDay;
        cachedBestLatitude = location.getLatitude();
        cachedBestLongitude = location.getLongitude();
        cachedBestAltitude = location.altitude;
    }

    const std::optional<double> evening = cachedEveningAvailable
                                              ? std::optional<double>(cachedEveningJd)
                                              : std::nullopt;
    const std::optional<double> morning = cachedMorningAvailable
                                              ? std::optional<double>(cachedMorningJd)
                                              : std::nullopt;
    const std::optional<double> best = VisibilityMath::nearestTime(core->getJD(), evening, morning);
    cachedBestAvailable = best.has_value();
    if (best)
    {
        const bool useEvening = cachedEveningAvailable && *best == cachedEveningJd;
        cachedBestJd = *best;
        cachedBestV = useEvening ? cachedEveningV : cachedMorningV;
    }

    if (!cachedBestAvailable)
    {
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                  tr("Best time: Not available<br/>"));
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                  tr("V at best time: Not available<br/>"));
        return;
    }

    const double offset = core->getUTCOffset(cachedBestJd);
    const QString localTime = QString::fromStdString(
        VisibilityMath::formatLocalTime(cachedBestJd, offset));
    moon->addToExtraInfoString(StelObject::OtherCoord,
                              tr("Best time: %1<br/>").arg(localTime));
    moon->addToExtraInfoString(StelObject::OtherCoord,
                              tr("V at best time: %1<br/>").arg(cachedBestV, 0, 'f', 2));
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

    addMoonInformation(core);

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

    const std::vector<VisibilityBand> bands =
        bandsForCriterion(selectedCriterion, moon, earth);

    if (bandsFilled)
    {
        for (const VisibilityBand& band : bands)
        {
            QVector<Vec3d> strip;
            auto flushStrip = [&]()
            {
                if (strip.size() >= 4)
                {
                    painter.setColor(band.color.r, band.color.g, band.color.b, 0.16f);
                    painter.enableClientStates(true);
                    painter.setVertexPointer(3, GL_DOUBLE, strip.constData());
                    painter.drawFromArray(StelPainter::TriangleStrip, strip.size(), 0, true);
                    painter.enableClientStates(false);
                }
                strip.clear();
            };

            for (double dazDeg = DAZ_MIN_DEG;
                 dazDeg <= DAZ_MAX_DEG + 1e-9;
                 dazDeg += DAZ_STEP_DEG)
            {
                double lowerArcv = 0.0;
                if (!solveArcv(band.lowerV, dazDeg, lowerArcv))
                {
                    flushStrip();
                    continue;
                }

                double upperArcv = ARCV_MAX_DEG;
                if (band.upperV && !solveArcv(*band.upperV, dazDeg, upperArcv))
                {
                    flushStrip();
                    continue;
                }

                double lowerAlt = sunAltDeg + lowerArcv;
                double upperAlt = sunAltDeg + upperArcv;
                if (upperAlt <= 0.0 || lowerAlt >= 90.0 || upperAlt <= lowerAlt)
                {
                    flushStrip();
                    continue;
                }
                lowerAlt = std::max(0.0, lowerAlt);
                upperAlt = std::min(89.999, upperAlt);
                const double azimuth = sunAzRad + dazDeg * DEG2RAD;
                strip.append(altAzVector(azimuth, lowerAlt * DEG2RAD));
                strip.append(altAzVector(azimuth, upperAlt * DEG2RAD));
            }
            flushStrip();
        }
    }

    auto drawContour = [&](double value, const Color& color)
    {
        painter.setColor(color.r, color.g, color.b, 0.95f);

        bool havePreviousSample = false;
        double previousDazDeg = 0.0;
        double previousAltDeg = 0.0;
        Vec3d previous;

        for (double dazDeg = DAZ_MIN_DEG;
             dazDeg <= DAZ_MAX_DEG + 1e-9;
             dazDeg += DAZ_STEP_DEG)
        {
            double arcvDeg = 0.0;
            if (!solveArcv(value, dazDeg, arcvDeg))
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

        // Put one label on each shared boundary near DAZ=+20 deg.
        double labelArcvDeg = 0.0;
        if (solveArcv(value, 20.0, labelArcvDeg))
        {
            const double labelAltDeg = sunAltDeg + labelArcvDeg;
            if (labelAltDeg >= 0.0 && labelAltDeg < 90.0)
            {
                const Vec3d labelPos = altAzVector(sunAzRad + 20.0 * DEG2RAD,
                                                    labelAltDeg * DEG2RAD);
                painter.drawText(labelPos,
                                 QString("V=%1").arg(value, 0, 'f', 2),
                                 0.0f, 5.0f, 4.0f, true);
            }
        }
    };

    // Each lower boundary is shared with the preceding category's upper edge,
    // so draw it once to avoid gaps, overlaps, and color-overdraw ambiguity.
    for (const VisibilityBand& band : bands)
        drawContour(band.lowerV, band.color);

    // Status label close to the Sun; useful for checking the automatic day filter.
    painter.setColor(1.0f, 1.0f, 1.0f, 0.90f);
    painter.drawText(sunAltAz,
                     QString("Conjunction day %1   Δ=%2 d")
                         .arg(dayIndex >= 0 ? QString("+%1").arg(dayIndex)
                                            : QString::number(dayIndex))
                         .arg(daysFromConjunction, 0, 'f', 2),
                     0.0f, 10.0f, -18.0f, true);
}
