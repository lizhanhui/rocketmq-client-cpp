#pragma once

#include "rocketmq/RocketMQ.h"
#include <string>

ROCKETMQ_NAMESPACE_BEGIN

class TracingAttribute {
public:
  static const std::string ARN_;
  static const std::string ACCESS_KEY_;
  static const std::string TOPIC_;
  static const std::string GROUP_;
  static const std::string MSG_ID_;
  static const std::string TAGS_;
  static const std::string TRANSACTION_ID_;
  static const std::string DELIVERY_TIMESTAMP_;
  static const std::string COMMIT_ACTION_;
  static const std::string BORN_HOST_;
  static const std::string KEYS_;
  static const std::string ATTEMPT_TIMES_;
  static const std::string MSG_TYPE_;

private:
  TracingAttribute();
};

ROCKETMQ_NAMESPACE_END