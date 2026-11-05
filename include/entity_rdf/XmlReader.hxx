#pragma once
#include "entity_rdf/XmlDto.hxx"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
struct _xmlNode;
namespace entity_rdf {
/**
 * @brief Reads restricted XML and validates exact element and attribute
 * contracts.
 */
class XmlReader {
public:
  /**
   * @brief Read a required nonempty XML attribute.
   *
   * @throws std::runtime_error If the attribute is absent or empty.
   * @param node Element containing the attribute.
   * @param key Required attribute name.
   * @return Exact attribute value.
   */
  static std::string attribute(const XmlElement &node, const std::string &key);
  /**
   * @brief Require an exact element name and set of nonempty attributes.
   *
   * Does not validate child elements.
   * @throws std::runtime_error If the element or attribute contract differs.
   * @param node Element to validate.
   * @param expected Required element name.
   * @param keys Exact allowed and required attribute names.
   */
  static void require(const XmlElement &node, const std::string &expected,
                      const std::vector<std::string> &keys);
  /**
   * @brief Parse a restricted XML document with network access disabled.
   *
   * @throws std::runtime_error If parsing fails or DTDs, namespaces or
   * unsupported content occur.
   * @param path Local XML input path.
   * @return Root element DTO.
   */
  XmlElement read(const std::filesystem::path &path) const;

private:
  /**
   * @brief Copy a libxml2 element tree into the supported XML DTO.
   *
   * @throws std::runtime_error If namespaces or nonblank non-element content
   * occur.
   * @param node Non-null libxml2 element owned by the current parsed document.
   * @return Element with copied attributes and recursively decoded children.
   */
  static XmlElement element(const ::_xmlNode *node);
};
} // namespace entity_rdf
