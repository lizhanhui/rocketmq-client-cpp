#pragma once

#include <system_error>

#include "MQClientException.h"
#include "PullResult.h"
#include "SendResult.h"

ROCKETMQ_NAMESPACE_BEGIN

struct AsyncCallback {};

class SendCallback : public AsyncCallback {
public:
  virtual ~SendCallback() = default;

  virtual void onSuccess(SendResult &send_result) noexcept = 0;

  virtual void onFailure(const std::error_code &ec) noexcept = 0;
};

class PullCallback : public AsyncCallback {
public:
  virtual ~PullCallback() = default;

  virtual void onSuccess(const PullResult &pull_result) = 0;

  virtual void onException(const MQException &e) = 0;
};

ROCKETMQ_NAMESPACE_END