#include "ProcessQueue.h"
#include "ClientInstance.h"
#include "DefaultMQPushConsumerImpl.h"
#include "Metadata.h"
#include "Protocol.h"
#include "Signature.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

using namespace std::chrono;

ROCKETMQ_NAMESPACE_BEGIN

ProcessQueue::ProcessQueue(MQMessageQueue message_queue, FilterExpression filter_expression,
                           ConsumeMessageType consume_type, int max_cache_size,
                           std::weak_ptr<DefaultMQPushConsumerImpl> call_back_owner,
                           std::shared_ptr<ClientInstance> client_instance)
    : message_queue_(std::move(message_queue)), filter_expression_(std::move(filter_expression)),
      consume_type_(consume_type), batch_size_(MixAll::DEFAULT_FETCH_MESSAGE_BATCH_SIZE),
      invisible_time_(MixAll::millisecondsOf(MixAll::DEFAULT_INVISIBLE_TIME_)), max_cache_quantity_(max_cache_size),
      max_cache_memory_(), simple_name_(message_queue_.simpleName()), call_back_owner_(std::move(call_back_owner)),
      client_instance_(std::move(client_instance)), cached_message_quantity_(0), cached_message_memory_(0) {
  SPDLOG_DEBUG("Created ProcessQueue={}", simpleName());
}

ProcessQueue::~ProcessQueue() {
  SPDLOG_INFO("ProcessQueue={} should have been re-balanced away, thus, is destructed", simpleName());
}

void ProcessQueue::callback(std::shared_ptr<ReceiveMessageCallback> callback) { callback_ = std::move(callback); }

bool ProcessQueue::expired() const {
  auto duration = std::chrono::steady_clock::now() - last_poll_timestamp_;
  auto throttle_duration = std::chrono::steady_clock::now() - last_throttle_timestamp_;
  if (duration > MixAll::PROCESS_QUEUE_EXPIRATION_THRESHOLD_ &&
      throttle_duration > MixAll::PROCESS_QUEUE_EXPIRATION_THRESHOLD_) {
    SPDLOG_WARN("ProcessQueue={} is expired. Duration from last poll is: {}ms; from last throttle is: {}ms",
                simpleName(), MixAll::millisecondsOf(duration), MixAll::millisecondsOf(throttle_duration));
    return true;
  }
  return false;
}

bool ProcessQueue::shouldThrottle() const {
  std::size_t current;
  {
    absl::MutexLock lk(&messages_mtx_);
    current = cached_messages_.size() + cached_message_quantity_.load(std::memory_order_relaxed);
  }

  bool need_throttle = current >= max_cache_quantity_;
  if (need_throttle) {
    SPDLOG_INFO("{}: Number of locally cached messages is {}, which exceeds threshold={}", simple_name_, current,
                max_cache_quantity_);
  }
  return need_throttle;
}

void ProcessQueue::receiveMessage() {
  switch (consume_type_) {
  case ConsumeMessageType::POP:
    popMessage();
    break;
  case ConsumeMessageType::PULL:
    pullMessage();
    break;
  }
}

void ProcessQueue::popMessage() {
  rmq::ReceiveMessageRequest request;

  absl::flat_hash_map<std::string, std::string> metadata;
  auto consumer_client = call_back_owner_.lock();
  if (!consumer_client) {
    return;
  }
  Signature::sign(consumer_client.get(), metadata);

  wrapPopMessageRequest(metadata, request);
  last_poll_timestamp_ = std::chrono::steady_clock::now();
  SPDLOG_DEBUG("Try to pop message from {}", message_queue_.simpleName());
  client_instance_->receiveMessage(message_queue_.serviceAddress(), metadata, request,
                                   absl::ToChronoMilliseconds(consumer_client->getIoTimeout()), callback_);
}

void ProcessQueue::pullMessage() {
  rmq::PullMessageRequest request;
  absl::flat_hash_map<std::string, std::string> metadata;
  wrapPullMessageRequest(metadata, request);
  client_instance_->pullMessage(message_queue_.serviceAddress(), metadata, request, callback_);
}

bool ProcessQueue::hasPendingMessages() const {
  absl::MutexLock lk(&messages_mtx_);
  return !cached_messages_.empty();
}

void ProcessQueue::cacheMessages(const std::vector<MQMessageExt>& messages) {
  auto consumer = call_back_owner_.lock();
  if (!consumer) {
    return;
  }

  {
    absl::MutexLock messages_lock_guard(&messages_mtx_);
    // TODO: use lock-when semantics
    absl::MutexLock offsets_lock_guard(&offsets_mtx_);
    for (const auto& message : messages) {
      const std::string& msg_id = message.getMsgId();
      if (!filter_expression_.accept(message)) {
        const std::string& topic = message.getTopic();
        auto callback = [topic, msg_id](bool ok) {
          if (ok) {
            SPDLOG_DEBUG("Ack message[Topic={}, MsgId={}] directly as it fails to pass filter expression", topic,
                         msg_id);
          } else {
            SPDLOG_WARN("Failed to ack message[Topic={}, MsgId={}] directly as it fails to pass filter expression",
                        topic, msg_id);
          }
        };
        consumer->ack(message, callback);
        continue;
      }
      cached_messages_.emplace_back(message);
      cached_message_quantity_.fetch_add(1, std::memory_order_relaxed);
      cached_message_memory_.fetch_add(message.getBody().size(), std::memory_order_relaxed);
      if (MessageModel::BROADCASTING == consumer->messageModel() && consumer->hasCustomOffsetStore()) {
        if (offsets_.size() == 1 && offsets_.begin()->released_) {
          int64_t previously_released = offsets_.begin()->offset_;
          offsets_.erase(OffsetRecord(previously_released));
        }
        offsets_.emplace(message.getQueueOffset());
      }
    }
  }
}

