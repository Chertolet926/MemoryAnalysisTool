#pragma once

#include "scrapers/models/statm_entry.hpp"
#include "scrapers/base_scraper.hpp"

namespace scrapers {
    namespace statm_rules {
        // Grammar for /proc/[pid]/statm with trailing whitespace/newline consumption
        inline auto config = 
            x3::uint64 >> x3::uint64 >> x3::uint64 >>
            x3::uint64 >> x3::uint64 >> x3::uint64 >> x3::uint64 >> x3::omit[*x3::space];
    }

    /*
    * @class StatmScraper
    * @brief Scraper responsible for parsing the /proc/[pid]/statm file format.
    */
    struct StatmScraper : BaseScraper<StatmScraper, StatmEntry> {
        static constexpr const char* name() { return "Statm scraper"; }
        static auto grammar() { return statm_rules::config; }
    };
}