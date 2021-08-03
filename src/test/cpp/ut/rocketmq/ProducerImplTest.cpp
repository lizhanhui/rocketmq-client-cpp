#include "ProducerImpl.h"
#include "ClientManagerFactory.h"
#include "ClientManagerMock.h"
#include "TopicRouteData.h"
#include "rocketmq/MQMessage.h"
#include "rocketmq/RocketMQ.h"
#include "rocketmq/SendResult.h"
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class ProducerImplTest : public testing::Test {
public:
  void SetUp() override {
    grpc_init();
    client_manager_ = std::make_shared<testing::NiceMock<ClientManagerMock>>();
    producer_ = std::make_shared<ProducerImpl>(group_);
    producer_->arn(arn_);
    producer_->setNameServerList(name_server_list_);
  }

  void TearDown() override { grpc_shutdown(); }

protected:
  std::shared_ptr<testing::NiceMock<ClientManagerMock>> client_manager_;
  std::shared_ptr<ProducerImpl> producer_;
  std::vector<std::string> name_server_list_{"10.0.0.1:9876"};
  std::string arn_{"arn:mq://test"};
  std::string group_{"CID_test"};
  std::string topic_{"Topic0"};
  int queue_id_{1};
  std::string tag_{"TagA"};
  std::string key_{"key-0"};
  std::string broker_name_{"broker-a"};
  int broker_id_{0};
  std::string message_body_{"Message Body Content"};
  std::string broker_host_{"10.0.0.1"};
  int broker_port_{10911};
};

TEST_F(ProducerImplTest, testStartShutdown) {
  Scheduler scheduler;
  ON_CALL(*client_manager_, getScheduler).WillByDefault(testing::ReturnRef(scheduler));
  producer_->start();
  producer_->shutdown();
}

TEST_F(ProducerImplTest, testSend) {
  Scheduler scheduler;
  ON_CALL(*client_manager_, getScheduler).WillByDefault(testing::ReturnRef(scheduler));

  std::vector<Partition> partitions;
  Topic topic(arn_, topic_);
  std::vector<Address> broker_addresses{Address(broker_host_, broker_port_)};
  ServiceAddress service_address(AddressScheme::IPv4, broker_addresses);
  Broker broker(broker_name_, broker_id_, service_address);
  Partition partition(topic, queue_id_, Permission::READ_WRITE, broker);

  partitions.emplace_back(partition);

  std::string debug_string;

  TopicRouteDataPtr topic_route_data = std::make_shared<TopicRouteData>(partitions, debug_string);

  auto mock_resolve_route = [&](const std::string& target_host, const Metadata& metadata,
                                const QueryRouteRequest& request, std::chrono::milliseconds timeout,
                                const std::function<void(bool, const TopicRouteDataPtr& ptr)>& cb) {
    cb(true, topic_route_data);
  };

  EXPECT_CALL(*client_manager_, resolveRoute)
      .Times(testing::AtMost(1))
      .WillRepeatedly(testing::Invoke(mock_resolve_route));

  bool cb_invoked = false;
  SendResult send_result;
  auto mock_send = [&](const std::string& target_host, const Metadata& metadata, SendMessageRequest& request,
                       SendCallback* cb) {
    cb->onSuccess(send_result);
    cb_invoked = true;
    return true;
  };

  EXPECT_CALL(*client_manager_, send).Times(testing::AtLeast(1)).WillRepeatedly(testing::Invoke(mock_send));
  producer_->start();

  MQMessage message(topic_, tag_, message_body_);
  producer_->send(message);
  EXPECT_TRUE(cb_invoked);
  producer_->shutdown();
}

ROCKETMQ_NAMESPACE_END