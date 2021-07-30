#include "ConsumeMessageService.h"
#include "MessageListenerMock.h"
#include "PushConsumerMock.h"
#include "gtest/gtest.h"
#include <memory>
#include "grpc/grpc.h"

ROCKETMQ_NAMESPACE_BEGIN

class ConsumeStandardMessageServiceTest : public testing::Test {
public:
  void SetUp() override {
    grpc_init();
    consumer_ = std::make_shared<testing::NiceMock<PushConsumerMock>>();
    std::weak_ptr<PushConsumer> consumer = std::dynamic_pointer_cast<PushConsumer>(consumer_);
    consume_standard_message_service_ =
        std::make_shared<ConsumeStandardMessageService>(consumer, thread_count_, &message_listener_);
    
  }

  void TearDown() override {
    grpc_shutdown();
  }

protected:
  int thread_count_{2};
  std::shared_ptr<testing::NiceMock<PushConsumerMock>> consumer_;
  std::shared_ptr<ConsumeStandardMessageService> consume_standard_message_service_;
  testing::NiceMock<StandardMessageListenerMock> message_listener_;
};

TEST_F(ConsumeStandardMessageServiceTest, testStart) {
  consume_standard_message_service_->start();
}

ROCKETMQ_NAMESPACE_END