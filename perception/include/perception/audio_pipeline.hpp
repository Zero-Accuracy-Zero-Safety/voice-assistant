#pragma once

#include "ring_buffer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdlib>
#include <stop_token>
#include <thread>
#include <utility>
#include <type_traits>

namespace perception {

template<size_t Capacity, typename T>
class AudioProducer {
private:
    using Ring = detail::AudioRingBuffer<Capacity, T>;
    Ring& ring_buffer_;

public:
    explicit AudioProducer(detail::AudioRingBuffer<Capacity, T>& buffer) noexcept : ring_buffer_(buffer) {
        if (!ring_buffer_.bind_producer()) [[unlikely]] std::abort();
    }

    // Absolute singleton    
    AudioProducer(const AudioProducer&) = delete;
    AudioProducer& operator=(const AudioProducer&) = delete;
    AudioProducer(AudioProducer&&) = delete;
    AudioProducer& operator=(AudioProducer&&) = delete;

    ~AudioProducer() { ring_buffer_.unbind_producer(); }

    [[nodiscard]] bool produce(const T* src, size_t len) noexcept {
        return ring_buffer_.write(src, len);
    }
};

template<std::size_t Capacity, std::size_t ChunkSize, typename T, typename Handler>
class AudioConsumer {
        static_assert(ChunkSize > 0 && ChunkSize <= Capacity, "ChunkSize must be in [1, Capacity]");
    static_assert(std::is_nothrow_invocable_v<Handler&, std::span<const T, ChunkSize>>,
                  "Handler must be callable as handler(std::span<const T, ChunkSize>) and be noexcept");

private:
    using Ring = detail::AudioRingBuffer<Capacity, T>;

    Ring& ring_buffer_;
    [[no_unique_address]] Handler handler_;
    std::array<T, ChunkSize> chunk_{};
    std::jthread worker_;

    void process_loop(std::stop_token st) noexcept {
        while (true) {
            const std::uint32_t seen = ring_buffer_.wake_snapshot();

            if (st.stop_requested()) [[unlikely]] break;

            if (ring_buffer_.read(chunk_)) {
                handler_(std::span<const T, ChunkSize>(chunk_));
            } else {
                ring_buffer_.wait_wake(seen);
            }
        }
    }

public:
    explicit AudioConsumer(Ring& buffer, Handler handler) noexcept(std::is_nothrow_move_constructible_v<Handler>) 
        : ring_buffer_(buffer), handler_(std::move(handler)) {
        if (!ring_buffer_.bind_consumer()) [[unlikely]] std::abort();
    }

    // Absolute singleton   
    AudioConsumer(const AudioConsumer&) = delete;
    AudioConsumer& operator=(const AudioConsumer&) = delete;
    AudioConsumer(AudioConsumer&&) = delete;
    AudioConsumer& operator=(AudioConsumer&&) = delete;

    ~AudioConsumer() {
        stop();
        ring_buffer_.unbind_consumer();
    }

    [[nodiscard]] static constexpr std::size_t get_chunk_size() noexcept { return ChunkSize; }

    void start() {
        if (worker_.joinable()) return;
        worker_ = std::jthread([this](std::stop_token st) noexcept { process_loop(st); });
    }

    void stop() {
        if (!worker_.joinable()) return;
        worker_.request_stop();
        ring_buffer_.wake();
        worker_.join();
    }
};  

template<std::size_t ChunkSize, std::size_t Capacity, typename T, typename Handler>
[[nodiscard]] auto make_consumer(detail::AudioRingBuffer<Capacity, T>& buffer, Handler&& handler) {
    return AudioConsumer<Capacity, ChunkSize, T, std::decay_t<Handler>>(
        buffer, std::forward<Handler>(handler));
}

} // namespace perception