#pragma once

#include "scrapers/smaps_scraper.hpp"
#include "scrapers/maps_scraper.hpp"
#include "utils/memory_types.hpp"
#include <expected>
#include <vector>

namespace process {
    /**
    * @brief Parses high-level memory region layouts from /proc/[pid]/maps.
    * @param pid    The target process identifier.
    * @return       A vector of basic memory map entries, or a scrapers::ErrorResult on failure.
    */
    [[nodiscard]] inline std::expected<std::vector<scrapers::MapEntry>, scrapers::ErrorResult> 
    inspect_maps(pid_t pid) noexcept {
        // Stack-allocated path construction to avoid heap overhead
        const StackPath maps_path(pid, "/maps");
        return scrapers::MapsScraper::scrape_file(maps_path.c_str());
    }

    /**
    * @brief Streams detailed proportional and physical memory metrics from /proc/[pid]/smaps.
    * @tparam       Callback Invocable type matching the stream-receiver signature.
    * @param pid    The target process identifier.
    * @param cb     Callback invoked sequentially per parsed block to avoid massive bulk heap allocations.
    * @return       An expected void state signaling completion, or a scrapers::ErrorResult if reading fails.
    */
    template <typename Callback>
    [[nodiscard]] inline std::expected<void, scrapers::ErrorResult> 
    inspect_smaps_stream(pid_t pid, Callback&& cb) noexcept {
        const StackPath smaps_path(pid, "/smaps"); // Stack-allocated path for streaming engine target
        
        // Forward the callback transparently to preserve reference semantics (lvalue/rvalue)
        return scrapers::SmapsScraper::scrape_stream(smaps_path.c_str(), std::forward<Callback>(cb));
    }
}