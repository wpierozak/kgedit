#include "entity_rdf/XmlReader.hxx"
#include <algorithm>
#include <libxml/parser.h>
#include <memory>
#include <stdexcept>
namespace entity_rdf {
std::string XmlReader::attribute(const XmlElement &node,
                                 const std::string &key) {
  if (!node.attributes.contains(key) || node.attributes.at(key).empty())
    throw std::runtime_error("Missing XML attribute: " + key);
  return node.attributes.at(key);
}
void XmlReader::require(const XmlElement &node, const std::string &expected,
                        const std::vector<std::string> &keys) {
  if (node.name != expected || node.attributes.size() != keys.size())
    throw std::runtime_error("Unexpected XML element/attributes: " + node.name);
  for (const std::string &key : keys)
    attribute(node, key);
}
XmlElement XmlReader::element(const ::_xmlNode *node) {
  if (node->ns)
    throw std::runtime_error("XML namespaces are not supported");
  XmlElement result;
  result.name = reinterpret_cast<const char *>(node->name);
  for (const xmlAttr *attr = node->properties; attr; attr = attr->next) {
    if (attr->ns)
      throw std::runtime_error("Namespaced attribute unsupported");
    xmlChar *text = xmlNodeListGetString(node->doc, attr->children, 1);
    if (!text)
      throw std::runtime_error("Missing XML value");
    result.attributes.emplace(reinterpret_cast<const char *>(attr->name),
                              reinterpret_cast<const char *>(text));
    xmlFree(text);
  }
  for (const xmlNode *child = node->children; child; child = child->next) {
    if (child->type == XML_ELEMENT_NODE)
      result.children.push_back(element(child));
    else if (child->type != XML_COMMENT_NODE && !xmlIsBlankNode(child))
      throw std::runtime_error("Unexpected XML content");
  }
  return result;
}
XmlElement XmlReader::read(const std::filesystem::path &path) const {
  std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> doc(
      xmlReadFile(path.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS),
      xmlFreeDoc);
  if (!doc || doc->intSubset || doc->extSubset)
    throw std::runtime_error("Invalid XML or prohibited DTD: " + path.string());
  const xmlNode *root = xmlDocGetRootElement(doc.get());
  if (!root)
    throw std::runtime_error("Empty XML");
  return element(root);
}
} // namespace entity_rdf
