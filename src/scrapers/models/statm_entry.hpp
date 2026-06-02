#pragma once

#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include "utils/memory_types.hpp"

namespace scrapers {
    struct StatmEntry {
        Pages size;     /// <-- Total program size (pages)
        Pages resident; /// <-- Resident set size (pages)
        Pages shared;   /// <-- Shared pages (i.e., backed by a file)
        Pages text;     /// <-- Text (code) pages
        Pages lib;      /// <-- Library (unused in Linux 2.6+)
        Pages data;     /// <-- Data + stack pages
        Pages dt;       /// <-- Dirty pages (unused in Linux 2.6+)
    };
}

BOOST_FUSION_ADAPT_STRUCT(scrapers::StatmEntry, size, resident, shared, text, lib, data, dt)