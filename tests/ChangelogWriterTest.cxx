#include "entity_rdf/ChangelogWriter.hxx"
#include "entity_rdf/ChangelogReader.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include "entity_rdf/XmlWriter.hxx"
#include <fstream>
#include <gtest/gtest.h>
namespace entity_rdf {
class ChangelogWriterTest : public testing::Test {
protected:
  std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "entity-rdf-publish-test";
  void SetUp() override {
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "first.ttl")
        << "<urn:dcs:node:one> a <urn:dcs:graph:Node>; <urn:dcs:graph:label> "
           "\"Original\" .";
    ChangelogWriter{}.create(directory / "first", "first",
                             directory / "first.ttl");
    XmlElement root{
        "graphChangeLog",
        {{"format", "rdf-changelog-v1"}},
        {{"include",
          {{"file", "first/changelog.xml"},
           {"sha256", Encoding::sha256(Encoding::readFile(
                          (directory / "first/changelog.xml").string()))}},
          {}}}};
    std::ofstream(directory / "root.xml") << XmlWriter{}.write(root);
  }
  void TearDown() override { std::filesystem::remove_all(directory); }
  void second(const std::string &turtle) {
    std::ofstream(directory / "second.ttl") << turtle;
    ChangelogWriter{}.create(directory / "second", "second",
                             directory / "second.ttl");
  }
};
TEST_F(ChangelogWriterTest,
       ShouldAppendValidatedChangesetWithoutEditingInputs) {
  second("<urn:dcs:node:one> a <urn:dcs:graph:Node>; <urn:dcs:graph:label> "
         "\"Updated\" .");
  std::string frozen =
      Encoding::readFile((directory / "first/changelog.xml").string());
  ChangelogWriter{}.append(directory / "root.xml",
                           directory / "second/changelog.xml");
  ChangeLog log = ChangelogReader{}.read(directory / "root.xml");
  EXPECT_EQ(log.changes.size(), 2);
  EXPECT_EQ(log.composed.nodes.at("one").labels,
            std::set<std::string>{"Updated"});
  EXPECT_EQ(Encoding::readFile((directory / "first/changelog.xml").string()),
            frozen);
}
TEST_F(ChangelogWriterTest,
       ShouldKeepRootUnchangedWhenCandidateHasInvalidReferences) {
  second(
      "<urn:dcs:relationship:bad> a <urn:dcs:graph:Relationship>; "
      "<urn:dcs:graph:source> <urn:dcs:node:missing>; <urn:dcs:graph:target> "
      "<urn:dcs:node:one>; <urn:dcs:graph:relationshipType> \"bad\" .");
  std::string original = Encoding::readFile((directory / "root.xml").string());
  EXPECT_THROW(ChangelogWriter{}.append(directory / "root.xml",
                                        directory / "second/changelog.xml"),
               std::runtime_error);
  EXPECT_EQ(Encoding::readFile((directory / "root.xml").string()), original);
}
TEST_F(ChangelogWriterTest, ShouldRefuseExistingChangesetOutput) {
  EXPECT_THROW(ChangelogWriter{}.create(directory / "first", "new",
                                        directory / "first.ttl"),
               std::runtime_error);
}
} // namespace entity_rdf
