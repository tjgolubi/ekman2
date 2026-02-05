/// @file
/// Reads a zipped ESRI Shapefile set by extracting to a temp directory and
/// then calling FarmDb::ReadShp(temp_shp_path).

#include "FarmDb.hpp"
#include "ZipArchive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace farm_db {

namespace {

namespace fs = std::filesystem;

struct TempDir {
  fs::path path;

  explicit TempDir(const fs::path& base) : path{base} { }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  ~TempDir() noexcept {
    std::error_code ec;
    if (!path.empty())
      fs::remove_all(path, ec);
  }
}; // TempDir

[[nodiscard]] fs::path MakeUniqueTempDir() {
  auto base = fs::temp_directory_path();

  std::random_device rd;
  std::uint64_t r1 = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
  std::uint64_t r2 = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();

  std::ostringstream oss;
  oss << "farmdb_shp_" << std::hex << r1 << "_" << r2;

  auto dir = base / oss.str();
  fs::create_directories(dir);
  return dir;
} // MakeUniqueTempDir

[[noreturn]] ThrowZipError( const fs::path& path, const std::string& msg) {
  std::ostringstream oss;
  oss << path.generic_string() << ": " << msg;
  throw std::runtime_error{oss.str()};
} // ThrowZipError

void ExtractEntryToFile(ZipArchive::File file, const fs::path& outPath) {
  auto out = std::ofstream{outPath, std::ios::binary};
  if (!out) {
    ThrowZipError(file.fullName(),
                  "failed to create file: " + outPath.generic_string());
  }
  file.open();
  auto buf = std::array<char, 8 * 1024>{};
  for (;;) {
    const auto n = file.read(buf.data(), buf.size());
    if (n == 0)
      break;
    out.write(buf.data(), static_cast<std::streamsize>(n));
    if (!out) {
      ThrowZipError(file.fullName().generic_string(),
                    "write failed: " + outPath.string());
    }
  }
  file.close();
} // ExtractEntryToFile

} // local

FarmDb FarmDb::ReadZip(const std::filesystem::path& zipPath) {
  static const auto TaskDataName = fs::path{"TASKDATA/TASKDATA.XML"};

  if (zipPath.extension() != ".zip")
    ThrowZipError(zipPath, "expected a .zip file");

  auto zip = ZipArchive{zipPath, ZIP_RDONLY};

  auto taskIdx = zip.file(TaskDataName);
  if (taskIdx) {
    const auto tmpPath = MakeUniqueTempDir();
    auto tmp = TempDir{tmpPath};
    const auto outfile  = tmpPath / TaskDataName.filename();
    taskIdx.open();
    ExtractEntryToFile(taskIdx, outfile);
    return ReadXml(outfile);
  }

  auto numEntries = zip.numEntries();
  if (numEntries < 3)
    ThrowZipError(zipPath, "zip contains too few entries");
  if (numEntries > 8)
    ThrowZipError(zipPath, "zip contains too many entries");

  auto pathShp = fs::path{};
  auto shpIdx  = ZipArchive::File{};
  for (int i = 0; i != numEntries; ++i) {
    auto name = zip.file(i).name();
    if (!name)
      continue;
    pathShp = fs::path{name};
    if (pathShp.extension() == ".shp") {
      shpIdx = zip.file(i);;
      break;
    }
  }

  if (!shpIdx)
    ThrowZipError(zipPath, "cannot find .shp file");

  auto pathShx = fs::path{pathShp}.replace_extension(".shx");
  auto pathDbf = fs::path{pathShp}.replace_extension(".dbf");
  auto pathPrj = fs::path{pathShp}.replace_extension(".prj");
  auto pathCpg = fs::path{pathShp}.replace_extension(".cpg");

  auto shxIdx  = zip.file(pathShx);
  auto dbfIdx  = zip.file(pathDbf);
  auto prjIdx  = zip.file(pathPrj);
  auto cpgIdx  = zip.file(pathCpg);

  if (!shxIdx || !dbfIdx)
    ThrowZipError(zipPath, "cannot find .shx and .dbf files");

  const auto tmpPath = MakeUniqueTempDir();
  auto tmp = TempDir{tmpPath};

  const auto stem   = pathShp.stem();
  const auto base   = tmpPath / stem;
  const auto outShp = fs::path{base}.replace_extension(".shp");
  const auto outShx = fs::path{base}.replace_extension(".shx");
  const auto outDbf = fs::path{base}.replace_extension(".dbf");

  ExtractEntryToFile(shpIdx, outShp);
  ExtractEntryToFile(shxIdx, outShx);
  ExtractEntryToFile(dbfIdx, outDbf);
  ExtractEntryToFile(dbfIdx, outDbf);
  ExtractEntryToFile(dbfIdx, outDbf);

  if (prjIdx) {
    const auto outPrj = fs::path{base}.replace_extension(".prj");
    ExtractEntryToFile(prjIdx, outPrj);
  }

  if (cpgIdx) {
    const auto outCpg = fs::path{base}.replace_extension(".cpg");
    ExtractEntryToFile(cpgIdx, outCpg);
  }

  return FarmDb::ReadShp(outShp);
} // FarmDb::ReadZip

} // farm_db
