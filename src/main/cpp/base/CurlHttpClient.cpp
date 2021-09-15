#include "CurlHttpClient.h"

#include <string>

#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

CurlHttpClient::CurlHttpClient() {
  curl_ = curl_easy_init();
  if (!curl_) {
    SPDLOG_WARN("curl_easy_init() failed");
  }
}

CurlHttpClient::~CurlHttpClient() {
  if (curl_) {
    curl_easy_cleanup(curl_);
    curl_ = nullptr;
  }
}

void CurlHttpClient::start() {}

void CurlHttpClient::shutdown() {}

/**
 * @brief We current implement this function in sync mode since async http request in CURL is sort of unnecessarily
 * complex.
 *
 * @param protocol
 * @param host
 * @param port
 * @param path
 * @param cb
 */
void CurlHttpClient::get(
    HttpProtocol protocol, const std::string& host, std::uint16_t port, const std::string& path,
    const std::function<void(int, const absl::flat_hash_map<std::string, std::string>&, const std::string&)>& cb) {

  if (!curl_) {
    int code = 400;
    absl::flat_hash_map<std::string, std::string> headers;
    std::string body;
    cb(code, headers, body);
    return;
  }

  CURLcode res;
  absl::MutexLock lk(&mtx_);

  SPDLOG_DEBUG("Reset CURL session");
  curl_easy_reset(curl_);

  std::string query;
  switch (protocol) {
  case HttpProtocol::HTTP:
    query = fmt::format("http://{}:{}{}", host, port, path);
    break;
  case HttpProtocol::HTTPS:
    query = fmt::format("https://{}:{}{}", host, port, path);
    break;
  }

  std::string response;

  curl_easy_setopt(curl_, CURLOPT_URL, query.c_str());
  SPDLOG_DEBUG("Set CURL url: {}", query);

  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHttpClient::writeCallback);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (void*)&response);

  SPDLOG_DEBUG("Set CURL IO timeout to {}ms", timeout_.count());
  curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_.count());

  SPDLOG_DEBUG("Set CURL connect timeout to {}ms", timeout_.count());
  curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, timeout_.count());

  SPDLOG_DEBUG("Prepare to initiate HTTP/GET request");
  res = curl_easy_perform(curl_);
  SPDLOG_DEBUG("CURL completes request/response");

  if (CURLE_OK == res) {
    SPDLOG_DEBUG("Response code: {}, text: {}", res, response);
    int code = 200;
    absl::flat_hash_map<std::string, std::string> headers;
    cb(code, headers, response);
  } else {
    SPDLOG_WARN("Response code: {}, text: {}", res, response);
    int code = 400;
    absl::flat_hash_map<std::string, std::string> headers;
    cb(code, headers, response);
  }
}

std::size_t CurlHttpClient::writeCallback(void* data, std::size_t size, std::size_t nmemb, void* param) {
  auto response = reinterpret_cast<std::string*>(param);
  response->reserve(size * nmemb);
  response->append(reinterpret_cast<const char*>(data), size * nmemb);
  return size * nmemb;
}

ROCKETMQ_NAMESPACE_END