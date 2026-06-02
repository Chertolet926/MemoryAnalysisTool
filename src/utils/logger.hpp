#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <memory>
#include <mutex>

namespace logger_utils {
    // A custom spdlog logger that automatically splits multi-line messages.
    class MultilineLogger : public spdlog::logger {
        public:
            using spdlog::logger::logger; // Inherits all constructors from the base logger class.

        private:
            std::mutex logger_mutex_; // // Prevents log interleaving from multiple threads
    
        protected:
            // Intercepts the log message before it is written to the sinks
            void sink_it_(const spdlog::details::log_msg &msg) override {
                std::lock_guard<std::mutex> lock(logger_mutex_);
                std::string_view payload(msg.payload.data(), msg.payload.size());
        
                size_t prev = 0; size_t pos = 0;
                spdlog::details::log_msg sub_msg = msg; // Copy the original message

                // Loop through newlines and log each line individually
                while ((pos = payload.find('\n', prev)) != std::string_view::npos) {
                    sub_msg.payload = payload.substr(prev, pos - prev);
                    spdlog::logger::sink_it_(sub_msg); // Forward line to sinks
                    prev = pos + 1;
                }

                // Log the remaining text or a blank line if the payload was empty
                if (prev < payload.size() || payload.empty()) {
                    sub_msg.payload = payload.substr(prev);
                    spdlog::logger::sink_it_(sub_msg);
                }
            }
    };

    // @brief Initializes and registers the default application logger.
    inline void initLogger() {
        // Use single-threaded (_st) sink because MultilineLogger handles the locking
        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_st>();
        
        // Instantiate our custom logger
        auto logger = std::make_shared<MultilineLogger>("default", console_sink);
        
        // Set global log format and logging level
        logger->set_pattern("%^[%Y-%m-%d %H:%M:%S] [%l]%$ %v");
        logger->set_level(spdlog::level::info);

        // Register as the default global logger
        spdlog::set_default_logger(logger);
    }
}