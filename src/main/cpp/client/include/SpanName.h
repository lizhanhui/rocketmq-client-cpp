#pragma once

#include "rocketmq/RocketMQ.h"
#include <string>

ROCKETMQ_NAMESPACE_BEGIN

class SpanName {
public:
  static const std::string SEND_MESSAGE_;
  static const std::string WAITING_CONSUMPTION_;
  static const std::string CONSUME_MESSAGE_;
  static const std::string END_MESSAGE_;
  static const std::string PULL_MESSAGE_;

private:
  SpanName();
};

ROCKETMQ_NAMESPACE_END