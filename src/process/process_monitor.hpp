#pragma once

#include "process/process_discovery.hpp"
#include "process/process_data.hpp"
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>

namespace process {

    enum class MonitorMode { Auto, Polling };

    class ProcessMonitor {
    public:
        using Callback = std::move_only_function<void(std::vector<ProcessEvent>) const>;

        explicit ProcessMonitor(Callback cb, 
                                std::chrono::milliseconds interval = std::chrono::milliseconds(500),
                                MonitorMode mode = MonitorMode::Auto) noexcept;
        ~ProcessMonitor();

        ProcessMonitor(const ProcessMonitor&) = delete;
        ProcessMonitor& operator=(const ProcessMonitor&) = delete;
        ProcessMonitor(ProcessMonitor&&) = delete;
        ProcessMonitor& operator=(ProcessMonitor&&) = delete;

        void start() noexcept;
        void stop() noexcept;

        void update_filter(FilterConfig config) noexcept;

    private:
        struct CachedState {
            std::string name;
            scrapers::StatmEntry last_metrics;
            pid_t ppid = 0;
            size_t thread_count = 1;
        };

        void run(std::stop_token st) noexcept;
        void run_polling(std::stop_token st) noexcept;
        
        void process_single_pid(pid_t pid, 
                                std::unordered_map<pid_t, CachedState>& cache, 
                                std::vector<ProcessEvent>& events) noexcept;

        [[nodiscard]] ProcessInfo create_process_info(pid_t pid, const ProcfsReader::StatusMeta& meta, const scrapers::StatmEntry& statm) const noexcept;

        Callback callback_;
        std::chrono::milliseconds interval_;
        MonitorMode mode_;
        ProcessFilter filter_;
        std::jthread worker_;
        std::mutex mutex_;
        std::condition_variable_any cv_;
    };
}