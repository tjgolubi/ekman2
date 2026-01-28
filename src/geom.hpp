#pragma once

#include <mp-units/systems/isq/space_and_time.h>
#include <mp-units/systems/si/units.h>
#include <mp-units/systems/si/unit_symbols.h>
#include <mp-units/systems/si/math.h>
#include <mp-units/math.h>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/core/cs.hpp>

#include <gsl-lite/gsl-lite.hpp>

#include <array>
#include <tuple>
#include <type_traits>
#include <cmath>

namespace geom {

using Radians =
      mp_units::quantity<mp_units::isq::angular_measure[mp_units::si::radian]>;

using Distance =
      mp_units::quantity<mp_units::isq::distance[mp_units::si::metre]>;

template<typename U> struct SquaredType {
  using type = decltype(std::declval<U>() * std::declval<U>());
};

template<typename U>
using SquaredTypeT = SquaredType<U>::type;

using DistanceSq = SquaredTypeT<Distance>;

class Vec {
  static constexpr std::size_t Dim = 2;
  using value_type   = Distance;
  using squared_type = DistanceSq;

private:
  std::array<value_type, Dim> a;

public:
  template<std::size_t I> requires (I < Dim)
  [[nodiscard]] constexpr value_type& get() noexcept
    { return std::get<I>(a); }
  template<std::size_t I> requires (I < Dim)
  [[nodiscard]] constexpr const value_type& get() const noexcept
    { return std::get<I>(a); }
  [[nodiscard]] constexpr value_type& dx() noexcept
    { return get<0>(); }
  [[nodiscard]] constexpr const value_type& dx() const noexcept
    { return get<0>(); }
  [[nodiscard]] constexpr value_type& dy() noexcept
    { return get<1>(); }
  [[nodiscard]] constexpr const value_type& dy() const noexcept
    { return get<1>(); }

  constexpr Vec() noexcept = default;
  constexpr Vec(const Vec&) noexcept = default;
  constexpr Vec& operator=(const Vec&) noexcept = default;
  constexpr bool operator==(const Vec&) const noexcept = default;
  constexpr Vec(value_type dx_, value_type dy_) noexcept : a{dx_, dy_} { }
  constexpr Vec(value_type mag, Radians theta) noexcept
    : a{mag * mp_units::si::cos(theta), mag * mp_units::si::sin(theta)}
    { gsl_Expects(mag >= value_type::zero()); }

  [[nodiscard]] constexpr const Vec& operator+() const noexcept
    { return *this; }
  [[nodiscard]] constexpr Vec operator-() const noexcept
    { return Vec{-a[0], -a[1]}; }

  constexpr Vec& operator+=(const Vec& rhs) noexcept
    { a[0] += rhs.a[0]; a[1] += rhs.a[1]; return *this; }
  constexpr Vec& operator-=(const Vec& rhs) noexcept
    { a[0] -= rhs.a[0]; a[1] -= rhs.a[1]; return *this; }
  constexpr Vec& operator*=(double s) noexcept
    { a[0] *= s; a[1] *= s; return *this; }
  constexpr Vec& operator/=(double s) noexcept
    { return *this *= (1/s); }

