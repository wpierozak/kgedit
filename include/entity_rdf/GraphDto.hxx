#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
/** @brief Offline graph versioning, RDF codecs and Neo4j artifact generation.
 */
namespace entity_rdf {
/**
 * @brief Carries a node definition with a preserved record ID and optional
 * canonical IRI.
 */
struct NodeRecord {
  /// Preserved dataset record ID; never regenerated during composition.
  std::string id;
  /// Complete application labels, excluding management labels.
  std::set<std::string> labels;
  /// Complete application properties with native or tagged values.
  nlohmann::json properties = nlohmann::json::object();
  /// Canonical subject IRI; empty only for legacy DTOs.
  std::string iri;
};
/**
 * @brief Carries an identified directed relationship and its endpoint
 * references.
 */
struct RelationshipRecord {
  /// Preserved relationship record ID.
  std::string id;
  /// Resolved source record ID, or an unresolved canonical IRI.
  std::string source;
  /// Resolved target record ID, or an unresolved canonical IRI.
  std::string target;
  /// Application relationship type.
  std::string type;
  /// Complete relationship properties.
  nlohmann::json properties = nlohmann::json::object();
  /// Canonical relationship subject IRI; empty for legacy DTOs.
  std::string iri;
  /// Canonical source reference retained across partial compositions.
  std::string sourceIri;
  /// Canonical target reference retained across partial compositions.
  std::string targetIri;
};
/**
 * @brief Stores complete application records indexed by preserved record IDs.
 */
struct Graph {
  /// Node definitions keyed by preserved record ID.
  std::map<std::string, NodeRecord> nodes;
  /// Relationship definitions keyed by preserved record ID.
  std::map<std::string, RelationshipRecord> relationships;
};
} // namespace entity_rdf
