#include "entity_rdf/TurtleReader.hxx"
#include <rdf4cpp.hpp>
#include <rdf4cpp/parser/RDFFileParser.hpp>
#include <sstream>
#include <stdexcept>

namespace entity_rdf {
RdfDocument TurtleReader::read(const std::filesystem::path &path) const {
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Missing Turtle file: " + path.string());
  RdfDocument result;
  for (const auto &item : rdf4cpp::parser::RDFFileParser{path.string()}) {
    if (!item.has_value()) {
      std::ostringstream error;
      error << item.error();
      throw std::runtime_error(path.string() + ": " + error.str());
    }
    const rdf4cpp::Quad &quad = item.value();
    if (!quad.subject().is_iri() || !quad.predicate().is_iri())
      throw std::runtime_error("Published records require explicit IRIs; blank "
                               "nodes are unsupported");
    RdfTerm object;
    if (quad.object().is_iri()) {
      object = {std::string(quad.object().as_iri().identifier()), {}, true};
    } else if (quad.object().is_literal()) {
      rdf4cpp::Literal literal = quad.object().as_literal();
      if (literal.datatype().identifier() ==
          "http://www.w3.org/1999/02/22-rdf-syntax-ns#langString")
        throw std::runtime_error(
            "Language-tagged literals are not Neo4j properties");
      object = {std::string(std::string_view(literal.lexical_form())),
                std::string(literal.datatype().identifier()), false};
    } else {
      throw std::runtime_error("Blank node object is unsupported");
    }
    result.push_back({std::string(quad.subject().as_iri().identifier()),
                      std::string(quad.predicate().as_iri().identifier()),
                      std::move(object)});
  }
  return result;
}
} // namespace entity_rdf
