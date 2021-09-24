#pragma once

#include "RocketMQ.h"

#include <cstdint>
#include <system_error>
#include <type_traits>

ROCKETMQ_NAMESPACE_BEGIN

enum class ErrorCode : int {
  Success = 0,

  /**
   * @brief Client state not as expected. Call Producer#start() first.
   *
   */
  IllegalState = 1,

  /**
   * @brief The server cannot process the request due to apprent client-side error. For example, topic contains invalid
   * character or is excessively long.
   *
   */
  BadRequest = 400,

  /**
   * @brief Authentication failed. Possibly caused by invalid credentials.
   *
   */
  Unauthorized = 401,

  /**
   * @brief Credentials are understood by server but authenticated user does not have privilege to perform the requested
   * action.
   *
   */
  Forbidden = 403,

  /**
   * @brief Topic not found, which should be created through console or administration API before hand.
   *
   */
  NotFound = 404,

  /**
   * @brief Timeout when connecting, reading from or writing to brokers.
   *
   */
  RequestTimeout = 408,

  /**
   * @brief Message body is too large.
   *
   */
  PayloadTooLarge = 413,

  /**
   * @brief When trying to perform an action whose dependent procedure state is not right, this code will be used.
   * 1. Acknowledge a message that is not previously received;
   * 2. Commit/Rollback a transactional message that does not exist;
   * 3. Commit an offset which is greater than maximum of partition;
   */
  PreconditionRequired = 428,

  /**
   * @brief Quota exchausted. The user has sent too many requests in a given amount of time.
   *
   */
  TooManyRequest = 429,

  /**
   * @brief A server operator has received a legal demand to deny access to a resource or to a set of resources that
   * includes the requested resource.
   *
   */
  UnavailableForLegalReasons = 451,

  /**
   * @brief Server side interval error
   *
   */
  InternalServerError = 500,

  /**
   * @brief The server either does not recognize the request method, or it lacks the ability to fulfil the request.
   *
   */
  NotImplemented = 501,

  /**
   * @brief The server was acting as a gateway or proxy and received an invalid response from the upstream server.
   *
   */
  BadGateway = 502,

  /**
   * @brief The server cannot handle the request (because it is overloaded or down for maintenance). Generally, this is
   * a temporary state.
   *
   */
  ServiceUnavailable = 503,

  /**
   * @brief The server was acting as a gateway or proxy and did not receive a timely response from the upstream server.
   *
   */
  GatewayTimeout = 504,

  /**
   * @brief The server does not support the protocol version used in the request.
   *
   */
  ProtocolVersionNotSupported = 505,

  /**
   * @brief The server is unable to store the representation needed to complete the request.
   *
   */
  InsufficientStorage = 507,
};

class ErrorCategory : public std::error_category {
public:
  static const ErrorCategory& instance() {
    static ErrorCategory instance;
    return instance;
  }

  const char* name() const noexcept override { return "RocketMQ"; }

  std::string message(int code) const override {
    ErrorCode ec = static_cast<ErrorCode>(code);
    switch (ec) {
    case ErrorCode::Success:
      return "Success";

    case ErrorCode::IllegalState:
      return "Client state illegal. Forgot to call start()?";

    case ErrorCode::BadRequest:
      return "Message is ill-formed. Check validity of your topic, tag, etc";

    case ErrorCode::Unauthorized:
      return "Authentication failed. Possibly caused by invalid credentials.";

    case ErrorCode::Forbidden:
      return "Authenticated user does not have privilege to perform the requested action";

    case ErrorCode::NotFound:
      return "Topic not found, which should be created through console or administration API before hand.";

    case ErrorCode::RequestTimeout:
      return "Timeout when connecting, reading from or writing to brokers.";

    case ErrorCode::PayloadTooLarge:
      return "Message body is too large.";

    case ErrorCode::PreconditionRequired:
      return "State of dependent procedure is not right";

    case ErrorCode::TooManyRequest:
      return "Quota exchausted. The user has sent too many requests in a given amount of time.";

    case ErrorCode::UnavailableForLegalReasons:
      return "A server operator has received a legal demand to deny access to a resource or to a set of resources that "
             "includes the requested resource.";

    case ErrorCode::InternalServerError:
      return "Server side interval error";

    case ErrorCode::NotImplemented:
      return "The server either does not recognize the request method, or it lacks the ability to fulfil the request.";

    case ErrorCode::BadGateway:
      return "The server was acting as a gateway or proxy and received an invalid response from the upstream server.";

    case ErrorCode::ServiceUnavailable:
      return "The server cannot handle the request (because it is overloaded or down for maintenance). Generally, this "
             "is a temporary state.";

    case ErrorCode::GatewayTimeout:
      return "The server was acting as a gateway or proxy and did not receive a timely response from the upstream "
             "server.";

    case ErrorCode::ProtocolVersionNotSupported:
      return "The server does not support the protocol version used in the request.";

    case ErrorCode::InsufficientStorage:
      return "The server is unable to store the representation needed to complete the request.";

    default:
      return "Not-Implemented";
    }
  }
};

constexpr int NO_TOPIC_ROUTE_INFO = -1;

constexpr int FAILED_TO_SELECT_MESSAGE_QUEUE = -2;

constexpr int FAILED_TO_RESOLVE_BROKER_ADDRESS_FROM_TOPIC_ROUTE = -3;

constexpr int FAILED_TO_SEND_MESSAGE = -4;

constexpr int FAILED_TO_POP_MESSAGE_ASYNCHRONOUSLY = -5;

constexpr int ILLEGAL_STATE = -6;

constexpr int MESSAGE_ILLEGAL = -7;

constexpr int BAD_CONFIGURATION = -8;

constexpr int MESSAGE_QUEUE_ILLEGAL = -9;

constexpr int ERR_INVALID_MAX_ATTEMPT_TIME = -10;

ROCKETMQ_NAMESPACE_END

namespace std {

template <>
struct is_error_code_enum<ROCKETMQ_NAMESPACE::ErrorCode> : true_type {};

} // namespace std

ROCKETMQ_NAMESPACE_BEGIN

std::error_code make_error_code(ROCKETMQ_NAMESPACE::ErrorCode code);

ROCKETMQ_NAMESPACE_END