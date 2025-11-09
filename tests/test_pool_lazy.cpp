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
    tmc::cpu_executor().set_thread_count(16).init();
  }

  static void TearDownTestSuite() { tmc::cpu_executor().teardown(); }

  static tmc::ex_cpu& ex() { return tmc::cpu_executor(); }
};

// Pool customization to pass through external pointer variable
// to each destructor_counter constructor
class DestructorCounterPool
    : public BitmapObjectPoolImpl<destructor_counter, DestructorCounterPool> {
  friend class BitmapObjectPoolImpl<destructor_counter, DestructorCounterPool>;
  void initialize(void* location) {
    ::new (location) destructor_counter(destroyed_count);
  }

public:
  std::atomic<size_t>* destroyed_count;
  DestructorCounterPool(std::atomic<size_t>& Count)
      : destroyed_count{&Count},
        BitmapObjectPoolImpl<destructor_counter, DestructorCounterPool>() {}
};

template <size_t Count> void destructor_count_test(tmc::ex_cpu& ex) {
  test_async_main(ex, []() -> tmc::task<void> {
    std::atomic<size_t> destroyed_count = 0;
    {
      DestructorCounterPool pool{destroyed_count};
      tmc::barrier bar(Count);

      auto tasks =
        std::ranges::views::iota(0UL, Count) |
        std::ranges::views::transform([&](size_t i) -> tmc::task<void> {
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
    EXPECT_EQ(destroyed_count.load(), Count);
  }());
}

TEST_F(CATEGORY, destructor_count_0) { destructor_count_test<0>(ex()); }
TEST_F(CATEGORY, destructor_count_1) { destructor_count_test<1>(ex()); }
TEST_F(CATEGORY, destructor_count_63) { destructor_count_test<63>(ex()); }
TEST_F(CATEGORY, destructor_count_64) { destructor_count_test<64>(ex()); }
TEST_F(CATEGORY, destructor_count_127) { destructor_count_test<127>(ex()); }
TEST_F(CATEGORY, destructor_count_128) { destructor_count_test<128>(ex()); }
TEST_F(CATEGORY, destructor_count_9999) { destructor_count_test<9999>(ex()); }

#undef CATEGORY
