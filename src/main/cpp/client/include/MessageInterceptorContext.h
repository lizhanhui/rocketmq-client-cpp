#pragma once

#include <chrono>
#include <string>
#include "HookPointStatus.h"

ROCKETMQ_NAMESPACE_BEGIN

class MessageInterceptorContext {
public:
  void status(HookPointStatus status);
  void duration(std::chrono::system_clock::duration duration);
  void messageIndex(int index);
  void messageBatchSize(int batch_size);
  void attemptTimes(int attempt_times);

  HookPointStatus status();
  std::chrono::system_clock::duration duration();
  int messageIndex();
  int messageBatchSize();
  int attemptTimes();

private:
  HookPointStatus status_;
  std::chrono::system_clock::duration duration_;
  int message_batch_size_{1};
  int message_index_{0};
  int attempt_times_{1};
};

ROCKETMQ_NAMESPACE_END