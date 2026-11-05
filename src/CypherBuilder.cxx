#include "entity_rdf/CypherBuilder.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <stdexcept>
namespace entity_rdf {
nlohmann::json CypherBuilder::statement(const std::string &query,
                                        const nlohmann::json &parameters) {
  return {{"statement", query}, {"parameters", parameters}};
}
std::string CypherBuilder::expression(const nlohmann::json &value,
                                      const std::string &path) {
  if (value.is_array()) {
    std::string result = "[";
    for (std::size_t i = 0; i < value.size(); ++i) {
      if (i)
        result += ",";
      result += expression(value[i], path + "[" + std::to_string(i) + "]");
    }
    return result + "]";
  }
  if (!value.is_object())
    return path;
  std::string kind = value.at("@type");
  std::string data = path + ".value";
  if (kind == "Date")
    return "date(" + data + ")";
  if (kind == "Time") {
    std::string lexical = value.at("value");
    return (lexical.find('+') != std::string::npos || lexical.ends_with('Z') ||
                    lexical.find('-', 1) != std::string::npos
                ? "time("
                : "localtime(") +
           data + ")";
  }
  if (kind == "DateTime") {
    if (value.contains("timezone"))
      return "datetime({datetime:datetime(" + data + "),timezone:" + path +
             ".timezone})";
    std::string text = value.at("value");
    return (text.ends_with('Z') || text.find('+', 10) != std::string::npos ||
                    text.find('-', 10) != std::string::npos
                ? "datetime("
                : "localdatetime(") +
           data + ")";
  }
  if (kind == "Duration")
    return "duration({months:" + data + "[0],days:" + data +
           "[1],seconds:" + data + "[2],nanoseconds:" + data + "[3]})";
  if (kind == "point") {
    std::string result = "point({srid:" + path + ".srid,x:" + path +
                         ".coordinates[0],y:" + path + ".coordinates[1]";
    if (value.at("coordinates").size() == 3)
      result += ",z:" + path + ".coordinates[2]";
    return result + "})";
  }
  if (kind == "float") {
    std::string lexical = value.at("value");
    if (lexical == "nan")
      return "(0.0/0.0)";
    if (lexical == "inf")
      return "(1.0/0.0)";
    if (lexical == "-inf")
      return "(-1.0/0.0)";
  }
  // HTTP JSON has no native byte-array parameter. Refuse rather than silently
  // store an integer array.
  throw std::runtime_error(
      "Neo4j HTTP projection does not support property kind: " + kind);
}
std::string CypherBuilder::propertyMap(const nlohmann::json &properties) {
  std::string result = "{";
  bool first = true;
  for (const auto &[key, value] : properties.items()) {
    if (!first)
      result += ",";
    first = false;
    result += Encoding::quote(key) + ":" +
              expression(value, "$properties." + Encoding::quote(key));
  }
  return result + "}";
}
nlohmann::json CypherBuilder::checkProperties(const std::string &id,
                                              const nlohmann::json &properties,
                                              bool relationship) const {
  std::string query = relationship ? "MATCH ()-[n]->() WHERE n._dataset_id=$id "
                                   : "MATCH (n:_DATASET_ {_dataset_id:$id}) ";
  query += "WITH n," + propertyMap(properties) + " AS expected RETURN ";
  std::string condition = "size(keys(n))=size(keys(expected))+1";
  for (const auto &[key, value] : properties.items()) {
    std::string actual = "n." + Encoding::quote(key),
                expected = "expected." + Encoding::quote(key);
    std::string equality =
        value.is_array()
            ? "size(" + actual + ")=size(" + expected +
                  ") AND all(i IN range(0,size(" + expected +
                  ")-1) WHERE CASE WHEN valueType(" + expected +
                  "[i])='FLOAT NOT NULL' THEN toString(" + actual +
                  "[i])=toString(" + expected + "[i]) ELSE " + actual +
                  "[i]=" + expected + "[i] END)"
            : "CASE WHEN valueType(" + expected +
                  ")='FLOAT NOT NULL' THEN toString(" + actual + ")=toString(" +
                  expected + ") ELSE " + actual + "=" + expected + " END";
    condition += " AND valueType(" + actual + ")=valueType(" + expected +
                 ") AND (" + equality + ")";
  }
  return statement(query + condition, {{"id", id}, {"properties", properties}});
}
nlohmann::json CypherBuilder::nodes(const Graph &graph) const {
  nlohmann::json result = nlohmann::json::array();
  for (const auto &[id, node] : graph.nodes) {
    // The reserved management label survives complete application-label
    // replacement.
    std::string query = "MERGE (n:_DATASET_ {_dataset_id:$id}) SET n=" +
                        propertyMap(node.properties) + ", n._dataset_id=$id";
    for (const std::string &label : node.labels)
      query += " SET n:" + Encoding::quote(label);
    result.push_back(
        statement(query, {{"id", id}, {"properties", node.properties}}));
  }
  return result;
}
nlohmann::json CypherBuilder::relationships(const Graph &graph) const {
  nlohmann::json result = nlohmann::json::array();
  for (const auto &[id, edge] : graph.relationships) {
    result.push_back(statement(
        "MATCH (a:_DATASET_ {_dataset_id:$source}),(b:_DATASET_ "
        "{_dataset_id:$target}) CREATE (a)-[r:" +
            Encoding::quote(edge.type) + "]->(b) SET r=" +
            propertyMap(edge.properties) + ",r._dataset_id=$id RETURN count(r)",
        {{"id", id},
         {"source", edge.source},
         {"target", edge.target},
         {"properties", edge.properties}}));
  }
  return result;
}
std::string CypherBuilder::schema(const nlohmann::json &item) {
  std::string name = Encoding::quote(item.at("name").get<std::string>());
  std::string kind = item.at("type"), entity = item.at("entityType");
  if (kind == "LOOKUP")
    return "CREATE LOOKUP INDEX " + name + " IF NOT EXISTS FOR " +
           (entity == "NODE" ? "(n) ON EACH labels(n)"
                             : "()-[n]-() ON EACH type(n)");
  std::string label =
      Encoding::quote(item.at("labelsOrTypes").at(0).get<std::string>());
  std::string pattern =
      entity == "NODE" ? "(n:" + label + ")" : "()-[n:" + label + "]-()";
  std::string properties;
  for (const nlohmann::json &field : item.at("properties")) {
    if (!properties.empty())
      properties += ",";
    properties += "n." + Encoding::quote(field.get<std::string>());
  }
  if (kind == "UNIQUENESS")
    return "CREATE CONSTRAINT " + name + " IF NOT EXISTS FOR " + pattern +
           " REQUIRE (" + properties + ") IS UNIQUE";
  return "CREATE RANGE INDEX " + name + " IF NOT EXISTS FOR " + pattern +
         " ON (" + properties + ")";
}
} // namespace entity_rdf
