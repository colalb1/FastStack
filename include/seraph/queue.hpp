#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace seraph
{

    class Queue
    {
      public:
        using value_type = int;

        Queue() = default;

        void enqueue(value_type value)
        {
            storage_.push_back(value);
        }

        value_type dequeue()
        {
            if (empty())
            {
                throw std::out_of_range("Cannot dequeue from an empty queue");
            }

            value_type value = storage_[head_];
            ++head_;

            if (head_ > kCompactionThreshold && head_ * 2 > storage_.size())
            {
                storage_.erase(
                        storage_.begin(),
                        storage_.begin() + static_cast<std::ptrdiff_t>(head_)
                );
                head_ = 0;
            }

            return value;
        }

        [[nodiscard]] value_type front() const
        {
            if (empty())
            {
                throw std::out_of_range("Cannot read front of an empty queue");
            }

            return storage_[head_];
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return head_ >= storage_.size();
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return storage_.size() - head_;
        }

        // TODO(colin): Replace vector+head_ with a ring buffer to avoid erase compaction.
        // TODO(colin): Template this queue so it supports arbitrary value types.
        // TODO(colin): Add benchmarks for enqueue/dequeue under sustained load.

      private:
        static constexpr std::size_t kCompactionThreshold = 64;
        std::vector<value_type> storage_;
        std::size_t head_ = 0;
    };

} // namespace seraph
