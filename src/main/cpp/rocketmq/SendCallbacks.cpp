#include "SendCallbacks.h"

#include "include/TransactionImpl.h"
#include "rocketmq/Logger.h"
#include "rocketmq/MQMessageQueue.h"
#include "spdlog/spdlog.h"

ROCKETMQ_NAMESPACE_BEGIN

void OnewaySendCallback::onException(const MQException& e) {
  SPDLOG_WARN("Failed to one-way send message. Message: {}", e.what());
}

void OnewaySendCallback::onSuccess(const SendResult& send_result) {
  SPDLOG_DEBUG("Send message in one-way OK. MessageId: {}", send_result.getMsgId());
}

OnewaySendCallback* onewaySendCallback() {
  static OnewaySendCallback callback;
  return &callback;
}

void AwaitSendCallback::await() {
  absl::MutexLock lk(&mtx_);
  while (!completed_) {
    cv_.Wait(&mtx_);
  }
}

void AwaitSendCallback::onSuccess(const SendResult& send_result) {
  send_result_ = send_result;
  success_ = true;
  completed_ = true;
  absl::MutexLock lk(&mtx_);
  cv_.SignalAll();
}

void AwaitSendCallback::onException(const MQException& e) {
  success_ = false;
  error_message_ = e.what();
  completed_ = true;
  absl::MutexLock lk(&mtx_);
  cv_.SignalAll();
}

void RetrySendCallback::onSuccess(const SendResult& send_result) {
  callback_->onSuccess(send_result);
  SPDLOG_DEBUG("");
  delete this;
}

void RetrySendCallback::onException(const MQException& e) {
  if (++attempt_times_ >= max_attempt_times_) {
    SPDLOG_WARN("Retried {} times, which exceeds the limit: {}", attempt_times_, max_attempt_times_);
    callback_->onException(e);
    delete this;
    return;
  }

  std::shared_ptr<ProducerImpl> producer = producer_.lock();
  if (!producer) {
    SPDLOG_WARN("Producer has been destructed");
    callback_->onException(e);
    delete this;
    return;
  }

  if (candidates_.empty()) {
    SPDLOG_WARN("No alternative hosts to perform additional retries");
    callback_->onException(e);
    delete this;
    return;
  }

  MQMessageQueue message_queue = candidates_[attempt_times_ % candidates_.size()];
  producer->sendImpl(message_, this, message_queue, attempt_times_);
}

ROCKETMQ_NAMESPACE_END