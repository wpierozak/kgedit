#pragma once
#include <string>
namespace entity_rdf {
/**
 * @brief Carries an HTTP status and response body for the retired transport.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
struct HttpResponse {
  /// HTTP response status code.
  long status;
  /// Unmodified response body bytes.
  std::string body;
};
} // namespace entity_rdf
