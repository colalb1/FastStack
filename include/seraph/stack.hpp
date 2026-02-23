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
        mutable Spinlock lock_;
        std::vector<T> data_;

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

        std::optional<T> pop() {
            SpinlockGuard guard(lock_);

            if (data_.empty()) {
                return std::nullopt;
            }

            T value = std::move(data_.back());
            data_.pop_back();

            return value;
        }

        std::optional<T> top() const {
            SpinlockGuard guard(lock_);

            if (data_.empty()) {
                return std::nullopt;
            }

            return data_.back();
        }

        bool empty() const noexcept {
            SpinlockGuard guard(lock_);

            return data_.empty();
        }

        std::size_t size() const noexcept {
            SpinlockGuard guard(lock_);

            return data_.size();
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

        [[nodiscard]] T top() const {
            if (storage_.empty()) {
                throw std::out_of_range("Cannot read top of an empty stack");
            }

            return storage_.back();
        }

        [[nodiscard]] bool empty() const noexcept {
            return storage_.empty();
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return storage_.size();
        }

        // TODO(colin): Template this container so Stack supports generic value types.
        // TODO(colin): Add custom allocator support if performance goals require it.
        // TODO(colin): Evaluate small-buffer optimization to reduce heap allocations.
    };

} // namespace seraph
