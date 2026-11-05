#pragma once
#include "entity_rdf/ChangelogDto.hxx"
#include "entity_rdf/XmlReader.hxx"
namespace entity_rdf {
/**
 * @brief Validates XML includes and checksums and composes ordered graph
 * changes.
 */
class ChangelogReader {
public:
  /**
   * @brief Read and compose all checksummed changesets in root include order.
   *
   * @throws std::runtime_error If XML, checksums, identities, schema or
   * composed references are invalid.
   * @param path Root changelog XML path.
   * @return Validated source history and complete graph.
   */
  ChangeLog read(const std::filesystem::path &path) const;

private:
  /**
   * @brief Resolve and checksum an input without escaping the changelog
   * boundary.
   *
   * @throws std::runtime_error If the declaration, containment or checksum
   * check fails.
   * @param parent Directory containing the input declaration.
   * @param entry XML declaration containing file and sha256 attributes.
   * @param boundary Canonical directory that must contain the resolved input.
   * @return Resolved input path, checksum and declaration kind.
   */
  static InputFile input(const std::filesystem::path &parent,
                         const XmlElement &entry,
                         const std::filesystem::path &boundary);
  /**
   * @brief Decode supported additive Neo4j schema definitions.
   *
   * @throws std::runtime_error If the schema format or any definition is
   * unsupported.
   * @param path Checksummed schema XML path.
   * @return Ordered JSON schema definition DTOs.
   */
  static nlohmann::json schema(const std::filesystem::path &path);
  /**
   * @brief Read one immutable changeset and its checked input files.
   *
   * Endpoint completeness is checked after composition, allowing cross-file
   * references.
   * @throws std::runtime_error If inputs change, records repeat, or
   * declarations are invalid.
   * @param file Resolved changeset declaration and expected checksum.
   * @param boundary Canonical directory containing every changeset input.
   * @return Changeset with decoded records, source triples and schema
   * additions.
   */
  ChangeSet change(const InputFile &file,
                   const std::filesystem::path &boundary) const;
};
} // namespace entity_rdf
