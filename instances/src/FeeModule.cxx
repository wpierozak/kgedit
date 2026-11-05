#include "entity_rdf/instances/FeeModule.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include "entity_rdf/instances/FeeConfigXml.hxx"
#include "entity_rdf/instances/FeeInstanceBuilder.hxx"
#include <algorithm>
#include <fstream>
#include <openssl/evp.h>
#include <stdexcept>
namespace entity_rdf::instances {
Graph FeeModule::build(const Graph &core, const FeeConfig &config) const {
  return FeeInstanceBuilder{}.build(core, config);
}
std::string FeeModule::uuid(const std::string &name) {
  return FeeInstanceBuilder::uuid(name);
}
void FeeModule::writeNew(const std::filesystem::path &path,
                         const std::string &content) {
  if (std::filesystem::exists(path))
    throw std::runtime_error("Refusing to overwrite " + path.string());
  std::ofstream output(path, std::ios::binary | std::ios::noreplace);
  output << content;
  if (!output)
    throw std::runtime_error("Cannot write " + path.string());
}
void FeeModule::generate(const Graph &core, const std::filesystem::path &config,
                         const std::filesystem::path &output,
                         const std::string &id) const {
  Graph graph = build(core, FeeConfigXml{}.read(config));
  Graph canonicalCore = core;
  GraphMapper::canonicalize(canonicalCore);
  GraphMapper::canonicalize(graph, canonicalCore);
  if (std::filesystem::exists(output))
    throw std::runtime_error("Instance output directory already exists");
  std::filesystem::create_directories(output);
  std::string source = Encoding::readFile(config.string()),
              turtle = TurtleWriter{}.write(graph);
  writeNew(output / "fee-parameters.xml", source);
  writeNew(output / "instances.ttl", turtle);
  writeNew(output / "changelog.xml",
           "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<graphChangeSet id=\"" +
               FeeConfigXml::escape(id) +
               "\">\n  <source file=\"fee-parameters.xml\" sha256=\"" +
               Encoding::sha256(source) +
               "\"/>\n  <rdf file=\"instances.ttl\" sha256=\"" +
               Encoding::sha256(turtle) + "\"/>\n</graphChangeSet>\n");
}
void FeeModule::createConfig(const Graph &core,
                             const std::filesystem::path &output,
                             const std::string &prefix) const {
  FeeConfig config;
  config.iriPrefix = prefix;
  for (const auto &[id, node] : core.nodes)
    if (node.labels.contains("FrontEndElectronic")) {
      std::vector<std::string> detectors, types;
      for (const auto &[edgeId, edge] : core.relationships) {
        if (edge.target == id && edge.type == "has_hardware" &&
            core.nodes.at(edge.source).labels.contains("DCSDetector"))
          detectors.push_back(core.nodes.at(edge.source).properties.at("iri"));
        if (edge.source == id && edge.type == "is_of_device_type")
          types.push_back(core.nodes.at(edge.target).properties.at("iri"));
      }
      std::ranges::sort(detectors);
      detectors.erase(std::unique(detectors.begin(), detectors.end()),
                      detectors.end());
      std::ranges::sort(types);
      types.erase(std::unique(types.begin(), types.end()), types.end());
      if (detectors.size() != 1 || types.size() != 1)
        throw std::runtime_error(
            "Ambiguous board topology; define instance XML explicitly");
      config.boards.push_back(
          {node.properties.at("iri"), detectors.front(), types.front()});
    }
  std::ranges::sort(config.boards, {}, &BoardConfig::iri);
  writeNew(output, FeeConfigXml{}.write(config));
}
} // namespace entity_rdf::instances
