#pragma once

#include "HttpClient.h"

#include <chrono>

#include "absl/synchronization/mutex.h"
#include "curl/curl.h"

#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class CurlHttpClient : public HttpClient {
public:
  CurlHttpClient();

  ~CurlHttpClient() override;

  void start() override;

  void shutdown() override;

  void get(HttpProtocol protocol, const std::string& host, std::uint16_t port, const std::string& path,
           const std::function<void(int, const absl::flat_hash_map<std::string, std::string>&, const std::string&)>& cb)
      override;

private:
  CURL* curl_;
  absl::Mutex mtx_;

  std::chrono::milliseconds timeout_{3000};

  static std::size_t writeCallback(void* data, std::size_t size, std::size_t nmemb, void* param);
};

ROCKETMQ_NAMESPACE_END