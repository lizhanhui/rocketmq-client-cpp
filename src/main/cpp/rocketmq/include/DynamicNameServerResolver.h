#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

#include "NameServerResolver.h"
#include "TopAddressing.h"

ROCKETMQ_NAMESPACE_BEGIN

class DynamicNameServerResolver : public NameServerResolver,
                                  public std::enable_shared_from_this<DynamicNameServerResolver> {
public:
  DynamicNameServerResolver(absl::string_view endpoint, std::chrono::milliseconds refresh_interval);

  std::vector<std::string> resolve() override LOCKS_EXCLUDED(name_server_list_mtx_);

private:
  std::string endpoint_;

  std::chrono::milliseconds refresh_interval_;
  std::chrono::steady_clock::time_point last_resolve_timepoint_;

  bool shouldRefresh() const;

  void refreshNameServerList(const std::vector<std::string>& name_server_list) LOCKS_EXCLUDED(name_server_list_mtx_);

  std::vector<std::string> name_server_list_ GUARDED_BY(name_server_list_mtx_);
  absl::Mutex name_server_list_mtx_;

  bool ssl_{false};
  std::unique_ptr<TopAddressing> top_addressing_;
};

ROCKETMQ_NAMESPACE_END