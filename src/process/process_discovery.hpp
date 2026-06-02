#pragma once

#include "sys_io/fs_error.hpp"
#include "scrapers/maps_scraper.hpp"
#include "scrapers/smaps_scraper.hpp"
#include <sys/types.h>
#include <string>
#include <vector>
#include <expected>
#include <atomic>
#include <array>
#include <string_view>
#include <utility>

namespace process {

    // ==============================================================================
    // Безаллокационный стековый конструктор путей для Procfs
    // ==============================================================================
    template <size_t Size = 64>
    class StackPath {
    public:
        StackPath(pid_t pid, std::string_view suffix = "") noexcept {
            // Форматируем прямо в стековый массив char без аллокаций в куче
            auto result = std::format_to_n(buffer_.data(), buffer_.size() - 1, "/proc/{}{}", pid, suffix);
            *result.out = '\0';
            size_ = result.size;
        }

        [[nodiscard]] const char* c_str() const noexcept { return buffer_.data(); }
        [[nodiscard]] std::string_view view() const noexcept { return {buffer_.data(), size_}; }

    private:
        std::array<char, Size> buffer_{};
        size_t size_ = 0;
    };

    // ==============================================================================
    // Сущности предметной области
    // ==============================================================================

    struct MemoryMetrics {
        uint64_t vm_size_bytes  = 0;
        uint64_t rss_bytes      = 0;
        uint64_t shared_bytes   = 0;
        uint64_t text_bytes     = 0;
        uint64_t data_bytes     = 0;

        auto operator<=>(const MemoryMetrics&) const = default;
    };

    struct ProcessInfo {
        pid_t pid           = 0;
        pid_t ppid          = 0;
        size_t thread_count = 1;
        std::string name;
        MemoryMetrics memory;
    };

    enum class ProcessEventKind { Created, Updated, Terminated };

    struct ProcessEvent {
        ProcessEventKind kind;
        ProcessInfo info;
    };

    struct FilterConfig {
        bool exclude_kernel_threads = true;
        bool exclude_zombies        = true;
        bool exclude_self           = true;
        bool only_current_user      = false;

        auto operator<=>(const FilterConfig&) const = default;
    };

    class ProcessFilter {
    public:
        explicit ProcessFilter(FilterConfig config = {}) noexcept;

        void set_config(FilterConfig config) noexcept;
        [[nodiscard]] FilterConfig get_config() const noexcept;

        [[nodiscard]] bool should_track(pid_t pid) const noexcept;

    private:
        std::atomic<FilterConfig> config_;
        pid_t self_pid_;
        uid_t owner_uid_;
    };

    using ScanResult = std::expected<std::vector<pid_t>, sys_io::error>;

    [[nodiscard]] ScanResult scan_pids(const ProcessFilter& filter = ProcessFilter{}) noexcept;
}