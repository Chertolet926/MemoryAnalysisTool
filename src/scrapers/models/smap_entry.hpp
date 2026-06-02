#pragma once

#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include "kv_registry.hpp"
#include "map_entry.hpp"

namespace scrapers {
    struct SmapEntry {
        MapEntry header;    // <-- The first line (addresses, perms, path)
        KVRegistry stats;   // <-- The following block of key-value metrics (Size, Rss, etc.)
    };
}

BOOST_FUSION_ADAPT_STRUCT(scrapers::SmapEntry, header, stats)