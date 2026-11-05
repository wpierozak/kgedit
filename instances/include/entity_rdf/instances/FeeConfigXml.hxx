#pragma once
#include "entity_rdf/instances/FeeDto.hxx"
#include <filesystem>
namespace entity_rdf::instances {
/**
 * @brief Reads and writes the versioned FEE instance configuration XML format.
 */
class FeeConfigXml {
public:
  /**
   * @brief Read and validate a fee-instance-config-v2 XML configuration.
   *
   * @throws std::runtime_error If the format or XML structure is invalid.
   * @param path Local configuration XML path.
   * @return Configured IRI prefix and board topology declarations.
   */
  FeeConfig read(const std::filesystem::path &path) const;
  /**
   * @brief Serialize FEE configuration without changing board order or identity
   * spelling.
   * @param config IRI prefix and explicit board declarations.
   * @return Standalone fee-instance-config-v2 XML document.
   */
  std::string write(const FeeConfig &config) const;
  /**
   * @brief Escape configuration text for an XML attribute.
   * @param text Unescaped configuration value.
   * @return Attribute-safe XML contents.
   */
  static std::string escape(const std::string &text);
};
} // namespace entity_rdf::instances
