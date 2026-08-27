#include "VisibilityContours.hpp"
#include "CrescentNavigatorDialog.hpp"
#include "VisibilityContoursDialog.hpp"
#include "VisibilityMath.hpp"

#include "Planet.hpp"
#include "SolarSystem.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelObserver.hpp"
#include "StelLocation.hpp"
#include "StelLocaleMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelUtils.hpp"
#include "precession.h"

#include <QDebug>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QSettings>
#include <QString>
#include <QTranslator>
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

// A small non-zero altitude bypasses StelObject::getRTSTime's
// atmosphere-dependent canonical correction while retaining the Moon's
// mathematical geometric center crossing.
constexpr double MOON_GEOMETRIC_HORIZON_EPSILON_DEG = 1e-9;

// The user's requested inversion interval.
constexpr double ARCV_MIN_DEG = -20.0;
constexpr double ARCV_MAX_DEG =  40.0;

// Contours are sampled in delta-azimuth. Roots naturally disappear near |DAZ|~50 deg.
constexpr double DAZ_MIN_DEG = -55.0;
constexpr double DAZ_MAX_DEG =  55.0;
constexpr double DAZ_STEP_DEG = 0.25;
constexpr double GREEN_BAND_UPPER_V = 27.0;
constexpr double SYNODIC_MONTH_DAYS = 29.530588853;
constexpr double LIGHT_SPEED_AU_PER_DAY = 173.1446326846693;
constexpr int MAX_NAVIGATOR_LUNATIONS = 24;
constexpr int NAVIGATOR_TRANSITION_HALF_SPAN_DAYS = 10;
constexpr double NAVIGATION_EPSILON_DAYS = 1.0 / 86400.0;

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
bool findNearestGeometricConjunction(const PlanetP& moon, const PlanetP& earth,
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

struct ObservationalGeometry
{
    double moonAltitudeDeg;
    double sunAltitudeDeg;
    double arcvDeg;
    double dazDeg;
    double widthArcmin;
    double v;
    double illuminatedFraction;
};

struct NavigatorVisibilitySample
{
    double solarEventJd;
    double bestTimeJd;
    double bestTimeJde;
    double conjunctionJde;
    int dayIndex;
    VisibilityMath::CrescentEventKind kind;
    double bestTimeMoonAltitudeDeg;
    double v;
};

struct NavigatorSampleCacheEntry
{
    double conjunctionJde;
    double deltaTSeconds;
    std::vector<NavigatorVisibilitySample> samples;
};

double jdeForJd(StelCore* core, double jd)
{
    return jd + core->computeDeltaT(jd) / 86400.0;
}

double jdForJde(StelCore* core, double jde)
{
    double jd = jde - core->computeDeltaT(jde) / 86400.0;
    return jde - core->computeDeltaT(jd) / 86400.0;
}

class ApparentStateGuard
{
public:
    ApparentStateGuard(StelCore* value, SolarSystem* system)
        : core(value)
        , solarSystem(system)
        , jd(value->getJDOfLastJDUpdate())
        , millis(value->getMilliSecondsOfLastJDUpdate())
        , useAberration(value->getUseAberration())
        , aberrationFactor(value->getAberrationFactor())
        , useNutation(value->getUseNutation())
        , useTopocentric(value->getUseTopocentricCoordinates())
        , lightTravelTime(system->getFlagLightTravelTime())
    {
        solarSystem->setFlagLightTravelTime(true);
        core->setUseAberration(true);
        core->setAberrationFactor(1.0);
        core->setUseNutation(true);
        core->setUseTopocentricCoordinates(true);
        core->update(0.0);
        solarSystem->computePositions(core, core->getJDE(),
                                      core->getCurrentPlanet());
        core->update(0.0);
    }

    ~ApparentStateGuard()
    {
        solarSystem->setFlagLightTravelTime(lightTravelTime);
        core->setUseAberration(useAberration);
        core->setAberrationFactor(aberrationFactor);
        core->setUseNutation(useNutation);
        core->setUseTopocentricCoordinates(useTopocentric);
        core->setJD(jd);
        core->setMilliSecondsOfLastJDUpdate(millis);
        core->update(0.0);
        solarSystem->computePositions(core, core->getJDE(),
                                      core->getCurrentPlanet());
        core->update(0.0);
    }

private:
    StelCore* core;
    SolarSystem* solarSystem;
    double jd;
    qint64 millis;
    bool useAberration;
    double aberrationFactor;
    bool useNutation;
    bool useTopocentric;
    bool lightTravelTime;
};

struct TopocentricConjunctionCacheEntry
{
    double geocentricJde;
    double latitude;
    double longitude;
    int altitude;
    double topocentricJde;
};

std::vector<double> apparentGeocentricConjunctionCache;
std::vector<TopocentricConjunctionCacheEntry>
    apparentTopocentricConjunctionCache;
std::vector<NavigatorSampleCacheEntry> navigatorSampleCache;
double topocentricCacheLatitude = std::numeric_limits<double>::quiet_NaN();
double topocentricCacheLongitude = std::numeric_limits<double>::quiet_NaN();
int topocentricCacheAltitude = std::numeric_limits<int>::min();
double navigatorCacheLatitude = std::numeric_limits<double>::quiet_NaN();
double navigatorCacheLongitude = std::numeric_limits<double>::quiet_NaN();
int navigatorCacheAltitude = std::numeric_limits<int>::min();
QString navigatorCacheTimeZone;

void clearNavigatorSampleCache()
{
    navigatorSampleCache.clear();
    navigatorCacheLatitude = std::numeric_limits<double>::quiet_NaN();
    navigatorCacheLongitude = std::numeric_limits<double>::quiet_NaN();
    navigatorCacheAltitude = std::numeric_limits<int>::min();
    navigatorCacheTimeZone.clear();
}

void rememberGeocentricConjunction(double conjunctionJde)
{
    for (const double cached : apparentGeocentricConjunctionCache)
    {
        if (std::abs(cached - conjunctionJde) < 1e-6)
            return;
    }
    apparentGeocentricConjunctionCache.push_back(conjunctionJde);
    if (apparentGeocentricConjunctionCache.size() > 64)
        apparentGeocentricConjunctionCache.erase(
            apparentGeocentricConjunctionCache.begin());
}

Vec3d observerOffsetVsopAtJde(StelCore* core, double jde)
{
    const StelObserver* observer = core ? core->getCurrentObserver() : nullptr;
    if (!observer)
        return Vec3d(0.0);
    const StelLocation& location = core->getCurrentLocation();
    const Vec4d offset = observer->getTopographicOffsetFromCenter();
    const double sigma = location.getLatitude() * DEG2RAD - offset[2];
    const Vec3d observerAltAz(offset[3] * std::sin(sigma), 0.0,
                             offset[3] * std::cos(sigma));
    const double jd = jdForJde(core, jde);
    const Mat4d altAzToVsop =
        observer->getRotEquatorialToVsop87()
        * observer->getRotAltAzToEquatorial(jd, jde);
    return altAzToVsop.multiplyWithoutTranslation(observerAltAz);
}

double topocentricGeometricLongitudeDifference(
    StelCore* core, const PlanetP& moon, const PlanetP& earth, double jde)
{
    if (!core || !moon || !earth || !std::isfinite(jde))
        return std::numeric_limits<double>::quiet_NaN();
    const Vec3d observerVsop = observerOffsetVsopAtJde(core, jde);
    const Vec3d moonTopo = moon->getEclipticPos(jde) - observerVsop;
    const Vec3d sunTopo = -earth->getEclipticPos(jde) - observerVsop;
    if (moonTopo.normSquared() <= 0.0 || sunTopo.normSquared() <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return wrapPi(std::atan2(moonTopo[1], moonTopo[0])
                  - std::atan2(sunTopo[1], sunTopo[0]));
}

class ApparentLongitudeEvaluator
{
public:
    ApparentLongitudeEvaluator(StelCore* coreValue,
                               SolarSystem* solarSystemValue,
                               const PlanetP& sunValue,
                               const PlanetP& moonValue,
                               const PlanetP& earthValue,
                               bool topocentricValue)
        : core(coreValue)
        , solarSystem(solarSystemValue)
        , sun(sunValue)
        , moon(moonValue)
        , earth(earthValue)
        , topocentric(topocentricValue)
        , guard(coreValue, solarSystemValue)
    {
    }

    double operator()(double jde)
    {
        if (!std::isfinite(jde))
            return std::numeric_limits<double>::quiet_NaN();
        core->setJD(jdForJde(core, jde));
        core->update(0.0);
        solarSystem->computePositions(core, core->getJDE(),
                                      core->getCurrentPlanet());
        // computePositions() updates Earth's date-dependent orientation.
        // Rebuild the core transforms once more so the apparent equatorial
        // vectors are converted through the matching true equinox/ecliptic.
        core->update(0.0);

        double moonLongitude = 0.0;
        double sunLongitude = 0.0;
        if (!eclipticLongitude(moon, moonLongitude)
            || !eclipticLongitude(sun, sunLongitude))
            return std::numeric_limits<double>::quiet_NaN();
        return wrapPi(moonLongitude - sunLongitude);
    }

private:
    bool eclipticLongitude(const PlanetP& body, double& longitude) const
    {
        Vec3d apparentJ2000 = body->getJ2000EquatorialPos(core);
        if (apparentJ2000.normSquared() <= 0.0)
            return false;

        if (!topocentric)
        {
            // Planet::getJ2000EquatorialPos() is observer-centered. Restore
            // the surface offset to obtain the apparent place at Earth center
            // while retaining Stellarium's light-time and annual aberration.
            const Vec3d observerOffset =
                core->getObserverHeliocentricEclipticPos()
                - earth->getHeliocentricEclipticPos();
            apparentJ2000 += StelCore::matVsop87ToJ2000
                                 .multiplyWithoutTranslation(observerOffset);
        }
        else
        {
            // Preserve Stellarium's geocentric apparent convention, including
            // its special Earth-observer treatment of the Moon, and add only
            // the observer's rotational contribution for diurnal aberration.
            const Vec3d rotationalVelocity =
                core->getObserverHeliocentricEclipticVelocity()
                - earth->getHeliocentricEclipticVelocity();
            const Vec3d diurnalPush = rotationalVelocity
                                      * (apparentJ2000.norm()
                                         / LIGHT_SPEED_AU_PER_DAY);
            apparentJ2000 += StelCore::matVsop87ToJ2000
                                 .multiplyWithoutTranslation(diurnalPush);
        }

        const Vec3d equinoxEquatorial = core->j2000ToEquinoxEqu(
            apparentJ2000, StelCore::RefractionOff);
        double rightAscension = 0.0;
        double declination = 0.0;
        StelUtils::rectToSphe(&rightAscension, &declination,
                              equinoxEquatorial);

        double epsilonA = 0.0;
        double chiA = 0.0;
        double omegaA = 0.0;
        double psiA = 0.0;
        getPrecessionAnglesVondrak(core->getJDE(), &epsilonA, &chiA,
                                   &omegaA, &psiA);
        double deltaPsi = 0.0;
        double deltaEpsilon = 0.0;
        getNutationAngles(core->getJDE(), &deltaPsi, &deltaEpsilon);
        double latitude = 0.0;
        StelUtils::equToEcl(rightAscension, declination,
                            epsilonA + deltaEpsilon,
                            &longitude, &latitude);
        return std::isfinite(longitude);
    }

    StelCore* core;
    SolarSystem* solarSystem;
    PlanetP sun;
    PlanetP moon;
    PlanetP earth;
    bool topocentric;
    ApparentStateGuard guard;
};

bool refineApparentConjunction(StelCore* core, SolarSystem* solarSystem,
                               const PlanetP& sun, const PlanetP& moon,
                               const PlanetP& earth, double seedJde,
                               bool topocentric, double& conjunctionJde)
{
    if (!core || !solarSystem || !sun || !moon || !earth
        || !std::isfinite(seedJde))
        return false;
    ApparentLongitudeEvaluator evaluator(core, solarSystem, sun, moon, earth,
                                         topocentric);
    const auto refined = VisibilityMath::refineWrappedLongitudeRoot(
        [&evaluator](double jde) { return evaluator(jde); },
        seedJde, topocentric ? 0.125 : 0.05);
    if (!refined)
        return false;
    conjunctionJde = *refined;
    return true;
}

bool findNearestConjunction(StelCore* core, SolarSystem* solarSystem,
                            const PlanetP& sun, const PlanetP& moon,
                            const PlanetP& earth, double currentJde,
                            double& conjunctionJde)
{
    double geometricSeed = 0.0;
    if (!findNearestGeometricConjunction(moon, earth, currentJde,
                                         geometricSeed))
        return false;
    for (const double cached : apparentGeocentricConjunctionCache)
    {
        if (std::abs(cached - geometricSeed) < 0.5)
        {
            conjunctionJde = cached;
            return true;
        }
    }
    if (!refineApparentConjunction(core, solarSystem, sun, moon, earth,
                                   geometricSeed, false, conjunctionJde))
        return false;
    rememberGeocentricConjunction(conjunctionJde);
    return true;
}

bool findAdjacentConjunction(StelCore* core, SolarSystem* solarSystem,
                             const PlanetP& sun, const PlanetP& moon,
                             const PlanetP& earth, double conjunctionJde,
                             int direction, double& adjacentJde)
{
    if (direction == 0 || !std::isfinite(conjunctionJde))
        return false;
    const double seed = conjunctionJde
                        + (direction > 0 ? SYNODIC_MONTH_DAYS
                                         : -SYNODIC_MONTH_DAYS);
    if (!findNearestConjunction(core, solarSystem, sun, moon, earth,
                                seed, adjacentJde))
        return false;
    return direction > 0 ? adjacentJde > conjunctionJde + 20.0
                         : adjacentJde < conjunctionJde - 20.0;
}

bool findConjunctionFromAnyPhase(StelCore* core, SolarSystem* solarSystem,
                                 const PlanetP& sun, const PlanetP& moon,
                                 const PlanetP& earth, double currentJde,
                                 double& conjunctionJde)
{
    bool found = false;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (double offset = -16.0; offset <= 16.0; offset += 4.0)
    {
        double candidate = 0.0;
        if (!findNearestConjunction(core, solarSystem, sun, moon, earth,
                                    currentJde + offset, candidate))
            continue;
        const double distance = std::abs(candidate - currentJde);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            conjunctionJde = candidate;
            found = true;
        }
    }
    return found;
}

bool findPrecedingConjunction(StelCore* core, SolarSystem* solarSystem,
                              const PlanetP& sun, const PlanetP& moon,
                              const PlanetP& earth, double currentJde,
                              double& conjunctionJde)
{
    double nearestJde = 0.0;
    if (!findConjunctionFromAnyPhase(core, solarSystem, sun, moon, earth,
                                     currentJde, nearestJde))
        return false;
    std::optional<double> previousJde;
    if (nearestJde > currentJde)
    {
        double value = 0.0;
        if (findAdjacentConjunction(core, solarSystem, sun, moon, earth,
                                    nearestJde, -1, value))
            previousJde = value;
    }
    const auto selected = VisibilityMath::selectPrecedingConjunction(
        currentJde, nearestJde, previousJde);
    if (!selected)
        return false;
    conjunctionJde = *selected;
    return true;
}

bool findTopocentricConjunction(StelCore* core, SolarSystem* solarSystem,
                                const PlanetP& sun, const PlanetP& moon,
                                const PlanetP& earth,
                                double geocentricConjunctionJde,
                                double& topocentricConjunctionJde)
{
    if (!core || !std::isfinite(geocentricConjunctionJde))
        return false;
    const StelLocation& location = core->getCurrentLocation();
    const double latitude = location.getLatitude();
    const double longitude = location.getLongitude();
    const int altitude = location.altitude;
    if (latitude != topocentricCacheLatitude
        || longitude != topocentricCacheLongitude
        || altitude != topocentricCacheAltitude)
    {
        apparentTopocentricConjunctionCache.clear();
        topocentricCacheLatitude = latitude;
        topocentricCacheLongitude = longitude;
        topocentricCacheAltitude = altitude;
    }
    for (const auto& cached : apparentTopocentricConjunctionCache)
    {
        if (std::abs(cached.geocentricJde - geocentricConjunctionJde) < 1e-6
            && cached.latitude == latitude
            && cached.longitude == longitude
            && cached.altitude == altitude)
        {
            topocentricConjunctionJde = cached.topocentricJde;
            return true;
        }
    }

    const auto geometricSeed = VisibilityMath::refineWrappedLongitudeRoot(
        [core, &moon, &earth](double jde)
        {
            return topocentricGeometricLongitudeDifference(
                core, moon, earth, jde);
        },
        geocentricConjunctionJde, 0.25);
    if (!geometricSeed
        || !refineApparentConjunction(core, solarSystem, sun, moon, earth,
                                      *geometricSeed, true,
                                      topocentricConjunctionJde))
        return false;

    apparentTopocentricConjunctionCache.push_back(
        {geocentricConjunctionJde, latitude, longitude, altitude,
         topocentricConjunctionJde});
    if (apparentTopocentricConjunctionCache.size() > 64)
        apparentTopocentricConjunctionCache.erase(
            apparentTopocentricConjunctionCache.begin());
    return true;
}

std::optional<ObservationalGeometry> observationalGeometryAtJd(
    StelCore* core, const PlanetP& moon, const PlanetP& earth, double jd)
{
    if (!core || !moon || !earth || !std::isfinite(jd))
        return std::nullopt;

    const double jde = jdeForJd(core, jd);
    if (!std::isfinite(jde))
        return std::nullopt;

    const StelLocation& location = core->getCurrentLocation();
    const StelObserver* observer = core->getCurrentObserver();
    if (!observer)
        return std::nullopt;
    const Vec4d offset = observer->getTopographicOffsetFromCenter();
    const double sigma = location.getLatitude() * DEG2RAD - offset[2];
    const Vec3d observerAltAz(offset[3] * std::sin(sigma), 0.0,
                             offset[3] * std::cos(sigma));
    const Mat4d altAzToVsop =
        observer->getRotEquatorialToVsop87()
        * observer->getRotAltAzToEquatorial(jd, jde);
    const Vec3d observerVsop =
        altAzToVsop.multiplyWithoutTranslation(observerAltAz);

    const Vec3d earthHelio = earth->getEclipticPos(jde);
    const Vec3d moonGeo = moon->getEclipticPos(jde);
    const Vec3d moonTopo = moonGeo - observerVsop;
    const Vec3d sunTopo = -earthHelio - observerVsop;
    if (moonTopo.normSquared() <= 0.0 || sunTopo.normSquared() <= 0.0)
        return std::nullopt;

    const Mat4d vsopToAltAz = altAzToVsop.transpose();
    const Vec3d moonAltAz =
        vsopToAltAz.multiplyWithoutTranslation(moonTopo);
    const Vec3d sunAltAz =
        vsopToAltAz.multiplyWithoutTranslation(sunTopo);
    double moonAzimuthRad = 0.0;
    double moonAltitudeRad = 0.0;
    double sunAzimuthRad = 0.0;
    double sunAltitudeRad = 0.0;
    StelUtils::rectToSphe(&moonAzimuthRad, &moonAltitudeRad, moonAltAz);
    StelUtils::rectToSphe(&sunAzimuthRad, &sunAltitudeRad, sunAltAz);
    const double moonAltitudeDeg = moonAltitudeRad * RAD2DEG;
    const double sunAltitudeDeg = sunAltitudeRad * RAD2DEG;

    const Vec3d moonHelio = earthHelio + moonGeo;
    const Vec3d observerHelio = earthHelio + observerVsop;
    const double observerMoonRq = (observerHelio - moonHelio).normSquared();
    const double moonRq = moonHelio.normSquared();
    const double observerRq = observerHelio.normSquared();
    const double phaseDenominator =
        2.0 * std::sqrt(observerMoonRq * moonRq);
    if (!(phaseDenominator > 0.0))
        return std::nullopt;
    const double cosPhaseAngle = std::clamp(
        (observerMoonRq + moonRq - observerRq) / phaseDenominator,
        -1.0, 1.0);
    const double illuminatedFraction =
        0.5 * std::abs(1.0 + cosPhaseAngle);
    const double diameterDeg = 2.0 * std::atan2(
        moon->getEquatorialRadius(), std::sqrt(observerMoonRq)) * RAD2DEG;
    const double arcvDeg = moonAltitudeDeg - sunAltitudeDeg;
    const double dazDeg = VisibilityMath::signedAngleDifferenceDeg(
        moonAzimuthRad * RAD2DEG, sunAzimuthRad * RAD2DEG);
    const double widthArcmin = VisibilityMath::illuminatedWidth(
        illuminatedFraction, diameterDeg);
    const double v = VisibilityMath::odehValue(arcvDeg, widthArcmin);
    if (!std::isfinite(moonAltitudeDeg)
        || !std::isfinite(sunAltitudeDeg)
        || !std::isfinite(arcvDeg) || !std::isfinite(dazDeg)
        || !std::isfinite(widthArcmin) || !std::isfinite(v))
        return std::nullopt;
    return ObservationalGeometry{moonAltitudeDeg, sunAltitudeDeg, arcvDeg,
                                 dazDeg, widthArcmin, v,
                                 illuminatedFraction};
}

double observationalVAtJd(StelCore* core, const PlanetP& moon,
                          const PlanetP& earth, double jd)
{
    const auto geometry = observationalGeometryAtJd(
        core, moon, earth, jd);
    return geometry ? geometry->v
                    : std::numeric_limits<double>::quiet_NaN();
}

bool validRise(const Vec4d& rts)
{
    return std::isfinite(rts[0]) && std::abs(rts[3]) < 100.0 && rts[3] != 30.0;
}

bool validSet(const Vec4d& rts)
{
    return std::isfinite(rts[2]) && std::abs(rts[3]) < 100.0 && rts[3] != 40.0;
}

class CoreTimeGuard
{
public:
    explicit CoreTimeGuard(StelCore* value)
        : core(value)
        , jd(value->getJDOfLastJDUpdate())
        , millis(value->getMilliSecondsOfLastJDUpdate())
    {
    }

    ~CoreTimeGuard()
    {
        core->setJD(jd);
        core->setMilliSecondsOfLastJDUpdate(millis);
        core->update(0.0);
    }

private:
    StelCore* core;
    double jd;
    qint64 millis;
};

struct LocalCivilDay
{
    qint64 number;
    int year;
    int month;
    int day;
};

struct SunsetInterval
{
    double previousSunsetJd;
    double nextSunsetJd;
    LocalCivilDay previousSunsetDay;
};

struct ObservationalHijriCalculation
{
    VisibilityMath::ObservationalHijriResult result;
    double validFromJd;
    double validUntilJd;
};

bool localCivilDayForJd(StelCore* core, double jd, LocalCivilDay& localDay)
{
    if (!core || !std::isfinite(jd))
        return false;
    const double localJd = jd + core->getUTCOffset(jd) / 24.0;
    StelUtils::getDateFromJulianDay(localJd, &localDay.year,
                                    &localDay.month, &localDay.day);
    double localMidnightJd = 0.0;
    if (!StelUtils::getJDFromDate(&localMidnightJd, localDay.year,
                                  localDay.month, localDay.day,
                                  0, 0, 0.0f))
        return false;
    localDay.number = static_cast<qint64>(
        std::llround(localMidnightJd - 0.5));
    return true;
}

LocalCivilDay localCivilDayFromNumber(qint64 number)
{
    LocalCivilDay result{number, 0, 0, 0};
    StelUtils::getDateFromJulianDay(static_cast<double>(number) + 0.5,
                                    &result.year, &result.month, &result.day);
    return result;
}

double referenceJdForLocalDay(StelCore* core, qint64 localDayNumber)
{
    const double localNoonJd = static_cast<double>(localDayNumber) + 1.0;
    double referenceJd = localNoonJd
                         - core->getUTCOffset(localNoonJd) / 24.0;
    referenceJd = localNoonJd
                  - core->getUTCOffset(referenceJd) / 24.0;
    return referenceJd;
}

bool conventionalSunsetForLocalDay(StelCore* core, const PlanetP& sun,
                                   qint64 localDayNumber, double& sunsetJd)
{
    if (!core || !sun)
        return false;
    const double referenceJd = referenceJdForLocalDay(core, localDayNumber);
    core->setJD(referenceJd);
    core->update(0.0);
    const Vec4d sunRts = sun->getRTSTime(
        core, VisibilityMath::CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG);
    if (!validSet(sunRts))
        return false;
    sunsetJd = sunRts[2];
    return std::isfinite(sunsetJd);
}

bool sunsetIntervalForJd(StelCore* core, const PlanetP& sun,
                         double currentJd, SunsetInterval& interval)
{
    LocalCivilDay today{};
    if (!localCivilDayForJd(core, currentJd, today))
        return false;

    double todaySunset = 0.0;
    if (!conventionalSunsetForLocalDay(core, sun, today.number, todaySunset))
        return false;

    if (VisibilityMath::sunsetHasOccurred(currentJd, todaySunset))
    {
        double tomorrowSunset = 0.0;
        if (!conventionalSunsetForLocalDay(
                core, sun, today.number + 1, tomorrowSunset))
            return false;
        interval = {todaySunset, tomorrowSunset, today};
        return tomorrowSunset > todaySunset;
    }

    double yesterdaySunset = 0.0;
    if (!conventionalSunsetForLocalDay(
            core, sun, today.number - 1, yesterdaySunset))
        return false;
    interval = {yesterdaySunset, todaySunset,
                localCivilDayFromNumber(today.number - 1)};
    return todaySunset > yesterdaySunset;
}

VisibilityMath::HijriLunationEvents
hijriVisibilityEventsForConjunction(
    StelCore* core, const PlanetP& sun, const PlanetP& moon,
    const PlanetP& earth, double conjunctionJde, int maximumDayIndex)
{
    VisibilityMath::HijriLunationEvents lunation{conjunctionJde, {}};
    if (!core || !sun || !moon || !earth || !std::isfinite(conjunctionJde)
        || maximumDayIndex < 0)
        return lunation;

    CoreTimeGuard restoreTime(core);
    const double conjunctionJd = jdForJde(core, conjunctionJde);
    LocalCivilDay conjunctionDay{};
    if (!localCivilDayForJd(core, conjunctionJd, conjunctionDay))
        return lunation;

    // One extra local day safely covers the UTC-offset edge of the final
    // rounded post-conjunction bin.
    for (int dayOffset = 0; dayOffset <= maximumDayIndex + 1; ++dayOffset)
    {
        const qint64 localDayNumber = conjunctionDay.number + dayOffset;
        const double referenceJd = referenceJdForLocalDay(
            core, localDayNumber);
        core->setJD(referenceJd);
        core->update(0.0);

        const Vec4d sunRts = sun->getRTSTime(
            core, VisibilityMath::CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG);
        const Vec4d moonRts = moon->getRTSTime(
            core, MOON_GEOMETRIC_HORIZON_EPSILON_DEG);
        if (!validSet(sunRts) || !validSet(moonRts))
            continue;

        const std::optional<double> bestTime =
            VisibilityMath::eveningBestTime(sunRts[2], moonRts[2]);
        if (!bestTime)
            continue;

        const auto geometry = observationalGeometryAtJd(
            core, moon, earth, *bestTime);
        if (!geometry
            || !VisibilityMath::moonIsUp(geometry->moonAltitudeDeg)
            || geometry->illuminatedFraction > 0.5)
            continue;

        const double eventJde = jdeForJd(core, *bestTime);
        if (!(eventJde > conjunctionJde))
            continue;
        const int dayIndex = VisibilityMath::conjunctionDayIndex(
            eventJde, conjunctionJde);
        if (dayIndex < 0)
            continue;
        if (dayIndex > maximumDayIndex)
            break;

        const LocalCivilDay localDay =
            localCivilDayFromNumber(localDayNumber);
        lunation.events.push_back(
            {localDay.number, localDay.year, localDay.month,
             localDay.day, conjunctionJde, sunRts[2],
             *bestTime, eventJde, dayIndex, geometry->v});
    }

    std::sort(lunation.events.begin(), lunation.events.end(),
              [](const auto& first, const auto& second)
              {
                  return first.sunsetJd < second.sunsetJd;
              });
    lunation.events.erase(
        std::unique(lunation.events.begin(), lunation.events.end(),
                    [](const auto& first, const auto& second)
                    {
                        return std::abs(first.sunsetJd
                                        - second.sunsetJd)
                               < NAVIGATION_EPSILON_DAYS;
                    }),
        lunation.events.end());
    return lunation;
}

ObservationalHijriCalculation calculateObservationalHijriDate(
    StelCore* core, const PlanetP& sun, double currentJd,
    double latitudeDeg,
    const std::vector<VisibilityMath::HijriLunationEvents>& lunations)
{
    ObservationalHijriCalculation result{
        {}, currentJd - 0.25, currentJd + 0.25};
    result.result.latitudePolicy =
        VisibilityMath::hijriLatitudePolicy(latitudeDeg);
    if (result.result.latitudePolicy
        == VisibilityMath::HijriLatitudePolicy::Unsupported)
    {
        result.result.availability =
            VisibilityMath::HijriAvailabilityReason::LatitudeUnsupported;
        return result;
    }
    if (!core || !sun || !std::isfinite(currentJd))
        return result;

    CoreTimeGuard restoreTime(core);
    LocalCivilDay currentCivilDay{};
    if (localCivilDayForJd(core, currentJd, currentCivilDay))
    {
        const double localMidnightJd =
            static_cast<double>(currentCivilDay.number) + 0.5;
        result.validFromJd = localMidnightJd
                             - core->getUTCOffset(currentJd) / 24.0;
        result.validUntilJd = result.validFromJd + 1.0;
    }

    SunsetInterval interval{};
    if (!sunsetIntervalForJd(core, sun, currentJd, interval))
        return result;
    result.validFromJd = interval.previousSunsetJd;
    result.validUntilJd = interval.nextSunsetJd;

    result.result = VisibilityMath::observationalHijriFromLunationEvents(
        lunations, interval.previousSunsetDay.number, currentJd,
        latitudeDeg);
    return result;
}

std::vector<NavigatorVisibilitySample> navigatorSamplesForConjunction(
    StelCore* core, const PlanetP& sun, const PlanetP& moon,
    const PlanetP& earth, double conjunctionJde)
{
    std::vector<NavigatorVisibilitySample> samples;
    if (!core || !sun || !moon || !earth || !std::isfinite(conjunctionJde))
        return samples;

    const double conjunctionJd = jdForJde(core, conjunctionJde);
    const double deltaTSeconds = core->computeDeltaT(conjunctionJd);
    const StelLocation& location = core->getCurrentLocation();
    const QString timeZone = core->getCurrentTimeZone();
    if (location.getLatitude() != navigatorCacheLatitude
        || location.getLongitude() != navigatorCacheLongitude
        || location.altitude != navigatorCacheAltitude
        || timeZone != navigatorCacheTimeZone)
    {
        navigatorSampleCache.clear();
        navigatorCacheLatitude = location.getLatitude();
        navigatorCacheLongitude = location.getLongitude();
        navigatorCacheAltitude = location.altitude;
        navigatorCacheTimeZone = timeZone;
    }
    for (const auto& cached : navigatorSampleCache)
    {
        if (std::abs(cached.conjunctionJde - conjunctionJde) < 1e-6
            && std::abs(cached.deltaTSeconds - deltaTSeconds) < 1e-6)
            return cached.samples;
    }

    CoreTimeGuard restoreTime(core);
    LocalCivilDay conjunctionDay{};
    if (!localCivilDayForJd(core, conjunctionJd, conjunctionDay))
        return samples;

    auto appendEvent = [&](double solarEventJd,
                           const std::optional<double>& bestTime,
                           VisibilityMath::CrescentEventKind kind)
    {
        if (!bestTime)
            return;

        const auto geometry = observationalGeometryAtJd(
            core, moon, earth, *bestTime);
        if (!geometry || geometry->illuminatedFraction > 0.5)
            return;

        const double bestTimeJde = jdeForJd(core, *bestTime);
        const bool correctConjunctionSide =
            kind == VisibilityMath::CrescentEventKind::Morning
                ? bestTimeJde < conjunctionJde
                : bestTimeJde > conjunctionJde;
        if (!correctConjunctionSide)
            return;

        const int dayIndex = VisibilityMath::conjunctionDayIndex(
            bestTimeJde, conjunctionJde);
        if (dayIndex < -NAVIGATOR_TRANSITION_HALF_SPAN_DAYS
            || dayIndex > NAVIGATOR_TRANSITION_HALF_SPAN_DAYS)
            return;

        samples.push_back(
            {solarEventJd, *bestTime, bestTimeJde, conjunctionJde,
             dayIndex, kind, geometry->moonAltitudeDeg, geometry->v});
    };

    // Two extra civil days cover UTC-offset edges of the rounded +/-10-day
    // transition-search window. The half-illuminated phase filter below keeps
    // the samples on the waning/waxing crescent halves of the lunation.
    constexpr int LOCAL_DAY_MARGIN = 2;
    for (int dayOffset =
             -NAVIGATOR_TRANSITION_HALF_SPAN_DAYS - LOCAL_DAY_MARGIN;
         dayOffset <=
             NAVIGATOR_TRANSITION_HALF_SPAN_DAYS + LOCAL_DAY_MARGIN;
         ++dayOffset)
    {
        const double referenceJd = referenceJdForLocalDay(
            core, conjunctionDay.number + dayOffset);
        core->setJD(referenceJd);
        core->update(0.0);

        const Vec4d sunRts = sun->getRTSTime(
            core, VisibilityMath::CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG);
        const Vec4d moonRts = moon->getRTSTime(
            core, MOON_GEOMETRIC_HORIZON_EPSILON_DEG);

        std::optional<double> morning;
        std::optional<double> evening;
        if (validRise(sunRts) && validRise(moonRts))
            morning = VisibilityMath::morningBestTime(sunRts[0], moonRts[0]);
        if (validSet(sunRts) && validSet(moonRts))
            evening = VisibilityMath::eveningBestTime(sunRts[2], moonRts[2]);

        if (validRise(sunRts))
            appendEvent(sunRts[0], morning,
                        VisibilityMath::CrescentEventKind::Morning);
        if (validSet(sunRts))
            appendEvent(sunRts[2], evening,
                        VisibilityMath::CrescentEventKind::Evening);
    }

    std::sort(samples.begin(), samples.end(),
              [](const auto& first, const auto& second)
              {
                  if (first.bestTimeJd != second.bestTimeJd)
                      return first.bestTimeJd < second.bestTimeJd;
                  return first.kind == VisibilityMath::CrescentEventKind::Morning
                         && second.kind
                                == VisibilityMath::CrescentEventKind::Evening;
              });
    samples.erase(
        std::unique(samples.begin(), samples.end(),
                    [](const auto& first, const auto& second)
                    {
                        return first.kind == second.kind
                               && std::abs(first.bestTimeJd
                                           - second.bestTimeJd)
                                      < NAVIGATION_EPSILON_DAYS;
                    }),
        samples.end());

    navigatorSampleCache.push_back(
        {conjunctionJde, deltaTSeconds, samples});
    if (navigatorSampleCache.size() > 48)
        navigatorSampleCache.erase(navigatorSampleCache.begin());
    return samples;
}

std::vector<VisibilityMath::CrescentEvent>
crescentTransitionEventsForConjunction(
    StelCore* core, const PlanetP& sun, const PlanetP& moon,
    const PlanetP& earth, double conjunctionJde,
    VisibilityMath::NavigationMode mode)
{
    const auto samples = navigatorSamplesForConjunction(
        core, sun, moon, earth, conjunctionJde);
    std::vector<VisibilityMath::CrescentEvent> events;

    const auto appendKind = [&](VisibilityMath::CrescentEventKind kind)
    {
        std::vector<const NavigatorVisibilitySample*> kindSamples;
        std::vector<std::optional<double>> values;
        for (const auto& sample : samples)
        {
            if (sample.kind != kind)
                continue;
            kindSamples.push_back(&sample);
            if (mode == VisibilityMath::NavigationMode::MoonUpOnly
                && !VisibilityMath::moonIsUp(
                    sample.bestTimeMoonAltitudeDeg))
                values.push_back(std::nullopt);
            else
                values.push_back(sample.v);
        }

        const auto selected = VisibilityMath::visibilityTransitionIndices(
            values, kind);
        for (const std::size_t index : selected)
        {
            const auto& sample = *kindSamples[index];
            const auto navigationTime = VisibilityMath::chooseNavigationTime(
                mode, kind, sample.solarEventJd, sample.bestTimeJd,
                sample.bestTimeMoonAltitudeDeg);
            if (!navigationTime)
                continue;
            events.push_back(
                {navigationTime->jd, sample.conjunctionJde,
                 sample.dayIndex, kind, mode, navigationTime->basis});
        }
    };

    appendKind(VisibilityMath::CrescentEventKind::Morning);
    appendKind(VisibilityMath::CrescentEventKind::Evening);
    VisibilityMath::sortCrescentEvents(events);
    events.erase(std::unique(events.begin(), events.end(),
                             [](const auto& first, const auto& second)
                             {
                                 return std::abs(first.jd - second.jd)
                                        < NAVIGATION_EPSILON_DAYS;
                             }),
                 events.end());
    return events;
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
                {boundaries[3], GREEN_BAND_UPPER_V, GREEN}};
    }
    return {{-0.96, 2.00, BLUE},
            {2.00, 5.65, MAGENTA},
            {5.65, GREEN_BAND_UPPER_V, GREEN}};
}

} // namespace

