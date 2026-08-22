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
// Evening solar-event fallbacks land just after sunset so calculations whose
// civil-day boundary is sunset cannot remain on the preceding Hijri date.
constexpr double POST_SUNSET_NAVIGATION_MARGIN_DAYS = 5.0 / 86400.0;
constexpr double HIJRI_CALCULATED_START_V = 1.35;
constexpr double HIJRI_OBSERVED_START_V = 5.83;

bool useArabicForProgramLanguage(const std::string& languageCode);
bool isGregorianCalendarDate(int year, int month, int day);

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

enum class EventFilter
{
    Both,
    Morning,
    Evening
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

struct HijriMonthYear
{
    int year;
    int month;
};

struct HijriDate
{
    int year;
    int month;
    int day;
};

struct ObservationalHijriDate
{
    HijriDate calculated;
    HijriDate observed;
};

struct HijriVisibilityEvent
{
    long long localDay;
    int gregorianYear;
    int gregorianMonth;
    int gregorianDay;
    double conjunctionJde;
    double sunsetJd;
    double bestTimeJd;
    double bestTimeJde;
    int dayIndex;
    double v;
};

EventFilter eventFilterFromString(const std::string& value);
const char* eventFilterKey(EventFilter filter);
const char* eventFilterName(EventFilter filter);
bool eventMatchesFilter(CrescentEventKind kind, EventFilter filter);
std::optional<HijriMonthYear> hijriMonthYearForEvent(
    int gregorianYear, int gregorianMonth, int gregorianDay,
    CrescentEventKind kind);
bool validHijriDate(const HijriDate& date);
bool sameHijriDate(const HijriDate& first, const HijriDate& second);
HijriDate advanceHijriDateAtSunset(const HijriDate& current,
                                   bool criterionMet);
ObservationalHijriDate advanceObservationalHijriAtSunset(
    const ObservationalHijriDate& current,
    const std::optional<double>& eveningBestTimeV);
bool hijriMonthStartEligible(int currentDay);
bool hijriForcedRolloverDue(long long monthStartLocalDay,
                            long long targetLocalDay);
std::optional<double> selectPrecedingConjunction(
    double currentJde, double nearestConjunctionJde,
    const std::optional<double>& previousConjunctionJde);
bool lunationCacheCoversJde(double currentJde,
                            double conjunctionJde,
                            double nextConjunctionJde);
std::optional<ObservationalHijriDate> observationalHijriFromLunationEvents(
    const std::vector<HijriVisibilityEvent>& events,
    long long currentSunsetDay, double currentJd,
    double historyStartJd);
std::string formatHijriDate(const HijriDate& date);
std::string formatObservationalHijriDate(
    const ObservationalHijriDate& date);
bool sunsetHasOccurred(double currentJd, double sunsetJd);

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
bool moonInformationAvailable(double moonAltitudeDeg, double currentJde,
                              double conjunctionJde);
bool validCrescentEvent(double eventJd, double eventJde, double conjunctionJde,
                        double moonAltitudeDeg);
std::optional<NavigationTime> chooseNavigationTime(
    NavigationMode mode, CrescentEventKind kind, double solarEventJd,
    const std::optional<double>& bestTimeJd, double bestTimeMoonAltitudeDeg);
void sortCrescentEvents(std::vector<CrescentEvent>& events);
std::optional<CrescentEvent> adjacentCrescentEvent(
    const std::vector<CrescentEvent>& events, double currentJd,
    int direction, EventFilter filter = EventFilter::Both,
    double epsilonDays = 1.0 / 86400.0);
}

#endif
