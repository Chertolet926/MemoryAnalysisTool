#pragma once

#include "scrapers/statm_scraper.hpp"
#include "scrapers/kv_scraper.hpp"
#include "utils/memory_types.hpp"
#include <optional>
#include <string>

namespace process {

    class ProcfsReader {
    public:
        // Holds essential metadata extracted from /proc/[pid]/status
        struct StatusMeta {
            std::string name = "Unknown";   // <-- Executable name of the process.
            pid_t ppid = 0;                 // <-- Parent process identifier.
            size_t thread_count = 1;        // <-- Number of active threads in the process task group.
        };

        /**
        * @brief Extracts core process metadata (Name, PPid, Threads) from /proc/[pid]/status.
        * @param pid    The target process identifier.
        * @return       std::optional<StatusMeta> The parsed metadata, or std::nullopt if the file cannot be read.
        */
        [[nodiscard]] static std::optional<StatusMeta> fetch_status_meta(pid_t pid) noexcept {
            const StackPath status_path(pid, "/status"); // Stack-allocated path construction to prevent heap overhead
            auto res = scrapers::KVScraper::scrape_file(status_path.c_str());
            if (!res) [[unlikely]] return std::nullopt;

            // Extract fields safely with zero-copy/fallback defaults
            return StatusMeta{
                .name = res->get<std::string>("Name", 0, "Unknown"),
                .ppid = static_cast<pid_t>(res->get<long>("PPid", 0, 0)),
                .thread_count = static_cast<size_t>(res->get<long>("Threads", 0, 1))
            };
        }

        /**
        * @brief Retrieves memory utilization metrics from /proc/[pid]/statm.
        * @param pid    The target process identifier.
        * @return       std::optional<scrapers::StatmEntry> Parsed memory counters, or std::nullopt on failure.
        */
        [[nodiscard]] static std::optional<scrapers::StatmEntry> fetch_metrics(pid_t pid) noexcept {
            const StackPath statm_path(pid, "/statm"); // Stack-allocated path construction
            auto res = scrapers::StatmScraper::scrape_file(statm_path.c_str());
            return res ? std::make_optional(*res) : std::nullopt; // Avoid explicit conditional branches where possible
        }
    };
}