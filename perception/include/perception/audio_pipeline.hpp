#pragma once

#include "ring_buffer.hpp"
#include <thread>
#include <concepts>

namespace perception {

template<size_t Capacity, typename T = uint16_t>
class AudioProducer {
private:
    detail::AudioRingBuffer<Capacity, T>& ring_buffer_;

public:
    explicit AudioProducer(detail::AudioRingBuffer<Capacite, T>& buffer) noexcept
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
};  

} // namespace perception