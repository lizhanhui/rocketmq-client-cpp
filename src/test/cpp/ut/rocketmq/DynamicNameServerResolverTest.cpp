#include "DynamicNameServerResolver.h"

#include "gtest/gtest.h"
#include <chrono>
#include <memory>

ROCKETMQ_NAMESPACE_BEGIN

class DynamicNameServerResolverTest : public testing::Test {
public:
  DynamicNameServerResolverTest()
      : resolver_(std::make_shared<DynamicNameServerResolver>(endpoint_, std::chrono::seconds(1))) {}

protected:
  std::string endpoint_{"http://jmenv.tbsite.net:8080/rocketmq/nsaddr"};
  std::shared_ptr<DynamicNameServerResolver> resolver_;
};

TEST_F(DynamicNameServerResolverTest, testResolve) {
  auto name_server_list = resolver_->resolve();
  ASSERT_FALSE(name_server_list.empty());
}

ROCKETMQ_NAMESPACE_END