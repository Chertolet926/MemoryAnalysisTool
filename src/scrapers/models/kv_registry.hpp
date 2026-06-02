#pragma once

#include <map>
#include <variant>
#include <vector>
#include <string>
#include <string_view>

namespace scrapers {
    using Value = std::variant<long, std::string>;
    using MapType = std::map<std::string, std::vector<Value>, std::less<>>;

    /**
     * @class KVRegistry
     * @brief Container for storing and accessing data.
     */
    struct KVRegistry : MapType {
        // Retrieves a value by key and index. Returns fallback if lookup fails
        // or the stored type does not match the requested type T.
        template <typename T> 
        T get(std::string_view key, size_t idx = 0, T fallback = T{}) const noexcept {
            auto it = this->find(key); // Find the key within the map
            
            // Verify the key exists and the index is within the value vector bounds and
            // attempt to safely extract the value as type
            if (it != this->end() && idx < it->second.size())
                if (auto* p = std::get_if<T>(&it->second[idx]))
                    return *p;

            return fallback; // Return the provided fallback if the lookup or type cast fails
        }
    };
}