#pragma once

#include "ClientManager.h"
#include "InvocationContext.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "opencensus/trace/exporter/span_data.h"
#include "opencensus/trace/exporter/span_exporter.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "rocketmq/RocketMQ.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

ROCKETMQ_NAMESPACE_BEGIN

namespace collector = opentelemetry::proto::collector;
namespace collector_trace = collector::trace::v1;

class OtlpExporter {
public:
  OtlpExporter() = delete;

  static void registerHandlers(std::shared_ptr<ClientManager> client_manager, std::weak_ptr<ClientConfig> client_config,
                               std::vector<std::string> hosts);
};

class ExportClient {
public:
  ExportClient(std::shared_ptr<CompletionQueue> completion_queue, std::shared_ptr<grpc::Channel> channel)
      : completion_queue_(std::move(completion_queue)), stub_(collector_trace::TraceService::NewStub(channel)) {}

  void asyncExport(const collector_trace::ExportTraceServiceRequest& request,
                   InvocationContext<collector_trace::ExportTraceServiceResponse>* invocation_context);

private:
  std::weak_ptr<CompletionQueue> completion_queue_;
  std::unique_ptr<collector_trace::TraceService::Stub> stub_;
};

class OtlpExporterHandler : public ::opencensus::trace::exporter::SpanExporter::Handler {
public:
  OtlpExporterHandler(std::shared_ptr<ClientManager> client_manager, std::weak_ptr<ClientConfig> client_config,
                      std::vector<std::string> hosts);

  void Export(const std::vector<::opencensus::trace::exporter::SpanData>& spans) override;

  void start();

  void shutdown();

private:
  std::shared_ptr<ClientManager> client_manager_;
  std::weak_ptr<ClientConfig> client_config_;
  std::vector<std::string> hosts_;
  std::shared_ptr<CompletionQueue> completion_queue_;
  std::thread poll_thread_;
  absl::Mutex start_mtx_;
  absl::CondVar start_cv_;

  absl::flat_hash_map<std::string, std::unique_ptr<ExportClient>> clients_map_;
  thread_local static std::uint32_t round_robin_;

  std::atomic_bool stopped_{false};

  void poll();
};

ROCKETMQ_NAMESPACE_END
