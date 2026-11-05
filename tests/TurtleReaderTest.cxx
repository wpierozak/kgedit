#include "entity_rdf/TurtleReader.hxx"
#include <fstream>
#include <gtest/gtest.h>

namespace entity_rdf {
class TurtleReaderTest : public testing::Test {
protected:
  std::filesystem::path path;
  void SetUp() override {
    path = std::filesystem::temp_directory_path() /
           ("entity-rdf-" +
            std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".ttl");
  }
  void TearDown() override { std::filesystem::remove(path); }
  void write(std::string_view content) {
    std::ofstream output(path);
    output << content;
  }
};
TEST_F(TurtleReaderTest, ShouldReadTypedLiteralsUsingRdf4cpp) {
  write("<urn:n> <urn:p> \"42\"^^<http://www.w3.org/2001/XMLSchema#long> .\n");
  RdfDocument document = TurtleReader().read(path);
  ASSERT_EQ(document.size(), 1);
  EXPECT_EQ(document[0].object.value, "42");
  EXPECT_EQ(document[0].object.datatype,
            "http://www.w3.org/2001/XMLSchema#long");
}
TEST_F(TurtleReaderTest, ShouldRejectInvalidTypedLiterals) {
  write(
      "<urn:n> <urn:p> \"wrong\"^^<http://www.w3.org/2001/XMLSchema#long> .\n");
  EXPECT_THROW(TurtleReader().read(path), std::runtime_error);
}
TEST_F(TurtleReaderTest, ShouldRejectAnonymousPublishedRecords) {
  write("[] <urn:p> \"value\" .\n");
  EXPECT_THROW(TurtleReader().read(path), std::runtime_error);
}
} // namespace entity_rdf
