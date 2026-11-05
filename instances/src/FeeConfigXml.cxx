#include "entity_rdf/instances/FeeConfigXml.hxx"
#include "entity_rdf/XmlReader.hxx"
#include "entity_rdf/XmlWriter.hxx"
#include <stdexcept>
namespace entity_rdf::instances {
FeeConfig FeeConfigXml::read(const std::filesystem::path &path) const {
  XmlElement root = XmlReader{}.read(path);
  XmlReader::require(root, "feeInstances", {"format", "iriPrefix"});
  if (XmlReader::attribute(root, "format") != "fee-instance-config-v2")
    throw std::runtime_error("Unsupported FEE config format");
  FeeConfig config;
  config.iriPrefix = XmlReader::attribute(root, "iriPrefix");
  for (const XmlElement &entry : root.children) {
    XmlReader::require(entry, "board", {"iri", "detectorIri", "typeIri"});
    if (!entry.children.empty())
      throw std::runtime_error("Unexpected board content");
    config.boards.push_back({XmlReader::attribute(entry, "iri"),
                             XmlReader::attribute(entry, "detectorIri"),
                             XmlReader::attribute(entry, "typeIri")});
  }
  return config;
}
std::string FeeConfigXml::escape(const std::string &text) {
  return XmlWriter::escape(text);
}
std::string FeeConfigXml::write(const FeeConfig &config) const {
  std::string output =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<feeInstances "
      "format=\"fee-instance-config-v2\" iriPrefix=\"" +
      escape(config.iriPrefix) + "\">\n";
  for (const BoardConfig &board : config.boards)
    output += "  <board iri=\"" + escape(board.iri) + "\" detectorIri=\"" +
              escape(board.detectorIri) + "\" typeIri=\"" +
              escape(board.typeIri) + "\"/>\n";
  return output + "</feeInstances>\n";
}
} // namespace entity_rdf::instances
