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

        explicit SpinlockStack(size_t reserve_hint) {
            data_.reserve(reserve_hint);
        }

        void reserve(size_t n) {
            SpinlockGuard guard(lock_);
            data_.reserve(n);
        }

        void push(const T& value) {
            T temp(value);

            SpinlockGuard guard(lock_);
            data_.push_back(std::move(temp));
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
            data_.push_back(std::move(temp));
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

        size_t size() const noexcept {
            SpinlockGuard guard(lock_);
            return data_.size();
        }
    };

    // This is a Treiber stack: https://en.wikipedia.org/wiki/Treiber_stack
    template <typename T> class CASStack {
      private:
        struct Node {
            T value;
            Node* next;

            template <typename... Args>
            explicit Node(Node* next_node, Args&&... args)
                : value(std::forward<Args>(args)...), next(next_node) {}
        };

        std::atomic<Node*> head_{nullptr};

      public:
        CASStack() = default;

        ~CASStack() {
            // Not safe under concurrent access.
            Node* node = head_.load(std::memory_order_relaxed);

            while (node) {
                Node* next = node->next;
                delete node;
                node = next;
            }
        }

        // No copies.
        CASStack(const CASStack&) = delete;
        CASStack& operator=(const CASStack&) = delete;

        void push(const T& value) {
            emplace(value);
        }

        void push(T&& value) {
            emplace(std::move(value));
        }

        template <typename... Args> void emplace(Args&&... args) {
            Node* new_node = new Node(nullptr, std::forward<Args>(args)...);
            Node* old_head = head_.load(std::memory_order_relaxed);

            do {
                new_node->next = old_head;
            } while (!head_.compare_exchange_weak(
                    old_head,
                    new_node,
                    std::memory_order_release,
                    std::memory_order_relaxed
            ));
        }

        std::optional<T> pop() {
            Node* old_head = head_.load(std::memory_order_acquire);

            while (old_head) {
                Node* next = old_head->next;

                if (head_.compare_exchange_weak(
                            old_head,
                            next,
                            std::memory_order_acquire,
                            std::memory_order_relaxed
                    )) {

                    std::optional<T> result(std::move(old_head->value));
                    delete old_head;

                    return result;
                }
            }

            return std::nullopt;
        }

        bool empty() const noexcept {
            return head_.load(std::memory_order_acquire) == nullptr;
        }
    };

} // namespace seraph
