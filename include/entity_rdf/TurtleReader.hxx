#pragma once
#include "entity_rdf/RdfDto.hxx"
#include <filesystem>

namespace entity_rdf {
/**
 * @brief Parses and validates Turtle using rdf4cpp without fetching remote
 * resources.
 */
class TurtleReader {
public:
  /**
   * @brief Parse Turtle and validate the supported term kinds and literal
   * datatypes.
   *
   * @throws std::runtime_error If the file is missing, malformed, or uses blank
   * nodes or language-tagged literals.
   * @param path Local Turtle file to parse.
   * @return RDF triples preserving explicit IRIs and typed literal values.
   */
  RdfDocument read(const std::filesystem::path &path) const;
};
} // namespace entity_rdf
