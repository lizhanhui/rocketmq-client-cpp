#include "ClientImpl.h"
#include "ClientManagerFactory.h"
#include "ClientManagerMock.h"
#include "HttpClientMock.h"
#include "Scheduler.h"
#include "TopAddressing.h"
#include "rocketmq/RocketMQ.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

ROCKETMQ_NAMESPACE_BEGIN

class TestClientImpl : public ClientImpl, public std::enable_shared_from_this<TestClientImpl> {
public:
  TestClientImpl(std::string group) : ClientImpl(std::move(group)) {}

  std::shared_ptr<ClientImpl> self() override { return shared_from_this(); }

  void prepareHeartbeatData(HeartbeatRequest& request) override {}
};

class ClientImplTest : public testing::Test {
public:
  void SetUp() override {
    grpc_init();
    client_manager_ = std::make_shared<testing::NiceMock<ClientManagerMock>>();
    ClientManagerFactory::getInstance().addClientManager(arn_, client_manager_);

    ON_CALL(*client_manager_, getScheduler).WillByDefault(testing::ReturnRef(scheduler_));
    ON_CALL(*client_manager_, topAddressing).WillByDefault(testing::ReturnRef(top_addressing_));

    client_ = std::make_shared<TestClientImpl>(group_);

    auto http_client = absl::make_unique<HttpClientMock>();

    std::string once{"10.0.0.1:9876"};
    std::string then{"10.0.0.1:9876;10.0.0.2:9876"};
    absl::flat_hash_map<std::string, std::string> header;
    int http_status = 200;
    auto once_cb =
        [&](HttpProtocol protocol, const std::string& host, std::uint16_t port, const std::string& path,
            const std::function<void(int, const absl::flat_hash_map<std::string, std::string>&, const std::string&)>&
                cb) { cb(http_status, header, once); };
    auto then_cb =
        [&](HttpProtocol protocol, const std::string& host, std::uint16_t port, const std::string& path,
            const std::function<void(int, const absl::flat_hash_map<std::string, std::string>&, const std::string&)>&
                cb) { cb(http_status, header, then); };

    EXPECT_CALL(*http_client, get).WillOnce(testing::Invoke(once_cb)).WillRepeatedly(testing::Invoke(then_cb));
    top_addressing_.injectHttpClient(std::move(http_client));
  }

  void TearDown() override { grpc_shutdown(); }

protected:
  std::string arn_{"arn:mq://test"};
  std::string group_{"Group-0"};
  std::shared_ptr<testing::NiceMock<ClientManagerMock>> client_manager_;
  Scheduler scheduler_;
  TopAddressing top_addressing_;
  std::shared_ptr<TestClientImpl> client_;
};

TEST_F(ClientImplTest, testBasic) {
  
}

ROCKETMQ_NAMESPACE_END