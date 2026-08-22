#include "VisibilityMath.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace VisibilityMath
{
namespace
{
constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double GREGORIAN_EPOCH = 1721425.5;
constexpr double ISLAMIC_EPOCH = 1948439.5;

bool isGregorianLeapYear(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool isValidGregorianDate(int year, int month, int day)
{
    if (month < 1 || month > 12 || day < 1)
        return false;
    constexpr int monthLengths[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    const int maximumDay = monthLengths[month - 1]
                           + (month == 2 && isGregorianLeapYear(year) ? 1 : 0);
    return day <= maximumDay;
}

double gregorianToJd(int year, int month, int day)
{
    int leapAdjustment = 0;
    if (month > 2)
        leapAdjustment = isGregorianLeapYear(year) ? -1 : -2;
    const double previousYear = static_cast<double>(year) - 1.0;
    return GREGORIAN_EPOCH - 1.0
           + 365.0 * previousYear
           + std::floor(previousYear / 4.0)
           - std::floor(previousYear / 100.0)
           + std::floor(previousYear / 400.0)
           + std::floor((((367.0 * month) - 362.0) / 12.0)
                        + leapAdjustment + day);
}

double islamicToJd(int year, int month, int day)
{
    return day + std::ceil(29.5 * (month - 1))
           + (static_cast<double>(year) - 1.0) * 354.0
           + std::floor((3.0 + 11.0 * year) / 30.0)
           + ISLAMIC_EPOCH - 1.0;
}
}

bool useArabicForProgramLanguage(const std::string& languageCode)
{
    return languageCode.size() >= 2
           && (languageCode[0] == 'a' || languageCode[0] == 'A')
           && (languageCode[1] == 'r' || languageCode[1] == 'R');
}

bool isGregorianCalendarDate(int year, int month, int day)
{
    return year > 1582
           || (year == 1582
               && (month > 10 || (month == 10 && day >= 15)));
}

EventFilter eventFilterFromString(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });
    if (normalized == "morning")
        return EventFilter::Morning;
    if (normalized == "evening")
        return EventFilter::Evening;
    return EventFilter::Both;
}

const char* eventFilterKey(EventFilter filter)
{
    switch (filter)
    {
    case EventFilter::Morning:
        return "morning";
    case EventFilter::Evening:
        return "evening";
    case EventFilter::Both:
    default:
        return "both";
    }
}

const char* eventFilterName(EventFilter filter)
{
    switch (filter)
    {
    case EventFilter::Morning:
        return "Morning";
    case EventFilter::Evening:
        return "Evening";
    case EventFilter::Both:
    default:
        return "Both";
    }
}

bool eventMatchesFilter(CrescentEventKind kind, EventFilter filter)
{
    return filter == EventFilter::Both
           || (filter == EventFilter::Morning
               && kind == CrescentEventKind::Morning)
           || (filter == EventFilter::Evening
               && kind == CrescentEventKind::Evening);
}

std::optional<HijriMonthYear> hijriMonthYearForEvent(
    int gregorianYear, int gregorianMonth, int gregorianDay,
    CrescentEventKind kind)
{
    if (!isValidGregorianDate(gregorianYear, gregorianMonth, gregorianDay))
        return std::nullopt;

    const double eventDateJd = gregorianToJd(
        gregorianYear, gregorianMonth, gregorianDay);
    const double shiftedJd = eventDateJd
                             + (kind == CrescentEventKind::Morning ? -10.0 : 10.0);
    const double normalizedJd = std::floor(shiftedJd) + 0.5;
    const double yearValue = std::floor(
        ((30.0 * (normalizedJd - ISLAMIC_EPOCH)) + 10646.0) / 10631.0);
    if (!std::isfinite(yearValue)
        || yearValue < std::numeric_limits<int>::min()
        || yearValue > std::numeric_limits<int>::max())
        return std::nullopt;

    const int year = static_cast<int>(yearValue);
    const double monthValue = std::min(
        12.0,
        std::ceil((normalizedJd - (29.0 + islamicToJd(year, 1, 1))) / 29.5)
            + 1.0);
    if (!std::isfinite(monthValue) || monthValue < 1.0 || monthValue > 12.0)
        return std::nullopt;
    return HijriMonthYear{year, static_cast<int>(monthValue)};
}

bool validHijriDate(const HijriDate& date)
{
    return date.year != 0
           && date.month >= 1 && date.month <= 12
           && date.day >= 1 && date.day <= 30;
}

bool sameHijriDate(const HijriDate& first, const HijriDate& second)
{
    return first.year == second.year
           && first.month == second.month
           && first.day == second.day;
}

HijriDate advanceHijriDateAtSunset(const HijriDate& current,
                                   bool criterionMet)
{
    if (!validHijriDate(current))
        return {0, 0, 0};

    const bool criterionStart = criterionMet && current.day >= 28;
    if (!criterionStart && current.day < 30)
        return {current.year, current.month, current.day + 1};

    int nextYear = current.year;
    int nextMonth = current.month + 1;
    if (nextMonth > 12)
    {
        nextMonth = 1;
        nextYear += nextYear == -1 ? 2 : 1;
    }
    return {nextYear, nextMonth, 1};
}

ObservationalHijriDate advanceObservationalHijriAtSunset(
    const ObservationalHijriDate& current,
    const std::optional<double>& eveningBestTimeV)
{
    const bool finiteV = eveningBestTimeV
                         && std::isfinite(*eveningBestTimeV);
    const bool calculatedMet = finiteV
                               && *eveningBestTimeV
                                      >= HIJRI_CALCULATED_START_V;
    const bool observedMet = finiteV
                             && *eveningBestTimeV
                                    >= HIJRI_OBSERVED_START_V;
    return {
        advanceHijriDateAtSunset(current.calculated, calculatedMet),
        advanceHijriDateAtSunset(current.observed, observedMet)
    };
}

bool hijriMonthStartEligible(int currentDay)
{
    return currentDay >= 28 && currentDay <= 30;
}

bool hijriForcedRolloverDue(long long monthStartLocalDay,
                            long long targetLocalDay)
{
    return targetLocalDay >= monthStartLocalDay
           && targetLocalDay - monthStartLocalDay >= 30;
}

std::optional<double> selectPrecedingConjunction(
    double currentJde, double nearestConjunctionJde,
    const std::optional<double>& previousConjunctionJde)
{
    if (!std::isfinite(currentJde))
        return std::nullopt;
    if (std::isfinite(nearestConjunctionJde)
        && nearestConjunctionJde <= currentJde)
        return nearestConjunctionJde;
    if (previousConjunctionJde
        && std::isfinite(*previousConjunctionJde)
        && *previousConjunctionJde <= currentJde)
        return *previousConjunctionJde;
    return std::nullopt;
}

bool lunationCacheCoversJde(double currentJde,
                            double conjunctionJde,
                            double nextConjunctionJde)
{
    return std::isfinite(currentJde)
           && std::isfinite(conjunctionJde)
           && std::isfinite(nextConjunctionJde)
           && nextConjunctionJde > conjunctionJde
           && currentJde >= conjunctionJde
           && currentJde < nextConjunctionJde;
}

namespace
{
struct HijriTrackState
{
    bool initialized = false;
    long long monthStartLocalDay = 0;
    int year = 0;
    int month = 0;
};

void incrementHijriMonth(HijriTrackState& state)
{
    ++state.month;
    if (state.month > 12)
    {
        state.month = 1;
        state.year += state.year == -1 ? 2 : 1;
    }
}

void forceHijriTrackThrough(HijriTrackState& state,
                            long long targetLocalDay)
{
    while (state.initialized
           && hijriForcedRolloverDue(state.monthStartLocalDay,
                                     targetLocalDay))
    {
        state.monthStartLocalDay += 30;
        incrementHijriMonth(state);
    }
}

std::optional<HijriDate> hijriTrackFromLunationEvents(
    const std::vector<HijriVisibilityEvent>& events,
    double threshold, long long currentSunsetDay)
{
    HijriTrackState state;
    std::size_t groupStart = 0;
    while (groupStart < events.size())
    {
        std::size_t groupEnd = groupStart + 1;
        while (groupEnd < events.size()
               && std::abs(events[groupEnd].conjunctionJde
                           - events[groupStart].conjunctionJde) < 1e-6)
            ++groupEnd;

        if (!state.initialized)
        {
            for (std::size_t index = groupStart; index < groupEnd; ++index)
            {
                const auto& event = events[index];
                if (event.v < threshold)
                    continue;
                const auto month = hijriMonthYearForEvent(
                    event.gregorianYear, event.gregorianMonth,
                    event.gregorianDay, CrescentEventKind::Evening);
                if (!month)
                    continue;
                state = {true, event.localDay, month->year, month->month};
                break;
            }
            groupStart = groupEnd;
            continue;
        }

        for (std::size_t index = groupStart; index < groupEnd; ++index)
        {
            const auto& event = events[index];
            forceHijriTrackThrough(state, event.localDay);
            const long long day = event.localDay
                                  - state.monthStartLocalDay + 1;
            if (event.v >= threshold
                && hijriMonthStartEligible(static_cast<int>(day)))
            {
                state.monthStartLocalDay = event.localDay;
                incrementHijriMonth(state);
                break;
            }
        }
        groupStart = groupEnd;
    }

    if (!state.initialized)
        return std::nullopt;
    forceHijriTrackThrough(state, currentSunsetDay);
    const long long day = currentSunsetDay - state.monthStartLocalDay + 1;
    if (day < 1 || day > 30)
        return std::nullopt;
    return HijriDate{state.year, state.month, static_cast<int>(day)};
}
}

std::optional<ObservationalHijriDate> observationalHijriFromLunationEvents(
    const std::vector<HijriVisibilityEvent>& inputEvents,
    long long currentSunsetDay, double currentJd,
    double historyStartJd)
{
    if (!std::isfinite(currentJd) || !std::isfinite(historyStartJd)
        || historyStartJd > currentJd)
        return std::nullopt;

    std::vector<HijriVisibilityEvent> events;
    events.reserve(inputEvents.size());
    for (const auto& event : inputEvents)
    {
        if (event.localDay > currentSunsetDay
            || !std::isfinite(event.conjunctionJde)
            || !std::isfinite(event.sunsetJd)
            || !std::isfinite(event.bestTimeJd)
            || !std::isfinite(event.bestTimeJde)
            || !std::isfinite(event.v)
            || event.sunsetJd < historyStartJd
            || event.sunsetJd > currentJd
            || event.bestTimeJde <= event.conjunctionJde
            || event.dayIndex < 0 || event.dayIndex > 3)
            continue;
        events.push_back(event);
    }
    std::sort(events.begin(), events.end(),
              [](const auto& first, const auto& second)
              {
                  if (first.conjunctionJde != second.conjunctionJde)
                      return first.conjunctionJde < second.conjunctionJde;
                  if (first.sunsetJd != second.sunsetJd)
                      return first.sunsetJd < second.sunsetJd;
                  return first.v < second.v;
              });
    if (events.empty())
        return std::nullopt;

    const auto calculated = hijriTrackFromLunationEvents(
        events, HIJRI_CALCULATED_START_V, currentSunsetDay);
    const auto observed = hijriTrackFromLunationEvents(
        events, HIJRI_OBSERVED_START_V, currentSunsetDay);
    if (!calculated || !observed)
        return std::nullopt;
    return ObservationalHijriDate{*calculated, *observed};
}

namespace
{
std::string paddedInteger(int value, int width)
{
    std::ostringstream output;
    output << std::setfill('0') << std::setw(width) << value;
    return output.str();
}

std::string formattedHijriYear(int year)
{
    if (year >= 0)
        return paddedInteger(year, 4);
    return "-" + paddedInteger(-year, 4);
}
}

std::string formatHijriDate(const HijriDate& date)
{
    if (!validHijriDate(date))
        return {};
    return paddedInteger(date.day, 2) + "/"
           + paddedInteger(date.month, 2) + "/"
           + formattedHijriYear(date.year);
}

std::string formatObservationalHijriDate(
    const ObservationalHijriDate& date)
{
    const std::string calculated = formatHijriDate(date.calculated);
    const std::string observed = formatHijriDate(date.observed);
    if (calculated.empty() || observed.empty())
        return {};
    if (sameHijriDate(date.calculated, date.observed))
        return calculated;
    return calculated + " - " + observed;
}

bool sunsetHasOccurred(double currentJd, double sunsetJd)
{
    return std::isfinite(currentJd) && std::isfinite(sunsetJd)
           && currentJd >= sunsetJd;
}

double odehPolynomial(double widthArcmin)
{
    return -0.1018 * widthArcmin * widthArcmin * widthArcmin
           + 0.7319 * widthArcmin * widthArcmin
           - 6.3226 * widthArcmin + 7.1651;
}

double odehValue(double arcvDeg, double widthArcmin)
{
    return arcvDeg - odehPolynomial(widthArcmin);
}

double theoreticalWidth(double arcvDeg, double dazDeg)
{
    return 15.0 * (1.0 - std::cos(arcvDeg * DEG2RAD)
                         * std::cos(dazDeg * DEG2RAD));
}

double illuminatedWidth(double illuminatedFraction, double angularDiameterDeg)
{
    if (!std::isfinite(illuminatedFraction) || !std::isfinite(angularDiameterDeg))
        return std::numeric_limits<double>::quiet_NaN();
    return std::clamp(illuminatedFraction, 0.0, 1.0)
           * std::max(0.0, angularDiameterDeg) * 60.0;
}

double horizontalParallaxDeg(double moonDistanceAu, double earthRadiusAu)
{
    if (!(moonDistanceAu > 0.0) || !(earthRadiusAu > 0.0)
        || earthRadiusAu >= moonDistanceAu)
        return std::numeric_limits<double>::quiet_NaN();
    return std::asin(earthRadiusAu / moonDistanceAu) * RAD2DEG;
}

double yallopBoundaryAsOdehV(double q, double hpDeg)
{
    return 10.0 * q + YALLOP_ODEH_OFFSET_DEG - hpDeg;
}

std::array<double, 4> yallopBoundaries(double hpDeg)
{
    constexpr std::array<double, 4> q = {{-0.232, -0.160, -0.014, 0.216}};
    std::array<double, 4> result{};
    for (std::size_t i = 0; i < q.size(); ++i)
        result[i] = yallopBoundaryAsOdehV(q[i], hpDeg);
    return result;
}

std::array<double, 4> fallbackYallopBoundaries()
{
    return yallopBoundaries(1.0);
}

int categoryIndex(double value, const double* lowerBoundaries, std::size_t count)
{
    if (!std::isfinite(value) || !lowerBoundaries || count == 0
        || value < lowerBoundaries[0])
        return -1;
    for (std::size_t i = count; i-- > 0;)
    {
        if (value >= lowerBoundaries[i])
            return static_cast<int>(i);
    }
    return -1;
}

std::string formatLocalTime(double jd, double utcOffsetHours)
{
    if (!std::isfinite(jd) || !std::isfinite(utcOffsetHours))
        return {};

    double localDayFraction = std::fmod(jd + 0.5 + utcOffsetHours / 24.0, 1.0);
    if (localDayFraction < 0.0)
        localDayFraction += 1.0;
    // Match StelUtils::getTimeFromJulianDay(), used by Stellarium's bottom
    // clock: retain the containing second instead of rounding into the next.
    int seconds = static_cast<int>(
        std::floor(localDayFraction * 86400.0 + 0.0001));
    seconds %= 86400;

    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainingSeconds = seconds % 60;
    std::ostringstream output;
    output << hours << 'h' << std::setfill('0') << std::setw(2) << minutes
           << 'm' << std::setw(2) << remainingSeconds << 's';
    return output.str();
}

std::optional<double> eveningBestTime(double sunsetJd, double moonsetJd)
{
    const double lag = moonsetJd - sunsetJd;
    if (!std::isfinite(lag) || lag <= 0.0)
        return std::nullopt;
    return sunsetJd + (4.0 / 9.0) * lag;
}

std::optional<double> morningBestTime(double sunriseJd, double moonriseJd)
{
    const double lag = sunriseJd - moonriseJd;
    if (!std::isfinite(lag) || lag <= 0.0)
        return std::nullopt;
    return sunriseJd - (4.0 / 9.0) * lag;
}

std::optional<double> nearestTime(double currentJd,
                                  const std::optional<double>& evening,
                                  const std::optional<double>& morning)
{
    if (evening && morning)
        return std::abs(*evening - currentJd) <= std::abs(*morning - currentJd)
                   ? evening : morning;
    if (evening)
        return evening;
    return morning;
}

int conjunctionDayIndex(double eventJde, double conjunctionJde)
{
    if (!std::isfinite(eventJde) || !std::isfinite(conjunctionJde))
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::lround(eventJde - conjunctionJde));
}