VisibilityContoursStelPluginInterface::VisibilityContoursStelPluginInterface()
    : arabicTranslator(new QTranslator(this))
    , translatorInstalled(false)
{
    connect(&StelApp::getInstance(), &StelApp::languageChanged,
            this, &VisibilityContoursStelPluginInterface::refreshTranslation);
    refreshTranslation();
}

VisibilityContoursStelPluginInterface::~VisibilityContoursStelPluginInterface()
{
    if (translatorInstalled)
        QCoreApplication::removeTranslator(arabicTranslator);
}

void VisibilityContoursStelPluginInterface::refreshTranslation()
{
    if (translatorInstalled)
    {
        QCoreApplication::removeTranslator(arabicTranslator);
        translatorInstalled = false;
    }
    const QString language = StelApp::getInstance().getLocaleMgr().getAppLanguage();
    if (!VisibilityMath::useArabicForProgramLanguage(language.toStdString()))
        return;
    if (!arabicTranslator->load(QStringLiteral(":/i18n/VisibilityContours_ar.qm")))
    {
        qWarning() << "VisibilityContours: could not load embedded Arabic translation";
        return;
    }
    translatorInstalled = QCoreApplication::installTranslator(arabicTranslator);
}

StelModule* VisibilityContoursStelPluginInterface::getStelModule() const
{
    return new VisibilityContours();
}

