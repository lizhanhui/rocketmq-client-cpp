#include "TracingMessageInterceptor.h"
#include "MessageAccessor.h"
#include "SpanName.h"
#include "TracingAttribute.h"
#include "TracingUtility.h"
#include <iostream>

ROCKETMQ_NAMESPACE_BEGIN

using namespace opentelemetry;

TracingMessageInterceptor::TracingMessageInterceptor(std::shared_ptr<BaseImpl> baseImpl) : baseImpl_(baseImpl) {}

void TracingMessageInterceptor::intercept(MessageHookPoint hookPoint, MQMessageExt& message,
                                          MessageInterceptorContext context) {
  nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = baseImpl_->getTracer();
  if (nullptr == tracer) {
    return;
  }
  std::string access_key;
  CredentialsProviderPtr providerPtr = baseImpl_->credentialsProvider();
  if (nullptr != providerPtr) {
    access_key = providerPtr->getCredentials().accessKey();
  }
  std::string arn = baseImpl_->arn();
  std::string group = baseImpl_->getGroupName();

  switch (hookPoint) {
  case PRE_SEND_MESSAGE: {
    auto span = tracer->StartSpan(SpanName::SEND_MESSAGE_);
    span->SetAttribute(TracingAttribute::ACCESS_KEY_, access_key);
    span->SetAttribute(TracingAttribute::ARN_, arn);
    span->SetAttribute(TracingAttribute::TOPIC_, message.getTopic());
    span->SetAttribute(TracingAttribute::MSG_ID_, message.getMsgId());
    span->SetAttribute(TracingAttribute::GROUP_, group);
    span->SetAttribute(TracingAttribute::TAGS_, message.getTags());
    span->SetAttribute(TracingAttribute::KEYS_, TracingUtility::convertMessageKeysVectorToString(message.getKeys()));
    span->SetAttribute(TracingAttribute::ATTEMPT_TIMES_, context.attemptTimes());
    // span->SetAttribute(TracingAttribute::MSG_TYPE, message.getMsgType().getName());
    int64_t delivery_timestamp = message.getDeliveryTimestamp();
    if (delivery_timestamp > 0) {
      span->SetAttribute(TracingAttribute::DELIVERY_TIMESTAMP_, delivery_timestamp);
    }
    std::string trace_context = TracingUtility::injectSpanContextToTraceParent(span->GetContext());
    std::string span_id = TracingUtility::serializeSpanId(span->GetContext().span_id());
    {
      absl::MutexLock lk(&inflight_span_mtx_);
      inflight_spans_.insert({span_id, span});
    }
    MessageAccessor::setTraceContext(message, trace_context);
    break;
  }
  case POST_SEND_MESSAGE: {
    std::string trace_context = message.traceContext();
    auto span_context = TracingUtility::extractContextFromTraceParent(trace_context);
    std::string span_id = TracingUtility::serializeSpanId(span_context.span_id());
    {
      absl::MutexLock lk(&inflight_span_mtx_);
      auto search = inflight_spans_.find(span_id);
      if (search != inflight_spans_.end()) {
        auto span = search->second;
        inflight_spans_.erase(search);
        trace::StatusCode status_code = TracingUtility::convertToTraceStatus(context.status());
        span->SetStatus(status_code);
        span->End();
      }
    }
    break;
  }
  case PRE_PULL_MESSAGE: {
    break;
  }
  case POST_PULL_MESSAGE: {
    auto system_start = std::chrono::system_clock::now();
    auto steady_start = std::chrono::steady_clock::now();

    trace::StartSpanOptions start_options;
    auto duration = context.duration();
    start_options.start_system_time = opentelemetry::core::SystemTimestamp(
        system_start - std::chrono::duration_cast<std::chrono::milliseconds>(duration));
    start_options.start_steady_time = opentelemetry::core::SteadyTimestamp(
        steady_start - std::chrono::duration_cast<std::chrono::milliseconds>(duration));

    std::string trace_context = message.traceContext();
    trace::SpanContext span_context = TracingUtility::extractContextFromTraceParent(trace_context);

    if (span_context.IsValid()) {
      start_options.parent = span_context;
    }

    auto span = tracer->StartSpan(SpanName::PULL_MESSAGE_, start_options);

    span->SetAttribute(TracingAttribute::ACCESS_KEY_, access_key);
    span->SetAttribute(TracingAttribute::ARN_, arn);
    span->SetAttribute(TracingAttribute::TOPIC_, message.getTopic());
    span->SetAttribute(TracingAttribute::MSG_ID_, message.getMsgId());
    span->SetAttribute(TracingAttribute::GROUP_, group);
    span->SetAttribute(TracingAttribute::TAGS_, message.getTags());
    span->SetAttribute(TracingAttribute::KEYS_, TracingUtility::convertMessageKeysVectorToString(message.getKeys()));
    span->SetAttribute(TracingAttribute::ATTEMPT_TIMES_, context.attemptTimes());
    // span->SetAttribute(TracingAttribute::MSG_TYPE, message.getMsgType().getName());
    break;
  }
  case PRE_MESSAGE_CONSUMPTION: {
    std::string trace_context = message.traceContext();
    auto span_context = TracingUtility::extractContextFromTraceParent(trace_context);

    trace::StartSpanOptions start_options;
    if (span_context.IsValid()) {
      start_options.parent = span_context;
    }
    auto system_start = std::chrono::system_clock::now();
    auto steady_start = std::chrono::steady_clock::now();

    auto waiting_consumption_duration =
        system_start - std::chrono::time_point_cast<std::chrono::milliseconds>(message.decodeTimestamp());

    start_options.start_system_time = opentelemetry::core::SystemTimestamp(system_start - waiting_consumption_duration);
    start_options.start_steady_time = opentelemetry::core::SteadyTimestamp(steady_start - waiting_consumption_duration);

    auto span = tracer->StartSpan(SpanName::WAITING_CONSUMPTION_, start_options);

    span->SetAttribute(TracingAttribute::ACCESS_KEY_, access_key);
    span->SetAttribute(TracingAttribute::ARN_, arn);
    span->SetAttribute(TracingAttribute::TOPIC_, message.getTopic());
    span->SetAttribute(TracingAttribute::MSG_ID_, message.getMsgId());
    span->SetAttribute(TracingAttribute::GROUP_, group);
    span->SetAttribute(TracingAttribute::TAGS_, message.getTags());
    span->SetAttribute(TracingAttribute::KEYS_, TracingUtility::convertMessageKeysVectorToString(message.getKeys()));
    span->SetAttribute(TracingAttribute::ATTEMPT_TIMES_, context.attemptTimes());
    // span->SetAttribute(TracingAttribute::MSG_TYPE, message.getMsgType().getName());

    span->End();
    break;
  }
  case POST_MESSAGE_CONSUMPTION: {
    std::string trace_context = message.traceContext();
    auto span_context = TracingUtility::extractContextFromTraceParent(trace_context);

    trace::StartSpanOptions start_options;
    if (span_context.IsValid()) {
      start_options.parent = span_context;
    }

    auto system_start = std::chrono::system_clock::now();
    auto steady_start = std::chrono::steady_clock::now();

    auto duration = context.duration();
    start_options.start_system_time = opentelemetry::core::SystemTimestamp(
        system_start - (context.messageBatchSize() - context.messageIndex()) *
                           std::chrono::duration_cast<std::chrono::milliseconds>(duration));
    start_options.start_steady_time = opentelemetry::core::SteadyTimestamp(
        steady_start - (context.messageBatchSize() - context.messageIndex()) *
                           std::chrono::duration_cast<std::chrono::milliseconds>(duration));

    auto span = tracer->StartSpan(SpanName::CONSUME_MESSAGE_, start_options);

    span->SetAttribute(TracingAttribute::ACCESS_KEY_, access_key);
    span->SetAttribute(TracingAttribute::ARN_, arn);
    span->SetAttribute(TracingAttribute::TOPIC_, message.getTopic());
    span->SetAttribute(TracingAttribute::MSG_ID_, message.getMsgId());
    span->SetAttribute(TracingAttribute::GROUP_, group);
    span->SetAttribute(TracingAttribute::TAGS_, message.getTags());
    span->SetAttribute(TracingAttribute::KEYS_, TracingUtility::convertMessageKeysVectorToString(message.getKeys()));
    span->SetAttribute(TracingAttribute::ATTEMPT_TIMES_, context.attemptTimes());
    // span->SetAttribute(TracingAttribute::MSG_TYPE, message.getMsgType().getName());

    span->End();
    break;
  }
  case PRE_END_MESSAGE: {
    break;
  }
  case POST_END_MESSAGE: {
    auto system_start = std::chrono::system_clock::now();
    auto steady_start = std::chrono::steady_clock::now();

    trace::StartSpanOptions start_options;
    auto duration = context.duration();
    start_options.start_system_time = opentelemetry::core::SystemTimestamp(
        system_start - std::chrono::duration_cast<std::chrono::milliseconds>(duration));
    start_options.start_steady_time = opentelemetry::core::SteadyTimestamp(
        steady_start - std::chrono::duration_cast<std::chrono::milliseconds>(duration));

    std::string trace_context = message.traceContext();
    auto span_context = TracingUtility::extractContextFromTraceParent(trace_context);

    if (span_context.IsValid()) {
      start_options.parent = span_context;
    }

    auto span = tracer->StartSpan(SpanName::PULL_MESSAGE_, start_options);

    span->SetAttribute(TracingAttribute::ACCESS_KEY_, access_key);
    span->SetAttribute(TracingAttribute::ARN_, arn);
    span->SetAttribute(TracingAttribute::TOPIC_, message.getTopic());
    span->SetAttribute(TracingAttribute::MSG_ID_, message.getMsgId());
    span->SetAttribute(TracingAttribute::GROUP_, group);

    span->End();
    break;
  }
  }
}

int TracingMessageInterceptor::inflightSpanNum() {
  {
    absl::MutexLock lk(&inflight_span_mtx_);
    return inflight_spans_.size();
  }
}
ROCKETMQ_NAMESPACE_END