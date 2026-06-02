#pragma once

#include "process/process_filter.hpp"
#include "process/procfs_reader.hpp"
#include <condition_variable>
#include "process/types.hpp"
#include <unordered_map>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

namespace process {
    // @brief A background telemetry engine for tracking Linux process life-cycles and memory metrics.
    class ProcessMonitor {
    public:
        // Invocable callback type for dispatching batched telemetry events.
        using Callback = std::move_only_function<void(std::vector<ProcessEvent>) const>;

        /**
        * @brief Constructs a new Process Monitor instance.
        * @param cb         Downstream receiver for process state mutations.
        * @param interval   Time to sleep between sequential scan sweeps (defaults to 500ms).
        */
        explicit ProcessMonitor(Callback cb, std::chrono::milliseconds interval = std::chrono::milliseconds(500)) noexcept;
        
        // @brief Destroys the monitor, implicitly halting the background worker thread.
        ~ProcessMonitor();

        // Disable copy and move semantics due to thread and mutex ownership
        ProcessMonitor(const ProcessMonitor&) = delete;
        ProcessMonitor& operator=(const ProcessMonitor&) = delete;
        ProcessMonitor(ProcessMonitor&&) = delete;
        ProcessMonitor& operator=(ProcessMonitor&&) = delete;

        void start() noexcept; // @brief Asynchronously launches the background monitoring loop.
        void stop() noexcept; // @brief Requests termination of the monitoring loop and blocks until completion.

        // @brief Dynamically updates the ruleset for filtering which processes are tracked.
        void update_filter(FilterConfig config) noexcept;

    private:
        // Represents the last known state of a process to compute deltas
        struct CachedState {
            std::string name;
            scrapers::StatmEntry last_metrics;
            pid_t ppid = 0;
            size_t thread_count = 1;
        };

        void run(std::stop_token st) noexcept; // Main execution loop run by the background thread
        
        // Evaluates a single PID against the cache to detect creations or mutations
        void process_single_pid(pid_t pid, std::unordered_map<pid_t, CachedState>& cache, 
            std::vector<ProcessEvent>& events) noexcept;

        // Constructs a standardized telemetry payload from kernel-specific data types
        [[nodiscard]] ProcessInfo create_process_info(pid_t pid, const ProcfsReader::StatusMeta& meta, 
            const scrapers::StatmEntry& statm) const noexcept;

        Callback callback_;                     // Downstream event dispatcher
        std::chrono::milliseconds interval_;    // Polling sleep duration
        ProcessFilter filter_;                  // Atomic ruleset for PID exclusion
        std::jthread worker_;                   // C++20 cooperatively interruptible thread
        std::mutex mutex_;                      // Synchronizes access to shared state (e.g., filter updates)
        std::condition_variable_any cv_;        // Used to wake the worker thread on filter updates or stop requests
    };
}