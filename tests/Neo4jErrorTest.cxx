#include "entity_rdf/Neo4jError.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
TEST(Neo4jErrorTest, ShouldRetryOnlyTransientNeo4jErrors) {
  EXPECT_TRUE(
      Neo4jError(
          nlohmann::json::array(
              {{{"code", "Neo.TransientError.Transaction.DeadlockDetected"}}}))
          .retryable());
  EXPECT_FALSE(
      Neo4jError(nlohmann::json::array(
                     {{{"code",
                        "Neo.ClientError.Schema.ConstraintValidationFailed"}}}))
          .retryable());
}
} // namespace entity_rdf
