#include "PushConsumerImpl.h"
#include "ClientManagerFactory.h"
#include "ClientManagerMock.h"
#include "rocketmq/RocketMQ.h"
#include "gtest/gtest.h"
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class PushConsumerImplTest : public testing::Test {
  void SetUp() override {
    client_manager_ = std::make_shared<testing::NiceMock<ClientManagerMock>>();
    ClientManagerFactory::getInstance().addClientManager(arn_, client_manager_);
    push_consumer_ = std::make_shared<PushConsumerImpl>(group_);
    push_consumer_->start();
  }

  void TearDown() override { push_consumer_->shutdown(); }

protected:
  std::string arn_{"arn:mq://test"};
  std::string group_{"CID_test"};
  std::shared_ptr<testing::NiceMock<ClientManagerMock>> client_manager_;
  std::shared_ptr<PushConsumerImpl> push_consumer_;
};

TEST_F(PushConsumerImplTest, testAck) {}

ROCKETMQ_NAMESPACE_END