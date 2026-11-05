#include "entity_rdf/Neo4jClient.hxx"
#include "entity_rdf/Neo4jError.hxx"

#include <stdexcept>

namespace entity_rdf {
Neo4jClient::Neo4jClient(std::shared_ptr<IHttpClient> http,
                         std::string transactionEndpoint)
    : http_(std::move(http)), endpoint_(std::move(transactionEndpoint)) {
  if (!http_)
    throw std::invalid_argument("HTTP client is required");
}

nlohmann::json Neo4jClient::execute(const nlohmann::json &statements) const {
  std::string payload = nlohmann::json{{"statements", statements}}.dump();
  HttpResponse response = http_->post(endpoint_, payload);
  nlohmann::json result = nlohmann::json::parse(response.body);
  if (!result.at(HttpFields::Errors).is_array())
    throw std::runtime_error("Invalid Neo4j errors response");
  if (!result.at(HttpFields::Errors).empty())
    throw Neo4jError(result.at(HttpFields::Errors));
  return result.at("results");
}

void Neo4jClient::importTurtle(const std::string &uri) const {
  nlohmann::json statements = nlohmann::json::array(
      {{{"statement", "CALL n10s.rdf.import.fetch($uri, 'Turtle') YIELD "
                      "terminationStatus, triplesLoaded, triplesParsed RETURN "
                      "terminationStatus, triplesLoaded, triplesParsed"},
        {"parameters", {{"uri", uri}}}}});
  nlohmann::json results = execute(statements);
  nlohmann::json rows = results.at(0).at("data");
  if (rows.size() != 1 || rows.at(0).at("row").at(0) != "OK" ||
      rows.at(0).at("row").at(1) != rows.at(0).at("row").at(2))
    throw std::runtime_error(
        "n10s did not import all Turtle statements successfully");
}
} // namespace entity_rdf
