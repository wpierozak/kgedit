#include "entity_rdf/XmlWriter.hxx"
namespace entity_rdf {
std::string XmlWriter::escape(const std::string &value) {
  std::string result;
  for (char c : value) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\n':
      result += "&#10;";
      break;
    case '\r':
      result += "&#13;";
      break;
    case '\t':
      result += "&#9;";
      break;
    default:
      result += c;
    }
  }
  return result;
}
void XmlWriter::element(std::string &result, const XmlElement &node,
                        std::size_t depth) {
  result += std::string(depth * 2, ' ') + "<" + node.name;
  for (const auto &[key, value] : node.attributes)
    result += " " + key + "=\"" + escape(value) + "\"";
  if (node.children.empty()) {
    result += "/>\n";
    return;
  }
  result += ">\n";
  for (const XmlElement &child : node.children)
    element(result, child, depth + 1);
  result += std::string(depth * 2, ' ') + "</" + node.name + ">\n";
}
std::string XmlWriter::write(const XmlElement &root) const {
  std::string result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  element(result, root, 0);
  return result;
}
} // namespace entity_rdf
