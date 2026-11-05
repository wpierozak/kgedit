#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace entity_rdf {
/**
 * @brief Defines response envelope keys used by the retired Neo4j HTTP adapter.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
struct HttpFields {
  /// Cypher statement envelope key.
  static constexpr const char *Statements = "statements";
  /// Neo4j error envelope key.
  static constexpr const char *Errors = "errors";
  /// Neo4j result envelope key.
  static constexpr const char *Results = "results";
  /// Neo4j error-code key.
  static constexpr const char *Code = "code";
};
/**
 * @brief Retired transaction exception carrying Neo4j transient-error
 * classification.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class Neo4jError : public std::runtime_error {
public:
  /**
   * @brief Capture a retired Neo4j error array and classify its first error.
   * @param errors Neo4j errors envelope containing code fields.
   */
  explicit Neo4jError(const nlohmann::json &errors)
      : std::runtime_error("Neo4j transaction failed: " + errors.dump()),
        retryable_(!errors.empty() && errors.front()
                                          .value(HttpFields::Code, "")
                                          .starts_with("Neo.TransientError.")) {
  }
  /**
   * @brief Report whether the first retired error has a transient Neo4j code.
   * @return True if the code starts with Neo.TransientError.; false otherwise.
   */
  bool retryable() const { return retryable_; }

private:
  /// Whether the first Neo4j error is classified as transient.
  bool retryable_;
};
} // namespace entity_rdf
