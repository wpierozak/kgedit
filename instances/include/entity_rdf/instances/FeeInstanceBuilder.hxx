#pragma once
#include "entity_rdf/GraphDto.hxx"
#include "entity_rdf/instances/FeeDto.hxx"
namespace entity_rdf::instances {
/**
 * @brief Builds FEE parameter instances while preserving historical identity
 * formulas.
 */
class FeeInstanceBuilder {
public:
  /**
   * @brief Generate parameter-instance records for configured board topology.
   *
   * @throws std::runtime_error If topology is ambiguous, identities collide or
   * configuration is inconsistent.
   * @param core Existing graph whose referenced boards, detectors and parameter
   * types must exist.
   * @param config Instance IRI prefix and board declarations.
   * @return Instance-only graph retaining historical UUID and record-ID
   * formulas.
   */
  Graph build(const Graph &core, const FeeConfig &config) const;
  /**
   * @brief Compute the historical OID-namespace UUID5 of an exact identity
   * name.
   *
   * @throws std::runtime_error If the SHA-1 computation fails.
   * @param name Identity string; detector normalization is performed by the
   * caller.
   * @return Lowercase hyphenated UUID5.
   */
  static std::string uuid(const std::string &name);

private:
  /**
   * @brief Construct one FEE parameter instance using historical naming
   * formulas.
   *
   * @throws std::runtime_error If detector normalization would require
   * unverified Unicode case mappings.
   * @param config Board configuration supplying identity references.
   * @param prefix Configured instance IRI prefix.
   * @param board Named board node.
   * @param detector Named detector node whose ASCII code is uppercased for UUID
   * creation.
   * @param type Named board device-type node.
   * @param parameter Named parameter definition node.
   * @return Instance node with generated UUID, record ID and application
   * properties.
   */
  static NodeRecord instance(const BoardConfig &config,
                             const std::string &prefix, const NodeRecord &board,
                             const NodeRecord &detector, const NodeRecord &type,
                             const NodeRecord &parameter);
  /**
   * @brief Reuse an existing instance record ID when its identities agree.
   *
   * @throws std::runtime_error If an identity matches a different definition or
   * node kind.
   * @param[in,out] node Generated instance whose stored ID may be retained.
   * @param core Existing records checked for IRI and UUID matches.
   */
  static void preserveIdentity(NodeRecord &node, const Graph &core);
  /**
   * @brief Add an instance and its instance_of and parameter_of relationships.
   *
   * @throws std::runtime_error If generated instance identities or record IDs
   * collide.
   * @param[in,out] graph Generated instance graph.
   * @param core Existing graph used to retain stored relationship IDs and
   * properties.
   * @param node Generated instance to insert.
   * @param parameter Referenced parameter definition record ID.
   * @param board Referenced board record ID.
   */
  static void append(Graph &graph, const Graph &core, NodeRecord node,
                     const std::string &parameter, const std::string &board);
  /**
   * @brief Require an indexed node with an expected label and nonempty name.
   *
   * @throws std::out_of_range If the IRI is absent.
   * @throws std::runtime_error If the node lacks the label or a valid name.
   * @param byIri IRI index containing borrowed node pointers.
   * @param iri Required application IRI.
   * @param label Required application label.
   * @return Borrowed matching node.
   */
  static const NodeRecord &
  require(const std::map<std::string, const NodeRecord *> &byIri,
          const std::string &iri, const std::string &label);
  /**
   * @brief Check whether the graph contains a matching directed relationship.
   * @param graph Graph to search.
   * @param source Source record ID.
   * @param type Relationship type.
   * @param target Target record ID.
   * @return True if at least one relationship matches all three fields.
   */
  static bool link(const Graph &graph, const std::string &source,
                   const std::string &type, const std::string &target);
  /**
   * @brief Extract a local name using historical separator precedence.
   * @param iri IRI or name to inspect without percent decoding.
   * @return Suffix after the last #, otherwise /, otherwise :; the whole input
   * if none occurs.
   */
  static std::string localName(const std::string &iri);
  /**
   * @brief Generate instances for all parameters of one configured board type.
   *
   * @throws std::runtime_error If configured topology disagrees with core
   * records or has no parameters.
   * @param[in,out] graph Generated instance graph.
   * @param core Validated reference graph.
   * @param byIri Borrowed index of core nodes by application IRI.
   * @param config Board and expected detector/type references.
   * @param prefix Instance IRI prefix.
   */
  void board(Graph &graph, const Graph &core,
             const std::map<std::string, const NodeRecord *> &byIri,
             const BoardConfig &config, const std::string &prefix) const;
};
} // namespace entity_rdf::instances
