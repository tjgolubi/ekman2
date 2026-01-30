/// @file
/// Reads ESRI Shapefiles (SHP/SHX/DBF) into a FarmDb.
///
/// The importer is intentionally strict:
/// - Only SHPT_POLYGON is accepted.
/// - DBF schema must match exactly:
///   fid, CLIENTNAME, FARM_NAME, FIELD_NAME, WITH_HOLES
/// - Rings are preserved exactly as stored by QGIS:
///   part 0 is outer, remaining parts are holes.
/// - No polygon correction, closure, or validation is performed.

#include "FarmDb.hpp"

#include <boost/geometry/algorithms/correct.hpp>

#include <shapefil.h>

#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace farm_db {

namespace {

namespace fs = std::filesystem;

[[noreturn]] void ThrowShpError(const fs::path& path, const std::string& msg) {
  auto str = path.generic_string() + ": " + msg;
  throw std::runtime_error{str};
} // ThrowShpError

[[noreturn]] void ThrowShpError(const fs::path& path, int recordIndex0,
                                const std::string& msg)
{
  auto oss = std::ostringstream{};
  oss << path.generic_string() << '(' << (recordIndex0+1) << "): " << msg;
  throw std::runtime_error{oss.str()};
} // ThrowShpError

struct FarmKey {
  std::string client;
  std::string farm;
  bool operator==(const FarmKey&) const = default;
}; // FarmKey

struct FieldKey {
  std::string client;
  std::string farm;
  std::string field;
  bool operator==(const FieldKey&) const = default;
}; // FieldKey

constexpr std::size_t HashCombine(std::size_t h1, std::size_t h2) noexcept
  { return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)); }

template<typename T>
constexpr std::size_t Hash(const T& x) noexcept { return std::hash<T>{}(x); }

template<typename T1, typename T2>
constexpr std::size_t Hash(const T1& x1, const T2& x2) noexcept
  { return HashCombine(Hash(x1), Hash(x2)); }

template<typename T1, typename T2, typename T3>
constexpr std::size_t Hash(const T1& x1, const T2& x2, const T3& x3) noexcept
  { return HashCombine(HashCombine(Hash(x1), Hash(x2)), Hash(x3)); }

struct FarmKeyHash {
  std::size_t operator()(const FarmKey& k) const noexcept
    { return Hash(k.client, k.farm); }
}; // FarmKeyHash

struct FieldKeyHash {
  std::size_t operator()(const FieldKey& k) const noexcept
    { return Hash(k.client, k.farm, k.field); }
}; // FieldKeyHash

[[nodiscard]]
std::string RequireDbfString(const fs::path& shpPath,
                             DBFHandle hDbf,
                             int recordIndex0,
                             int fieldIndex,
                             const char* fieldName)
{
  const auto* s = DBFReadStringAttribute(hDbf, recordIndex0, fieldIndex);
  if (!s || s[0] == '\0') {
    std::ostringstream oss;
    oss << "missing or empty DBF field '" << fieldName << "'";
    ThrowShpError(shpPath, recordIndex0, oss.str());
  }
  return std::string{s};
} // RequireDbfString

void RequireDbfSchemaExact(const fs::path& shpPath, DBFHandle hDbf) {
  constexpr int ExpectedCount = 5;
  constexpr const char* ExpectedNames[ExpectedCount] = {
    "fid",
    "CLIENTNAME",
    "FARM_NAME",
    "FIELD_NAME",
    "WITH_HOLES",
  };

  const auto nFields = DBFGetFieldCount(hDbf);
  if (nFields != ExpectedCount) {
    std::ostringstream oss;
    oss << "DBF field count mismatch: expected " << ExpectedCount
        << ", got " << nFields;
    ThrowShpError(shpPath, oss.str());
  }

  for (int i = 0; i < ExpectedCount; ++i) {
    char name[64] = {};
    int width = 0;
    int decimals = 0;
    (void) DBFGetFieldInfo(hDbf, i, name, &width, &decimals);

    if (std::string_view{name} != std::string_view{ExpectedNames[i]}) {
      std::ostringstream oss;
      oss << "DBF schema mismatch at field index " << i
          << ": expected '" << ExpectedNames[i]
          << "', got '" << name << "'";
      ThrowShpError(shpPath, oss.str());
    }
  }
} // RequireDbfSchemaExact

void AppendRingLiteral(Polygon& poly, int ringIndex,
                       const double* xs, const double* ys,
                       int start, int end)
{
  // Literal preservation:
  // - do not force closure
  // - do not reorder points
  // - do not validate or correct
  auto& ring = (ringIndex == 0) ? poly.outer() : poly.inners().emplace_back();

  ring.reserve(static_cast<std::size_t>(end - start));

  using mp_units::si::unit_symbols::deg;
  for (int i = start; i != end; ++i) {
    const auto lon = xs[i];
    const auto lat = ys[i];
    ring.emplace_back(lat * deg, lon * deg);
  }
} // AppendRingLiteral

} // local

