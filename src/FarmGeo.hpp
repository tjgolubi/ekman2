#pragma once
#include "FarmDb.hpp"
#include <vector>

namespace farm_db {

namespace ggl = boost::geometry;

namespace geo {

using Point        = LatLon;
using LineString   = ggl::model::linestring<Point>;
using PolyLine     = ggl::model::multi_linestring<LineString>;
using Ring         = ggl::model::ring<Point, true >;
using Hole         = ggl::model::ring<Point, false>;
using Polygon      = ggl::model::polygon<Point>;
using MultiPolygon = ggl::model::multi_polygon<Polygon>;
using Path         = LineString;
using MultiPath    = PolyLine;

Path MakePath(const std::vector<LatLon>& pts);
Ring MakeRing(const std::vector<LatLon>& pts);
Hole MakeHole(const std::vector<LatLon>& pts);

} // geo


} // farm_db
