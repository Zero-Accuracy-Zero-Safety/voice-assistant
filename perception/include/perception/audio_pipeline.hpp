#pragma once

#include "ring_buffer.hpp"
#include <thread>
#include <concepts>
#include <vector>

namespace perception {

template<size_t Capacity, typename T = uint16_t>
class AudioProducer {
private:
    detail::AudioRingBuffer<Capacity, T>& ring_buffer_;

public:
    explicit AudioProducer(detail::AudioRingBuffer<Capacity, T>& buffer) noexcept
        : ring_buffer_(buffer) {}

    [[nodiscard]] bool produce(const T* src, size_t len) noexcept {
        return ring_buffer_.write(src, len);
    }
};

template<size_t Capacity, typename T = uint16_t>
class AudioConsumer {
private:
    detail::AudioRingBuffer<Capacity, T>& ring_buffer_;
    size_t chunk_size_;
    std::atomic<bool> running_{false};
    std::jthread worker_thread_;

    void process_loop() {
        std::vector<T> processing_chunk(chunk_size_);

        while (running_.load(std::memory_order_relaxed)) {
            if (ring_buffer_.size() >= chunk_size_) {
                if (ring_buffer_.read(processing_chunk.data(), chunk_size_)) {
                    // TODO: detector logic
                }
            } else {
                auto current_head = ring_buffer_.get_head();
                ring_buffer_.wait_for_head(current_head);
            }
        }
    }

public:
    explicit AudioConsumer(detail::AudioRingBuffer<Capacity, T>& buffer, size_t chunk_size) noexcept
        : ring_buffer_(buffer), chunk_size_(chunk_size) {}

    [[nodiscard]] bool consume(T* dest) noexcept {
        if (ring_buffer_.size() < chunk_size_) {
            return false;
        }
        return ring_buffer_.read(dest, chunk_size_);
    }

    [[nodiscard]] constexpr size_t get_chunk_size() const noexcept {
        return chunk_size_;
    }

    void start() {
        if (running_.load(std::memory_order_relaxed)) return;
        running_.store(true, std::memory_order_relaxed);
        worker_thread_ = std::jthread([this]() { process_loop(); });
    }

    void stop() {
        running_.store(false, std::memory_order_relaxed);
        ring_buffer_.notify();
    }
};  

} // namespace perception