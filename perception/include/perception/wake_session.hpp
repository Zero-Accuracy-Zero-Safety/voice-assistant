#pragma once

#include "detector.hpp"
#include "preroll_buffer.hpp"
#include "unix_socket_client.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

namespace perception {

struct SessionConfig {
    float       threshold         = 0.5f;
    std::size_t hits_required     = 1;
    std::size_t cooldown_chunks   = 12;
    std::size_t max_stream_chunks = 187;
};

enum class SessionState { 
    Idle, 
    Streaming, 
    Cooldown 
};


template<std::size_t ChunkSize, std::size_t PreRollChunks, typename Detector>
    requires FrameDetector<Detector, ChunkSize>
class WakeSession {
private:
    using Chunk = std::span<const std::int16_t, ChunkSize>;

    Detector&          detector_;
    UnixSocketClient&  client_;
    const char*        sock_path_;
    SessionConfig      cfg_;

    PreRollBuffer<ChunkSize, PreRollChunks> preroll_;
    SessionState state_ = SessionState::Idle;
    std::size_t  hits_    = 0;
    std::size_t  counter_ = 0;

    [[nodiscard]] bool send_samples(const std::int16_t* data, std::size_t n) noexcept {
        return client_.send_all(data, n * sizeof(std::int16_t));
    }

    void finish_stream(const char* reason) noexcept {
        std::fprintf(stderr, "[session] stream finished: %s\n", reason);
        client_.close();
        preroll_.clear();
        counter_ = 0;
        state_ = SessionState::Cooldown;
    }

    void on_idle(Chunk chunk) noexcept {
        preroll_.push(chunk);

        const float score = detector_.process(chunk);
        hits_ = (score >= cfg_.threshold) ? hits_ + 1 : 0;
        if (hits_ < cfg_.hits_required) return;
        hits_ = 0;

        std::fprintf(stderr, "[session] wake word! score=%.2f, pre-roll=%zu chunks\n", static_cast<double>(score), preroll_.size_chunks());

        if (!client_.connect(sock_path_)) {
            std::fprintf(stderr, "[session] connect failed, skipping this trigger\n");
            preroll_.clear();
            counter_ = 0;
            state_ = SessionState::Cooldown;
            return;
        }

        const bool ok = preroll_.drain_to(
            [this](const std::int16_t* data, std::size_t n) noexcept { return send_samples(data, n); });
        if (!ok) {
            finish_stream("send of pre-roll failed");
            return;
        }

        counter_ = 0;
        state_ = SessionState::Streaming;
    }

    void on_streaming(Chunk chunk) noexcept {
        if (!send_samples(chunk.data(), ChunkSize)) {
            finish_stream("peer closed connection");
            return;
        }
        if (++counter_ >= cfg_.max_stream_chunks) {
            finish_stream("max stream length reached");
        }
    }

    void on_cooldown() noexcept {
        if (++counter_ >= cfg_.cooldown_chunks) {
            counter_ = 0;
            state_ = SessionState::Idle;
        }
    }

public:
    WakeSession(Detector& detector, UnixSocketClient& client, const char* sock_path, SessionConfig cfg = {}) noexcept
        : detector_(detector), client_(client), sock_path_(sock_path), cfg_(cfg) {}

    void on_chunk(Chunk chunk) noexcept {
        switch (state_) {
            case SessionState::Idle:      on_idle(chunk);      break;
            case SessionState::Streaming: on_streaming(chunk); break;
            case SessionState::Cooldown:  on_cooldown();       break;
        }
    }

    [[nodiscard]] SessionState state() const noexcept { return state_; }
};

} // namespace perception
