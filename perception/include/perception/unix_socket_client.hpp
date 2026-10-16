#pragma once

#include <cstddef>
#include <utility>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace perception {

class UnixSocketClient {
private:
    int fd_ = -1;

public:
    explicit UnixSocketClient() noexcept = default;

    ~UnixSocketClient() { close(); }

    UnixSocketClient(const UnixSocketClient&) = delete;
    UnixSocketClient& operator=(const UnixSocketClient&) = delete;

    UnixSocketClient(UnixSocketClient&& other) noexcept 
        : fd_(std::exchange(other.fd_, -1)) {}

    UnixSocketClient& operator=(UnixSocketClient&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = std::exchange(other.fd_, -1);
        }
        
        return *this;
    }

    [[nodiscard]] bool connect(const char* path) noexcept { 
        if (!path) return false;
        close();

        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;

        if (std::strlen(path) >= sizeof(addr.sun_path)) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        return true;
    }

    [[nodiscard]] bool send_all(const void* data, std::size_t len) noexcept {
        if (fd_ < 0 || !data) return false;

        const char* ptr = static_cast<const char*>(data);
        size_t total_sent = 0;

        while (total_sent < len) {
            ssize_t sent = ::send(fd_, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (sent == 0) {
                return false;
            }
            total_sent += static_cast<std::size_t>(sent);
        }
        return true;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }
};

} // namespace perception
