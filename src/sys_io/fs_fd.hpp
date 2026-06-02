#pragma once

#include <utility>
#include <unistd.h>

namespace sys_io {
    class UniqueFd {
        private:
            int fd_ = -1;

        public:
            UniqueFd() noexcept = default;
            explicit UniqueFd(int fd) noexcept : fd_(fd) {}
            ~UniqueFd() noexcept { reset(); }

            UniqueFd(const UniqueFd&) = delete;
            UniqueFd& operator=(const UniqueFd&) = delete;

            UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
            UniqueFd& operator=(UniqueFd&& other) noexcept {
                if (this != &other) reset(std::exchange(other.fd_, -1));
                return *this;
            }

            [[nodiscard]] int get() const noexcept { return fd_; }
            [[nodiscard]] explicit operator bool() const noexcept { return fd_ != -1; }

            void reset(int fd = -1) noexcept {
                if (fd_ != -1) ::close(fd_);
                fd_ = fd;
            }
    };
}