#pragma once

#include "entity_rdf/HttpClient.hxx"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace entity_rdf {
/**
 * @brief Retired adapter for Neo4j transactional HTTP and n10s import calls.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class Neo4jClient {
public:
  /**
   * @brief Bind the retired client to an injected HTTP transport.
   *
   * @throws std::invalid_argument If the transport is null.
   * @param http Non-null shared transport.
   * @param transactionEndpoint Neo4j transactional commit URL.
   */
  Neo4jClient(std::shared_ptr<IHttpClient> http,
              std::string transactionEndpoint);
  /**
   * @brief Submit retired autocommit Cypher statements and validate the error
   * envelope.
   *
   * @throws Neo4jError If Neo4j reports statement errors.
   * @throws std::runtime_error If the errors envelope is malformed.
   * @param statements Ordered statement and parameter DTOs.
   * @return Neo4j result tables.
   */
  nlohmann::json execute(const nlohmann::json &statements) const;
  /**
   * @brief Invoke retired n10s import and require complete triple loading.
   *
   * @throws std::runtime_error If n10s fails or parsed and loaded triple counts
   * differ.
   * @param uri Turtle location visible to Neo4j.
   */
  void importTurtle(const std::string &uri) const;

private:
  /// Shared retired HTTP transport.
  std::shared_ptr<IHttpClient> http_;
  /// Retired autocommit transaction endpoint.
  std::string endpoint_;
};
} // namespace entity_rdf
