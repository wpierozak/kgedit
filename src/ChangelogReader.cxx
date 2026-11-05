#include "entity_rdf/ChangelogReader.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <set>
#include <stdexcept>
namespace entity_rdf {
InputFile ChangelogReader::input(const std::filesystem::path &parent,
                                 const XmlElement &entry,
                                 const std::filesystem::path &boundary) {
  XmlReader::require(entry, entry.name, {"file", "sha256"});
  if (!entry.children.empty())
    throw std::runtime_error("Input declaration must be empty");
  std::filesystem::path relative = XmlReader::attribute(entry, "file");
  if (relative.is_absolute())
    throw std::runtime_error("Input path must be relative");
  std::filesystem::path path = std::filesystem::canonical(parent / relative);
  std::filesystem::path inside = path.lexically_relative(boundary);
  if (inside.empty() || *inside.begin() == "..")
    throw std::runtime_error("Input escapes changelog directory");
  std::string checksum = Encoding::sha256(Encoding::readFile(path.string()));
  if (checksum != XmlReader::attribute(entry, "sha256"))
    throw std::runtime_error("Checksum mismatch: " + path.string());
  return {path, checksum, entry.name};
}
nlohmann::json ChangelogReader::schema(const std::filesystem::path &path) {
  XmlElement root = XmlReader{}.read(path);
  XmlReader::require(root, "graphSchema", {"format"});
  if (XmlReader::attribute(root, "format") != "neo4j-schema-v1")
    throw std::runtime_error("Unknown schema format");
  nlohmann::json result = nlohmann::json::array();
  std::set<std::string> names;
  for (const XmlElement &child : root.children) {
    XmlReader::require(child, "definition", {"name", "type", "entityType"});
    nlohmann::json item = child.attributes;
    if (!names.insert(XmlReader::attribute(child, "name")).second)
      throw std::runtime_error("Duplicate schema name");
    item["labelsOrTypes"] = nlohmann::json::array();
    item["properties"] = nlohmann::json::array();
    for (const XmlElement &field : child.children) {
      if (field.name != "label" && field.name != "property")
        throw std::runtime_error("Unknown schema field");
      XmlReader::require(field, field.name, {"name"});
      if (!field.children.empty())
        throw std::runtime_error("Unexpected schema content");
      item[field.name == "label" ? "labelsOrTypes" : "properties"].push_back(
          XmlReader::attribute(field, "name"));
    }
    std::string kind = XmlReader::attribute(child, "type"),
                entity = XmlReader::attribute(child, "entityType");
    if (entity != "NODE" && entity != "RELATIONSHIP")
      throw std::runtime_error("Invalid schema entity type");
    if (kind == "LOOKUP") {
      if (!item["labelsOrTypes"].empty() || !item["properties"].empty())
        throw std::runtime_error("Invalid lookup schema");
    } else if ((kind != "UNIQUENESS" && kind != "RANGE") ||
               item["labelsOrTypes"].size() != 1 ||
               item["properties"].empty() ||
               (kind == "UNIQUENESS" && entity != "NODE"))
      throw std::runtime_error("Unsupported schema definition");
    result.push_back(std::move(item));
  }
  return result;
}
ChangeSet ChangelogReader::change(const InputFile &file,
                                  const std::filesystem::path &boundary) const {
  XmlElement root = XmlReader{}.read(file.path);
  if (Encoding::sha256(Encoding::readFile(file.path.string())) != file.checksum)
    throw std::runtime_error("Changeset changed while validating");
  XmlReader::require(root, "graphChangeSet", {"id"});
  ChangeSet result;
  result.id = XmlReader::attribute(root, "id");
  result.checksum = file.checksum;
  std::set<std::filesystem::path> paths;
  for (const XmlElement &entry : root.children) {
    if (entry.name != "rdf" && entry.name != "schema" && entry.name != "source")
      throw std::runtime_error("Unknown changeset input: " + entry.name);
    InputFile member = input(file.path.parent_path(), entry, boundary);
    if (!paths.insert(member.path).second)
      throw std::runtime_error("Duplicate changeset input");
    if (entry.name == "rdf") {
      RdfDocument document = TurtleReader{}.read(member.path);
      if (Encoding::sha256(Encoding::readFile(member.path.string())) !=
          member.checksum)
        throw std::runtime_error("Input changed while validating");
      Graph graph = GraphMapper{}.decode(document);
      result.document.insert(result.document.end(), document.begin(),
                             document.end());
      for (const auto &[id, node] : graph.nodes)
        if (result.graph.nodes.contains(id))
          throw std::runtime_error(
              "Duplicate node definition within changeset: " + id);
      for (const auto &[id, edge] : graph.relationships)
        if (result.graph.relationships.contains(id))
          throw std::runtime_error(
              "Duplicate relationship definition within changeset: " + id);
      GraphMapper::overlay(result.graph, graph);
    } else if (entry.name == "schema")
      for (const nlohmann::json &item : schema(member.path))
        result.schema.push_back(item);
    result.files.push_back(std::move(member));
  }
  if (result.graph.nodes.empty() && result.graph.relationships.empty() &&
      result.schema.empty())
    throw std::runtime_error("Empty changeset");
  return result;
}
ChangeLog ChangelogReader::read(const std::filesystem::path &path) const {
  std::filesystem::path rootPath = std::filesystem::canonical(path);
  XmlElement root = XmlReader{}.read(rootPath);
  XmlReader::require(root, "graphChangeLog", {"format"});
  if (XmlReader::attribute(root, "format") != "rdf-changelog-v1")
    throw std::runtime_error("Unsupported changelog format");
  ChangeLog result;
  std::set<std::string> ids;
  std::set<std::filesystem::path> paths;
  std::set<std::string> schemaNames;
  for (const XmlElement &entry : root.children) {
    if (entry.name != "include")
      throw std::runtime_error("Expected changelog include");
    InputFile file =
        input(rootPath.parent_path(), entry, rootPath.parent_path());
    if (!paths.insert(file.path).second)
      throw std::runtime_error("Repeated changelog include");
    ChangeSet set = change(file, rootPath.parent_path());
    if (!ids.insert(set.id).second)
      throw std::runtime_error("Duplicate changeset id: " + set.id);
    for (const nlohmann::json &item : set.schema)
      if (!schemaNames.insert(item.at("name")).second)
        throw std::runtime_error("Schema definitions cannot be overridden");
    GraphMapper::overlay(result.composed, set.graph);
    // Each changeset is an application boundary; references can cross files
    // within it.
    GraphMapper::validate(result.composed);
    result.changes.push_back(std::move(set));
  }
  if (result.changes.empty())
    throw std::runtime_error("Empty changelog");
  return result;
}
} // namespace entity_rdf
