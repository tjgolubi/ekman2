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
#if 0
    for (const auto& part:  field->parts)
      os << field->name << "\tBoundary\t" << ggl::wkt(Geo(part)) << '\n';
#endif
    for (const auto& swath: field->swaths)
      os << field->name << "\tSwath\t" << ggl::wkt(Geo(swath.path)) << '\n';
  }
} // FarmDb::writeWkt

} // farm_db
