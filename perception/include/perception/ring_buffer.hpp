#pragma once

#include <concepts>
#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <cstring>
#include <type_traits>

namespace perception::detail {

template<typename T>
concept AudioSample = std::is_trivial_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);

template<size_t Capacity, typename T = uint8_t>
    requires AudioSample<T> && ((Capacity & (Capacity - 1)) == 0)
class AudioRingBuffer {
private:
    static constexpr size_t Mask = Capacity - 1;
    std::array<T, Capacity> buffer_{};

    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    static_assert(Capacity > 0, "Capacity must be greater than zero");

    explicit constexpr AudioRingBuffer() noexcept = default;
    ~AudioRingBuffer() = default;

    // Non-copyable
    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

    // Can be moved
    AudioRingBuffer(AudioRingBuffer&&) noexcept = default;
    AudioRingBuffer& operator=(AudioRingBuffer&&) noexcept = default;

    [[nodiscard]] bool write(const T* src, size_t len) noexcept {
        if (len >= Capacity) [[unlikely]] return false;

        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if ((current_head - current_tail) + len > Capacity) [[unlikely]] return false; 

        const size_t head_idx = current_head & Mask;
        const size_t space_to_end = Capacity - head_idx;
        const size_t first_chunk = std::min(len, space_to_end);
        const size_t second_chunk = len - first_chunk;

       std::memcpy(&buffer_[head_idx], src, first_chunk * sizeof(T));
    
       if (second_chunk > 0) [[unlikely]] {
            std::memcpy(&buffer_[0], src + first_chunk, second_chunk * sizeof(T));
       }

        head_.store(current_head + len, std::memory_order_release);
        head_.notify_one();
        return true;
    }

    [[nodiscard]] bool read(T* dest, size_t len) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if ((current_head - current_tail) < len) [[unlikely]] return false;

        const size_t tail_idx = current_tail & Mask;
        const size_t space_to_end = Capacity - tail_idx;
        const size_t first_chunk = std::min(len, space_to_end);
        const size_t second_chunk = len - first_chunk;

        std::memcpy(dest, &buffer_[tail_idx], first_chunk * sizeof(T));

        if (second_chunk > 0) [[unlikely]] {
            std::memcpy(dest + first_chunk, &buffer_[0], second_chunk * sizeof(T));
        }

        tail_.store(current_tail + len, std::memory_order_release);
        return true;
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return Capacity;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        return current_head - current_tail;
    }

    [[nodiscard]] size_t get_head() const noexcept {
        return head_.load(std::memory_order_relaxed);
    }

    void wait_for_head(size_t expected) const noexcept {
        head_.wait(expected, std::memory_order_acquire);
    }

    void notify() noexcept {
        head_.notify_one();
    }
};

} // namespace perception::detail