// Copyright (c) 2025 Logan McDougall
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>

template <typename T, typename Derived> class BitmapObjectPool {
  // std::optional-like type that allocates space for an object
  // without managing its lifetime.
  union alignas(64) pool_opt {
    T value;

    operator T&() & { return value; }
    operator const T&() const& { return value; }
    operator T&&() && { return static_cast<T&&>(value); }

    // Don't construct the contained object; the pool will do it.
    pool_opt() {}

    // Don't destroy the contained object; the pool will do it.
    ~pool_opt() {}

    pool_opt(const pool_opt&) = delete;
    pool_opt& operator=(const pool_opt&) = delete;
    pool_opt(pool_opt&&) = delete;
    pool_opt& operator=(pool_opt&&) = delete;
  };

  static constexpr uint64_t ONE_BIT = static_cast<uint64_t>(1);
  std::atomic<uint64_t> available_bits;
  std::atomic<uint64_t> count;
  std::array<pool_opt, 64> objects;

public:
  // This version lazily initializes objects as needed.
  // Thus it starts with 0 available objects (all bits are 0).
  BitmapObjectPool() : available_bits{0}, count{0} {}

  // Destroy any objects that were used.
  ~BitmapObjectPool() {
    for (size_t i = 0; i < count; ++i) {
      objects[i].value.~T();
    }
  }

protected:
  bool try_init(uint64_t& idx) {
    idx = count.load(std::memory_order_relaxed);
    while (true) {
      if (idx >= 64) {
        return false;
      }
      if (count.compare_exchange_strong(idx, idx + 1)) {
        // Use CRTP to delegate initialization to derived class
        ::new (static_cast<void*>(&objects[idx].value)) T();
        static_cast<Derived*>(this)->init(objects[idx]);
        return true;
      }
    }
  }

public:
  // Try to acquire each currently available element of the list one-by-one and
  // run func() on it.
  template <typename Fn> void for_each_available(Fn func) {
    auto max = count.load(std::memory_order_relaxed);
    for (uint64_t i = 0; i < max; ++i) {
      auto bit = ONE_BIT << i;
      // Try to clear this bit to take ownership of the object.
      // If it was already clear, nothing happens.
      auto bits = available_bits.fetch_and(~bit);
      if ((bits & bit) != 0) {
        // We now own this object. Run the caller's functor on it.
        func(objects[i]);
        // Now release the object
        available_bits.fetch_or(bit);
      }
    }
  }

  bool try_acquire(uint64_t& idx) {
    auto bits = available_bits.load(std::memory_order_relaxed);
    while (true) {
      if (bits == 0) {
        return try_init(idx);
      }
      idx = static_cast<uint64_t>(std::countr_zero(bits));
      auto bit = ONE_BIT << idx;
      // Clear this bit to take ownership of the object
      bits = available_bits.fetch_and(~bit);
      if ((bits & bit) != 0) {
        return true;
      }
    }
  }

  uint64_t acquire() {
    size_t idx;
    [[maybe_unused]] bool ok = try_acquire(idx);
    assert(ok && "All pool objects are in use!");
    return idx;
  }

  // Pass the index that you got from acquire().
  // Returns a referencec to the underlying object.
  T& get(uint64_t idx) { return objects[idx]; }

  void release(uint64_t idx) {
    auto bit = ONE_BIT << idx;
    // Set this bit to release ownership of the object
    [[maybe_unused]] auto old = available_bits.fetch_or(bit);
    assert((old & bit) == 0 && "Released object you didn't own!");
  }

  class ScopedPoolObject {
    friend BitmapObjectPool;

  public:
    T& value;

  private:
    BitmapObjectPool& pool;
    uint64_t idx;
    ScopedPoolObject(BitmapObjectPool& Pool, uint64_t Idx)
        : value{Pool.get(Idx)}, pool{Pool}, idx{Idx} {}

  public:
    ~ScopedPoolObject() { pool.release(idx); }
  };

  ScopedPoolObject acquire_scoped() {
    return ScopedPoolObject{*this, acquire()};
  }
};

// CRTP customization to reserve space for any container type
template <typename C>
class ContainerPool : public BitmapObjectPool<C, ContainerPool<C>> {
  friend class BitmapObjectPool<C, ContainerPool<C>>;

  // Implement init() for the container type
  void init(C& newContainer) { newContainer.reserve(500); }

  // This replaces the need for the map_vector functionality
  // instead we just process any free maps one-by-one in place
  void clean() {
    BitmapObjectPool<C, ContainerPool<C>>::for_each_available([](C& map) {
      // if (absl::erase_if(map, [](const auto& pair) {
      //       return pair.second->dwReference == 0;
      //     }) > 0)
      //   map.rehash(0);
    });
  }
};
