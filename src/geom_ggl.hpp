/// @file
/// @copyright 2025 Terry Golubiewski, all rights reserved.
/// @author Terry Golubiewski

#pragma once
#include "geom.hpp"

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/core/cs.hpp>

#include <type_traits>
#include <cstddef>

namespace boost::geometry::traits {

template<>
struct tag<geom::Pt> { using type = point_tag; };

template<>
struct coordinate_type<geom::Pt> { using type = double; };

template<>
struct coordinate_system<geom::Pt> { using type = cs::cartesian; };

template<>
struct dimension<geom::Pt> : std::integral_constant<std::size_t, 2> { };

template<std::size_t Dim>
requires (Dim == 0 || Dim == 1)
struct access<geom::Pt, Dim> {
  static constexpr double get(const geom::Pt& p)
    { return p[Dim].numerical_value_in(mp_units::si::metre); }
  static constexpr void set(geom::Pt& p, double v)
    { p[Dim] = v * mp_units::si::metre; }
}; // access

template<>
struct make<geom::Pt> {
  using point_type = geom::Pt;
  static constexpr auto is_specialized = true;
  static constexpr point_type apply(double x, double y)
    { return point_type{x * mp_units::si::metre, y * mp_units::si::metre}; }
}; // make

} // boost::geometry::traits
