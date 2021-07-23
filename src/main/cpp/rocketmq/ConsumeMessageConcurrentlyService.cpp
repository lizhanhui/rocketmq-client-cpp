#include "ConsumeMessageService.h"
#include "DefaultMQPushConsumerImpl.h"
#include "LoggerImpl.h"
#include "Protocol.h"
#include "TracingUtility.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include <limits>

ROCKETMQ_NAMESPACE_BEGIN

ConsumeMessageConcurrentlyService::ConsumeMessageConcurrentlyService(std::weak_ptr<DefaultMQPushConsumerImpl> consumer,
                                                                     int thread_count,
                                                                     MQMessageListener* message_listener_ptr)
    : ConsumeMessageService(std::move(consumer), thread_count, message_listener_ptr) {}

void ConsumeMessageConcurrentlyService::start() {
  ConsumeMessageService::start();
  State expected = State::STARTING;
  if (state_.compare_exchange_strong(expected, State::STARTED)) {
    SPDLOG_DEBUG("ConsumeMessageConcurrentlyService started");
  }
}

void ConsumeMessageConcurrentlyService::shutdown() {
  while (State::STARTING == state_.load(std::memory_order_relaxed)) {
    absl::SleepFor(absl::Milliseconds(10));
  }

  State expected = State::STARTED;
  if (state_.compare_exchange_strong(expected, State::STOPPING)) {
    ConsumeMessageService::shutdown();
    SPDLOG_DEBUG("ConsumeMessageConcurrentlyService shut down");
  }
}

void ConsumeMessageConcurrentlyService::submitConsumeTask(const ProcessQueueWeakPtr& process_queue) {
  ProcessQueueSharedPtr process_queue_ptr = process_queue.lock();
  if (!process_queue_ptr) {
    SPDLOG_WARN("ProcessQueue was destructed. It is likely that client should have shutdown.");
    return;
  }
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer_impl_ptr = process_queue_ptr->getCallbackOwner().lock();

  if (!consumer_impl_ptr) {
    return;
  }

  std::string topic = process_queue_ptr->topic();
  bool has_more = true;
  while (has_more) {
    std::vector<MQMessageExt> messages;
    uint32_t batch_size = consumer_impl_ptr->consumeBatchSize();
    has_more = process_queue_ptr->take(batch_size, messages);
    if (messages.empty()) {
      assert(!has_more);
      break;
    }

    // In case custom executor is used.
    const Executor& custom_executor = consumer_impl_ptr->customExecutor();
    if (custom_executor) {
      std::function<void(void)> consume_task =
          std::bind(&ConsumeMessageConcurrentlyService::consumeTask, this, process_queue, messages);
      custom_executor(consume_task);
      SPDLOG_DEBUG("Submit consumer task to custom executor with message-batch-size={}", messages.size());
      continue;
    }

    // submit batch message
    std::function<void(void)> consume_task =
        std::bind(&ConsumeMessageConcurrentlyService::consumeTask, this, process_queue_ptr, messages);
    SPDLOG_DEBUG("Submit consumer task to thread pool with message-batch-size={}", messages.size());
    pool_->Add(consume_task);
  }
}

MessageListenerType ConsumeMessageConcurrentlyService::getConsumeMsgServiceListenerType() {
  return messageListenerConcurrently;
}

void ConsumeMessageConcurrentlyService::dispatch() {
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer = consumer_weak_ptr_.lock();
  if (!consumer) {
    SPDLOG_WARN("The consumer has already destructed");
    return;
  }

  auto lambda = [this](const ProcessQueueSharedPtr& process_queue_ptr) { submitConsumeTask(process_queue_ptr); };
  consumer->iterateProcessQueue(lambda);
}

