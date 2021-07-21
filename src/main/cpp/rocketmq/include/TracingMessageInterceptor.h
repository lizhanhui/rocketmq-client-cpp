#pragma once

#include "BaseImpl.h"
#include "MessageInterceptor.h"

ROCKETMQ_NAMESPACE_BEGIN

class TracingMessageInterceptor : public MessageInterceptor {
public:
  explicit TracingMessageInterceptor(std::shared_ptr<BaseImpl> baseImpl);
  void intercept(MessageHookPoint hookPoint, MQMessageExt& message, MessageInterceptorContext context) override;
  int inflightSpanNum();

private:
  std::shared_ptr<BaseImpl> baseImpl_;
  absl::flat_hash_map<std::string, opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      inflight_spans_ GUARDED_BY(inflight_span_mtx_);
  absl::Mutex inflight_span_mtx_;
};

ROCKETMQ_NAMESPACE_END