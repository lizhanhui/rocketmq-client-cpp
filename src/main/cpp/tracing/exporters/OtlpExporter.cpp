#include "OtlpExporter.h"
#include "InvocationContext.h"
#include "Signature.h"
#include "fmt/format.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

ROCKETMQ_NAMESPACE_BEGIN

namespace trace = opentelemetry::proto::trace::v1;
namespace common = opentelemetry::proto::common::v1;

void OtlpExporter::registerHandlers(std::shared_ptr<ClientManager> client_manager,
                                    std::weak_ptr<ClientConfig> client_config, std::vector<std::string> hosts) {
  auto handler =
      absl::make_unique<OtlpExporterHandler>(std::move(client_manager), std::move(client_config), std::move(hosts));
  handler->start();
  opencensus::trace::exporter::SpanExporter::RegisterHandler(std::move(handler));
}

void ExportClient::asyncExport(const collector_trace::ExportTraceServiceRequest& request,
                               InvocationContext<collector_trace::ExportTraceServiceResponse>* invocation_context) {
  auto completion_queue = completion_queue_.lock();
  if (!completion_queue) {
    // Server should have shutdown
    return;
  }

  invocation_context->response_reader =
      stub_->PrepareAsyncExport(&invocation_context->context, request, completion_queue.get());
  invocation_context->response_reader->StartCall();
  invocation_context->response_reader->Finish(&invocation_context->response, &invocation_context->status,
                                              invocation_context);
}

OtlpExporterHandler::OtlpExporterHandler(std::shared_ptr<ClientManager> client_manager,
                                         std::weak_ptr<ClientConfig> client_config, std::vector<std::string> hosts)
    : client_manager_(std::move(client_manager)), client_config_(std::move(client_config)), hosts_(std::move(hosts)),
      completion_queue_(std::make_shared<CompletionQueue>()) {

  for (const auto& host : hosts_) {
    auto channel = client_manager_->createChannel(host);
    auto export_client = absl::make_unique<ExportClient>(completion_queue_, channel);
    clients_map_.emplace(host, std::move(export_client));
  }
}

void OtlpExporterHandler::start() {
  poll_thread_ = std::thread(std::bind(&OtlpExporterHandler::poll, this));
  {
    absl::MutexLock lk(&start_mtx_);
    start_cv_.Wait(&start_mtx_);
  }
}

void OtlpExporterHandler::shutdown() {
  stopped_.store(true, std::memory_order_relaxed);
  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }
}

void OtlpExporterHandler::poll() {
  {
    // Notify main thread that the poller thread has started
    absl::MutexLock lk(&start_mtx_);
    start_cv_.SignalAll();
  }

  while (!stopped_.load(std::memory_order_relaxed)) {
    bool ok = false;
    void* opaque_invocation_context;
    while (completion_queue_->Next(&opaque_invocation_context, &ok)) {
      auto invocation_context = static_cast<BaseInvocationContext*>(opaque_invocation_context);
      if (!ok) {
        // the call is dead
        SPDLOG_WARN("CompletionQueue#Next assigned ok false, indicating the call is dead");
      }
      invocation_context->onCompletion(ok);
    }
    SPDLOG_INFO("CompletionQueue is fully drained and shut down");
  }
  SPDLOG_INFO("poll completed and quit");
}

