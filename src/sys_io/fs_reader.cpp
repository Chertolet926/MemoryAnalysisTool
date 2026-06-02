#include "fs_reader.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <concepts>

namespace sys_io {
    // Helper constraint-validated wrapper to automatically resume syscalls interrupted by signals
    template <std::invocable Func>
    requires std::integral<std::invoke_result_t<Func>>
    static inline auto eintr_retry(Func&& func) noexcept {
        while (true) if (auto res = func(); res != -1 || errno != EINTR) return res;
    }

    FileReader::FileReader(UniqueFd fd, std::string path) noexcept
        : fd_(std::move(fd)), path_(std::move(path)), chunk_buffer_(std::make_unique<char[]>(CHUNK_SIZE)) {}

    /**
    * @brief Securely opens a file descriptor for sequential read-only access.
    *        Prevents symlink traversal attacks via O_NOFOLLOW.
    *
    * @param path   The direct target filesystem path.
    * @return       A valid FileReader instance or a structured file system error context.
    */
    std::expected<FileReader, error> FileReader::open(const std::filesystem::path& path) noexcept {
        int raw_fd = eintr_retry([&]() noexcept {
            // Secure flags: close on exec to prevent leaks, drop symlinks via NOFOLLOW
            return ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        });

        if (raw_fd < 0) return FAIL_WITH(static_cast<fs_errc>(errno), 
            sys_io::fs_error_info{path.string(), "", 0});

        return FileReader(UniqueFd(raw_fd), path.string());
    }

    // @brief Low-level method to read the next unparsed raw segment from the file descriptor.
    ssize_t FileReader::read_next_chunk(char* dest) noexcept {
        return eintr_retry([&]() noexcept {
            return ::read(fd_.get(), dest, CHUNK_SIZE);
        });
    }

    // @brief Refills the internal underlying chunk cache from the file stream.
    std::expected<bool, error> FileReader::populate_buffer() noexcept {
        ssize_t bytes = read_next_chunk(chunk_buffer_.get());

        if (bytes < 0) return FAIL_WITH(static_cast<fs_errc>(errno), 
            sys_io::fs_error_info{path_, std::strerror(errno), 0});

        buffer_bytes_ = static_cast<size_t>(bytes);
        buffer_offset_ = 0;
        return bytes > 0; // Returns true if bytes were successfully cached
    }

    /**
    * @brief Slurps the entire remaining contents of the file stream into memory.
    * Includes a 20MB guardrail to defend against unbound memory exhaustion.
    */
    std::expected<std::string, error> FileReader::read_all() noexcept {
        std::string out;
        out.reserve(CHUNK_SIZE);

        while (true) {
            ssize_t bytes = read_next_chunk(chunk_buffer_.get());
            if (bytes == 0) break; // Clean EOF encountered

            if (bytes < 0) return FAIL_WITH(static_cast<fs_errc>(errno), 
                sys_io::fs_error_info{path_, std::strerror(errno), 0});

            // Memory exhaustion guard-rail (Max 20MB)
            if (bytes + out.size() > 20 * 1024 * 1024) return FAIL_WITH(fs_errc::over_limit,
                  sys_io::fs_error_info{path_, "size limit exceeded", out.size()});

            out.append(chunk_buffer_.get(), bytes);
        }
        return out;
    }

    /**
    * @brief Extracts a single newline-terminated string from the internal streaming buffer.
    * Amortizes physical reads using chunks.
    * @param out Destination string to populate. Cleared automatically upon entry.
    * @return True if a line was read, false on EOF, or a trailing stream error.
    */
    std::expected<bool, error> FileReader::read_line(std::string& out) noexcept {
        out.clear();
        bool has_chars = false;

        while (true) {
            // Buffer empty or exhausted: request data reload from disk
            if (buffer_offset_ >= buffer_bytes_) {
                auto refilled = populate_buffer();
                if (!refilled) [[unlikely]] return std::unexpected(refilled.error());
                if (!*refilled) return has_chars; // EOF condition met
            }

            const char* start = chunk_buffer_.get() + buffer_offset_;
            const char* end = chunk_buffer_.get() + buffer_bytes_;
            const char* newline = std::find(start, end, '\n');

            // Calculate direct size span up to the delimiter or block margin
            size_t len = (newline != end) ? (newline - start + 1) : (end - start);
            out.append(start, len);
            buffer_offset_ += len;

            if (newline != end) return true; // Complete line found
            has_chars = true; // Found characters, continue searching across block boundary
        }
    }

    BlockReader::BlockReader(FileReader reader) noexcept
        : reader_(std::move(reader)) {}

    /**
    * @brief Parses and extracts a logical multiline record block based on a demarcation rule.
    *        Uses a single-line lookahead strategy to parse variable-sized structural blocks safely.
    *
    * @param is_start_of_block  User predicate to identify the opening boundary of a new block sequence.
    * @return                   An optional block string, std::nullopt when execution stream completes, or parsing error.
    */
    std::expected<std::optional<std::string>, error> BlockReader::read_next_block(
        const BlockBoundaryPredicate& is_start_of_block) noexcept {
        if (eof_reached_) return std::nullopt;

        std::string block;

        // Step 1: Drain lookahead line retained from the previous iteration loop
        if (has_lookahead_) {
            block = std::move(lookahead_line_);
            has_lookahead_ = false;
        }

        std::string line;
        while (true) {
            auto line_result = reader_.read_line(line);
            if (!line_result) [[unlikely]] return std::unexpected(line_result.error());

            // Handle end-of-file termination boundaries safely
            if (!*line_result) {
                eof_reached_ = true;
                return block.empty() ? std::nullopt : std::make_optional(std::move(block));
            }

            // Step 2: Check if this line signals a transition into a new block structure
            if (is_start_of_block(line) && !block.empty()) {
                lookahead_line_ = std::move(line); // Stash line for the next block evaluation loop
                has_lookahead_ = true;
                return std::move(block); // Return current collected block
            }

            // Accumulate lines inside the currently active block
            if (block.empty()) block = std::move(line);
            else               block += line;
        }
    }
}
