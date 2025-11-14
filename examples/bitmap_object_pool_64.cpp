#include "bitmap_object_pool_64.hpp"

// Using TMC to test this because it's an easy way to spin up multiple threads,
// but not required for the above code to work.
#define TMC_IMPL
#include "tmc/all_headers.hpp"
#include <ranges>
#include <string>
#include <unordered_map>

using namespace tzcnt_utils;

void print_from_pool(
  BitmapObjectPool64<std::unordered_map<int, std::string>>& pool, int i
) {
  if (i % 2 == 0) {
    // Demonstrate the use of manual acquire/release
    auto idx = pool.acquire();
    std::unordered_map<int, std::string>& map = pool.ref(idx);

    const auto iter = map.find(i);
    if (iter != map.cend()) {
      auto s = iter->second;
      // std::printf("%s ", s.c_str());
    } else {
      auto [it, inserted] = map.emplace(i, std::to_string(i));
      assert(inserted);
      std::printf("%s ", it->second.c_str());
    }

    pool.release(idx);
  } else {
    // Demonstrate the use of the scoped object
    auto obj = pool.acquire_scoped();
    std::unordered_map<int, std::string>& map = obj.value;

    const auto iter = map.find(i);
    if (iter != map.cend()) {
      auto s = iter->second;
      // std::printf("%s ", s.c_str());
    } else {
      auto [it, inserted] = map.emplace(i, std::to_string(i));
      assert(inserted);
      std::printf("%s ", it->second.c_str());
    }
  }
}

tmc::task<void>
pool_user(BitmapObjectPool64<std::unordered_map<int, std::string>>& pool) {
  for (size_t i = 0; i < 10; ++i) {
    print_from_pool(pool, i);
  }
  co_return;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  tmc::cpu_executor().set_thread_count(4);
  return tmc::async_main([]() -> tmc::task<int> {
    BitmapObjectPool64<std::unordered_map<int, std::string>> stringPool;

    auto tasks = std::ranges::views::iota(0, 10000) |
                 std::ranges::views::transform([&](int i) -> tmc::task<void> {
                   return pool_user(stringPool);
                 });
    co_await tmc::spawn_many(tasks);
    co_return 0;
  }());
}
