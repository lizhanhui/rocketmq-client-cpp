#include "TracingAttribute.h"

ROCKETMQ_NAMESPACE_BEGIN

const std::string TracingAttribute::ARN_ = "arn";
const std::string TracingAttribute::ACCESS_KEY_ = "ak";
const std::string TracingAttribute::TOPIC_ = "topic";
const std::string TracingAttribute::GROUP_ = "group";
const std::string TracingAttribute::MSG_ID_ = "msg_id";
const std::string TracingAttribute::TAGS_ = "tags";
const std::string TracingAttribute::TRANSACTION_ID_ = "trans_id";
const std::string TracingAttribute::DELIVERY_TIMESTAMP_ = "delivery_timestamp";
const std::string TracingAttribute::COMMIT_ACTION_ = "commit_action";
const std::string TracingAttribute::BORN_HOST_ = "born_host";
const std::string TracingAttribute::KEYS_ = "keys";
const std::string TracingAttribute::ATTEMPT_TIMES_ = "attempt_times";
const std::string TracingAttribute::MSG_TYPE_ = "msg_type";

TracingAttribute::TracingAttribute(){};

ROCKETMQ_NAMESPACE_END