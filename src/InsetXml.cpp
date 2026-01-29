#include "FarmDb.hpp"

#include <boost/program_options.hpp>

#include <pugixml.hpp>

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
  std::string inputPath = "TASKDATA.XML";
  std::string outputPath;
  double insetFt = 0.0;
}; // Options

std::optional<Options> ParseArgs(int argc, const char* argv[]) {
  auto opts = Options{};

  auto desc = po::options_description("Options");
  desc.add_options()
    ("help,h", "Show help.")
    ("input,i",
      po::value<std::string>(&opts.inputPath)->default_value("TASKDATA.XML"),
      "Input ISO11783 file (default: TASKDATA.XML).")
    ("inset,d", po::value<double>(&opts.insetFt)->required(),
      "Inset distance in feet (required).")
    ("output,o", po::value<std::string>(&opts.outputPath)->required(),
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

  if (opts.insetFt <= 0.0) {
    std::cerr << "Error: inset distance must be > 0.\n";
    std::exit(2);
  }

  if (opts.outputPath == opts.inputPath) {
    std::cerr << "Error: output file must be different than input file.\n";
    std::exit(2);
  }

  return opts;
} // ParseArgs

int main(int argc, const char* argv[]) {
  try {
    const auto opts = ParseArgs(argc, argv);
    if (!opts)
      return 0;

    auto input  = fs::path{opts->inputPath};
    auto output = fs::path{opts->outputPath};
    auto inset  = opts->insetFt * mp_units::yard_pound::foot;

    static const char* RootName = "ISO11783_TaskData";

    auto doc = pugi::xml_document{};
    auto res = doc.load_file(input.c_str(),
                             pugi::parse_default | pugi::parse_ws_pcdata);
    if (!res) {
      std::cerr << std::format("XML parse error: {} (offset {})\n",
                               res.description(), res.offset);
      return 1;
    }

    auto root = doc.child(RootName);
    if (!root) {
      std::cerr << std::format("error: missing root <{}>\n", RootName);
      return 1;
    }

    auto db = farm_db::ReadFarmDb(root);
    std::cout << db.customers.size() << " customers\n"
              << db.farms.size()     << " farms\n"
              << db.fields.size()    << " fields\n";

#if 0
    db.swVendor  = "Terry Golubiewski";
    db.swVersion = "0.1 (alpha)";
#endif

    db.inset(inset);
    db.writeXml(output);
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

