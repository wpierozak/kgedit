/** @file main.cxx
 * @brief Offline kgedit command-line entry point.
 */
#include "entity_rdf/CommandLine.hxx"
#include <exception>
#include <iostream>
/**
 * @brief Run the offline CLI and report command execution failures.
 * @param argc Argument count including the executable name.
 * @param argv Command and argument strings.
 * @return Zero on success or help, CLI11's parsing status, or one on execution
 * failure.
 */
int main(int argc, char **argv) {
  try {
    return entity_rdf::CommandLine{std::cout, std::cerr}.run(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << "kgedit: " << error.what() << '\n';
    return 1;
  }
}
