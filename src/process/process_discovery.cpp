#include "process/process_discovery.hpp"
#include "scrapers/kv_scraper.hpp"
#include "sys_io/fs_directory.hpp"
#include <sys/stat.h>
#include <charconv>
#include <unistd.h>

namespace process {

    static_assert(std::atomic<FilterConfig>::is_always_lock_free, 
                  "FilterConfig loop requirements strict: must be lock-free");

    ProcessFilter::ProcessFilter(FilterConfig config) noexcept
        : config_(config), self_pid_(::getpid()), owner_uid_(::getuid()) {}

    void ProcessFilter::set_config(FilterConfig config) noexcept {
        config_.store(config, std::memory_order_relaxed);
    }

    FilterConfig ProcessFilter::get_config() const noexcept {
        return config_.load(std::memory_order_relaxed);
    }

    bool ProcessFilter::should_track(pid_t pid) const noexcept {
        if (pid <= 1) [[unlikely]] return false;
        
        const auto current_config = get_config();
        if (current_config.exclude_self && pid == self_pid_) [[unlikely]] return false;

        // Zero-allocation путь
        const StackPath proc_path(pid);

        struct stat st;
        if (::stat(proc_path.c_str(), &st) == 0) {
            if (current_config.only_current_user && st.st_uid != owner_uid_) return false;
        } else {
            return false; 
        }

        if (current_config.exclude_kernel_threads || current_config.exclude_zombies) {
            const StackPath status_path(pid, "/status");
            const auto res = scrapers::KVScraper::scrape_file(status_path.c_str());
            if (!res) [[unlikely]] return false;

            if (current_config.exclude_kernel_threads) {
                const long ppid = res->get<long>("PPid", 0, 0);
                if (pid == 2 || ppid == 2) return false;
                if (res->get<long>("Kthread", 0, 0) == 1) return false;
            }

            if (current_config.exclude_zombies) {
                const auto state = res->get<std::string_view>("State", 0, "");
                if (!state.empty() && state[0] == 'Z') return false;
            }
        }
        return true;
    }

    ScanResult scan_pids(const ProcessFilter& filter) noexcept {
        std::vector<pid_t> pids;
        pids.reserve(1024);

        auto res = sys_io::enumerate_dir("/proc", [&pids, &filter](std::string_view name) noexcept {
            if (name.empty()) return;
            
            pid_t pid = 0;
            const auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), pid);
            if (ec == std::errc{} && ptr == name.data() + name.size()) {
                if (filter.should_track(pid)) {
                    pids.push_back(pid);
                }
            }
        });

        if (!res) [[unlikely]] return std::unexpected(res.error());
        return pids;
    }
}