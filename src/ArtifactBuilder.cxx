#include "entity_rdf/ArtifactBuilder.hxx"
#include "entity_rdf/CypherBuilder.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include "entity_rdf/XmlWriter.hxx"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace entity_rdf {
std::string ArtifactBuilder::literal(const nlohmann::json &value) {
  if (value.is_object()) {
    std::string result = "{";
    for (const auto &[key, item] : value.items()) {
      if (result.size() != 1)
        result += ",";
      result += Encoding::quote(key) + ":" + literal(item);
    }
    return result + "}";
  }
  if (value.is_array()) {
    std::string result = "[";
    for (const nlohmann::json &item : value) {
      if (result.size() != 1)
        result += ",";
      result += literal(item);
    }
    return result + "]";
  }
  if (value.is_number_float() && !std::isfinite(value.get<double>()))
    throw std::runtime_error(
        "Encode non-finite floats explicitly before export");
  return value.dump(-1, ' ', false);
}
std::string ArtifactBuilder::render(const std::string &query,
                                    const nlohmann::json &rows) {
  // Parameters are generated tokens. Preserve dollar signs inside escaped
  // identifiers, which may legitimately occur in application labels/keys.
  std::string rendered;
  bool identifier = false;
  for (std::size_t index = 0; index < query.size(); ++index) {
    char character = query[index];
    if (character == '`')
      identifier = !identifier;
    if (character == '$' && !identifier)
      rendered += "input.";
    else
      rendered += character;
  }
  return "UNWIND " + literal(rows) +
         " AS input MATCH (resource:Resource {uri:input.__iri}) " + rendered +
         ";\n";
}
std::string ArtifactBuilder::projections(const nlohmann::json &statements,
                                         const std::vector<std::string> &iris) {
  std::map<std::string, nlohmann::json> groups;
  for (std::size_t index = 0; index < statements.size(); ++index) {
    nlohmann::json row = statements.at(index).at("parameters");
    row["__iri"] = iris.at(index);
    std::string query = statements.at(index).at("statement");
    if (!groups.contains(query))
      groups[query] = nlohmann::json::array();
    groups.at(query).push_back(std::move(row));
  }
  std::string result;
  for (const auto &[query, rows] : groups) {
    for (std::size_t start = 0; start < rows.size(); start += 200) {
      nlohmann::json batch = nlohmann::json::array();
      for (std::size_t index = start;
           index < std::min(start + 200, rows.size()); ++index)
        batch.push_back(rows.at(index));
      result += render(query, batch);
    }
  }
  return result;
}
std::string ArtifactBuilder::cypher(const ChangeLog &log,
                                    const std::string &turtleUri,
                                    std::size_t triples) const {
  GraphMapper::validate(log.composed);
  Encoding::validateIri(turtleUri);
  if (turtleUri.empty() || turtleUri.find(':') == std::string::npos)
    throw std::runtime_error("Supply an absolute Neo4j-visible TTL URI");
  std::string result =
      "// Generated for cypher-shell --fail-fast; Neo4j 5.26 + n10s 5.26.0.\n"
      "// Requires an empty database. Execute once; inspect manifest.xml "
      "first.\n"
      "MATCH (n) WITH count(n) AS total RETURN CASE WHEN total=0 THEN true "
      "ELSE datetime('entity-rdf requires an empty database') IS NOT NULL END "
      "AS emptyDatabase;\n"
      "CREATE CONSTRAINT n10s_unique_uri IF NOT EXISTS FOR (n:Resource) "
      "REQUIRE n.uri IS UNIQUE;\n"
      "CALL "
      "n10s.graphconfig.init({handleVocabUris:'KEEP',handleRDFTypes:'NODES',"
      "keepCustomDataTypes:true});\n"
      "CALL n10s.rdf.import.fetch(" +
      literal(turtleUri) +
      ",'Turtle') YIELD "
      "terminationStatus,triplesLoaded,triplesParsed,extraInfo RETURN "
      "CASE "
      "WHEN terminationStatus='OK' AND triplesLoaded=" +
      std::to_string(triples) +
      " AND triplesParsed=" + std::to_string(triples) +
      " THEN true ELSE datetime('entity-rdf incomplete RDF import: ' + "
      "terminationStatus + ' loaded=' + toString(triplesLoaded) + ' parsed=' + "
      "toString(triplesParsed) + ' ' + coalesce(extraInfo,'')) IS NOT "
      "NULL END AS imported;\n";
  result += "CREATE CONSTRAINT entity_rdf_id IF NOT EXISTS FOR (n:_DATASET_) "
            "REQUIRE n._dataset_id IS UNIQUE;\n";
  for (const ChangeSet &change : log.changes)
    for (const nlohmann::json &definition : change.schema)
      result += CypherBuilder::schema(definition) + ";\n";
  result += ":begin\n";
  std::vector<std::string> nodeIris, relationshipIris;
  for (const auto &[id, node] : log.composed.nodes) {
    if (node.iri.empty())
      throw std::runtime_error("Build requires canonical node IRIs");
    nodeIris.push_back(node.iri);
  }
  for (const auto &[id, edge] : log.composed.relationships) {
    if (edge.iri.empty())
      throw std::runtime_error("Build requires canonical relationship IRIs");
    relationshipIris.push_back(edge.iri);
  }
  result += projections(CypherBuilder{}.nodes(log.composed), nodeIris);
  result += projections(CypherBuilder{}.relationships(log.composed),
                        relationshipIris);
  result +=
      "MATCH (n:_DATASET_) WITH count(n) AS total RETURN CASE WHEN total=" +
      std::to_string(log.composed.nodes.size()) +
      " THEN true ELSE datetime('entity-rdf missing application nodes') IS NOT "
      "NULL END AS nodesVerified;\n";
  result += "MATCH (:_DATASET_)-[r]->(:_DATASET_) WITH count(r) AS total "
            "RETURN CASE WHEN total=" +
            std::to_string(log.composed.relationships.size()) +
            " THEN true ELSE datetime('entity-rdf missing application "
            "relationships') IS NOT NULL END AS relationshipsVerified;\n";
  result += "MATCH (n:Resource) DETACH DELETE n;\n:commit\n";
  return result;
}
void ArtifactBuilder::writeNew(const std::filesystem::path &path,
                               const std::string &content) {
  std::ofstream output(path, std::ios::binary | std::ios::noreplace);
  output << content;
  output.close();
  if (!output)
    throw std::runtime_error("Cannot write " + path.string());
}
void ArtifactBuilder::build(const ChangeLog &log,
                            const std::filesystem::path &output,
                            const std::string &turtleUri) const {
  if (std::filesystem::exists(output))
    throw std::runtime_error("Artifact output directory already exists");
  GraphMapper::validate(log.composed);
  std::string turtle = TurtleWriter{}.write(log.composed);
  std::size_t triples = std::ranges::count(turtle, '\n') - 2;
  std::string script = cypher(log, turtleUri, triples);
  XmlElement manifest{
      "entityRdfArtifacts",
      {{"format", "entity-rdf-artifacts-v1"},
       {"neo4j", "5.26"},
       {"n10s", "5.26.0"},
       {"nodes", std::to_string(log.composed.nodes.size())},
       {"relationships", std::to_string(log.composed.relationships.size())},
       {"triples", std::to_string(triples)},
       {"ttlUri", turtleUri}},
      {}};
  for (const ChangeSet &change : log.changes)
    manifest.children.push_back(
        {"changeset", {{"id", change.id}, {"sha256", change.checksum}}, {}});
  manifest.children.push_back(
      {"artifact",
       {{"file", "combined.ttl"}, {"sha256", Encoding::sha256(turtle)}},
       {}});
  manifest.children.push_back(
      {"artifact",
       {{"file", "import.cypher"}, {"sha256", Encoding::sha256(script)}},
       {}});
  std::string metadata = XmlWriter{}.write(manifest);
  // Publish a complete bundle by renaming a sibling directory. Existing outputs
  // are never overwritten; partial generation remains outside the final path.
  std::filesystem::path pending = output;
  pending += ".pending";
  if (!std::filesystem::create_directory(pending))
    throw std::runtime_error("Pending artifact directory already exists");
  try {
    writeNew(pending / "combined.ttl", turtle);
    writeNew(pending / "import.cypher", script);
    writeNew(pending / "manifest.xml", metadata);
    if (std::filesystem::exists(output))
      throw std::runtime_error("Artifact output appeared during generation");
    std::filesystem::rename(pending, output);
  } catch (...) {
    std::filesystem::remove_all(pending);
    throw;
  }
}
} // namespace entity_rdf
