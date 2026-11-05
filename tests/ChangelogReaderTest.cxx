#include "entity_rdf/ChangelogReader.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <fstream>
#include <gtest/gtest.h>
namespace entity_rdf {
class ChangelogReaderTest : public testing::Test {
protected:
  std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "entity-rdf-changelog-test";
  void SetUp() override { std::filesystem::create_directories(directory); }
  void TearDown() override { std::filesystem::remove_all(directory); }
  void write(const std::string &name, const std::string &text) {
    std::ofstream(directory / name) << text;
  }
  std::string checksum(const std::string &name) {
    return Encoding::sha256(Encoding::readFile((directory / name).string()));
  }
  void make(const std::string &data) {
    write("data.ttl", data);
    write("change.xml",
          "<graphChangeSet id=\"first\"><rdf file=\"data.ttl\" sha256=\"" +
              checksum("data.ttl") + "\"/></graphChangeSet>");
    write("root.xml", "<graphChangeLog format=\"rdf-changelog-v1\"><include "
                      "file=\"change.xml\" sha256=\"" +
                          checksum("change.xml") + "\"/></graphChangeLog>");
  }
  std::string nodeData() {
    Graph graph;
    graph.nodes.emplace("one",
                        NodeRecord{"one", {"Entity"}, {{"name", "one"}}});
    return TurtleWriter{}.write(graph);
  }
  ChangeLog read() { return ChangelogReader{}.read(directory / "root.xml"); }
};
TEST_F(ChangelogReaderTest, ShouldReadCheckedTurtleInputs) {
  make(nodeData());
  EXPECT_EQ(read().composed.nodes.size(), 1);
}
TEST_F(ChangelogReaderTest, ShouldRejectChangedInput) {
  make(nodeData());
  write("data.ttl", nodeData() + "# edited\n");
  EXPECT_THROW(read(), std::runtime_error);
}
TEST_F(ChangelogReaderTest, ShouldRejectChangedChangeset) {
  make(nodeData());
  write("change.xml", "<graphChangeSet id=\"edited\"/>");
  EXPECT_THROW(read(), std::runtime_error);
}
TEST_F(ChangelogReaderTest, ShouldRejectDtd) {
  write("root.xml", "<!DOCTYPE graphChangeLog [<!ENTITY x "
                    "'x'>]><graphChangeLog format=\"rdf-changelog-v1\"/>");
  EXPECT_THROW(read(), std::runtime_error);
}
TEST_F(ChangelogReaderTest, ShouldRejectUnknownConfigurationFields) {
  write("root.xml", "<graphChangeLog format=\"rdf-changelog-v1\" typo=\"x\"/>");
  EXPECT_THROW(read(), std::runtime_error);
}
TEST_F(ChangelogReaderTest, ShouldRejectUnresolvedRelationship) {
  Graph graph;
  graph.relationships.emplace(
      "edge", RelationshipRecord{"edge", "missing", "missing", "self",
                                 nlohmann::json::object()});
  make(TurtleWriter{}.write(graph));
  EXPECT_THROW(read(), std::runtime_error);
}
TEST_F(ChangelogReaderTest, ShouldResolveReferencesAcrossFiles) {
  make(nodeData());
  Graph edge;
  edge.relationships.emplace("edge",
                             RelationshipRecord{"edge", "one", "one", "self",
                                                nlohmann::json::object()});
  write("edges.ttl", TurtleWriter{}.write(edge));
  write("change.xml",
        "<graphChangeSet id=\"first\"><rdf file=\"edges.ttl\" sha256=\"" +
            checksum("edges.ttl") + "\"/><rdf file=\"data.ttl\" sha256=\"" +
            checksum("data.ttl") + "\"/></graphChangeSet>");
  write("root.xml", "<graphChangeLog format=\"rdf-changelog-v1\"><include "
                    "file=\"change.xml\" sha256=\"" +
                        checksum("change.xml") + "\"/></graphChangeLog>");
  EXPECT_EQ(read().composed.relationships.size(), 1);
}
} // namespace entity_rdf
