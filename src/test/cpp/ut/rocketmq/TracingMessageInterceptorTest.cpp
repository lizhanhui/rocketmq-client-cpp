#include "TracingMessageInterceptor.h"
#include "iostream"
#include "gtest/gtest.h"

ROCKETMQ_NAMESPACE_BEGIN

class DummyBaseImpl : public BaseImpl {
public:
  DummyBaseImpl(std::string group_name) : BaseImpl(std::move(group_name)){};

  void prepareHeartbeatData(HeartbeatRequest& request) override {}

  void updateCache() {
    std::vector<Partition> partitions;
    Topic topic("dummyArn", "dummyTopicName");
    int32_t partition_id = 0;

    std::vector<Address> addresses;

    Address address("127.0.0.1", 10911);
    addresses.emplace_back(address);

    ServiceAddress service_address(AddressScheme::IPv4, addresses);
    Broker broker("dummyBrokerName", 0, service_address);
    Partition partition(topic, partition_id, Permission::READ_WRITE, broker);

    partitions.emplace_back(partition);
    TopicRouteDataPtr topic_route_data = std::make_shared<TopicRouteData>(partitions, "debug_string");

    topic_route_table_.insert({"dummyTopicName", topic_route_data});
    updateTraceProvider();
  }
};

TEST(TracingMessageInterceptorTest, testSendMessage) {
  std::shared_ptr<DummyBaseImpl> base_impl = std::make_shared<DummyBaseImpl>("dummyGroup");
  base_impl->updateCache();

  MQMessageExt messageExt;
  std::shared_ptr<TracingMessageInterceptor> interceptor = std::make_shared<TracingMessageInterceptor>(base_impl);
  MessageInterceptorContext context;
  interceptor->intercept(MessageHookPoint::PRE_SEND_MESSAGE, messageExt, context);
  ASSERT_EQ(1, interceptor->inflightSpanNum());
  interceptor->intercept(MessageHookPoint::POST_SEND_MESSAGE, messageExt, context);
  ASSERT_EQ(0, interceptor->inflightSpanNum());
}

ROCKETMQ_NAMESPACE_END