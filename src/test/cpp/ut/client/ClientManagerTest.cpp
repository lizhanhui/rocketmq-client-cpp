#include "ClientManagerMock.h"
#include "gtest/gtest.h"
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class ClientManagerTest : public testing::Test {
public:
  void SetUp() override {
    client_manager_ = std::make_shared<testing::NiceMock<ClientManagerMock>>();
  }

  void TearDown() override {}

private:
  std::shared_ptr<testing::NiceMock<ClientManagerMock>> client_manager_;
};

TEST_F(ClientManagerTest, testSetUp) {

}

ROCKETMQ_NAMESPACE_END