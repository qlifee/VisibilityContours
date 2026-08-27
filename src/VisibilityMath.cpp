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

bool hijriMonthStartEligible(int currentDay)
{
    return currentDay >= 29 && currentDay <= 30;
}

bool hijriForcedRolloverDue(long long monthStartLocalDay,
                            long long targetLocalDay)
{
    return targetLocalDay >= monthStartLocalDay
           && targetLocalDay - monthStartLocalDay >= 30;
}

HijriLatitudePolicy hijriLatitudePolicy(double latitudeDeg)
{
    if (!std::isfinite(latitudeDeg) || std::abs(latitudeDeg) > 60.0)
        return HijriLatitudePolicy::Unsupported;
    if (std::abs(latitudeDeg) > 55.0)
        return HijriLatitudePolicy::FollowLowerLatitude;
    return HijriLatitudePolicy::Standard;
}

std::optional<int> hijriMaximumConjunctionBin(double latitudeDeg)
{
    if (!std::isfinite(latitudeDeg))
        return std::nullopt;
    const double absoluteLatitude = std::abs(latitudeDeg);
    if (absoluteLatitude > 60.0)
        return std::nullopt;
    if (absoluteLatitude >= 59.0)
        return 5;
    if (absoluteLatitude > 45.0)
        return 4;
    return 3;
}

bool hijriEventInLatitudeWindow(int dayIndex, double latitudeDeg)
{
    const auto maximumBin = hijriMaximumConjunctionBin(latitudeDeg);
    return maximumBin && dayIndex >= 0 && dayIndex <= *maximumBin;
}

bool observationalHijriAvailable(const ObservationalHijriResult& result)
{
    return result.availability == HijriAvailabilityReason::Available
           && validHijriDate(result.date.calculated)
           && validHijriDate(result.date.observed);
}

