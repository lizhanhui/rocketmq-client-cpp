#include <chrono>
#include <memory>

#include "DynamicNameServerResolver.h"
#include "PushConsumerImpl.h"
#include "StaticNameServerResolver.h"
#include "rocketmq/DefaultMQPushConsumer.h"

ROCKETMQ_NAMESPACE_BEGIN

DefaultMQPushConsumer::DefaultMQPushConsumer(const std::string& group_name) {
  impl_ = std::make_shared<PushConsumerImpl>(group_name);
}

void DefaultMQPushConsumer::start() { impl_->start(); }

void DefaultMQPushConsumer::shutdown() {
  impl_->shutdown();
  SPDLOG_DEBUG("PushConsumerImpl shared_ptr use_count={}", impl_.use_count());
}

void DefaultMQPushConsumer::subscribe(const std::string& topic, const std::string& expression,
                                      ExpressionType expression_type) {
  impl_->subscribe(topic, expression, expression_type);
}

void DefaultMQPushConsumer::setConsumeFromWhere(ConsumeFromWhere policy) { impl_->setConsumeFromWhere(policy); }

void DefaultMQPushConsumer::registerMessageListener(MessageListener* listener) {
  impl_->registerMessageListener(listener);
}

void DefaultMQPushConsumer::setNamesrvAddr(const std::string& name_srv) {
  auto name_server_resolver = std::make_shared<StaticNameServerResolver>(name_srv);
  impl_->withNameServerResolver(name_server_resolver);
}

void DefaultMQPushConsumer::setNameServerListDiscoveryEndpoint(const std::string& discovery_endpoint) {
  if (discovery_endpoint.empty()) {
    return;
  }

  auto name_server_resolver = std::make_shared<DynamicNameServerResolver>(discovery_endpoint, std::chrono::seconds(10));
  impl_->withNameServerResolver(name_server_resolver);
}

void DefaultMQPushConsumer::setGroupName(const std::string& group_name) { impl_->setGroupName(group_name); }

void DefaultMQPushConsumer::setConsumeThreadCount(int thread_count) { impl_->consumeThreadPoolSize(thread_count); }

void DefaultMQPushConsumer::setInstanceName(const std::string& instance_name) { impl_->setInstanceName(instance_name); }

int DefaultMQPushConsumer::getProcessQueueTableSize() { return impl_->getProcessQueueTableSize(); }

void DefaultMQPushConsumer::setUnitName(std::string unit_name) { impl_->setUnitName(std::move(unit_name)); }

const std::string& DefaultMQPushConsumer::getUnitName() const { return impl_->getUnitName(); }

void DefaultMQPushConsumer::enableTracing(bool enabled) { impl_->enableTracing(enabled); }

bool DefaultMQPushConsumer::isTracingEnabled() { return impl_->isTracingEnabled(); }

void DefaultMQPushConsumer::setAsyncPull(bool) {}

void DefaultMQPushConsumer::setConsumeMessageBatchMaxSize(int batch_size) { impl_->consumeBatchSize(batch_size); }

void DefaultMQPushConsumer::setCustomExecutor(const Executor& executor) { impl_->setCustomExecutor(executor); }

void DefaultMQPushConsumer::setThrottle(const std::string& topic, uint32_t threshold) {
  impl_->setThrottle(topic, threshold);
}

void DefaultMQPushConsumer::setResourceNamespace(const std::string& resource_namespace) {
  impl_->resourceNamespace(resource_namespace);
}

void DefaultMQPushConsumer::setCredentialsProvider(CredentialsProviderPtr credentials_provider) {
  impl_->setCredentialsProvider(std::move(credentials_provider));
}

void DefaultMQPushConsumer::setMessageModel(MessageModel message_model) { impl_->setMessageModel(message_model); }

std::string DefaultMQPushConsumer::groupName() const { return impl_->getGroupName(); }

ROCKETMQ_NAMESPACE_END