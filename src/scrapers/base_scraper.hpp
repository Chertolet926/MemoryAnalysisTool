#pragma once

#include "scrapers/scrapper_errors.hpp"
#include <boost/spirit/home/x3.hpp>
#include "utils/error_utils.hpp"
#include "sys_io/fs_reader.hpp"
#include <boost/filesystem.hpp>
#include <spdlog/spdlog.h>
#include <string_view>
#include <iterator>
#include <expected>
#include <utility>

namespace scrapers {
    namespace x3 = boost::spirit::x3;

    using ErrEnum = scrapers::scrapper_errc;
    using ErrorResult = error_utils::error_with_context<ErrEnum, scrapers::scrapper_error_info>;

    /**
    * @brief A base class providing standardized parsing and validation logic.
    * @tparam ScrapeConfig A configuration struct providing the parser grammar and name.
    * @tparam T The type of the object to be parsed.
    */
    template<typename ScrapeConfig, typename T> class BaseScraper {
        public:
            /**
            * @brief Parses a string into type T using the provided grammar.
            * @param s              The input string to parse.
            * @param source_name    Identifier for the source (e.g., file path or label).
            * @return               A std::expected containing the result or an ErrorResult on failure.
            */
            /**
             * @brief Parses a string into type T using the provided grammar.
             * @param s              The input string to parse.
             * @param source_name    Identifier for the source (e.g., file path or label).
             * @return               A std::expected containing the result or an ErrorResult on failure.
             */
            static std::expected<T, ErrorResult> scrape(std::string_view s, std::string source_name = "memory_buffer") noexcept {
                auto it = s.begin();

                if (T result{}; x3::phrase_parse(it, s.end(), ScrapeConfig::grammar(), x3::blank, result))
                    if (it == s.end()) return result;

                return FAIL_WITH(ErrEnum::invalid_syntax, scrapers::scrapper_error_info{
                    std::string(ScrapeConfig::name()),
                    std::move(source_name),
                    static_cast<size_t>(std::distance(s.begin(), it))
                });
            }

            /**
            * @brief Validates a string against a specific Spirit X3 rule.
            * @param s              The input string to validate.
            * @param rule           The X3 rule/grammar to apply.
            * @return               True if the input matches the rule entirely.
            */
            template<typename Rule> static bool validate_rule(std::string_view s, Rule&& rule) noexcept {
                auto it = s.begin();
                return x3::phrase_parse(it, s.end(), rule, x3::blank) && it == s.end();
            }

            /** @brief Validates a string against the default class grammar. */
            static bool validate(std::string_view s) noexcept {
                return validate_rule(s, ScrapeConfig::grammar());
            }

            /**
             * @brief Reads a file and attempts to parse its content.
             * @param filepath      Path to the file.
             * @return              A std::expected containing the result or an ErrorResult on failure.
             */
            static std::expected<T, ErrorResult> scrape_file(const std::filesystem::path& filepath) noexcept {             
                auto file_reader = sys_io::FileReader::open(filepath);
                if (!file_reader) return FAIL_WITH(ErrEnum::file_not_found, scrapers::scrapper_error_info{
                    ScrapeConfig::name(), filepath.string(), file_reader.error().ctx.offset});

                auto content_res = file_reader->read_all();
                if (!content_res) return FAIL_WITH(ErrEnum::file_read_failed, scrapers::scrapper_error_info{
                    ScrapeConfig::name(), filepath.string(), content_res.error().ctx.offset});
                
                return scrape(*content_res, filepath.string());
            }
    };
}