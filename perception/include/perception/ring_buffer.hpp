#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <bit>

namespace perception {

template<std::size_t Capacity, typename T = std::int16_t>
class AudioProducer;

template<std::size_t Capacity, std::size_t ChunkSize, typename T, typename Handler>
class AudioConsumer;

} // namespace perception

namespace perception::detail {

template<typename T>
concept AudioSample = std::is_trivially_copyable_v<T> 
                    && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);

/** @brief Lock-free SPSC ring buffer for audio */
template<size_t Capacity, typename T = uint8_t>
    requires AudioSample<T> && (Capacity > 0) && (std::has_single_bit(Capacity))
class AudioRingBuffer {
private:
    friend class ::perception::AudioProducer<Capacity, T>;
    template<std::size_t C, std::size_t N, typename U, typename H>
    friend class ::perception::AudioConsumer;

    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    static constexpr size_t Mask = Capacity - 1;
    std::array<T, Capacity> buffer_{};

    alignas(64) std::atomic<size_t>   head_{0};
    alignas(64) std::atomic<size_t>   tail_{0};

    // 32 bits -> futex for 4-byte atomic
    alignas(64) std::atomic<uint32_t> wake_seq_{0};
    alignas(64) std::atomic<uint32_t> overruns_{0};

    // Cold-flags -> garant of single roles
    alignas(64) std::atomic<bool>     producer_bound_{false};
    std::atomic<bool>                 consumer_bound_{false};

    [[nodiscard]] bool write(const T* src, std::size_t len) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);

        if (len > Capacity || (head - tail) + len > Capacity) [[unlikely]] {
            overruns_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        const std::size_t head_idx = head & Mask;
        const std::size_t first_chunk = std::min(len, Capacity - head_idx);
        const std::size_t second_chunk = len - first_chunk;

        std::memcpy(&buffer_[head_idx], src, first_chunk * sizeof(T));
        if (second_chunk > 0) [[unlikely]] {
            std::memcpy(&buffer_[0], src + first_chunk, second_chunk * sizeof(T));
        }

        head_.store(head + len, std::memory_order_release);

        wake_seq_.fetch_add(1, std::memory_order_release);
        wake_seq_.notify_one();
        return true;
    }

    template<std::size_t N>
        requires (N > 0 && N <= Capacity)
    [[nodiscard]] bool read(std::array<T, N>& dest) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);

        if ((head - tail) < N) [[unlikely]] return false;

        const std::size_t tail_idx = tail & Mask;
        const std::size_t first_chunk = std::min(N, Capacity - tail_idx);
        const std::size_t second_chunk = N - first_chunk;

        std::memcpy(dest.data(), &buffer_[tail_idx], first_chunk * sizeof(T));
        if (second_chunk > 0) [[unlikely]] {
            std::memcpy(dest.data() + first_chunk, &buffer_[0], second_chunk * sizeof(T));
        }

        tail_.store(tail + N, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::uint32_t wake_snapshot() const noexcept {
        return wake_seq_.load(std::memory_order_acquire);
    }

    void wait_wake(std::uint32_t seen) const noexcept {
        wake_seq_.wait(seen, std::memory_order_acquire);
    }

    void wake() noexcept {
        wake_seq_.fetch_add(1, std::memory_order_release);
        wake_seq_.notify_all();
    }

    [[nodiscard]] bool bind_producer() noexcept { return !producer_bound_.exchange(true, std::memory_order_acq_rel); }
    void unbind_producer() noexcept { producer_bound_.store(false, std::memory_order_release); }

    [[nodiscard]] bool bind_consumer() noexcept { return !consumer_bound_.exchange(true, std::memory_order_acq_rel); }
    void unbind_consumer() noexcept { consumer_bound_.store(false, std::memory_order_release); }


public:
    static_assert(Capacity > 0, "Capacity must be greater than zero");

    explicit constexpr AudioRingBuffer() noexcept = default;
    ~AudioRingBuffer() = default;

    // Non-copyable and non-moved
    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;
    AudioRingBuffer(AudioRingBuffer&&) = delete;
    AudioRingBuffer& operator=(AudioRingBuffer&&) = delete;

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    [[nodiscard]] size_t size() const noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_relaxed);
        return current_head - current_tail;
    }

    [[nodiscard]] std::uint32_t overruns() const noexcept {
        return overruns_.load(std::memory_order_relaxed);
    }
};

} // namespace perception::detail