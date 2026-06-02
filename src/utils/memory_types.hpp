#pragma once

#include <cstdint>
#include <unistd.h>
#include <array>
#include <string_view>
#include <format>

struct Pages {
    uint64_t count = 0; // <-- Raw number of pages

    // @brief Default constructor initializing page count.
    Pages(uint64_t val = 0) noexcept : count(val) {}

    // @brief Assignment operator from a raw 64-bit unsigned integer.
    Pages& operator=(uint64_t val) noexcept {
        count = val;
        return *this;
    }

    // @brief Converts the internal page count into bytes.
    uint64_t bytes() const noexcept {
        // Thread-safe initialization: cache system page size once, fallback to 4KB if query fails
        static const uint64_t page_size = []() noexcept {
            const long sz = ::sysconf(_SC_PAGESIZE);
            return sz > 0 ? static_cast<uint64_t>(sz) : 4096ULL;
        }();

        return count * page_size;
    }

    // @brief Implicit conversion operator to treat Pages as a raw uint64_t.
    operator uint64_t() const noexcept { return count; }
};

template <size_t Size = 64> class StackPath {
    public:
        /**
        * @brief Constructs and formats a path matching: /proc/{pid}{suffix}
        * @param pid        Target Process ID.
        * @param suffix     Optional file/directory appended to the pid (e.g., "/statm").
        */
        StackPath(pid_t pid, std::string_view suffix = "") noexcept {
            // Perform compile-time bounds-checked formatting directly into the stack array
            auto result = std::format_to_n(buffer_.data(), buffer_.size() - 1, "/proc/{}{}", pid, suffix);
            *result.out = '\0'; // Explicitly guarantee null-termination for C-style system APIs
            size_ = result.size;
        }

        // @brief Returns a pointer to a null-terminated C-string representation.
        [[nodiscard]] const char* c_str() const noexcept { return buffer_.data(); }

        // @brief Returns a lightweight, non-owning string_view projection of the formatted path.
        [[nodiscard]] std::string_view view() const noexcept { return {buffer_.data(), size_}; }

    private:
        std::array<char, Size> buffer_{};   // <-- Contiguous stack memory storage
        size_t size_ = 0;                   // <-- Exact length of the formatted string excluding null-terminator
};