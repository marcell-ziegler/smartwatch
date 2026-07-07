#pragma once

// Modular geographic helpers. Straight-line ("as the crow flies") for now; a
// polyline / along-track variant can be added behind this same interface later
// (fractionAlongPolyline, distanceAlongRoute) without changing callers.
namespace geo
{
    struct Point
    {
        double lat = 0.0;
        double lon = 0.0;
    };

    // Distance in metres between two lat/lon points, via an equirectangular
    // approximation (local planar projection with cos-lat scaling). Sub-metre
    // accurate over a line this short (tens of km) and cheap. Swap for
    // haversine if a much longer line ever needs it.
    double distanceMeters(Point a, Point b);

    // Fraction 0..1 of the way from `from` to `to` that `user` projects onto,
    // clamped to the segment: 0 = at/behind `from`, 1 = at/beyond `to`. This is
    // the straight-segment version; the polyline version will return an
    // arc-length fraction along the traced track. Degenerate (from == to)
    // returns 0.
    double fractionAlongSegment(Point from, Point to, Point user);
}
