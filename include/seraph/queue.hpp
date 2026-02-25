#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace seraph {

    template <typename T> class Queue {
      public:
        Queue() = default;

        [[nodiscard]] T front() const {
            if (empty()) {
                throw std::out_of_range("Cannot read front of an empty queue");
            }

            return storage_[head_];
        }

        [[nodiscard]] T back() const {
            if (empty()) {
                throw std::out_of_range("Cannot read front of an empty queue");
            }

            return storage_[0];
        }

        [[nodiscard]] bool empty() const noexcept {
            return head_ >= storage_.size();
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return storage_.size() - head_;
        }

        void push(T value) {
            storage_.push_back(value);
        }

        void push_range(T value) {
            // push back multiples values
        }

        void emplace(T value) {
            // construct element in-place
            return;
        }

        T pop() {
            if (empty()) {
                throw std::out_of_range("Cannot dequeue from an empty queue");
            }

            T value = storage_[head_];
            ++head_;

            if (head_ > kCompactionThreshold && head_ * 2 > storage_.size()) {
                storage_.erase(
                        storage_.begin(),
                        storage_.begin() + static_cast<std::ptrdiff_t>(head_)
                );
                head_ = 0;
            }

            return value;
        }

        void swap() {
            // Exchanges the contents of the container adaptor with those of other
            return;
        }

      private:
        static constexpr std::size_t kCompactionThreshold = 64;
        std::vector<T> storage_;
        std::size_t head_ = 0;
    };

} // namespace seraph
