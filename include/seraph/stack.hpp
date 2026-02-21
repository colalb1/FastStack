#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace seraph
{

    class Stack
    {
      public:
        using value_type = int;

        Stack() = default;

        void push(value_type value)
        {
            storage_.push_back(value);
        }

        value_type pop()
        {
            if (storage_.empty())
            {
                throw std::out_of_range("Cannot pop from an empty stack");
            }

            value_type value = storage_.back();
            storage_.pop_back();
            return value;
        }

        [[nodiscard]] value_type top() const
        {
            if (storage_.empty())
            {
                throw std::out_of_range("Cannot read top of an empty stack");
            }

            return storage_.back();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return storage_.empty();
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return storage_.size();
        }

        // TODO(colin): Template this container so Stack supports generic value types.
        // TODO(colin): Add custom allocator support if performance goals require it.
        // TODO(colin): Evaluate small-buffer optimization to reduce heap allocations.

      private:
        std::vector<value_type> storage_;
    };

} // namespace seraph
