#pragma once

#include <vector>

#include "absl/strings/string_view.h"

#include "NameServerResolver.h"
#include "rocketmq/RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class StaticNameServerResolver : public NameServerResolver {
public:
  StaticNameServerResolver(absl::string_view name_server_list);

  std::vector<std::string> resolve() override;

private:
  std::vector<std::string> name_server_list_;
};

ROCKETMQ_NAMESPACE_END