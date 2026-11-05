#include "entity_rdf/SchemaManager.hxx"
#include "entity_rdf/CypherBuilder.hxx"
#include <stdexcept>
namespace entity_rdf {
nlohmann::json SchemaManager::infrastructure() {
  nlohmann::json definitions = nlohmann::json::array();
  for (const std::vector<std::string> &fields :
       std::vector<std::vector<std::string>>{
           {"entity_rdf_ledger", "_VERSIONING_", "name"},
           {"entity_rdf_id", "_DATASET_", "_dataset_id"},
           {"n10s_unique_uri", "Resource", "uri"}})
    definitions.push_back(
        {{"name", fields[0]},
         {"type", "UNIQUENESS"},
         {"entityType", "NODE"},
         {"labelsOrTypes", nlohmann::json::array({fields[1]})},
         {"properties", nlohmann::json::array({fields[2]})}});
  return definitions;
}
nlohmann::json SchemaManager::read() const {
  nlohmann::json result = client_.execute(nlohmann::json::array(
      {CypherBuilder::statement(
           "SHOW CONSTRAINTS YIELD "
           "name,type,entityType,labelsOrTypes,properties RETURN "
           "name,type,entityType,labelsOrTypes,properties"),
       CypherBuilder::statement(
           "SHOW INDEXES YIELD "
           "name,type,entityType,labelsOrTypes,properties,owningConstraint,"
           "options WHERE owningConstraint IS NULL RETURN "
           "name,type,entityType,labelsOrTypes,properties,options")}));
  nlohmann::json definitions = nlohmann::json::object();
  for (const nlohmann::json &table : result)
    for (const nlohmann::json &entry : table.at("data")) {
      const nlohmann::json &row = entry.at("row");
      nlohmann::json labels = row.at(3).is_null() ? nlohmann::json::array()
                                                  : row.at(3),
                     properties = row.at(4).is_null() ? nlohmann::json::array()
                                                      : row.at(4);
      nlohmann::json item = {{"name", row.at(0)},
                             {"type", row.at(1)},
                             {"entityType", row.at(2)},
                             {"labelsOrTypes", labels},
                             {"properties", properties}};
      if (row.size() > 5) {
        nlohmann::json expected = {{"indexConfig", nlohmann::json::object()},
                                   {"indexProvider", row.at(1) == "LOOKUP"
                                                         ? "token-lookup-1.0"
                                                         : "range-1.0"}};
        if (row.at(5) != expected)
          throw std::runtime_error("Unsupported index options: " +
                                   row.at(0).get<std::string>());
      }
      definitions[row.at(0).get<std::string>()] = std::move(item);
    }
  return definitions;
}
void SchemaManager::ensure(const nlohmann::json &definitions) const {
  nlohmann::json existing = read();
  for (const nlohmann::json &item : definitions) {
    std::string name = item.at("name");
    if (existing.contains(name) && existing.at(name) != item)
      throw std::runtime_error("Conflicting schema definition: " + name);
    if (!existing.contains(name))
      client_.execute(nlohmann::json::array(
          {CypherBuilder::statement(CypherBuilder::schema(item))}));
  }
  existing = read();
  for (const nlohmann::json &item : definitions)
    if (!existing.contains(item.at("name")) ||
        existing.at(item.at("name").get<std::string>()) != item)
      throw std::runtime_error("Schema creation was skipped due to "
                               "incompatible existing definition");
}
void SchemaManager::compare(const nlohmann::json &definitions) const {
  nlohmann::json expected = nlohmann::json::object();
  for (const nlohmann::json &item : infrastructure())
    expected[item.at("name").get<std::string>()] = item;
  for (const nlohmann::json &item : definitions)
    expected[item.at("name").get<std::string>()] = item;
  if (read() != expected)
    throw std::runtime_error("Schema differs from composed changelog and "
                             "infrastructure definitions");
}
} // namespace entity_rdf
