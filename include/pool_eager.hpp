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

template <typename T> class BitmapObjectPool {
  static constexpr uint64_t ONE_BIT = static_cast<uint64_t>(1);
  std::atomic<uint64_t> available_bits;
  std::array<T, 64> objects;

public:
  // This version eagerly default-constructs 64 of the pooled object.
  // Thus it starts with 64 available objects (all bits are 1).
  BitmapObjectPool() : available_bits{static_cast<uint64_t>(-1)} {}

  bool try_acquire(uint64_t& idx) {
    auto bits = available_bits.load(std::memory_order_relaxed);
    while (true) {
      if (bits == 0) {
        return false;
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

  void release(uint64_t idx) {
    auto bit = ONE_BIT << idx;
    // Set this bit to release ownership of the object
    [[maybe_unused]] auto old = available_bits.fetch_or(bit);
    assert((old & bit) == 0 && "Released object you didn't own!");
  }

  // Pass the index that you got from acquire()
  T& ref(uint64_t idx) { return objects[idx]; }

  class ScopedPoolObject {
    friend BitmapObjectPool;

  public:
    T& value;

  private:
    BitmapObjectPool& pool;
    uint64_t idx;
    ScopedPoolObject(BitmapObjectPool& Pool, uint64_t Idx)
        : value{Pool.ref(Idx)}, pool{Pool}, idx{Idx} {}

  public:
    ~ScopedPoolObject() { pool.release(idx); }
  };

  ScopedPoolObject acquire_scoped() {
    return ScopedPoolObject{*this, acquire()};
  }
};
