#pragma once

#include "absl/memory/memory.h"
#include "opencensus/trace/exporter/span_data.h"
#include "opencensus/trace/exporter/span_exporter.h"
#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class OtlpExporter {
public:
  OtlpExporter() = delete;

  static void registerHandlers();
};

class OtlpExporterHandler : public ::opencensus::trace::exporter::SpanExporter::Handler {
public:
  void Export(const std::vector<::opencensus::trace::exporter::SpanData>& spans) override;
};

ROCKETMQ_NAMESPACE_END
