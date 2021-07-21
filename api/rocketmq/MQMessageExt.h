#pragma once

#include <chrono>
#include "MQMessage.h"

ROCKETMQ_NAMESPACE_BEGIN

class MessageAccessor;

class MQMessageExt : public MQMessage {
public:
  MQMessageExt();

  MQMessageExt(const MQMessageExt& other);

  MQMessageExt& operator=(const MQMessageExt& other);

  int32_t getQueueId() const;

  /**
   * @return Milli-seconds since epoch in perspective of system_clock.
   */
  int64_t getBornTimestamp() const;

  int64_t getDeliveryTimestamp() const;

  std::chrono::system_clock::time_point bornTimestamp() const;

  std::string getBornHost() const;

  std::chrono::system_clock::time_point storeTimestamp() const;
  int64_t getStoreTimestamp() const;

  std::chrono::system_clock::time_point decodeTimestamp() const;

  std::string getStoreHost() const;

  const std::string& getMsgId() const;

  int64_t getQueueOffset() const;

  int32_t getReconsumeTimes() const;

  const std::string& receiptHandle() const;

  const std::string& traceContext() const;

  bool operator==(const MQMessageExt& other);

  friend class MessageAccessor;
};

ROCKETMQ_NAMESPACE_END