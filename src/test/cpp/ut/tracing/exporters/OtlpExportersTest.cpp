#include "OtlpExporter.h"
#include "opencensus/trace/sampler.h"
#include "opencensus/trace/span.h"
#include "rocketmq/RocketMQ.h"
#include "gtest/gtest.h"
#include <chrono>
#include <thread>

ROCKETMQ_NAMESPACE_BEGIN

TEST(OtlpExporterTest, testExport) {
  OtlpExporter::registerHandlers();
  static opencensus::trace::AlwaysSampler sampler;

  auto span_generator = [&] {
    int total = 20;
    while (total) {
      auto span = opencensus::trace::Span::StartSpan("TestSpan", nullptr, {&sampler});
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      span.End();
      if (0 == --total % 10) {
        std::cout << "Total: " << total << std::endl;
      }
    }
  };

  std::thread t(span_generator);
  if (t.joinable()) {
    t.join();
  }
}

ROCKETMQ_NAMESPACE_END