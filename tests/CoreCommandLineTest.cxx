#include "entity_rdf/CommandLine.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>
namespace entity_rdf {
/** @brief Exercises CLI parsing independently from application startup. */
class CoreCommandLineTest : public testing::Test {
protected:
  /// Per-test directory containing fixture inputs and newly generated outputs.
  std::filesystem::path directory;
  /// Captured normal output and help text.
  std::ostringstream output;
  /// Captured argument parsing diagnostics.
  std::ostringstream error;

  /** @brief Allocate an isolated directory for the current test case. */
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() / "kgedit-command-line" /
                testing::UnitTest::GetInstance()->current_test_info()->name();
    std::filesystem::create_directories(directory);
  }
  /** @brief Remove only this test case's fixture files. */
  void TearDown() override { std::filesystem::remove_all(directory); }

  /**
   * @brief Run a complete argument vector with captured output streams.
   * @param arguments Arguments including the executable name.
   * @return Command success or CLI11 parsing status.
   */
  int run(const std::vector<std::string> &arguments) {
    output.str("");
    error.str("");
    std::vector<const char *> pointers;
    for (const std::string &argument : arguments)
      pointers.push_back(argument.c_str());
    return CommandLine{output, error}.run(static_cast<int>(pointers.size()),
                                          pointers.data());
  }

