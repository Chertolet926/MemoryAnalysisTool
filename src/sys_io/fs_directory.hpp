#pragma once

#include "sys_io/fs_error.hpp"
#include <expected>
#include <filesystem>
#include <concepts>
#include <string_view>
#include <memory>

namespace sys_io {
    using error = error_utils::error_with_context<fs_errc, fs_error_info>;

    namespace detail {
        [[nodiscard]] std::expected<void*, error> open_directory(const std::filesystem::path& path) noexcept;
        void close_directory(void* handle) noexcept;
        [[nodiscard]] const char* read_directory(void* handle) noexcept;
    }

    template <std::invocable<std::string_view> Callback>
    std::expected<void, error> enumerate_dir(const std::filesystem::path& path, Callback&& cb) noexcept {
        auto dir = detail::open_directory(path);
        if (!dir) [[unlikely]] return std::unexpected(dir.error());

        std::unique_ptr<void, decltype(&detail::close_directory)> guard(*dir, detail::close_directory);

        while (const char* name = detail::read_directory(guard.get())) {
            std::string_view sv{name};
            if (sv != "." && sv != "..") [[likely]] cb(sv);
        }

        return {};
    }
}