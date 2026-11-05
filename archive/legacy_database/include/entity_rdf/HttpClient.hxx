#pragma once

#include "entity_rdf/HttpDto.hxx"
#include <stdexcept>
#include <string>
#include <string_view>

namespace entity_rdf {
/**
 * @brief Abstract HTTP transport used only by the retired database executor.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class IHttpClient {
public:
  /**
   * @brief Destroy an HTTP transport through the retired abstract interface.
   */
  virtual ~IHttpClient() = default;
  /**
   * @brief Reject DELETE unless a retired transport overrides this operation.
   *
   * @throws std::runtime_error Always in this default implementation.
   */
  virtual void remove(std::string_view) const {
    throw std::runtime_error("HTTP DELETE is unsupported");
  }
  /**
   * @brief Send a retired HTTP POST request.
   * @param url Target URL.
   * @param body Exact request body bytes.
   * @return HTTP status and response body.
   */
  virtual HttpResponse post(std::string_view url,
                            std::string_view body) const = 0;
};

/**
 * @brief Retired libcurl transport for authenticated Neo4j JSON requests.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class CurlHttpClient final : public IHttpClient {
public:
  /**
   * @brief Initialize the retired curl runtime and retain Basic authentication
   * credentials.
   *
   * @throws std::runtime_error If global libcurl initialization fails.
   * @param credentials Username and password separated by a colon.
   */
  explicit CurlHttpClient(std::string credentials);
  /**
   * @brief Send an authenticated retired HTTP DELETE request.
   *
   * @throws std::runtime_error If transport fails or the status is outside the
   * 2xx range.
   * @param url Absolute HTTP or HTTPS URL.
   */
  void remove(std::string_view url) const override;
  /**
   * @brief Send an authenticated retired JSON POST request.
   *
   * @throws std::runtime_error If transport fails or the status is outside the
   * 2xx range.
   * @param url Absolute HTTP or HTTPS URL.
   * @param body JSON request body bytes.
   * @return Successful HTTP response.
   */
  HttpResponse post(std::string_view url, std::string_view body) const override;

private:
  /**
   * @brief Execute a retired authenticated JSON request with bounded timeouts.
   *
   * @throws std::runtime_error If allocation, configuration, transfer or status
   * validation fails.
   * @param url Absolute HTTP or HTTPS URL.
   * @param body Exact request bytes.
   * @param method HTTP method string passed to libcurl.
   * @return Successful response status and body.
   */
  HttpResponse request(std::string_view url, std::string_view body,
                       const char *method) const;
  /**
   * @brief Append libcurl response bytes without allowing exceptions across the
   * C callback.
   * @param bytes Incoming response buffer.
   * @param size Size of each incoming item.
   * @param count Number of incoming items.
   * @param output Pointer to the destination std::string.
   * @return Number of accepted bytes, or zero on overflow or append failure.
   */
  static std::size_t append(char *bytes, std::size_t size, std::size_t count,
                            void *output);
  /// Retained Basic authentication credentials.
  std::string credentials_;
};
} // namespace entity_rdf
