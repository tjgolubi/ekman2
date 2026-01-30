#include "FarmGeo.hpp"

#include <boost/geometry/io/wkt/write.hpp>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace ggl = boost::geometry;

namespace farm_db {

void FarmDb::writeWkt(const std::filesystem::path& output) const {
  auto os = std::ofstream{output, std::ios::binary};
  os.precision(15);
  if (!os) {
    throw std::runtime_error{"FarmDb::writeWkt: cannot write to '"
                       + output.generic_string() + "'"};
  }
  for (const auto& field: fields) {
    int p = 0;
    auto useSuffix = (field->parts.size() > 1);
    for (const auto& part:  field->parts) {
      auto partName = std::string{"Boundary"};
      if (useSuffix) partName += " F" + std::to_string(++p);
      os << field->name << '\t' << partName
         << '\t' << ggl::wkt(part) << '\n';
    }
    for (const auto& swath: field->swaths) {
      os << field->name << '\t' << swath.name
         << '\t' << ggl::wkt(swath.path) << '\n';
    }
  }
} // FarmDb::writeWkt

} // farm_db