void OtlpExporterHandler::Export(const std::vector<::opencensus::trace::exporter::SpanData>& spans) {
  if (clients_map_.empty()) {
    SPDLOG_WARN("No exporter client is available");
    return;
  }

  auto client_config = client_config_.lock();
  if (!client_config) {
    // MQ client has destructed.
    return;
  }

  uint32_t client_index = round_robin_++ % clients_map_.size();

  auto iterator = clients_map_.begin();
  for (uint32_t i = 0; i < client_index; i++) {
    iterator++;
  }

  auto& exporter_client = iterator->second;

  collector_trace::ExportTraceServiceRequest request;

  auto resource = new trace::ResourceSpans();
  auto instrument_library_span = new trace::InstrumentationLibrarySpans();
  for (const auto& span : spans) {
    auto item = new trace::Span();
    item->set_trace_id(span.context().trace_id().ToHex());
    item->set_span_id(span.context().span_id().ToHex());
    item->set_parent_span_id(span.parent_span_id().ToHex());
    item->set_name(span.name().data());

    item->set_start_time_unix_nano(absl::ToUnixNanos(span.start_time()));
    item->set_end_time_unix_nano(absl::ToUnixNanos(span.end_time()));

    item->set_kind(trace::Span_SpanKind::Span_SpanKind_SPAN_KIND_CLIENT);

    // OpenCensus has annotations or message_events, which maps to events in OpenTelemetry.
    if (!span.message_events().events().empty()) {
      for (const auto& event : span.message_events().events()) {
        auto ev = new trace::Span::Event();
        ev->set_time_unix_nano(absl::ToUnixNanos(event.timestamp()));
      }
      item->set_dropped_events_count(span.message_events().dropped_events_count());
    }

    if (!span.annotations().events().empty()) {
      for (const auto& annotation : span.annotations().events()) {
        auto ev = new trace::Span::Event();
        ev->set_time_unix_nano(absl::ToUnixNanos(annotation.timestamp()));
        auto attrs = annotation.event().attributes();
        for (const auto& attr : attrs) {
          auto kv = new common::KeyValue();
          kv->set_key(attr.first);
          auto value = new common::AnyValue();
          switch (attr.second.type()) {
          case opencensus::trace::AttributeValueRef::Type::kString:
            value->set_string_value(attr.second.string_value());
            break;
          case opencensus::trace::AttributeValueRef::Type::kInt:
            value->set_int_value(attr.second.int_value());
            break;
          case opencensus::trace::AttributeValueRef::Type::kBool:
            value->set_bool_value(attr.second.bool_value());
            break;
          }
          ev->mutable_attributes()->AddAllocated(kv);
        }
        item->mutable_events()->AddAllocated(ev);
      }
      item->set_dropped_events_count(span.annotations().dropped_events_count());
    }

    for (const auto& link : span.links()) {
      auto span_link = new trace::Span::Link();
      span_link->set_trace_id(link.trace_id().ToHex());
      span_link->set_span_id(link.span_id().ToHex());
      for (const auto& attribute : link.attributes()) {
        auto kv = new common::KeyValue();
        kv->set_key(attribute.first);
        auto value = new common::AnyValue();
        switch (attribute.second.type()) {
        case opencensus::trace::AttributeValueRef::Type::kString:
          value->set_string_value(attribute.second.string_value());
          break;
        case opencensus::trace::AttributeValueRef::Type::kBool:
          value->set_bool_value(attribute.second.bool_value());
          break;
        case opencensus::trace::AttributeValueRef::Type::kInt:
          value->set_int_value(attribute.second.int_value());
          break;
        }
        kv->set_allocated_value(value);
        span_link->mutable_attributes()->AddAllocated(kv);
      }

      item->mutable_links()->AddAllocated(span_link);
    }
    item->set_dropped_links_count(span.num_attributes_dropped());

    for (const auto& attribute : span.attributes()) {
      auto kv = new common::KeyValue();
      kv->set_key(attribute.first);
      auto value = new common::AnyValue();
      switch (attribute.second.type()) {
      case opencensus::trace::AttributeValueRef::Type::kString:
        value->set_string_value(attribute.second.string_value());
        break;
      case opencensus::trace::AttributeValueRef::Type::kBool:
        value->set_bool_value(attribute.second.bool_value());
        break;
      case opencensus::trace::AttributeValueRef::Type::kInt:
        value->set_int_value(attribute.second.int_value());
        break;
      }
      item->mutable_attributes()->AddAllocated(kv);
    }

    if (span.status().ok()) {
      item->mutable_status()->set_code(trace::Status_StatusCode::Status_StatusCode_STATUS_CODE_OK);
    } else {
      item->mutable_status()->set_code(trace::Status_StatusCode::Status_StatusCode_STATUS_CODE_ERROR);
      item->mutable_status()->set_message(span.status().error_message());
    }

    // item->mutable_status()->set_code()

    instrument_library_span->mutable_spans()->AddAllocated(item);
  }
  resource->mutable_instrumentation_library_spans()->AddAllocated(instrument_library_span);
  request.mutable_resource_spans()->AddAllocated(resource);

  auto invocation_context = new InvocationContext<collector_trace::ExportTraceServiceResponse>();
  invocation_context->remote_address = iterator->first;
  auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(3);
  invocation_context->context.set_deadline(deadline);

  absl::flat_hash_map<std::string, std::string> metadata;
  Signature::sign(client_config.get(), metadata);
  for (const auto& entry : metadata) {
    invocation_context->context.AddMetadata(entry.first, entry.second);
  }
  auto callback = [](const InvocationContext<collector_trace::ExportTraceServiceResponse>* invocation_context) {
    if (invocation_context->status.ok()) {
      SPDLOG_DEBUG("Export tracing spans OK");
    } else {
      SPDLOG_WARN("Failed to export tracing spans to {}", invocation_context->remote_address);
    }
  };
  invocation_context->callback = callback;

  exporter_client->asyncExport(request, invocation_context);
}

thread_local std::uint32_t OtlpExporterHandler::round_robin_ = 0;

ROCKETMQ_NAMESPACE_END