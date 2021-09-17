#include "DynamicNameServerResolver.h"

#include <chrono>
#include <cstdint>
#include <memory>

#include "spdlog/spdlog.h"

ROCKETMQ_NAMESPACE_BEGIN

DynamicNameServerResolver::DynamicNameServerResolver(absl::string_view endpoint,
                                                     std::chrono::milliseconds refresh_interval)
    : endpoint_(endpoint.data(), endpoint.length()), refresh_interval_(refresh_interval),
      last_resolve_timepoint_(std::chrono::steady_clock::now() - refresh_interval_) {
  absl::string_view remains;
  if (absl::StartsWith(endpoint_, "https://")) {
    ssl_ = true;
    remains = absl::StripPrefix(endpoint_, "https://");
  } else {
    remains = absl::StripPrefix(endpoint_, "http://");
  }

  std::int32_t port = 80;
  if (ssl_) {
    port = 443;
  }

  absl::string_view host;
  if (absl::StrContains(remains, ':')) {
    std::vector<absl::string_view> segments = absl::StrSplit(remains, ':');
    host = segments[0];
    remains = absl::StripPrefix(remains, host);
    remains = absl::StripPrefix(remains, ":");

    segments = absl::StrSplit(remains, '/');
    if (!absl::SimpleAtoi(segments[0], &port)) {
      SPDLOG_WARN("Failed to parse port of name-server-list discovery service endpoint");
      abort();
    }
    remains = absl::StripPrefix(remains, segments[0]);
  } else {
    std::vector<absl::string_view> segments = absl::StrSplit(remains, '/');
    host = segments[0];
    remains = absl::StripPrefix(remains, host);
  }

  top_addressing_ = absl::make_unique<TopAddressing>(std::string(host.data(), host.length()), port,
                                                     std::string(remains.data(), remains.length()));
}

std::vector<std::string> DynamicNameServerResolver::resolve() {
  if (!shouldRefresh()) {
    absl::MutexLock lk(&name_server_list_mtx_);
    return name_server_list_;
  }

  std::weak_ptr<DynamicNameServerResolver> ptr(shared_from_this());
  auto callback = [ptr](bool success, const std::vector<std::string>& name_server_list) {
    if (success && !name_server_list.empty()) {
      std::shared_ptr<DynamicNameServerResolver> resolver = ptr.lock();
      if (resolver) {
        resolver->refreshNameServerList(name_server_list);
      }
    }
  };
  top_addressing_->fetchNameServerAddresses(callback);

  {
    absl::MutexLock lk(&name_server_list_mtx_);
    return name_server_list_;
  }
}

void DynamicNameServerResolver::refreshNameServerList(const std::vector<std::string>& name_server_list) {
  if (!name_server_list.empty()) {
    absl::MutexLock lk(&name_server_list_mtx_);
    name_server_list_ = name_server_list;
    last_resolve_timepoint_ = std::chrono::steady_clock::now();
  }
}

bool DynamicNameServerResolver::shouldRefresh() const {
  return last_resolve_timepoint_ + refresh_interval_ > std::chrono::steady_clock::now();
}

ROCKETMQ_NAMESPACE_END