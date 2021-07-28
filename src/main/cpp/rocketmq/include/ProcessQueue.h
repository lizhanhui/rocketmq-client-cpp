#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <set>

#include "ClientInstance.h"
#include "FilterExpression.h"
#include "MixAll.h"
#include "TopicAssignmentInfo.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "apache/rocketmq/v1/service.pb.h"
#include "rocketmq/ConsumerType.h"
#include "rocketmq/MQMessageExt.h"
#include "rocketmq/MQMessageQueue.h"

ROCKETMQ_NAMESPACE_BEGIN

struct OffsetRecord {
  explicit OffsetRecord(int64_t offset) : offset_(offset), released_(false) {}
  OffsetRecord(int64_t offset, bool released) : offset_(offset), released_(released) {}
  int64_t offset_;
  bool released_;
};

ROCKETMQ_NAMESPACE_END

namespace std {

template <> struct less<ROCKETMQ_NAMESPACE::OffsetRecord> {
  bool operator()(const ROCKETMQ_NAMESPACE::OffsetRecord& lhs, const ROCKETMQ_NAMESPACE::OffsetRecord& rhs) {
    return lhs.offset_ < rhs.offset_;
  }
};

} // namespace std

ROCKETMQ_NAMESPACE_BEGIN

class DefaultMQPushConsumerImpl;

/**
 * @brief Once messages are fetched(either pulled or popped) from remote server, they are firstly put into cache.
 * Dispatcher thread, after waking up, will submit them into thread-pool. Messages at this phase are called "inflight"
 * state. Once messages are processed by user-passed-in callback, their quota will be released for future incoming
 * messages.
 */
class ProcessQueue {
public:
  ProcessQueue(MQMessageQueue message_queue, FilterExpression filter_expression, ConsumeMessageType consume_type,
               int max_cache_size, std::weak_ptr<DefaultMQPushConsumerImpl> call_back_owner,
               std::shared_ptr<ClientInstance> client_instance);

  ~ProcessQueue();

  void callback(std::shared_ptr<ReceiveMessageCallback> callback);

  MQMessageQueue getMQMessageQueue();

  bool expired() const;

  bool shouldThrottle() const LOCKS_EXCLUDED(messages_mtx_);

  const FilterExpression& getFilterExpression() const;

  std::weak_ptr<DefaultMQPushConsumerImpl> getCallbackOwner();

  std::shared_ptr<ClientInstance> getClientInstance();

  void receiveMessage();

  const std::string& simpleName() const { return simple_name_; }

  std::string topic() const { return message_queue_.getTopic(); }

  bool hasPendingMessages() const LOCKS_EXCLUDED(messages_mtx_);

  /**
   * Put message fetched from broker into cache.
   *
   * @param messages
   */
  void cacheMessages(const std::vector<MQMessageExt>& messages) LOCKS_EXCLUDED(messages_mtx_, offsets_mtx_);

  /**
   * @return Number of messages that is not yet dispatched to thread pool, likely, due to topic-rate-limiting.
   */
  uint32_t cachedMessagesSize() const LOCKS_EXCLUDED(messages_mtx_) {
    absl::MutexLock lk(&messages_mtx_);
    return cached_messages_.size();
  }

  /**
   * Dispatch messages from cache to thread pool in form of consumeTask.
   * @param batch_size
   * @param messages
   * @return true if there are more messages to consume in cache
   */
  bool take(uint32_t batch_size, std::vector<MQMessageExt>& messages) LOCKS_EXCLUDED(messages_mtx_);

  void updateThrottleTimestamp() { last_throttle_timestamp_ = std::chrono::steady_clock::now(); }

  ConsumeMessageType consumeType() const { return consume_type_; }

  void nextOffset(int64_t next_offset) {
    assert(next_offset >= 0);
    next_offset_ = next_offset;
  }

  int64_t nextOffset() const { return next_offset_; }

  bool committedOffset(int64_t& offset) LOCKS_EXCLUDED(offsets_mtx_);

  void release(uint64_t body_size, int64_t offset) LOCKS_EXCLUDED(messages_mtx_, offsets_mtx_);

  bool unbindFifoConsumeTask() {
    bool expected = true;
    return has_fifo_task_bound_.compare_exchange_strong(expected, false, std::memory_order_relaxed);
  }

  bool bindFifoConsumeTask() {
    bool expected = false;
    return has_fifo_task_bound_.compare_exchange_strong(expected, true, std::memory_order_relaxed);
  }

  void fetchBatchSize(int32_t fetch_batch_size) {
    fetch_batch_size_ = fetch_batch_size;
  }

private:
  MQMessageQueue message_queue_;

  /**
   * Expression used to filter message in the server side.
   */
  const FilterExpression filter_expression_;

  ConsumeMessageType consume_type_{ConsumeMessageType::POP};

  int fetch_batch_size_;

  std::chrono::milliseconds invisible_time_;

  std::chrono::steady_clock::time_point last_poll_timestamp_{std::chrono::steady_clock::now()};

  std::chrono::steady_clock::time_point last_throttle_timestamp_{std::chrono::steady_clock::now()};

  absl::Time born_timestamp_{absl::Now()};

  ConsumeInitialMode initial_mode_{ConsumeInitialMode::MAX};

  /**
   * Maximum number of locally cached messages. Once exceeding this threshold, fetch-loop should be throttled.
   */
  uint32_t max_cache_quantity_;

  /**
   * Max cached memory in bytes
   */
  uint64_t max_cache_memory_;

  std::string simple_name_;

  // callback
  std::weak_ptr<DefaultMQPushConsumerImpl> call_back_owner_;
  std::shared_ptr<ClientInstance> client_instance_;

  std::shared_ptr<ReceiveMessageCallback> callback_;

  /**
   * Messages that are pending to be submitted to thread pool.
   */
  mutable std::vector<MQMessageExt> cached_messages_ GUARDED_BY(messages_mtx_);

  mutable absl::Mutex messages_mtx_;

  /**
   * @brief Quantity of the cached messages.
   *
   */
  std::atomic<uint32_t> cached_message_quantity_;

  /**
   * @brief Total body memory size of the cached messages.
   *
   */
  std::atomic<uint64_t> cached_message_memory_;

  int64_t next_offset_{-1};

  /**
   * If this process queue is used in FIFO scenario, this field marks if there is an task in thread pool.
   */
  std::atomic_bool has_fifo_task_bound_{false};

  std::set<OffsetRecord> offsets_ GUARDED_BY(offsets_mtx_);
  absl::Mutex offsets_mtx_;

  void popMessage();
  void wrapPopMessageRequest(absl::flat_hash_map<std::string, std::string>& metadata,
                             rmq::ReceiveMessageRequest& request);

  void pullMessage();
  void wrapPullMessageRequest(absl::flat_hash_map<std::string, std::string>& metadata,
                              rmq::PullMessageRequest& request);
};

using ProcessQueueSharedPtr = std::shared_ptr<ProcessQueue>;
using ProcessQueueWeakPtr = std::weak_ptr<ProcessQueue>;

ROCKETMQ_NAMESPACE_END