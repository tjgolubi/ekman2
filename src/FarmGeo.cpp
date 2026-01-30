#include "FarmGeo.hpp"

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/core/cs.hpp>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <exception>
#include <utility>

namespace farm_db {

namespace geo {

Path MakePath(const std::vector<LatLon>& pts)
  { return Path{pts.begin(), pts.end()}; }

Ring MakeRing(const std::vector<LatLon>& pts) {
  auto out = Ring{pts.begin(), pts.end()};
  ggl::correct(out);
  auto msg = std::string{};
  if (!ggl::is_valid(out, msg))
    throw std::runtime_error{"MakeGeoRing: not a ring: " + msg};
  return out;
} // MakeRing

Hole MakeHole(const std::vector<LatLon>& pts) {
  auto out = Hole{pts.begin(), pts.end()};
  ggl::correct(out);
  auto msg = std::string{};
  if (!ggl::is_valid(out, msg))
    throw std::runtime_error{"MakeHole: not a hole: " + msg};
  return out;
} // MakeHole

} // geo

void Field::sortByArea() {
  std::ranges::sort(parts, [](const geo::Polygon& a, const geo::Polygon& b) {
    return (ggl::area(a) > ggl::area(b));
  });
} // Field::areaSort

} // farm_db