bool ProcessQueue::take(int batch_size, std::vector<MQMessageExt>& messages) {
  absl::MutexLock lock(&messages_mtx_);
  if (cached_messages_.empty()) {
    return false;
  }

  for (auto it = cached_messages_.begin(); it != cached_messages_.end();) {
    if (--batch_size < 0) {
      break;
    }

    messages.push_back(*it);
    it = cached_messages_.erase(it);
  }
  return !cached_messages_.empty();
}

bool ProcessQueue::committedOffset(int64_t& offset) {
  absl::MutexLock lk(&offsets_mtx_);
  if (offsets_.empty()) {
    return false;
  }
  offset = offsets_.begin()->offset_;
  return true;
}

void ProcessQueue::release(uint64_t body_size, int64_t offset) {
  auto consumer = call_back_owner_.lock();
  if (!consumer) {
    return;
  }

  cached_message_quantity_.fetch_sub(1);
  cached_message_memory_.fetch_sub(body_size);

  if (MessageModel::BROADCASTING == consumer->messageModel() && consumer->hasCustomOffsetStore()) {
    absl::MutexLock lk(&offsets_mtx_);
    if (offsets_.size() > 1) {
      offsets_.erase(OffsetRecord(offset));
    } else {
      assert(offsets_.begin()->offset_ == offset);
      offsets_.erase(OffsetRecord(offset));
      offsets_.emplace(OffsetRecord(offset, true));
    }
  }
}

void ProcessQueue::wrapPopMessageRequest(absl::flat_hash_map<std::string, std::string>& metadata,
                                         rmq::ReceiveMessageRequest& request) {
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer = call_back_owner_.lock();
  assert(consumer);
  request.set_client_id(consumer->clientId());
  request.mutable_group()->set_name(consumer->getGroupName());
  request.mutable_group()->set_arn(consumer->arn());
  request.mutable_partition()->set_id(message_queue_.getQueueId());
  request.mutable_partition()->mutable_topic()->set_name(message_queue_.getTopic());
  request.mutable_partition()->mutable_topic()->set_arn(consumer->arn());

  auto search = consumer->getTopicFilterExpressionTable().find(message_queue_.getTopic());
  if (consumer->getTopicFilterExpressionTable().end() != search) {
    FilterExpression expression = search->second;
    switch (expression.type_) {
    case TAG:
      request.mutable_filter_expression()->set_type(rmq::FilterType::TAG);
      request.mutable_filter_expression()->set_expression(expression.content_);
      break;
    case SQL92:
      request.mutable_filter_expression()->set_type(rmq::FilterType::SQL);
      request.mutable_filter_expression()->set_expression(expression.content_);
      break;
    }
  } else {
    request.mutable_filter_expression()->set_type(rmq::FilterType::TAG);
    request.mutable_filter_expression()->set_expression("*");
  }

  // Batch size
  request.set_batch_size(batch_size_);

  // Consume policy
  request.set_consume_policy(rmq::ConsumePolicy::RESUME);

  // Set invisible time
  request.mutable_invisible_duration()->set_seconds(
      std::chrono::duration_cast<std::chrono::seconds>(invisible_time_).count());
  auto fraction = invisible_time_ - std::chrono::duration_cast<std::chrono::seconds>(invisible_time_);
  int32_t nano_seconds = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(fraction).count());
  request.mutable_invisible_duration()->set_nanos(nano_seconds);
}

void ProcessQueue::wrapPullMessageRequest(absl::flat_hash_map<std::string, std::string>& metadata,
                                          rmq::PullMessageRequest& request) {
  std::shared_ptr<DefaultMQPushConsumerImpl> consumer = call_back_owner_.lock();
  assert(consumer);
  request.set_client_id(consumer->clientId());
  request.mutable_group()->set_name(consumer->getGroupName());
  request.mutable_group()->set_arn(consumer->arn());
  request.mutable_partition()->set_id(message_queue_.getQueueId());
  request.mutable_partition()->mutable_topic()->set_name(message_queue_.getTopic());
  request.mutable_partition()->mutable_topic()->set_arn(consumer->arn());
  request.set_offset(next_offset_);
  request.set_batch_size(consumer->receiveBatchSize());
  auto filter_expression_table = consumer->getTopicFilterExpressionTable();
  auto search = filter_expression_table.find(message_queue_.getTopic());
  if (filter_expression_table.end() != search) {
    FilterExpression expression = search->second;
    switch (expression.type_) {
    case TAG:
      request.mutable_filter_expression()->set_type(rmq::FilterType::TAG);
      request.mutable_filter_expression()->set_expression(expression.content_);
      break;
    case SQL92:
      request.mutable_filter_expression()->set_type(rmq::FilterType::SQL);
      request.mutable_filter_expression()->set_expression(expression.content_);
      break;
    }
  } else {
    request.mutable_filter_expression()->set_type(rmq::FilterType::TAG);
    request.mutable_filter_expression()->set_expression("*");
  }
}

std::weak_ptr<DefaultMQPushConsumerImpl> ProcessQueue::getCallbackOwner() { return call_back_owner_; }

std::shared_ptr<ClientInstance> ProcessQueue::getClientInstance() { return client_instance_; }

MQMessageQueue ProcessQueue::getMQMessageQueue() { return message_queue_; }

const FilterExpression& ProcessQueue::getFilterExpression() const { return filter_expression_; }

ROCKETMQ_NAMESPACE_END