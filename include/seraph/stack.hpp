#pragma once

#include "locks.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <thread>
#include <utility>
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
            Node(Node* n, Args&&... args) : value(std::forward<Args>(args)...), next(n) {}
        };

        struct HazardRecord {
            std::atomic<std::thread::id> owner;
            std::atomic<Node*> pointer;
        };

        static constexpr size_t k_max_hazard_pointers{128};
        static HazardRecord hazard_records_[k_max_hazard_pointers];

        static thread_local HazardRecord* local_hazard_;

        static HazardRecord* acquire_hazard() {
            if (local_hazard_) {
                return local_hazard_;
            }

            for (size_t iii{0}; iii < k_max_hazard_pointers; ++iii) {
                std::thread::id empty;
                if (hazard_records_[iii].owner.compare_exchange_strong(
                            empty,
                            std::this_thread::get_id(),
                            std::memory_order_acq_rel
                    )) {

                    local_hazard_ = &hazard_records_[iii];
                    return local_hazard_;
                }
            }

            std::terminate(); // Too many threads
        }

        static bool is_hazard(Node* node) {
            for (size_t iii{0}; iii < k_max_hazard_pointers; ++iii) {
                if (hazard_records_[iii].pointer.load(std::memory_order_acquire) == node) {
                    return true;
                }
            }

            return false;
        }

        static thread_local std::vector<Node*> retire_list_;

        static void retire(Node* node) {
            retire_list_.push_back(node);

            if (retire_list_.size() >= 2 * k_max_hazard_pointers) {
                scan();
            }
        }

        static void scan() {
            auto iterator(retire_list_.begin());

            while (iterator != retire_list_.end()) {
                if (!is_hazard(*iterator)) {
                    delete *iterator;
                    iterator = retire_list_.erase(iterator);
                }
                else {
                    ++iterator;
                }
            }
        }

        std::atomic<Node*> head_{nullptr};
        std::atomic<size_t> size_{0};

      public:
        CASStack() = default;

        ~CASStack() {
            // Stack must be quiescent
            Node* node(head_.load(std::memory_order_relaxed));

            while (node) {
                Node* next = node->next;
                delete node;
                node = next;
            }
        }

        CASStack(const CASStack&) = delete;
        CASStack& operator=(const CASStack&) = delete;

        template <typename... Args> void emplace(Args&&... args) {
            Node* new_node(new Node(nullptr, std::forward<Args>(args)...));
            Node* old_head(head_.load(std::memory_order_relaxed));

            do {
                new_node->next = old_head;
            } while (!head_.compare_exchange_weak(
                    old_head,
                    new_node,
                    std::memory_order_release,
                    std::memory_order_relaxed
            ));

            size_.fetch_add(1, std::memory_order_relaxed);
        }

        void push(const T& value) {
            emplace(value);
        }

        void push(T&& value) {
            emplace(std::move(value));
        }

        std::optional<T> pop() {
            HazardRecord* hazard = acquire_hazard();

            Node* old_head = head_.load(std::memory_order_acquire);

            while (old_head) {
                hazard->pointer.store(old_head, std::memory_order_release);

                if (head_.load(std::memory_order_acquire) != old_head) {
                    old_head = head_.load(std::memory_order_acquire);
                    continue;
                }

                Node* next = old_head->next;

                if (head_.compare_exchange_weak(
                            old_head,
                            next,
                            std::memory_order_acquire,
                            std::memory_order_relaxed
                    )) {

                    hazard->pointer.store(nullptr, std::memory_order_release);

                    size_.fetch_sub(1, std::memory_order_relaxed);

                    std::optional<T> result(std::move(old_head->value));
                    retire(old_head);

                    return result;
                }
            }

            hazard->pointer.store(nullptr, std::memory_order_release);
            return std::nullopt;
        }

        bool empty() const noexcept {
            return head_.load(std::memory_order_acquire) == nullptr;
        }

        size_t size() const noexcept {
            return size_.load(std::memory_order_relaxed);
        }
    };

    // Static storage definitions
    template <typename T>
    typename CASStack<T>::HazardRecord
            CASStack<T>::hazard_records_[CASStack<T>::k_max_hazard_pointers];

    template <typename T>
    thread_local typename CASStack<T>::HazardRecord* CASStack<T>::local_hazard_ = nullptr;

    template <typename T>
    thread_local std::vector<typename CASStack<T>::Node*> CASStack<T>::retire_list_;

} // namespace seraph
