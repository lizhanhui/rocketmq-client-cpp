#include "MessageInterceptorContext.h"

ROCKETMQ_NAMESPACE_BEGIN

void MessageInterceptorContext::status(HookPointStatus status) { status_ = status; }

HookPointStatus MessageInterceptorContext::status() { return status_; }

void MessageInterceptorContext::duration(std::chrono::system_clock::duration duration) { duration_ = duration; }

std::chrono::system_clock::duration MessageInterceptorContext::duration() { return duration_; }

void MessageInterceptorContext::messageIndex(int message_index) { message_index_ = message_index; }

int MessageInterceptorContext::messageIndex() { return message_index_; }

void MessageInterceptorContext::attemptTimes(int attempt_times) { attempt_times_ = attempt_times; }

int MessageInterceptorContext::attemptTimes() { return attempt_times_; }

void MessageInterceptorContext::messageBatchSize(int message_batch_size) { message_batch_size_ = message_batch_size; }

int MessageInterceptorContext::messageBatchSize() { return message_batch_size_; }

ROCKETMQ_NAMESPACE_END