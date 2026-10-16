#pragma once

#include "miniaudio.h"
#include "audio_pipeline.hpp"
#include <cstdint>
#include <cstdlib>

namespace perception {

template<size_t Capacity, typename T>
class AudioCapture {
private:
    ma_device device_{};
    bool is_running_{false};

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) noexcept {
        (void)pOutput;
        if (!pInput || !pDevice->pUserData) [[unlikely]] return;

        auto* producer = static_cast<AudioProducer<Capacity, T>*>(pDevice->pUserData);
        const auto* samples = static_cast<const T*>(pInput);

        (void)producer->produce(samples, frameCount);
    }

public:
    explicit AudioCapture(AudioProducer<Capacity, T>& producer, std::uint32_t sample_rate = 16000) noexcept {
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.capture.format   = ma_format_s16;
        config.capture.channels = 1;
        config.sampleRate       = sample_rate;
        config.dataCallback     = data_callback;
        config.pUserData        = &producer;

        if (ma_device_init(nullptr, &config, &device_) != MA_SUCCESS) [[unlikely]] {
            std::abort();
        }
    }

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    AudioCapture(AudioCapture&&) = delete;
    AudioCapture& operator=(AudioCapture&&) = delete;

    ~AudioCapture() noexcept {
        stop();
        ma_device_uninit(&device_);
    }

    void start() noexcept {
        if (is_running_) return;
        if (ma_device_start(&device_) == MA_SUCCESS) {
            is_running_ = true;
        }
    }

    void stop() noexcept {
        if (!is_running_) return;
        ma_device_stop(&device_);
        is_running_ = false;
    }

    [[nodiscard]] constexpr bool is_running() const noexcept { return is_running_; }
};

} // namespace perception