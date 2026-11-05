#pragma once
#include "entity_rdf/GraphDto.hxx"
namespace entity_rdf {
/**
 * @brief Serializes application records deterministically as lossless Turtle.
 */
class TurtleWriter {
public:
  /**
   * @brief Serialize graph records in deterministic preserved-ID order.
   *
   * Canonical records carry g:recordId; ID-only DTOs retain legacy transport
   * subjects.
   * @param graph Records to serialize; endpoint IRI fields may reference
   * external records.
   * @return Standalone Turtle with standard prefixes and explicit relationship
   * resources.
   */
  std::string write(const Graph &graph) const;

private:
  /**
   * @brief Validate and render an absolute IRI using supported prefixes when
   * possible.
   * @param value Absolute IRI to render.
   * @return A prefixed name or an angle-bracketed IRI.
   */
  static std::string iri(const std::string &value);
  /**
   * @brief Encode one supported application scalar as a typed Turtle literal.
   *
   * @throws std::runtime_error If the scalar representation is unsupported.
   * @param value Native JSON scalar or supported tagged Neo4j value.
   * @return Escaped lexical value with an explicit datatype.
   */
  static std::string literal(const nlohmann::json &value);
  /**
   * @brief Append scalar properties and exclusively owned array resources.
   * @param[in,out] output Turtle text receiving emitted triples.
   * @param subject Owning application record IRI.
   * @param properties Complete application property map.
   */
  static void properties(std::string &output, const std::string &subject,
                         const nlohmann::json &properties);
  /**
   * @brief Append one Turtle triple, abbreviating rdf:type as a.
   * @param[in,out] output Turtle text receiving the statement.
   * @param subject Absolute subject IRI.
   * @param predicate Absolute predicate IRI.
   * @param object Already encoded Turtle object expression.
   */
  static void triple(std::string &output, const std::string &subject,
                     const std::string &predicate, const std::string &object);
};
} // namespace entity_rdf
