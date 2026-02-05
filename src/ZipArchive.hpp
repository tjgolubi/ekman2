#include <zip.h>

#include <gsl-lite/gsl-lite.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <iostream>
#include <stdexcept>

namespace farm_db {

namespace fs  = std::filesystem;
namespace gsl = gsl_lite;

class ZipArchive {
  zip_t*   za = nullptr;
  fs::path _name;

  zip_error_t* getError() const { return zip_get_error(za); }

  static std::string ErrorStr(zip_error_t* ze)
    { return { zip_error_strerror(ze) }; }

  [[noreturn]] void die(const std::string& msg)
    { throw std::runtime_error{_name.generic_string() + ": " + msg}; }

  [[noreturn]] void die(std::string msg, zip_error_t* ze) {
    msg = _name.generic_string() + ": " + msg + ": " + ErrorStr(ze);
    throw std::runtime_error{msg};
  } // die

  [[noreturn]] void die(std::string msg, int err) {
    zip_error_t ze;
    zip_error_init_with_code(&ze, err);
    msg += ErrorStr(&ze);
    zip_error_fini(&ze);
    die(msg);
  } // die

  [[noreturn]] void dieError(const std::string& msg) { die(msg, getError()); }

public:
  class Source {
    ZipArchive*   za = nullptr;
    zip_source_t* zs = nullptr;

  private:
    friend class ZipArchive;

    Source(ZipArchive& archive, zip_source_t* zs_) : za{&archive}, zs{zs_} { }

  public:
    Source() = default;

    ~Source() noexcept { if (zs) zip_source_free(zs); }

    void release() { za = nullptr; zs = nullptr; }

  }; // Source

  class File {
    ZipArchive* za = nullptr;
    int _index = -1;
    zip_file_t* zf = nullptr;

  public:
    gsl::czstring name(zip_flags_t flags=0) const {
      if (!za || _index < 0) return nullptr;
      return zip_get_name(za->za, _index, flags);
    } // getName

    operator bool() const { return (za != nullptr && _index >= 0); }

    bool isOpen() const { return (zf != nullptr); }

    int index() const { return _index; }

    ZipArchive& archive() { return *za; }
    const ZipArchive& archive() const { return *za; }

  private:
    std::string errName() const {
      if (!za || _index < 0) return {"(null)"};
      auto cp = name();
      if (!cp) return {"(error)"};
      return { cp };
    } // errName

    [[noreturn]] void die(const std::string& msg) const
      { za->die(errName() + ": " + msg); }

    [[noreturn]] void die(const std::string& msg, int err) const
      { za->die(errName() + ": " + msg, err); }

    [[noreturn]] void dieError(const std::string& msg) const
      { za->dieError(errName() + ": " + msg); }

  public:
    fs::path fullName() const { return za->name() / name(); }

    void release() noexcept {
      za = nullptr;
      zf = nullptr;
      _index = -1;
    } // release

    bool close(int* errp) noexcept {
      if (!isOpen()) return true;
      auto err = zip_fclose(zf);
      zf = nullptr;
      _index = -1;
      if (err == ZIP_ER_OK) return true;
      *errp = err;
      return false;
    } // close

    void close() {
      auto err = ZIP_ER_OK;
      if (!close(&err)) die("cannot close", err);
    } // close

    std::size_t read(void* buf, std::size_t size) {
      auto bytesRead = zip_fread(zf, buf, size);
      if (bytesRead < 0) dieError("read error");
      return gsl::narrow<std::size_t>(bytesRead);
    } // read

    void open(zip_flags_t flags=0) {
      close();
      zf = zip_fopen_index(za->za, _index, flags);
      if (!zf) dieError("cannot open");
    } // open

    void replace(Source& src, zip_flags_t flags=0) {
      if (zip_file_replace(za->za, _index, src.zs, flags) != 0)
        dieError("cannot replace");
    } // replace

    zip_stat_t stat(zip_flags_t flags=0) const {
      zip_stat_t sb;
      zip_stat_init(&sb);
      if (zip_stat_index(za->za, _index, flags, &sb) != 0)
        dieError("cannot stat");
      return sb;
    } // stat

    std::size_t size() const { return gsl::narrow<std::size_t>(stat().size); }

    File() = default;

private:
    friend class ZipArchive;

    File(ZipArchive& archive, int index, zip_file_t* zf_=nullptr)
      : za{&archive}, _index{index}, zf{zf_}  { }

  }; // File

  std::string errorStr() const { return ErrorStr(getError()); }

  const fs::path& name() const { return _name; }

  void discard() noexcept { zip_discard(za); za = nullptr; }

  bool close(zip_error_t** ze) noexcept {
    if (!za) return true;
    if (zip_close(za) != 0) { *ze = zip_get_error(za); return false; }
    za = nullptr;
    return true;
  } // close

  void close() {
    if (!za) return;
    if (zip_close(za) != 0) dieError("cannot close");
    za = nullptr;
  } // close

  void open(const fs::path& name, int flags=0) {
    close();
    _name = name;
    auto err = ZIP_ER_OK;
    za = zip_open(_name.string().c_str(), flags, &err);
    if (!za) die("cannot open", err);
  } // open

  bool isOpen() const { return (za != nullptr); }

  ZipArchive() = default;

  ZipArchive(const std::filesystem::path name, int flags=0)
    { open(name, flags); }

  ~ZipArchive() noexcept {
    if (za) discard();
  } // dtor

  int numEntries(int flags=0) const noexcept
    { return zip_get_num_entries(za, flags); }

  File file(int index) { return File{*this, index}; }

  File file(const fs::path& name, int flags=0) {
    auto idx = zip_name_locate(za, name.string().c_str(), flags);
    if (idx < 0) return File{};
    return File{*this, gsl::narrow<int>(idx)};
  } // file

  Source source(std::string_view buf) {
    auto zs = zip_source_buffer(za, buf.data(), buf.size(), false);
    if (!zs)
      dieError("cannot add buffer (size=" + std::to_string(buf.size()) + ")");
    return Source{*this, zs};
  } // buffer

  File addFile(const fs::path& name, Source& src, zip_flags_t flags=0) {
    auto idx = zip_file_add(za, name.string().c_str(), src.zs, flags);
    if (idx < 0) dieError("cannot add `" + name.generic_string() + "'");
    return File{*this, gsl::narrow<int>(idx)};
  } // addFile

}; // ZipArchive


} // farm_db
