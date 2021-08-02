#include "TopAddressing.h"
#include "HttpClientMock.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

ROCKETMQ_NAMESPACE_BEGIN

class TopAddressingTest : public testing::Test {
public:
  void SetUp() override {
    auto http_client_ = absl::make_unique<testing::NiceMock<HttpClientMock>>();
    auto mock_get =
        [](HttpProtocol protocol, const std::string& host, std::uint16_t port, const std::string& path,
           const std::function<void(int, const absl::flat_hash_map<std::string, std::string>&, const std::string&)>&
               cb) {
          absl::flat_hash_map<std::string, std::string> headers;
          std::string body("10.0.0.1:9876");
          cb(200, headers, body);
        };
    ON_CALL(*http_client_, get).WillByDefault(testing::Invoke(mock_get));
    top_addressing_.injectHttpClient(std::move(http_client_));
  }

  void TearDown() override {}

protected:
  TopAddressing top_addressing_;
};

TEST_F(TopAddressingTest, testFetchNameServerAddresses) {
  bool completed = false;
  absl::Mutex mtx;
  absl::CondVar cv;

  auto callback = [&](bool ok, const std::vector<std::string>& name_server_list) {
    absl::MutexLock lk(&mtx);
    completed = true;
    cv.SignalAll();
  };
  top_addressing_.fetchNameServerAddresses(callback);
  {
    absl::MutexLock lk(&mtx);
    cv.WaitWithDeadline(&mtx, absl::Now() + absl::Seconds(3));
  }
  EXPECT_TRUE(completed);
}

ROCKETMQ_NAMESPACE_END