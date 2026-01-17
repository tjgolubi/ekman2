/// @file
/// @copyright 2025 Terry Golubiewski, all rights reserved.
/// @author Terry Golubiewski
#pragma once

#include "FarmGeo.hpp"
#include "FarmXy.hpp"

#include <vector>

namespace farm_db {

using Distance = geom::Distance;

constexpr Distance DefaultSimplifyTol = 0.10 * mp_units::si::metre;

std::vector<xy::MultiPath>
BoundarySwaths(const xy::Polygon& poly_in, Distance offset,
               Distance simplifyTol = DefaultSimplifyTol);

std::vector<geo::MultiPath>
BoundarySwaths(const geo::Polygon& poly_in, Distance offset,
               Distance simplifyTol = DefaultSimplifyTol);

} // farm_db
