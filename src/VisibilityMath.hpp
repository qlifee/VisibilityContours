#ifndef VISIBILITYMATH_HPP
#define VISIBILITYMATH_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace VisibilityMath
{
constexpr double YALLOP_ODEH_OFFSET_DEG = 4.672;

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
}

#endif
