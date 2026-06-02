#include "fs_directory.hpp"
#include <cerrno>
#include <cstring>
#include <dirent.h>

namespace sys_io::detail {
    std::expected<void*, error> open_directory(const std::filesystem::path& path) noexcept {
        if (DIR* dir = ::opendir(path.c_str())) [[likely]] return dir;
        return error_utils::make_err(static_cast<fs_errc>(errno),
            sys_io::fs_error_info{path.string(), std::strerror(errno), 0});
    }

    void close_directory(void* handle) noexcept {
        if (handle) ::closedir(static_cast<DIR*>(handle));
    }

    const char* read_directory(void* handle) noexcept {
        if (!handle) [[unlikely]] return nullptr;

        struct dirent* entry = ::readdir(static_cast<DIR*>(handle));
        return entry ? entry->d_name : nullptr;
    }
}