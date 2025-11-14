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

namespace tzcnt_utils {

/// Small object pool that holds 64 objects. It uses a 64-bitmap
/// to track which objects are available; thus it requires a 64-bit machine. All
/// objects are eagerly initialized. Objects are checked out in a LIFO manner,
/// so that the most-frequently used objects will remain hot in cache.
///
/// There are 2 usage patterns:
///
/// Manual pattern:
/// 1. acquire() - check out an object index
/// 2. get() - retrieve a reference to the object, using the index from
/// acquire()
/// 3. release() - return the object to the pool, using the index from acquire()
///
/// Modern pattern:
/// 1. acquire_scoped() - returns an object which wraps a reference to a pool
/// object, and automatically returns that object to the pool when it goes out
/// of scope.
/// 2. access the .value property of the scoped object to use it.
///
/// In either case, the references returned are directly to the objects stored
/// in the pool. Be careful not to accidentally move or copy this object, as
/// the original object is what will be returned to the pool afterward.
///
/// This pool holds only 64 objects. Trying to check out more than 64 objects at
/// once will be an error (assert in debug mode, UB in release mode).
template <typename T> class BitmapObjectPool64 {
  static constexpr uint64_t ONE_BIT = static_cast<uint64_t>(1);
  std::atomic<uint64_t> available_bits;
  std::array<T, 64> objects;

public:
  // This version eagerly default-constructs 64 of the pooled object.
  // Thus it starts with 64 available objects (all bits are 1).
  BitmapObjectPool64() : available_bits{static_cast<uint64_t>(-1)} {}

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
    uint64_t idx;
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
    friend BitmapObjectPool64;

  public:
    T& value;

  private:
    BitmapObjectPool64& pool;
    uint64_t idx;
    ScopedPoolObject(BitmapObjectPool64& Pool, uint64_t Idx)
        : value{Pool.ref(Idx)}, pool{Pool}, idx{Idx} {}

  public:
    ~ScopedPoolObject() { pool.release(idx); }
  };

  ScopedPoolObject acquire_scoped() {
    return ScopedPoolObject{*this, acquire()};
  }
};

} // namespace tzcnt_utils