void ConsumeMessageConcurrentlyService::consumeTask(const ProcessQueueWeakPtr& process_queue,
                                                    const std::vector<MQMessageExt>& msgs) {
  ProcessQueueSharedPtr process_queue_shared_ptr = process_queue.lock();
  if (!process_queue_shared_ptr || msgs.empty()) {
    return;
  }
  std::string topic = msgs.begin()->getTopic();
  ConsumeStatus status;
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer = consumer_weak_ptr_.lock();
  // consumer might have been destructed.
  if (!consumer) {
    return;
  }
  // consumer does not start yet.
#ifdef ENABLE_TRACING
  nostd::shared_ptr<trace::Tracer> tracer = consumer_shared_ptr->getTracer();
  if (!tracer) {
    return;
  }
  auto system_start = std::chrono::system_clock::now();
#endif

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
    SPDLOG_ERROR("Business callback raised an exception when consumeMessage");
  }

  auto duration = std::chrono::steady_clock::now() - steady_start;

  // Log client consume-time costs
  SPDLOG_DEBUG("Business callback spent {}ms processing {} messages.", MixAll::millisecondsOf(duration), msgs.size());

#ifdef ENABLE_TRACING
  std::chrono::microseconds average_duration =
      std::chrono::microseconds(MixAll::microsecondsOf(duration) / msgs.size());
#endif

  for (const auto& msg : msgs) {
    const std::string& message_id = msg.getMsgId();

    // Release message number and memory quota
    process_queue_shared_ptr->releaseQuota(message_id);

#ifdef ENABLE_TRACING
    nostd::shared_ptr<trace::Span> span = nostd::shared_ptr<trace::Span>(nullptr);
    trace::EndSpanOptions end_options;
    if (consumer_shared_ptr->isTracingEnabled()) {
      const std::string& serialized_span_context = msg.traceContext();
      trace::SpanContext span_context = TracingUtility::extractContextFromTraceParent(serialized_span_context);
      trace::StartSpanOptions start_options;
      start_options.start_system_time =
          opentelemetry::core::SystemTimestamp(system_start + i * std::chrono::microseconds(average_duration));
      start_options.start_steady_time =
          opentelemetry::core::SteadyTimestamp(steady_start + i * std::chrono::microseconds(average_duration));
      start_options.parent = span_context;
      end_options.end_steady_time =
          opentelemetry::core::SteadyTimestamp(steady_start + (i + 1) * std::chrono::microseconds(average_duration));
      span = tracer->StartSpan("ConsumeMessage", start_options);
    }
#endif

#ifdef ENABLE_TRACING
    if (span) {
      span->SetAttribute(TracingUtility::get().expired_, false);
      span->SetAttribute(TracingUtility::get().success_, CONSUME_SUCCESS == status);
      span->End(end_options);
    }
#endif

    if (MessageModel::CLUSTERING == consumer->messageModel()) {
      if (status == CONSUME_SUCCESS) {
        auto callback = [process_queue_shared_ptr, message_id](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Acknowledge message[MessageQueue={}, MsgId={}] OK", process_queue_shared_ptr->simpleName(),
                         message_id);
          } else {
            SPDLOG_WARN("Failed to acknowledge message[MessageQueue={}, MsgId={}]",
                        process_queue_shared_ptr->simpleName(), message_id);
          }
        };
        consumer->ack(msg, callback);
      } else {
        auto callback = [process_queue_shared_ptr, message_id](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Nack message[MessageQueue={}, MsgId={}] OK", process_queue_shared_ptr->simpleName(),
                         message_id);
          } else {
            SPDLOG_INFO(
                "Failed to negative acknowledge message[MessageQueue={}, MsgId={}]. Message will be re-consumed "
                "after default invisible time",
                process_queue_shared_ptr->simpleName(), message_id);
          }
        };
        consumer->nack(msg, callback);
      }
    }
  }

  if (MessageModel::BROADCASTING == consumer->messageModel()) {
    if (consumer->offset_store_) {
      consumer->offset_store_->updateOffset(process_queue_shared_ptr->getMQMessageQueue(),
                                            process_queue_shared_ptr->nextOffset());
    }
  }
}

ROCKETMQ_NAMESPACE_END