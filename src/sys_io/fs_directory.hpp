#pragma once

#include "sys_io/fs_error.hpp"
#include <expected>
#include <filesystem>
#include <concepts>
#include <string_view>
#include <memory>

namespace sys_io {
    // High-level directory error descriptor carrying system context.
    using error = error_utils::error_with_context<fs_errc, fs_error_info>;

    namespace detail {
        [[nodiscard]] std::expected<void*, error> open_directory(const std::filesystem::path& path) noexcept;
        void close_directory(void* handle) noexcept;
        [[nodiscard]] const char* read_directory(void* handle) noexcept;
    }

    /**
    * @brief Performs a high-performance, non-allocating linear sweep of a filesystem directory.
    *        Skips standard POSIX navigation loops ("." and "..").
    *
    * @tparam       Callback An invocable target that accepts a standalone std::string_view parameter.
    * @param path   The direct path targeting the directory to scan.
    * @param cb     Invocable target triggered sequentially for every individual directory item name.
    * @return       A standard expected container wrapping success (void) or a descriptive error payload.
    */
    template <std::invocable<std::string_view> Callback>
    std::expected<void, error> enumerate_dir(const std::filesystem::path& path, Callback&& cb) noexcept {
        // Attempt to capture the native underlying directory handle
        auto dir = detail::open_directory(path);
        if (!dir) [[unlikely]] return std::unexpected(dir.error());

        // RAII scope guard: safely manages life-cycle cleanup of the void* descriptor via custom deleter
        std::unique_ptr<void, decltype(&detail::close_directory)> guard(*dir, detail::close_directory);

        // Progressively read available filenames sequentially
        while (const char* name = detail::read_directory(guard.get())) {
            std::string_view sv{name};
            
            // Bypass localized parent and current directory linkages
            if (sv != "." && sv != "..") [[likely]] cb(sv);
        }

        return {};
    }
}