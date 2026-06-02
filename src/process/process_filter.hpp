#pragma once

#include "scrapers/kv_scraper.hpp"
#include "utils/memory_types.hpp"
#include "process/types.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>

namespace process {
    //@brief Thread-safe predicate evaluator that determines if a given PID meets active tracking rules.
    class ProcessFilter {
        private:
            std::atomic<FilterConfig> config_;  // <-- Runtime evaluation options managed atomically.
            pid_t self_pid_;                    // <-- Cached PID of this application to avoid self-monitoring.
            uid_t owner_uid_;                   // <-- Cached effective UID of the host runner to verify ownership.

        public:
            /**
            * @brief Constructs a new ProcessFilter instance and caches the current process context.
            * @param config Initial rules mapping out which processes to track or ignore.
            */
            explicit ProcessFilter(FilterConfig config = {}) noexcept :
                config_(config), self_pid_(::getpid()), owner_uid_(::getuid()) {}

            /**
            * @brief Dynamically updates the active configuration criteria.
            * @param config The new configuration options to enforce.
            */
            void set_config(FilterConfig config) noexcept {
                // Relaxed ordering is sufficient as configuration changes do not act as data synchronization barriers
                config_.store(config, std::memory_order_relaxed);
            }

            /**
            * @brief Retrieves a copy of the current filter options.
            * @return FilterConfig The active rule criteria.
            */
            [[nodiscard]] FilterConfig get_config() const noexcept {
                return config_.load(std::memory_order_relaxed);
            }

            /**
            * @brief Determines whether a given process should be tracked based on active constraints.
            * @param pid    The process identifier being audited.
            * @return       true If the process matches all criteria and should be analyzed.
            * @return       false If the process is excluded or fails validation rules.
            */
            [[nodiscard]] bool should_track(pid_t pid) const noexcept {
                if (pid <= 1) [[unlikely]] return false; // Ignore idle (0) and init (1) tasks explicitly
            
                const auto cfg = get_config();
                if (cfg.exclude_self && pid == self_pid_) [[unlikely]] return false;

                const StackPath proc_path(pid);

                // Fast filesystem sanity check to confirm existence and inspect UID properties
                struct stat st;
                if (::stat(proc_path.c_str(), &st) != 0) return false;
                if (cfg.only_current_user && st.st_uid != owner_uid_) return false;

                // Defer high-overhead filesystem reads until mandatory rules demand them
                if (cfg.exclude_kernel_threads || cfg.exclude_zombies) {
                    const StackPath status_path(pid, "/status");
                    auto res = scrapers::KVScraper::scrape_file(status_path.c_str());
                    if (!res) [[unlikely]] return false;

                    if (cfg.exclude_kernel_threads) {
                        const long ppid = res->get<long>("PPid", 0, 0);
                        // Match against kthreadd (PID 2), its direct children, or explicitly marked kthreads
                        if (pid == 2 || ppid == 2) return false;
                        if (res->get<long>("Kthread", 0, 0) == 1) return false;
                    }

                    if (cfg.exclude_zombies) {
                        const auto state = res->get<std::string>("State", 0, "");
                        if (!state.empty() && state[0] == 'Z') return false;
                    }
                }

                return true;
            }
    };
}