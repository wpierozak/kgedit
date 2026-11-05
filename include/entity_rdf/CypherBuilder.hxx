#pragma once
#include "entity_rdf/GraphDto.hxx"
namespace entity_rdf {
/**
 * @brief Generates parameterized application projection and schema statements.
 */
class CypherBuilder {
public:
  /**
   * @brief Wrap a parameterized query in a Neo4j statement DTO.
   * @param query Cypher query text.
   * @param parameters Named query parameters.
   * @return JSON object containing statement and parameters fields.
   */
  static nlohmann::json
  statement(const std::string &query,
            const nlohmann::json &parameters = nlohmann::json::object());
  /**
   * @brief Generate a query comparing stored property names, types and values.
   * @param id Preserved record ID to match.
   * @param properties Complete expected application properties.
   * @param relationship True to match a relationship; false to match a managed
   * node.
   * @return Parameterized statement returning the property comparison result.
   */
  nlohmann::json checkProperties(const std::string &id,
                                 const nlohmann::json &properties,
                                 bool relationship) const;
  /**
   * @brief Generate node upserts with complete property replacement.
   *
   * Preserves the management label; application label removal is a separate
   * concern.
   * @throws std::runtime_error If a property cannot be expressed using
   * supported Cypher constructors.
   * @param graph Application nodes indexed by preserved record ID.
   * @return Parameterized statement DTOs in record-ID order.
   */
  nlohmann::json nodes(const Graph &graph) const;
  /**
   * @brief Generate directed relationship creation statements.
   *
   * Creates distinct relationships for parallel records; does not remove
   * existing relationships.
   * @throws std::runtime_error If a property has no supported Cypher
   * representation.
   * @param graph Relationships whose endpoints are resolved to preserved node
   * IDs.
   * @return Parameterized statement DTOs in record-ID order, each returning a
   * creation count.
   */
  nlohmann::json relationships(const Graph &graph) const;
  /**
   * @brief Generate an additive constraint or index creation statement.
   * @param item Previously validated supported schema definition DTO.
   * @return Identifier-escaped CREATE statement with IF NOT EXISTS.
   */
  static std::string schema(const nlohmann::json &item);

private:
  /**
   * @brief Generate a native property expression referencing a parameter path.
   *
   * @throws std::runtime_error If the tagged value cannot be projected,
   * including native byte arrays.
   * @param value Scalar, array or tagged Neo4j value describing the expected
   * type.
   * @param path Cypher expression pointing to the corresponding parameter
   * value.
   * @return Expression preserving supported temporal, spatial and scalar types.
   */
  static std::string expression(const nlohmann::json &value,
                                const std::string &path);
  /**
   * @brief Generate a complete Cypher property map from parameter references.
   * @param properties Application property names and typed values.
   * @return Map expression with escaped keys and native value constructors.
   */
  static std::string propertyMap(const nlohmann::json &properties);
};
} // namespace entity_rdf
