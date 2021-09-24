#include "rocketmq/ErrorCode.h"

ROCKETMQ_NAMESPACE_BEGIN

std::error_code make_error_code(ROCKETMQ_NAMESPACE::ErrorCode code) {
  const ErrorCategory& instance = ErrorCategory::instance();
  return {static_cast<int>(code), instance};
}

ROCKETMQ_NAMESPACE_END