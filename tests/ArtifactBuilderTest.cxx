#include "entity_rdf/ArtifactBuilder.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include "entity_rdf/XmlReader.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class ArtifactBuilderTest : public testing::Test {
protected:
  std::filesystem::path output =
      std::filesystem::temp_directory_path() / "entity-rdf-artifact-test";
  ChangeLog log;
  void SetUp() override {
    log.composed.nodes.emplace(
        "board",
        NodeRecord{"board",
                   {"Board`$Label"},
                   {{"name", "quote '\"; MATCH (n) DELETE n; \\ \n"}}});
    log.composed.relationships.emplace(
        "loop",
        RelationshipRecord{"loop", "board", "board", "self", {{"order", 3}}});
    GraphMapper::canonicalize(log.composed);
    log.changes.push_back({"first", "source-checksum"});
  }
  void TearDown() override {
    std::filesystem::remove_all(output);
    std::filesystem::remove_all(output.string() + ".pending");
  }
  void build() { ArtifactBuilder{}.build(log, output, "file:///combined.ttl"); }
};
TEST_F(ArtifactBuilderTest,
       ShouldProduceDeterministicCompleteBundleWithChecksums) {
  build();
  std::string turtle = Encoding::readFile((output / "combined.ttl").string());
  std::string script = Encoding::readFile((output / "import.cypher").string());
  XmlElement manifest = XmlReader{}.read(output / "manifest.xml");
  EXPECT_EQ(manifest.attributes.at("nodes"), "1");
  EXPECT_EQ(manifest.attributes.at("relationships"), "1");
  EXPECT_EQ(manifest.children.at(1).attributes.at("sha256"),
            Encoding::sha256(turtle));
  EXPECT_EQ(manifest.children.at(2).attributes.at("sha256"),
            Encoding::sha256(script));
  EXPECT_EQ(std::stoul(manifest.attributes.at("triples")),
            TurtleReader{}.read(output / "combined.ttl").size());
  EXPECT_NE(script.find("n10s.rdf.import.fetch"), std::string::npos);
  EXPECT_NE(script.find(":begin\n"), std::string::npos);
  EXPECT_NE(script.find("`Board``$Label`"), std::string::npos);
  EXPECT_NE(script.find("input.properties"), std::string::npos);
  EXPECT_EQ(script.find("$properties"), std::string::npos);
  EXPECT_THROW(build(), std::runtime_error);
  std::filesystem::path second = output.string() + "-second";
  ArtifactBuilder{}.build(log, second, "file:///combined.ttl");
  EXPECT_EQ(Encoding::readFile((second / "import.cypher").string()), script);
  EXPECT_EQ(Encoding::readFile((second / "manifest.xml").string()),
            Encoding::readFile((output / "manifest.xml").string()));
  std::filesystem::remove_all(second);
}
TEST_F(ArtifactBuilderTest,
       ShouldRejectUnsupportedValuesBeforePublishingFiles) {
  log.composed.nodes.at("board").properties["bytes"] = {{"@type", "bytes"},
                                                        {"value", "ff00"}};
  EXPECT_THROW(build(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(output));
  EXPECT_FALSE(std::filesystem::exists(output.string() + ".pending"));
}
TEST_F(ArtifactBuilderTest,
       ShouldRejectMissingEndpointsAndRelativeImportLocations) {
  log.composed.relationships.at("loop").target = "missing";
  EXPECT_THROW(build(), std::runtime_error);
  log.composed.relationships.at("loop").target = "board";
  EXPECT_THROW(ArtifactBuilder{}.build(log, output, "combined.ttl"),
               std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(output));
}
} // namespace entity_rdf
