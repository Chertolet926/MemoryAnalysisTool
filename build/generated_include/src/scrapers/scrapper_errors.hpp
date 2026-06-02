#pragma once

#include "utils/error_utils.hpp"
#include <spdlog/spdlog.h>

namespace scrapers {
    // Enum representing all possible error types in the scraping system
    enum class scrapper_errc {
        success = 0,        // <-- Success (no error)
        invalid_syntax,     // <-- Syntax error in input data
        file_not_found,     // <-- File not found or open error
        file_read_failed,   // <-- File read error
        file_too_large      // <-- File too large error
    };

    // Structure containing detailed metadata about the error location
    struct scrapper_error_info {
        std::string scrapper_name;       // <-- Name of scrapper where error ocured
        std::string file_path;           // <-- Path to file where error ocured
        size_t offset;                   // <-- Offset in source text
    };

    // Registers the enum with the Boost error system and defines custom error message mapping.
    // The error message template is defined in CMake and can include placeholders for dynamic content.
    DEFINE_ERROR_CATEGORY(scrapper_errc, "Scrappers", {
        switch (e) {
            case scrapers::scrapper_errc::invalid_syntax:   return "Invalid syntax";
            case scrapers::scrapper_errc::file_not_found:   return "File not found or failed to open";
            case scrapers::scrapper_errc::file_read_failed: return "Failed to read file";
            case scrapers::scrapper_errc::file_too_large:   return "File too large";
            default: return "Unknown scrapper error";
        }
    })
}
// Allows the enum to be used with boost::system::error_code
REGISTER_BOOST_ERROR(scrapers::scrapper_errc)

// Registers a custom formatter to display structured error info in logs.
// The template is defined in CMake and can include placeholders for dynamic content.
REGISTER_ERROR_FORMATTER(scrapers::scrapper_errc, scrapers::scrapper_error_info, R"(├── Error Message: {}
├── Scraper:       {}
├── Source file:   {}
└── Position:      {})",
    scrapers::get_error_message(err), ctx.scrapper_name,
    ctx.file_path, ctx.offset
)