  /**
   * @brief Write a minimal checksummed canonical changelog fixture.
   * @return Root XML path containing spaces to exercise positional path
   * handling.
   */
  std::string changelog() {
    const std::string turtle = "<urn:test:node> a <urn:dcs:graph:Node>; "
                               "<urn:dcs:graph:recordId> \"preserved-id\"; "
                               "<urn:dcs:graph:label> \"Example\" .\n";
    std::ofstream(directory / "data.ttl") << turtle;
    const std::string change =
        "<graphChangeSet id=\"first\"><rdf file=\"data.ttl\" sha256=\"" +
        Encoding::sha256(turtle) + "\"/></graphChangeSet>";
    std::ofstream(directory / "change.xml") << change;
    std::filesystem::path root = directory / "root with spaces.xml";
    std::ofstream(root)
        << "<graphChangeLog format=\"rdf-changelog-v1\"><include "
           "file=\"change.xml\" sha256=\"" +
               Encoding::sha256(change) + "\"/></graphChangeLog>";
    return root.string();
  }
  /**
   * @brief Write an independent canonical node definition for authoring tests.
   * @return Path to the Turtle input file.
   */
  std::filesystem::path additionalNode() {
    std::filesystem::path path = directory / "additional node.ttl";
    std::ofstream(path) << "<urn:test:additional> a <urn:dcs:graph:Node>; "
                           "<urn:dcs:graph:recordId> \"additional-id\"; "
                           "<urn:dcs:graph:label> \"Example\" .\n";
    return path;
  }
};
TEST_F(CoreCommandLineTest, ShouldShowRootHelpWithoutReadingInputs) {
  EXPECT_EQ(run({"kgedit", "--help"}), 0);
  EXPECT_NE(output.str().find("compose"), std::string::npos);
  EXPECT_TRUE(error.str().empty());
}
TEST_F(CoreCommandLineTest,
       ShouldShowHelpForEverySubcommandWithoutRequiredArguments) {
  for (const std::string &command :
       {"validate", "compose", "build", "new-change", "append"}) {
    SCOPED_TRACE(command);
    EXPECT_EQ(run({"kgedit", command, "--help"}), 0);
    EXPECT_NE(output.str().find("ROOT"), std::string::npos);
    EXPECT_TRUE(error.str().empty());
  }
}
TEST_F(CoreCommandLineTest, ShouldSupportShortHelp) {
  EXPECT_EQ(run({"kgedit", "-h"}), 0);
  EXPECT_EQ(run({"kgedit", "build", "-h"}), 0);
  EXPECT_NE(output.str().find("--ttl-uri"), std::string::npos);
}
TEST_F(CoreCommandLineTest,
       ShouldRejectMissingRequiredArgumentsBeforeReadingInputs) {
  const std::vector<std::vector<std::string>> cases = {
      {"kgedit"},
      {"kgedit", "validate"},
      {"kgedit", "compose", "missing.xml"},
      {"kgedit", "build", "missing.xml", "output"},
      {"kgedit", "new-change", "missing.xml", "output", "id"},
      {"kgedit", "append", "missing.xml"}};
  for (const std::vector<std::string> &arguments : cases) {
    SCOPED_TRACE(testing::PrintToString(arguments));
    EXPECT_NE(run(arguments), 0);
    EXPECT_FALSE(error.str().empty());
  }
}
TEST_F(CoreCommandLineTest,
       ShouldRejectUnknownCommandsAndOptionsBeforeReadingInputs) {
  EXPECT_NE(run({"kgedit", "unknown", "missing.xml"}), 0);
  EXPECT_FALSE(error.str().empty());
  EXPECT_NE(run({"kgedit", "validate", "missing.xml", "--unknown"}), 0);
  EXPECT_NE(error.str().find("--unknown"), std::string::npos);
}
TEST_F(CoreCommandLineTest,
       ShouldRejectMultipleSubcommandsBeforeExecutingEither) {
  EXPECT_NE(run({"kgedit", "validate", "missing.xml", "compose", "missing.xml",
                 (directory / "unexpected.ttl").string()}),
            0);
  EXPECT_FALSE(std::filesystem::exists(directory / "unexpected.ttl"));
}
TEST_F(CoreCommandLineTest,
       ShouldRejectExtraArgumentsWithoutPublishingOutputs) {
  EXPECT_NE(run({"kgedit", "compose", changelog(),
                 (directory / "unexpected.ttl").string(), "extra"}),
            0);
  EXPECT_FALSE(std::filesystem::exists(directory / "unexpected.ttl"));
}
TEST_F(CoreCommandLineTest, ShouldRejectMissingAndRepeatedTurtleUriValues) {
  EXPECT_NE(run({"kgedit", "build", "missing.xml",
                 (directory / "bundle").string(), "--ttl-uri"}),
            0);
  EXPECT_NE(
      run({"kgedit", "build", "missing.xml", (directory / "bundle").string(),
           "--ttl-uri", "file:///one.ttl", "--ttl-uri", "file:///two.ttl"}),
      0);
  EXPECT_FALSE(std::filesystem::exists(directory / "bundle"));
}
TEST_F(CoreCommandLineTest, ShouldValidateCanonicalChangelogWithSpaceInPath) {
  EXPECT_EQ(run({"kgedit", "validate", changelog()}), 0);
  EXPECT_EQ(output.str(), "1 changesets, 1 nodes, 0 relationships\n");
  EXPECT_TRUE(error.str().empty());
}
TEST_F(CoreCommandLineTest, ShouldComposeTurtleWithSpaceInOutputPath) {
  std::filesystem::path path = directory / "combined graph.ttl";
  EXPECT_EQ(run({"kgedit", "compose", changelog(), path.string()}), 0);
  EXPECT_NE(Encoding::readFile(path.string()).find("<urn:test:node>"),
            std::string::npos);
  EXPECT_THROW(run({"kgedit", "compose", changelog(), path.string()}),
               std::runtime_error);
}
TEST_F(CoreCommandLineTest,
       ShouldAcceptTurtleUriBeforePositionalsAndPublishBundle) {
  std::filesystem::path bundle = directory / "bundle";
  EXPECT_EQ(run({"kgedit", "build", "--ttl-uri", "file:///combined.ttl",
                 changelog(), bundle.string()}),
            0);
  EXPECT_TRUE(std::filesystem::exists(bundle / "combined.ttl"));
  EXPECT_TRUE(std::filesystem::exists(bundle / "import.cypher"));
  EXPECT_NE(Encoding::readFile((bundle / "manifest.xml").string())
                .find("file:///combined.ttl"),
            std::string::npos);
}
TEST_F(CoreCommandLineTest,
       ShouldCreateAndAppendChangesetUsingExistingArgumentOrder) {
  std::string root = changelog();
  std::filesystem::path change = directory / "new change";
  EXPECT_EQ(run({"kgedit", "new-change", root, change.string(), "second",
                 additionalNode().string()}),
            0);
  EXPECT_EQ(
      run({"kgedit", "append", root, (change / "changelog.xml").string()}), 0);
  EXPECT_EQ(run({"kgedit", "validate", root}), 0);
  EXPECT_EQ(output.str(), "2 changesets, 2 nodes, 0 relationships\n");
}
TEST_F(CoreCommandLineTest, ShouldOmitDomainSpecificCommandsFromHelp) {
  EXPECT_EQ(run({"kgedit", "--help"}), 0);
  EXPECT_EQ(output.str().find("generate-instances"), std::string::npos);
  EXPECT_EQ(output.str().find("create-instance-config"), std::string::npos);
}
} // namespace entity_rdf
