#include "ConsumeMessageService.h"
#include "DefaultMQPushConsumerImpl.h"
#include "MessageAccessor.h"
#include <limits>

ROCKETMQ_NAMESPACE_BEGIN

ConsumeMessageOrderlyService ::ConsumeMessageOrderlyService(std::weak_ptr<DefaultMQPushConsumerImpl> consumer_impl_ptr,
                                                            int thread_count, MQMessageListener* message_listener_ptr)
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

void ConsumeMessageOrderlyService::submitConsumeTask0(const std::shared_ptr<DefaultMQPushConsumerImpl>& consumer,
                                                      ProcessQueueWeakPtr process_queue,
                                                      std::vector<MQMessageExt> messages) {
  // In case custom executor is used.
  const Executor& custom_executor = consumer->customExecutor();
  if (custom_executor) {
    std::function<void(void)> consume_task =
        std::bind(&ConsumeMessageOrderlyService::consumeTask, this, process_queue, messages);
    custom_executor(consume_task);
    SPDLOG_DEBUG("Submit FIFO consume task to custom executor");
    return;
  }

  // submit batch message
  std::function<void(void)> consume_task =
      std::bind(&ConsumeMessageOrderlyService::consumeTask, this, process_queue, messages);
  SPDLOG_DEBUG("Submit FIFO consume task to thread pool");
  pool_->Add(consume_task);
}

void ConsumeMessageOrderlyService::submitConsumeTask(const ProcessQueueWeakPtr& process_queue) {
  ProcessQueueSharedPtr process_queue_ptr = process_queue.lock();
  if (!process_queue_ptr) {
    SPDLOG_INFO("Process queue has destructed");
    return;
  }

  auto consumer = consumer_weak_ptr_.lock();
  if (!consumer) {
    SPDLOG_INFO("Consumer has destructed");
    return;
  }

  if (process_queue_ptr->bindFifoConsumeTask()) {
    std::vector<MQMessageExt> messages;
    process_queue_ptr->take(consumer->consumeBatchSize(), messages);
    assert(!messages.empty());

    submitConsumeTask0(consumer, process_queue, std::move(messages));
  }
}

MessageListenerType ConsumeMessageOrderlyService::getConsumeMsgServiceListenerType() {
  return MessageListenerType::messageListenerOrderly;
}

void ConsumeMessageOrderlyService::consumeTask(const ProcessQueueWeakPtr& process_queue,
                                               std::vector<MQMessageExt>& msgs) {
  ProcessQueueSharedPtr process_queue_ptr = process_queue.lock();
  if (!process_queue_ptr) {
    return;
  }
  std::string topic = msgs.begin()->getTopic();
  ConsumeStatus status;
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer = consumer_weak_ptr_.lock();
  // consumer might have been destructed.
  if (!consumer) {
    return;
  }

  std::shared_ptr<RateLimiter<10>> rate_limiter = rateLimiter(topic);
  if (rate_limiter) {
    // Acquire permits one-by-one to avoid large batch hungry issue.
    for (std::size_t i = 0; i < msgs.size(); i++) {
      rate_limiter->acquire();
    }
    SPDLOG_DEBUG("{} rate-limit permits acquired", msgs.size());
  }

  auto steady_start = std::chrono::steady_clock::now();

  try {
    assert(nullptr != message_listener_ptr_);
    status = message_listener_ptr_->consumeMessage(msgs);
  } catch (...) {
    status = RECONSUME_LATER;
    SPDLOG_ERROR("Business FIFO callback raised an exception when consumeMessage");
  }

  auto duration = std::chrono::steady_clock::now() - steady_start;

  // Log client consume-time costs
  SPDLOG_DEBUG("Business callback spent {}ms processing {} messages.", MixAll::millisecondsOf(duration), msgs.size());

  if (MessageModel::CLUSTERING == consumer->messageModel()) {
    if (status == CONSUME_SUCCESS) {
      for (const auto& msg : msgs) {
        const std::string& message_id = msg.getMsgId();
        // Release message number and memory quota
        process_queue_ptr->release(message_id, msg.getQueueOffset());
        auto callback = [process_queue_ptr, message_id](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Acknowledge FIFO message[MessageQueue={}, MsgId={}] OK", process_queue_ptr->simpleName(),
                         message_id);
          } else {
            SPDLOG_WARN("Failed to acknowledge FIFO message[MessageQueue={}, MsgId={}]",
                        process_queue_ptr->simpleName(), message_id);
          }
        };
        consumer->ack(msg, callback);
      }
      process_queue_ptr->unbindFifoConsumeTask();
      signalDispatcher();
    } else {
      int32_t min_reconsume_times = std::numeric_limits<int32_t>::max();
      for (auto& msg : msgs) {
        MessageAccessor::setAttemptTimes(msg, msg.getReconsumeTimes() + 1);
        if (msg.getReconsumeTimes() < min_reconsume_times) {
          min_reconsume_times = msg.getReconsumeTimes();
        }
      }

      if (min_reconsume_times < consumer->max_delivery_attempts_) {
        // Submit consume task to thread pool.
        submitConsumeTask0(consumer, process_queue, std::move(msgs));
        SPDLOG_INFO("Business callback failed to process FIFO messages. Re-submit consume task back to thread pool");
      } else {
        process_queue_ptr->unbindFifoConsumeTask();
        // TODO: Move them to DLQ
      }
    }
  } else if (MessageModel::BROADCASTING == consumer->messageModel()) {
    for (const auto& msg : msgs) {
      process_queue_ptr->release(msg.getMsgId(), msg.getQueueOffset());
      process_queue_ptr->nextOffset(msg.getQueueOffset());
    }

    if (consumer->offset_store_) {
      int64_t committed_offset;
      if (process_queue_ptr->committedOffset(committed_offset)) {
        consumer->offset_store_->updateOffset(process_queue_ptr->getMQMessageQueue(), committed_offset);
      }
    }
  }
}

ROCKETMQ_NAMESPACE_END