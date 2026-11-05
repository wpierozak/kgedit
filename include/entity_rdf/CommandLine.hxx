#pragma once
#include <filesystem>
#include <iosfwd>
#include <string>
namespace entity_rdf {
/**
 * @brief Parses CLI11 subcommands and dispatches offline graph operations.
 */
class CommandLine {
public:
  /**
   * @brief Bind the output streams used for command results and parser
   * messages.
   * @param output Standard output stream; must outlive this dispatcher.
   * @param error Standard error stream; must outlive this dispatcher.
   */
  CommandLine(std::ostream &output, std::ostream &error);

  /**
   * @brief Parse exactly one subcommand before opening inputs or writing
   * outputs.
   * @param argc Argument count including the executable name.
   * @param argv Command and argument strings.
   * @return Zero on success or help; CLI11's nonzero exit code on parsing
   * errors.
   * @throws std::exception If a parsed command fails during input validation or
   * execution.
   */
  int run(int argc, const char *const *argv) const;

private:
  /**
   * @brief Write a new CLI output file without replacing an existing path.
   * @param path Destination file path whose parent must already exist.
   * @param content Exact output bytes.
   * @throws std::runtime_error If the output exists or cannot be written.
   */
  static void writeNew(const std::filesystem::path &path,
                       const std::string &content);

  /// Borrowed stream receiving command results and generated help.
  std::ostream &output_;
  /// Borrowed stream receiving CLI11 parsing diagnostics.
  std::ostream &error_;
};
} // namespace entity_rdf
