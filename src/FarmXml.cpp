#include "FarmDb.hpp"

#include "get_attr.hpp"

#include <pugixml.hpp>

#include <zip.h>

#include <gsl-lite/gsl-lite.hpp>

#include <regex>
#include <string>
#include <vector>
#include <string_view>
#include <format>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <exception>
#include <ranges>
#include <algorithm>
#include <utility>
#include <cstdint>

namespace gsl = gsl_lite;

// ---------------------------------------------------------------------
// ISOXML constants (centralized so parse/write share the same strings).
namespace isoxml {

constexpr char Root[] = "ISO11783_TaskData";

namespace root_attr {
  constexpr char VersionMajor[]             = "VersionMajor";
  constexpr char VersionMinor[]             = "VersionMinor";
  constexpr char DataTransferOrigin[]       = "DataTransferOrigin";
  constexpr char MgmtSoftwareManufacturer[] = "ManagementSoftwareManufacturer";
  constexpr char MgmtSoftwareVersion[]      = "ManagementSoftwareVersion";
} // root_attr

enum class PointType {
  Flag=1, Other, Access, Storage, Obstacle, GuideA, GuideB,
  GuideCenter, GuidePoint, Field, Base
};

using PointTypes = tjg::EnumList<PointType, PointType::Flag, PointType::Other,
    PointType::Access, PointType::Storage, PointType::Obstacle,
    PointType::GuideA, PointType::GuideB, PointType::GuideCenter,
    PointType::GuidePoint, PointType::Field, PointType::Base>;

const char* Name(PointType x) noexcept {
  switch (x) {
    case PointType::Flag:        return "Flag";
    case PointType::Other:       return "Other";
    case PointType::Access:      return "Access";
    case PointType::Storage:     return "Storage";
    case PointType::Obstacle:    return "Obstacle";
    case PointType::GuideA:      return "GuideA";
    case PointType::GuideB:      return "GuideB";
    case PointType::GuideCenter: return "GuideCenter";
    case PointType::GuidePoint:  return "GuidePoint";
    case PointType::Field:       return "Field";
    case PointType::Base:        return "Base";
    default: return nullptr;
  }
} // Name(PointType)

enum class LineStringType {
  Exterior=1, Interior, TramLine, Sampling, Guidance, Drainage, Fence, Flag,
  Obstacle
};

using LineStringTypes = tjg::EnumList<LineStringType, LineStringType::Exterior,
    LineStringType::Interior, LineStringType::TramLine,
    LineStringType::Sampling, LineStringType::Guidance,
    LineStringType::Drainage, LineStringType::Fence, LineStringType::Flag,
    LineStringType::Obstacle>;

const char* Name(LineStringType x) noexcept {
  switch (x) {
    case LineStringType::Exterior:  return "Exterior";
    case LineStringType::Interior:  return "Interior";
    case LineStringType::TramLine:  return "TramLine";
    case LineStringType::Sampling:  return "Sampling";
    case LineStringType::Guidance:  return "Guidance";
    case LineStringType::Drainage:  return "Drainage";
    case LineStringType::Fence:     return "Fence";
    case LineStringType::Flag:      return "Flag";
    case LineStringType::Obstacle:  return "Obstacle";
    default: return nullptr;
  }
} // Name(LineStringType)

enum class PolygonType {
  Boundary=1, Treatment, Water, Building, Road, Obstacle, Flag, Other, Field,
  Headland, Buffer, Windbreak
};

using PolygonTypes = tjg::EnumList<PolygonType, PolygonType::Boundary,
    PolygonType::Treatment, PolygonType::Water, PolygonType::Building,
    PolygonType::Road, PolygonType::Obstacle, PolygonType::Flag,
    PolygonType::Other, PolygonType::Field, PolygonType::Headland,
    PolygonType::Buffer, PolygonType::Windbreak>;

const char* Name(PolygonType x) noexcept {
  switch (x) {
    case PolygonType::Boundary:  return "Boundary";
    case PolygonType::Treatment: return "Treatment";
    case PolygonType::Water:     return "Water";
    case PolygonType::Building:  return "Building";
    case PolygonType::Road:      return "Road";
    case PolygonType::Obstacle:  return "Obstacle";
    case PolygonType::Flag:      return "Flag";
    case PolygonType::Other:     return "Other";
    case PolygonType::Field:     return "Field";
    case PolygonType::Headland:  return "Headland";
    case PolygonType::Buffer:    return "Buffer";
    case PolygonType::Windbreak: return "Windbreak";
    default: return nullptr;
  }
} // Name(PolygonType)

} // isoxml

