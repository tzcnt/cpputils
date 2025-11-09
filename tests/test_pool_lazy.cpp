#include "pool_lazy.hpp"
#include "test_common.hpp"
#include "tmc/all_headers.hpp"

#include <atomic>
#include <gtest/gtest.h>
#include <ranges>
#include <vector>

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
            // Use this barrier to force each task to acquire a newly created
            // pool object, before releasing them all
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

template <size_t Count> void vector_test(tmc::ex_cpu& ex) {
  test_async_main(ex, []() -> tmc::task<void> {
    BitmapObjectPool<std::vector<size_t>> pool;

    auto tasks =
      std::ranges::views::iota(0UL, Count) |
      std::ranges::views::transform([&](size_t i) -> tmc::task<void> {
        return [](
                 BitmapObjectPool<std::vector<size_t>>& Pool, size_t idx
               ) -> tmc::task<void> {
          auto obj = Pool.acquire();
          auto& vec = obj.value;
          vec.push_back(idx);
          co_return;
        }(pool, i);
      });
    co_await tmc::spawn_many(tasks);

    std::vector<size_t> results;
    results.reserve(Count);
    size_t vecCount = 0;
    pool.for_each_available(
      [&results, &vecCount](std::vector<size_t>& poolVec) -> void {
        results.insert(
          results.end(), std::make_move_iterator(poolVec.begin()),
          std::make_move_iterator(poolVec.end())
        );
        ++vecCount;
      }
    );

    if (Count > 1000) {
      // Just verify that multithreaded accesses are occurring.
      // This could theoretically fail, but should be fine.
      EXPECT_GT(vecCount, 1);
    }

    // Each value should be present although they may be in different pools
    std::sort(results.begin(), results.end());
    for (size_t i = 0; i < Count; ++i) {
      EXPECT_EQ(i, results[i]);
    }
  }());
}

TEST_F(CATEGORY, vector_0) { vector_test<0>(ex()); }
TEST_F(CATEGORY, vector_1) { vector_test<1>(ex()); }
TEST_F(CATEGORY, vector_63) { vector_test<63>(ex()); }
TEST_F(CATEGORY, vector_64) { vector_test<64>(ex()); }
TEST_F(CATEGORY, vector_127) { vector_test<127>(ex()); }
TEST_F(CATEGORY, vector_128) { vector_test<128>(ex()); }
TEST_F(CATEGORY, vector_9999) { vector_test<9999>(ex()); }

#undef CATEGORY
