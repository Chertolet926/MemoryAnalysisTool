#pragma once

#include <boost/system/error_code.hpp>
#include <spdlog/fmt/fmt.h>
#include <expected>
#include <utility>

namespace error_utils {
    // A container for an error and its associated diagnostic context.
    template <typename ErrorType, typename ContextType> struct error_with_context {
        ErrorType err;
        ContextType ctx;
    };

    // Helper to create a std::unexpected containing error_with_context.
    template <typename ErrorType, typename ContextType>
    inline auto make_err(ErrorType err, ContextType context) {
        return ::std::unexpected(error_with_context<ErrorType, ContextType>{err, ::std::move(context)});
    }
}

/**
 * @brief Defines a custom boost::system::error_category for a specific enum.
 * This macro automates the boilerplate required to integrate custom enums 
 * with the Boost error system, including message mapping and code conversion.
 */
#define DEFINE_ERROR_CATEGORY(EnumName, CategoryString, MessageLogic)                                                       \
    /**--------------------- Define the error category class for the given enum ----------------------------------------**/ \
    struct EnumName##_category : public boost::system::error_category {                                                     \
        const char *name() const noexcept override { return CategoryString; }                                               \
        ::std::string message(int ev) const override {                                                                      \
            auto e = static_cast<EnumName>(ev);                                                                             \
            MessageLogic                                                                                                    \
            return boost::system::system_category().message(ev);                                                            \
        }                                                                                                                   \
    };                                                                                                                      \
    /**--------------------- Provides a singleton instance of the category ---------------------------------------------**/ \
    inline const boost::system::error_category &get_##EnumName##_cat() noexcept {                                           \
        static EnumName##_category instance;                                                                                \
        return instance;                                                                                                    \
    }                                                                                                                       \
    /**--------------------- Enables automatic conversion from EnumName to boost::system::error_code -------------------**/ \
    inline boost::system::error_code make_error_code(EnumName e) noexcept {                                                 \
        return {static_cast<int>(e), get_##EnumName##_cat()};                                                               \
    }                                                                                                                       \
    /**--------------------- Helper to get error message from enum value -----------------------------------------------**/ \
    inline ::std::string get_error_message(EnumName e) noexcept {                                                           \
        return get_##EnumName##_cat().message(static_cast<int>(e));                                                         \
    }

/**
 * @brief Registers a custom formatter for spdlog/fmt for an error_with_context object.
 * @param ErrorType         The error enum type.
 * @param ContextType       The context object type.
 * @param FormatTemplate    The string literal for formatting (e.g., "Error: {}, Context: {}").
 * @param ... Additional arguments passed to the formatter (access 'err' and 'ctx' variables).
 */
#define REGISTER_ERROR_FORMATTER(ErrorType, ContextType, FormatTemplate, ...)                                               \
    namespace fmt {                                                                                                         \
        template <>                                                                                                         \
        struct formatter<::error_utils::error_with_context<ErrorType, ContextType>> {                                       \
            constexpr auto parse(format_parse_context& fctx) { return fctx.begin(); }                                       \
            template <typename FormatContext>                                                                               \
            auto format(const ::error_utils::error_with_context<ErrorType, ContextType>& e, FormatContext& fctx) const {    \
                [[maybe_unused]] const auto& err = e.err;                                                                   \
                [[maybe_unused]] const auto& ctx = e.ctx;                                                                   \
                return ::fmt::format_to(fctx.out(), FormatTemplate __VA_OPT__(,) __VA_ARGS__);                              \
            }                                                                                                               \
        };                                                                                                                  \
    }

// @brief Convenience wrapper to return an error with context.
#define REGISTER_BOOST_ERROR(FullEnumName)                                                                                  \
    namespace boost::system {                                                                                               \
        template <> struct is_error_code_enum<FullEnumName> : ::std::true_type {};                                          \
    }

// @brief Convenience wrapper to return an error with context.
#define FAIL_WITH(Err, ...) ::error_utils::make_err(Err, __VA_ARGS__)