#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace entity_rdf {
std::string TurtleWriter::iri(const std::string &value) {
  Encoding::validateIri(value);
  static constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
      prefixes{{{Vocabulary::Xsd, "xsd:"}, {"urn:dcs:graph:", "g:"}}};
  for (const auto &[prefix, name] : prefixes) {
    if (!value.starts_with(prefix))
      continue;
    std::string_view local = std::string_view(value).substr(prefix.size());
    if (!local.empty() && std::ranges::all_of(local, [](char character) {
          return (character >= 'A' && character <= 'Z') ||
                 (character >= 'a' && character <= 'z');
        }))
      return std::string(name) + std::string(local);
  }
  return "<" + value + ">";
}
void TurtleWriter::triple(std::string &output, const std::string &subject,
                          const std::string &predicate,
                          const std::string &object) {
  output += iri(subject) + " " +
            (predicate == Vocabulary::Type ? "a" : iri(predicate)) + " " +
            object + " .\n";
}
std::string TurtleWriter::literal(const nlohmann::json &value) {
  std::string datatype = Vocabulary::Xsd;
  std::string lexical;
  if (value.is_string()) {
    datatype += "string";
    lexical = value.get<std::string>();
  } else if (value.is_boolean()) {
    datatype += "boolean";
    lexical = value.dump();
  } else if (value.is_number_integer()) {
    datatype += "long";
    lexical = value.dump();
  } else if (value.is_number_float()) {
    datatype += "double";
    lexical = value.dump();
  } else if (value.is_object()) {
    std::string kind = value.at("@type");
    if (kind == "float") {
      datatype += "double";
      std::string v = value.at("value");
      lexical = v == "nan" ? "NaN" : v == "inf" ? "INF" : "-INF";
    } else {
      datatype = std::string(Vocabulary::Neo4jPrefix) + kind;
      lexical = value.dump();
    }
  } else
    throw std::runtime_error("Unsupported literal value");
  return nlohmann::json(lexical).dump(-1, ' ', false) + "^^" + iri(datatype);
}
void TurtleWriter::properties(std::string &output, const std::string &subject,
                              const nlohmann::json &properties) {
  for (const auto &[key, value] : properties.items()) {
    std::string predicate =
        std::string(Vocabulary::PropertyPrefix) + Encoding::percent(key);
    if (!value.is_array()) {
      triple(output, subject, predicate, literal(value));
      continue;
    }
    std::string owner = std::string(Vocabulary::ValuePrefix) +
                        Encoding::sha256(subject + "\n" + key);
    triple(output, subject, predicate, iri(owner));
    triple(output, owner, Vocabulary::Type, iri(Vocabulary::Array));
    triple(output, owner, Vocabulary::Length, literal(value.size()));
    for (std::size_t i = 0; i < value.size(); ++i)
      triple(output, owner,
             std::string(Vocabulary::ItemPrefix) + std::to_string(i),
             literal(value[i]));
  }
}
std::string TurtleWriter::write(const Graph &graph) const {
  std::string output = "@prefix xsd: <http://www.w3.org/2001/XMLSchema#> "
                       ".\n@prefix g: <urn:dcs:graph:> .\n";
  for (const auto &[id, node] : graph.nodes) {
    std::string subject =
        node.iri.empty()
            ? std::string(Vocabulary::NodePrefix) + Encoding::percent(id)
            : node.iri;
    triple(output, subject, Vocabulary::Type, iri(Vocabulary::Node));
    if (!node.iri.empty())
      triple(output, subject, Vocabulary::RecordId, literal(id));
    for (const std::string &label : node.labels)
      triple(output, subject, Vocabulary::Label, literal(label));
    properties(output, subject, node.properties);
  }
  for (const auto &[id, edge] : graph.relationships) {
    std::string subject = edge.iri.empty()
                              ? std::string(Vocabulary::RelationshipPrefix) +
                                    Encoding::percent(id)
                              : edge.iri;
    triple(output, subject, Vocabulary::Type, iri(Vocabulary::Relationship));
    if (!edge.iri.empty())
      triple(output, subject, Vocabulary::RecordId, literal(id));
    triple(output, subject, Vocabulary::Source,
           iri(edge.sourceIri.empty() ? std::string(Vocabulary::NodePrefix) +
                                            Encoding::percent(edge.source)
                                      : edge.sourceIri));
    triple(output, subject, Vocabulary::Target,
           iri(edge.targetIri.empty() ? std::string(Vocabulary::NodePrefix) +
                                            Encoding::percent(edge.target)
                                      : edge.targetIri));
    triple(output, subject, Vocabulary::RelationType, literal(edge.type));
    properties(output, subject, edge.properties);
  }
  return output;
}
} // namespace entity_rdf
