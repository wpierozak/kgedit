#include "entity_rdf/PropertyCodec.hxx"
#include <gtest/gtest.h>
namespace entity_rdf {
class PropertyCodecTest : public testing::Test {
protected:
  nlohmann::json decode(const std::string &kind, const std::string &lexical) {
    return PropertyCodec::decode({lexical, "urn:dcs:neo4j:" + kind, false});
  }
};
TEST_F(PropertyCodecTest, ShouldPreserveNanosecondTemporalEncoding) {
  EXPECT_EQ(
      decode(
          "DateTime",
          R"({"@type":"DateTime","value":"2026-01-01T12:34:56.123456789+01:00","timezone":"Europe/Warsaw"})")
          .at("timezone"),
      "Europe/Warsaw");
}
TEST_F(PropertyCodecTest, ShouldRejectPointWithWrongDimension) {
  EXPECT_THROW(
      decode("point",
             R"({"@type":"point","srid":4326,"coordinates":[1.0,2.0,3.0]})"),
      std::runtime_error);
}
TEST_F(PropertyCodecTest, ShouldRejectMalformedBytes) {
  EXPECT_THROW(decode("bytes", R"({"@type":"bytes","value":"zz"})"),
               std::runtime_error);
}
TEST_F(PropertyCodecTest, ShouldRejectDurationWithMissingComponents) {
  EXPECT_THROW(decode("Duration", R"({"@type":"Duration","value":[1,2]})"),
               std::runtime_error);
}
TEST_F(PropertyCodecTest, ShouldRejectIntegersOutsideNeo4jRange) {
  EXPECT_THROW(PropertyCodec::decode(
                   {"9223372036854775808",
                    "http://www.w3.org/2001/XMLSchema#integer", false}),
               std::runtime_error);
}
} // namespace entity_rdf

namespace entity_rdf {
TEST_F(PropertyCodecTest, ShouldRejectInvalidTemporalLexicalForm) {
  EXPECT_THROW(decode("Date", R"({"@type":"Date","value":"invalid"})"),
               std::exception);
}
} // namespace entity_rdf
