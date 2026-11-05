#include "entity_rdf/instances/FeeModule.hxx"
#include "entity_rdf/instances/FeeConfigXml.hxx"
#include <gtest/gtest.h>
namespace entity_rdf::instances {
class FeeModuleTest : public testing::Test {
protected:
  Graph core;
  FeeConfig config{"urn:instance:",
                   {{"urn:board", "urn:detector", "urn:type"}}};
  void SetUp() override {
    core.nodes.emplace("board",
                       NodeRecord{"board",
                                  {"FrontEndElectronic"},
                                  {{"name", "PMA0"}, {"iri", "urn:board"}}});
    core.nodes.emplace("detector",
                       NodeRecord{"detector",
                                  {"DCSDetector"},
                                  {{"name", "ft0"}, {"iri", "urn:detector"}}});
    core.nodes.emplace("type",
                       NodeRecord{"type",
                                  {"FrontEndElectronicType"},
                                  {{"name", "PMType"}, {"iri", "urn:type"}}});
    core.nodes.emplace("parameter", NodeRecord{"parameter",
                                               {"HardwareParameter"},
                                               {{"name", "threshold"},
                                                {"iri", "urn:parameter"}}});
    core.relationships.emplace(
        "hardware", RelationshipRecord{
                        "hardware", "detector", "board", "has_hardware", {}});
    core.relationships.emplace(
        "type",
        RelationshipRecord{"type", "board", "type", "is_of_device_type", {}});
    core.relationships.emplace(
        "parameter",
        RelationshipRecord{
            "parameter", "type", "parameter", "has_parameter", {}});
  }
};
TEST_F(FeeModuleTest, ShouldGenerateInstancesSeparatelyFromCore) {
  Graph generated = FeeModule{}.build(core, config);
  ASSERT_EQ(generated.nodes.size(), 1);
  EXPECT_EQ(generated.relationships.size(), 2);
  EXPECT_EQ(core.nodes.size(), 4);
  EXPECT_EQ(generated.nodes.begin()->second.properties.at("name"),
            "PMA0/threshold");
}
TEST_F(FeeModuleTest, ShouldRejectTopologyMismatch) {
  core.relationships.erase("hardware");
  EXPECT_THROW(FeeModule{}.build(core, config), std::runtime_error);
}
TEST_F(FeeModuleTest, ShouldRejectDuplicateBoardConfiguration) {
  config.boards.push_back(config.boards.front());
  EXPECT_THROW(FeeModule{}.build(core, config), std::runtime_error);
}
TEST_F(FeeModuleTest, ShouldPreserveHistoricalUuid5) {
  EXPECT_EQ(FeeModule::uuid("FT0.PMA0.threshold"),
            "792c4aa7-0a22-58da-9d28-4310d811ccc8");
}
} // namespace entity_rdf::instances
namespace entity_rdf::instances {
TEST_F(FeeModuleTest,
       ShouldKeepStoredRecordIdsWhenRegeneratingConfiguredInstances) {
  Graph generated = FeeModule{}.build(core, config);
  NodeRecord existing = generated.nodes.begin()->second;
  existing.id = "stored-legacy-id";
  core.nodes.emplace(existing.id, existing);
  core.relationships.emplace("stored-link",
                             RelationshipRecord{"stored-link",
                                                existing.id,
                                                "board",
                                                "parameter_of",
                                                {{"note", "preserved"}}});
  Graph updated = FeeModule{}.build(core, config);
  EXPECT_TRUE(updated.nodes.contains("stored-legacy-id"));
  EXPECT_TRUE(updated.relationships.contains("stored-link"));
  EXPECT_EQ(updated.relationships.at("stored-link").properties.at("note"),
            "preserved");
}
} // namespace entity_rdf::instances
