#include "StaticNameServerResolver.h"

#include "absl/strings/str_split.h"

#include "gtest/gtest.h"
#include <vector>

ROCKETMQ_NAMESPACE_BEGIN

class StaticNameServerResolverTest : public testing::Test {
public:
  StaticNameServerResolverTest() : resolver_(name_server_list_) {}

protected:
  std::string name_server_list_{"10.0.0.1:9876;10.0.0.2:9876"};
  StaticNameServerResolver resolver_;
};

TEST_F(StaticNameServerResolverTest, testResolve) {
  std::vector<std::string> segments = absl::StrSplit(name_server_list_, ';');
  ASSERT_EQ(segments, resolver_.resolve());
}

ROCKETMQ_NAMESPACE_END