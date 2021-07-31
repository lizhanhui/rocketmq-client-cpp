#include "Assignment.h"
#include "ClientInstance.h"
#include "ClientManager.h"
#include "InvocationContext.h"
#include "MessageAccessor.h"
#include "ProcessQueueImpl.h"
#include "PushConsumerMock.h"
#include "ReceiveMessageCallbackMock.h"
#include "RpcClientMock.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "apache/rocketmq/v1/definition.pb.h"
#include "rocketmq/MQMessageExt.h"
#include "gtest/gtest.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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
    consumer_ = std::make_shared<testing::NiceMock<PushConsumerMock>>();
    auto consumer = std::dynamic_pointer_cast<PushConsumer>(consumer_);
    process_queue_ = std::make_shared<ProcessQueueImpl>(message_queue_, filter_expression_, ConsumeMessageType::POP,
                                                        consumer, client_instance_);
    receive_message_callback_ = std::make_shared<testing::NiceMock<ReceiveMessageCallbackMock>>();
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
  std::string tag_{"TagA"};
  FilterExpression filter_expression_{tag_};
  MQMessageQueue message_queue_;
  std::shared_ptr<testing::NiceMock<RpcClientMock>> rpc_client_;
  std::shared_ptr<ClientInstance> client_instance_;
  std::shared_ptr<testing::NiceMock<PushConsumerMock>> consumer_;
  std::shared_ptr<ProcessQueueImpl> process_queue_;
  std::shared_ptr<testing::NiceMock<ReceiveMessageCallbackMock>> receive_message_callback_;
  std::string arn_{"arn:test"};
  std::string message_body_{"Sample body"};

  uint32_t threshold_quantity_{32};
  uint64_t threshold_memory_{4096};
  uint32_t consume_batch_size_{8};
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

TEST_F(ProcessQueueTest, testShouldThrottle) {
  EXPECT_CALL(*consumer_, maxCachedMessageQuantity)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_quantity_));
  EXPECT_CALL(*consumer_, maxCachedMessageMemory)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_memory_));
  EXPECT_FALSE(process_queue_->shouldThrottle());
}

TEST_F(ProcessQueueTest, testShouldThrottle_ByQuantity) {
  std::vector<MQMessageExt> messages;
  for (uint32_t i = 0; i < threshold_quantity_; i++) {
    MQMessageExt message;
    message.setTopic(topic_);
    message.setTags(tag_);
    MessageAccessor::setQueueId(message, 0);
    MessageAccessor::setQueueOffset(message, i);
    message.setBody(std::to_string(i));
    messages.emplace_back(message);
  }

  process_queue_->cacheMessages(messages);

  EXPECT_CALL(*consumer_, maxCachedMessageQuantity)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_quantity_));
  EXPECT_CALL(*consumer_, maxCachedMessageMemory)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_memory_));
  EXPECT_TRUE(process_queue_->shouldThrottle());
}

TEST_F(ProcessQueueTest, testShouldThrottle_ByMemory) {
  std::vector<MQMessageExt> messages;
  size_t body_length = 1024 * 4;
  for (uint32_t i = 0; i < threshold_quantity_ / 2; i++) {
    MQMessageExt message;
    message.setTopic(topic_);
    message.setTags(tag_);
    MessageAccessor::setQueueId(message, 0);
    MessageAccessor::setQueueOffset(message, i);
    message.setBody(std::string(body_length, 'c'));
    messages.emplace_back(message);
  }

  process_queue_->cacheMessages(messages);

  EXPECT_CALL(*consumer_, maxCachedMessageQuantity)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_quantity_));
  EXPECT_CALL(*consumer_, maxCachedMessageMemory)
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(threshold_memory_));
  EXPECT_TRUE(process_queue_->shouldThrottle());
}

TEST_F(ProcessQueueTest, testHasPendingMessages) { EXPECT_FALSE(process_queue_->hasPendingMessages()); }

TEST_F(ProcessQueueTest, testHasPendingMessages2) {
  std::vector<MQMessageExt> messages;
  size_t body_length = 1024;
  for (size_t i = 0; i < threshold_quantity_; i++) {
    MQMessageExt message;
    message.setTopic(topic_);
    message.setTags(tag_);
    MessageAccessor::setQueueId(message, 0);
    MessageAccessor::setQueueOffset(message, i);
    message.setBody(std::string(body_length, 'c'));
    messages.emplace_back(message);
  }
  process_queue_->cacheMessages(messages);
  EXPECT_TRUE(process_queue_->hasPendingMessages());
}

TEST_F(ProcessQueueTest, testTake) {
  std::vector<MQMessageExt> messages;
  EXPECT_FALSE(process_queue_->take(consume_batch_size_, messages));
  EXPECT_TRUE(messages.empty());
}

TEST_F(ProcessQueueTest, testTake2) {

  {
    std::vector<MQMessageExt> messages;
    size_t body_length = 1024;
    for (size_t i = 0; i < threshold_quantity_; i++) {
      MQMessageExt message;
      message.setTopic(topic_);
      message.setTags(tag_);
      MessageAccessor::setQueueId(message, 0);
      MessageAccessor::setQueueOffset(message, i);
      message.setBody(std::string(body_length, 'c'));
      messages.emplace_back(message);
    }
    process_queue_->cacheMessages(messages);
    EXPECT_EQ(threshold_quantity_, process_queue_->cachedMessagesSize());
  }

  std::vector<MQMessageExt> msgs;
  EXPECT_TRUE(process_queue_->take(consume_batch_size_, msgs));
  EXPECT_FALSE(msgs.empty());
  EXPECT_EQ(tag_, msgs.begin()->getTags());
  EXPECT_EQ(topic_, msgs.begin()->getTopic());
  EXPECT_EQ(threshold_quantity_ - consume_batch_size_, process_queue_->cachedMessagesSize());
}

ROCKETMQ_NAMESPACE_END