StelPluginInfo VisibilityContoursStelPluginInterface::getPluginInfo() const
{
    StelPluginInfo info;
    info.id = "VisibilityContours";
    info.displayedName = tr("Crescent Visibility & Hijri Date");
    info.authors = tr("Sultan ALKHULAIFI");
    info.contact = "qlifee@gmail.com";
    info.description = tr("Draws selectable Odeh and Yallop lunar-crescent visibility bands and reports observational V and best time.")
                       + QStringLiteral("<br/><br/><b>")
                       + tr("Calculation method")
                       + QStringLiteral("</b><br/>")
                       + tr("All contours and displayed V values use the Odeh visibility equation, with airless geometric topocentric ARCV and topocentric crescent width W.")
                       + QStringLiteral("<br/><br/>")
                       + tr("When Yallop is selected, its q boundaries are converted to equivalent Odeh V boundaries using the Moon's current horizontal parallax. Contour labels and Moon information therefore show equivalent V values, not q values.")
                       + QStringLiteral(
                           "<br/><br/><b>References:</b><br/>"
                           "<a href=\"https://doi.org/10.1007/s10686-005-9002-5\">Odeh — <i>New Criterion for Lunar Crescent Visibility</i></a><br/>"
                           "<a href=\"https://assets.admiralty.co.uk/public/documents/2025-08/HMNAO%20Technical%20Notes%20Index.pdf?VersionId=DfKow0usAp5ANPUBA_pWq4.DJ6nbkX2q\">Yallop — <i>A Method for Predicting the First Sighting of the New Crescent Moon</i></a>");
    info.version = VISIBILITYCONTOURS_PLUGIN_VERSION;
    info.license = VISIBILITYCONTOURS_PLUGIN_LICENSE;
    info.startByDefault = true;
    return info;
}

