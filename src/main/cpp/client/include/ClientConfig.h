#pragma once

#include <string>

#include "absl/time/time.h"

#include "rocketmq/CredentialsProvider.h"

ROCKETMQ_NAMESPACE_BEGIN

class ClientConfig {
public:
  virtual ~ClientConfig() = default;

  virtual const std::string& region() const = 0;

  virtual const std::string& serviceName() const = 0;

  virtual const std::string& resourceNamespace() const = 0;

  virtual CredentialsProviderPtr credentialsProvider() = 0;

  virtual const std::string& tenantId() const = 0;

  virtual absl::Duration getIoTimeout() const = 0;

  virtual absl::Duration getLongPollingTimeout() const = 0;

  virtual const std::string& getGroupName() const = 0;

  virtual std::string clientId() const = 0;
};

ROCKETMQ_NAMESPACE_END