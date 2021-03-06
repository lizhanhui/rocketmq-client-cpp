#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "RocketMQ.h"

ROCKETMQ_NAMESPACE_BEGIN

class Transaction {
public:
  Transaction() = default;

  virtual ~Transaction() = default;

  virtual bool commit() = 0;

  virtual bool rollback() = 0;

  virtual std::string messageId() const = 0;

  virtual std::string transactionId() const = 0;
};

using TransactionPtr = std::unique_ptr<Transaction>;

enum class TransactionState : int8_t {
  COMMIT = 0,
  ROLLBACK = 1,
};

ROCKETMQ_NAMESPACE_END