#include "test_common.hpp"
#include "tmc/all_headers.hpp"

#include <atomic>
#include <gtest/gtest.h>

#define CATEGORY test_pool_lazy

class CATEGORY : public testing::Test {
protected:
  static void SetUpTestSuite() {
    tmc::cpu_executor().set_thread_count(4).init();
  }

  static void TearDownTestSuite() { tmc::cpu_executor().teardown(); }

  static tmc::ex_cpu& ex() { return tmc::cpu_executor(); }
};

TEST_F(CATEGORY, nonblocking) {
  test_async_main(ex(), []() -> tmc::task<void> {
    tmc::atomic_condvar<int> cv(1);
    EXPECT_EQ(cv.ref().load(std::memory_order_relaxed), 1);
    co_await cv.await(2);
    cv.ref().store(2, std::memory_order_relaxed);
    co_await cv.await(3);
  }());
}

#undef CATEGORY
