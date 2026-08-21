#ifndef VISIBILITYMATH_HPP
#define VISIBILITYMATH_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace VisibilityMath
{
constexpr double YALLOP_ODEH_OFFSET_DEG = 4.672;
constexpr double CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG = -0.8333;

bool useArabicForProgramLanguage(const std::string& languageCode);

enum class CrescentEventKind
{
    Morning,
    Evening
};

enum class NavigationMode
{
    MoonUpOnly,
    MoonUpOrDown
};

enum class EventTimeBasis
{
    BestTime,
    Sunrise,
    Sunset
};

struct NavigationTime
{
    double jd;
    EventTimeBasis basis;
};

struct CrescentEvent
{
    double jd;
    double conjunctionJde;
    int dayIndex;
    CrescentEventKind kind;
    NavigationMode mode = NavigationMode::MoonUpOnly;
    EventTimeBasis basis = EventTimeBasis::BestTime;
};

double odehPolynomial(double widthArcmin);
double odehValue(double arcvDeg, double widthArcmin);
double theoreticalWidth(double arcvDeg, double dazDeg);
double illuminatedWidth(double illuminatedFraction, double angularDiameterDeg);
double horizontalParallaxDeg(double moonDistanceAu, double earthRadiusAu);
double yallopBoundaryAsOdehV(double q, double horizontalParallaxDeg);
std::array<double, 4> yallopBoundaries(double horizontalParallaxDeg);
std::array<double, 4> fallbackYallopBoundaries();
int categoryIndex(double value, const double* lowerBoundaries, std::size_t count);
std::string formatLocalTime(double jd, double utcOffsetHours);

std::optional<double> eveningBestTime(double sunsetJd, double moonsetJd);
std::optional<double> morningBestTime(double sunriseJd, double moonriseJd);
std::optional<double> nearestTime(double currentJd,
                                  const std::optional<double>& evening,
                                  const std::optional<double>& morning);
int conjunctionDayIndex(double eventJde, double conjunctionJde);
bool eventInConjunctionWindow(double eventJde, double conjunctionJde);
bool moonIsUp(double moonAltitudeDeg);
bool validCrescentEvent(double eventJd, double eventJde, double conjunctionJde,
                        double moonAltitudeDeg);
std::optional<NavigationTime> chooseNavigationTime(
    NavigationMode mode, CrescentEventKind kind, double solarEventJd,
    const std::optional<double>& bestTimeJd, double bestTimeMoonAltitudeDeg);
void sortCrescentEvents(std::vector<CrescentEvent>& events);
std::optional<CrescentEvent> adjacentCrescentEvent(
    const std::vector<CrescentEvent>& events, double currentJd,
    int direction, double epsilonDays = 1.0 / 86400.0);
}

#endif
