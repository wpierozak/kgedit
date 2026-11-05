#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/PropertyCodec.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace entity_rdf {
std::string GraphMapper::id(const std::string &iri, const char *prefix) {
  if (!iri.starts_with(prefix))
    throw std::runtime_error("Unexpected record IRI: " + iri);
  std::string result = Encoding::unpercent(
      std::string_view(iri).substr(std::char_traits<char>::length(prefix)));
  if (result.empty())
    throw std::runtime_error("Empty record identity");
  return result;
}
RdfTerm GraphMapper::single(const std::vector<RdfTriple> &triples,
                            const char *predicate) {
  const RdfTerm *result = nullptr;
  for (const RdfTriple &triple : triples)
    if (triple.predicate == predicate) {
      if (result)
        throw std::runtime_error("Duplicate field: " + std::string(predicate));
      result = &triple.object;
    }
  if (!result)
    throw std::runtime_error("Missing field: " + std::string(predicate));
  return *result;
}
nlohmann::json GraphMapper::scalar(const RdfTerm &term) {
  return PropertyCodec::decode(term);
}
void GraphMapper::checkPredicates(const std::vector<RdfTriple> &triples,
                                  const std::set<std::string> &allowed) {
  for (const RdfTriple &triple : triples)
    if (!allowed.contains(triple.predicate) &&
        !triple.predicate.starts_with(Vocabulary::PropertyPrefix))
      throw std::runtime_error("Unknown record predicate: " + triple.predicate);
}
nlohmann::json GraphMapper::value(const RdfTerm &term, const Subjects &subjects,
                                  std::set<std::string> &consumed) const {
  if (!term.iri)
    return scalar(term);
  if (!term.value.starts_with(Vocabulary::ValuePrefix) ||
      !consumed.insert(term.value).second)
    throw std::runtime_error("Compound values must have one owner");
  const std::vector<RdfTriple> &triples = subjects.at(term.value);
  RdfTerm kind = single(triples, Vocabulary::Type);
  if (!kind.iri || kind.value != Vocabulary::Array)
    throw std::runtime_error("Expected array value");
  nlohmann::json length = scalar(single(triples, Vocabulary::Length));
  if (!length.is_number_integer() || length.get<std::int64_t>() < 0 ||
      length.get<std::uint64_t>() > 1000000)
    throw std::runtime_error("Invalid array length");
  if (triples.size() != length.get<std::size_t>() + 2)
    throw std::runtime_error("Unexpected array fields");
  nlohmann::json result = nlohmann::json::array();
  for (std::size_t i = 0; i < length.get<std::size_t>(); ++i) {
    std::string predicate =
        std::string(Vocabulary::ItemPrefix) + std::to_string(i);
    nlohmann::json item = scalar(single(triples, predicate.c_str()));
    if (!result.empty() && result.front().type() != item.type())
      throw std::runtime_error("Neo4j arrays must be homogeneous");
    result.push_back(std::move(item));
  }
  return result;
}
nlohmann::json GraphMapper::properties(const std::vector<RdfTriple> &triples,
                                       const Subjects &subjects,
                                       std::set<std::string> &consumed) const {
  nlohmann::json result = nlohmann::json::object();
  for (const RdfTriple &triple : triples)
    if (triple.predicate.starts_with(Vocabulary::PropertyPrefix)) {
      std::string key =
          Encoding::unpercent(std::string_view(triple.predicate)
                                  .substr(std::char_traits<char>::length(
                                      Vocabulary::PropertyPrefix)));
      if (key.empty() || key == Vocabulary::DatasetId || result.contains(key))
        throw std::runtime_error("Empty, reserved or duplicate property: " +
                                 key);
      result[key] = value(triple.object, subjects, consumed);
    }
  return result;
}
NodeRecord GraphMapper::readNode(const std::string &subject,
                                 const std::vector<RdfTriple> &triples,
                                 const Subjects &subjects,
                                 std::set<std::string> &consumed) const {
  checkPredicates(triples,
                  {Vocabulary::Type, Vocabulary::Label, Vocabulary::RecordId});
  NodeRecord node;
  bool canonical = std::ranges::any_of(triples, [](const RdfTriple &triple) {
    return triple.predicate == Vocabulary::RecordId;
  });
  if (canonical) {
    nlohmann::json recordId = scalar(single(triples, Vocabulary::RecordId));
    if (!recordId.is_string() || recordId.get<std::string>().empty())
      throw std::runtime_error("Record ID must be a nonempty string");
    node.id = recordId.get<std::string>();
    node.iri = subject;
  } else
    node.id = id(subject, Vocabulary::NodePrefix);
  node.properties = properties(triples, subjects, consumed);
  if (canonical) {
    if (node.properties.contains("iri") && node.properties.at("iri") != subject)
      throw std::runtime_error("Node subject differs from application IRI");
    node.properties["iri"] = subject;
  }
  for (const RdfTriple &triple : triples)
    if (triple.predicate == Vocabulary::Label) {
      nlohmann::json label = scalar(triple.object);
      if (!label.is_string() || label.get<std::string>().empty())
        throw std::runtime_error("Invalid label");
      node.labels.insert(label.get<std::string>());
    }
  if (node.labels.contains(Vocabulary::DatasetLabel) ||
      node.labels.contains(Vocabulary::LedgerLabel) ||
      node.labels.contains("Resource") ||
      node.labels.contains("_GraphConfig") ||
      node.labels.contains("_NsPrefDef"))
    throw std::runtime_error("Reserved node label");
  return node;
}
RelationshipRecord GraphMapper::readRelationship(
    const std::string &subject, const std::vector<RdfTriple> &triples,
    const Subjects &subjects, std::set<std::string> &consumed) const {
  checkPredicates(triples,
                  {Vocabulary::Type, Vocabulary::Source, Vocabulary::Target,
                   Vocabulary::RelationType, Vocabulary::RecordId});
  RdfTerm source = single(triples, Vocabulary::Source),
          target = single(triples, Vocabulary::Target);
  if (!source.iri || !target.iri)
    throw std::runtime_error("Relationship endpoints must be IRIs");
  nlohmann::json type = scalar(single(triples, Vocabulary::RelationType));
  if (!type.is_string() || type.get<std::string>().empty())
    throw std::runtime_error("Empty relationship type");
  RelationshipRecord edge;
  edge.type = type.get<std::string>();
  edge.properties = properties(triples, subjects, consumed);
  bool canonical = std::ranges::any_of(triples, [](const RdfTriple &triple) {
    return triple.predicate == Vocabulary::RecordId;
  });
  if (canonical) {
    nlohmann::json recordId = scalar(single(triples, Vocabulary::RecordId));
    if (!recordId.is_string() || recordId.get<std::string>().empty())
      throw std::runtime_error("Record ID must be a nonempty string");
    edge.id = recordId.get<std::string>();
    edge.iri = subject;
    edge.source = edge.sourceIri = source.value;
    edge.target = edge.targetIri = target.value;
    if (edge.properties.contains("iri") && edge.properties.at("iri") != subject)
      throw std::runtime_error(
          "Relationship subject differs from application IRI");
    edge.properties["iri"] = subject;
  } else {
    edge.id = id(subject, Vocabulary::RelationshipPrefix);
    edge.source = id(source.value, Vocabulary::NodePrefix);
    edge.target = id(target.value, Vocabulary::NodePrefix);
  }
  return edge;
}
Graph GraphMapper::decode(const RdfDocument &document) const {
  Subjects subjects;
  for (const RdfTriple &triple : document) {
    std::vector<RdfTriple> &group = subjects[triple.subject];
    bool duplicate = false;
    for (const RdfTriple &prior : group)
      if (prior.predicate == triple.predicate &&
          prior.object.value == triple.object.value &&
          prior.object.datatype == triple.object.datatype &&
          prior.object.iri == triple.object.iri)
        duplicate = true;
    if (!duplicate)
      group.push_back(triple);
  }
  Graph result;
  std::set<std::string> consumed;
  for (const auto &[subject, triples] : subjects) {
    if (subject.starts_with(Vocabulary::ValuePrefix))
      continue;
    consumed.insert(subject);
    RdfTerm kind = single(triples, Vocabulary::Type);
    if (!kind.iri)
      throw std::runtime_error("Record type must be an IRI");
    if (kind.value == Vocabulary::Node) {
      NodeRecord node = readNode(subject, triples, subjects, consumed);
      if (!result.nodes.emplace(node.id, std::move(node)).second)
        throw std::runtime_error("Duplicate node record ID");
    } else if (kind.value == Vocabulary::Relationship) {
      RelationshipRecord edge =
          readRelationship(subject, triples, subjects, consumed);
      if (!result.relationships.emplace(edge.id, std::move(edge)).second)
        throw std::runtime_error("Duplicate relationship record ID");
    } else
      throw std::runtime_error("Unknown RDF record type: " + kind.value);
  }
  if (consumed.size() != subjects.size())
    throw std::runtime_error("Unowned RDF value resource");
  resolve(result);
  return result;
}
void GraphMapper::overlay(Graph &graph, const Graph &change) {
  Graph candidate = graph;
  for (const auto &[id, node] : change.nodes) {
    if (candidate.nodes.contains(id) && candidate.nodes.at(id).iri != node.iri)
      throw std::runtime_error("Cannot change an existing node IRI");
    candidate.nodes.insert_or_assign(id, node);
  }
  for (const auto &[id, edge] : change.relationships) {
    if (candidate.relationships.contains(id) &&
        candidate.relationships.at(id).iri != edge.iri)
      throw std::runtime_error("Cannot change an existing relationship IRI");
    candidate.relationships.insert_or_assign(id, edge);
  }
  resolve(candidate);
  graph = std::move(candidate);
}
void GraphMapper::resolve(Graph &graph) {
  std::map<std::string, std::string> identities;
  for (const auto &[id, node] : graph.nodes) {
    if (id.empty() || id != node.id)
      throw std::runtime_error("Invalid node record ID");
    if (!node.iri.empty()) {
      Encoding::validateIri(node.iri);
      if (node.iri.starts_with(Vocabulary::ValuePrefix))
        throw std::runtime_error("Canonical IRI uses reserved value namespace");
      if (node.properties.value("iri", "") != node.iri)
        throw std::runtime_error("Node subject differs from application IRI");
      if (!identities.emplace(node.iri, id).second)
        throw std::runtime_error("Duplicate canonical IRI: " + node.iri);
    }
  }
  std::set<std::string> relationshipIris;
  for (auto &[id, edge] : graph.relationships) {
    if (id.empty() || id != edge.id)
      throw std::runtime_error("Invalid relationship record ID");
    if (!edge.iri.empty()) {
      Encoding::validateIri(edge.iri);
      if (edge.iri.starts_with(Vocabulary::ValuePrefix) ||
          edge.properties.value("iri", "") != edge.iri)
        throw std::runtime_error("Invalid relationship canonical IRI");
    }
    if (!edge.iri.empty() && (!relationshipIris.insert(edge.iri).second ||
                              identities.contains(edge.iri)))
      throw std::runtime_error("Duplicate canonical IRI: " + edge.iri);
    if (!edge.sourceIri.empty())
      edge.source = identities.contains(edge.sourceIri)
                        ? identities.at(edge.sourceIri)
                        : edge.sourceIri;
    if (!edge.targetIri.empty())
      edge.target = identities.contains(edge.targetIri)
                        ? identities.at(edge.targetIri)
                        : edge.targetIri;
  }
}
void GraphMapper::canonicalize(Graph &graph, const Graph &context) {
  for (auto &[id, node] : graph.nodes) {
    if (node.iri.empty())
      node.iri = node.properties.value(
          "iri",
          std::string(Vocabulary::CanonicalNodePrefix) + Encoding::percent(id));
    if (node.iri.empty())
      throw std::runtime_error("Empty application IRI");
    if (node.properties.contains("iri") &&
        node.properties.at("iri") != node.iri)
      throw std::runtime_error("Node subject differs from application IRI");
    node.properties["iri"] = node.iri;
  }
  for (auto &[id, edge] : graph.relationships) {
    if (edge.iri.empty())
      edge.iri = edge.properties.value(
          "iri", std::string(Vocabulary::CanonicalRelationshipPrefix) +
                     Encoding::percent(id));
    if (edge.properties.contains("iri") &&
        edge.properties.at("iri") != edge.iri)
      throw std::runtime_error(
          "Relationship subject differs from application IRI");
    edge.properties["iri"] = edge.iri;
    for (const bool source : {true, false}) {
      std::string &reference = source ? edge.sourceIri : edge.targetIri;
      const std::string &endpoint = source ? edge.source : edge.target;
      if (reference.empty()) {
        if (graph.nodes.contains(endpoint))
          reference = graph.nodes.at(endpoint).iri;
        else if (context.nodes.contains(endpoint))
          reference = context.nodes.at(endpoint).iri;
        else
          throw std::runtime_error("Missing endpoint: " + endpoint);
      }
    }
  }
  resolve(graph);
}
void GraphMapper::validate(const Graph &graph) {
  Graph checked = graph;
  resolve(checked);
  for (const auto &[id, edge] : graph.relationships) {
    if (!graph.nodes.contains(edge.source) ||
        !graph.nodes.contains(edge.target))
      throw std::runtime_error("Missing endpoint in relationship " + id);
    if ((!edge.sourceIri.empty() &&
         graph.nodes.at(edge.source).iri != edge.sourceIri) ||
        (!edge.targetIri.empty() &&
         graph.nodes.at(edge.target).iri != edge.targetIri))
      throw std::runtime_error(
          "Unresolved canonical endpoint in relationship " + id);
  }
}
} // namespace entity_rdf
