#pragma once

#include "scrapers/models/smap_entry.hpp"
#include "scrapers/base_scraper.hpp"
#include "maps_scraper.hpp"
#include "kv_scraper.hpp"
#include "sys_io/fs_reader.hpp"
#include <vector>

namespace scrapers {
    namespace smaps_rules {
        inline auto const smaps_entry = scrapers::maps_rules::line >> 
            x3::eol >> +(scrapers::kv_rules::line >> x3::eol);

        inline auto const config = +(smaps_entry | x3::omit[x3::eol]);
    }

    struct SmapsScraper : BaseScraper<SmapsScraper, std::vector<SmapEntry>> {
        static constexpr const char* name() { return "Smaps scraper"; }
        static auto grammar() { return smaps_rules::config; }

        template <typename CallBack> static std::expected<void, ErrorResult> scrape_stream(
            const std::filesystem::path& filepath, CallBack&& callback) noexcept{
            
            auto file_reader = sys_io::FileReader::open(filepath);
            if (!file_reader) {
                auto ctx = scrapers::scrapper_error_info{name(), filepath.string(), file_reader.error().ctx.offset};
                return FAIL_WITH(ErrEnum::file_not_found, ctx);
            }

            sys_io::BlockReader block_reader(std::move(*file_reader));
            
            while (true) {
                auto block_res = block_reader.read_next_block(&MapsScraper::is_vma_start);
                if (!block_res) {
                    auto ctx = scrapers::scrapper_error_info{name(), filepath.string(), block_res.error().ctx.offset};
                    return FAIL_WITH(ErrEnum::file_read_failed, ctx);
                }

                if (!*block_res) break; // EOF

                if (auto parse_result = scrape(**block_res, filepath.string())) {
                    callback(std::move(*parse_result));
                }
            }

            return {};
        }
    };
}