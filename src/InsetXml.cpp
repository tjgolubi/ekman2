#include "FarmDb.hpp"

#include <boost/program_options.hpp>

#include <mp-units/systems/yard_pound.h>

#include <filesystem>
#include <string>
#include <optional>
#include <stdexcept>
#include <exception>
#include <iostream>
#include <iomanip>
#include <format>
#include <cstdlib>

namespace po = boost::program_options;
namespace fs = std::filesystem;

struct Options {
  fs::path inputPath = "TASKDATA.XML";
  fs::path outputPath;
  double insetFt = 0.0;
  std::string insetName;
}; // Options

std::optional<Options> ParseArgs(int argc, const char* argv[]) {
  auto opts = Options{};

  auto desc = po::options_description("Options");
  desc.add_options()
    ("help,h", "Show help.")
    ("input,i",
      po::value<fs::path>(&opts.inputPath)->default_value("TASKDATA.XML"),
      "Input ISO11783 file (default: TASKDATA.XML).")
    ("inset,d", po::value<double>(&opts.insetFt)->required(),
      "Inset distance in feet (required).")
    ("name,n", po::value<std::string>(&opts.insetName)->default_value("Inset"),
      "Inset name (default: \"Inset\").")
    ("output,o", po::value<fs::path>(&opts.outputPath)->required(),
      "Output file path (required).");

  auto positional = po::positional_options_description{};
  positional.add("inset",  1);
  positional.add("output", 1);

  auto vm = po::variables_map{};
  try {
    auto parsed = po::command_line_parser(argc, argv)
      .options(desc)
      .positional(positional)
      .run();

    po::store(parsed, vm);

    if (vm.count("help") != 0U) {
      std::cout
        << "Usage:\n"
        << "  InsetXml [options] <inset_feet> <output>\n\n"
        << desc
        << "\n\n"
        << "The input  file extension must be .xml.\n"
        << "The output file extension must be either .xml or .wkt.\n"
        << "\n"
        << "Examples:\n"
        << "  InsetXml 12.5 out_TASKDATA.xml\n"
        << "  InsetXml -i TASKDATA.XML 12.5 out_TASKDATA.xml\n"
        << "  InsetXml --input TASKDATA.XML 12.5 out_TASKDATA.xml\n";
      return std::nullopt;
    }

    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "Command line error: " << e.what() << "\n\n";
    std::cerr
      << "Usage:\n"
      << "  InsetXml [options] <inset_feet> <output>\n\n"
      << desc
      << "\n";
    std::exit(2);
  }

  if (opts.insetFt <= 0.5) {
    std::cerr << "Error: inset distance must be > 0.5 ft.\n";
    std::exit(2);
  }

  if (opts.outputPath == opts.inputPath) {
    std::cerr << "Error: output file must be different than input file.\n";
    std::exit(2);
  }

  auto ext = opts.inputPath.extension();
  if (ext != ".xml" && ext != ".XML" && ext != ".zip") {
    std::cerr << "Error: input file extension must be .xml or .zip\n";
    std::exit(2);
  }

  ext = opts.outputPath.extension();
  if (ext != ".xml" && ext != ".XML" && ext != ".wkt" && ext != ".WKT"
       && ext != ".zip")
  {
    std::cerr << "Error: output file extension must be .xml, .wkt, or .zip\n";
    std::exit(2);
  }

  return opts;
} // ParseArgs

int main(int argc, const char* argv[]) {
  try {
    std::cout << "InsetXml v0.0 built on " __DATE__ " " __TIME__ << '\n';

    const auto opts = ParseArgs(argc, argv);
    if (!opts)
      return 1;

    auto db = farm_db::FarmDb::ReadXml(opts->inputPath);
    std::cout << db.customers.size() << " customers\n"
              << db.farms.size()     << " farms\n"
              << db.fields.size()    << " fields\n";

#if 0
    db.swVendor  = "Terry Golubiewski";
    db.swVersion = "0.1 (alpha)";
#endif

    if (opts->insetFt != 0.0)
      db.inset(opts->insetName, opts->insetFt * mp_units::yard_pound::foot);
    const auto ext = opts->outputPath.extension();
    if (ext == ".wkt" || ext == ".WKT")
      db.writeWkt(opts->outputPath);
    else
      db.writeXml(opts->outputPath);
    return 0;
  }
  catch (std::exception& x) {
    std::cerr << "Exception: " << x.what() << '\n';
  }
  catch (...) {
    std::cerr << "Unknown exception\n";
  }

  return 1;
} // main
