#pragma once
#include <string>
#include <vector>
/** @brief Offline FEE configuration and parameter-instance generation. */
namespace entity_rdf::instances {
/**
 * @brief Identifies a board and its expected detector and device-type topology.
 */
struct BoardConfig {
  /// Application IRI of the configured board.
  std::string iri;
  /// Expected owning detector IRI.
  std::string detectorIri;
  /// Expected board device-type IRI.
  std::string typeIri;
};
/**
 * @brief Configures the IRI prefix and boards used for FEE instance generation.
 */
struct FeeConfig {
  /// Prefix used when minting parameter-instance IRIs.
  std::string iriPrefix;
  /// Board topology declarations in configuration order.
  std::vector<BoardConfig> boards;
};
} // namespace entity_rdf::instances
