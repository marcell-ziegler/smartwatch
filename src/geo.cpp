#include "geo.h"
#include <cmath>

namespace geo
{
    namespace
    {
        constexpr double DEG2RAD = 0.017453292519943295; // pi / 180
        constexpr double M_PER_DEG_LAT = 111320.0;       // WGS84 mean

        // Local east/north offset (metres) of `p` from `origin`, using the
        // origin latitude to scale longitude. Good enough for a short line.
        void toLocal(Point p, Point origin, double cosLat, double &east, double &north)
        {
            east = (p.lon - origin.lon) * M_PER_DEG_LAT * cosLat;
            north = (p.lat - origin.lat) * M_PER_DEG_LAT;
        }
    }

    double distanceMeters(Point a, Point b)
    {
        const double cosLat = std::cos((a.lat + b.lat) * 0.5 * DEG2RAD);
        double east, north;
        toLocal(b, a, cosLat, east, north);
        return std::sqrt(east * east + north * north);
    }

    double fractionAlongSegment(Point from, Point to, Point user)
    {
        const double cosLat = std::cos((from.lat + to.lat) * 0.5 * DEG2RAD);

        double abE, abN; // from -> to
        double apE, apN; // from -> user
        toLocal(to, from, cosLat, abE, abN);
        toLocal(user, from, cosLat, apE, apN);

        const double len2 = abE * abE + abN * abN;
        if (len2 <= 0.0)
            return 0.0; // degenerate segment

        double t = (apE * abE + apN * abN) / len2;
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;
        return t;
    }
}
