#include "pool_lazy.hpp"
#include "test_common.hpp"
#include "tmc/all_headers.hpp"

#include <atomic>
#include <gtest/gtest.h>
#include <ranges>

#define CATEGORY test_pool_lazy

class CATEGORY : public testing::Test {
protected:
  static void SetUpTestSuite() {
    tmc::cpu_executor().set_thread_count(1).init();
  }

  static void TearDownTestSuite() { tmc::cpu_executor().teardown(); }

  static tmc::ex_cpu& ex() { return tmc::cpu_executor(); }
};

// Pool customization to reserve space for any container type
class DestructorCounterPool : public BitmapObjectPool<destructor_counter> {
  void initialize(void* location) override final {
    ::new (location) destructor_counter(destroyed_count);
  }

public:
  std::atomic<size_t>* destroyed_count;
  DestructorCounterPool(std::atomic<size_t>& Count)
      : destroyed_count{&Count}, BitmapObjectPool<destructor_counter>() {}
};

TEST_F(CATEGORY, destructor_count) {
  test_async_main(ex(), []() -> tmc::task<void> {
    std::atomic<size_t> destroyed_count = 0;
    {
      DestructorCounterPool pool{destroyed_count};
      tmc::barrier bar(63);

      auto tasks = std::ranges::views::iota(0, 63) |
                   std::ranges::views::transform([&](int i) -> tmc::task<void> {
                     return [](
                              DestructorCounterPool& Pool, tmc::barrier& Bar
                            ) -> tmc::task<void> {
                       auto obj = Pool.acquire();
                       co_await Bar;
                       co_return;
                     }(pool, bar);
                   });
      co_await tmc::spawn_many(tasks);
    }
    EXPECT_EQ(destroyed_count, 63);
  }());
}

#undef CATEGORY
