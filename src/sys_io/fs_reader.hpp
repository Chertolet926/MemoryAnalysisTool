#pragma once

#include "sys_io/fs_error.hpp"
#include "sys_io/fs_fd.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <memory>

namespace sys_io {
    using error = error_utils::error_with_context<fs_errc, fs_error_info>;
    using BlockBoundaryPredicate = std::move_only_function<bool(std::string_view) const>;

    class FileReader {
        public:
            FileReader() noexcept = default;

            [[nodiscard]] static std::expected<FileReader, error> open(const std::filesystem::path& path) noexcept;

            [[nodiscard]] std::expected<std::string, error> read_all() noexcept;
            [[nodiscard]] std::expected<bool, error> read_line(std::string& out) noexcept;

        private:
            explicit FileReader(UniqueFd fd, std::string path) noexcept;

            [[nodiscard]] ssize_t read_next_chunk(char* dest) noexcept;
            [[nodiscard]] std::expected<bool, error> populate_buffer() noexcept;

            static constexpr size_t CHUNK_SIZE = 8192;

            UniqueFd fd_;
            std::string path_;
            std::unique_ptr<char[]> chunk_buffer_;
            size_t buffer_offset_ = 0;
            size_t buffer_bytes_ = 0;
    };

    class BlockReader {
        public:
            explicit BlockReader(FileReader reader) noexcept;

            [[nodiscard]] std::expected<std::optional<std::string>, error> read_next_block(
                const BlockBoundaryPredicate& is_start_of_block) noexcept;

        private:
            FileReader reader_;
            std::string lookahead_line_;
            bool has_lookahead_ = false;
            bool eof_reached_ = false;
    };
}