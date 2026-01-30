#include "FarmDb.hpp"
#include "FarmGeo.hpp"
#include "BoundarySwaths.hpp"

#include <format>
#include <string>

namespace farm_db {

const char* Name(Swath::Type x) noexcept {
  switch (x) {
    case Swath::Type::AB:     return "AB";
    case Swath::Type::APlus:  return "APlus";
    case Swath::Type::Curve:  return "Curve";
    case Swath::Type::Pivot:  return "Pivot";
    case Swath::Type::Spiral: return "Spiral";
    default: return nullptr;
  }
} // Name(Swath::Type)

const char* Name(Swath::Option x) noexcept {
  switch (x) {
    case Swath::Option::CW:   return "CW";
    case Swath::Option::CCW:  return "CCW";
    case Swath::Option::Full: return "Full";
    default: return nullptr;
  }
} // Name(Swath::Option)

const char* Name(Swath::Direction x) noexcept {
  switch (x) {
    case Swath::Direction::Both:  return "Both";
    case Swath::Direction::Left:  return "Left";
    case Swath::Direction::Right: return "Right";
    case Swath::Direction::None:  return "None";
    default: return nullptr;
  }
} // Name(Swath::Direction)

const char* Name(Swath::Extension x) noexcept {
  switch (x) {
    case Swath::Extension::Both:  return "Both";
    case Swath::Extension::First: return "First";
    case Swath::Extension::Last:  return "Last";
    case Swath::Extension::None:  return "None";
    default: return nullptr;
  }
} // Name(Swath::Extension)

const char* Name(Swath::Method x) noexcept {
  switch (x) {
    case Swath::Method::NoGps:       return "NoGps";
    case Swath::Method::GNSS:        return "GNSS";
    case Swath::Method::DGNSS:       return "DGNSS";
    case Swath::Method::PreciseGNSS: return "PreciseGNSS";
    case Swath::Method::RtkInt:      return "RtkInt";
    case Swath::Method::RtkFloat:    return "RtkFloat";
    case Swath::Method::DR:          return "DR";
    case Swath::Method::Manual:      return "Manual";
    case Swath::Method::Sim:         return "Sim";
    case Swath::Method::PC:          return "PC";
    case Swath::Method::Other:       return "Other";
    default: return nullptr;
  }
} // Name(Swath::Method)

void Field::inset(const std::string& name, Distance dist) {
  swaths.clear();
  int f = 0;
  int i = 0;
  for (const auto& part: parts) {
    auto geoPolys = farm_db::BoundarySwaths(farm_db::Geo(part), dist);
    auto partName = name;
    if (++f != 1)
      partName += " F" + std::to_string(f);
    int n = 0;
    bool useSuffix = (geoPolys.size() > 1);
    for (const auto& geoPoly: geoPolys) {
      auto swathName = partName;
      if (useSuffix)
        swathName += "_" + std::to_string(++n);
      auto& swath = swaths.emplace_back(swathName);
      swath.path = farm_db::MakePath(geoPoly.outer());
      for (const auto& geoRing: geoPoly.inners()) {
        auto innerName = std::format("{} I{}", name , ++i);
        auto& swath2 = swaths.emplace_back(innerName);
        swath2.path = farm_db::MakePath(geoRing);
      }
    }
  }
} // inset

void FarmDb::inset(const std::string& name, Distance dist) {
  for (auto& field: fields)
    field->inset(name, dist);
} // inset

} // farm_db
