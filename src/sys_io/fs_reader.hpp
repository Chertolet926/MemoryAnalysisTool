#pragma once

#include "sys_io/fs_error.hpp"
#include "sys_io/fs_fd.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <memory>

namespace sys_io {
    // High-level directory error descriptor carrying system context.
    using error = error_utils::error_with_context<fs_errc, fs_error_info>;

    // Functional predicate wrapper to detect structural block boundaries.
    using BlockBoundaryPredicate = std::move_only_function<bool(std::string_view) const>;

    class FileReader {
        public:
            // @brief Constructs an uninitialized, empty FileReader instance.
            FileReader() noexcept = default;

            /**
            * @brief Securely opens a file for sequential reading.
            * @param path   The filesystem path targeting the file.
            * @return       A valid active FileReader instance, or a descriptive I/O error wrapper.
            */
            [[nodiscard]] static std::expected<FileReader, error> open(const std::filesystem::path& path) noexcept;

            /**
            * @brief Slurps the entire remaining file contents into a standard string.
            * @note         Features an internal 20MB safety limit to prevent memory exhaustion.
            * @return       The complete file data as a string, or an error payload.
            */
            [[nodiscard]] std::expected<std::string, error> read_all() noexcept;

            /**
            * @brief Extracts a single newline-terminated string from the stream.
            *        Amortizes physical system reads via internal chunk caching.
            *
            * @param out    Reference to a target string populated with the extracted line (cleared on entry).
            * @return       True if a line was successfully retrieved, false if EOF is reached, or an error.
            */
            [[nodiscard]] std::expected<bool, error> read_line(std::string& out) noexcept;

        private:
            // Private constructor invoked exclusively by the static open() factory
            explicit FileReader(UniqueFd fd, std::string path) noexcept;

            // Directly invokes native POSIX read() wrapped inside an EINTR retry loop
            [[nodiscard]] ssize_t read_next_chunk(char* dest) noexcept;

            // Synchronously reloads the internal memory cache from the disk stream
            [[nodiscard]] std::expected<bool, error> populate_buffer() noexcept;

            static constexpr size_t CHUNK_SIZE = 8192; // Optimized 8KB hardware-friendly block capacity

            UniqueFd fd_;                           // <-- RAII file descriptor handle
            std::string path_;                      // <-- Absolute or relative path reference for logging
            std::unique_ptr<char[]> chunk_buffer_;  // <-- Contiguous block cache to buffer raw disk records
            size_t buffer_offset_ = 0;              // <-- Current linear consumption index inside the chunk
            size_t buffer_bytes_ = 0;               // <-- Total valid data bytes available in the active chunk
    };

    class BlockReader {
        public:
            // @brief Constructs a BlockReader by taking absolute ownership of a FileReader engine.
            explicit BlockReader(FileReader reader) noexcept;

            /**
            * @brief Assembles and returns the next contiguous multi-line text block.
            * @param is_start_of_block  Callback logic establishing the starting marker of a new sequence block.
            * @return                   An optional containing the aggregated block string, std::nullopt on EOF, or a reading error.
            */
            [[nodiscard]] std::expected<std::optional<std::string>, error> read_next_block(
                const BlockBoundaryPredicate& is_start_of_block) noexcept;

        private:
            FileReader reader_;             // <-- Owned underlying file stream worker
            std::string lookahead_line_;    // <-- Temporary register holding the opening line of the *next* block
            bool has_lookahead_ = false;    // <-- State flag indicating the lookahead cache contains structural data
            bool eof_reached_ = false;      // <-- Circuit breaker tripped once the stream runs completely dry
    };
}