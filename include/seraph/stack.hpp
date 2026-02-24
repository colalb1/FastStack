#pragma once

#include "locks.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

namespace seraph {
    template <typename T> class Stack {
      private:
        // Starts in a spinlock-protected vector mode and promotes once to lock-free CAS.
        static constexpr size_t k_default_thread_threshold = 4;
        static constexpr size_t k_default_streak_threshold = 256;

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

        struct HazardReleaser {
            ~HazardReleaser() {
                if (local_hazard_) {
                    local_hazard_->pointer.store(nullptr, std::memory_order_release);
                    local_hazard_->owner.store(std::thread::id{}, std::memory_order_release);
                    local_hazard_ = nullptr;
                }
            }
        };

        static constexpr size_t k_max_hazard_pointers{128};
        static HazardRecord hazard_records_[k_max_hazard_pointers];
        static thread_local HazardRecord* local_hazard_;
        static thread_local HazardReleaser hazard_releaser_;
        static thread_local std::vector<Node*> retire_list_;

        class ActiveOperationScope {
          public:
            explicit ActiveOperationScope(Stack& stack) : stack_(stack) {
                const size_t active_now =
                        stack_.active_ops_.fetch_add(1, std::memory_order_acq_rel) + 1;
                stack_.observe_contention(active_now);
            }

            ~ActiveOperationScope() {
                stack_.active_ops_.fetch_sub(1, std::memory_order_acq_rel);
            }

          private:
            Stack& stack_;
        };

        static HazardRecord* acquire_hazard() {
            if (local_hazard_) {
                return local_hazard_;
            }

            for (size_t iii = 0; iii < k_max_hazard_pointers; ++iii) {
                std::thread::id empty;
                if (hazard_records_[iii].owner.compare_exchange_strong(
                            empty,
                            std::this_thread::get_id(),
                            std::memory_order_acq_rel
                    )) {
                    (void)hazard_releaser_;
                    local_hazard_ = &hazard_records_[iii];
                    return local_hazard_;
                }
            }

            std::terminate();
        }

        static bool is_hazard(Node* node) {
            for (size_t iii = 0; iii < k_max_hazard_pointers; ++iii) {
                if (hazard_records_[iii].pointer.load(std::memory_order_acquire) == node) {
                    return true;
                }
            }
            return false;
        }

        static void scan() {
            auto iterator = retire_list_.begin();
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

        static void retire(Node* node) {
            retire_list_.push_back(node);
            if (retire_list_.size() >= 2 * k_max_hazard_pointers) {
                scan();
            }
        }

        template <typename... Args> void cas_emplace_impl(Args&&... args) {
            Node* new_node(new Node(nullptr, std::forward<Args>(args)...));
            Node* old_head(cas_head_.load(std::memory_order_relaxed));

            do {
                new_node->next = old_head;
            } while (!cas_head_.compare_exchange_weak(
                    old_head,
                    new_node,
                    std::memory_order_release,
                    std::memory_order_relaxed
            ));

            cas_size_.fetch_add(1, std::memory_order_relaxed);
        }

        std::optional<T> cas_pop_impl() {
            HazardRecord* hazard = acquire_hazard();
            Node* old_head = cas_head_.load(std::memory_order_acquire);

            while (old_head) {
                hazard->pointer.store(old_head, std::memory_order_release);

                if (cas_head_.load(std::memory_order_acquire) != old_head) {
                    old_head = cas_head_.load(std::memory_order_acquire);
                    continue;
                }

                Node* next = old_head->next;
                if (cas_head_.compare_exchange_weak(
                            old_head,
                            next,
                            std::memory_order_acquire,
                            std::memory_order_relaxed
                    )) {
                    hazard->pointer.store(nullptr, std::memory_order_release);
                    cas_size_.fetch_sub(1, std::memory_order_relaxed);

                    std::optional<T> result(std::move(old_head->value));
                    retire(old_head);
                    return result;
                }
            }

            hazard->pointer.store(nullptr, std::memory_order_release);
            return std::nullopt;
        }

        std::optional<T> cas_top_impl() const {
            HazardRecord* hazard = acquire_hazard();
            Node* old_head = cas_head_.load(std::memory_order_acquire);

            while (old_head) {
                hazard->pointer.store(old_head, std::memory_order_release);

                if (cas_head_.load(std::memory_order_acquire) != old_head) {
                    old_head = cas_head_.load(std::memory_order_acquire);
                    continue;
                }

                std::optional<T> result(old_head->value);
                hazard->pointer.store(nullptr, std::memory_order_release);
                return result;
            }

            hazard->pointer.store(nullptr, std::memory_order_release);
            return std::nullopt;
        }

        bool cas_empty_impl() const noexcept {
            return cas_head_.load(std::memory_order_acquire) == nullptr;
        }

        size_t cas_size_impl() const noexcept {
            return cas_size_.load(std::memory_order_relaxed);
        }

        void clear_cas_nodes() {
            Node* node(cas_head_.load(std::memory_order_relaxed));
            while (node) {
                Node* next = node->next;
                delete node;
                node = next;
            }
            cas_head_.store(nullptr, std::memory_order_relaxed);
            cas_size_.store(0, std::memory_order_relaxed);
        }

        void observe_contention(size_t active_now) {
            if (using_cas_.load(std::memory_order_acquire)) {
                return;
            }

            if (active_now >= contention_thread_threshold_) {
                const size_t streak =
                        contention_streak_.fetch_add(1, std::memory_order_acq_rel) + 1;
                if (streak >= promotion_streak_threshold_) {
                    promotion_requested_.store(true, std::memory_order_release);
                }
            }
            else {
                contention_streak_.store(0, std::memory_order_release);
            }
        }

        void maybe_promote_to_cas() {
            if (using_cas_.load(std::memory_order_acquire) ||
                !promotion_requested_.load(std::memory_order_acquire)) {
                return;
            }

            std::unique_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_relaxed)) {
                return;
            }

            std::vector<T> transfer_buffer;
            {
                SpinlockGuard guard(spin_lock_);
                transfer_buffer = std::move(spin_data_);
                spin_data_.clear();
            }

            for (auto& value : transfer_buffer) {
                cas_emplace_impl(std::move(value));
            }

            using_cas_.store(true, std::memory_order_release);
        }

