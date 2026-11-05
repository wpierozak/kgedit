#include "entity_rdf/Vocabulary.hxx"
#include <fstream>
#include <iterator>
#include <memory>
#include <openssl/evp.h>
#include <rdf4cpp/IRI.hpp>
#include <stdexcept>
namespace entity_rdf {
void Encoding::validateIri(std::string_view value) {
  rdf4cpp::IRI::validate(value);
  if (value.find(':') == std::string_view::npos)
    throw std::runtime_error("Expected absolute IRI");
}
std::string Encoding::percent(std::string_view value) {
  const char *hex = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '~')
      result += c;
    else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}
std::string Encoding::unpercent(std::string_view value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '%') {
      result += value[i];
      continue;
    }
    if (i + 2 >= value.size())
      throw std::runtime_error("Invalid percent encoding");
    const auto digit = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      throw std::runtime_error("Noncanonical percent encoding");
    };
    result +=
        static_cast<char>((digit(value[i + 1]) << 4) | digit(value[i + 2]));
    i += 2;
  }
  if (percent(result) != value)
    throw std::runtime_error("Noncanonical record/key encoding");
  return result;
}
std::string Encoding::sha256(std::string_view value) {
  unsigned char bytes[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (EVP_Digest(value.data(), value.size(), bytes, &length, EVP_sha256(),
                 nullptr) != 1)
    throw std::runtime_error("SHA256 failed");
  const char *hex = "0123456789abcdef";
  std::string result;
  result.reserve(length * 2);
  for (unsigned int i = 0; i < length; ++i) {
    result += hex[bytes[i] >> 4];
    result += hex[bytes[i] & 15];
  }
  return result;
}
std::string Encoding::readFile(const std::string &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    throw std::runtime_error("Cannot read " + path);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}
std::string Encoding::quote(std::string_view value) {
  std::string result = "`";
  for (char c : value) {
    result += c;
    if (c == '`')
      result += c;
  }
  return result + "`";
}
} // namespace entity_rdf
