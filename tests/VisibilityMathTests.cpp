#include "VisibilityMath.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
bool close(double a, double b, double eps = 1e-9)
{
    return std::abs(a - b) < eps;
}
}

int main()
{
    using namespace VisibilityMath;

    assert(useArabicForProgramLanguage("ar"));
    assert(useArabicForProgramLanguage("ar_QA"));
    assert(useArabicForProgramLanguage("AR"));
    assert(!useArabicForProgramLanguage("en"));
    assert(!useArabicForProgramLanguage("fa"));
    assert(!useArabicForProgramLanguage("a"));
    assert(!isGregorianCalendarDate(1582, 10, 4));
    assert(isGregorianCalendarDate(1582, 10, 15));
    assert(!isGregorianCalendarDate(1200, 1, 1));
    assert(isGregorianCalendarDate(2026, 8, 22));
    assert(formatConjunctionAge(0.0) == "+0h00m");
    assert(formatConjunctionAge(0.77) == "+18h29m");
    assert(formatConjunctionAge(-0.77) == "-18h29m");
    assert(formatConjunctionAge(1.75) == "+42h00m");
    assert(formatConjunctionAge((5.0 * 60.0 + 7.0) / 1440.0)
           == "+5h07m");
    assert(formatConjunctionAge(59.6 / 1440.0) == "+1h00m");
    assert(formatConjunctionAge(-59.6 / 1440.0) == "-1h00m");
    assert(formatConjunctionAge(29.0 / 86400.0) == "+0h00m");
    assert(formatConjunctionAge(30.0 / 86400.0) == "+0h01m");
    assert(formatConjunctionAge(std::nan("")).empty());
    assert(formatSignedDuration(0.0) == "+0h00m");
    assert(formatSignedDuration(-17.0 / 1440.0) == "-0h17m");
    assert(formatSignedDuration(83.0 / 1440.0) == "+1h23m");

    assert(eventFilterFromString("both") == EventFilter::Both);
    assert(eventFilterFromString("Morning") == EventFilter::Morning);
    assert(eventFilterFromString("EVENING") == EventFilter::Evening);
    assert(eventFilterFromString("invalid") == EventFilter::Both);
    assert(std::string(eventFilterKey(EventFilter::Both)) == "both");
    assert(std::string(eventFilterName(EventFilter::Morning)) == "Morning");
    assert(eventMatchesFilter(CrescentEventKind::Morning, EventFilter::Both));
    assert(eventMatchesFilter(CrescentEventKind::Morning, EventFilter::Morning));
    assert(!eventMatchesFilter(CrescentEventKind::Evening, EventFilter::Morning));
    assert(eventMatchesFilter(CrescentEventKind::Evening, EventFilter::Evening));

    const auto septemberMorning = hijriMonthYearForEvent(
        2026, 9, 8, CrescentEventKind::Morning);
    const auto septemberEvening = hijriMonthYearForEvent(
        2026, 9, 8, CrescentEventKind::Evening);
    assert(septemberMorning && septemberMorning->year == 1448
           && septemberMorning->month == 3);
    assert(septemberEvening && septemberEvening->year == 1448
           && septemberEvening->month == 4);
    const auto rolloverMorning = hijriMonthYearForEvent(
        2026, 6, 7, CrescentEventKind::Morning);
    const auto rolloverEvening = hijriMonthYearForEvent(
        2026, 6, 7, CrescentEventKind::Evening);
    assert(rolloverMorning && rolloverMorning->year == 1447
           && rolloverMorning->month == 12);
    assert(rolloverEvening && rolloverEvening->year == 1448
           && rolloverEvening->month == 1);
    assert(!hijriMonthYearForEvent(2026, 2, 30,
                                   CrescentEventKind::Morning));

    assert(close(HIJRI_CALCULATED_START_V, 1.35));
    assert(close(HIJRI_OBSERVED_START_V, 5.83));
    const HijriDate rabiAwwal30{1448, 3, 30};
    const HijriDate rabiAkhir1{1448, 4, 1};
    assert(validHijriDate(rabiAwwal30));
    assert(!validHijriDate({1448, 13, 1}));
    assert(sameHijriDate(rabiAkhir1, {1448, 4, 1}));
    assert(!sameHijriDate(rabiAkhir1, rabiAwwal30));

    assert(sameHijriDate(
        advanceHijriDateAtSunset({1448, 3, 27}, true),
        HijriDate{1448, 3, 28}));
    assert(sameHijriDate(
        advanceHijriDateAtSunset({1448, 3, 28}, true),
        rabiAkhir1));
    assert(sameHijriDate(
        advanceHijriDateAtSunset({1448, 3, 29}, false),
        rabiAwwal30));
    assert(sameHijriDate(
        advanceHijriDateAtSunset(rabiAwwal30, false),
        rabiAkhir1));
    assert(sameHijriDate(
        advanceHijriDateAtSunset({1447, 12, 30}, false),
        HijriDate{1448, 1, 1}));
    assert(sameHijriDate(
        advanceHijriDateAtSunset({-1, 12, 30}, false),
        HijriDate{1, 1, 1}));
    assert(!validHijriDate(
        advanceHijriDateAtSunset({1448, 0, 1}, false)));

    const ObservationalHijriDate aligned{{1448, 3, 30},
                                         {1448, 3, 30}};
    const auto belowCalculated = advanceObservationalHijriAtSunset(
        aligned, HIJRI_CALCULATED_START_V - 1e-9);
    assert(sameHijriDate(belowCalculated.calculated, rabiAkhir1));
    assert(sameHijriDate(belowCalculated.observed, rabiAkhir1));

    const ObservationalHijriDate lowerBoundaryState{{1448, 3, 29},
                                                     {1448, 3, 29}};
    const auto lowerBoundaryBelow = advanceObservationalHijriAtSunset(
        lowerBoundaryState, HIJRI_CALCULATED_START_V - 1e-9);
    assert(sameHijriDate(lowerBoundaryBelow.calculated, rabiAwwal30));
    assert(sameHijriDate(lowerBoundaryBelow.observed, rabiAwwal30));

    const auto calculatedOnly = advanceObservationalHijriAtSunset(
        lowerBoundaryState, HIJRI_CALCULATED_START_V);
    assert(sameHijriDate(calculatedOnly.calculated, rabiAkhir1));
    assert(sameHijriDate(calculatedOnly.observed, rabiAwwal30));
    assert(formatObservationalHijriDate(calculatedOnly)
           == "01/04/1448 - 30/03/1448");

    const auto calculatedOnlyUpperEdge = advanceObservationalHijriAtSunset(
        lowerBoundaryState, HIJRI_OBSERVED_START_V - 1e-9);
    assert(sameHijriDate(calculatedOnlyUpperEdge.calculated, rabiAkhir1));
    assert(sameHijriDate(calculatedOnlyUpperEdge.observed, rabiAwwal30));

    const auto bothCriteria = advanceObservationalHijriAtSunset(
        lowerBoundaryState, HIJRI_OBSERVED_START_V);
    assert(sameHijriDate(bothCriteria.calculated, rabiAkhir1));
    assert(sameHijriDate(bothCriteria.observed, rabiAkhir1));
    assert(formatObservationalHijriDate(bothCriteria) == "01/04/1448");

    const auto aboveObserved = advanceObservationalHijriAtSunset(
        lowerBoundaryState, HIJRI_OBSERVED_START_V + 1e-9);
    assert(sameHijriDate(aboveObserved.calculated, rabiAkhir1));
    assert(sameHijriDate(aboveObserved.observed, rabiAkhir1));

    const auto advancedRange = advanceObservationalHijriAtSunset(
        calculatedOnly, std::nullopt);
    assert(sameHijriDate(advancedRange.calculated,
                         HijriDate{1448, 4, 2}));
    assert(sameHijriDate(advancedRange.observed, rabiAkhir1));
    assert(formatObservationalHijriDate(advancedRange)
           == "02/04/1448 - 01/04/1448");
    assert(formatHijriDate({1448, 4, 1}) == "01/04/1448");
    assert(formatHijriDate({-54, 1, 1}) == "01/01/-0054");
    assert(formatHijriDate({1448, 0, 1}).empty());
    assert(!sunsetHasOccurred(100.0 - 1e-9, 100.0));
    assert(sunsetHasOccurred(100.0, 100.0));
    assert(sunsetHasOccurred(100.0 + 1e-9, 100.0));
    assert(!sunsetHasOccurred(std::nan(""), 100.0));

    assert(!hijriMonthStartEligible(27));
    assert(hijriMonthStartEligible(28));
    assert(hijriMonthStartEligible(29));
    assert(hijriMonthStartEligible(30));
    assert(!hijriMonthStartEligible(31));
    assert(!hijriForcedRolloverDue(100, 129));
    assert(hijriForcedRolloverDue(100, 130));
    assert(close(*selectPrecedingConjunction(
                     100.0, 99.0, std::nullopt), 99.0));
    assert(close(*selectPrecedingConjunction(
                     100.0, 101.0, 99.0), 99.0));
    assert(!selectPrecedingConjunction(
        100.0, 101.0, std::nullopt));
    assert(lunationCacheCoversJde(100.0, 100.0, 129.5));
    assert(lunationCacheCoversJde(129.499999, 100.0, 129.5));
    assert(!lunationCacheCoversJde(99.999999, 100.0, 129.5));
    assert(!lunationCacheCoversJde(129.5, 100.0, 129.5));
    assert(!lunationCacheCoversJde(
        110.0, 100.0, std::nan("")));

    const auto hijriEvent = [](
        long long localDay, int year, int month, int day,
        double conjunctionJde, double sunsetJd, double bestTimeJde,
        int dayIndex, double v)
    {
        return HijriVisibilityEvent{
            localDay, year, month, day, conjunctionJde,
            sunsetJd, bestTimeJde, bestTimeJde, dayIndex, v};
    };

    const std::vector<HijriVisibilityEvent> dohaHistory = {
        hijriEvent(100, 2026, 7, 15, 99.5, 100.70, 100.75, 1, 6.00),
        hijriEvent(101, 2026, 7, 16, 99.5, 101.70, 101.75, 2, 10.00),
        hijriEvent(129, 2026, 8, 13, 128.5, 129.70, 129.75, 1, 2.05),
        hijriEvent(130, 2026, 8, 14, 128.5, 130.70, 130.75, 2, 15.92),
        hijriEvent(131, 2026, 8, 15, 128.5, 131.70, 131.75, 3, 31.57)
    };
    const auto dohaAugust13 = observationalHijriFromLunationEvents(
        dohaHistory, 129, 129.71, 94.71);
    const auto dohaAugust14 = observationalHijriFromLunationEvents(
        dohaHistory, 130, 130.71, 95.71);
    const auto dohaAugust15 = observationalHijriFromLunationEvents(
        dohaHistory, 131, 131.71, 96.71);
    assert(dohaAugust13 && formatObservationalHijriDate(*dohaAugust13)
                              == "01/03/1448 - 30/02/1448");
    assert(dohaAugust14 && formatObservationalHijriDate(*dohaAugust14)
                              == "02/03/1448 - 01/03/1448");
    assert(dohaAugust15 && formatObservationalHijriDate(*dohaAugust15)
                              == "03/03/1448 - 02/03/1448");
    assert(!observationalHijriFromLunationEvents(
        {hijriEvent(129, 2026, 8, 13, 128.5,
                    129.70, 129.75, 1, 2.05)},
        129, 129.71, 94.71));

    // The lower track may start in bin 0 while the stricter track continues
    // to bin +1. Both results become active at their event's sunset.
    const std::vector<HijriVisibilityEvent> independentCrossings = {
        hijriEvent(100, 2026, 7, 15, 99.5, 100.70, 100.75, 1, 6.00),
        hijriEvent(128, 2026, 8, 12, 128.4, 128.70, 128.75, 0, 2.00),
        hijriEvent(129, 2026, 8, 13, 128.4, 129.70, 129.75, 1, 6.00)
    };
    const auto independent = observationalHijriFromLunationEvents(
        independentCrossings, 129, 129.71, 94.71);
    assert(independent && independent->calculated.day == 2
           && independent->observed.day == 1);

    // A rounded bin-0 event before conjunction is not a waxing-crescent start.
    assert(!observationalHijriFromLunationEvents(
        {hijriEvent(100, 2026, 7, 15, 100.0,
                    100.70, 99.90, 0, 20.0)},
        100, 100.71, 65.71));

    // A qualifying day-27 event is skipped; day 28 starts the month, and a
    // later qualifying event in the same lunation does not restart it.
    const std::vector<HijriVisibilityEvent> earlyCrossing = {
        hijriEvent(100, 2026, 7, 15, 99.5, 100.70, 100.75, 1, 6.00),
        hijriEvent(126, 2026, 8, 10, 126.4, 126.70, 126.75, 0, 6.00),
        hijriEvent(127, 2026, 8, 11, 126.4, 127.70, 127.75, 1, 6.00),
        hijriEvent(128, 2026, 8, 12, 126.4, 128.70, 128.75, 2, 6.00)
    };
    const auto afterLaterCrossing = observationalHijriFromLunationEvents(
        earlyCrossing, 128, 128.71, 93.71);
    assert(afterLaterCrossing
           && afterLaterCrossing->calculated.day == 2
           && afterLaterCrossing->observed.day == 2);

    const auto beforeEligibleSunset = observationalHijriFromLunationEvents(
        earlyCrossing, 126, 127.69, 92.69);
    const auto afterEligibleSunset = observationalHijriFromLunationEvents(
        earlyCrossing, 127, 127.71, 92.71);
    assert(beforeEligibleSunset
           && beforeEligibleSunset->calculated.day == 27);
    assert(afterEligibleSunset
           && afterEligibleSunset->calculated.day == 1);

    // With no accepted crossing, the next sunset after day 30 forces day 1.
    const auto forced = observationalHijriFromLunationEvents(
        {hijriEvent(100, 2026, 7, 15, 99.5,
                    100.70, 100.75, 1, 6.00)},
        130, 130.71, 95.71);
    assert(forced && forced->calculated.day == 1
           && forced->observed.day == 1);

    // A synchronization event outside the preceding 35 days is unavailable.
    assert(!observationalHijriFromLunationEvents(
        {hijriEvent(100, 2026, 7, 15, 99.5,
                    100.70, 100.75, 1, 6.00)},
        136, 136.71, 101.71));

    assert(close(CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG, -0.8333));
    assert(close(theoreticalWidth(0.0, 0.0), 0.0));
    assert(close(illuminatedWidth(0.1, 0.5), 3.0));
    assert(close(arcminutesToArcseconds(3.0), 180.0));
    assert(std::isnan(arcminutesToArcseconds(std::nan(""))));
    assert(close(signedAngleDifferenceDeg(1.0, 359.0), 2.0));
    assert(close(signedAngleDifferenceDeg(359.0, 1.0), -2.0));
    assert(close(signedAngleDifferenceDeg(180.0, 0.0), 180.0));
    assert(close(signedAngleDifferenceDeg(0.0, 180.0), 180.0));
    assert(std::isnan(signedAngleDifferenceDeg(std::nan(""), 0.0)));

    const auto wrappedDifference = [](double value)
    {
        constexpr double root = 100.125;
        constexpr double pi = 3.141592653589793238462643383279502884;
        double difference = value - root;
        while (difference <= -pi) difference += 2.0 * pi;
        while (difference > pi) difference -= 2.0 * pi;
        return difference;
    };
    const auto refinedRoot = refineWrappedLongitudeRoot(
        wrappedDifference, 100.0);
    assert(refinedRoot && close(*refinedRoot, 100.125, 1e-6));
    assert(!refineWrappedLongitudeRoot(
        [](double) { return 1.0; }, 100.0));
    assert(!refineWrappedLongitudeRoot(
        [](double) { return std::nan(""); }, 100.0));
    assert(std::isnan(horizontalParallaxDeg(0.0, 1.0)));

    const auto y = yallopBoundaries(1.0);
    assert(close(y[0], 1.352));
    assert(close(y[1], 2.072));
    assert(close(y[2], 3.532));
    assert(close(y[3], 5.832));
    assert(y[0] < y[1] && y[1] < y[2] && y[2] < y[3]);
    assert(categoryIndex(y[0] - 1e-6, y.data(), y.size()) == -1);
    assert(categoryIndex(y[0], y.data(), y.size()) == 0);
    assert(categoryIndex(y[1] - 1e-6, y.data(), y.size()) == 0);
    assert(categoryIndex(y[1], y.data(), y.size()) == 1);
    assert(categoryIndex(y[3], y.data(), y.size()) == 3);

    const std::array<double, 3> odeh = {{-0.96, 2.00, 5.65}};
    assert(categoryIndex(-0.960001, odeh.data(), odeh.size()) == -1);
    assert(categoryIndex(-0.96, odeh.data(), odeh.size()) == 0);
    assert(categoryIndex(1.999999, odeh.data(), odeh.size()) == 0);
    assert(categoryIndex(2.00, odeh.data(), odeh.size()) == 1);
    assert(categoryIndex(5.649999, odeh.data(), odeh.size()) == 1);
    assert(categoryIndex(5.65, odeh.data(), odeh.size()) == 2);

    const auto fallback = fallbackYallopBoundaries();
    assert(close(fallback[0], 1.352));
    assert(close(fallback[1], 2.072));
    assert(close(fallback[2], 3.532));
    assert(close(fallback[3], 5.832));

    assert(formatLocalTime(2451544.5, 0.0) == "0h00m00s");
    assert(formatLocalTime(2451544.5 + (5.0 * 3600.0 + 6.0 * 60.0 + 7.0) / 86400.0, 0.0)
           == "5h06m07s");
    assert(formatLocalTime(2451544.5, 3.0) == "3h00m00s");
    assert(formatLocalTime(2451544.5 + 86399.6 / 86400.0, 0.0)
           == "23h59m59s");
    assert(formatLocalTime(2451544.5 + 86400.0 / 86400.0, 0.0)
           == "0h00m00s");
    assert(formatLocalTime(2451544.5, -1.0) == "23h00m00s");

    const auto evening = eveningBestTime(100.0, 100.09);
    const auto morning = morningBestTime(100.0, 99.91);
    assert(evening && close(*evening, 100.04));
    assert(morning && close(*morning, 99.96));
    assert(!eveningBestTime(100.0, 99.9));
    assert(!morningBestTime(100.0, 100.1));
    assert(close(*nearestTime(100.1, evening, morning), *evening));

    assert(conjunctionDayIndex(100.49, 100.0) == 0);
    assert(conjunctionDayIndex(100.50, 100.0) == 1);
    assert(eventInConjunctionWindow(97.0, 100.0));
    assert(eventInConjunctionWindow(103.0, 100.0));
    assert(eventInConjunctionWindow(96.500001, 100.0));
    assert(eventInConjunctionWindow(103.499999, 100.0));
    assert(!eventInConjunctionWindow(96.5, 100.0));
    assert(!eventInConjunctionWindow(103.5, 100.0));
    assert(!eventInConjunctionWindow(96.0, 100.0));
    assert(!eventInConjunctionWindow(104.0, 100.0));
    assert(!moonIsUp(-0.000001));
    assert(!moonIsUp(0.0));
    assert(moonIsUp(0.000001));
    assert(moonInformationAvailable(0.01, 97.0, 100.0));
    assert(moonInformationAvailable(0.01, 103.0, 100.0));
    assert(!moonInformationAvailable(0.0, 100.0, 100.0));
    assert(!moonInformationAvailable(-0.01, 100.0, 100.0));
    assert(!moonInformationAvailable(0.01, 96.5, 100.0));
    assert(!moonInformationAvailable(0.01, 103.5, 100.0));
    assert(!moonInformationAvailable(0.01, 100.0, std::nan("")));
    assert(validCrescentEvent(97.0, 97.0, 100.0, 0.01));
    assert(validCrescentEvent(103.0, 103.0, 100.0, 0.01));
    assert(!validCrescentEvent(96.0, 96.0, 100.0, 0.01));
    assert(!validCrescentEvent(104.0, 104.0, 100.0, 0.01));
    assert(!validCrescentEvent(99.0, 99.0, 100.0, 0.0));
    assert(!validCrescentEvent(99.0, 99.0, 100.0, -0.01));

    const std::optional<double> bestTime = 100.25;
    const auto upOnlyBest = chooseNavigationTime(
        NavigationMode::MoonUpOnly, CrescentEventKind::Morning,
        100.30, bestTime, 0.01);
    assert(upOnlyBest && close(upOnlyBest->jd, 100.25));
    assert(upOnlyBest->basis == EventTimeBasis::BestTime);
    assert(!chooseNavigationTime(
        NavigationMode::MoonUpOnly, CrescentEventKind::Morning,
        100.30, bestTime, 0.0));
    const auto allBest = chooseNavigationTime(
        NavigationMode::MoonUpOrDown, CrescentEventKind::Evening,
        100.70, bestTime, 0.01);
    assert(allBest && close(allBest->jd, 100.25));
    assert(allBest->basis == EventTimeBasis::BestTime);
    const auto sunriseFallback = chooseNavigationTime(
        NavigationMode::MoonUpOrDown, CrescentEventKind::Morning,
        100.30, bestTime, 0.0);
    assert(sunriseFallback && close(sunriseFallback->jd, 100.30));
    assert(sunriseFallback->basis == EventTimeBasis::Sunrise);
    const auto sunsetFallback = chooseNavigationTime(
        NavigationMode::MoonUpOrDown, CrescentEventKind::Evening,
        100.70, std::nullopt, -1.0);
    assert(sunsetFallback
           && close(sunsetFallback->jd,
                    100.70 + POST_SUNSET_NAVIGATION_MARGIN_DAYS));
    assert(sunsetFallback->jd > 100.70);
    assert(sunsetHasOccurred(sunsetFallback->jd, 100.70));
    assert(sunsetFallback->basis == EventTimeBasis::Sunset);
    assert(!chooseNavigationTime(
        NavigationMode::MoonUpOrDown, CrescentEventKind::Evening,
        std::nan(""), std::nullopt, -1.0));

    std::vector<CrescentEvent> events = {
        {101.75, 100.0, 2, CrescentEventKind::Evening},
        {100.25, 100.0, 0, CrescentEventKind::Morning},
        {100.75, 100.0, 1, CrescentEventKind::Evening},
        {101.25, 100.0, 1, CrescentEventKind::Morning}
    };
    sortCrescentEvents(events);
    assert(events[0].kind == CrescentEventKind::Morning);
    assert(events[1].kind == CrescentEventKind::Evening);
    assert(events[2].kind == CrescentEventKind::Morning);
    assert(events[3].kind == CrescentEventKind::Evening);

    const auto first = adjacentCrescentEvent(events, 100.0, 1);
    const auto afterExact = adjacentCrescentEvent(events, 100.25, 1);
    const auto beforeExact = adjacentCrescentEvent(events, 100.75, -1);
    const auto last = adjacentCrescentEvent(events, 102.0, -1);
    assert(first && close(first->jd, 100.25));
    assert(afterExact && close(afterExact->jd, 100.75));
    assert(beforeExact && close(beforeExact->jd, 100.25));
    assert(last && close(last->jd, 101.75));
    assert(!adjacentCrescentEvent(events, 102.0, 1));
    assert(!adjacentCrescentEvent(events, 100.0, -1));

    const auto firstMorning = adjacentCrescentEvent(
        events, 100.0, 1, EventFilter::Morning);
    const auto afterExactMorning = adjacentCrescentEvent(
        events, 100.25, 1, EventFilter::Morning);
    const auto beforeExactMorning = adjacentCrescentEvent(
        events, 101.25, -1, EventFilter::Morning);
    const auto firstEvening = adjacentCrescentEvent(
        events, 100.0, 1, EventFilter::Evening);
    const auto afterExactEvening = adjacentCrescentEvent(
        events, 100.75, 1, EventFilter::Evening);
    const auto beforeExactEvening = adjacentCrescentEvent(
        events, 101.75, -1, EventFilter::Evening);
    assert(firstMorning && close(firstMorning->jd, 100.25));
    assert(afterExactMorning && close(afterExactMorning->jd, 101.25));
    assert(beforeExactMorning && close(beforeExactMorning->jd, 100.25));
    assert(firstEvening && close(firstEvening->jd, 100.75));
    assert(afterExactEvening && close(afterExactEvening->jd, 101.75));
    assert(beforeExactEvening && close(beforeExactEvening->jd, 100.75));
    assert(!adjacentCrescentEvent(events, 101.25, 1,
                                  EventFilter::Morning));
    assert(!adjacentCrescentEvent(events, 100.75, -1,
                                  EventFilter::Evening));
    const auto morningInsideForwardEpsilon = adjacentCrescentEvent(
        events, 100.25 - 0.5 / 86400.0, 1, EventFilter::Morning);
    const auto morningOutsideForwardEpsilon = adjacentCrescentEvent(
        events, 100.25 - 2.0 / 86400.0, 1, EventFilter::Morning);
    const auto eveningInsideBackwardEpsilon = adjacentCrescentEvent(
        events, 100.75 + 0.5 / 86400.0, -1, EventFilter::Evening);
    const auto eveningOutsideBackwardEpsilon = adjacentCrescentEvent(
        events, 100.75 + 2.0 / 86400.0, -1, EventFilter::Evening);
    assert(morningInsideForwardEpsilon
           && close(morningInsideForwardEpsilon->jd, 101.25));
    assert(morningOutsideForwardEpsilon
           && close(morningOutsideForwardEpsilon->jd, 100.25));
    assert(!eveningInsideBackwardEpsilon);
    assert(eveningOutsideBackwardEpsilon
           && close(eveningOutsideBackwardEpsilon->jd, 100.75));

    std::vector<CrescentEvent> lunations = {
        {103.0, 100.0, 3, CrescentEventKind::Evening},
        {126.5, 129.5, -3, CrescentEventKind::Morning}
    };
    sortCrescentEvents(lunations);
    const auto nextLunation = adjacentCrescentEvent(lunations, 103.0, 1);
    const auto previousLunation = adjacentCrescentEvent(lunations, 126.5, -1);
    assert(nextLunation && close(nextLunation->jd, 126.5));
    assert(previousLunation && close(previousLunation->jd, 103.0));
    const auto nextMorningLunation = adjacentCrescentEvent(
        lunations, 103.0, 1, EventFilter::Morning);
    const auto previousEveningLunation = adjacentCrescentEvent(
        lunations, 126.5, -1, EventFilter::Evening);
    assert(nextMorningLunation && close(nextMorningLunation->jd, 126.5));
    assert(previousEveningLunation && close(previousEveningLunation->jd, 103.0));

    std::cout << "VisibilityMathTests passed\n";
    return 0;
}
