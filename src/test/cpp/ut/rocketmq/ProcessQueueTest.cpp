#include "Assignment.h"
#include "ClientInstance.h"
#include "ClientManager.h"
#include "DefaultMQPushConsumerImpl.h"
#include "InvocationContext.h"
#include "ProcessQueueImpl.h"
#include "ProcessQueueMock.h"
#include "ReceiveMessageCallbackMock.h"
#include "RpcClientMock.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include <apache/rocketmq/v1/definition.pb.h>
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
    process_queue_ = std::make_shared<ProcessQueueImpl>(message_queue_, filter_expression_, ConsumeMessageType::POP,
                                                    consumer_, client_instance_);
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
  FilterExpression filter_expression_{"TagA"};
  MQMessageQueue message_queue_;
  std::shared_ptr<testing::NiceMock<RpcClientMock>> rpc_client_;
  std::shared_ptr<ClientInstance> client_instance_;
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer_;
  std::shared_ptr<ProcessQueueImpl> process_queue_;
  std::shared_ptr<testing::NiceMock<ReceiveMessageCallbackMock>> receive_message_callback_;
  std::string arn_{"arn:test"};
  std::string message_body_{"Sample body"};
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

TEST_F(ProcessQueueTest, testReceiveMessageByPop) {
  absl::Mutex mtx;
  absl::CondVar cv;
  bool completed = false;
  auto callback = [&](const ReceiveMessageRequest& request,
                      InvocationContext<ReceiveMessageResponse>* invocation_context) {
    invocation_context->onCompletion(true);
    absl::MutexLock lk(&mtx);
    completed = true;
    cv.SignalAll();
  };
  EXPECT_CALL(*rpc_client_, asyncReceive).Times(testing::AtLeast(1)).WillRepeatedly(testing::Invoke(callback));
  EXPECT_CALL(*rpc_client_, ok).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*receive_message_callback_, onSuccess).Times(testing::AtLeast(1));
  process_queue_->receiveMessage();

  while (!completed) {
    absl::MutexLock lk(&mtx);
    cv.Wait(&mtx);
  }
}

TEST_F(ProcessQueueTest, testReceiveMessageByPull) {
  process_queue_->consumeType(ConsumeMessageType::PULL);
  absl::Mutex mtx;
  absl::CondVar cv;
  bool completed = false;
  auto callback = [&](const PullMessageRequest& request, InvocationContext<PullMessageResponse>* invocation_context) {
    invocation_context->response.set_min_offset(0);
    invocation_context->response.set_next_offset(33);
    invocation_context->response.set_max_offset(64);

    for (int32_t i = 0; i < request.batch_size(); i++) {
      auto message = new rmq::Message;
      message->set_body(message_body_);
      message->mutable_topic()->set_arn(request.partition().topic().arn());
      message->mutable_topic()->set_name(request.partition().topic().name());
      message->mutable_system_attribute()->mutable_body_digest()->set_type(rmq::DigestType::SHA1);
      std::string digest;
      MixAll::sha1(message_body_, digest);
      message->mutable_system_attribute()->mutable_body_digest()->set_checksum(digest);
      invocation_context->response.mutable_messages()->AddAllocated(message);
    }
    invocation_context->onCompletion(true);
    absl::MutexLock lk(&mtx);
    completed = true;
    cv.SignalAll();
  };
  EXPECT_CALL(*rpc_client_, asyncPull).Times(testing::AtLeast(1)).WillRepeatedly(testing::Invoke(callback));
  EXPECT_CALL(*rpc_client_, ok).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*receive_message_callback_, onSuccess).Times(testing::AtLeast(1));
  process_queue_->receiveMessage();

  while (!completed) {
    absl::MutexLock lk(&mtx);
    cv.Wait(&mtx);
  }
}

ROCKETMQ_NAMESPACE_END