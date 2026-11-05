#pragma once
#include "entity_rdf/GraphDto.hxx"
#include "entity_rdf/RdfDto.hxx"
#include <map>
#include <set>
namespace entity_rdf {
/**
 * @brief Maps the supported RDF record vocabulary to application graph DTOs.
 */
class GraphMapper {
public:
  /**
   * @brief Decode supported canonical or legacy RDF records into graph DTOs.
   *
   * References may remain unresolved until composition.
   * @throws std::runtime_error If vocabulary, literal values, record ownership
   * or identities are invalid.
   * @param document Parsed RDF triples; identical triples have set semantics.
   * @return Partial graph with locally resolvable endpoints converted to record
   * IDs.
   */
  Graph decode(const RdfDocument &document) const;
  /**
   * @brief Replace complete records while preserving canonical identity
   * bindings.
   *
   * Allows unresolved references until validate() is called at a composition
   * boundary.
   * @throws std::runtime_error If record IDs or canonical identities conflict.
   * @param[in,out] graph Accumulated graph; unchanged if validation of the
   * candidate fails.
   * @param change Records to add or replace, with preserved IDs and canonical
   * IRIs.
   */
  static void overlay(Graph &graph, const Graph &change);
  /**
   * @brief Assign missing canonical IRIs once during record authoring.
   *
   * Reuses existing application IRIs and preserves record IDs and UUID
   * properties.
   * @throws std::runtime_error If identities conflict or an endpoint cannot be
   * identified.
   * @param[in,out] graph Authored records; application iri properties are
   * synchronized.
   * @param context Canonical external nodes used to resolve referenced endpoint
   * IDs.
   */
  static void canonicalize(Graph &graph, const Graph &context = {});
  /**
   * @brief Index canonical identities and resolve available endpoint IRIs to
   * IDs.
   *
   * References outside this partial graph remain as IRIs for later composition.
   * @throws std::runtime_error If IDs, canonical IRIs or application identity
   * properties conflict.
   * @param[in,out] graph Graph whose known endpoints are resolved in place.
   */
  static void resolve(Graph &graph);
  /**
   * @brief Require consistent identities and complete endpoints in a composed
   * graph.
   *
   * @throws std::runtime_error If identities conflict or any endpoint is
   * missing or inconsistent.
   * @param graph Complete graph to validate without modifying it.
   */
  static void validate(const Graph &graph);

private:
  /// RDF triples grouped by their exact subject IRI.
  using Subjects = std::map<std::string, std::vector<RdfTriple>>;
  /**
   * @brief Decode a node and validate its identity, labels and properties.
   * @param subject Record subject IRI.
   * @param triples Triples describing this record.
   * @param subjects All subjects in the current RDF document.
   * @param[in,out] consumed Subjects claimed by records and their owned array
   * values.
   * @return Node record retaining the preserved ID and any canonical subject
   * IRI.
   */
  NodeRecord readNode(const std::string &subject,
                      const std::vector<RdfTriple> &triples,
                      const Subjects &subjects,
                      std::set<std::string> &consumed) const;
  /**
   * @brief Decode an identified relationship and its directed endpoint
   * references.
   * @param subject Record subject IRI.
   * @param triples Triples describing this record.
   * @param subjects All subjects in the current RDF document.
   * @param[in,out] consumed Subjects claimed by records and their owned array
   * values.
   * @return Relationship record with unresolved canonical or decoded legacy
   * endpoints.
   */
  RelationshipRecord readRelationship(const std::string &subject,
                                      const std::vector<RdfTriple> &triples,
                                      const Subjects &subjects,
                                      std::set<std::string> &consumed) const;
  /**
   * @brief Decode a scalar property or an exclusively owned ordered array.
   *
   * @throws std::runtime_error If the array is shared, malformed, heterogeneous
   * or unsupported.
   * @param term Literal or owned-array resource reference.
   * @param subjects RDF subjects available for array lookup.
   * @param[in,out] consumed Claimed resource subjects used to enforce exclusive
   * ownership.
   * @return Typed JSON property value.
   */
  nlohmann::json value(const RdfTerm &term, const Subjects &subjects,
                       std::set<std::string> &consumed) const;
  /**
   * @brief Decode the canonical percent-encoded suffix of a legacy record IRI.
   *
   * @throws std::runtime_error If the namespace, suffix or percent encoding is
   * invalid.
   * @param iri Legacy transport subject or endpoint IRI.
   * @param prefix Required node or relationship transport namespace.
   * @return Nonempty preserved record ID.
   */
  static std::string id(const std::string &iri, const char *prefix);
  /**
   * @brief Require exactly one occurrence of a structural predicate.
   *
   * @throws std::runtime_error If the field is missing or repeated.
   * @param triples Triples describing a single subject.
   * @param predicate Required predicate IRI.
   * @return The matching object term.
   */
  static RdfTerm single(const std::vector<RdfTriple> &triples,
                        const char *predicate);
  /**
   * @brief Decode one typed RDF literal as an application scalar.
   * @param term Typed literal term accepted by PropertyCodec.
   * @return Type-preserving JSON value or tagged Neo4j value.
   */
  static nlohmann::json scalar(const RdfTerm &term);
  /**
   * @brief Decode application property predicates and their owned values.
   *
   * @throws std::runtime_error If property names are empty, reserved,
   * duplicated or incorrectly encoded.
   * @param triples Triples describing one application record.
   * @param subjects All RDF subjects used to resolve owned arrays.
   * @param[in,out] consumed Subjects claimed by decoded array properties.
   * @return Complete application property map.
   */
  nlohmann::json properties(const std::vector<RdfTriple> &triples,
                            const Subjects &subjects,
                            std::set<std::string> &consumed) const;
  /**
   * @brief Reject structural predicates outside the supported record
   * vocabulary.
   *
   * @throws std::runtime_error If an unsupported predicate occurs.
   * @param triples Triples describing the record.
   * @param allowed Permitted structural predicates; application property
   * predicates are also allowed.
   */
  static void checkPredicates(const std::vector<RdfTriple> &triples,
                              const std::set<std::string> &allowed);
};
} // namespace entity_rdf
