#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include <new>

namespace orderbook {

// Fixed-size block pool allocator for type T.
//
// Pre-allocates PoolSize objects in a single contiguous slab so that
// hot-path allocations are O(1) pointer pops from a free-list and avoid
// hitting the system allocator (no malloc jitter).
//
// Objects that would overflow the slab fall back to operator new/delete,
// so the allocator is always safe even under unexpected load.
template<typename T, std::size_t PoolSize = 65536>
class PoolAllocator {
public:
    PoolAllocator() {
        // Allocate a raw slab without constructing any T objects.
        slab_ = static_cast<T*>(::operator new(sizeof(T) * PoolSize));
        free_list_.reserve(PoolSize);
        for (std::size_t i = 0; i < PoolSize; ++i) {
            free_list_.push_back(slab_ + i);
        }
    }

    ~PoolAllocator() {
        ::operator delete(slab_);
    }

    // Non-copyable, non-movable (owns unique slab memory)
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // Allocate raw storage for one T. The caller must placement-new into
    // the returned pointer before use.
    T* allocate() {
        if (!free_list_.empty()) {
            T* ptr = free_list_.back();
            free_list_.pop_back();
            return ptr;
        }
        // Slab exhausted — fall back to heap (won't happen under normal load)
        return static_cast<T*>(::operator new(sizeof(T)));
    }

    // Return storage to the pool. Does NOT call ~T(); the caller must
    // explicitly destroy the object (ptr->~T()) before deallocating.
    void deallocate(T* ptr) {
        if (ptr >= slab_ && ptr < slab_ + PoolSize) {
            free_list_.push_back(ptr);
        } else {
            ::operator delete(ptr);
        }
    }

    std::size_t available() const { return free_list_.size(); }
    std::size_t capacity()  const { return PoolSize; }

private:
    T*                  slab_;
    std::vector<T*>     free_list_;
};

} // namespace orderbook
