#include <string>
#include <vector>

#include "config.h"
#include "mjolnir/util.h"

using namespace valhalla::mjolnir;

#include "baldr/rapidjson_utils.h"
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cxxopts.hpp>
#include <iostream>

#include "filesystem.h"
#include "midgard/logging.h"
#include "midgard/util.h"

// List the build stages
void list_stages() {
  std::cout << "Build stage strings (in order)" << std::endl;
  for (int i = static_cast<int>(BuildStage::kInitialize); i <= static_cast<int>(BuildStage::kCleanup);
       ++i) {
    std::cout << "    " << to_string(static_cast<BuildStage>(i)) << std::endl;
  }
}

int main(int argc, char** argv) {
  try {
    // args
    std::string config_file_path = "";
    std::vector<std::string> input_files;

    // ref:
    // https://github.com/jarro2783/cxxopts/blob/302302b30839505703d37fb82f536c53cf9172fa/src/example.cpp
    cxxopts::Options options(
        "valhalla_build_tiles",
        "valhalla_build_tiles v" VALHALLA_VERSION
        "\n\nvalhalla_build_tiles is a program that creates the route graph\nfrom one or multiple osm.pbf extract(s)\n");

    options.add_options()("h,help",
                          "Print this help message.")("v,version",
                                                      "Print the version of this software.")(
        "c,config", "Path to the configuration file",
        cxxopts::value<std::string>())("i,inline-config", "Inline JSON config",
                                       cxxopts::value<std::string>())(
        "s,start", "Starting stage of the build pipeline",
        cxxopts::value<std::string>()->default_value(
            "initialize"))("e,end", "End stage of the build pipeline",
                           cxxopts::value<std::string>()->default_value(
                               "cleanup"))("f,files", "positional arguments",
                                           cxxopts::value<std::vector<std::string>>());

    options.parse_positional({"files"});
    options.positional_help("OSM PBF file(s)");
    auto result = options.parse(argc, argv);

    if (result.count("version")) {
      std::cout << "valhalla_build_tiles " << VALHALLA_VERSION << "\n";
      return EXIT_SUCCESS;
    }

    if (result.count("help")) {
      std::cout << options.help() << "\n";
      list_stages();
      return EXIT_SUCCESS;
    }

    // Convert stage strings to BuildStage
    BuildStage start_stage = BuildStage::kInitialize;
    BuildStage end_stage = BuildStage::kCleanup;
    if (result.count("start") > 0) {
      start_stage = string_to_buildstage(result["start"].as<std::string>());
      if (start_stage == BuildStage::kInvalid) {
        std::cerr << "Invalid start stage" << std::endl;
        list_stages();
        return EXIT_FAILURE;
      }
    }
    if (result.count("end") > 0) {
      end_stage = string_to_buildstage(result["end"].as<std::string>());
      if (end_stage == BuildStage::kInvalid) {
        std::cerr << "Invalid end stage" << std::endl;
        list_stages();
        return EXIT_FAILURE;
      }
    }
    LOG_INFO("Start stage = " + to_string(start_stage) + " End stage = " + to_string(end_stage));

    // Make sure start stage < end stage
    if (static_cast<int>(start_stage) > static_cast<int>(end_stage)) {
      std::cerr << "Starting build stage is after ending build stage in pipeline. "
                << " Please revise options!" << std::endl;
      list_stages();
      return EXIT_FAILURE;
    }

    if (result.count("files") == 0 &&
        (start_stage <= BuildStage::kParseNodes && end_stage >= BuildStage::kParseWays)) {
      std::cerr << "Input file is required\n\n" << options.help() << "\n\n";
      return EXIT_FAILURE;
    }
    input_files = result["files"].as<decltype(input_files)>();

    // Read the config file
    boost::property_tree::ptree pt;
    if (result.count("inline-config")) {
      std::stringstream ss;
      ss << result["inline-config"].as<std::string>();
      rapidjson::read_json(ss, pt);
    } else if (result.count("config") &&
               filesystem::is_regular_file(config_file_path = result["config"].as<std::string>())) {
      rapidjson::read_json(config_file_path, pt);
    } else {
      std::cerr << "Configuration is required\n\n" << options.help() << "\n\n";
      return EXIT_FAILURE;
    }

    // configure logging
    boost::optional<boost::property_tree::ptree&> logging_subtree =
        pt.get_child_optional("mjolnir.logging");
    if (logging_subtree) {
      auto logging_config = valhalla::midgard::ToMap<const boost::property_tree::ptree&,
                                                     std::unordered_map<std::string, std::string>>(
          logging_subtree.get());
      valhalla::midgard::logging::Configure(logging_config);
    }

    // Build some tiles!
    if (build_tile_set(pt, result["files"].as<std::vector<std::string>>(), start_stage, end_stage)) {
      return EXIT_SUCCESS;
    } else {
      return EXIT_FAILURE;
    }
  } catch (const cxxopts::OptionException& e) {
    std::cout << "Unable to parse command line options because: " << e.what() << std::endl;
    exit(1);
  }
}
