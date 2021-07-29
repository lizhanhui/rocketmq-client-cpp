#include "ProcessQueue.h"
#include "gtest/gtest.h"
#include "ClientInstance.h"
#include "DefaultMQPushConsumerImpl.h"
#include "InvocationContext.h"
#include "absl/memory/memory.h"
#include <chrono>
#include <memory>
#include "RpcClientMock.h"
#include <iostream>

ROCKETMQ_NAMESPACE_BEGIN

class ProcessQueueTest : public testing::Test {
public:
  void SetUp() override {
    rpc_client_ = std::make_shared<testing::NiceMock<RpcClientMock>>();
    message_queue_.serviceAddress(service_address_);
    message_queue_.setTopic(topic_);
    message_queue_.setBrokerName(broker_name_);
    message_queue_.setQueueId(queue_id_);
    client_instance_ = std::make_shared<ClientInstance>(arn_);
    client_instance_->addRpcClient(service_address_, rpc_client_);
    process_queue_ = absl::make_unique<ProcessQueue>(message_queue_, filter_expression_, ConsumeMessageType::POP, consumer_, client_instance_);
  }

  void TearDown() override {
    
  }

protected:
  std::string broker_name_{"broker-a"};
  int queue_id_{0};
  std::string topic_{"TestTopic"};
  std::string service_address_{"ipv4:10.0.0.1:10911"};
  FilterExpression filter_expression_{"TagA"};
  MQMessageQueue message_queue_;
  std::shared_ptr<testing::NiceMock<RpcClientMock>> rpc_client_;
  std::shared_ptr<ClientInstance> client_instance_;
  std::weak_ptr<DefaultMQPushConsumerImpl> consumer_;
  std::unique_ptr<ProcessQueue> process_queue_;
  std::string arn_{"arn:test"};
};

TEST_F(ProcessQueueTest, testBind) {
  EXPECT_TRUE(process_queue_->bindFifoConsumeTask());
  EXPECT_FALSE(process_queue_->bindFifoConsumeTask());
  EXPECT_TRUE(process_queue_->unbindFifoConsumeTask());
  EXPECT_FALSE(process_queue_->unbindFifoConsumeTask());
  EXPECT_TRUE(process_queue_->bindFifoConsumeTask());
}

TEST_F(ProcessQueueTest, testExpired) {
  EXPECT_FALSE(process_queue_->expired());
  process_queue_->idle_since_ -= MixAll::PROCESS_QUEUE_EXPIRATION_THRESHOLD_;
  EXPECT_TRUE(process_queue_->expired());
}

TEST_F(ProcessQueueTest, testReceiveMessage) {
  auto callback = [](const ReceiveMessageRequest& request,
                     InvocationContext<ReceiveMessageResponse>* invocation_context) {
    std::cout << "callback" << std::endl;
    invocation_context->onCompletion(true);
  };
  EXPECT_CALL(*rpc_client_, asyncReceive).Times(testing::AtMost(1)).WillRepeatedly(testing::Invoke(callback));
  process_queue_->receiveMessage();
}

ROCKETMQ_NAMESPACE_END