#include "entity_rdf/Transaction.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class TransactionTest : public testing::Test {
protected:
  class Http final : public IHttpClient {
  public:
    mutable std::vector<std::string> calls;
    bool fail = false;
    HttpResponse post(std::string_view url, std::string_view) const override {
      calls.emplace_back(url);
      if (url.ends_with("/tx"))
        return {
            201,
            R"({"commit":"http://untrusted/db/neo4j/tx/42/commit","errors":[],"results":[]})"};
      if (fail)
        return {200, R"({"errors":[{"message":"failed"}],"results":[]})"};
      return {200, R"({"errors":[],"results":[]})"};
    }
    void remove(std::string_view url) const override {
      calls.emplace_back("DELETE " + std::string(url));
    }
  };
  std::shared_ptr<Http> http = std::make_shared<Http>();
  const std::string endpoint = "http://localhost/db/neo4j/tx/commit";
};
TEST_F(TransactionTest, ShouldCommitUsingConfiguredOrigin) {
  {
    Transaction transaction(http, endpoint);
    transaction.commit();
  }
  EXPECT_EQ(http->calls.at(1), "http://localhost/db/neo4j/tx/42/commit");
  EXPECT_EQ(http->calls.size(), 2);
}
TEST_F(TransactionTest, ShouldRollbackUncommittedTransaction) {
  { Transaction transaction(http, endpoint); }
  EXPECT_EQ(http->calls.back(), "DELETE http://localhost/db/neo4j/tx/42");
}
TEST_F(TransactionTest, ShouldRollbackAfterCypherFailure) {
  {
    Transaction transaction(http, endpoint);
    http->fail = true;
    EXPECT_THROW(transaction.execute(nlohmann::json::array()),
                 std::runtime_error);
  }
  EXPECT_EQ(http->calls.back(), "DELETE http://localhost/db/neo4j/tx/42");
}
} // namespace entity_rdf
