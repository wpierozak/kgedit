#pragma once
#include "entity_rdf/GraphDto.hxx"
#include "entity_rdf/instances/FeeDto.hxx"
#include <filesystem>
namespace entity_rdf::instances {
/**
 * @brief Coordinates offline FEE configuration and canonical Turtle changeset
 * generation.
 */
class FeeModule {
public:
  /**
   * @brief Build FEE instance DTOs without filesystem or database access.
   * @param core Reference graph providing the required board topology.
   * @param config Explicit instance-generation settings.
   * @return Instance-only graph from FeeInstanceBuilder.
   */
  Graph build(const Graph &core, const FeeConfig &config) const;
  /**
   * @brief Publish canonical instance Turtle and its checksummed changeset.
   *
   * @throws std::runtime_error If generation fails or output files already
   * exist or cannot be written.
   * @param core Reference graph supplying topology and stored identities.
   * @param config FEE configuration XML path.
   * @param output New output directory for configuration, Turtle and changeset
   * XML.
   * @param id Identifier written to the generated changeset.
   */
  void generate(const Graph &core, const std::filesystem::path &config,
                const std::filesystem::path &output,
                const std::string &id) const;
  /**
   * @brief Derive explicit FEE board configuration from unambiguous core
   * topology.
   *
   * @throws std::runtime_error If board topology is ambiguous or output
   * publication fails.
   * @param core Core graph to inspect.
   * @param output New configuration XML path.
   * @param prefix IRI prefix to use for generated instances.
   */
  void createConfig(const Graph &core, const std::filesystem::path &output,
                    const std::string &prefix) const;
  /**
   * @brief Expose the historical instance UUID5 calculation.
   * @param name Exact normalized instance identity name.
   * @return UUID string returned by FeeInstanceBuilder::uuid().
   */
  static std::string uuid(const std::string &name);

private:
  /**
   * @brief Write a new file without overwriting an existing destination.
   *
   * @throws std::runtime_error If the file exists or writing fails.
   * @param path Destination file path; its parent must exist.
   * @param content Exact bytes to write.
   */
  static void writeNew(const std::filesystem::path &path,
                       const std::string &content);
};
} // namespace entity_rdf::instances
