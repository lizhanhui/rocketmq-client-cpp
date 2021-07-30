#pragma once

#include "ConsumeMessageService.h"
#include "Consumer.h"
#include "ProcessQueue.h"
#include <functional>
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class PushConsumer : virtual public Consumer {
public:
  ~PushConsumer() override = default;

  virtual void iterateProcessQueue(const std::function<void(ProcessQueueSharedPtr)>& cb) = 0;

  virtual MessageModel messageModel() const = 0;

  virtual void ack(const MQMessageExt& msg, const std::function<void(bool)>& callback) = 0;

  virtual void forwardToDeadLetterQueue(const MQMessageExt& message, const std::function<void(bool)>& cb) = 0;

  virtual const Executor& customExecutor() const = 0;

  virtual uint32_t consumeBatchSize() const = 0;

  virtual int32_t maxDeliveryAttempts() const = 0;

  virtual void updateOffset(const MQMessageQueue& message_queue, int64_t offset) = 0;

  virtual void nack(const MQMessageExt& message, const std::function<void(bool)>& callback) = 0;

  virtual std::shared_ptr<ConsumeMessageService> getConsumeMessageService() = 0;

  virtual bool receiveMessage(const MQMessageQueue& message_queue, const FilterExpression& filter_expression,
                              ConsumeMessageType consume_type) = 0;
};

ROCKETMQ_NAMESPACE_END