FarmDb FarmDb::ReadShp(const fs::path& path) {
  if (path.extension() != ".shp") ThrowShpError(path, "expected a .shp file");

  const auto shpPath = path;
  const auto shxPath = fs::path{path}.replace_extension(".shx");
  const auto dbfPath = fs::path{path}.replace_extension(".dbf");

  if (!fs::exists(shpPath)) ThrowShpError(shpPath, "file does not exist");
  if (!fs::exists(shxPath))
    ThrowShpError(shpPath, "missing required sibling .shx file");
  if (!fs::exists(dbfPath))
    ThrowShpError(shpPath, "missing required sibling .dbf file");

  // Shapelib classic API uses narrow paths.
  const auto shpPathStr = shpPath.string();
  const auto dbfPathStr = dbfPath.string();

  SHPHandle hShp = SHPOpen(shpPathStr.c_str(), "rb");
  if (!hShp) ThrowShpError(shpPath, "SHPOpen failed");

  struct ShpCloser {
    void operator()(SHPHandle h) const noexcept { if (h) SHPClose(h); }
  }; // ShpCloser
  using UniqShpPtr =
                  std::unique_ptr<std::remove_pointer_t<SHPHandle>, ShpCloser>;

  const auto shpGuard = UniqShpPtr{hShp};

  DBFHandle hDbf = DBFOpen(dbfPathStr.c_str(), "rb");
  if (!hDbf) ThrowShpError(shpPath, "DBFOpen failed");

  struct DbfCloser {
    void operator()(DBFHandle h) const noexcept { if (h) DBFClose(h); }
  }; // DbfCloser
  using UniqDbfPtr =
                  std::unique_ptr<std::remove_pointer_t<DBFHandle>, DbfCloser>;

  const auto dbfGuard = UniqDbfPtr{hDbf};

  RequireDbfSchemaExact(shpPath, hDbf);

  int shapeType = 0;
  int nEntities = 0;
  double minBound[4] = {};
  double maxBound[4] = {};
  SHPGetInfo(hShp, &nEntities, &shapeType, minBound, maxBound);

  if (shapeType != SHPT_POLYGON) {
    std::ostringstream oss;
    oss << "unsupported shape type: " << shapeType
        << " (only SHPT_POLYGON is allowed)";
    ThrowShpError(shpPath, oss.str());
  }

  if (DBFGetRecordCount(hDbf) != nEntities) {
    std::ostringstream oss;
    oss << "record count mismatch: SHP has " << nEntities
        << ", DBF has " << DBFGetRecordCount(hDbf);
    ThrowShpError(shpPath, oss.str());
  }

  auto db = FarmDb{};

  std::unordered_map<std::string, Customer*> customersByName;
  std::unordered_map<FarmKey,   Farm*, FarmKeyHash > farmsByKey;
  std::unordered_map<FieldKey, Field*, FieldKeyHash> fieldsByKey;

  for (int i = 0; i < nEntities; ++i) {
    const auto clientName = RequireDbfString(shpPath, hDbf, i, 1, "CLIENTNAME");
    const auto farmName   = RequireDbfString(shpPath, hDbf, i, 2, "FARM_NAME");
    const auto fieldName  = RequireDbfString(shpPath, hDbf, i, 3, "FIELD_NAME");

    Customer* customer = nullptr;
    {
      auto it = customersByName.find(clientName);
      if (it != customersByName.end()) {
        customer = it->second;
      } else {
        db.customers.push_back(std::make_unique<Customer>(clientName));
        customer = db.customers.back().get();
        customersByName.emplace(customer->name, customer);
      }
    }

    Farm* farm = nullptr;
    {
      FarmKey k{clientName, farmName};
      auto it = farmsByKey.find(k);
      if (it != farmsByKey.end()) {
        farm = it->second;
      } else {
        db.farms.push_back(std::make_unique<Farm>(farmName));
        farm = db.farms.back().get();
        farm->customer = customer;
        customer->farms.push_back(farm);
        farmsByKey.emplace(std::move(k), farm);
      }

      if (farm->customer != customer)
        ThrowShpError(shpPath, i, "farm->customer mismatch for this record");
    }

    Field* field = nullptr;
    {
      FieldKey k{clientName, farmName, fieldName};
      auto it = fieldsByKey.find(k);
      if (it != fieldsByKey.end()) {
        field = it->second;
      } else {
        db.fields.push_back(std::make_unique<Field>(fieldName));
        field = db.fields.back().get();
        field->customer = customer;
        field->farm = farm;
        farm->fields.push_back(field);
        fieldsByKey.emplace(std::move(k), field);
      }

      if (field->farm != farm)
        ThrowShpError(shpPath, i, "field->farm mismatch for this record");

      if (field->customer != customer) {
        ThrowShpError(shpPath, i, "field->customer mismatch for this record");
      }

      if (!field->farm || field->farm->customer != field->customer) {
        ThrowShpError(shpPath, i,
                "invariant violated: field->farm->customer != field->customer");
      }
    }

    auto obj = SHPReadObject(hShp, i);
    if (!obj) ThrowShpError(shpPath, i, "SHPReadObject failed");

    struct ObjDestroy {
      void operator()(SHPObject* p) const noexcept
        { if (p) SHPDestroyObject(p); }
    }; // ObjDestroy
    using UniqObjPtr = std::unique_ptr<SHPObject, ObjDestroy>;

    const auto objGuard = UniqObjPtr{obj};

    if (obj->nParts <= 0)
      ThrowShpError(shpPath, i, "polygon has no parts/rings");

    if (obj->nVertices <= 0)
      ThrowShpError(shpPath, i, "polygon has no vertices");

    auto poly = Polygon{};

    for (int part = 0; part != obj->nParts; ++part) {
      const auto start = obj->panPartStart[part];
      const auto end = (part + 1) < obj->nParts
                     ? obj->panPartStart[part + 1]
                     : obj->nVertices;

      if (start < 0 || end < 0 || start >= end || end > obj->nVertices)
        ThrowShpError(shpPath, i, "invalid part vertex range");

      AppendRingLiteral(poly, part, obj->padfX, obj->padfY, start, end);
    }

    boost::geometry::correct(poly);
    field->parts.push_back(std::move(poly));
  }

  return db;
} // FarmDb::ReadShp

} // farm_db
