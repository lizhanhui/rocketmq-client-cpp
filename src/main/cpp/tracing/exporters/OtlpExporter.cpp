#include "OtlpExporter.h"
#include "fmt/format.h"
#include <iostream>

ROCKETMQ_NAMESPACE_BEGIN

void OtlpExporter::registerHandlers() {
  opencensus::trace::exporter::SpanExporter::RegisterHandler(absl::make_unique<OtlpExporterHandler>());
}

void OtlpExporterHandler::Export(const std::vector<::opencensus::trace::exporter::SpanData>& spans) {
  std::cout << "Start to export..." << std::endl;
  for (const auto& span : spans) {
    std::cout << fmt::format("Span {} took {}ms", span.name().data(),
                             absl::ToInt64Milliseconds(span.end_time() - span.start_time())) << std::endl;
  }
  std::cout << "Export job completed" << std::endl;
}

ROCKETMQ_NAMESPACE_END