#pragma once

#include "scrapers/models/map_entry.hpp"
#include "scrapers/base_scraper.hpp"
#include <vector>

namespace scrapers {
    namespace maps_rules {
        inline x3::uint_parser<uint64_t, 16> const hex64; // Parser for 64-bit hexadecimal addresses

        x3::rule<struct perms_id, Perms> const perms = "perms";
        x3::rule<struct line_id, MapEntry> const line = "line";

        // Converts a 4-character string (e.g., "rwxp") into a Perms object
        inline auto const perms_def = x3::lexeme[x3::repeat(4)[x3::char_("rwxps-")]][([](auto& ctx) {
            auto const& attr = x3::_attr(ctx);
            x3::_val(ctx) = Perms(attr[0], attr[1], attr[2], attr[3]);
        })];

        // Defines the structure of a single /proc/maps line
        inline auto const line_def = 
            hex64 >> x3::lit('-') >> hex64 >> perms >>
            hex64 >> x3::lexeme[+x3::graph] >> x3::long_ >> x3::lexeme[*x3::graph];

        // Root grammar: parse multiple lines, ignoring empty ones
        inline auto const config = *(line | x3::omit[+x3::eol]);

        BOOST_SPIRIT_DEFINE(perms, line);
    }

    /**
    * @class MapsScraper
    * @brief Scraper for parsing memory map information from /proc/[pid]/maps.
    */
    struct MapsScraper : BaseScraper<MapsScraper, std::vector<MapEntry>> {
        static constexpr const char* name() { return "Maps scraper"; }
        static auto grammar() { return maps_rules::config; }

        /**
        * @brief Checks if a specific string line matches the format of a valid VMA entry.
        * @param line   The string line to inspect.
        * @return       true if the line strictly follows the VMA format, false otherwise.
        */
        static bool is_vma_start(std::string_view line) noexcept {
            return validate_rule(line, maps_rules::line);
        }
    };
}