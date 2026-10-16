#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace perception {

template<typename D, std::size_t N>
concept FrameDetector = requires(D& d, std::span<const std::int16_t, N> frame) {
    { d.process(frame) } noexcept -> std::convertible_to<float>;
};

class ManualTriggerDetector {
private:
    std::atomic<bool> pending_{false};

public:
    void trigger() noexcept { pending_.store(true, std::memory_order_release); }

    template<std::size_t N>
    [[nodiscard]] float process(std::span<const std::int16_t, N>) noexcept {
        return pending_.exchange(false, std::memory_order_acq_rel) ? 1.0f : 0.0f;
    }
};

} // namespace perception
