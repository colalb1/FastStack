#pragma once

#include "locks.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <stack>
#include <stdexcept>
#include <vector>

// TODO:
//  1. Spinlock
//  2. Compare-and-Swap
namespace seraph {
    template <typename T> class SpinlockStack {
      private:
        struct alignas(CacheLineSize) AlignedSpinlock {
            Spinlock lock;
        };

        AlignedSpinlock lock_;

        // Deque avoids reallocation delays under a spinlock.
        std::deque<T> data_;

      public:
        SpinlockStack() = default;

        void push(const T& value) {
            SpinlockGuard guard(lock_);

            data_.push_back(value);
        }

        void push(T&& value) {
            SpinlockGuard guard(lock_);

            data_.push_back(std::move(value));
        }

        // Emplace support for perfect forwarding
        template <typename... Args> void emplace(Args&&... args) {
            // Constructing the object outside of the spinlock because
            // an expensive creation creates long spins & wastes CPU.
            T temp(std::forward<Args>(args)...);

            SpinlockGuard guard(lock_);
            data_.push_back(std::move(tmp));
        }

        std::optional<T> pop() {
            std::optional<T> result;

            {
                SpinlockGuard guard(lock_);

                if (data_.empty()) {
                    return std::nullopt;
                }

                result.emplace(std::move(data_.back()));
                data_.pop_back();
                // Lock released
            }

            return result;
        }

        std::optional<T> top() const {
            std::optional<T> result;

            {
                SpinlockGuard guard(lock_);
                if (data_.empty()) {
                    return std::nullopt;
                }

                result.emplace(data_.back());
            }

            return result;
        }

        bool empty() const noexcept {
            SpinlockGuard guard(lock_);

            return data_.empty();
        }
    };

    // This is a Treiber stack: https://en.wikipedia.org/wiki/Treiber_stack
    template <typename T> class CASStack {
      private:
        std::vector<T> storage_;

      public:
        CASStack() = default;

        void push(T value) {
            storage_.push_back(value);
        }

        T pop() {
            if (storage_.empty()) {
                throw std::out_of_range("Cannot pop from an empty stack");
            }

            T value = storage_.back();
            storage_.pop_back();
            return value;
        }

        T top() const {
            if (storage_.empty()) {
                throw std::out_of_range("Cannot read top of an empty stack");
            }

            return storage_.back();
        }

        bool empty() const noexcept {
            return storage_.empty();
        }
    };

} // namespace seraph
