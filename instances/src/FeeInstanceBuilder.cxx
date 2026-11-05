#include "entity_rdf/instances/FeeInstanceBuilder.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <algorithm>
#include <openssl/evp.h>
#include <stdexcept>
namespace entity_rdf::instances {
std::string FeeInstanceBuilder::uuid(const std::string &name) {
  // RFC 4122 UUID5, OID namespace, retaining the historical Python uuid5
  // contract.
  const unsigned char prefix[] = {0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad,
                                  0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0,
                                  0x4f, 0xd4, 0x30, 0xc8};
  std::string input(reinterpret_cast<const char *>(prefix), 16);
  input += name;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (EVP_Digest(input.data(), input.size(), digest, &length, EVP_sha1(),
                 nullptr) != 1)
    throw std::runtime_error("UUID5 hash failed");
  digest[6] = (digest[6] & 0x0f) | 0x50;
  digest[8] = (digest[8] & 0x3f) | 0x80;
  std::string result;
  const char *hex = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      result += '-';
    result += hex[digest[i] >> 4];
    result += hex[digest[i] & 15];
  }
  return result;
}
std::string FeeInstanceBuilder::localName(const std::string &iri) {
  for (char separator : {'#', '/', ':'}) {
    std::size_t pos = iri.rfind(separator);
    if (pos != std::string::npos)
      return iri.substr(pos + 1);
  }
  return iri;
}
const NodeRecord &FeeInstanceBuilder::require(
    const std::map<std::string, const NodeRecord *> &byIri,
    const std::string &iri, const std::string &label) {
  const NodeRecord &node = *byIri.at(iri);
  if (!node.labels.contains(label) || !node.properties.contains("name") ||
      !node.properties.at("name").is_string() ||
      node.properties.at("name") == "")
    throw std::runtime_error("Expected named " + label + ": " + iri);
  return node;
}
bool FeeInstanceBuilder::link(const Graph &graph, const std::string &source,
                              const std::string &type,
                              const std::string &target) {
  return std::ranges::any_of(graph.relationships, [&](const auto &entry) {
    const RelationshipRecord &edge = entry.second;
    return edge.source == source && edge.target == target && edge.type == type;
  });
}
void FeeInstanceBuilder::board(
    Graph &graph, const Graph &core,
    const std::map<std::string, const NodeRecord *> &byIri,
    const BoardConfig &config, const std::string &prefix) const {
  const NodeRecord &board = require(byIri, config.iri, "FrontEndElectronic");
  const NodeRecord &detector =
      require(byIri, config.detectorIri, "DCSDetector");
  const NodeRecord &type =
      require(byIri, config.typeIri, "FrontEndElectronicType");
  if (!link(core, detector.id, "has_hardware", board.id) ||
      !link(core, board.id, "is_of_device_type", type.id))
    throw std::runtime_error("FEE config disagrees with core topology");
  std::set<std::string> parameterIds;
  for (const auto &[id, edge] : core.relationships)
    if (edge.source == type.id && edge.type == "has_parameter")
      parameterIds.insert(edge.target);
  if (parameterIds.empty())
    throw std::runtime_error("No parameter definitions for configured board");
  for (const std::string &parameterId : parameterIds) {
    const NodeRecord &candidate = core.nodes.at(parameterId);
    const NodeRecord &parameter =
        require(byIri, candidate.properties.at("iri"), "HardwareParameter");
    NodeRecord node =
        instance(config, prefix, board, detector, type, parameter);
    preserveIdentity(node, core);
    append(graph, core, std::move(node), parameter.id, board.id);
  }
}
NodeRecord FeeInstanceBuilder::instance(const BoardConfig &config,
                                        const std::string &prefix,
                                        const NodeRecord &board,
                                        const NodeRecord &detector,
                                        const NodeRecord &type,
                                        const NodeRecord &parameter) {
  std::string detectorName = detector.properties.at("name"),
              boardName = board.properties.at("name"),
              parameterName = parameter.properties.at("name");
  std::string normalized = detectorName;
  // Detector codes are ASCII; other Unicode case-folding has no verified
  // contract.
  for (char &c : normalized) {
    if (static_cast<unsigned char>(c) > 127)
      throw std::runtime_error(
          "Detector identity normalization requires ASCII");
    if (c >= 'a' && c <= 'z')
      c -= ('a' - 'A');
  }
  std::string identity =
      uuid(normalized + "." + boardName + "." + parameterName);
  std::string instanceIri =
      prefix + Encoding::percent(localName(config.iri)) + "/" +
      Encoding::percent(localName(parameter.properties.at("iri")));
  std::string id =
      "n_" +
      Encoding::sha256(nlohmann::json::array({"entity_identity", identity})
                           .dump(-1, ' ', false));
  return {id,
          {"Entity", "FEEParameterInstance"},
          {{"board_iri", config.iri},
           {"board_name", boardName},
           {"board_type", type.properties.at("name")},
           {"board_type_iri", config.typeIri},
           {"detector", detectorName},
           {"detector_iri", config.detectorIri},
           {"name", boardName + "/" + parameterName},
           {"parameter_iri", parameter.properties.at("iri")},
           {"parameter_name", parameterName},
           {"should_filter_on_explore", true},
           {"entity_identity", identity},
           {"iri", instanceIri}}};
}
void FeeInstanceBuilder::preserveIdentity(NodeRecord &node, const Graph &core) {
  for (const auto &[id, existing] : core.nodes) {
    bool sameIri = existing.properties.value("iri", "") ==
                   node.properties.at("iri").get<std::string>();
    bool sameIdentity =
        existing.properties.value("entity_identity", "") ==
        node.properties.at("entity_identity").get<std::string>();
    if (!sameIri && !sameIdentity)
      continue;
    if (!sameIri || !sameIdentity ||
        !existing.labels.contains("FEEParameterInstance"))
      throw std::runtime_error(
          "Generated instance identity conflicts with an existing definition");
    node.id = existing.id;
  }
}
void FeeInstanceBuilder::append(Graph &graph, const Graph &core,
                                NodeRecord node, const std::string &parameter,
                                const std::string &board) {
  std::string nodeId = node.id;
  for (const auto &[id, existing] : graph.nodes)
    if (existing.properties.at("iri") ==
            node.properties.at("iri").get<std::string>() ||
        existing.properties.at("entity_identity") ==
            node.properties.at("entity_identity"))
      throw std::runtime_error(
          "Configured instances have colliding identities");
  if (!graph.nodes.emplace(nodeId, std::move(node)).second)
    throw std::runtime_error("Generated node ID collision");
  for (const std::string relation : {"instance_of", "parameter_of"}) {
    std::string target = relation == "instance_of" ? parameter : board;
    bool existing = false;
    for (const auto &[id, edge] : core.relationships)
      if (edge.source == nodeId && edge.type == relation &&
          edge.target == target) {
        graph.relationships.emplace(id, edge);
        existing = true;
      }
    if (existing)
      continue;
    std::string id =
        "e_" +
        Encoding::sha256(nlohmann::json::array({nodeId, target, relation,
                                                nlohmann::json::object()})
                             .dump(-1, ' ', false));
    graph.relationships.emplace(id,
                                RelationshipRecord{id, nodeId, target, relation,
                                                   nlohmann::json::object()});
  }
}

Graph FeeInstanceBuilder::build(const Graph &core,
                                const FeeConfig &config) const {
  GraphMapper::validate(core);
  std::map<std::string, const NodeRecord *> byIri;
  for (const auto &[id, node] : core.nodes)
    if (node.properties.contains("iri")) {
      std::string iri = node.properties.at("iri");
      if (!byIri.emplace(iri, &node).second)
        throw std::runtime_error("Ambiguous core IRI");
    }
  Graph graph;
  std::set<std::string> boards;
  for (const BoardConfig &item : config.boards) {
    if (!boards.insert(item.iri).second)
      throw std::runtime_error("Duplicate configured board");
    board(graph, core, byIri, item, config.iriPrefix);
  }
  Graph composed = core;
  GraphMapper::overlay(composed, graph);
  GraphMapper::validate(composed);
  return graph;
}
} // namespace entity_rdf::instances
