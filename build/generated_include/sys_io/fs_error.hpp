#pragma once

#include "utils/error_utils.hpp"
#include <string>
#include <cerrno>

namespace sys_io {
    enum class fs_errc {
        success = 0,
        not_found = ENOTDIR,
        denied = EACCES,
        limit_reached = EMFILE,
        read_failed = EIO,
        over_limit = EFBIG,
        invalid =  EINVAL
    };

    struct fs_error_info {
        std::string path;
        std::string details;
        size_t offset = 0;
    };

    DEFINE_ERROR_CATEGORY(fs_errc, "System IO", {
        switch (e) {
            case sys_io::fs_errc::not_found:     return "Not found";
            case sys_io::fs_errc::denied:        return "Access denied";
            case sys_io::fs_errc::limit_reached: return "Descriptor limit reached";
            case sys_io::fs_errc::read_failed:   return "Read failure";
            case sys_io::fs_errc::over_limit:    return "Size limit exceeded";
            case sys_io::fs_errc::invalid:       return "Invalid file context";
            default:                             return "Unknown IO error";
        }
    })
}

REGISTER_BOOST_ERROR(sys_io::fs_errc)
REGISTER_ERROR_FORMATTER(sys_io::fs_errc, sys_io::fs_error_info, R"()",
    sys_io::get_error_message(err),
    boost::system::system_category().message(static_cast<int>(err)),
    ctx.path, ctx.offset
)
