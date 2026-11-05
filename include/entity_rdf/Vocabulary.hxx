#pragma once
#include <string>
#include <string_view>
namespace entity_rdf {
/**
 * @brief Defines RDF vocabulary, canonical identity namespaces and reserved
 * graph names.
 */
struct Vocabulary {
  /// Standard rdf:type predicate.
  static constexpr const char *Type =
      "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
  /// Standard XML Schema datatype namespace.
  static constexpr const char *Xsd = "http://www.w3.org/2001/XMLSchema#";
  /// Application node record class.
  static constexpr const char *Node = "urn:dcs:graph:Node";
  /// Identified relationship record class.
  static constexpr const char *Relationship = "urn:dcs:graph:Relationship";
  /// Owned array resource class.
  static constexpr const char *Array = "urn:dcs:graph:Array";
  /// Application label predicate.
  static constexpr const char *Label = "urn:dcs:graph:label";
  /// Directed source endpoint predicate.
  static constexpr const char *Source = "urn:dcs:graph:source";
  /// Directed target endpoint predicate.
  static constexpr const char *Target = "urn:dcs:graph:target";
  /// Application relationship-type predicate.
  static constexpr const char *RelationType = "urn:dcs:graph:relationshipType";
  /// Owned array length predicate.
  static constexpr const char *Length = "urn:dcs:graph:length";
  /// Preserved record-ID predicate.
  static constexpr const char *RecordId = "urn:dcs:graph:recordId";
  /// Namespace for newly minted missing node IRIs.
  static constexpr const char *CanonicalNodePrefix = "urn:alice:dcs:node:";
  /// Namespace for newly minted missing relationship IRIs.
  static constexpr const char *CanonicalRelationshipPrefix =
      "urn:alice:dcs:relationship:";
  /// Legacy node transport namespace.
  static constexpr const char *NodePrefix = "urn:dcs:node:";
  /// Legacy relationship transport namespace.
  static constexpr const char *RelationshipPrefix = "urn:dcs:relationship:";
  /// Reserved namespace for exclusively owned compound values.
  static constexpr const char *ValuePrefix = "urn:dcs:value:";
  /// Namespace for percent-encoded application property names.
  static constexpr const char *PropertyPrefix = "urn:dcs:property:";
  /// Namespace for ordered array item predicates.
  static constexpr const char *ItemPrefix = "urn:dcs:item:";
  /// Namespace for tagged native Neo4j datatypes.
  static constexpr const char *Neo4jPrefix = "urn:dcs:neo4j:";
  /// Reserved property retaining the preserved dataset ID.
  static constexpr const char *DatasetId = "_dataset_id";
  /// Reserved label identifying projected application nodes.
  static constexpr const char *DatasetLabel = "_DATASET_";
  /// Retired ledger label, still reserved against application use.
  static constexpr const char *LedgerLabel = "_VERSIONING_";
};
/**
 * @brief Provides canonical IRI encoding, hashing, file reading and identifier
 * escaping.
 */
class Encoding {
public:
  /**
   * @brief Validate an absolute IRI using rdf4cpp.
   *
   * @throws rdf4cpp::ParsingError If RDF IRI validation fails.
   * @throws std::runtime_error If the IRI has no scheme separator.
   * @param value IRI spelling to validate without normalizing it.
   */
  static void validateIri(std::string_view value);
  /**
   * @brief Percent-encode UTF-8 bytes using canonical uppercase escapes.
   * @param value Raw identifier or property-name bytes.
   * @return ASCII encoding leaving only letters, digits and -._~ unescaped.
   */
  static std::string percent(std::string_view value);
  /**
   * @brief Decode only canonically spelled percent-encoded identifiers.
   *
   * @throws std::runtime_error If escapes are invalid, lowercase or otherwise
   * noncanonical.
   * @param value Encoded identifier or property-name bytes.
   * @return Original decoded bytes.
   */
  static std::string unpercent(std::string_view value);
  /**
   * @brief Compute a lowercase hexadecimal SHA-256 digest.
   *
   * @throws std::runtime_error If OpenSSL digest computation fails.
   * @param value Exact bytes to hash.
   * @return 64-character checksum.
   */
  static std::string sha256(std::string_view value);
  /**
   * @brief Read a local file as unmodified binary bytes.
   *
   * @throws std::runtime_error If the file cannot be opened.
   * @param path Input filesystem path.
   * @return Complete file contents.
   */
  static std::string readFile(const std::string &path);
  /**
   * @brief Escape a Cypher identifier by doubling embedded backticks.
   * @param value Label, relationship type, schema name or property key.
   * @return Backtick-delimited Cypher identifier.
   */
  static std::string quote(std::string_view value);
};
} // namespace entity_rdf
