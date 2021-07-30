#pragma once

#include "ClientConfig.h"
#include "absl/container/flat_hash_set.h"
#include <functional>

ROCKETMQ_NAMESPACE_BEGIN

class Client : virtual public ClientConfig {
public:
  ~Client() override = default;

  virtual void endpointsInUse(absl::flat_hash_set<std::string>& endpoints) = 0;

  virtual void heartbeat() = 0;

  virtual bool active() = 0;

  /**
   * For endpoints that are marked as inactive due to one or multiple business operation failure, this function is to
   * initiate health-check RPCs; Once the health-check passes, they are conceptually add back to serve further business
   * workload.
   */
  virtual void healthCheck() = 0;

  virtual void schedule(const std::string& task_name, const std::function<void(void)>& task,
                        std::chrono::milliseconds delay) = 0;

  virtual bool isStopped() const = 0;
};
ROCKETMQ_NAMESPACE_END