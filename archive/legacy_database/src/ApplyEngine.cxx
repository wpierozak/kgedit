#include "entity_rdf/ApplyEngine.hxx"
#include "entity_rdf/CypherBuilder.hxx"
#include "entity_rdf/GraphComparator.hxx"
#include "entity_rdf/Neo4jError.hxx"
#include "entity_rdf/SchemaManager.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
namespace entity_rdf {
ApplyEngine::ApplyEngine(std::shared_ptr<IHttpClient> http,
                         std::string endpoint)
    : http_(std::move(http)), endpoint_(std::move(endpoint)),
      client_(http_, endpoint_) {}
AppliedHistory ApplyEngine::decodeHistory(const nlohmann::json &results) {
  const nlohmann::json &rows = results.at(0).at("data");
  if (rows.empty())
    return {};
  if (rows.size() != 1)
    throw std::runtime_error("Multiple version ledgers");
  const nlohmann::json &row = rows.at(0).at("row");
  return {row.at(0).get<std::vector<std::string>>(),
          row.at(1).get<std::vector<std::string>>()};
}
AppliedHistory ApplyEngine::history() const {
  return decodeHistory(client_.execute(nlohmann::json::array(
      {CypherBuilder::statement("MATCH (l:_VERSIONING_ {name:'entity-rdf'}) "
                                "RETURN l.ids,l.checksums")})));
}
void ApplyEngine::initialize() const {
  // Never adopt an unmanaged populated database by guessing application
  // identities.
  nlohmann::json rows =
      client_.execute(nlohmann::json::array({CypherBuilder::statement(
          "MATCH (n) WHERE (NOT n:Resource OR NOT coalesce(n.uri STARTS WITH "
          "'urn:dcs:stage:',false)) AND NOT n:_GraphConfig AND NOT "
          "n:_NsPrefDef AND NOT n:_VERSIONING_ RETURN count(n)")}));
  if (history().ids.empty() &&
      rows.at(0).at("data").at(0).at("row").at(0).get<std::size_t>() != 0)
    throw std::runtime_error(
        "Unmanaged populated database: refusing automatic adoption");
  SchemaManager{client_}.ensure(SchemaManager::infrastructure());
  nlohmann::json config = client_.execute(nlohmann::json::array(
      {CypherBuilder::statement("MATCH (n:_GraphConfig) RETURN count(n)")}));
  if (config.at(0).at("data").at(0).at("row").at(0) == 0)
    client_.execute(nlohmann::json::array({CypherBuilder::statement(
        "CALL "
        "n10s.graphconfig.init({handleVocabUris:'KEEP',handleRDFTypes:'NODES',"
        "keepCustomDataTypes:true})")}));
  nlohmann::json settings = client_.execute(nlohmann::json::array(
      {CypherBuilder::statement("CALL n10s.graphconfig.show() YIELD "
                                "param,value RETURN param,value")}));
  nlohmann::json actual = nlohmann::json::object();
  for (const nlohmann::json &row : settings.at(0).at("data"))
    actual[row.at("row").at(0).get<std::string>()] = row.at("row").at(1);
  if (actual.value("handleVocabUris", "") != "KEEP" ||
      actual.value("handleRDFTypes", "") != "NODES" ||
      !actual.value("keepCustomDataTypes", false))
    throw std::runtime_error(
        "Incompatible n10s graph configuration; refusing to change it");
}
void ApplyEngine::stage(const ChangeSet &change,
                        const std::string &prefix) const {
  for (std::size_t start = 0; start < change.document.size(); start += 5000) {
    std::span<const RdfTriple> batch =
        std::span<const RdfTriple>(change.document)
            .subspan(start, std::min<std::size_t>(5000, change.document.size() -
                                                            start));
    std::string turtle = TurtleWriter{}.staging(batch, prefix);
    nlohmann::json result =
        client_.execute(nlohmann::json::array({CypherBuilder::statement(
            "CALL n10s.rdf.import.inline($turtle,'Turtle') YIELD "
            "terminationStatus,triplesLoaded,triplesParsed RETURN "
            "terminationStatus,triplesLoaded,triplesParsed",
            {{"turtle", turtle}})}));
    const nlohmann::json &rows = result.at(0).at("data");
    if (rows.size() != 1 || rows.at(0).at("row").at(0) != "OK" ||
        rows.at(0).at("row").at(1) != rows.at(0).at("row").at(2) ||
        rows.at(0).at("row").at(2) != batch.size())
      throw std::runtime_error(
          "Incomplete n10s import; application and ledger unchanged");
  }
}
void ApplyEngine::cleanStage(const std::string &prefix) const {
  client_.execute(nlohmann::json::array({CypherBuilder::statement(
      "MATCH (n:Resource) WHERE n.uri STARTS WITH $prefix DETACH DELETE n",
      {{"prefix", prefix}})}));
}
void ApplyEngine::executeBatches(Transaction &tx,
                                 const nlohmann::json &statements) {
  for (std::size_t start = 0; start < statements.size(); start += 200) {
    nlohmann::json batch = nlohmann::json::array();
    for (std::size_t i = start; i < std::min(start + 200, statements.size());
         ++i)
      batch.push_back(statements[i]);
    tx.execute(batch);
  }
}
void ApplyEngine::replaceLabels(Transaction &tx, const Graph &graph) const {
  nlohmann::json ids = nlohmann::json::array();
  for (const auto &[id, node] : graph.nodes)
    ids.push_back(id);
  nlohmann::json result = tx.execute(nlohmann::json::array(
      {CypherBuilder::statement("MATCH (n:_DATASET_) WHERE n._dataset_id IN "
                                "$ids RETURN n._dataset_id,labels(n)",
                                {{"ids", ids}})}));
  nlohmann::json removals = nlohmann::json::array();
  for (const nlohmann::json &row : result.at(0).at("data"))
    for (const std::string &label :
         row.at("row").at(1).get<std::vector<std::string>>())
      if (label != Vocabulary::DatasetLabel)
        removals.push_back(CypherBuilder::statement(
            "MATCH (n:_DATASET_ {_dataset_id:$id}) REMOVE n:" +
                Encoding::quote(label),
            {{"id", row.at("row").at(0)}}));
  executeBatches(tx, removals);
}
bool ApplyEngine::applyOne(const ChangeLog &log, std::size_t index) const {
  for (unsigned int attempt = 0;; ++attempt) {
    try {
      return applyOneAttempt(log, index);
    } catch (const Neo4jError &error) {
      if (!error.retryable() || attempt >= 5)
        throw;
      // Neo4j aborts transiently failed transactions; recheck the ledger under
      // a fresh lock.
      std::this_thread::sleep_for(
          std::chrono::milliseconds(50 * (attempt + 1)));
    }
  }
}
bool ApplyEngine::applyOneAttempt(const ChangeLog &log,
                                  std::size_t index) const {
  const ChangeSet &change = log.changes.at(index);
  Transaction tx(http_, endpoint_);
  // Updating a dependent property acquires a Neo4j write lock before reading
  // history.
  AppliedHistory applied =
      decodeHistory(tx.execute(nlohmann::json::array({CypherBuilder::statement(
          "MERGE (l:_VERSIONING_ {name:'entity-rdf'}) ON CREATE SET "
          "l.ids=[],l.checksums=[] SET l.lock=coalesce(l.lock,0)+1 RETURN "
          "l.ids,l.checksums")})));
  std::size_t next = History::pending(log, applied);
  if (next > index) {
    tx.commit();
    return false;
  }
  if (next != index)
    throw std::runtime_error("Changeset order changed concurrently");
  if (next == 0) {
    nlohmann::json count =
        tx.execute(nlohmann::json::array({CypherBuilder::statement(
            "MATCH (n) WHERE (NOT n:Resource OR NOT coalesce(n.uri STARTS WITH "
            "'urn:dcs:stage:',false)) AND NOT n:_GraphConfig AND NOT "
            "n:_NsPrefDef AND NOT n:_VERSIONING_ RETURN count(n)")}));
    if (count.at(0).at("data").at(0).at("row").at(0) != 0)
      throw std::runtime_error(
          "Database became populated before baseline application");
  }
  replaceLabels(tx, change.graph);
  executeBatches(tx, CypherBuilder{}.nodes(change.graph));
  nlohmann::json edgeStatements = CypherBuilder{}.relationships(change.graph);
  nlohmann::json edgeIds = nlohmann::json::array();
  for (const auto &[id, edge] : change.graph.relationships)
    edgeIds.push_back(id);
  tx.execute(nlohmann::json::array({CypherBuilder::statement(
      "MATCH ()-[r]->() WHERE r._dataset_id IN $ids DELETE r",
      {{"ids", edgeIds}})}));
  for (std::size_t start = 0; start < edgeStatements.size(); start += 200) {
    nlohmann::json batch = nlohmann::json::array();
    for (std::size_t i = start;
         i < std::min(start + 200, edgeStatements.size()); ++i)
      batch.push_back(edgeStatements[i]);
    nlohmann::json result = tx.execute(batch);
    for (const nlohmann::json &item : result)
      if (item.at("data").at(0).at("row").at(0) != 1)
        throw std::runtime_error(
            "Missing relationship endpoint; rolling back changeset");
  }
  tx.execute(nlohmann::json::array({CypherBuilder::statement(
      "MATCH (l:_VERSIONING_ {name:'entity-rdf'}) SET "
      "l.ids=l.ids+$id,l.checksums=l.checksums+$checksum",
      {{"id", change.id}, {"checksum", change.checksum}})}));
  tx.commit();
  return true;
}
std::size_t ApplyEngine::apply(const ChangeLog &log) const {
  std::size_t start = History::pending(log, history());
  // Validate projection support before making database writes.
  for (const ChangeSet &change : log.changes) {
    CypherBuilder{}.nodes(change.graph);
    CypherBuilder{}.relationships(change.graph);
  }
  if (start == log.changes.size())
    return 0;
  initialize();
  std::size_t count = 0;
  for (std::size_t i = start; i < log.changes.size(); ++i) {
    std::random_device random;
    std::string nonce = std::to_string(random()) + std::to_string(random());
    std::string prefix =
        "urn:dcs:stage:" + log.changes[i].checksum + ":" + nonce + ":";
    try {
      stage(log.changes[i], prefix);
    } catch (const std::exception &error) {
      throw std::runtime_error(std::string(error.what()) +
                               "; staging prefix: " + prefix);
    }
    SchemaManager{client_}.ensure(log.changes[i].schema);
    if (applyOne(log, i))
      ++count;
    try {
      cleanStage(prefix);
    } catch (const std::exception &error) {
      std::cerr << "Changeset committed; staging cleanup pending: "
                << error.what() << '\n';
    }
  }
  return count;
}
void ApplyEngine::compare(const ChangeLog &log) const {
  if (History::pending(log, history()) != log.changes.size())
    throw std::runtime_error("Changelog has unapplied changesets");
  nlohmann::json schema = nlohmann::json::array();
  for (const ChangeSet &change : log.changes)
    for (const nlohmann::json &item : change.schema)
      schema.push_back(item);
  SchemaManager{client_}.compare(schema);
  GraphComparator{client_}.compare(log.composed);
}
} // namespace entity_rdf
