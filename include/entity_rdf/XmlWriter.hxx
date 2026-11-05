#pragma once
#include "entity_rdf/XmlDto.hxx"
namespace entity_rdf {
/**
 * @brief Serializes XML DTOs with deterministic indentation and attribute
 * escaping.
 */
class XmlWriter {
public:
  /**
   * @brief Serialize an XML DTO with a declaration and deterministic
   * indentation.
   * @param root Root of the supported element-only tree.
   * @return UTF-8 XML text with escaped attribute values.
   */
  std::string write(const XmlElement &root) const;
  /**
   * @brief Escape special characters and whitespace in an XML attribute value.
   * @param value Unescaped attribute text.
   * @return XML-safe attribute contents without surrounding quotes.
   */
  static std::string escape(const std::string &value);

private:
  /**
   * @brief Append one XML element and its descendants.
   * @param[in,out] result Serialized XML receiving the element.
   * @param node Element with trusted XML names and unescaped attribute values.
   * @param depth Indentation depth, using two spaces per level.
   */
  static void element(std::string &result, const XmlElement &node,
                      std::size_t depth);
};
} // namespace entity_rdf
