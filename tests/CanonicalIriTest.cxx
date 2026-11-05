#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include <fstream>
#include <gtest/gtest.h>
namespace entity_rdf {
class CanonicalIriTest : public testing::Test {
protected:
  std::filesystem::path path =
      std::filesystem::temp_directory_path() / "canonical-iri-test.ttl";
  Graph graph;
  void SetUp() override {
    graph.nodes.emplace("board",
                        NodeRecord{"board",
                                   {"Board"},
                                   {{"iri", "urn:alice:board:PMA0"},
                                    {"entity_identity", "unchanged-uuid"}}});
    graph.nodes.emplace(
        "parameter",
        NodeRecord{"parameter", {"Parameter"}, {{"name", "power"}}});
    graph.relationships.emplace(
        "link",
        RelationshipRecord{
            "link", "board", "parameter", "has_parameter", {{"rank", 42}}});
    GraphMapper::canonicalize(graph);
  }
  void TearDown() override { std::filesystem::remove(path); }
  Graph decode(const std::string &text) {
    std::ofstream(path) << text;
    return GraphMapper{}.decode(TurtleReader{}.read(path));
  }
  Graph roundtrip(const Graph &input) {
    return decode(TurtleWriter{}.write(input));
  }
};
TEST_F(CanonicalIriTest, ShouldReuseExistingIriAndPreserveIdentifiers) {
  Graph result = roundtrip(graph);
  EXPECT_EQ(result.nodes.at("board").iri, "urn:alice:board:PMA0");
  EXPECT_EQ(result.nodes.at("board").id, "board");
  EXPECT_EQ(result.nodes.at("board").properties.at("entity_identity"),
            "unchanged-uuid");
  EXPECT_EQ(result.relationships.at("link").source, "board");
  GraphMapper::validate(result);
}
TEST_F(CanonicalIriTest, ShouldMintMissingIrisOnceAndUsePrefixes) {
  EXPECT_EQ(graph.nodes.at("parameter").iri, "urn:alice:dcs:node:parameter");
  EXPECT_EQ(graph.relationships.at("link").iri,
            "urn:alice:dcs:relationship:link");
  std::string expected = TurtleWriter{}.write(graph);
  GraphMapper::canonicalize(graph);
  EXPECT_EQ(TurtleWriter{}.write(graph), expected);
  EXPECT_NE(expected.find("g:recordId"), std::string::npos);
  EXPECT_NE(expected.find("^^xsd:string"), std::string::npos);
  EXPECT_NE(expected.find(" a g:Node"), std::string::npos);
  EXPECT_EQ(TurtleWriter{}.write(roundtrip(graph)), expected);
}
TEST_F(CanonicalIriTest,
       ShouldResolveEndpointIrisAcrossFilesAndEarlierChanges) {
  Graph nodes = graph;
  nodes.relationships.clear();
  Graph edges = graph;
  edges.nodes.clear();
  Graph result = roundtrip(edges);
  EXPECT_THROW(GraphMapper::validate(result), std::runtime_error);
  GraphMapper::overlay(result, roundtrip(nodes));
  GraphMapper::validate(result);
  EXPECT_EQ(result.relationships.at("link").target, "parameter");
  GraphMapper::overlay(nodes, roundtrip(edges));
  GraphMapper::validate(nodes);
}
TEST_F(CanonicalIriTest, ShouldReplaceCompleteDefinitionForSameIri) {
  Graph change;
  NodeRecord replacement = graph.nodes.at("board");
  replacement.labels = {"Updated"};
  replacement.properties = {{"iri", replacement.iri}, {"name", "new"}};
  change.nodes.emplace(replacement.id, replacement);
  GraphMapper::overlay(graph, change);
  EXPECT_EQ(graph.nodes.at("board").labels, std::set<std::string>{"Updated"});
  EXPECT_FALSE(graph.nodes.at("board").properties.contains("entity_identity"));
}
TEST_F(CanonicalIriTest, ShouldRejectIdentityReassignmentAndDuplicateIris) {
  Graph change;
  NodeRecord replacement = graph.nodes.at("board");
  replacement.id = "different-id";
  change.nodes.emplace(replacement.id, replacement);
  EXPECT_THROW(GraphMapper::overlay(graph, change), std::runtime_error);
  change.nodes.clear();
  replacement = graph.nodes.at("board");
  replacement.iri = "urn:changed";
  replacement.properties["iri"] = replacement.iri;
  change.nodes.emplace(replacement.id, replacement);
  EXPECT_THROW(GraphMapper::overlay(graph, change), std::runtime_error);
}
TEST_F(CanonicalIriTest,
       ShouldRejectSubjectPropertyMismatchAndMissingRecordId) {
  graph.nodes.at("board").properties["iri"] = "urn:different";
  EXPECT_THROW(roundtrip(graph), std::runtime_error);
  EXPECT_THROW(decode("<urn:alice:board:PMA0> a <urn:dcs:graph:Node> ."),
               std::runtime_error);
}
TEST_F(CanonicalIriTest, ShouldPreserveParallelEdgesAndSelfLoops) {
  RelationshipRecord edge = graph.relationships.at("link");
  edge.id = "second";
  edge.iri = "urn:alice:dcs:relationship:second";
  edge.properties["iri"] = edge.iri;
  graph.relationships.emplace(edge.id, edge);
  graph.relationships.at("link").targetIri = graph.nodes.at("board").iri;
  Graph result = roundtrip(graph);
  EXPECT_EQ(result.relationships.size(), 2);
  EXPECT_EQ(result.relationships.at("link").target, "board");
  EXPECT_EQ(result.relationships.at("second").target, "parameter");
}
} // namespace entity_rdf

namespace entity_rdf {
TEST_F(CanonicalIriTest, ShouldRejectInvalidCanonicalIrisBeforeWritingTurtle) {
  graph.nodes.at("board").iri = "urn:bad> . <urn:injected>";
  graph.nodes.at("board").properties["iri"] = graph.nodes.at("board").iri;
  EXPECT_THROW(TurtleWriter{}.write(graph), std::exception);
}
} // namespace entity_rdf
