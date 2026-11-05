#include "entity_rdf/History.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class HistoryTest : public testing::Test {
protected:
  ChangeLog log;
  void SetUp() override {
    ChangeSet first;
    first.id = "first";
    first.checksum = "aaa";
    ChangeSet second;
    second.id = "second";
    second.checksum = "bbb";
    log.changes = {first, second};
  }
};
TEST_F(HistoryTest, ShouldApplyOnlyPendingSuffix) {
  EXPECT_EQ(History::pending(log, {{"first"}, {"aaa"}}), 1);
}
TEST_F(HistoryTest, ShouldSkipIdenticalRepeatedLoad) {
  EXPECT_EQ(History::pending(log, {{"first", "second"}, {"aaa", "bbb"}}), 2);
}
TEST_F(HistoryTest, ShouldRejectEditedAppliedChangeset) {
  EXPECT_THROW(History::pending(log, {{"first"}, {"edited"}}),
               std::runtime_error);
}
TEST_F(HistoryTest, ShouldRejectReorderedHistory) {
  EXPECT_THROW(History::pending(log, {{"second"}, {"bbb"}}),
               std::runtime_error);
}
TEST_F(HistoryTest, ShouldRejectTruncatedChangelog) {
  log.changes.resize(1);
  EXPECT_THROW(History::pending(log, {{"first", "second"}, {"aaa", "bbb"}}),
               std::runtime_error);
}
} // namespace entity_rdf
