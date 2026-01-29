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

geo::Ring MakeGeoRing(const Path& path) {
  auto out = geo::Ring{path.begin(), path.end()};
  ggl::correct(out);
  auto msg = std::string{};
  if (!ggl::is_valid(out, msg))
    throw std::runtime_error{"MakeGeoRing: not a ring: " + msg};
  return out;
} // MakeGeoRing

geo::Hole MakeGeoHole(const Path& path) {
  auto out = geo::Hole{path.begin(), path.end()};
  ggl::correct(out);
  auto msg = std::string{};
  if (!ggl::is_valid(out, msg))
    throw std::runtime_error{"MakeGeoHole: not a hole: " + msg};
  return out;
} // MakeGeoHole

geo::Polygon Geo(const Polygon& poly) {
  auto out = geo::Polygon{};
  out.outer() = MakeGeoRing(poly.outer);
  for (const auto& p: poly.inners)
    out.inners().push_back(MakeGeoRing(p));
  ggl::correct(out);
  auto msg = std::string{};
  if (!ggl::is_valid(out, msg))
    throw std::runtime_error{"Geo(Polygon): invalid polygon: " + msg};
  return out;
} // Geo(Polygon)

Polygon MakePolygon(const geo::Polygon& geoPoly) {
  {
    auto msg = std::string{};
    if (!ggl::is_valid(geoPoly, msg))
      throw std::runtime_error{"MakePolygon: invalid polygon: " + msg};
  }
  auto poly = Polygon{};
  poly.outer = MakeRing(geoPoly.outer());
  for (const auto& p: geoPoly.inners())
    poly.inners.push_back(MakeRing(p));
  return poly;
} // MakePolygon

void Field::sortByArea() {
  auto geoParts = std::vector<geo::Polygon>{};
  geoParts.reserve(parts.size());
  for (const auto& p: parts)
    geoParts.emplace_back(Geo(p));
  std::ranges::sort(geoParts, [](const geo::Polygon& a, const geo::Polygon& b) {
    return (ggl::area(a) > ggl::area(b));
  });
  auto p2 = std::vector<Polygon>{};
  p2.reserve(geoParts.size());
  for (const auto& p: geoParts)
    p2.emplace_back(MakePolygon(p));
  parts = std::move(p2);
} // Field::areaSort

} // farm_db
