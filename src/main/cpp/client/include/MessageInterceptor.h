#pragma once

#include "MessageHookPoint.h"
#include "MessageInterceptorContext.h"

ROCKETMQ_NAMESPACE_BEGIN

class MessageInterceptor {
public:
  virtual ~MessageInterceptor() = default;
  virtual void intercept(MessageHookPoint hookPoint, MQMessageExt& message, MessageInterceptorContext context) = 0;
};

ROCKETMQ_NAMESPACE_END