#pragma once
#include "entity_rdf/RdfDto.hxx"
#include <nlohmann/json.hpp>
namespace entity_rdf {
/**
 * @brief Decodes RDF literals without losing supported Neo4j value types.
 */
class PropertyCodec {
public:
  /**
   * @brief Decode a supported RDF literal without coercing application value
   * types.
   *
   * @throws std::runtime_error If the datatype, lexical value, range or tagged
   * payload is unsupported.
   * @param term Typed scalar literal; IRI terms are not scalar properties.
   * @return Native JSON scalar or validated tagged Neo4j value.
   */
  static nlohmann::json decode(const RdfTerm &term);

private:
  /**
   * @brief Validate a tagged temporal, spatial or byte value payload.
   *
   * @throws std::runtime_error If payload fields, components, ranges or
   * dimensions are invalid.
   * @param value Tagged JSON object with matching datatype and \@type.
   */
  static void validateTagged(const nlohmann::json &value);
};
} // namespace entity_rdf
