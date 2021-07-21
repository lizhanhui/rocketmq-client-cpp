#include "SpanName.h"

ROCKETMQ_NAMESPACE_BEGIN

const std::string SpanName::SEND_MESSAGE_ = "SendMessage";
const std::string SpanName::WAITING_CONSUMPTION_ = "WaitingConsumption";
const std::string SpanName::CONSUME_MESSAGE_ = "ConsumeMessage";
const std::string SpanName::END_MESSAGE_ = "EndMessage";
const std::string SpanName::PULL_MESSAGE_ = "PullMessage";

SpanName::SpanName() {}

ROCKETMQ_NAMESPACE_END