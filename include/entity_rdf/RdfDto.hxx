#pragma once
#include <string>
#include <vector>

namespace entity_rdf {
/**
 * @brief Represents an IRI object or a typed literal in the supported RDF
 * subset.
 */
struct RdfTerm {
  /// Exact IRI spelling or literal lexical form.
  std::string value;
  /// Literal datatype IRI; empty for IRI objects.
  std::string datatype;
  /// True for an IRI object; false for a typed literal.
  bool iri;
};
/**
 * @brief Carries one RDF statement with explicit IRI subject and predicate.
 */
struct RdfTriple {
  /// Absolute subject IRI.
  std::string subject;
  /// Absolute predicate IRI.
  std::string predicate;
  /// IRI or typed literal object.
  RdfTerm object;
};
/// Parsed source triples in parser order; duplicate triples may occur.
using RdfDocument = std::vector<RdfTriple>;
} // namespace entity_rdf
