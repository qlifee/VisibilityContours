#include "VisibilityMath.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
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

    const ObservationalHijriDate formattedRange{{1448, 4, 2},
                                                 {1448, 4, 1}};
    assert(formatObservationalHijriDate(formattedRange)
           == "02/04/1448 - 01/04/1448");
    assert(formatHijriDate({1448, 4, 1}) == "01/04/1448");
    assert(formatHijriDate({-54, 1, 1}) == "01/01/-0054");
    assert(formatHijriDate({1448, 0, 1}).empty());
    assert(!sunsetHasOccurred(100.0 - 1e-9, 100.0));
    assert(sunsetHasOccurred(100.0, 100.0));
    assert(sunsetHasOccurred(100.0 + 1e-9, 100.0));
    assert(!sunsetHasOccurred(std::nan(""), 100.0));

    assert(!hijriMonthStartEligible(28));
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

    const double justBelow45 = std::nextafter(45.0, 44.0);
    const double justAbove45 = std::nextafter(45.0, 46.0);
    const double justBelow59 = std::nextafter(59.0, 58.0);
    const double justAbove59 = std::nextafter(59.0, 60.0);
    const double justBelow60 = std::nextafter(60.0, 59.0);
    const double justAbove60 = std::nextafter(60.0, 61.0);
    assert(hijriLatitudePolicy(55.0) == HijriLatitudePolicy::Standard);
    assert(hijriLatitudePolicy(-55.0) == HijriLatitudePolicy::Standard);
    assert(hijriLatitudePolicy(std::nextafter(55.0, 56.0))
           == HijriLatitudePolicy::FollowLowerLatitude);
    assert(hijriLatitudePolicy(-60.0)
           == HijriLatitudePolicy::FollowLowerLatitude);
    assert(hijriLatitudePolicy(justAbove60)
           == HijriLatitudePolicy::Unsupported);
    assert(hijriLatitudePolicy(std::nan(""))
           == HijriLatitudePolicy::Unsupported);
    assert(hijriMaximumConjunctionBin(justBelow45) == 3);
    assert(hijriMaximumConjunctionBin(-justBelow45) == 3);
    assert(hijriMaximumConjunctionBin(45.0) == 3);
    assert(hijriMaximumConjunctionBin(-45.0) == 3);
    assert(hijriMaximumConjunctionBin(justAbove45) == 4);
    assert(hijriMaximumConjunctionBin(-justAbove45) == 4);
    assert(hijriMaximumConjunctionBin(justBelow59) == 4);
    assert(hijriMaximumConjunctionBin(-justBelow59) == 4);
    assert(hijriMaximumConjunctionBin(59.0) == 5);
    assert(hijriMaximumConjunctionBin(-59.0) == 5);
    assert(hijriMaximumConjunctionBin(justAbove59) == 5);
    assert(hijriMaximumConjunctionBin(-justAbove59) == 5);
    assert(hijriMaximumConjunctionBin(justBelow60) == 5);
    assert(hijriMaximumConjunctionBin(-justBelow60) == 5);
    assert(hijriMaximumConjunctionBin(60.0) == 5);
    assert(hijriMaximumConjunctionBin(-60.0) == 5);
    assert(!hijriMaximumConjunctionBin(justAbove60));
    assert(!hijriMaximumConjunctionBin(-justAbove60));
    assert(!hijriMaximumConjunctionBin(std::nan("")));
    assert(!hijriEventInLatitudeWindow(4, 45.0));
    assert(hijriEventInLatitudeWindow(4, justAbove45));
    assert(!hijriEventInLatitudeWindow(5, justBelow59));
    assert(hijriEventInLatitudeWindow(5, 59.0));
    assert(!hijriEventInLatitudeWindow(6, 60.0));

    const auto hijriEvent = [](
        long long localDay, int year, int month, int day,
        double conjunctionJde, double sunsetJd, double bestTimeJde,
        int dayIndex, double v)
    {
        return HijriVisibilityEvent{
            localDay, year, month, day, conjunctionJde,
            sunsetJd, bestTimeJde, bestTimeJde, dayIndex, v};
    };

    const std::vector<HijriLunationEvents> dohaHistory = {
        {99.5, {
            hijriEvent(100, 2026, 7, 15, 99.5, 100.70, 100.75, 1, 6.00),
            hijriEvent(101, 2026, 7, 16, 99.5, 101.70, 101.75, 2, 10.00)}},
        {128.5, {
            hijriEvent(129, 2026, 8, 13, 128.5, 129.70, 129.75, 1, 2.05),
            hijriEvent(130, 2026, 8, 14, 128.5, 130.70, 130.75, 2, 15.92),
            hijriEvent(131, 2026, 8, 15, 128.5, 131.70, 131.75, 3, 31.57)}}
    };
    const auto dohaAugust13 = observationalHijriFromLunationEvents(
        dohaHistory, 129, 129.71, 25.0);
    const auto dohaAugust14 = observationalHijriFromLunationEvents(
        dohaHistory, 130, 130.71, 25.0);
    const auto dohaAugust15 = observationalHijriFromLunationEvents(
        dohaHistory, 131, 131.71, 25.0);
    assert(observationalHijriAvailable(dohaAugust13));
    assert(formatObservationalHijriDate(dohaAugust13.date)
           == "01/03/1448 - 30/02/1448");
    assert(observationalHijriAvailable(dohaAugust14));
    assert(formatObservationalHijriDate(dohaAugust14.date)
           == "02/03/1448 - 01/03/1448");
    assert(observationalHijriAvailable(dohaAugust15));
    assert(formatObservationalHijriDate(dohaAugust15.date)
           == "03/03/1448 - 02/03/1448");
    auto reversedDohaHistory = dohaHistory;
    std::reverse(reversedDohaHistory.begin(), reversedDohaHistory.end());
    const auto reversedDoha = observationalHijriFromLunationEvents(
        reversedDohaHistory, 131, 131.71, 25.0);
    assert(observationalHijriAvailable(reversedDoha));
    assert(formatObservationalHijriDate(reversedDoha.date)
           == "03/03/1448 - 02/03/1448");
    const auto dohaAnchorCoverage = hijriHistoryAnchorCoverage(
        dohaHistory, 129.71, 25.0);
    assert(dohaAnchorCoverage[0] && dohaAnchorCoverage[1]);
    const auto missingObservedAnchor = observationalHijriFromLunationEvents(
        {{128.5, {hijriEvent(129, 2026, 8, 13, 128.5,
                            129.70, 129.75, 1, 2.05)}}},
        129, 129.71, 25.0);
    assert(missingObservedAnchor.availability
           == HijriAvailabilityReason::HistoryUnavailable);

    // The lower track may start in bin 0 while the stricter track continues
    // to bin +1. Both results become active at their event's sunset.
    const std::vector<HijriLunationEvents> independentCrossings = {
        {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                          100.70, 100.75, 1, 6.00)}},
        {128.4, {
            hijriEvent(128, 2026, 8, 12, 128.4, 128.70, 128.75, 0, 2.00),
            hijriEvent(129, 2026, 8, 13, 128.4, 129.70, 129.75, 1, 6.00)}}
    };
    const auto independent = observationalHijriFromLunationEvents(
        independentCrossings, 129, 129.71, 25.0);
    assert(observationalHijriAvailable(independent));
    assert(independent.date.calculated.day == 1);
    assert(independent.date.observed.day == 1);
    assert(independent.calculatedPrematureStart);
    assert(!independent.observedPrematureStart);

    // A rounded bin-0 event before conjunction is not a waxing-crescent start.
    const auto preConjunction = observationalHijriFromLunationEvents(
        {{100.0, {hijriEvent(100, 2026, 7, 15, 100.0,
                            100.70, 99.90, 0, 20.0)}}},
        100, 100.71, 25.0);
    assert(preConjunction.availability
           == HijriAvailabilityReason::HistoryUnavailable);

    // The first premature crossing consumes its lunation and schedules the
    // compliant start after 29 complete days. Later crossings do not restart it.
    const std::vector<HijriLunationEvents> earlyCrossing = {
        {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                          100.70, 100.75, 1, 6.00)}},
        {126.4, {
            hijriEvent(126, 2026, 8, 10, 126.4, 126.70, 126.75, 0, 6.00),
            hijriEvent(127, 2026, 8, 11, 126.4, 127.70, 127.75, 1, 6.00),
            hijriEvent(128, 2026, 8, 12, 126.4, 128.70, 128.75, 2, 6.00)}}
    };
    const auto beforeScheduledSunset = observationalHijriFromLunationEvents(
        earlyCrossing, 128, 128.71, 25.0);
    const auto atScheduledSunset = observationalHijriFromLunationEvents(
        earlyCrossing, 129, 129.71, 25.0);
    const auto afterScheduledSunset = observationalHijriFromLunationEvents(
        earlyCrossing, 130, 130.71, 25.0);
    assert(observationalHijriAvailable(beforeScheduledSunset));
    assert(beforeScheduledSunset.date.calculated.day == 29);
    assert(!beforeScheduledSunset.calculatedPrematureStart);
    assert(beforeScheduledSunset.calculatedPrematureScheduled);
    assert(observationalHijriAvailable(atScheduledSunset));
    assert(atScheduledSunset.date.calculated.day == 1);
    assert(atScheduledSunset.calculatedPrematureStart);
    assert(afterScheduledSunset.date.calculated.day == 2);
    assert(afterScheduledSunset.calculatedPrematureStart);

    // Every crossing after 1...28 completed days is deferred to the sunset
    // completing 29 days. Crossings after 29 or 30 completed days start the
    // next month immediately and are not marked premature.
    for (int completedDays = 1; completedDays <= 28; ++completedDays)
    {
        const long long crossingDay = 100 + completedDays;
        const double conjunction = crossingDay - 0.5;
        const std::vector<HijriLunationEvents> history = {
            {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                              100.70, 100.75, 1, 6.0)}},
            {conjunction,
             {hijriEvent(crossingDay, 2026, 8, 1,
                         conjunction, crossingDay + 0.70,
                         crossingDay + 0.75, 1, 6.0)}}};
        const auto crossing = observationalHijriFromLunationEvents(
            history, crossingDay, crossingDay + 0.71, 25.0);
        assert(observationalHijriAvailable(crossing));
        assert(crossing.date.calculated.day == completedDays + 1);
        assert(crossing.calculatedPrematureScheduled);
        assert(!crossing.calculatedPrematureStart);

        const auto scheduled = observationalHijriFromLunationEvents(
            history, 129, 129.71, 25.0);
        assert(observationalHijriAvailable(scheduled));
        assert(scheduled.date.calculated.day == 1);
        assert(scheduled.calculatedPrematureStart);
        assert(!scheduled.calculatedPrematureScheduled);
    }
    for (int completedDays : {29, 30})
    {
        const long long crossingDay = 100 + completedDays;
        const double conjunction = crossingDay - 0.5;
        const auto eligible = observationalHijriFromLunationEvents(
            {{99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                                100.70, 100.75, 1, 6.0)}},
             {conjunction,
              {hijriEvent(crossingDay, 2026, 8, 1,
                          conjunction, crossingDay + 0.70,
                          crossingDay + 0.75, 1, 6.0)}}},
            crossingDay, crossingDay + 0.71, 25.0);
        assert(observationalHijriAvailable(eligible));
        assert(eligible.date.calculated.day == 1);
        assert(!eligible.calculatedPrematureStart);
        assert(!eligible.calculatedPrematureScheduled);
    }

    auto warningClearedHistory = earlyCrossing;
    warningClearedHistory.push_back(
        {157.4, {hijriEvent(158, 2026, 9, 9, 157.4,
                           158.70, 158.75, 1, 6.00)}});
    const auto warningCleared = observationalHijriFromLunationEvents(
        warningClearedHistory, 158, 158.71, 25.0);
    assert(observationalHijriAvailable(warningCleared));
    assert(warningCleared.date.calculated.day == 1);
    assert(!warningCleared.calculatedPrematureStart);

    const std::vector<HijriLunationEvents> exactThresholds = {
        {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                          100.70, 100.75, 1,
                          HIJRI_OBSERVED_START_V)}},
        {128.5, {hijriEvent(129, 2026, 8, 13, 128.5,
                           129.70, 129.75, 1,
                           HIJRI_CALCULATED_START_V)}}};
    const auto exactLower = observationalHijriFromLunationEvents(
        exactThresholds, 129, 129.71, 25.0);
    assert(observationalHijriAvailable(exactLower));
    assert(exactLower.date.calculated.day == 1);
    assert(exactLower.date.observed.day == 30);
    auto belowThresholds = exactThresholds;
    belowThresholds[1].events[0].v =
        std::nextafter(HIJRI_CALCULATED_START_V, 0.0);
    const auto belowLower = observationalHijriFromLunationEvents(
        belowThresholds, 129, 129.71, 25.0);
    assert(observationalHijriAvailable(belowLower));
    assert(belowLower.date.calculated.day == 30);
    assert(belowLower.date.observed.day == 30);

    // With no accepted crossing, the next sunset after day 30 forces day 1.
    const auto forced = observationalHijriFromLunationEvents(
        {{99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                           100.70, 100.75, 1, 6.00)}},
         {128.5, {}}},
        130, 130.71, 25.0);
    assert(observationalHijriAvailable(forced));
    assert(forced.date.calculated.day == 1
           && forced.date.observed.day == 1);
    assert(!forced.calculatedPrematureStart);

    // A forced start consumes the lunation; its later crossing cannot restart it.
    const auto forcedConsumesLunation = observationalHijriFromLunationEvents(
        {{99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                           100.70, 100.75, 1, 6.00)}},
         {128.5, {hijriEvent(131, 2026, 8, 15, 128.5,
                            131.70, 131.75, 3, 20.0)}}},
        131, 131.71, 25.0);
    assert(observationalHijriAvailable(forcedConsumesLunation));
    assert(forcedConsumesLunation.date.calculated.day == 2);

    // An active-lunation crossing is never allowed to synchronize itself.
    // Replay must continue back through an empty preceding lunation to the
    // older anchor, producing the same result whether opened before or after
    // the active crossing.
    const std::vector<HijriLunationEvents> olderAnchorHistory = {
        {40.5, {hijriEvent(41, 2026, 5, 17, 40.5,
                          41.70, 41.75, 1, 6.0)}},
        {69.5, {}},
        {98.5, {hijriEvent(99, 2026, 7, 14, 98.5,
                          99.70, 99.75, 1, 6.0)}}};
    const auto activeOnlyCoverage = hijriHistoryAnchorCoverage(
        {{69.5, {}}, olderAnchorHistory.back()}, 99.71, 25.0);
    assert(!activeOnlyCoverage[0] && !activeOnlyCoverage[1]);
    const auto olderCoverage = hijriHistoryAnchorCoverage(
        olderAnchorHistory, 99.71, 25.0);
    assert(olderCoverage[0] && olderCoverage[1]);
    const auto beforeDeferredStart = observationalHijriFromLunationEvents(
        olderAnchorHistory, 99, 99.71, 25.0);
    assert(observationalHijriAvailable(beforeDeferredStart));
    assert(beforeDeferredStart.date.calculated.day == 29);
    assert(beforeDeferredStart.calculatedPrematureScheduled);
    assert(!beforeDeferredStart.calculatedPrematureStart);
    const auto afterDeferredStart = observationalHijriFromLunationEvents(
        olderAnchorHistory, 100, 100.71, 25.0);
    assert(observationalHijriAvailable(afterDeferredStart));
    assert(afterDeferredStart.date.calculated.day == 1);
    assert(afterDeferredStart.calculatedPrematureStart);

    // Exact adaptive-bin behavior remains separate from the normal -3...+3 gate.
    const std::vector<HijriLunationEvents> binFourAnchor = {
        {99.5, {hijriEvent(103, 2026, 7, 18, 99.5,
                          103.70, 103.75, 4, 6.0)}},
        {128.5, {}}};
    assert(!observationalHijriAvailable(
        observationalHijriFromLunationEvents(
            binFourAnchor, 130, 130.71, 45.0)));
    assert(observationalHijriAvailable(
        observationalHijriFromLunationEvents(
            binFourAnchor, 130, 130.71, justAbove45)));
    const std::vector<HijriLunationEvents> binFiveAnchor = {
        {99.5, {hijriEvent(104, 2026, 7, 19, 99.5,
                          104.70, 104.75, 5, 6.0)}},
        {128.5, {}}};
    assert(!observationalHijriAvailable(
        observationalHijriFromLunationEvents(
            binFiveAnchor, 130, 130.71, justBelow59)));
    assert(observationalHijriAvailable(
        observationalHijriFromLunationEvents(
            binFiveAnchor, 130, 130.71, 59.0)));
    const auto unsupportedLatitude = observationalHijriFromLunationEvents(
        binFiveAnchor, 130, 130.71, justAbove60);
    assert(unsupportedLatitude.availability
           == HijriAvailabilityReason::LatitudeUnsupported);

    // The newest nine numerical lunations are the hard history limit.
    std::vector<HijriLunationEvents> nineLunations;
    nineLunations.push_back(
        {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                           100.70, 100.75, 1, 6.0)}});
    for (int index = 1; index < MAX_HIJRI_HISTORY_LUNATIONS; ++index)
        nineLunations.push_back({99.5 + 29.5 * index, {}});
    const auto ninthGroupAnchor = observationalHijriFromLunationEvents(
        nineLunations, 336, 336.71, 25.0);
    assert(observationalHijriAvailable(ninthGroupAnchor));
    std::vector<HijriLunationEvents> tenLunations = nineLunations;
    tenLunations.push_back({99.5 + 29.5 * MAX_HIJRI_HISTORY_LUNATIONS, {}});
    const auto tenthGroupAnchor = observationalHijriFromLunationEvents(
        tenLunations, 366, 366.71, 25.0);
    assert(tenthGroupAnchor.availability
           == HijriAvailabilityReason::HistoryUnavailable);

    // Replay early, normal, missing-event, and normal lunations across every
    // sunset and derive the actual starts from the displayed day number.
    const std::vector<HijriLunationEvents> compliantHistory = {
        {99.5, {hijriEvent(100, 2026, 7, 15, 99.5,
                          100.70, 100.75, 1, 6.0)}},
        {126.4, {hijriEvent(126, 2026, 8, 10, 126.4,
                           126.70, 126.75, 0, 6.0)}},
        {157.4, {hijriEvent(158, 2026, 9, 9, 157.4,
                           158.70, 158.75, 1, 6.0)}},
        {187.4, {}},
        {216.4, {hijriEvent(217, 2026, 11, 7, 216.4,
                           217.70, 217.75, 1, 6.0)}}};
    std::vector<long long> actualStarts;
    for (long long targetDay = 100; targetDay <= 217; ++targetDay)
    {
        const auto displayed = observationalHijriFromLunationEvents(
            compliantHistory, targetDay,
            static_cast<double>(targetDay) + 0.71, 25.0);
        assert(observationalHijriAvailable(displayed));
        if (displayed.date.calculated.day == 1)
            actualStarts.push_back(targetDay);
    }
    const std::vector<long long> expectedStarts{100, 129, 158, 188, 217};
    assert(actualStarts == expectedStarts);
    for (std::size_t index = 1; index < actualStarts.size(); ++index)
    {
        const long long length = actualStarts[index]
                                 - actualStarts[index - 1];
        assert(length == 29 || length == 30);
    }

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

    const auto assertIndices = [](const std::vector<std::size_t>& actual,
                                  std::initializer_list<std::size_t> expected)
    {
        assert(actual == std::vector<std::size_t>(expected));
    };
    assertIndices(visibilityTransitionIndices(
                      {0.5, 1.2, 1.35, 3.0, 5.83, 8.0},
                      CrescentEventKind::Evening),
                  {1, 2, 4});
    assertIndices(visibilityTransitionIndices(
                      {1.2, 6.0, 8.0}, CrescentEventKind::Evening),
                  {0, 1});
    assertIndices(visibilityTransitionIndices(
                      {1.2, 5.83}, CrescentEventKind::Evening),
                  {0, 1});
    assert(visibilityTransitionIndices(
               {5.83}, CrescentEventKind::Evening).empty());
    assert(visibilityTransitionIndices(
               {0.5, 1.35, 4.0}, CrescentEventKind::Evening).empty());
    assert(visibilityTransitionIndices(
               {2.0, 5.83}, CrescentEventKind::Evening).empty());
    assert(visibilityTransitionIndices(
               {0.5, 1.349999}, CrescentEventKind::Evening).empty());

    assertIndices(visibilityTransitionIndices(
                      {8.0, 6.0, 5.83, 4.0, 1.35, 1.34},
                      CrescentEventKind::Morning),
                  {2, 3, 5});
    assertIndices(visibilityTransitionIndices(
                      {6.0, 1.0}, CrescentEventKind::Morning),
                  {0, 1});
    assertIndices(visibilityTransitionIndices(
                      {5.83, 1.34}, CrescentEventKind::Morning),
                  {0, 1});
    assert(visibilityTransitionIndices(
               {1.34}, CrescentEventKind::Morning).empty());
    assert(visibilityTransitionIndices(
               {6.0, 4.0, 2.0}, CrescentEventKind::Morning).empty());
    assert(visibilityTransitionIndices(
               {6.0, 5.83}, CrescentEventKind::Morning).empty());
    assert(visibilityTransitionIndices(
               {4.0, 1.0}, CrescentEventKind::Morning).empty());

    const std::optional<double> missingV;
    assertIndices(visibilityTransitionIndices(
                      {0.7, missingV, std::nan(""), 1.5,
                       missingV, 5.9},
                      CrescentEventKind::Evening),
                  {0, 3, 5});
    assertIndices(visibilityTransitionIndices(
                      {6.0, missingV, std::nan(""), 4.0,
                       missingV, 1.0},
                      CrescentEventKind::Morning),
                  {0, 3, 5});
    assert(visibilityTransitionIndices(
               {missingV, std::nan("")},
               CrescentEventKind::Evening).empty());
    assertIndices(visibilityTransitionIndices(
                      {1.0, 2.0, 1.1, 6.0},
                      CrescentEventKind::Evening),
                  {0, 1, 3});
    assertIndices(visibilityTransitionIndices(
                      {6.0, 4.0, 6.2, 1.0},
                      CrescentEventKind::Morning),
                  {2, 3});
    assert(visibilityTransitionIndices(
               {1.0, 2.0}, CrescentEventKind::Evening,
               std::nan(""), 5.83).empty());
    assert(visibilityTransitionIndices(
               {1.0, 2.0}, CrescentEventKind::Evening,
               5.83, 1.35).empty());
    assert(visibilityTransitionIndices(
               {}, CrescentEventKind::Evening).empty());
    assert(visibilityTransitionIndices(
               {1.0, std::numeric_limits<double>::infinity(), 6.0},
               CrescentEventKind::Evening)
               == std::vector<std::size_t>({0, 2}));
    assert(visibilityTransitionIndices(
               {1.0, 2.0}, CrescentEventKind::Evening,
               1.35, 1.35).empty());
    assert(visibilityTransitionIndices(
               {1.0, 2.0}, CrescentEventKind::Evening,
               1.35, std::numeric_limits<double>::infinity()).empty());

    std::vector<CrescentEvent> transitionEvents = {
        {74.0, 70.0, 4, CrescentEventKind::Evening},
        {97.0, 100.0, -3, CrescentEventKind::Morning},
        {98.0, 100.0, -2, CrescentEventKind::Morning},
        {99.0, 100.0, -1, CrescentEventKind::Morning},
        {101.0, 100.0, 1, CrescentEventKind::Evening},
        {102.0, 100.0, 2, CrescentEventKind::Evening},
        {103.0, 100.0, 3, CrescentEventKind::Evening},
        {126.0, 129.5, -4, CrescentEventKind::Morning}
    };
    sortCrescentEvents(transitionEvents);
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 99.0, 1, EventFilter::Both)->jd,
                 101.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 101.0, -1, EventFilter::Both)->jd,
                 99.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 99.0, 1,
                     EventFilter::Evening)->jd,
                 101.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 101.0, -1,
                     EventFilter::Morning)->jd,
                 99.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 99.0, 1,
                     EventFilter::Morning)->jd,
                 126.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 101.0, -1,
                     EventFilter::Evening)->jd,
                 74.0));
    assert(close(adjacentCrescentEvent(
                     transitionEvents, 101.0 - 0.5 / 86400.0, 1,
                     EventFilter::Evening)->jd,
                 102.0));

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
