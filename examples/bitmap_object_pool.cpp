#include "bitmap_object_pool.hpp"

// Using TMC to test this because it's an easy way to spin up multiple threads,
// but not required for this to work.
#define TMC_IMPL
#include "tmc/all_headers.hpp"

#include <ranges>
#include <string>
#include <unordered_map>

// Pool customization to reserve space for any container type
template <typename C>
class ContainerPool final : public BitmapObjectPoolImpl<C, ContainerPool<C>> {
  friend class BitmapObjectPoolImpl<C, ContainerPool<C>>;
  void initialize(void* location) {
    C* newContainer = ::new (location) C();
    newContainer->reserve(500);
  }
};

void print_from_pool(
  ContainerPool<std::unordered_map<int, std::string>>& pool, int i
) {
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

tmc::task<void>
pool_user(ContainerPool<std::unordered_map<int, std::string>>& pool) {
  for (size_t i = 0; i < 10; ++i) {
    print_from_pool(pool, i);
  }
  co_return;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  tmc::cpu_executor().set_thread_count(64);
  return tmc::async_main([]() -> tmc::task<int> {
    ContainerPool<std::unordered_map<int, std::string>> stringPool;

    auto tasks = std::ranges::views::iota(0, 1000000) |
                 std::ranges::views::transform([&](int i) -> tmc::task<void> {
                   return pool_user(stringPool);
                 });
    co_await tmc::spawn_many(tasks);
    co_return 0;
  }());
}

// struct O {
//   O() {}
//   ~O() { std::printf("destroyed "); }
// };

// tmc::task<void> opool_user(BitmapObjectPool<O>& pool) {
//   auto scoped = pool.acquire();
//   co_return;
// }
// int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
//   tmc::cpu_executor().set_thread_count(64);
//   return tmc::async_main([]() -> tmc::task<int> {
//     BitmapObjectPool<O> stringPool;

//     auto tasks = std::ranges::views::iota(0, 1000000) |
//                  std::ranges::views::transform([&](int i) -> tmc::task<void>
//                  {
//                    return opool_user(stringPool);
//                  });
//     co_await tmc::spawn_many(tasks);
//     co_return 0;
//   }());
// }
