#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <fstream>
#include <gtest/gtest.h>
namespace entity_rdf {
class GraphMapperTest : public testing::Test {
protected:
  std::filesystem::path path =
      std::filesystem::temp_directory_path() / "entity-rdf-graph-test.ttl";
  void TearDown() override { std::filesystem::remove(path); }
  Graph example() const {
    Graph graph;
    graph.nodes.emplace(
        "one", NodeRecord{"one",
                          {"Entity", "Board"},
                          {{"name", "one"},
                           {"numbers", nlohmann::json::array({2, 1, 2})},
                           {"enabled", true}}});
    graph.nodes.emplace("two",
                        NodeRecord{"two", {"Entity"}, {{"name", "two"}}});
    for (const std::string &id : {"first", "second"})
      graph.relationships.emplace(
          id, RelationshipRecord{id, "one", "two", "connects", {{"rank", 42}}});
    return graph;
  }
  Graph roundtrip(const Graph &graph) {
    std::ofstream(path) << TurtleWriter{}.write(graph);
    return GraphMapper{}.decode(TurtleReader{}.read(path));
  }
};
TEST_F(GraphMapperTest, ShouldPreserveParallelEdgesAndArrayOrder) {
  Graph graph = roundtrip(example());
  GraphMapper::validate(graph);
  EXPECT_EQ(graph.relationships.size(), 2);
  EXPECT_EQ(graph.nodes.at("one").properties.at("numbers"),
            nlohmann::json::array({2, 1, 2}));
  EXPECT_EQ(graph.relationships.at("second").properties.at("rank"), 42);
}
TEST_F(GraphMapperTest, ShouldReplaceWholeRecordByIdentity) {
  Graph graph = example();
  Graph change;
  change.nodes.emplace("one",
                       NodeRecord{"one", {"Renamed"}, {{"name", "new"}}});
  GraphMapper::overlay(graph, change);
  EXPECT_EQ(graph.nodes.at("one").labels, std::set<std::string>{"Renamed"});
  EXPECT_EQ(graph.nodes.at("one").properties,
            nlohmann::json({{"name", "new"}}));
  EXPECT_EQ(graph.relationships.size(), 2);
}
TEST_F(GraphMapperTest, ShouldRejectMissingEndpointAfterComposition) {
  Graph graph = example();
  graph.nodes.erase("two");
  EXPECT_THROW(GraphMapper::validate(graph), std::runtime_error);
}
TEST_F(GraphMapperTest, ShouldPreserveSelfLoop) {
  Graph graph = example();
  graph.relationships.at("first").target = "one";
  EXPECT_EQ(roundtrip(graph).relationships.at("first").target, "one");
}
TEST_F(GraphMapperTest, ShouldRejectReservedTransportProperty) {
  Graph graph = example();
  graph.nodes.at("one").properties["_dataset_id"] = "spoof";
  EXPECT_THROW(roundtrip(graph), std::runtime_error);
}
TEST_F(GraphMapperTest, ShouldWriteDeterministicTurtle) {
  EXPECT_EQ(TurtleWriter{}.write(example()),
            TurtleWriter{}.write(roundtrip(example())));
}
TEST_F(GraphMapperTest, ShouldPreserveUnicodeAndEmptyArray) {
  Graph graph = example();
  graph.nodes.at("one").properties["name"] = "📡";
  graph.nodes.at("one").properties["empty"] = nlohmann::json::array();
  EXPECT_EQ(roundtrip(graph).nodes.at("one").properties,
            graph.nodes.at("one").properties);
}
} // namespace entity_rdf
