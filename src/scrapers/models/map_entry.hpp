#pragma once

#include <boost/fusion/include/adapt_struct.hpp>
#include <cstdint>
#include <string>

namespace scrapers {
    struct Perms {
        uint8_t r : 1, w : 1, x : 1, s : 1; // 4 bits packed into 1 byte
        
        constexpr Perms() noexcept : r(0), w(0), x(0), s(0) {}

        // Maps input chars ('r', 'w', 'x', 's') to the corresponding flag bits
        constexpr Perms(char r_in, char w_in, char x_in, char s_in) noexcept
            : r(r_in == 'r'), w(w_in == 'w'), x(x_in == 'x'), s(s_in == 's') {}

        // Permission flag accessors
        constexpr bool is_r() const noexcept { return r; }
        constexpr bool is_w() const noexcept { return w; }
        constexpr bool is_x() const noexcept { return x; }
        
        // Shared flag is false for private memory segments
        constexpr bool is_private() const noexcept { return !s; }  
    };

    struct MapEntry {
        uint64_t start;   /// <-- Starting virtual address of the mapping
        uint64_t end;     /// <-- Ending virtual address (exclusive)
        Perms perms;      /// <-- Parsed permissions bitfield
        uint64_t offset;  /// <-- Offset into the file or shared memory object
        std::string dev;  /// <-- Device identifier (major:minor)
        uint64_t inode;   /// <-- Inode number on the device
        std::string path; /// <-- Path to the mapped file, or special tags ([stack], [heap], etc.)
    };
}

BOOST_FUSION_ADAPT_STRUCT(scrapers::MapEntry, start, end, perms, offset, dev, inode, path)