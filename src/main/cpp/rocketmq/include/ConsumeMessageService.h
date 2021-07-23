#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "ProcessQueue.h"
#include "RateLimiter.h"
#include "rocketmq/MQMessageListener.h"
#include "rocketmq/State.h"
#include "src/cpp/server/dynamic_thread_pool.h"

ROCKETMQ_NAMESPACE_BEGIN

class DefaultMQPushConsumerImpl;

class ConsumeMessageService {
public:
  ConsumeMessageService(std::weak_ptr<DefaultMQPushConsumerImpl> consumer, int thread_count,
                        MQMessageListener* message_listener_ptr);

  virtual ~ConsumeMessageService() = default;

  /**
   * Make it noncopyable.
   */
  ConsumeMessageService(const ConsumeMessageService& other) = delete;
  ConsumeMessageService& operator=(const ConsumeMessageService& other) = delete;

  /**
   * Start the dispatcher thread, which will dispatch messages in process queue to thread pool in form of runnable
   * functor.
   */
  virtual void start();

  /**
   * Stop the dispatcher thread and then reset the thread pool.
   */
  virtual void shutdown();

  virtual void submitConsumeTask(const ProcessQueueWeakPtr& process_queue_ptr) = 0;

  virtual MessageListenerType getConsumeMsgServiceListenerType() = 0;

  /**
   * Signal dispatcher thread to check new pending messages.
   */
  void signalDispatcher();

  /**
   * Set throttle threshold per topic.
   *
   * @param topic
   * @param threshold
   */
  void throttle(const std::string& topic, uint32_t threshold);

  bool hasConsumeRateLimiter(const std::string& topic) const LOCKS_EXCLUDED(rate_limiter_table_mtx_);

  std::shared_ptr<RateLimiter<10>> rateLimiter(const std::string& topic) const LOCKS_EXCLUDED(rate_limiter_table_mtx_);

protected:
  RateLimiterObserver rate_limiter_observer_;

  mutable absl::flat_hash_map<std::string, std::shared_ptr<RateLimiter<10>>>
      rate_limiter_table_ GUARDED_BY(rate_limiter_table_mtx_);
  mutable absl::Mutex rate_limiter_table_mtx_; // Protects rate_limiter_table_

  std::atomic<State> state_;

  int thread_count_;
  std::unique_ptr<grpc::ThreadPoolInterface> pool_;
  std::weak_ptr<DefaultMQPushConsumerImpl> consumer_weak_ptr_;

  absl::Mutex dispatch_mtx_;
  std::thread dispatch_thread_;
  absl::CondVar dispatch_cv_;

  MQMessageListener* message_listener_ptr_;

  /**
   * Dispatch messages to thread pool. Implementation of this function should be sub-class specific.
   */
  virtual void dispatch() = 0;
};

class ConsumeMessageConcurrentlyService : public ConsumeMessageService {
public:
  ConsumeMessageConcurrentlyService(std::weak_ptr<DefaultMQPushConsumerImpl> consumer, int thread_count,
                                    MQMessageListener* message_listener_ptr);

  ~ConsumeMessageConcurrentlyService() override = default;

  void start() override;

  void shutdown() override;

  void submitConsumeTask(const ProcessQueueWeakPtr& process_queue) override;

  MessageListenerType getConsumeMsgServiceListenerType() override;

protected:
  void dispatch() override;

private:
  void consumeTask(const ProcessQueueWeakPtr& process_queue, const std::vector<MQMessageExt>& msgs);
};

class ConsumeMessageOrderlyService : public ConsumeMessageService {
public:
  ConsumeMessageOrderlyService(std::weak_ptr<DefaultMQPushConsumerImpl> consumer_impl_ptr, int thread_count,
                               MQMessageListener* message_listener_ptr);
  void start() override;

  void shutdown() override;

  void submitConsumeTask(const ProcessQueueWeakPtr& process_queue) override;

  MessageListenerType getConsumeMsgServiceListenerType() override;

protected:
  void dispatch() override;

private:
  void consumeTask(const ProcessQueueWeakPtr& process_queue, std::vector<MQMessageExt>& msgs);

  void submitConsumeTask0(const std::shared_ptr<DefaultMQPushConsumerImpl>& consumer, ProcessQueueWeakPtr process_queue,
                          std::vector<MQMessageExt> messages);
};

ROCKETMQ_NAMESPACE_END