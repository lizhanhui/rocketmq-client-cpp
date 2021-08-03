#include "PushConsumerImpl.h"
#include "ClientManagerFactory.h"
#include "ClientManagerMock.h"
#include "grpc/grpc.h"
#include "rocketmq/MQMessageExt.h"
#include "rocketmq/RocketMQ.h"
#include "gtest/gtest.h"
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class PushConsumerImplTest : public testing::Test {
  void SetUp() override {
    grpc_init();
    client_manager_ = std::make_shared<testing::NiceMock<ClientManagerMock>>();
    ClientManagerFactory::getInstance().addClientManager(arn_, client_manager_);
    push_consumer_ = std::make_shared<PushConsumerImpl>(group_);
    push_consumer_->arn(arn_);
    push_consumer_->setNameServerList(name_server_list_);
    push_consumer_->start();
  }

  void TearDown() override {
    push_consumer_->shutdown();
    grpc_shutdown();
  }

protected:
  std::vector<std::string> name_server_list_{"10.0.0.1:9876"};
  std::string arn_{"arn:mq://test"};
  std::string group_{"CID_test"};
  std::string topic_{"Topic0"};
  std::string tag_{"TagA"};
  std::string key_{"key-0"};
  std::string message_body_{"Message Body Content"};
  int delay_level_{1};
  std::shared_ptr<testing::NiceMock<ClientManagerMock>> client_manager_;
  std::shared_ptr<PushConsumerImpl> push_consumer_;
};

TEST_F(PushConsumerImplTest, testAck) {
  auto ack_cb = [](const std::string& target_host, const Metadata& metadata, const AckMessageRequest& request,
                   std::chrono::milliseconds timeout, const std::function<void(bool)>& cb) { cb(true); };

  EXPECT_CALL(*client_manager_, ack).Times(testing::AtLeast(1)).WillRepeatedly(testing::Invoke(ack_cb));

  bool completed = false;
  absl::Mutex mtx;
  absl::CondVar cv;
  auto callback = [&](bool ok) {
    absl::MutexLock lk(&mtx);
    completed = true;
    cv.SignalAll();
  };

  MQMessageExt message;
  message.setTopic(topic_);
  message.setBody(message_body_);
  message.setTags(tag_);
  message.setKey(key_);
  message.setDelayTimeLevel(delay_level_);

  push_consumer_->ack(message, callback);

  {
    absl::MutexLock lk(&mtx);
    if (!completed) {
      cv.WaitWithDeadline(&mtx, absl::Now() + absl::Seconds(3));
    }
  }
  EXPECT_TRUE(completed);
}

ROCKETMQ_NAMESPACE_END