#pragma once
#include "entity_rdf/HttpClient.hxx"
#include <memory>
#include <nlohmann/json.hpp>
namespace entity_rdf {
/**
 * @brief Retired RAII owner of an explicit Neo4j HTTP transaction.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class Transaction {
public:
  /**
   * @brief Open a retired explicit Neo4j transaction through an injected
   * transport.
   *
   * @throws std::runtime_error If the endpoint or returned transaction URL is
   * invalid.
   * @param http Non-null shared HTTP transport.
   * @param commitEndpoint Neo4j URL ending in /tx/commit.
   */
  Transaction(std::shared_ptr<IHttpClient> http,
              const std::string &commitEndpoint);
  /**
   * @brief Attempt rollback of an uncommitted retired transaction without
   * throwing.
   */
  ~Transaction();
  /** @brief Copy construction is prohibited for transaction owners. */
  Transaction(const Transaction &) = delete;
  /** @brief Copy assignment is prohibited for transaction owners. */
  Transaction &operator=(const Transaction &) = delete;
  /**
   * @brief Execute statements inside the open retired transaction.
   *
   * @throws Neo4jError If Neo4j reports a transaction error.
   * @param statements Ordered statement DTOs.
   * @return Neo4j result tables.
   */
  nlohmann::json execute(const nlohmann::json &statements);
  /**
   * @brief Commit the retired transaction and suppress destructor rollback on
   * success.
   *
   * @throws Neo4jError If Neo4j rejects the commit.
   */
  void commit();

private:
  /**
   * @brief Send and validate a retired transactional HTTP request.
   *
   * @throws std::runtime_error If HTTP status or response structure is invalid.
   * @throws Neo4jError If Neo4j reports statement errors.
   * @param url Transaction or commit URL.
   * @param statements Ordered statement DTOs.
   * @return Decoded Neo4j response envelope.
   */
  nlohmann::json post(const std::string &url,
                      const nlohmann::json &statements) const;
  /// Shared retired HTTP transport.
  std::shared_ptr<IHttpClient> http_;
  /// Validated URL of the open transaction.
  std::string base_;
  /// Whether commit completed successfully.
  bool committed_ = false;
};
} // namespace entity_rdf
