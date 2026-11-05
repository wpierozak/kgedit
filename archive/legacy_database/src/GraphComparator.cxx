#include "entity_rdf/GraphComparator.hxx"
#include "entity_rdf/CypherBuilder.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <stdexcept>
namespace entity_rdf {
bool GraphComparator::encodedEqual(const std::string &id,
                                   const nlohmann::json &properties,
                                   bool relationship) const {
  bool encoded = false;
  for (const nlohmann::json &value : properties)
    if (value.is_object() ||
        (value.is_array() && !value.empty() && value.front().is_object()))
      encoded = true;
  if (!encoded)
    return false;
  nlohmann::json result = client_.execute(nlohmann::json::array(
      {CypherBuilder{}.checkProperties(id, properties, relationship)}));
  const nlohmann::json &rows = result.at(0).at("data");
  if (rows.size() != 1 || rows.at(0).at("row").at(0) != true)
    throw std::runtime_error("Typed properties differ: " + id);
  return true;
}
void GraphComparator::compare(const Graph &graph) const {
  nlohmann::json result = client_.execute(nlohmann::json::array(
      {CypherBuilder::statement(
           "MATCH (n) WHERE NOT n:Resource AND NOT n:_GraphConfig AND NOT "
           "n:_NsPrefDef AND NOT n:_VERSIONING_ RETURN "
           "n._dataset_id,labels(n),properties(n)"),
       CypherBuilder::statement(
           "MATCH (a)-[r]->(b) WHERE NOT a:Resource AND NOT b:Resource RETURN "
           "r._dataset_id,a._dataset_id,b._dataset_id,type(r),properties("
           "r)")}));
  if (result.at(0).at("data").size() != graph.nodes.size() ||
      result.at(1).at("data").size() != graph.relationships.size())
    throw std::runtime_error("Graph counts differ");
  for (const nlohmann::json &entry : result.at(0).at("data")) {
    const nlohmann::json &row = entry.at("row");
    const NodeRecord &expected = graph.nodes.at(row.at(0).get<std::string>());
    std::set<std::string> labels = row.at(1).get<std::set<std::string>>();
    labels.erase(Vocabulary::DatasetLabel);
    nlohmann::json properties = row.at(2);
    properties.erase(Vocabulary::DatasetId);
    if (encodedEqual(expected.id, expected.properties, false))
      properties = expected.properties;
    if (labels != expected.labels ||
        properties.dump() != expected.properties.dump())
      throw std::runtime_error("Node differs: " + expected.id);
  }
  for (const nlohmann::json &entry : result.at(1).at("data")) {
    const nlohmann::json &row = entry.at("row");
    const RelationshipRecord &expected =
        graph.relationships.at(row.at(0).get<std::string>());
    nlohmann::json properties = row.at(4);
    properties.erase(Vocabulary::DatasetId);
    if (encodedEqual(expected.id, expected.properties, true))
      properties = expected.properties;
    if (row.at(1) != expected.source || row.at(2) != expected.target ||
        row.at(3) != expected.type ||
        properties.dump() != expected.properties.dump())
      throw std::runtime_error("Relationship differs: " + expected.id);
  }
}
} // namespace entity_rdf
