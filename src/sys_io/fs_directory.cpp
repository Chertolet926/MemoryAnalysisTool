#include "fs_directory.hpp"
#include <cerrno>
#include <cstring>
#include <dirent.h>

namespace sys_io::detail {
    /**
    * @brief Opens a native POSIX directory stream.
    * @param path   The filesystem path targeting the directory.
    * @return       A void pointer hiding the underlying DIR* handle, 
    *               or a structured error wrapper on failure.
    */
    std::expected<void*, error> open_directory(const std::filesystem::path& path) noexcept {
        // Attempt to open the directory stream using POSIX API
        if (DIR* dir = ::opendir(path.c_str())) [[likely]] return dir;

        // Construct a descriptive error using the current thread-local errno
        return error_utils::make_err(static_cast<fs_errc>(errno),
            sys_io::fs_error_info{path.string(), std::strerror(errno), 0});
    }

    /**
    * @brief Closes the provided native directory stream handle.
    * @param handle     Pointer to the active native DIR stream. Safe if handle is nullptr.
    */
    void close_directory(void* handle) noexcept {
        if (handle) ::closedir(static_cast<DIR*>(handle));
    }

    /**
    * @brief Iterates and reads the next entry name from the directory handle.
    * @param handle     Pointer to the active native DIR stream.
    * @return           Pointer to a null-terminated string containing the filename, 
    *                   or nullptr if the end of the stream is reached or handle is invalid.
    */
    const char* read_directory(void* handle) noexcept {
        if (!handle) [[unlikely]] return nullptr;

        // Fetch the next directory entry sequence
        struct dirent* entry = ::readdir(static_cast<DIR*>(handle));
        return entry ? entry->d_name : nullptr;
    }
}