#include "entity_rdf/CypherBuilder.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class CypherBuilderTest : public testing::Test {
protected:
  Graph graph;
  void SetUp() override {
    graph.nodes.emplace(
        "one", NodeRecord{"one", {"Odd`Label"}, {{"name", "not ' Cypher"}}});
  }
};
TEST_F(CypherBuilderTest, ShouldParameterizePropertyValuesAndEscapeLabels) {
  nlohmann::json result = CypherBuilder{}.nodes(graph);
  EXPECT_EQ(result.at(0).at("parameters").at("properties").at("name"),
            "not ' Cypher");
  EXPECT_NE(
      result.at(0).at("statement").get<std::string>().find("`Odd``Label`"),
      std::string::npos);
  EXPECT_EQ(
      result.at(0).at("statement").get<std::string>().find("not ' Cypher"),
      std::string::npos);
}
TEST_F(CypherBuilderTest, ShouldRejectByteArrayBeforeDatabaseWrites) {
  graph.nodes.at("one").properties["payload"] = {{"@type", "bytes"},
                                                 {"value", "ff00"}};
  EXPECT_THROW(CypherBuilder{}.nodes(graph), std::runtime_error);
}
} // namespace entity_rdf
