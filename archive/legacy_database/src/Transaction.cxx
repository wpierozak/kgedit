#include "entity_rdf/Transaction.hxx"
#include "entity_rdf/Neo4jError.hxx"
#include <stdexcept>
namespace entity_rdf {
Transaction::Transaction(std::shared_ptr<IHttpClient> http,
                         const std::string &commitEndpoint)
    : http_(std::move(http)) {
  if (!http_ || !commitEndpoint.ends_with("/tx/commit"))
    throw std::runtime_error("Expected Neo4j transaction commit endpoint");
  std::string transactions =
      commitEndpoint.substr(0, commitEndpoint.size() - 7);
  nlohmann::json response = post(transactions, nlohmann::json::array());
  std::string returned = response.at("commit");
  std::size_t position = returned.rfind("/tx/");
  if (position == std::string::npos || !returned.ends_with("/commit"))
    throw std::runtime_error("Invalid transaction URL");
  std::string identifier =
      returned.substr(position + 4, returned.size() - position - 11);
  if (identifier.empty() ||
      identifier.find_first_not_of("0123456789") != std::string::npos)
    throw std::runtime_error("Invalid Neo4j transaction identifier");
  base_ = transactions + "/" + identifier;
}
nlohmann::json Transaction::post(const std::string &url,
                                 const nlohmann::json &statements) const {
  HttpResponse response =
      http_->post(url, nlohmann::json{{"statements", statements}}.dump());
  nlohmann::json result = nlohmann::json::parse(response.body);
  if (response.status < 200 || response.status >= 300)
    throw std::runtime_error("Unexpected Neo4j transaction HTTP status");
  if (!result.at(HttpFields::Errors).is_array())
    throw std::runtime_error("Invalid Neo4j errors response");
  if (!result.at(HttpFields::Errors).empty())
    throw Neo4jError(result.at(HttpFields::Errors));
  return result;
}
nlohmann::json Transaction::execute(const nlohmann::json &statements) {
  return post(base_, statements).at("results");
}
void Transaction::commit() {
  post(base_ + "/commit", nlohmann::json::array());
  committed_ = true;
}
Transaction::~Transaction() {
  if (!committed_ && !base_.empty()) {
    try {
      http_->remove(base_);
    } catch (...) { /* Neo4j also expires disconnected transactions. */
    }
  }
}
} // namespace entity_rdf
