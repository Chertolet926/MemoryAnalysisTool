#pragma once

#include <cstdint>
#include <unistd.h>

struct Pages {
    uint64_t count = 0;

    Pages(uint64_t val = 0) noexcept : count(val) {}

    Pages& operator=(uint64_t val) noexcept {
        count = val;
        return *this;
    }

    uint64_t bytes() const noexcept {
        static const uint64_t page_size = []() noexcept {
            const long sz = ::sysconf(_SC_PAGESIZE);
            return sz > 0 ? static_cast<uint64_t>(sz) : 4096ULL;
        }();
        return count * page_size;
    }

    operator uint64_t() const noexcept { return count; }
};
