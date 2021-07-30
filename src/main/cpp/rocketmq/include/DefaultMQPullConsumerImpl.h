#pragma once

#include <future>
#include <memory>

#include "ClientConfig.h"
#include "ClientImpl.h"
#include "ClientInstance.h"
#include "rocketmq/ConsumerType.h"
#include "rocketmq/MQMessageQueue.h"

ROCKETMQ_NAMESPACE_BEGIN

class DefaultMQPullConsumerImpl : public ClientImpl,
                                  public std::enable_shared_from_this<DefaultMQPullConsumerImpl> {
public:
  explicit DefaultMQPullConsumerImpl(std::string group_name) : ClientImpl(std::move(group_name)) {}

  void start() override;

  void shutdown() override;

  std::future<std::vector<MQMessageQueue>> queuesFor(const std::string& topic);

  std::future<int64_t> queryOffset(const OffsetQuery& query);

  void pull(const PullMessageQuery& query, PullCallback* callback);

  void prepareHeartbeatData(HeartbeatRequest& request) override;

protected:
  std::shared_ptr<ClientImpl> self() override {
    return shared_from_this();
  }
};

ROCKETMQ_NAMESPACE_END