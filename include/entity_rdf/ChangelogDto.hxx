#pragma once
#include "entity_rdf/GraphDto.hxx"
#include "entity_rdf/RdfDto.hxx"
#include <filesystem>
#include <vector>
namespace entity_rdf {
/**
 * @brief Describes one checksummed input contained within a changelog
 * directory.
 */
struct InputFile {
  /// Resolved local input path.
  std::filesystem::path path;
  /// Expected lowercase SHA-256 digest.
  std::string checksum;
  /// Input declaration kind: rdf, schema, source or include.
  std::string kind;
};
/**
 * @brief Carries one ordered graph change, its source triples and schema
 * additions.
 */
struct ChangeSet {
  /// Immutable changeset identifier.
  std::string id;
  /// SHA-256 digest of the changeset XML.
  std::string checksum;
  /// Checked input files in declaration order.
  std::vector<InputFile> files;
  /// Complete replacement records defined in this changeset.
  Graph graph;
  /// Parsed source triples retained for legacy compatibility.
  RdfDocument document;
  /// Ordered additive schema definition DTOs.
  nlohmann::json schema = nlohmann::json::array();
};
/**
 * @brief Contains validated changesets and their fully composed application
 * graph.
 */
struct ChangeLog {
  /// Validated changesets in include order.
  std::vector<ChangeSet> changes;
  /// Complete graph after all ordered replacements.
  Graph composed;
};
} // namespace entity_rdf
