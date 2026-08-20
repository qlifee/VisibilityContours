#include "VisibilityMath.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

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

    assert(close(CONVENTIONAL_SUN_CENTER_ALTITUDE_DEG, -0.8333));
    assert(close(theoreticalWidth(0.0, 0.0), 0.0));
    assert(close(illuminatedWidth(0.1, 0.5), 3.0));
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
    assert(formatLocalTime(2451544.5 + 86399.6 / 86400.0, 0.0) == "0h00m00s");
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
    assert(validCrescentEvent(97.0, 97.0, 100.0, 0.01));
    assert(validCrescentEvent(103.0, 103.0, 100.0, 0.01));
    assert(!validCrescentEvent(96.0, 96.0, 100.0, 0.01));
    assert(!validCrescentEvent(104.0, 104.0, 100.0, 0.01));
    assert(!validCrescentEvent(99.0, 99.0, 100.0, 0.0));
    assert(!validCrescentEvent(99.0, 99.0, 100.0, -0.01));

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

    std::vector<CrescentEvent> lunations = {
        {103.0, 100.0, 3, CrescentEventKind::Evening},
        {126.5, 129.5, -3, CrescentEventKind::Morning}
    };
    sortCrescentEvents(lunations);
    const auto nextLunation = adjacentCrescentEvent(lunations, 103.0, 1);
    const auto previousLunation = adjacentCrescentEvent(lunations, 126.5, -1);
    assert(nextLunation && close(nextLunation->jd, 126.5));
    assert(previousLunation && close(previousLunation->jd, 103.0));

    std::cout << "VisibilityMathTests passed\n";
    return 0;
}