bool eventInConjunctionWindow(double eventJde, double conjunctionJde)
{
    const int dayIndex = conjunctionDayIndex(eventJde, conjunctionJde);
    return dayIndex >= -3 && dayIndex <= 3;
}

bool moonIsUp(double moonAltitudeDeg)
{
    return std::isfinite(moonAltitudeDeg) && moonAltitudeDeg > 0.0;
}

bool moonInformationAvailable(double moonAltitudeDeg, double currentJde,
                              double conjunctionJde)
{
    return moonIsUp(moonAltitudeDeg)
           && eventInConjunctionWindow(currentJde, conjunctionJde);
}

bool validCrescentEvent(double eventJd, double eventJde, double conjunctionJde,
                        double moonAltitudeDeg)
{
    return std::isfinite(eventJd) && moonIsUp(moonAltitudeDeg)
           && eventInConjunctionWindow(eventJde, conjunctionJde);
}

std::optional<NavigationTime> chooseNavigationTime(
    NavigationMode mode, CrescentEventKind kind, double solarEventJd,
    const std::optional<double>& bestTimeJd, double bestTimeMoonAltitudeDeg)
{
    const bool validBestTime = bestTimeJd && std::isfinite(*bestTimeJd)
                               && moonIsUp(bestTimeMoonAltitudeDeg);
    if (validBestTime)
        return NavigationTime{*bestTimeJd, EventTimeBasis::BestTime};
    if (mode == NavigationMode::MoonUpOnly || !std::isfinite(solarEventJd))
        return std::nullopt;
    if (kind == CrescentEventKind::Morning)
        return NavigationTime{solarEventJd, EventTimeBasis::Sunrise};
    return NavigationTime{solarEventJd + POST_SUNSET_NAVIGATION_MARGIN_DAYS,
                          EventTimeBasis::Sunset};
}

void sortCrescentEvents(std::vector<CrescentEvent>& events)
{
    std::sort(events.begin(), events.end(), [](const CrescentEvent& a,
                                                const CrescentEvent& b)
    {
        if (a.jd != b.jd)
            return a.jd < b.jd;
        return a.kind == CrescentEventKind::Morning
               && b.kind == CrescentEventKind::Evening;
    });
}

std::optional<CrescentEvent> adjacentCrescentEvent(
    const std::vector<CrescentEvent>& events, double currentJd,
    int direction, EventFilter filter, double epsilonDays)
{
    if (!std::isfinite(currentJd) || !(epsilonDays >= 0.0) || direction == 0)
        return std::nullopt;

    if (direction > 0)
    {
        for (const CrescentEvent& event : events)
        {
            if (eventMatchesFilter(event.kind, filter)
                && event.jd > currentJd + epsilonDays)
                return event;
        }
        return std::nullopt;
    }

    for (auto it = events.rbegin(); it != events.rend(); ++it)
    {
        if (eventMatchesFilter(it->kind, filter)
            && it->jd < currentJd - epsilonDays)
            return *it;
    }
    return std::nullopt;
}
}
