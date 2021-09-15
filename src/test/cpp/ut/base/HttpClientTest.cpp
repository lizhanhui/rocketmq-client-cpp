#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "gtest/gtest.h"

#include "CurlHttpClient.h"
#include "LoggerImpl.h"
#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class HttpClientTest : public testing::Test {
public:
  void SetUp() override {
    SPDLOG_DEBUG("GHttpClient::SetUp() starts");
    http_client.start();
    SPDLOG_DEBUG("GHttpClient::SetUp() completed");
  }

  void TearDown() override {
    SPDLOG_DEBUG("GHttpClientTest::TearDown() starts");
    http_client.shutdown();
    SPDLOG_DEBUG("GHttpClientTest::TearDown() completed");
  }

protected:
  CurlHttpClient http_client;
};

TEST_F(HttpClientTest, testBasics) {}

TEST_F(HttpClientTest, testGet) {
  auto cb = [](int code, const absl::flat_hash_map<std::string, std::string>& headers, const std::string& body) {
    SPDLOG_INFO("Response received. Status-code: {}, Body: {}", code, body);
  };

  http_client.get(HttpProtocol::HTTP, "www.baidu.com", 80, "/", cb);
}

TEST_F(HttpClientTest, testJMEnv) {
  auto cb = [](int code, const absl::flat_hash_map<std::string, std::string>& headers, const std::string& body) {
    SPDLOG_INFO("Response received. Status-code: {}, Body: {}", code, body);
  };

  http_client.get(HttpProtocol::HTTP, "jmenv.tbsite.net", 8080, "/rocketmq/nsaddr", cb);
}

ROCKETMQ_NAMESPACE_END