bool sameObservationalHijriResult(
    const ObservationalHijriResult& first,
    const ObservationalHijriResult& second)
{
    return sameHijriDate(first.date.calculated, second.date.calculated)
           && sameHijriDate(first.date.observed, second.date.observed)
           && first.availability == second.availability
           && first.latitudePolicy == second.latitudePolicy
           && first.calculatedPrematureStart
                  == second.calculatedPrematureStart
           && first.observedPrematureStart == second.observedPrematureStart
           && first.calculatedPrematureScheduled
                  == second.calculatedPrematureScheduled
           && first.observedPrematureScheduled
                  == second.observedPrematureScheduled;
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
    bool activeMonthPremature = false;
    std::optional<long long> scheduledPrematureStartLocalDay;
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

void startNextHijriMonth(HijriTrackState& state, long long localDay,
                         bool premature)
{
    state.monthStartLocalDay = localDay;
    incrementHijriMonth(state);
    state.activeMonthPremature = premature;
    state.scheduledPrematureStartLocalDay.reset();
}

bool validHijriVisibilityEvent(const HijriVisibilityEvent& event,
                               double conjunctionJde,
                               double latitudeDeg)
{
    return std::isfinite(conjunctionJde)
           && std::isfinite(event.conjunctionJde)
           && std::isfinite(event.sunsetJd)
           && std::isfinite(event.bestTimeJd)
           && std::isfinite(event.bestTimeJde)
           && std::isfinite(event.v)
           && std::abs(event.conjunctionJde - conjunctionJde) < 1e-5
           && event.bestTimeJd > event.sunsetJd
           && event.bestTimeJde > conjunctionJde
           && event.dayIndex
                  == conjunctionDayIndex(event.bestTimeJde, conjunctionJde)
           && hijriEventInLatitudeWindow(event.dayIndex, latitudeDeg);
}

std::vector<HijriLunationEvents> normalizedHijriLunations(
    const std::vector<HijriLunationEvents>& input,
    double latitudeDeg)
{
    std::vector<HijriLunationEvents> lunations;
    lunations.reserve(input.size());
    for (const auto& inputLunation : input)
    {
        if (!std::isfinite(inputLunation.conjunctionJde))
            continue;
        HijriLunationEvents lunation{inputLunation.conjunctionJde, {}};
        for (const auto& event : inputLunation.events)
        {
            if (validHijriVisibilityEvent(event,
                                          inputLunation.conjunctionJde,
                                          latitudeDeg))
                lunation.events.push_back(event);
        }
        std::sort(lunation.events.begin(), lunation.events.end(),
                  [](const auto& first, const auto& second)
                  {
                      if (first.sunsetJd != second.sunsetJd)
                          return first.sunsetJd < second.sunsetJd;
                      return first.v < second.v;
                  });
        lunations.push_back(std::move(lunation));
    }
    std::sort(lunations.begin(), lunations.end(),
              [](const auto& first, const auto& second)
              {
                  return first.conjunctionJde < second.conjunctionJde;
              });
    lunations.erase(
        std::unique(lunations.begin(), lunations.end(),
                    [](const auto& first, const auto& second)
                    {
                        return std::abs(first.conjunctionJde
                                        - second.conjunctionJde) < 1e-6;
                    }),
        lunations.end());
    if (lunations.size()
        > static_cast<std::size_t>(MAX_HIJRI_HISTORY_LUNATIONS))
    {
        lunations.erase(
            lunations.begin(),
            lunations.end() - MAX_HIJRI_HISTORY_LUNATIONS);
    }
    return lunations;
}

const HijriVisibilityEvent* firstQualifyingEvent(
    const HijriLunationEvents& lunation, double threshold,
    double currentJd)
{
    for (const auto& event : lunation.events)
    {
        // The visibility is evaluated at best time, but the calendar changes
        // at that evening's conventional sunset, even when best time is later.
        if (event.sunsetJd <= currentJd && event.v >= threshold)
            return &event;
    }
    return nullptr;
}

struct HijriTrackReplay
{
    std::optional<HijriDate> date;
    bool activeMonthPremature = false;
    bool prematureScheduled = false;
};

HijriTrackReplay replayHijriTrack(
    const std::vector<HijriLunationEvents>& lunations,
    double threshold, long long currentSunsetDay, double currentJd)
{
    HijriTrackState state;
    std::size_t anchorLunation = lunations.size();
    for (std::size_t group = 0; group < lunations.size(); ++group)
    {
        const HijriVisibilityEvent* event = firstQualifyingEvent(
            lunations[group], threshold, currentJd);
        if (!event || event->localDay > currentSunsetDay)
            continue;
        const auto month = hijriMonthYearForEvent(
            event->gregorianYear, event->gregorianMonth,
            event->gregorianDay, CrescentEventKind::Evening);
        if (!month)
            continue;
        state.initialized = true;
        state.monthStartLocalDay = event->localDay;
        state.year = month->year;
        state.month = month->month;
        anchorLunation = group;
        break;
    }

    if (!state.initialized)
        return {};

    for (std::size_t group = anchorLunation + 1;
         group < lunations.size(); ++group)
    {
        if (state.scheduledPrematureStartLocalDay
            && *state.scheduledPrematureStartLocalDay <= currentSunsetDay)
        {
            startNextHijriMonth(
                state, *state.scheduledPrematureStartLocalDay, true);
        }

        const HijriVisibilityEvent* event = firstQualifyingEvent(
            lunations[group], threshold, currentJd);
        if (event && event->localDay > currentSunsetDay)
            event = nullptr;

        const long long forcedStartLocalDay =
            state.monthStartLocalDay + 30;
        // A criterion reached at the same sunset as the day-30 fallback wins.
        // If the fallback is earlier, it consumes this numerical lunation.
        if (forcedStartLocalDay <= currentSunsetDay
            && (!event || forcedStartLocalDay < event->localDay))
        {
            startNextHijriMonth(state, forcedStartLocalDay, false);
            continue;
        }

        if (!event)
            continue;

        const long long completedDays =
            event->localDay - state.monthStartLocalDay;
        if (completedDays < 0)
            continue;
        if (completedDays >= 29 && completedDays <= 30)
        {
            startNextHijriMonth(state, event->localDay, false);
            continue;
        }
        if (completedDays < 29)
        {
            state.scheduledPrematureStartLocalDay =
                state.monthStartLocalDay + 29;
            if (*state.scheduledPrematureStartLocalDay <= currentSunsetDay)
            {
                startNextHijriMonth(
                    state, *state.scheduledPrematureStartLocalDay, true);
            }
            continue;
        }

        // A late event cannot create a month longer than 30 days.
        if (forcedStartLocalDay <= currentSunsetDay)
            startNextHijriMonth(state, forcedStartLocalDay, false);
    }

    if (state.scheduledPrematureStartLocalDay
        && *state.scheduledPrematureStartLocalDay <= currentSunsetDay)
    {
        startNextHijriMonth(
            state, *state.scheduledPrematureStartLocalDay, true);
    }
    while (hijriForcedRolloverDue(state.monthStartLocalDay,
                                  currentSunsetDay))
    {
        startNextHijriMonth(
            state, state.monthStartLocalDay + 30, false);
    }

    const long long day = currentSunsetDay - state.monthStartLocalDay + 1;
    if (day < 1 || day > 30)
        return {};
    return {HijriDate{state.year, state.month, static_cast<int>(day)},
            state.activeMonthPremature,
            state.scheduledPrematureStartLocalDay.has_value()};
}
}

