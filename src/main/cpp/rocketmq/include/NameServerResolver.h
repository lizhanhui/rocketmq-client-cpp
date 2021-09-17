#pragma once

#include <string>
#include <vector>

#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class NameServerResolver {
public:
  virtual ~NameServerResolver() = default;

  virtual std::vector<std::string> resolve() = 0;
};

ROCKETMQ_NAMESPACE_END