#include "ConsumeMessageService.h"
#include "DefaultMQPushConsumerImpl.h"

ROCKETMQ_NAMESPACE_BEGIN

ConsumeMessageOrderlyService ::ConsumeMessageOrderlyService(
    std::weak_ptr<DefaultMQPushConsumerImpl> consumer_impl_ptr, int thread_count,
    MQMessageListener* message_listener_ptr)
    : ConsumeMessageService(std::move(consumer_impl_ptr), thread_count, message_listener_ptr) {
  // Suppress field not used warning for now
  (void)message_listener_ptr_;
}

void ConsumeMessageOrderlyService::start() {
  ConsumeMessageService::start();
  State expected = State::STARTING;
  if (state_.compare_exchange_strong(expected, State::STARTED)) {
    SPDLOG_DEBUG("ConsumeMessageOrderlyService started");
  }
}

void ConsumeMessageOrderlyService::shutdown() {
  // Wait till consume-message-orderly-service has fully started; otherwise, we may potentially miss closing resources
  // in concurrent scenario.
  while (State::STARTING == state_.load(std::memory_order_relaxed)) {
    absl::SleepFor(absl::Milliseconds(10));
  }

  State expected = State::STARTED;
  if (state_.compare_exchange_strong(expected, STOPPING)) {
    ConsumeMessageService::shutdown();
    SPDLOG_INFO("ConsumeMessageOrderlyService shut down");
  }
}

void ConsumeMessageOrderlyService::submitConsumeTask(const ProcessQueueWeakPtr& process_queue, int32_t permits) {}

MessageListenerType ConsumeMessageOrderlyService::getConsumeMsgServiceListenerType() {
  return MessageListenerType::messageListenerOrderly;
}

void ConsumeMessageOrderlyService::dispatch() {
  // Not implemented.
}

void ConsumeMessageOrderlyService::consumeTask(const ProcessQueueWeakPtr& process_queue,
                                               const std::vector<MQMessageExt>& msgs) {}

ROCKETMQ_NAMESPACE_END