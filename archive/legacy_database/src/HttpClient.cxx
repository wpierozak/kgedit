#include "entity_rdf/HttpClient.hxx"

#include <curl/curl.h>
#include <limits>
#include <memory>
#include <stdexcept>

namespace entity_rdf {
/** @brief Retired RAII guard for process-wide libcurl initialization. */
class CurlRuntime {
public:
  /**
   * @brief Initialize the retired process-wide libcurl runtime.
   *
   * @throws std::runtime_error If curl_global_init fails.
   */
  CurlRuntime() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
      throw std::runtime_error("Cannot initialize curl runtime");
  }
  /**
   * @brief Release the retired process-wide libcurl runtime.
   */
  ~CurlRuntime() { curl_global_cleanup(); }
};
/** @brief Retired checked wrapper for configuring libcurl request options. */
class CurlOptions {
public:
  /**
   * @brief Set a typed libcurl option and check the result.
   *
   * @tparam T Type accepted by the selected libcurl option.
   * @throws std::runtime_error If libcurl rejects the option.
   * @param client Request handle to configure.
   * @param option libcurl option identifier.
   * @param value Option value with the type expected by libcurl.
   */
  template <class T> static void set(CURL *client, CURLoption option, T value) {
    if (curl_easy_setopt(client, option, value) != CURLE_OK)
      throw std::runtime_error("Cannot configure curl request");
  }
};

CurlHttpClient::CurlHttpClient(std::string credentials)
    : credentials_(std::move(credentials)) {
  static const CurlRuntime runtime;
}

std::size_t CurlHttpClient::append(char *bytes, std::size_t size,
                                   std::size_t count, void *output) {
  if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size)
    return 0;
  std::size_t length = size * count;
  try {
    static_cast<std::string *>(output)->append(bytes, length);
    return length;
  } catch (...) {
    return 0;
  }
}

HttpResponse CurlHttpClient::post(std::string_view url,
                                  std::string_view body) const {
  return request(url, body, "POST");
}
void CurlHttpClient::remove(std::string_view url) const {
  request(url, "", "DELETE");
}
HttpResponse CurlHttpClient::request(std::string_view url,
                                     std::string_view body,
                                     const char *method) const {
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> client(curl_easy_init(),
                                                             curl_easy_cleanup);
  if (!client)
    throw std::runtime_error("Cannot initialize curl");
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
      curl_slist_append(nullptr, "Content-Type: application/json"),
      curl_slist_free_all);
  if (!headers)
    throw std::runtime_error("Cannot allocate HTTP headers");
  std::string address(url);
  HttpResponse result{0, {}};
  CurlOptions::set(client.get(), CURLOPT_URL, address.c_str());
  CurlOptions::set(client.get(), CURLOPT_CUSTOMREQUEST, method);
  CurlOptions::set(client.get(), CURLOPT_USERPWD, credentials_.c_str());
  CurlOptions::set(client.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  CurlOptions::set(client.get(), CURLOPT_HTTPHEADER, headers.get());
  CurlOptions::set(client.get(), CURLOPT_POSTFIELDS, body.data());
  CurlOptions::set(client.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(body.size()));
  CurlOptions::set(client.get(), CURLOPT_WRITEFUNCTION,
                   &CurlHttpClient::append);
  CurlOptions::set(client.get(), CURLOPT_WRITEDATA, &result.body);
  CurlOptions::set(client.get(), CURLOPT_CONNECTTIMEOUT, 15L);
  CurlOptions::set(client.get(), CURLOPT_TIMEOUT, 300L);
  CurlOptions::set(client.get(), CURLOPT_PROTOCOLS_STR, "http,https");
  CURLcode status = curl_easy_perform(client.get());
  if (status != CURLE_OK)
    throw std::runtime_error(std::string("HTTP request failed: ") +
                             curl_easy_strerror(status));
  curl_easy_getinfo(client.get(), CURLINFO_RESPONSE_CODE, &result.status);
  if (result.status < 200 || result.status >= 300)
    throw std::runtime_error("Neo4j HTTP status " +
                             std::to_string(result.status));
  return result;
}
} // namespace entity_rdf
