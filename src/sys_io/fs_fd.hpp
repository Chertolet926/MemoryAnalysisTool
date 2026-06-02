#pragma once

#include <utility>
#include <unistd.h>

namespace sys_io {
    class UniqueFd {
        private:
            int fd_ = -1; // <-- The underlying raw POSIX file descriptor.

        public:
            // @brief Constructs an empty, invalid UniqueFd instance.
            UniqueFd() noexcept = default;

            // @brief Constructs a UniqueFd wrapper by taking exclusive ownership of a raw descriptor.
            explicit UniqueFd(int fd) noexcept : fd_(fd) {}

            // @brief Destructor. Closes the managed file descriptor if it is valid.
            ~UniqueFd() noexcept { reset(); }

            // Disable copy semantics to guarantee exclusive resource ownership
            UniqueFd(const UniqueFd&) = delete;
            UniqueFd& operator=(const UniqueFd&) = delete;

            // @brief Move constructor. Transfers descriptor ownership from another instance.
            UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

            // @brief Move assignment operator. Releases the current descriptor and assumes ownership of the new one.
            UniqueFd& operator=(UniqueFd&& other) noexcept {
                if (this != &other) reset(std::exchange(other.fd_, -1));
                return *this;
            }

            // @brief Retrieves the underlying raw file descriptor.
            [[nodiscard]] int get() const noexcept { return fd_; }

            // @brief Boolean operator to check if the wrapper holds a valid descriptor.
            [[nodiscard]] explicit operator bool() const noexcept { return fd_ != -1; }

            // @brief Closes the currently managed descriptor (if active) and replaces it with a new one.
            void reset(int fd = -1) noexcept {
                if (fd_ != -1) ::close(fd_); // Trigger native POSIX cleanup
                fd_ = fd;
            }
    };
}