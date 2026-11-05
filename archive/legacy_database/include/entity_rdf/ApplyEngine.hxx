#pragma once
#include "entity_rdf/ChangelogDto.hxx"
#include "entity_rdf/History.hxx"
#include "entity_rdf/Neo4jClient.hxx"
#include "entity_rdf/Transaction.hxx"
namespace entity_rdf {
/**
 * @brief Retired executor for staged RDF projection and transactional changeset
 * ledgers.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class ApplyEngine {
public:
  /**
   * @brief Bind the retired executor to an HTTP transport and endpoint.
   * @param http Shared transport implementation.
   * @param endpoint Neo4j transactional commit endpoint.
   */
  ApplyEngine(std::shared_ptr<IHttpClient> http, std::string endpoint);
  /**
   * @brief Apply the unapplied suffix of the retired managed changelog.
   *
   * @throws std::runtime_error If history, staging, schema or transactional
   * projection fails.
   * @param log Validated changelog matching the database ledger prefix.
   * @return Number of changesets committed by this invocation.
   */
  std::size_t apply(const ChangeLog &log) const;
  /**
   * @brief Verify the retired ledger, schema and complete application graph.
   *
   * @throws std::runtime_error If unapplied changes or database drift are
   * detected.
   * @param log Expected composed changelog.
   */
  void compare(const ChangeLog &log) const;

private:
  /**
   * @brief Read the retired database changeset ledger.
   * @return Ordered applied IDs and checksums; empty when no ledger exists.
   */
  AppliedHistory history() const;
  /**
   * @brief Decode exactly one retired version ledger result.
   *
   * @throws std::runtime_error If multiple ledgers are returned.
   * @param results Neo4j result tables for the ledger query.
   * @return Decoded history, or an empty history if the query has no rows.
   */
  static AppliedHistory decodeHistory(const nlohmann::json &results);
  /**
   * @brief Initialize retired infrastructure only for an empty unmanaged
   * target.
   *
   * @throws std::runtime_error If a populated unmanaged target or incompatible
   * n10s configuration is found.
   */
  void initialize() const;
  /**
   * @brief Import a changeset into isolated retired n10s staging resources.
   *
   * @throws std::runtime_error If any batch fails or imports an incomplete set
   * of triples.
   * @param change Validated triples to stage.
   * @param prefix Invocation-specific prefix applied to every resource IRI.
   */
  void stage(const ChangeSet &change, const std::string &prefix) const;
  /**
   * @brief Delete only retired staging resources belonging to one invocation.
   * @param prefix Invocation-specific staging resource prefix.
   */
  void cleanStage(const std::string &prefix) const;
  /**
   * @brief Apply one changeset while holding the retired ledger write lock.
   *
   * Graph replacement and ledger advancement share one transaction.
   * @param log Validated changelog.
   * @param index Changeset index expected to follow the applied ledger prefix.
   * @return True if committed here; false if another applier already committed
   * it.
   */
  bool applyOneAttempt(const ChangeLog &log, std::size_t index) const;
  /**
   * @brief Retry a retired changeset transaction after bounded transient
   * failures.
   * @param log Validated changelog.
   * @param index Changeset index to apply.
   * @return True if this invocation commits the changeset; false if it was
   * already applied.
   */
  bool applyOne(const ChangeLog &log, std::size_t index) const;
  /**
   * @brief Remove prior application labels while retaining the management
   * label.
   * @param[in,out] tx Retired transaction receiving removal queries.
   * @param graph Changed nodes identified by preserved record IDs.
   */
  void replaceLabels(Transaction &tx, const Graph &graph) const;
  /**
   * @brief Execute retired statement batches inside the same transaction.
   * @param[in,out] tx Transaction receiving the statements.
   * @param statements Ordered Neo4j statement envelopes.
   */
  static void executeBatches(Transaction &tx, const nlohmann::json &statements);
  /// Shared retired HTTP transport.
  std::shared_ptr<IHttpClient> http_;
  /// Retired transaction commit endpoint.
  std::string endpoint_;
  /// Retired autocommit database adapter.
  Neo4jClient client_;
};
} // namespace entity_rdf