VisibilityContours::VisibilityContours()
    : cachedForJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedConjunctionJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedInformationForJDE(std::numeric_limits<double>::quiet_NaN())
    , cachedInformationConjunctionJDE(
          std::numeric_limits<double>::quiet_NaN())
    , cachedBestLocalDay(std::numeric_limits<double>::quiet_NaN())
    , cachedBestLatitude(std::numeric_limits<double>::quiet_NaN())
    , cachedBestLongitude(std::numeric_limits<double>::quiet_NaN())
    , cachedBestAltitude(std::numeric_limits<int>::min())
    , cachedEveningJd(std::numeric_limits<double>::quiet_NaN())
    , cachedEveningV(std::numeric_limits<double>::quiet_NaN())
    , cachedEveningLagDays(std::numeric_limits<double>::quiet_NaN())
    , cachedMorningJd(std::numeric_limits<double>::quiet_NaN())
    , cachedMorningV(std::numeric_limits<double>::quiet_NaN())
    , cachedMorningLagDays(std::numeric_limits<double>::quiet_NaN())
    , cachedEveningAvailable(false)
    , cachedMorningAvailable(false)
    , cachedEveningLagAvailable(false)
    , cachedMorningLagAvailable(false)
    , cachedBestJd(std::numeric_limits<double>::quiet_NaN())
    , cachedBestV(std::numeric_limits<double>::quiet_NaN())
    , cachedBestAvailable(false)
    , cachedHijriValidFromJd(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriValidUntilJd(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriLatitude(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriLongitude(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriAltitude(std::numeric_limits<int>::min())
    , cachedHijriResult{}
    , cachedHijriEventsConjunctionJde(
          std::numeric_limits<double>::quiet_NaN())
    , cachedHijriEventsNextConjunctionJde(
          std::numeric_limits<double>::quiet_NaN())
    , cachedHijriEventsLatitude(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriEventsLongitude(std::numeric_limits<double>::quiet_NaN())
    , cachedHijriEventsAltitude(std::numeric_limits<int>::min())
    , selectedCriterion(QStringLiteral("Yallop"))
    , bandsFilled(true)
    , navigatorShown(false)
    , navigatorEarthAvailable(true)
    , navigatorEventFilter(VisibilityMath::EventFilter::Both)
    , settings(nullptr)
    , configDialog(new VisibilityContoursDialog(this))
    , navigatorDialog(new CrescentNavigatorDialog(this))
{
    setObjectName("VisibilityContours");
}

VisibilityContours::~VisibilityContours()
{
    clearNavigatorSampleCache();
    delete navigatorDialog;
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
    if (navigatorDialog && navigatorShown)
        navigatorDialog->setVisible(true);
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

bool VisibilityContours::navigatorVisible() const
{
    return navigatorShown;
}

QString VisibilityContours::eventFilter() const
{
    return QString::fromLatin1(
        VisibilityMath::eventFilterName(navigatorEventFilter));
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

void VisibilityContours::setNavigatorVisible(bool visible)
{
    if (navigatorShown == visible)
        return;
    navigatorShown = visible;
    if (navigatorDialog)
        navigatorDialog->setVisible(visible);
    saveSettings();
    emit navigatorVisibleChanged(visible);
}

void VisibilityContours::setEventFilter(const QString& value)
{
    const VisibilityMath::EventFilter normalized =
        VisibilityMath::eventFilterFromString(value.toStdString());
    if (navigatorEventFilter == normalized)
        return;
    navigatorEventFilter = normalized;
    saveSettings();
    emit eventFilterChanged(eventFilter());
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
    bandsFilled = settings->value(QStringLiteral("fill_bands"), true).toBool();
    navigatorShown = settings->value(QStringLiteral("show_navigator"), false).toBool();
    navigatorEventFilter = VisibilityMath::eventFilterFromString(
        settings->value(QStringLiteral("event_filter"),
                        QStringLiteral("both")).toString().toStdString());
    settings->setValue(
        QStringLiteral("event_filter"),
        QString::fromLatin1(VisibilityMath::eventFilterKey(navigatorEventFilter)));
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
    settings->setValue(QStringLiteral("show_navigator"), navigatorShown);
    settings->setValue(
        QStringLiteral("event_filter"),
        QString::fromLatin1(VisibilityMath::eventFilterKey(navigatorEventFilter)));
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

    const StelLocation& location = core->getCurrentLocation();
    const double currentJd = core->getJD();
    const double currentJde = core->getJDE();
    const QString currentTimeZone = core->getCurrentTimeZone();
    const double latitude = location.getLatitude();
    const bool hijriCacheValid =
        std::isfinite(cachedHijriValidFromJd)
        && std::isfinite(cachedHijriValidUntilJd)
        && currentJd >= cachedHijriValidFromJd
        && currentJd < cachedHijriValidUntilJd
        && location.getLatitude() == cachedHijriLatitude
        && location.getLongitude() == cachedHijriLongitude
        && location.altitude == cachedHijriAltitude
        && currentTimeZone == cachedHijriTimeZone;
    if (!hijriCacheValid)
    {
        const auto latitudePolicy =
            VisibilityMath::hijriLatitudePolicy(latitude);
        if (latitudePolicy
            != VisibilityMath::HijriLatitudePolicy::Unsupported)
        {
            const bool cacheLocationMatches =
                latitude == cachedHijriEventsLatitude
                && location.getLongitude() == cachedHijriEventsLongitude
                && location.altitude == cachedHijriEventsAltitude
                && currentTimeZone == cachedHijriEventsTimeZone;
            const bool eventCacheValid = cacheLocationMatches
                && VisibilityMath::lunationCacheCoversJde(
                    currentJde, cachedHijriEventsConjunctionJde,
                    cachedHijriEventsNextConjunctionJde);
            if (!eventCacheValid)
            {
                cachedHijriEvents.clear();
                double precedingConjunctionJde = 0.0;
                const bool havePrecedingConjunction =
                    findPrecedingConjunction(
                        core, solarSystem, sun, moon, earth,
                        currentJde, precedingConjunctionJde);
                if (havePrecedingConjunction)
                {
                    const int maximumDayIndex =
                        *VisibilityMath::hijriMaximumConjunctionBin(
                            latitude);
                    double conjunctionJde = precedingConjunctionJde;
                    for (int historyIndex = 0;
                         historyIndex
                             < VisibilityMath::MAX_HIJRI_HISTORY_LUNATIONS;
                         ++historyIndex)
                    {
                        cachedHijriEvents.push_back(
                            hijriVisibilityEventsForConjunction(
                                core, sun, moon, earth,
                                conjunctionJde, maximumDayIndex));
                        const auto anchors =
                            VisibilityMath::hijriHistoryAnchorCoverage(
                                cachedHijriEvents, currentJd, latitude);
                        // The active lunation must be replayed from an older
                        // synchronization anchor; otherwise opening the Moon
                        // panel after its crossing would lose premature-start
                        // history and make the result access-order dependent.
                        if (historyIndex > 0 && anchors[0] && anchors[1])
                            break;
                        if (historyIndex + 1
                            >= VisibilityMath::MAX_HIJRI_HISTORY_LUNATIONS)
                            break;

                        double previousConjunctionJde = 0.0;
                        if (!findAdjacentConjunction(
                                core, solarSystem, sun, moon, earth,
                                conjunctionJde, -1,
                                previousConjunctionJde))
                            break;
                        conjunctionJde = previousConjunctionJde;
                    }
                    std::sort(cachedHijriEvents.begin(),
                              cachedHijriEvents.end(),
                              [](const auto& first, const auto& second)
                              {
                                  return first.conjunctionJde
                                         < second.conjunctionJde;
                              });
                    cachedHijriEventsConjunctionJde =
                        precedingConjunctionJde;

                    double nextConjunctionJde = 0.0;
                    cachedHijriEventsNextConjunctionJde =
                        findAdjacentConjunction(
                            core, solarSystem, sun, moon, earth,
                            precedingConjunctionJde, 1,
                            nextConjunctionJde)
                            ? nextConjunctionJde
                            : std::numeric_limits<double>::quiet_NaN();
                }
                else
                {
                    cachedHijriEventsConjunctionJde =
                        std::numeric_limits<double>::quiet_NaN();
                    cachedHijriEventsNextConjunctionJde =
                        std::numeric_limits<double>::quiet_NaN();
                }
                cachedHijriEventsLatitude = latitude;
                cachedHijriEventsLongitude = location.getLongitude();
                cachedHijriEventsAltitude = location.altitude;
                cachedHijriEventsTimeZone = currentTimeZone;
            }

            // A cache first built late in the lunation may have stopped at an
            // anchor within that same lunation. If the clock then moves back
            // before that sunset, extend the cached history lazily instead of
            // reporting a transient unavailable date.
            auto anchors = VisibilityMath::hijriHistoryAnchorCoverage(
                cachedHijriEvents, currentJd, latitude);
            const int maximumDayIndex =
                *VisibilityMath::hijriMaximumConjunctionBin(latitude);
            while ((!anchors[0] || !anchors[1])
                   && !cachedHijriEvents.empty()
                   && cachedHijriEvents.size()
                          < static_cast<std::size_t>(
                              VisibilityMath::MAX_HIJRI_HISTORY_LUNATIONS))
            {
                double previousConjunctionJde = 0.0;
                if (!findAdjacentConjunction(
                        core, solarSystem, sun, moon, earth,
                        cachedHijriEvents.front().conjunctionJde, -1,
                        previousConjunctionJde))
                    break;
                cachedHijriEvents.insert(
                    cachedHijriEvents.begin(),
                    hijriVisibilityEventsForConjunction(
                        core, sun, moon, earth,
                        previousConjunctionJde, maximumDayIndex));
                anchors = VisibilityMath::hijriHistoryAnchorCoverage(
                    cachedHijriEvents, currentJd, latitude);
            }
        }
        else
        {
            // Above the supported latitude boundary there is deliberately no
            // proxy-location or ephemeris-history calculation.
            cachedHijriEvents.clear();
            cachedHijriEventsConjunctionJde =
                std::numeric_limits<double>::quiet_NaN();
            cachedHijriEventsNextConjunctionJde =
                std::numeric_limits<double>::quiet_NaN();
        }
        const ObservationalHijriCalculation calculation =
            calculateObservationalHijriDate(
                core, sun, currentJd, latitude,
                cachedHijriEvents);
        cachedHijriValidFromJd = calculation.validFromJd;
        cachedHijriValidUntilJd = calculation.validUntilJd;
        cachedHijriLatitude = latitude;
        cachedHijriLongitude = location.getLongitude();
        cachedHijriAltitude = location.altitude;
        cachedHijriTimeZone = currentTimeZone;
        cachedHijriResult = calculation.result;
    }

    const QString hijriDateText =
        VisibilityMath::observationalHijriAvailable(cachedHijriResult)
        ? QString::fromStdString(
              VisibilityMath::formatObservationalHijriDate(
                  cachedHijriResult.date))
        : QString();
    if (navigatorShown && navigatorDialog)
        navigatorDialog->setObservationalHijriResult(cachedHijriResult);

    const auto ltrValue = [](const QString& value)
    {
        return QStringLiteral("<span dir=\"ltr\">%1</span>").arg(value);
    };
    const auto addHijriInformation = [&]()
    {
        if (VisibilityMath::observationalHijriAvailable(cachedHijriResult))
        {
            moon->addToExtraInfoString(
                StelObject::OtherCoord,
                tr("Hijri date: %1<br/>")
                    .arg(QStringLiteral("<b>%1</b>")
                             .arg(ltrValue(hijriDateText))));
            if (cachedHijriResult.latitudePolicy
                == VisibilityMath::HijriLatitudePolicy::FollowLowerLatitude)
            {
                moon->addToExtraInfoString(
                    StelObject::OtherCoord,
                    tr("Follow date of lower latitude.<br/>"));
            }
            if (cachedHijriResult.calculatedPrematureStart)
            {
                moon->addToExtraInfoString(
                    StelObject::OtherCoord,
                    tr("Possible premature start<br/>"));
            }
        }
        else if (cachedHijriResult.availability
                 == VisibilityMath::HijriAvailabilityReason::LatitudeUnsupported)
        {
            moon->addToExtraInfoString(
                StelObject::OtherCoord,
                tr("Hijri date: Not available; follow date of lower latitude<br/>"));
        }
        else
        {
            moon->addToExtraInfoString(
                StelObject::OtherCoord,
                tr("Hijri date: Not available<br/>"));
        }
    };

    const auto currentGeometry = observationalGeometryAtJd(
        core, moon, earth, currentJd);
    const double moonAltitudeDeg = currentGeometry
        ? currentGeometry->moonAltitudeDeg
        : altitudeRad(moon->getAltAzPosGeometric(core)) * RAD2DEG;
    if (!std::isfinite(cachedInformationForJDE)
        || std::abs(currentJde - cachedInformationForJDE) > 0.20)
    {
        double conjunctionJde = 0.0;
        if (findConjunctionFromAnyPhase(
                core, solarSystem, sun, moon, earth, currentJde,
                conjunctionJde))
            cachedInformationConjunctionJDE = conjunctionJde;
        else
            cachedInformationConjunctionJDE =
                std::numeric_limits<double>::quiet_NaN();
        cachedInformationForJDE = currentJde;
    }

    const bool informationAvailable =
        VisibilityMath::moonInformationAvailable(
            moonAltitudeDeg, currentJde,
            cachedInformationConjunctionJDE);
    const auto addMoonParametersHeading = [&]()
    {
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            QStringLiteral("<b>%1</b><br/>")
                .arg(tr("Moon visibility parameters")));
    };

    const auto addMoonParameters = [&](const std::optional<bool>& useEvening)
    {
        const QString width = currentGeometry
            ? QString::number(
                  VisibilityMath::arcminutesToArcseconds(
                      currentGeometry->widthArcmin), 'f', 2)
                  + QStringLiteral("″")
            : QStringLiteral("-");
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("Width W: %1<br/>").arg(ltrValue(width)));

        const QString arcv = informationAvailable && currentGeometry
            ? QString::number(currentGeometry->arcvDeg, 'f', 2)
                  + QStringLiteral("°")
            : QStringLiteral("-");
        const QString daz = informationAvailable && currentGeometry
            ? QString::number(currentGeometry->dazDeg, 'f', 2)
                  + QStringLiteral("°")
            : QStringLiteral("-");
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("ARCV: %1<br/>").arg(ltrValue(arcv)));
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("DAZ: %1<br/>").arg(ltrValue(daz)));

        if (useEvening)
        {
            const bool lagAvailable = *useEvening
                ? cachedEveningLagAvailable
                : cachedMorningLagAvailable;
            const double lagDays = *useEvening
                ? cachedEveningLagDays
                : cachedMorningLagDays;
            const QString lag = informationAvailable && lagAvailable
                ? QString::fromStdString(
                      VisibilityMath::formatSignedDuration(lagDays))
                : QStringLiteral("-");
            moon->addToExtraInfoString(
                StelObject::OtherCoord,
                (*useEvening ? tr("Evening lag: %1<br/>")
                             : tr("Morning lag: %1<br/>"))
                    .arg(ltrValue(lag)));
        }

        QString geocentricAge = QStringLiteral("-");
        QString topocentricAge = QStringLiteral("-");
        if (std::isfinite(cachedInformationConjunctionJDE))
        {
            geocentricAge = QString::fromStdString(
                VisibilityMath::formatConjunctionAge(
                    currentJde - cachedInformationConjunctionJDE));
            double topocentricConjunctionJde = 0.0;
            if (findTopocentricConjunction(
                    core, solarSystem, sun, moon, earth,
                    cachedInformationConjunctionJDE,
                    topocentricConjunctionJde))
            {
                topocentricAge = QString::fromStdString(
                    VisibilityMath::formatConjunctionAge(
                        currentJde - topocentricConjunctionJde));
            }
        }
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("Age from (Geo) Conjunction: %1<br/>")
                .arg(ltrValue(geocentricAge)));
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("Age from (Topo) Conjunction: %1<br/>")
                .arg(ltrValue(topocentricAge)));

        const double deltaT = core->computeDeltaT(currentJd);
        const QString deltaTText = std::isfinite(deltaT)
            ? QString::number(deltaT, 'f', 2)
            : QStringLiteral("-");
        moon->addToExtraInfoString(
            StelObject::OtherCoord,
            tr("ΔT (TT−UT1): %1 s<br/>")
                .arg(ltrValue(deltaTText)));
    };

    const double vNow = currentGeometry
        ? currentGeometry->v
        : observationalVAtJd(core, moon, earth, currentJd);

    const double localDay = std::floor(
        core->getJD() + core->getUTCOffset(core->getJD()) / 24.0 + 0.5);
    if (!std::isfinite(cachedBestLocalDay) || localDay != cachedBestLocalDay
        || location.getLatitude() != cachedBestLatitude
        || location.getLongitude() != cachedBestLongitude
        || location.altitude != cachedBestAltitude)
    {
        // Solar events use the conventional upper-limb threshold encoded as a
        // geometric Sun-center altitude. Lunar events retain the airless
        // mathematical center crossing.
        const Vec4d sunRts = sun->getRTSTime(
            core, VisibilityMath::CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG);
        const Vec4d moonRts = moon->getRTSTime(
            core, MOON_GEOMETRIC_HORIZON_EPSILON_DEG);

        cachedEveningLagAvailable = validSet(sunRts) && validSet(moonRts);
        cachedMorningLagAvailable = validRise(sunRts) && validRise(moonRts);
        cachedEveningLagDays = cachedEveningLagAvailable
            ? moonRts[2] - sunRts[2]
            : std::numeric_limits<double>::quiet_NaN();
        cachedMorningLagDays = cachedMorningLagAvailable
            ? sunRts[0] - moonRts[0]
            : std::numeric_limits<double>::quiet_NaN();

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
            cachedEveningV = observationalVAtJd(
                core, moon, earth, *evening);
        }
        if (morning)
        {
            cachedMorningJd = *morning;
            cachedMorningV = observationalVAtJd(
                core, moon, earth, *morning);
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
    std::optional<bool> bestUsesEvening;
    if (best)
    {
        const bool useEvening = cachedEveningAvailable && *best == cachedEveningJd;
        bestUsesEvening = useEvening;
        cachedBestJd = *best;
        cachedBestV = useEvening ? cachedEveningV : cachedMorningV;
    }

    addHijriInformation();
    addMoonParametersHeading();

    if (!informationAvailable)
    {
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                   tr("V now: -<br/>"));
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                   tr("Best time: -<br/>"));
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                   tr("V at best time: -<br/>"));
        addMoonParameters(bestUsesEvening);
        return;
    }

    moon->addToExtraInfoString(StelObject::OtherCoord,
        tr("V now: %1<br/>").arg(vNow, 0, 'f', 2));

    if (!cachedBestAvailable)
    {
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                  tr("Best time: Not available<br/>"));
        moon->addToExtraInfoString(StelObject::OtherCoord,
                                  tr("V at best time: Not available<br/>"));
        addMoonParameters(bestUsesEvening);
        return;
    }

    const double offset = core->getUTCOffset(cachedBestJd);
    const QString localTime = QString::fromStdString(
        VisibilityMath::formatLocalTime(cachedBestJd, offset));
    moon->addToExtraInfoString(StelObject::OtherCoord,
                              tr("Best time: %1<br/>").arg(localTime));
    moon->addToExtraInfoString(StelObject::OtherCoord,
                              tr("V at best time: %1<br/>").arg(cachedBestV, 0, 'f', 2));
    addMoonParameters(bestUsesEvening);
}

