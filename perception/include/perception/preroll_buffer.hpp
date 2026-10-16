#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace perception {

/**
 * Pre-roll: histroy of some last chunks
 * Used ONLY by Consumer
 */
template<std::size_t ChunkSize, std::size_t MaxChunks, typename T = std::int16_t>
class PreRollBuffer {
    static_assert(ChunkSize > 0 && MaxChunks > 0);
private:
    std::array<std::array<T, ChunkSize>, MaxChunks> slots_{};
    std::size_t next_  = 0;
    std::size_t count_ = 0;

public:
    void push(std::span<const T, ChunkSize> chunk) noexcept {
        std::copy(chunk.begin(), chunk.end(), slots_[next_].begin());
        next_ = (next_ + 1) % MaxChunks;
        if (count_ < MaxChunks) ++count_;
    }

    template<typename Send>
    [[nodiscard]] bool drain_to(Send&& send) const {
        const std::size_t oldest = (next_ + MaxChunks - count_) % MaxChunks;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& slot = slots_[(oldest + i) % MaxChunks];
            if (!send(slot.data(), ChunkSize)) return false;
        }
        return true;
    }

    void clear() noexcept {
        next_ = 0;
        count_ = 0;
    }

    [[nodiscard]] std::size_t size_chunks() const noexcept { return count_; }
};

} // namespace perception
