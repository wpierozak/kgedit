#include "entity_rdf/PropertyCodec.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <charconv>
#include <cmath>
#include <rdf4cpp.hpp>
#include <stdexcept>
namespace entity_rdf {
nlohmann::json PropertyCodec::decode(const RdfTerm &term) {
  if (term.iri)
    throw std::runtime_error("Expected literal property value");
  std::string xsd = Vocabulary::Xsd;
  if (term.datatype == xsd + "string")
    return term.value;
  if (term.datatype == xsd + "boolean") {
    if (term.value == "true" || term.value == "1")
      return true;
    if (term.value == "false" || term.value == "0")
      return false;
  }
  if (term.datatype == xsd + "long" || term.datatype == xsd + "integer") {
    std::int64_t result = 0;
    std::string_view text = term.value;
    if (text.starts_with('+'))
      text.remove_prefix(1);
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (error == std::errc{} && end == text.data() + text.size())
      return result;
  }
  if (term.datatype == xsd + "double") {
    if (term.value == "NaN" || term.value == "INF" || term.value == "-INF")
      return nlohmann::json{{"@type", "float"},
                            {"value", term.value == "NaN"   ? "nan"
                                      : term.value == "INF" ? "inf"
                                                            : "-inf"}};
    double result = 0;
    const auto [end, error] = std::from_chars(
        term.value.data(), term.value.data() + term.value.size(), result);
    if (error == std::errc{} && end == term.value.data() + term.value.size() &&
        std::isfinite(result))
      return result;
  }
  if (term.datatype.starts_with(Vocabulary::Neo4jPrefix)) {
    nlohmann::json result = nlohmann::json::parse(term.value);
    std::string kind = term.datatype.substr(
        std::char_traits<char>::length(Vocabulary::Neo4jPrefix));
    if (!result.is_object() || result.value("@type", "") != kind)
      throw std::runtime_error("Mismatched Neo4j property encoding");
    if (kind != "Date" && kind != "Time" && kind != "DateTime" &&
        kind != "Duration" && kind != "point" && kind != "bytes" &&
        kind != "bytearray")
      throw std::runtime_error("Unsupported Neo4j property kind");
    validateTagged(result);
    return result;
  }
  throw std::runtime_error("Unsupported or invalid property datatype: " +
                           term.datatype);
}
void PropertyCodec::validateTagged(const nlohmann::json &value) {
  std::string kind = value.at("@type");
  if (kind == "point") {
    if (value.size() != 3 || !value.at("srid").is_number_integer() ||
        !value.at("coordinates").is_array())
      throw std::runtime_error("Invalid point encoding");
    int srid = value.at("srid");
    std::size_t dimension = (srid == 7203 || srid == 4326)   ? 2
                            : (srid == 9157 || srid == 4979) ? 3
                                                             : 0;
    if (dimension == 0 || value.at("coordinates").size() != dimension)
      throw std::runtime_error("Point SRID/dimension mismatch");
    for (const nlohmann::json &coordinate : value.at("coordinates"))
      if (!coordinate.is_number() || !std::isfinite(coordinate.get<double>()))
        throw std::runtime_error("Invalid point coordinate");
  } else if (kind == "Duration") {
    if (value.size() != 2 || !value.at("value").is_array() ||
        value.at("value").size() != 4)
      throw std::runtime_error("Invalid duration encoding");
    for (const nlohmann::json &component : value.at("value"))
      if (!component.is_number_integer())
        throw std::runtime_error("Duration components must be integers");
  } else {
    if (!value.at("value").is_string())
      throw std::runtime_error("Encoded value must be a string");
    if (kind == "Date" || kind == "Time" || kind == "DateTime") {
      std::string datatype =
          std::string(Vocabulary::Xsd) + (kind == "Date"   ? "date"
                                          : kind == "Time" ? "time"
                                                           : "dateTime");
      [[maybe_unused]] rdf4cpp::Literal validated =
          rdf4cpp::Literal::make_typed(value.at("value").get<std::string>(),
                                       rdf4cpp::IRI(datatype));
    }
    if (kind == "bytes" || kind == "bytearray") {
      std::string hex = value.at("value");
      if (hex.size() % 2 ||
          hex.find_first_not_of("0123456789abcdef") != std::string::npos ||
          value.size() != 2)
        throw std::runtime_error("Invalid byte encoding");
    } else if (kind == "DateTime" && value.contains("timezone")) {
      if (value.size() != 3 || !value.at("timezone").is_string())
        throw std::runtime_error("Invalid timezone encoding");
    } else if (value.size() != 2)
      throw std::runtime_error("Unknown encoded property fields");
  }
}
} // namespace entity_rdf
