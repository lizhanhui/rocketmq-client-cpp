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

void ConsumeMessageConcurrentlyService::submitConsumeTask(const ProcessQueueWeakPtr& process_queue, int32_t permits) {
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
  std::shared_ptr<RateLimiter<10>> rate_limiter = rateLimiter(topic);
  bool has_more = true;
  while (has_more) {
    std::vector<MQMessageExt> messages;
    uint32_t batch_size = consumer_impl_ptr->consumeBatchSize();
    uint32_t acquired;
    if (rate_limiter) {
      uint32_t permits_to_acquire =
          std::min({batch_size, rate_limiter->available(), process_queue_ptr->cachedMessagesSize()});
      acquired = rate_limiter->acquire(permits_to_acquire);
      if (!acquired) {
        SPDLOG_DEBUG("Throttled: failed to acquire permits from rate limiter");
        break;
      }
    } else if (permits < 0) {
      acquired = batch_size;
    } else {
      SPDLOG_WARN("submitConsumeTask with permits = 0");
      break;
    }

    has_more = process_queue_ptr->consume(acquired, messages);
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

  auto lambda = [this](const ProcessQueueSharedPtr& process_queue_ptr) {
    // Note: we have already got process_queue_table_mtx_ locked
    std::string topic = process_queue_ptr->topic();
    if (hasConsumeRateLimiter(topic)) {
      std::shared_ptr<RateLimiter<10>> rate_limiter = rateLimiter(topic);
      if (rate_limiter) {
        uint32_t permits = rate_limiter->available();
        if (permits) {
          submitConsumeTask(process_queue_ptr, permits);
        }
      } else {
        submitConsumeTask(process_queue_ptr, -1);
      }
    } else {
      submitConsumeTask(process_queue_ptr, -1);
    }
  };
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

  auto steady_start = std::chrono::steady_clock::now();

  try {
    assert(nullptr != message_listener_ptr_);
    status = message_listener_ptr_->consumeMessage(msgs);
  } catch (...) {
    status = RECONSUME_LATER;
    SPDLOG_ERROR("Business callback raised an exception when consumeMessage");
  }

  auto duration = std::chrono::steady_clock::now() - steady_start;
#ifdef ENABLE_TRACING
  std::chrono::microseconds average_duration =
      std::chrono::microseconds(MixAll::microsecondsOf(duration) / msgs.size());
#endif

  std::vector<std::string> msg_id_list;
  msg_id_list.reserve(msgs.size());

  for (const auto& msg : msgs) {
    msg_id_list.emplace_back(msg.getMsgId());
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

    process_queue_shared_ptr->messageCachedNumber().fetch_sub(1, std::memory_order_relaxed);

#ifdef ENABLE_TRACING
    if (span) {
      span->SetAttribute(TracingUtility::get().expired_, false);
      span->SetAttribute(TracingUtility::get().success_, CONSUME_SUCCESS == status);
      span->End(end_options);
    }
#endif

    if (MessageModel::CLUSTERING == consumer->messageModel()) {
      if (status == CONSUME_SUCCESS) {
        auto ack_callback = [process_queue_shared_ptr, msg](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Acknowledge message[MessageQueue={}, MsgId={}] OK", process_queue_shared_ptr->simpleName(),
                         msg.getMsgId());
          } else {
            SPDLOG_WARN("Failed to acknowledge message[MessageQueue={}, MsgId={}]",
                        process_queue_shared_ptr->simpleName(), msg.getMsgId());
          }
        };
        consumer->ack(msg, ack_callback);
      } else {
        auto nack_callback = [process_queue_shared_ptr, msg](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Nack message[MessageQueue={}, MsgId={}] OK", process_queue_shared_ptr->simpleName(),
                         msg.getMsgId());
          } else {
            SPDLOG_INFO(
                "Failed to negative acknowledge message[MessageQueue={}, MsgId={}]. Message will be re-consumed "
                "after default invisible time",
                process_queue_shared_ptr->simpleName(), msg.getMsgId());
          }
        };
        consumer->nack(msg, nack_callback);
      }
    }
  }

  if (MessageModel::BROADCASTING == consumer->messageModel()) {
    if (consumer->offset_store_) {
      consumer->offset_store_->updateOffset(process_queue_shared_ptr->getMQMessageQueue(),
                                            process_queue_shared_ptr->nextOffset());
    }
  }

  // Log client consume-time costs
  SPDLOG_DEBUG("Business callback spent {}ms processing {} messages. Message-identifier-list: {}",
               MixAll::millisecondsOf(duration), msgs.size(),
               absl::StrJoin(msg_id_list.begin(), msg_id_list.end(), ","));

  bool signal_dispatcher = false;
  {
    absl::MutexLock lk(&rate_limiter_table_mtx_);
    if (rate_limiter_table_.contains(topic)) {
      signal_dispatcher = true;
    }
  }

  if (signal_dispatcher) {
    signalDispatcher();
  }
}

ROCKETMQ_NAMESPACE_END