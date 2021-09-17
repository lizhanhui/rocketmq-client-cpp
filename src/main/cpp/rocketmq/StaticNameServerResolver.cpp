#include "StaticNameServerResolver.h"

#include "absl/strings/str_split.h"

ROCKETMQ_NAMESPACE_BEGIN

StaticNameServerResolver::StaticNameServerResolver(absl::string_view name_server_list)
    : name_server_list_(absl::StrSplit(name_server_list, ';')) {}

std::vector<std::string> StaticNameServerResolver::resolve() { return name_server_list_; }

ROCKETMQ_NAMESPACE_END