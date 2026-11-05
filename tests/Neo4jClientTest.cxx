#include "entity_rdf/Neo4jClient.hxx"
#include <gtest/gtest.h>

namespace entity_rdf {
class FakeHttpClient final : public IHttpClient {
public:
  mutable std::string request;
  std::string response = R"({"errors":[],"results":[]})";
  HttpResponse post(std::string_view, std::string_view body) const override {
    request = body;
    return {200, response};
  }
};

class Neo4jClientTest : public testing::Test {
protected:
  std::shared_ptr<FakeHttpClient> http = std::make_shared<FakeHttpClient>();
  Neo4jClient client{http, "http://localhost:7474/db/neo4j/tx/commit"};
};

TEST_F(Neo4jClientTest, ShouldRejectCypherErrorsEvenWithSuccessfulHttpStatus) {
  http->response =
      R"({"errors":[{"code":"Neo.ClientError.Statement.SyntaxError"}],"results":[]})";
  EXPECT_THROW(client.execute(nlohmann::json::array()), std::runtime_error);
}

TEST_F(Neo4jClientTest, ShouldRequireSuccessfulAndCompleteN10sImport) {
  http->response = R"({"errors":[],"results":[{"data":[{"row":["OK",3,4]}]}]})";
  EXPECT_THROW(client.importTurtle("file:///import/change.ttl"),
               std::runtime_error);
}

TEST_F(Neo4jClientTest, ShouldSendParameterizedN10sCallInsideCpp) {
  http->response = R"({"errors":[],"results":[{"data":[{"row":["OK",4,4]}]}]})";
  client.importTurtle("file:///import/change.ttl");
  nlohmann::json request = nlohmann::json::parse(http->request);
  EXPECT_EQ(request.at("statements").at(0).at("parameters").at("uri"),
            "file:///import/change.ttl");
}
} // namespace entity_rdf
