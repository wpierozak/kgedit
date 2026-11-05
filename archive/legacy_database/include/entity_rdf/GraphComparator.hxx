#pragma once
#include "entity_rdf/GraphDto.hxx"
#include "entity_rdf/Neo4jClient.hxx"
namespace entity_rdf {
/**
 * @brief Retired verifier of database contents against composed application
 * records.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class GraphComparator {
public:
  /**
   * @brief Borrow the retired database client used for comparisons.
   * @param client Client that must outlive this comparator.
   */
  explicit GraphComparator(const Neo4jClient &client) : client_(client) {}
  /**
   * @brief Compare complete application records with the retired database
   * projection.
   *
   * @throws std::runtime_error If counts, topology, labels, property types or
   * values differ.
   * @param graph Expected records, labels, endpoints, types and property
   * values.
   */
  void compare(const Graph &graph) const;

private:
  /**
   * @brief Check native Neo4j values for records containing tagged properties.
   *
   * @throws std::runtime_error If the comparison query reports a mismatch.
   * @param id Preserved record ID.
   * @param properties Expected property map.
   * @param relationship True for a relationship; false for a managed node.
   * @return True if tagged values were checked; false if no tagged values
   * require checking.
   */
  bool encodedEqual(const std::string &id, const nlohmann::json &properties,
                    bool relationship) const;
  /// Borrowed retired database client.
  const Neo4jClient &client_;
};
} // namespace entity_rdf
