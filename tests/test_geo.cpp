#include "framework.h"
#include "geo.h"
#include <cmath>

namespace
{
    bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
}

void test_geo()
{
    using geo::Point;

    SECTION("distanceMeters");
    // Same point -> 0.
    CHECK(geo::distanceMeters({59.85, 17.65}, {59.85, 17.65}) == 0.0);
    // 0.001 deg of latitude ~= 111.32 m (within a metre).
    CHECK(near(geo::distanceMeters({59.85, 17.65}, {59.851, 17.65}), 111.32, 1.0));
    // Symmetric.
    CHECK(near(geo::distanceMeters({59.85, 17.65}, {59.86, 17.66}),
              geo::distanceMeters({59.86, 17.66}, {59.85, 17.65}), 1e-6));
    // A degree of longitude shrinks by cos(lat) at ~60N (~0.5) -> ~55.7 km.
    CHECK(near(geo::distanceMeters({60.0, 17.0}, {60.0, 18.0}), 55660.0, 200.0));

    SECTION("fractionAlongSegment");
    Point from{59.80, 17.60};
    Point to{59.90, 17.60}; // due north, 0.1 deg
    // Midpoint -> ~0.5.
    CHECK(near(geo::fractionAlongSegment(from, to, {59.85, 17.60}), 0.5, 0.01));
    // At endpoints.
    CHECK(near(geo::fractionAlongSegment(from, to, from), 0.0, 1e-6));
    CHECK(near(geo::fractionAlongSegment(from, to, to), 1.0, 1e-6));
    // Before `from` and beyond `to` clamp to 0 / 1.
    CHECK(geo::fractionAlongSegment(from, to, {59.70, 17.60}) == 0.0);
    CHECK(geo::fractionAlongSegment(from, to, {60.00, 17.60}) == 1.0);
    // Off to the side still projects onto the along-track position (~0.5).
    CHECK(near(geo::fractionAlongSegment(from, to, {59.85, 17.62}), 0.5, 0.02));
    // Degenerate segment -> 0.
    CHECK(geo::fractionAlongSegment(from, from, {59.85, 17.60}) == 0.0);
}
