#pragma once

#include <boost/fusion/adapted/std_pair.hpp>
#include "models/kv_registry.hpp"
#include "base_scraper.hpp"
#include <vector>
#include <string>

namespace scrapers {
    namespace kv_rules {
        // Defines the structure: key is string, values are a vector of types
        inline x3::rule<struct val_id, Value> const val = "val";
        inline x3::rule<struct str_id, std::string> const str = "str";
        inline x3::rule<struct line_id, std::pair<std::string, std::vector<Value>>> const line = "line";

        // Helper: ensures we don't accidentally match prefixes of other tokens
        inline auto const eow = !x3::graph;

        // Matches any non-whitespace sequence
        inline auto const str_def = x3::lexeme[+(x3::graph - x3::char_(" \t\n\r"))];

        // Parses values: tries Hex, then Long int, then default String
        inline auto const val_def = (x3::lexeme["0x" >> x3::hex >> eow]) | x3::lexeme[x3::long_ >> eow] | str;

        inline auto const key = x3::lexeme[+(x3::graph - ':')];  // Key: everything before a colon
        inline auto const line_def = key >> ':' >> *val;         // Rule: Key followed by colon and any number of values
        inline auto const config = (line % x3::eol) >> *x3::eol; // Parser for multiple lines separated by newlines

        BOOST_SPIRIT_DEFINE(str, val, line);
    }

    /**
    * @struct KVScraper
    * @brief    A scraper specialized in parsing Key-Value (KV) configuration files.
    * @details  This struct implements the parsing logic for inputs where data is organized as "key: value1 value2...".
    *           The grammar defined within handles hexadecimal values (prefixed with 0x),
    *           long integers, and generic strings.
    */
    struct KVScraper : BaseScraper<KVScraper, KVRegistry> {
        static constexpr const char* name() { return "KV scraper"; }
        static auto grammar() { return kv_rules::config; }
    };
}