namespace farm_db {

namespace {

using XmlNode = pugi::xml_node;

[[noreturn]] void InvalidNode(const XmlNode& xml, std::string what) {
  auto k = tjg::name(xml);
  what.reserve(what.size() + 8 + k.size());
  what += " on <";
  what += k;
  what += ">";
  throw std::runtime_error{what};
} // InvalidNode

[[noreturn]] void InvalidAttr(const XmlNode& xml, const char* key,
                              std::string what="Invalid attribute")
{
  auto k = std::string_view{key};
  what.reserve(what.size() + k.size() + 40);
  what += " \"";
  what += k;
  what += "\" ";
  auto a = xml.attribute(k);
  if (a) {
    what += "= ";
    what += a.as_string();
  }
  else {
    what += "is missing";
  }
  InvalidNode(xml, what);
} // InvalidAttr

template<typename T>
std::optional<T> GetAttr(const XmlNode& x, const char* key) {
  const auto& a = x.attribute(key);
  return (a) ? tjg::try_get_attr<T>(a) : std::nullopt;
} // GetAttr

template<typename T=std::string>
T RequireAttr(const pugi::xml_node& x, const char* key) {
  const auto& a = x.attribute(key);
  if (!a)
    InvalidAttr(x, key);
  auto v = GetAttr<T>(x, key);
  if (v)
    return *v;
  if constexpr (std::is_same_v<T, std::string>) {
    auto s = a.as_string();
    if (s[0] != '\0')
      return s;
  }
  InvalidAttr(x, key);
} // RequireAttr

int GetId(std::string pfx, const std::string& attr) {
  pfx += "-?([0-9]+)";
  const auto re = std::regex{pfx};
  auto m = std::smatch{};
  if (!std::regex_match(attr, m, re) || m.size() != 2 || !m[1].matched)
    return -1;
  return std::stoi(m[1]);
} // GetId

using IndexDb = std::vector<int>;

gsl::index FindIndex(const IndexDb& db, int id) {
  auto it = std::ranges::find(db, id);
  if (it == db.end())
    return gsl::index{-1};
  return std::distance(db.begin(), it);
} // FindIndex

struct Point {
  isoxml::PointType type;
  LatLon point;
  Point() = default;
  Point(isoxml::PointType type_, const LatLon& pt_)
    : type{type_}, point{pt_} { }
}; // Point

Point ReadPoint(const XmlNode& x) {
  for (const auto& a: x.attributes()) {
    auto k = tjg::name(a);
    if (k == "A" || k == "C" || k == "D")
      continue;
    std::cerr << "ReadPoint: extra attribute ignored: " << k << '\n';
  }
  return Point{RequireAttr<isoxml::PointType>(x, "A"),
               LatLon{RequireAttr<double>(x, "C") * units::deg,
                      RequireAttr<double>(x, "D") * units::deg}};
} // ReadPoint

void WritePoint(XmlNode& node, const LatLon& pt, isoxml::PointType type) {
  using mp_units::si::unit_symbols::deg;
  auto pnt = node.append_child("PNT");
  pnt.append_attribute("A") = static_cast<int>(type);
  pnt.append_attribute("C") = pt.latitude .numerical_value_in(deg);
  pnt.append_attribute("D") = pt.longitude.numerical_value_in(deg);
} // WritePoint

Path ReadPath(const XmlNode& x, isoxml::PointType expPtType) {
  gsl_Expects(tjg::name(x) == "LSG");
  for (const auto& a: x.attributes()) {
    auto k = tjg::name(a);
    if (k == "A")
      continue;
    std::cerr << "ReadPath: extra attribute ignored: " << k << '\n';
  }
  auto pts = Path{};
  pts.reserve(std::distance(x.begin(), x.end()));
  for (const auto& c: x.children()) {
    if (c.type() != pugi::node_element)
      continue;
    auto k = tjg::name(c);
    if (k != "PNT") {
      std::cerr << "ReadPath: element ignored: " <<  k << '\n';
      continue;
    }
    auto pt = ReadPoint(c);
    if (pt.type != expPtType) {
      auto msg = std::string{"ReadPath: expected "} + Name(expPtType) + ": got "
                 + Name(pt.type);
      throw std::runtime_error{msg};
    }
    pts.push_back(pt.point);
  }
  return pts;
} // ReadPath

void WritePath(XmlNode& node, const Path& path, isoxml::LineStringType lsgType,
                                                isoxml::PointType ptType)
{
  auto lsg = node.append_child("LSG");
  lsg.append_attribute("A") = static_cast<int>(lsgType);
  for (const auto& p: path)
    WritePoint(lsg, p, ptType);
} // WritePath

Path ReadSwathPath(const XmlNode& x) {
  using namespace isoxml;
  auto lsgType = RequireAttr<LineStringType>(x, "A");
  if (lsgType != LineStringType::Guidance) {
    auto msg = std::string{"ReadSwathPath: LSG type mismatch: "}
             + Name(lsgType);
    throw std::runtime_error{msg};
  }
  for (const auto& a: x.attributes()) {
    auto k = tjg::name(a);
    if (k == "A")
      continue;
    std::cerr << "ReadSwathPath: extra attribute ignored: " << k << '\n';
  }
  bool firstPt = true;
  bool lastPt  = false;
  auto pts = Path{};
  pts.reserve(std::distance(x.begin(), x.end()));
  for (const auto& c: x.children()) {
    if (c.type() != pugi::node_element)
      continue;
    auto k = tjg::name(c);
    if (k != "PNT") {
      std::cerr << "ReadSwathPath: element ignored: " <<  k << '\n';
      continue;
    }
    auto pt = ReadPoint(c);
    bool err = false;
    using namespace isoxml;
    switch (pt.type) {
      case PointType::GuideA:    err = !firstPt || lastPt; break;
      case PointType::GuidePoint:err =  firstPt || lastPt; break;
      case PointType::GuideB:    err =  firstPt || lastPt; lastPt = true; break;
      default: err = true; break;
    }
    if (err) {
      auto msg = std::string{"ReadSwathPath: unexpected point type: "};
      throw std::runtime_error{msg + Name(pt.type)};
    }
    firstPt = false;
    pts.push_back(pt.point);
  }
  return pts;
} // ReadSwathPath

void WriteSwathPath(XmlNode& node, const Path& path) {
  using namespace isoxml;
  auto lsg = node.append_child("LSG");
  lsg.append_attribute("A") = static_cast<int>(LineStringType::Guidance);
  if (path.empty())
    return;
  auto iter = path.begin();
  WritePoint(lsg, *iter, PointType::GuideA);
  if (++iter == path.end())
    return;
  const auto last = std::prev(path.end());
  while (iter != last)
    WritePoint(lsg, *iter++, PointType::GuidePoint);
  WritePoint(lsg, *iter, PointType::GuideB);
} // WriteSwathPath

Polygon ReadPolygon(const XmlNode& x, isoxml::PolygonType polyType,
                                      isoxml::PointType ptType)
{
  if (RequireAttr<isoxml::PolygonType>(x, "A") != polyType)
    throw std::runtime_error{"ReadPolygon: invalid type"};
  Polygon poly;
  for (const auto& lsg: x.children("LSG")) {
    auto ring = ReadPath(lsg, ptType);
    auto lsgType = RequireAttr<isoxml::LineStringType>(lsg, "A");
    using namespace isoxml;
    switch (lsgType) {
      case LineStringType::Exterior:
        if (!poly.outer.empty())
          throw std::runtime_error{"Polygon: multiple exterior rings"};
        poly.outer = std::move(ring);
        break;
      case LineStringType::Interior:
        poly.inners.emplace_back(std::move(ring));
        break;
      default: {
        auto msg = std::string{"ReadPolygon: unexpected LineString type: "}
                 + Name(lsgType);
        throw std::runtime_error{msg};
      }
    }
  }
  for (const auto& c: x.children()) {
    if (c.type() != pugi::node_element)
      continue;
    auto k = tjg::name(c);
    if (k == "LSG")
      continue;
    std::cerr << "ReadPolygon: element ignored: " << k << '\n';
  }
  if (poly.outer.empty())
    throw std::runtime_error{"ReadPolygon: missing exterior ring"};
  if (std::ssize(poly.outer) < 4)
    throw std::runtime_error{"ReadPolygon: exterior ring too small"};
  for (const auto& r: poly.inners) {
    if (std::ssize(r) < 4)
      throw std::runtime_error{"ReadPolygon: interior ring too small"};
  }
  return poly;
} // ReadPolygon

Polygon ReadBoundary(const XmlNode& x) {
  using namespace isoxml;
  return ReadPolygon(x, PolygonType::Boundary, PointType::Field);
}

void WritePolygon(XmlNode& node, const Polygon& poly,
                  isoxml::PolygonType polyType, isoxml::PointType ptType)
{
  using namespace isoxml;
  auto pln = node.append_child("PLN");
  pln.append_attribute("A") = static_cast<int>(polyType);
  WritePath(pln, poly.outer, LineStringType::Exterior, ptType);
  for (const auto& path: poly.inners)
    WritePath(pln, path, LineStringType::Interior, ptType);
} // WritePolygon

void WriteBoundary(XmlNode& node, const Polygon& poly) {
  using namespace isoxml;
  WritePolygon(node, poly, PolygonType::Boundary, PointType::Field);
} // WriteBoundary

Swath ReadSwath(const XmlNode& node) {
  using namespace isoxml;
  auto idStr = RequireAttr<std::string>(node, "A");
  auto id = GetId("GGP", idStr);
  if (id < 0)
    throw std::runtime_error{"ReadSwath: invalid guide id: " + idStr};
  auto name = RequireAttr<std::string>(node, "B");
  for (const auto& a: node.attributes()) {
    auto k = tjg::name(a);
    if (k == "A" || k == "B")
      continue;
    std::cerr << "ReadSwath: guide attribute ignored: " << k << '\n';
  }
  auto type = Swath::Type{};
  auto path = Path{};
  auto option    = std::optional<Swath::Option>{};
  auto direction = std::optional<Swath::Direction>{};
  auto extension = std::optional<Swath::Extension>{};
  auto method    = std::optional<Swath::Method>{};
  auto heading   = std::optional<HdgDeg>{};
  auto otherAttr = std::vector<Attribute>{};
  for (const auto& c: node.children()) {
    if (c.type() != pugi::node_element)
      continue;
    auto k = tjg::name(c);
    if (k != "GPN") {
      std::cerr << "ReadSwath: ignored guide element: " << k << '\n';
      continue;
    }
    if (!path.empty())
      throw std::runtime_error{"ReadSwath: too many swaths"};
    auto swIdStr = RequireAttr<std::string>(c, "A");
    auto swId = GetId("GPN", swIdStr);
    if (swId != id) {
      throw std::runtime_error{
                        "ReadSwath: id mismatch: " + idStr + " != " + swIdStr};
    }
    type = RequireAttr<Swath::Type>(c, "C");
    for (const auto& a: c.attributes()) {
      using mp_units::si::unit_symbols::deg;
      auto k = tjg::name(a);
      if (k == "A" || k == "C")
        continue;
      if (k == "B") {
        auto name2 = tjg::get_attr<std::string>(a);
        if (name2 != name) {
          std::cerr << "ReadSwath: name mismatch ignored: "
                    << name << " != " << name2 << '\n';
          }
      }
      else if (k == "E") {
        direction = tjg::get_attr<Swath::Direction>(a);
      }
      else if (k == "F") {
        extension = tjg::get_attr<Swath::Extension>(a);
      }
      else if (k == "G") {
        heading = tjg::get_attr<double>(a) * deg;
      }
      else if (k == "I") {
        method = tjg::get_attr<Swath::Method>(a);
      }
      else otherAttr.emplace_back(k, a.value());
    }
    for (const auto& p: c.children()) {
      if (p.type() != pugi::node_element)
        continue;
      auto k = tjg::name(p);
      if (k == "LSG") path = ReadSwathPath(p);
      else std::cerr << "ReadSwath: ignored element: " << k << '\n';
    }
  }
  if (path.empty())
    throw std::runtime_error{"ReadSwath: missing path"};
  auto swath = Swath{std::move(name), type};
  swath.path = std::move(path);
  swath.option    = option;
  swath.direction = direction;
  swath.extension = extension;
  swath.heading   = heading;
  swath.method    = method;
  swath.otherAttr = std::move(otherAttr);
  return swath;
} // ReadSwath

void WriteSwath(XmlNode& node, const Swath& swath, int id) {
  using mp_units::si::unit_symbols::deg;
  using namespace isoxml;
  const auto idStr = std::to_string(id);
  auto ggp = node.append_child("GGP");
  ggp.append_attribute("A") = "GGP" + idStr;
  auto name = swath.name;
  if (name.empty()) name = "Swath" + idStr;
  ggp.append_attribute("B") = name;
  auto gpn = ggp.append_child("GPN");
  gpn.append_attribute("A") = "GPN" + idStr;
  gpn.append_attribute("B") = name;
  gpn.append_attribute("C") = static_cast<int>(swath.type);
  if (swath.option)
    gpn.append_attribute("D") = static_cast<int>(*swath.option);
  gpn.append_attribute("E") =
            static_cast<int>(swath.direction.value_or(Swath::Direction::Both));
  gpn.append_attribute("F") =
            static_cast<int>(swath.extension.value_or(Swath::Extension::Both));
  gpn.append_attribute("G") =
            swath.heading.value_or(0.0 * deg).numerical_value_in(deg);
  gpn.append_attribute("I") =
            static_cast<int>(swath.method.value_or(Swath::Method::NoGps));
#if 0
  if (swath.direction)
    gpn.append_attribute("E") = static_cast<int>(*swath.direction);
  if (swath.extension)
    gpn.append_attribute("F") = static_cast<int>(*swath.extension);
  if (swath.heading)
    gpn.append_attribute("G") = swath.heading->numerical_value_in(deg);
  if (swath.method)
    gpn.append_attribute("I") = static_cast<int>(*swath.method);
#endif
  for (const auto& [k, v]: swath.otherAttr)
    gpn.append_attribute(k) = v;
  WriteSwathPath(gpn, swath.path);
} // WriteSwath

void WriteCustomer(XmlNode& node, const Customer& cust, int id) {
  auto ctr = node.append_child("CTR");
  ctr.append_attribute("A") = "CTR" + std::to_string(id);
  ctr.append_attribute("B") = cust.name;
  for (const auto& [k, v]: cust.otherAttr)
    ctr.append_attribute(k) = v;
} // WriteCustomer

void WriteFarm(XmlNode& node, const Farm& farm, int id, int custId) {
  auto frm = node.append_child("FRM");
  frm.append_attribute("A") = "FRM" + std::to_string(id);
  frm.append_attribute("B") = farm.name;
  if (custId != 0) frm.append_attribute("I") = "CTR" + std::to_string(custId);
  for (const auto& [k, v]: farm.otherAttr)
    frm.append_attribute(k) = v;
} // WriteFarm

void WriteField(XmlNode& node, const Field& field, int id,
                int custId, int farmId, int& swathId)
{
  auto pfd = node.append_child("PFD");
  pfd.append_attribute("A") = "PFD" + std::to_string(id);
  pfd.append_attribute("C") = field.name;
  pfd.append_attribute("D") = 0;
  if (custId != 0) pfd.append_attribute("E") = "CTR" + std::to_string(custId);
  if (farmId != 0) pfd.append_attribute("F") = "FRM" + std::to_string(farmId);
  for (const auto& [k, v]: field.otherAttr)
    pfd.append_attribute(k) = v;
  for (const auto& p: field.parts)
    WriteBoundary(pfd, p);
  for (const auto& s: field.swaths)
    WriteSwath(pfd, s, ++swathId);
} // WriteField

pugi::xml_document ReadZip(const std::filesystem::path& zipPath) {
  auto err = 0;
  auto* zip = zip_open(zipPath.generic_string().c_str(), ZIP_RDONLY, &err);
  if (!zip)
    throw std::runtime_error("zip_open failed");

  constexpr auto name = "TASKDATA/TASKDATA.XML";

  auto* zf = zip_fopen(zip, name, 0);
  if (!zf) {
    zip_close(zip);
    throw std::runtime_error("zip_fopen failed");
  }

  std::string str;
  str.reserve(static_cast<std::size_t>(file_size(zipPath)));
  std::vector<char> buf(8192);

  for (;;) {
    auto n = zip_fread(zf, buf.data(), buf.size());
    if (n < 0) {
      zip_fclose(zf);
      zip_close(zip);
      throw std::runtime_error("zip_fread failed");
    }
    if (n == 0) break;
    str.append(buf.data(), static_cast<std::size_t>(n));
  }

  zip_fclose(zf);
  zip_close(zip);

  auto iss = std::istringstream(str);
  auto doc = pugi::xml_document{};
  auto res = doc.load(iss, pugi::parse_default | pugi::parse_ws_pcdata);
  if (!res) {
    auto msg = std::format("{}: XML parse error: {} (offset {})",
                           name, res.description(), res.offset);
    throw std::runtime_error{msg};
  }
  return doc;
} // ReadZip

} // local

FarmDb FarmDb::ReadXml(const std::filesystem::path& input) {
  auto doc = pugi::xml_document{};

  auto ext = input.extension();

  if (ext == ".xml" || ext == ".XML") {
    auto res = doc.load_file(input.c_str(),
                             pugi::parse_default | pugi::parse_ws_pcdata);
    if (!res) {
      auto msg = std::format("{}: XML parse error: {} (offset {})",
                         input.generic_string(), res.description(), res.offset);
      throw std::runtime_error{msg};
    }
  }
  else if (ext == ".zip") {
    doc = ReadZip(input);
  }
  else {
    auto msg = std::string{"FarmDb::ReadXml: invalid filename extension: "}
             + input.generic_string();
    throw std::runtime_error{msg};
  }

  auto root = doc.child(isoxml::Root);
  if (!root) {
    auto msg = std::format("{}: missing root <{}>",
                           input.generic_string(), isoxml::Root);
    throw std::runtime_error{msg};
  }

  FarmDb db;
  db.versionMajor = RequireAttr<int>(root, isoxml::root_attr::VersionMajor);
  db.versionMinor = RequireAttr<int>(root, isoxml::root_attr::VersionMinor);
  auto custDb  = IndexDb{};
  auto farmDb  = IndexDb{};
  auto fieldDb = IndexDb{};
  for (const auto& a : root.attributes()) {
    const auto k = tjg::name(a);
    if (   k == isoxml::root_attr::VersionMajor
        || k == isoxml::root_attr::VersionMinor)
      continue;
    if (k == isoxml::root_attr::DataTransferOrigin)
      db.dataTransferOrigin = tjg::get_attr<int>(a);
    else if (k == isoxml::root_attr::MgmtSoftwareManufacturer)
      db.swVendor  = tjg::get_attr<std::string>(a);
    else if (k == isoxml::root_attr::MgmtSoftwareVersion)
      db.swVersion = tjg::get_attr<std::string>(a);
    else
      db.otherAttr.emplace_back(k, a.value());
  }
  if (db.versionMajor < 0 || db.versionMinor < 0)
    throw std::runtime_error{"ReadFarmDb: missing VersionMajor/VersionMinor"};
  for (const auto& c: root.children()) {
    if (c.type() != pugi::node_element)
      continue;
    auto k = tjg::name(c);
    if (k == "CTR") {
      auto idStr = RequireAttr<std::string>(c ,"A");
      auto id = GetId("CTR", idStr);
      if (id < 0)
        throw std::runtime_error{"ReadFarmDb: invalid customer id: " + idStr};
      if (std::ranges::contains(custDb, id))
        throw std::runtime_error{"ReadFarmDb: duplicate customer: " + idStr};
      auto cust = std::make_unique<Customer>(RequireAttr<std::string>(c, "B"));
      for (const auto& a: c.attributes()) {
        auto k = tjg::name(a);
        if (k == "A" || k == "B")
          continue;
        cust->otherAttr.emplace_back(a.name(), a.value());
      }
      custDb.push_back(id);
      db.customers.emplace_back(std::move(cust));
    }
    else if (k == "FRM") {
      auto idStr = RequireAttr<std::string>(c ,"A");
      auto id = GetId("FRM", idStr);
      if (id < 0)
        throw std::runtime_error{"ReadFarmDb: invalid farm id: " + idStr};
      if (std::ranges::contains(farmDb, id))
        throw std::runtime_error{"ReadFarmDb: duplicate farm: " + idStr};
      auto farm = std::make_unique<Farm>(RequireAttr<std::string>(c, "B"));
      for (const auto& a: c.attributes()) {
        auto k = tjg::name(a);
        if (k == "A" || k == "B")
          continue;
        if (k == "I") {
          auto ctrIdStr = tjg::get_attr<std::string>(a);
          auto ctrIdx = FindIndex(custDb, GetId("CTR", ctrIdStr));
          if (ctrIdx < 0) {
            throw std::runtime_error{"ReadFarm: invalid customer id: "
                                     + ctrIdStr};
          }
          auto cust = db.customers[ctrIdx].get();
          farm->customer = cust;
        }
        else farm->otherAttr.emplace_back(a.name(), a.value());
      }
      auto ptr = farm.get();
      farmDb.push_back(id);
      db.farms.emplace_back(std::move(farm));
      if (ptr->customer)
        ptr->customer->farms.push_back(ptr);
    }
    else if (k == "PFD") {
      const auto idStr = RequireAttr<std::string>(c, "A");
      auto id = GetId("PFD", idStr);
      if (id < 0)
        throw std::runtime_error{"ReadFarmDb: invalid field id: " + idStr};
      if (std::ranges::contains(fieldDb, id))
        throw std::runtime_error{"ReadFarmDb: duplicate field: " + idStr};
      if (RequireAttr<int>(c, "D") != 0)
        throw std::runtime_error{"ReadFarmDb: non-zero field area"};
      auto field = std::make_unique<Field>(RequireAttr<std::string>(c, "C"));
      for (const auto& a: c.attributes()) {
        auto k = tjg::name(a);
        if (k == "A" || k == "C" || k == "D")
          continue;
        if (k == "E") {
          auto ctrIdStr = tjg::get_attr<std::string>(a);
          auto ctrIdx = FindIndex(custDb, GetId("CTR", ctrIdStr));
          if (ctrIdx < 0) {
            throw std::runtime_error{
                                "ReadFarmDb: invalid customer id: " + ctrIdStr};
          }
          if (field->customer) {
            throw std::runtime_error{
                            "ReadFarmDb: field already belongs to a customer"};
          }
          field->customer = db.customers[ctrIdx].get();
        }
        else if (k == "F") {
          auto frmIdStr = tjg::get_attr<std::string>(a);
          auto frmIdx = FindIndex(farmDb, GetId("FRM", frmIdStr));
          if (frmIdx < 0) {
            throw std::runtime_error{
                                    "ReadFarmDb: invalid farm id: " + frmIdStr};
          }
          if (field->farm) {
            throw std::runtime_error{
                                "ReadFarmDb: field already belongs to a farm"};
          }
          field->farm = db.farms[frmIdx].get();
        }
        else field->otherAttr.emplace_back(k, a.value());
      }
      if (field->farm && field->farm->customer != field->customer)
        throw std::runtime_error{"ReadFarmDb: field/farm customer mismatch"};
      for (const auto& p: c.children()) {
        if (p.type() != pugi::node_element)
          continue;
        auto k = tjg::name(p);
        if      (k == "PLN") field->parts.emplace_back(ReadBoundary(p));
        else if (k == "GGP") field->swaths.emplace_back(ReadSwath(p));
        else std::cerr << "ReadFarmDb: ignored field element " << k << '\n';
      }
      field->sortByArea();
      auto ptr = field.get();
      fieldDb.push_back(id);
      db.fields.emplace_back(std::move(field));
      if (ptr->farm)
        ptr->farm->fields.push_back(ptr);
    }
    else if (k == "VPN") { }
    else std::cerr << "ReadFarmDb: ignored element " << k << '\n';
  }
  return db;
} // FarmDb::ReadXml

namespace {

pugi::xml_document CreateDoc(const FarmDb& db) {
  auto doc = pugi::xml_document{};

  auto decl = doc.prepend_child(pugi::node_declaration);
  decl.append_attribute("version")  = "1.0";
  decl.append_attribute("encoding") = "utf-8";
  auto root = doc.append_child(isoxml::Root);
  for (const auto& [k, v]: db.otherAttr)
    root.append_attribute(k) = v;
  if (db.versionMajor < 0 || db.versionMinor < 0) {
    auto msg = std::format("WriteFarmDb: invalid version: {}.{}",
                           db.versionMajor, db.versionMinor);
    throw std::runtime_error{msg};
  }
  root.append_attribute(isoxml::root_attr::VersionMajor) = db.versionMajor;
  root.append_attribute(isoxml::root_attr::VersionMinor) = db.versionMinor;
  root.append_attribute(isoxml::root_attr::MgmtSoftwareManufacturer) =
                                                           db.swVendor;
  root.append_attribute(isoxml::root_attr::MgmtSoftwareVersion) =
                                                           db.swVersion;
  if (db.dataTransferOrigin != -1) {
    root.append_attribute(isoxml::root_attr::DataTransferOrigin) =
                                                          db.dataTransferOrigin;
  }

  auto findCustId = [&](const Customer* cust) -> int {
    for (auto iter = db.customers.cbegin(); iter != db.customers.cend(); ++iter)
    {
      if (iter->get() == cust)
        return static_cast<int>(std::distance(db.customers.cbegin(), iter) + 1);
    }
    return 0;
  };

  auto findFarmId = [&](const Farm* farm) -> int {
    for (auto iter = db.farms.cbegin(); iter != db.farms.cend(); ++iter) {
      if (iter->get() == farm)
        return static_cast<int>(std::distance(db.farms.cbegin(), iter) + 1);
    }
    return 0;
  };

  for (int i = 0; i != std::ssize(db.customers); ++i)
    WriteCustomer(root, *db.customers[i], i+1);
  for (int i = 0; i != std::ssize(db.farms); ++i) {
    const auto& farm = *db.farms[i];
    WriteFarm(root, farm, i+1, findCustId(farm.customer));
  }
  int swathId = 0;
  for (int i = 0; i != std::ssize(db.fields); ++i) {
    const auto& field = *db.fields[i];
    WriteField(root, field, i+1,
               findCustId(field.customer), findFarmId(field.farm), swathId);
  }
  struct Value {
    int offset;
    const char* scale;
    int digits;
    const char* units;
  }; // Value
  const auto Values = std::array<Value, 9>{{
    { 0, "0.001", 2, "l"       },
    { 0, "0.001", 2, "kg"      },
    { 0, "0.01" , 2, "l/ha"    },
    { 0, "0.01" , 2, "kg/ha"   },
    { 0, "1"    , 0, "sds/m^2" },
    { 0, "1"    , 0, "mm"      },
    { 0, "1"    , 0, "N/m"     },
    { 0, "1"    , 0, "sds"     },
    { 0, "1"    , 0, "°"       }
  }}; // Values
  for (int i = 0; i != std::ssize(Values); ++i) {
    const auto& value = Values[i];
    auto vpn = root.append_child("VPN");
    vpn.append_attribute("A") = "VPN" + std::to_string(i+1);
    vpn.append_attribute("B") = value.offset;
    vpn.append_attribute("C") = value.scale;
    vpn.append_attribute("D") = value.digits;
    vpn.append_attribute("E") = value.units;
  }

  return doc;
} // CreateDoc

bool WriteZip(const std::filesystem::path& zipPath, const std::string& xml) {
  auto err = 0;
  auto* zip = zip_open(zipPath.generic_string().c_str(),
                       ZIP_CREATE | ZIP_TRUNCATE, &err);
  if (!zip)
    return false;

  auto* src = zip_source_buffer(zip, xml.data(), xml.size(), 0);
  if (!src) {
    zip_discard(zip);
    return false;
  }

  constexpr auto name = "TASKDATA/TASKDATA.XML";
  if (zip_file_add(zip, name, src, ZIP_FL_OVERWRITE) < 0) {
    zip_source_free(src);  // Only free on failure; on success zip owns src.
    zip_discard(zip);
    return false;
  }

  if (zip_close(zip) < 0) {  // Commits/writes the archive.
    zip_discard(zip);
    return false;
  }
  return true;
} // WriteZip

} // local

void FarmDb::writeXml(const std::filesystem::path& output) const {
  auto doc = CreateDoc(*this);
  auto ext = output.extension();
  if (ext == ".xml" || ext == ".XML") {
    if (doc.save_file(output.c_str(), "  "))
      return;
  }
  else if (ext == ".zip") {
    auto os = std::ostringstream{};
    if (os) {
      doc.save(os, "  ");
      if (WriteZip(output, os.str()))
        return;
    }
  }
  else {
    auto msg = std::string{"FarmDb::writeXml: invalid filename extension: "}
             + output.generic_string();
    throw std::runtime_error{msg};
  }
  auto msg =std::string{"error writing '"} + output.generic_string() + "'";
  throw std::runtime_error{"FarmDb::writeXml:" + msg};
} // writeXml

} // farm_db

// =======================================================================

namespace tjg {
template<>
struct EnumValues<isoxml::PointType>     : isoxml::PointTypes         { };
template<>
struct EnumValues<isoxml::LineStringType>: isoxml::LineStringTypes    { };
template<>
struct EnumValues<isoxml::PolygonType>   : isoxml::PolygonTypes       { };
} // tjg