void VisibilityContours::navigateForward()
{
    navigateToCrescent(1, VisibilityMath::NavigationMode::MoonUpOnly);
}

void VisibilityContours::navigateBackward()
{
    navigateToCrescent(-1, VisibilityMath::NavigationMode::MoonUpOnly);
}

void VisibilityContours::navigateAllForward()
{
    navigateToCrescent(1, VisibilityMath::NavigationMode::MoonUpOrDown);
}

void VisibilityContours::navigateAllBackward()
{
    navigateToCrescent(-1, VisibilityMath::NavigationMode::MoonUpOrDown);
}

void VisibilityContours::navigateToCrescent(
    int direction, VisibilityMath::NavigationMode mode)
{
    StelCore* core = StelApp::getInstance().getCore();
    if (!core || direction == 0 || !navigatorDialog)
        return;

    const PlanetP currentPlanet = core->getCurrentPlanet();
    if (!currentPlanet || currentPlanet->getEnglishName() != QStringLiteral("Earth"))
    {
        navigatorEarthAvailable = false;
        navigatorDialog->setStatusMessage(
            CrescentNavigatorDialog::StatusMessage::EarthOnly);
        navigatorDialog->setNavigationEnabled(false);
        return;
    }

    SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
    StelMovementMgr* movement = GETSTELMODULE(StelMovementMgr);
    StelObjectMgr* objectMgr = GETSTELMODULE(StelObjectMgr);
    if (!solarSystem || !movement || !objectMgr)
    {
        navigatorDialog->setStatusMessage(
            CrescentNavigatorDialog::StatusMessage::Unavailable);
        return;
    }

    const PlanetP sun = solarSystem->getSun();
    const PlanetP moon = solarSystem->getMoon();
    const PlanetP earth = solarSystem->getEarth();
    if (!sun || !moon || !earth)
    {
        navigatorDialog->setStatusMessage(
            CrescentNavigatorDialog::StatusMessage::Unavailable);
        return;
    }

    const double currentJd = core->getJD();
    double conjunctionJde = 0.0;
    if (!findConjunctionFromAnyPhase(
            core, solarSystem, sun, moon, earth, core->getJDE(),
            conjunctionJde))
    {
        navigatorDialog->setStatusMessage(
            CrescentNavigatorDialog::StatusMessage::NotFound);
        return;
    }

    std::optional<VisibilityMath::CrescentEvent> destination;
    for (int lunation = 0; lunation < MAX_NAVIGATOR_LUNATIONS; ++lunation)
    {
        const auto events = crescentTransitionEventsForConjunction(
            core, sun, moon, earth, conjunctionJde, mode);
        destination = VisibilityMath::adjacentCrescentEvent(
            events, currentJd, direction, navigatorEventFilter,
            NAVIGATION_EPSILON_DAYS);
        if (destination)
            break;

        double adjacentJde = 0.0;
        if (!findAdjacentConjunction(
                core, solarSystem, sun, moon, earth, conjunctionJde,
                direction, adjacentJde))
            break;
        conjunctionJde = adjacentJde;
    }

    if (!destination)
    {
        navigatorDialog->setStatusMessage(
            CrescentNavigatorDialog::StatusMessage::NotFound);
        return;
    }

    const double preservedFov = movement->getCurrentFov();
    core->setJD(destination->jd);
    core->setTimeRate(0.0);
    core->update(0.0);

    objectMgr->setFlagSelectedObjectPointer(true);
    solarSystem->setFlagPointer(true);
    if (settings)
    {
        settings->setValue(QStringLiteral("viewing/flag_show_selection_marker"), true);
        settings->setValue(QStringLiteral("astro/flag_planets_pointers"), true);
        settings->sync();
    }

    objectMgr->setSelectedObject(qSharedPointerCast<StelObject>(moon),
                                 StelModule::ReplaceSelection);
    const QList<StelObjectP>& selected = objectMgr->getSelectedObject();
    if (!selected.isEmpty())
    {
        const float duration = movement->getAutoMoveDuration();
        movement->moveToObject(selected.first(), duration);
        movement->setFlagTracking(true);
        movement->zoomTo(preservedFov, duration);
    }

    const double utcOffset = core->getUTCOffset(destination->jd);
    int year = 0;
    int month = 0;
    int day = 0;
    StelUtils::getDateFromJulianDay(destination->jd + utcOffset / 24.0,
                                    &year, &month, &day);
    const QString localDate = QStringLiteral("%1-%2-%3")
                                  .arg(year, 4, 10, QLatin1Char('0'))
                                  .arg(month, 2, 10, QLatin1Char('0'))
                                  .arg(day, 2, 10, QLatin1Char('0'));
    const auto hijri = VisibilityMath::hijriMonthYearForEvent(
        year, month, day, destination->kind);
    const bool gregorianCalendar =
        VisibilityMath::isGregorianCalendarDate(year, month, day);
    navigatorDialog->setEventStatus(destination->kind,
                                    localDate, gregorianCalendar,
                                    hijri ? hijri->year : 0,
                                    hijri ? hijri->month : 0);
    qInfo() << "VisibilityContours navigator selected"
            << (destination->kind == VisibilityMath::CrescentEventKind::Morning
                    ? "Morning" : "Evening")
            << "day" << destination->dayIndex
            << "basis" << static_cast<int>(destination->basis)
            << "filter" << VisibilityMath::eventFilterKey(navigatorEventFilter)
            << "JD" << destination->jd;
}

