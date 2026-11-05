#pragma once
#include <map>
#include <string>
#include <vector>
namespace entity_rdf {
/**
 * @brief Carries the supported element-only XML tree independently of libxml2.
 */
struct XmlElement {
  /// Unqualified element name.
  std::string name;
  /// Attribute names and decoded text values.
  std::map<std::string, std::string> attributes;
  /// Child elements in document order.
  std::vector<XmlElement> children;
};
} // namespace entity_rdf
