#pragma once
#include "entity_rdf/ChangelogDto.hxx"
namespace entity_rdf {
/**
 * @brief Builds deterministic offline Turtle, Cypher and provenance bundles.
 */
class ArtifactBuilder {
public:
  /**
   * @brief Validate and publish a complete artifact bundle in a new directory.
   *
   * Generation finishes before publication. A sibling .pending directory stages
   * the bundle.
   * @throws std::runtime_error If identities, projection values or output paths
   * are unsupported, or publication fails.
   * @param log Validated changelog with canonical record IRIs.
   * @param output New output directory; its parent must already exist.
   * @param turtleUri Absolute TTL location visible to the external Neo4j
   * importer.
   */
  void build(const ChangeLog &log, const std::filesystem::path &output,
             const std::string &turtleUri) const;
  /**
   * @brief Generate a cypher-shell script for an empty-database snapshot
   * import.
   *
   * Does not connect to Neo4j. Schema and RDF import precede the projection
   * transaction.
   * @throws std::runtime_error If canonical identities or projection values are
   * unsupported.
   * @param log Validated complete graph and ordered schema additions.
   * @param turtleUri Absolute Neo4j-visible location of the matching Turtle
   * artifact.
   * @param triples Expected parsed and loaded triple count in that artifact.
   * @return Script containing import guards, schema setup and transactional
   * projection.
   */
  std::string cypher(const ChangeLog &log, const std::string &turtleUri,
                     std::size_t triples) const;

private:
  /**
   * @brief Encode a JSON value as a Cypher literal, including nested maps and
   * lists.
   *
   * @throws std::runtime_error If a floating-point value is not finite.
   * @param value Value to embed in the generated script.
   * @return Escaped Cypher expression.
   */
  static std::string literal(const nlohmann::json &value);
  /**
   * @brief Render a generated query template with a batch of literal input
   * rows.
   * @param query Generated parameterized query; dollar signs in quoted
   * identifiers are preserved.
   * @param rows Parameter maps including the __iri key used to match imported
   * resources.
   * @return One semicolon-terminated UNWIND projection statement.
   */
  static std::string render(const std::string &query,
                            const nlohmann::json &rows);
  /**
   * @brief Group equivalent projection templates and render bounded input
   * batches.
   * @param statements Statement envelopes produced by CypherBuilder.
   * @param iris Canonical resource IRIs in the same order as the statements.
   * @return Deterministically ordered Cypher projection statements.
   */
  static std::string projections(const nlohmann::json &statements,
                                 const std::vector<std::string> &iris);
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
} // namespace entity_rdf