void VisibilityContours::updateNavigatorAvailability(StelCore* core)
{
    if (!core || !navigatorDialog)
        return;
    const PlanetP currentPlanet = core->getCurrentPlanet();
    const bool onEarth = currentPlanet
                         && currentPlanet->getEnglishName() == QStringLiteral("Earth");
    if (onEarth == navigatorEarthAvailable)
        return;
    navigatorEarthAvailable = onEarth;
    navigatorDialog->setNavigationEnabled(onEarth);
    navigatorDialog->setStatusMessage(
        onEarth ? CrescentNavigatorDialog::StatusMessage::Ready
                : CrescentNavigatorDialog::StatusMessage::EarthOnly);
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

    updateNavigatorAvailability(core);

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

    // Geometric/airless Sun altitude. The shared -0.8333 deg center-altitude
    // threshold represents conventional observed sunrise/sunset of the upper limb.
    Vec3d sunAltAz = sun->getAltAzPosGeometric(core);
    sunAltAz.normalize();
    double sunAzRad = 0.0;
    double sunAltRad = 0.0;
    StelUtils::rectToSphe(&sunAzRad, &sunAltRad, sunAltAz);
    const double sunAltDeg = sunAltRad * RAD2DEG;
    if (sunAltDeg > VisibilityMath::CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG)
        return;

    // Determine the signed offset from the nearest apparent geocentric
    // longitude conjunction in the true ecliptic/equinox of date.
    const double jde = core->getJDE();
    if (!std::isfinite(cachedForJDE) || std::abs(jde - cachedForJDE) > 0.20)
    {
        double conjunction = 0.0;
        if (findNearestConjunction(
                core, solarSystem, sun, moon, earth, jde, conjunction))
            cachedConjunctionJDE = conjunction;
        else
            cachedConjunctionJDE = std::numeric_limits<double>::quiet_NaN();
        cachedForJDE = jde;
    }

    if (!std::isfinite(cachedConjunctionJDE))
        return;

    const double daysFromConjunction = jde - cachedConjunctionJDE;
    const int dayIndex = static_cast<int>(std::lround(daysFromConjunction));

    // Seven requested phase-day bins only: -3, -2, -1, 0, +1, +2, +3.
    if (dayIndex < -3 || dayIndex > 3)
        return;

    const StelProjectorP projector = core->getProjection(StelCore::FrameAltAz,
                                                          StelCore::RefractionOff);
    StelPainter painter(projector);
    painter.setBlending(true);
    painter.setLineSmooth(true);
    painter.setLineWidth(2.2f);
    QFont overlayFont = QGuiApplication::font();
    overlayFont.setPixelSize(StelApp::getInstance().getScreenFontSize());
    painter.setFont(overlayFont);

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

    // Signed continuous age from the apparent geocentric conjunction.
    QString conjunctionAge = QString::fromStdString(
        VisibilityMath::formatConjunctionAge(daysFromConjunction));
    const QString language =
        StelApp::getInstance().getLocaleMgr().getAppLanguage();
    if (VisibilityMath::useArabicForProgramLanguage(language.toStdString()))
    {
        // Keep the sign, Western numerals, and h/m suffixes in LTR order
        // inside the surrounding Arabic sentence.
        conjunctionAge = QStringLiteral("\u2066%1\u2069").arg(conjunctionAge);
    }
    painter.setColor(1.0f, 1.0f, 1.0f, 0.90f);
    painter.drawText(sunAltAz,
                     tr("Age from (Geo) Conjunction = %1")
                         .arg(conjunctionAge),
                     0.0f, 10.0f, -18.0f, true);
}
