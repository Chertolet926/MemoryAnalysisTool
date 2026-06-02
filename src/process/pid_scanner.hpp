#pragma once

#include "process/process_filter.hpp"
#include "sys_io/fs_directory.hpp"
#include <string_view>
#include <sys/types.h>
#include <charconv>
#include <expected>
#include <vector>

namespace process {
    // Result type containing either a vector of discovered PIDs or a system I/O error.
    using ScanResult = std::expected<std::vector<pid_t>, sys_io::error>;

    /**
    * @brief Scans the Linux /proc filesystem to discover active process identifiers.
    * @param filter     Predicate logic used to determine if a discovered PID should be tracked.
    * @return           ScanResult A vector of matching PIDs, or a sys_io::error if directory traversal fails.
    */
    [[nodiscard]] inline ScanResult scan_pids(const ProcessFilter& filter = ProcessFilter{}) noexcept {
        // Pre-allocate memory to mitigate heap churn during rapid process discovery
        std::vector<pid_t> pids; pids.reserve(1024);

        // Perform zero-allocation directory enumeration
        auto res = sys_io::enumerate_dir("/proc", [&pids, &filter](std::string_view entry_name) noexcept {
            if (entry_name.empty()) return;

            // High-performance, non-throwing string-to-integer parsing
            pid_t pid = 0;
            const auto [ptr, ec] = std::from_chars(
                entry_name.data(), entry_name.data() + entry_name.size(), pid
            );

            // Confirm the directory name is completely numeric and matches tracking rules
            if (ec == std::errc{} && ptr == entry_name.data() + entry_name.size())
                if (filter.should_track(pid)) pids.push_back(pid);
        });

        // Forward internal file system errors directly upstream
        if (!res) [[unlikely]] return std::unexpected(res.error());
        return pids;
    }
}