#pragma once
#include "entity_rdf/Neo4jClient.hxx"
namespace entity_rdf {
/**
 * @brief Retired manager of supported Neo4j constraints and indexes.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class SchemaManager {
public:
  /**
   * @brief Borrow the retired database client used for schema operations.
   * @param client Client that must outlive this manager.
   */
  explicit SchemaManager(const Neo4jClient &client) : client_(client) {}
  /**
   * @brief Create missing retired schema definitions and reject incompatible
   * names.
   *
   * @throws std::runtime_error If existing definitions conflict or equivalent
   * incompatible schema prevents creation.
   * @param definitions Validated supported schema definitions.
   */
  void ensure(const nlohmann::json &definitions) const;
  /**
   * @brief Compare retired application and infrastructure schema exactly.
   *
   * @throws std::runtime_error If actual supported schema differs from the
   * expected inventory.
   * @param definitions Expected application schema definitions.
   */
  void compare(const nlohmann::json &definitions) const;
  /**
   * @brief Describe the retired ledger, dataset-ID and n10s URI uniqueness
   * constraints.
   * @return Infrastructure schema definition DTOs.
   */
  static nlohmann::json infrastructure();

private:
  /**
   * @brief Read supported retired constraints and non-constraint-backed
   * indexes.
   *
   * @throws std::runtime_error If an index uses unsupported options.
   * @return Schema definitions keyed by name.
   */
  nlohmann::json read() const;
  /// Borrowed retired database client.
  const Neo4jClient &client_;
};
} // namespace entity_rdf
