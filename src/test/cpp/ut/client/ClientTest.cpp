#include "gtest/gtest.h"
#include <memory>
#include "ClientMock.h"
#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class ClientTest : public testing::Test {
public:
  void SetUp() override {
    client_ = std::make_shared<testing::NiceMock<ClientMock>>();
  }

  void TearDown() override {}
  
protected:
  std::shared_ptr<testing::NiceMock<ClientMock>> client_;
};

ROCKETMQ_NAMESPACE_END