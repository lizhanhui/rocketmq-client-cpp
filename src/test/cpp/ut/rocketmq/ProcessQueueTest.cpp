#include "ProcessQueue.h"
#include "AsyncReceiveMessageCallback.h"
#include "ClientInstance.h"
#include "ClientManager.h"
#include "DefaultMQPushConsumerImpl.h"
#include "InvocationContext.h"
#include "RpcClientMock.h"
#include "absl/memory/memory.h"
#include "gtest/gtest.h"
#include <chrono>
#include <iostream>
#include <memory>

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
    ClientManager::getInstance().addClientInstance(arn_, client_instance_);
    client_instance_->addRpcClient(service_address_, rpc_client_);
    auto credentials_provider = std::make_shared<StaticCredentialsProvider>(access_key_, access_secret_);
    consumer_ = std::make_shared<DefaultMQPushConsumerImpl>(group_name_);
    consumer_->setCredentialsProvider(credentials_provider);
    consumer_->arn(arn_);
    consumer_->region(region_);
    process_queue_ = std::make_shared<ProcessQueue>(message_queue_, filter_expression_, ConsumeMessageType::POP,
                                                    consumer_, client_instance_);
    receive_message_callback_ = std::make_shared<AsyncReceiveMessageCallback>(process_queue_);
    process_queue_->callback(receive_message_callback_);
  }

  void TearDown() override {}

protected:
  std::string access_key_{"ak"};
  std::string access_secret_{"secret"};
  std::string group_name_{"TestGroup"};
  std::string broker_name_{"broker-a"};
  std::string region_{"cn-hangzhou"};
  int queue_id_{0};
  std::string topic_{"TestTopic"};
  std::string service_address_{"ipv4:10.0.0.1:10911"};
  FilterExpression filter_expression_{"TagA"};
  MQMessageQueue message_queue_;
  std::shared_ptr<testing::NiceMock<RpcClientMock>> rpc_client_;
  std::shared_ptr<ClientInstance> client_instance_;
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer_;
  std::shared_ptr<ProcessQueue> process_queue_;
  std::shared_ptr<AsyncReceiveMessageCallback> receive_message_callback_;
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
  EXPECT_CALL(*rpc_client_, asyncReceive).Times(testing::AtLeast(1)).WillRepeatedly(testing::Invoke(callback));
  EXPECT_CALL(*rpc_client_, ok).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(true));
  process_queue_->receiveMessage();
}

ROCKETMQ_NAMESPACE_END