        mutable std::shared_mutex mode_mutex_;

        mutable Spinlock spin_lock_;
        std::vector<T> spin_data_;

        std::atomic<Node*> cas_head_{nullptr};
        std::atomic<size_t> cas_size_{0};
        std::atomic<bool> using_cas_{false};

        const size_t contention_thread_threshold_;
        const size_t promotion_streak_threshold_;

        std::atomic<size_t> active_ops_{0};
        std::atomic<size_t> contention_streak_{0};
        std::atomic<bool> promotion_requested_{false};

      public:
        Stack()
            : contention_thread_threshold_(k_default_thread_threshold),
              promotion_streak_threshold_(k_default_streak_threshold) {}

        explicit Stack(size_t reserve_hint)
            : contention_thread_threshold_(k_default_thread_threshold),
              promotion_streak_threshold_(k_default_streak_threshold) {
            spin_data_.reserve(reserve_hint);
        }

        Stack(size_t reserve_hint, size_t contention_thread_threshold, size_t streak_threshold)
            : contention_thread_threshold_(std::max<size_t>(2, contention_thread_threshold)),
              promotion_streak_threshold_(std::max<size_t>(1, streak_threshold)) {
            spin_data_.reserve(reserve_hint);
        }

        ~Stack() {
            if (using_cas_.load(std::memory_order_acquire)) {
                clear_cas_nodes();
            }
        }

        Stack(const Stack&) = delete;
        Stack& operator=(const Stack&) = delete;

        void reserve(size_t n) {
            ActiveOperationScope scope(*this);
            maybe_promote_to_cas();

            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                return;
            }

            SpinlockGuard guard(spin_lock_);
            spin_data_.reserve(n);
        }

        void push(const T& value) {
            ActiveOperationScope scope(*this);
            maybe_promote_to_cas();

            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                cas_emplace_impl(value);
            }
            else {
                T temp(value);
                SpinlockGuard guard(spin_lock_);
                spin_data_.push_back(std::move(temp));
            }
        }

        void push(T&& value) {
            ActiveOperationScope scope(*this);
            maybe_promote_to_cas();

            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                cas_emplace_impl(std::move(value));
            }
            else {
                SpinlockGuard guard(spin_lock_);
                spin_data_.push_back(std::move(value));
            }
        }

        template <typename... Args> void emplace(Args&&... args) {
            ActiveOperationScope scope(*this);
            maybe_promote_to_cas();

            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                cas_emplace_impl(std::forward<Args>(args)...);
            }
            else {
                T temp(std::forward<Args>(args)...);
                SpinlockGuard guard(spin_lock_);
                spin_data_.push_back(std::move(temp));
            }
        }

        std::optional<T> pop() {
            ActiveOperationScope scope(*this);
            maybe_promote_to_cas();

            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                return cas_pop_impl();
            }

            SpinlockGuard guard(spin_lock_);
            if (spin_data_.empty()) {
                return std::nullopt;
            }

            std::optional<T> result(std::move(spin_data_.back()));
            spin_data_.pop_back();
            return result;
        }

        std::optional<T> top() const {
            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                return cas_top_impl();
            }

            SpinlockGuard guard(spin_lock_);
            if (spin_data_.empty()) {
                return std::nullopt;
            }

            return spin_data_.back();
        }

        bool empty() const noexcept {
            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                return cas_empty_impl();
            }

            SpinlockGuard guard(spin_lock_);
            return spin_data_.empty();
        }

        size_t size() const noexcept {
            std::shared_lock mode_guard(mode_mutex_);
            if (using_cas_.load(std::memory_order_acquire)) {
                return cas_size_impl();
            }

            SpinlockGuard guard(spin_lock_);
            return spin_data_.size();
        }

        bool is_using_cas() const noexcept {
            return using_cas_.load(std::memory_order_acquire);
        }
    };

    template <typename T>
    typename Stack<T>::HazardRecord Stack<T>::hazard_records_[Stack<T>::k_max_hazard_pointers];

    template <typename T>
    thread_local typename Stack<T>::HazardRecord* Stack<T>::local_hazard_ = nullptr;

    template <typename T> thread_local typename Stack<T>::HazardReleaser Stack<T>::hazard_releaser_;

    template <typename T> thread_local std::vector<typename Stack<T>::Node*> Stack<T>::retire_list_;

} // namespace seraph
