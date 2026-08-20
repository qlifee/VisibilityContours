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

    std::cout << "VisibilityMathTests passed\n";
    return 0;
}