  [[nodiscard]] constexpr Vec operator+(const Vec& rhs) const noexcept
    { return Vec{a[0]+rhs.a[0], a[1]+rhs.a[1]}; }
  [[nodiscard]] constexpr Vec operator-(const Vec& rhs) const noexcept
    { return Vec{a[0]-rhs.a[0], a[1]-rhs.a[1]}; }
  [[nodiscard]] constexpr Vec operator*(double s) const noexcept
    { return Vec{a[0]*s, a[1]*s}; }
  [[nodiscard]] constexpr Vec operator/(double s) const noexcept
    { return *this * (1/s); }
  [[nodiscard]] constexpr squared_type norm2() const noexcept
    { return a[0]*a[0] + a[1]*a[1]; }
  [[nodiscard]] constexpr value_type norm() const noexcept
    { return mp_units::hypot(a[0], a[1]); }
  [[nodiscard]] constexpr auto angle() const noexcept
    { return mp_units::si::atan2(a[1], a[0]); }
  [[nodiscard]] friend constexpr Vec operator*(double s, const Vec& rhs) noexcept
    { return rhs * s; }
  [[nodiscard]] friend constexpr squared_type dot(const Vec& u, const Vec& v) noexcept
    { return u.a[0] * v.a[0] + u.a[1] * v.a[1]; }
  [[nodiscard]] friend constexpr squared_type operator*(const Vec& u, const Vec& v) noexcept
    { return dot(u, v); }
  [[nodiscard]] friend constexpr squared_type cross(const Vec& u, const Vec& v) noexcept
    { return u.a[0] * v.a[1] - u.a[1] * v.a[0]; }
  [[nodiscard]] constexpr auto angle_wrt(const Vec& ref) const noexcept
    { return mp_units::si::atan2(cross(ref, *this), dot(ref, *this)); }
  [[nodiscard]] constexpr Vec rotate(Radians angle) noexcept {
    const auto c = mp_units::si::cos(angle);
    const auto s = mp_units::si::sin(angle);
    return { c * a[0] - s * a[1], s * a[0] + c * a[1] };
  }
  [[nodiscard]] constexpr value_type& operator[](int idx) noexcept
    { return a[idx]; }
  [[nodiscard]] constexpr const value_type& operator[](int idx) const noexcept
    { return a[idx]; }
  [[nodiscard]] constexpr value_type& at(int idx)
    { return a.at(idx); }
  [[nodiscard]] constexpr const value_type& at(int idx) const
    { return a.at(idx); }
}; // Vec

class Pt {
  static constexpr std::size_t Dim = 2;
  using value_type = Distance;
  Vec v; // displacement from origin

public:
  template<std::size_t I> requires (I < Dim)
  [[nodiscard]] constexpr value_type& get() noexcept
    { return v.template get<I>(); }
  template<std::size_t I> requires (I < Dim)
  [[nodiscard]] constexpr const value_type& get() const noexcept
    { return v.template get<I>(); }
  [[nodiscard]] constexpr value_type& x() noexcept
    { return get<0>(); }
  [[nodiscard]] constexpr const value_type& x() const noexcept
    { return get<0>(); }
  [[nodiscard]] constexpr value_type& y() noexcept
    { return get<1>(); }
  [[nodiscard]] constexpr const value_type& y() const noexcept
    { return get<1>(); }

  constexpr Pt() = default;
  constexpr Pt(const Pt&) = default;
  constexpr Pt& operator=(const Pt&) = default;
  constexpr bool operator==(const Pt&) const = default;
  constexpr Pt(Distance x_, Distance y_) : v{x_, y_} { }

  [[nodiscard]] constexpr value_type& operator[](int idx) noexcept
    { return v[idx]; }
  [[nodiscard]] constexpr const value_type& operator[](int idx) const
    { return v[idx]; }
  [[nodiscard]] constexpr value_type& at(int idx)
    { return v.at(idx); }
  [[nodiscard]] constexpr const value_type& at(int idx) const
    { return v.at(idx); }
}; // Pt

constexpr Vec operator-(const Pt& lhs, const Pt& rhs) noexcept
  { return Vec{lhs.x()-rhs.x(), lhs.y()-rhs.y()}; }

constexpr Pt operator+(const Pt& p, const Vec& v) noexcept
  { return Pt{p.x() + v.dx(), p.y() + v.dy()}; }

constexpr Pt operator-(const Pt& p, const Vec& v) noexcept
  { return Pt{p.x() - v.dx(), p.y() - v.dy()}; }

constexpr Distance Dist(const Pt& a, const Pt& b) noexcept
  { return (b - a).norm(); }

constexpr DistanceSq Dist2(const Pt& a, const Pt& b) noexcept
  { return (b - a).norm2(); }

} // geom

namespace std {

template<>
struct tuple_size<geom::Pt> : integral_constant<size_t, 2> {};

template<size_t I> requires (I < 2)
struct tuple_element<I, geom::Pt> { using type = geom::Distance; };

template<>
struct tuple_size<geom::Vec> : integral_constant<size_t, 2> {};

template<size_t I> requires (I < 2)
struct tuple_element<I, geom::Vec> { using type = geom::Distance; };

} // std

namespace geom::test {

using namespace mp_units::si::unit_symbols;

constexpr auto p0 = Pt{0 * m, 0 * m};
constexpr auto p1 = Pt{3 * m, 0 * m};
constexpr auto p2 = Pt{3 * m, 4 * m};
constexpr auto vx = p1 - p0;
constexpr auto vy = p2 - p1;
constexpr auto vh = vx + vy;
static_assert(p0 + vh == p2);
static_assert(vh.norm2() == 25.0 * m * m);
#if 0
static_assert(vh.norm() == 5.0);
static_assert(vx.angle() == Radians{0.0});
static_assert(vy.angle() == Radians{HalfPi});
static_assert(vh.angle() == mp_units::si::atan2(4.0, 3.0));
static_assert(vh.angle_wrt(vx) == vh.angle());
static_assert(vh.angle_wrt(vy) == vh.angle() - Radians{HalfPi});
#endif

} // geom::test
