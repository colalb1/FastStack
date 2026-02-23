#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

// Need three versions: atomic, spinlock, compare-and-swap
namespace seraph {
    template <typename T> class AtomicStack {
      private:
        std::vector<T> storage_;

      public:
        Stack() = default;

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

    template <typename T> class SpinlockStack {
      private:
        std::vector<T> storage_;

      public:
        Stack() = default;

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

    template <typename T> class CASStack {
      private:
        std::vector<T> storage_;

      public:
        Stack() = default;

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
