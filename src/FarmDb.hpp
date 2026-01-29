/// @file
/// @copyright 2025 Terry Golubiewski, all rights reserved.
/// @author Terry Golubiewski

#pragma once

#include "enum_help.hpp"
#include "geom.hpp"

#include <mp-units/systems/isq/space_and_time.h>
#include <mp-units/systems/si.h>
#include <mp-units/framework/quantity.h>

#include <filesystem>
#include <string>
#include <vector>
#include <string_view>
#include <span>
#include <optional>
#include <functional>
#include <utility>

namespace farm_db {

namespace units {

constexpr auto deg = mp_units::si::degree;

constexpr struct heading final
  : mp_units::quantity_spec<mp_units::isq::angular_measure> {} heading;
constexpr struct latitude final
  : mp_units::quantity_spec<mp_units::isq::angular_measure> {} latitude;
constexpr struct longitude final
  : mp_units::quantity_spec<mp_units::isq::angular_measure> {} longitude;

} // units

using HdgDeg = mp_units::quantity<units::heading  [units::deg]>;
using LatDeg = mp_units::quantity<units::latitude [units::deg]>;
using LonDeg = mp_units::quantity<units::longitude[units::deg]>;

struct LatLon {
  LatDeg latitude;
  LonDeg longitude;
  LatLon() = default;
  LatLon(LatDeg lat_, LonDeg lon_) : latitude{lat_}, longitude(lon_) { }
}; // LatLon

using Path = std::vector<LatLon>;
using Ring = Path;

struct Attribute {
  std::string key;
  std::string value;
  Attribute(std::string_view k, const char* v)
    : key{k}, value{v} { }
}; // Attribute

struct Polygon {
  Ring outer;
  std::vector<Ring> inners;
  Polygon() = default;
}; // Polygon

struct Swath {
  enum class Type { AB = 1, APlus, Curve, Pivot, Spiral };
  using TypeList = tjg::EnumList<Type, Type::AB, Type::APlus, Type::Curve,
                                 Type::Pivot, Type::Spiral>;

  enum class Option { CW=1, CCW, Full };
  using OptionList = tjg::EnumList<Option, Option::CW,
      Option::CCW, Option::Full>;

  enum class Direction { Both=1, Left, Right, None };
  using DirectionList = tjg::EnumList<Direction, Direction::Both,
      Direction::Left, Direction::Right, Direction::None>;

  enum class Extension { Both=1, First, Last, None };
  using ExtensionList = tjg::EnumList<Extension, Extension::Both,
      Extension::First, Extension::Last, Extension::None>;

  enum class Method {
    NoGps=0, GNSS, DGNSS, PreciseGNSS, RtkInt, RtkFloat, DR, Manual, Sim,
    PC=16, Other
  };
  using MethodList = tjg::EnumList<Method, Method::NoGps,
      Method::GNSS, Method::DGNSS, Method::PreciseGNSS,
      Method::RtkInt, Method::RtkFloat, Method::DR,
      Method::Manual, Method::Sim, Method::PC, Method::Other>;

  std::string name;
  Type type;
  std::optional<Option>    option;
  std::optional<Direction> direction;
  std::optional<Extension> extension;
  std::optional<HdgDeg>    heading;
  std::optional<Method>    method;
  Path path;
  std::vector<Attribute> otherAttr;
  Swath() = default;
  explicit Swath(std::string_view name_, Type type_ = Type::Curve)
    : name{name_}, type{type_} { }
}; // Swath

const char* Name(Swath::Type      x) noexcept;
const char* Name(Swath::Option    x) noexcept;
const char* Name(Swath::Direction x) noexcept;
const char* Name(Swath::Extension x) noexcept;
const char* Name(Swath::Method    x) noexcept;

struct Customer;
struct Farm;

struct Field {
  std::string name;
  Customer* customer = nullptr;
  Farm* farm = nullptr;
  std::vector<Polygon> parts;
  std::vector<Swath>   swaths;
  std::vector<Attribute> otherAttr;
  Field() = default;
  explicit Field(std::string_view name_) : name{name_} { }
  void inset(const std::string& name, geom::Distance dist);
  void sortByArea();
}; // Field

struct Farm {
  std::string name;
  Customer* customer = nullptr;
  std::vector<Field*> fields;
  std::vector<Attribute> otherAttr;
  Farm() = default;
  explicit Farm(std::string_view name_) : name{name_} { }
}; // Farm

struct Customer {
  std::string name;
  std::vector<Farm*> farms;
  std::vector<Attribute> otherAttr;
  Customer() = default;
  explicit Customer(std::string_view name_) : name{name_} { }
}; // Customer

struct FarmDb {
  int versionMajor       =  3;
  int versionMinor       =  0;
  int dataTransferOrigin = -1;
  std::string swVendor;
  std::string swVersion;
  std::vector<std::unique_ptr<Customer>> customers;
  std::vector<std::unique_ptr<Farm>>     farms;
  std::vector<std::unique_ptr<Field>>    fields;
  std::vector<Attribute> otherAttr;
  FarmDb() = default;
  void inset(const std::string& name, geom::Distance dist);
  void writeXml(const std::filesystem::path& output) const;
  void writeWkt(const std::filesystem::path& output) const;
  static FarmDb ReadXml(const std::filesystem::path& input);
}; // FarmDb

} // farm_db

namespace tjg {
template<>
struct EnumValues<farm_db::Swath::Type>     : farm_db::Swath::TypeList      { };
template<>
struct EnumValues<farm_db::Swath::Option>   : farm_db::Swath::OptionList    { };
template<>
struct EnumValues<farm_db::Swath::Direction>: farm_db::Swath::DirectionList { };
template<>
struct EnumValues<farm_db::Swath::Extension>: farm_db::Swath::ExtensionList { };
template<>
struct EnumValues<farm_db::Swath::Method>    : farm_db::Swath::MethodList   { };
} // tjg
