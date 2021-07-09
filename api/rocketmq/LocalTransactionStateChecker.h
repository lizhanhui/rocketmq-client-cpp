#pragma once

#include "MQMessageExt.h"
#include "Transaction.h"
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class LocalTransactionStateChecker {
public:
  virtual ~LocalTransactionStateChecker() = default;

  virtual TransactionState checkLocalTransactionState(const MQMessageExt& message) = 0;
};

using LocalTransactionStateCheckerPtr = std::unique_ptr<LocalTransactionStateChecker>;

ROCKETMQ_NAMESPACE_END