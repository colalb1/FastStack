#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <vector>

namespace seraph {
    template <typename T> class RingBuffer {
      private:
        std::vector<T> data_;
        size_t data_size_{0};

        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};

      public:
        RingBuffer(size_t data_size) : data_size_(data_size) {
            std::vector<T> data_(new std::vector<T>(data_size_));
        }

        ~RingBuffer() {
            delete data_;
        }

        RingBuffer(const RingBuffer&) = delete;
        auto operator=(const RingBuffer&) -> RingBuffer& = delete;

        void push(const T& value) {
            return;
        }

        void push(T&& value) {
            return;
        }

        template <typename... Args> void emplace(Args&&... args) {
            return;
        }

        [[nodiscard]] auto pop() -> std::optional<T> {
            return 0;
        }

        [[nodiscard]] auto front() const -> std::optional<T> {
            return 0;
        }

        [[nodiscard]] auto back() const -> std::optional<T> {
            return 0;
        }

        [[nodiscard]] auto empty() const noexcept -> bool {
            return false;
        }

        [[nodiscard]] auto size() const noexcept -> std::size_t {
            return size_.load(std::memory_order_acq_rel);
        }
    };
} // namespace seraph