std::array<bool, 2> hijriHistoryAnchorCoverage(
    const std::vector<HijriLunationEvents>& inputLunations,
    double currentJd, double latitudeDeg)
{
    std::array<bool, 2> found{{false, false}};
    if (!std::isfinite(currentJd)
        || !hijriMaximumConjunctionBin(latitudeDeg))
        return found;
    const auto lunations = normalizedHijriLunations(
        inputLunations, latitudeDeg);
    // The newest group is the active lunation being evaluated. Anchoring from
    // it would discard the preceding calendar state and make the result depend
    // on whether the cache was opened before or after its visibility crossing.
    if (lunations.size() < 2)
        return found;
    for (std::size_t group = 0; group + 1 < lunations.size(); ++group)
    {
        for (const auto& event : lunations[group].events)
        {
            if (event.sunsetJd > currentJd)
                continue;
            const auto month = hijriMonthYearForEvent(
                event.gregorianYear, event.gregorianMonth,
                event.gregorianDay, CrescentEventKind::Evening);
            if (!month)
                continue;
            found[0] = found[0] || event.v >= HIJRI_CALCULATED_START_V;
            found[1] = found[1] || event.v >= HIJRI_OBSERVED_START_V;
        }
    }
    return found;
}

ObservationalHijriResult observationalHijriFromLunationEvents(
    const std::vector<HijriLunationEvents>& inputLunations,
    long long currentSunsetDay, double currentJd,
    double latitudeDeg)
{
    ObservationalHijriResult result;
    result.latitudePolicy = hijriLatitudePolicy(latitudeDeg);
    if (result.latitudePolicy == HijriLatitudePolicy::Unsupported)
    {
        result.availability = HijriAvailabilityReason::LatitudeUnsupported;
        return result;
    }
    if (!std::isfinite(currentJd))
        return result;

    const auto lunations = normalizedHijriLunations(
        inputLunations, latitudeDeg);
    const auto coverage = hijriHistoryAnchorCoverage(
        lunations, currentJd, latitudeDeg);
    if (!coverage[0] || !coverage[1])
        return result;

    const HijriTrackReplay calculated = replayHijriTrack(
        lunations, HIJRI_CALCULATED_START_V,
        currentSunsetDay, currentJd);
    const HijriTrackReplay observed = replayHijriTrack(
        lunations, HIJRI_OBSERVED_START_V,
        currentSunsetDay, currentJd);
    if (!calculated.date || !observed.date)
        return result;

    result.date = {*calculated.date, *observed.date};
    result.availability = HijriAvailabilityReason::Available;
    result.calculatedPrematureStart = calculated.activeMonthPremature;
    result.observedPrematureStart = observed.activeMonthPremature;
    result.calculatedPrematureScheduled = calculated.prematureScheduled;
    result.observedPrematureScheduled = observed.prematureScheduled;
    return result;
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

double arcminutesToArcseconds(double arcminutes)
{
    return std::isfinite(arcminutes)
               ? arcminutes * 60.0
               : std::numeric_limits<double>::quiet_NaN();
}

double signedAngleDifferenceDeg(double firstDeg, double secondDeg)
{
    if (!std::isfinite(firstDeg) || !std::isfinite(secondDeg))
        return std::numeric_limits<double>::quiet_NaN();
    double difference = std::fmod(firstDeg - secondDeg, 360.0);
    if (difference <= -180.0)
        difference += 360.0;
    else if (difference > 180.0)
        difference -= 360.0;
    return difference;
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

std::string formatSignedDuration(double days)
{
    const double durationMinutes = days * 1440.0;
    if (!std::isfinite(durationMinutes)
        || durationMinutes
               > static_cast<double>(std::numeric_limits<long long>::max())
        || durationMinutes
               < static_cast<double>(std::numeric_limits<long long>::min()))
        return {};

    const long long signedMinutes = std::llround(durationMinutes);
    const bool negative = signedMinutes < 0;
    const unsigned long long absoluteMinutes = negative
        ? static_cast<unsigned long long>(-(signedMinutes + 1)) + 1ULL
        : static_cast<unsigned long long>(signedMinutes);
    const unsigned long long hours = absoluteMinutes / 60ULL;
    const unsigned long long minutes = absoluteMinutes % 60ULL;

    std::ostringstream output;
    output << (negative ? '-' : '+') << hours << 'h'
           << std::setfill('0') << std::setw(2) << minutes << 'm';
    return output.str();
}

std::string formatConjunctionAge(double daysFromConjunction)
{
    return formatSignedDuration(daysFromConjunction);
}

std::optional<double> refineWrappedLongitudeRoot(
    const std::function<double(double)>& longitudeDifference,
    double seed, double maximumHalfSpanDays, double toleranceDays)
{
    if (!longitudeDifference || !std::isfinite(seed)
        || !(maximumHalfSpanDays > 0.0) || !(toleranceDays > 0.0))
        return std::nullopt;

    const double centerValue = longitudeDifference(seed);
    if (!std::isfinite(centerValue))
        return std::nullopt;
    if (std::abs(centerValue) < 1e-14)
        return seed;

    constexpr double PI = 3.141592653589793238462643383279502884;
    double halfSpan = std::min(1.0 / 1440.0, maximumHalfSpanDays);
    double lo = seed;
    double hi = seed;
    double flo = centerValue;
    double fhi = centerValue;
    bool bracketed = false;

    while (halfSpan <= maximumHalfSpanDays + 1e-15)
    {
        lo = seed - halfSpan;
        hi = seed + halfSpan;
        flo = longitudeDifference(lo);
        fhi = longitudeDifference(hi);
        if (!std::isfinite(flo) || !std::isfinite(fhi))
            return std::nullopt;
        // A conjunction crosses zero continuously. A difference close to 2pi
        // is the wrapped discontinuity at opposition and is not a root.
        if (std::abs(fhi - flo) < PI && flo * fhi <= 0.0)
        {
            bracketed = true;
            break;
        }
        if (halfSpan >= maximumHalfSpanDays)
            break;
        halfSpan = std::min(maximumHalfSpanDays, halfSpan * 2.0);
    }

    if (!bracketed)
        return std::nullopt;

    while (hi - lo > toleranceDays)
    {
        const double mid = 0.5 * (lo + hi);
        const double fm = longitudeDifference(mid);
        if (!std::isfinite(fm))
            return std::nullopt;
        if (std::abs(fm) < 1e-14)
            return mid;
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
    return 0.5 * (lo + hi);
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

std::vector<std::size_t> visibilityTransitionIndices(
    const std::vector<std::optional<double>>& chronologicalV,
    CrescentEventKind kind, double lowerV, double upperV)
{
    std::vector<std::size_t> finiteIndices;
    if (!std::isfinite(lowerV) || !std::isfinite(upperV)
        || !(lowerV < upperV))
        return finiteIndices;

    const auto appendFinite = [&](std::size_t index)
    {
        const auto& value = chronologicalV[index];
        if (value && std::isfinite(*value))
            finiteIndices.push_back(index);
    };
    for (std::size_t index = 0; index < chronologicalV.size(); ++index)
        appendFinite(index);

    if (finiteIndices.empty())
        return {};

    std::vector<std::size_t> selected;
    if (kind == CrescentEventKind::Evening)
    {
        // Use the first bracketed waxing crossing after conjunction.
        std::size_t lowerCrossing = finiteIndices.size();
        for (std::size_t position = 1; position < finiteIndices.size(); ++position)
        {
            if (*chronologicalV[finiteIndices[position - 1]] < lowerV
                && *chronologicalV[finiteIndices[position]] >= lowerV)
            {
                lowerCrossing = position;
                break;
            }
        }
        if (lowerCrossing == finiteIndices.size())
            return {};

        std::size_t upperCrossing = lowerCrossing;
        while (upperCrossing < finiteIndices.size()
               && *chronologicalV[finiteIndices[upperCrossing]] < upperV)
            ++upperCrossing;
        if (upperCrossing == finiteIndices.size())
            return {};

        selected.push_back(finiteIndices[lowerCrossing - 1]);
        selected.push_back(finiteIndices[lowerCrossing]);
        if (upperCrossing != lowerCrossing)
            selected.push_back(finiteIndices[upperCrossing]);
    }
    else
    {
        // Use the final bracketed waning crossing before conjunction. This
        // avoids mixing an older dip with a later rebound in sparse samples.
        std::size_t upperExit = finiteIndices.size();
        for (std::size_t position = 1; position < finiteIndices.size(); ++position)
        {
            if (*chronologicalV[finiteIndices[position - 1]] >= upperV
                && *chronologicalV[finiteIndices[position]] < upperV)
                upperExit = position;
        }
        if (upperExit == finiteIndices.size())
            return {};

        std::size_t lowerExit = upperExit;
        while (lowerExit < finiteIndices.size()
               && *chronologicalV[finiteIndices[lowerExit]] >= lowerV)
            ++lowerExit;
        if (lowerExit == finiteIndices.size())
            return {};

        selected.push_back(finiteIndices[upperExit - 1]);
        selected.push_back(finiteIndices[upperExit]);
        if (lowerExit != upperExit)
            selected.push_back(finiteIndices[lowerExit]);
    }

    selected.erase(std::unique(selected.begin(), selected.end()),
                   selected.end());
    return selected;
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
