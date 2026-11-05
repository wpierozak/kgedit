#include "entity_rdf/ApplyEngine.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class ApplyEngineTest : public testing::Test {
protected:
  class Http final : public IHttpClient {
  public:
    mutable std::vector<std::string> queries;
    bool populated = true;
    bool applied = false;
    HttpResponse post(std::string_view, std::string_view body) const override {
      nlohmann::json request = nlohmann::json::parse(body);
      std::string query = request.at("statements").at(0).at("statement");
      queries.push_back(query);
      nlohmann::json rows = nlohmann::json::array();
      if (query.find("RETURN l.ids") != std::string::npos) {
        if (applied)
          rows.push_back({{"row", nlohmann::json::array(
                                      {nlohmann::json::array({"first"}),
                                       nlohmann::json::array({"aaa"})})}});
      } else
        rows.push_back({{"row", nlohmann::json::array({populated ? 1 : 0})}});
      return {200, nlohmann::json{{"errors", nlohmann::json::array()},
                                  {"results",
                                   nlohmann::json::array({{{"data", rows}}})}}
                       .dump()};
    }
  };
  std::shared_ptr<Http> http = std::make_shared<Http>();
  ChangeLog log;
  void SetUp() override {
    ChangeSet change;
    change.id = "first";
    change.checksum = "aaa";
    log.changes.push_back(change);
  }
};
TEST_F(ApplyEngineTest, ShouldRefuseUnmanagedDatabaseBeforeWrites) {
  ApplyEngine engine(http, "http://test/db/neo4j/tx/commit");
  EXPECT_THROW(engine.apply(log), std::runtime_error);
  ASSERT_EQ(http->queries.size(), 3);
  for (const std::string &query : http->queries)
    EXPECT_EQ(query.find("CREATE"), std::string::npos);
}
TEST_F(ApplyEngineTest, ShouldSkipAlreadyAppliedChangesetsBeforeStaging) {
  http->applied = true;
  ApplyEngine engine(http, "http://test/db/neo4j/tx/commit");
  EXPECT_EQ(engine.apply(log), 0);
  EXPECT_EQ(http->queries.size(), 1);
}
} // namespace entity_rdf
