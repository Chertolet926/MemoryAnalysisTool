#include "process/process_monitor.hpp"
#include "process/pid_scanner.hpp"
#include <spdlog/spdlog.h>
#include <unordered_set>

namespace process {
    /**
    * @brief Constructs a ProcessMonitor instance with execution parameters.
    * @param cb         Invocable target triggered when telemetry updates or state changes are detected.
    * @param interval   Time duration to sleep between sequential /proc scan sweeps.
    * @param mode       Structural strategy flag for monitoring operations.
    */
    ProcessMonitor::ProcessMonitor(Callback cb, std::chrono::milliseconds interval) noexcept : callback_(std::move(cb)), interval_(interval){}

    // @brief Destructor. Ensures the background worker thread is safely halted and joined.
    ProcessMonitor::~ProcessMonitor() { stop(); }

    // @brief Spawns the background telemetry worker thread if it is not already active.
    void ProcessMonitor::start() noexcept {
        std::lock_guard lock(mutex_);
        if (!worker_.joinable()) worker_ = std::jthread(
            [this](std::stop_token st) noexcept { run(st); });
    }

    // @brief Initiates a cooperative cancellation request, interrupts waiting states, and joins the worker.
    void ProcessMonitor::stop() noexcept {
        
        {
            // Scoped lock bounds to quickly evaluate lifecycle status
            std::lock_guard lock(mutex_);
            if (!worker_.joinable()) return;
        }
        
        worker_.request_stop();
        cv_.notify_all();       // Wake up thread if it is currently parked within wait_for
        worker_.join();         // Block until the execution frame unwinds entirely
    }

    /**
    * @brief Forwards a new runtime configuration rule-set to the internal atomic filter.
    * @param config The updated process filtering constraints.
    */
    void ProcessMonitor::update_filter(FilterConfig config) noexcept {
        filter_.set_config(config);
    }

    void ProcessMonitor::run(std::stop_token st) noexcept {
        spdlog::info("ProcessMonitor: Loop initialized.");

        std::unordered_map<pid_t, CachedState> cache;
        std::vector<ProcessEvent> events;
        std::unordered_set<pid_t> active_set;
        
        // Pre-allocate to prevent runtime churn
        events.reserve(128);
        active_set.reserve(1024);

        while (!st.stop_requested()) {
            if (auto pids_res = scan_pids(filter_)) {
                active_set.clear();
                active_set.insert(pids_res->begin(), pids_res->end());
                events.clear();

                // Step 1: Evict terminated processes from the cache
                std::erase_if(cache, [&](auto& item) {
                    auto& [pid, state] = item;
                    if (!active_set.contains(pid)) {
                        events.push_back({
                            ProcessEventKind::Terminated,
                            ProcessInfo{pid, state.ppid, state.thread_count, std::move(state.name)}
                        });    
                        return true;
                    }
                    return false;
                });

                // Step 2: Process active execution layout
                for (pid_t pid : *pids_res)
                    process_single_pid(pid, cache, events);

                // Step 3: Dispatch event delta downstream
                if (!events.empty() && callback_)
                    callback_(std::move(events));
            } else {
                std::this_thread::yield(); // Back-off briefly if /proc scan fails
            }

            // Sleep until the next interval or a stop request
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, st, interval_, [&st] { return st.stop_requested(); });
        }
    }

    void ProcessMonitor::process_single_pid(
        pid_t pid, std::unordered_map<pid_t, CachedState>& cache, 
        std::vector<ProcessEvent>& events) noexcept {
        
        auto metrics = ProcfsReader::fetch_metrics(pid);
        if (!metrics) return; // Early exit on read failure

        if (auto it = cache.find(pid); it != cache.end()) {
            // Warm path: Process is already being tracked
            auto& cached = it->second;
            const auto& m = *metrics;
            const auto& c = cached.last_metrics;
            
            // Trigger update only if memory structural boundaries shift
            if (c.size != m.size || c.resident != m.resident || 
                c.shared != m.shared || c.text != m.text || c.data != m.data) {
                
                cached.last_metrics = m;
                ProcfsReader::StatusMeta meta{
                    .name = cached.name, .ppid = cached.ppid, .thread_count = cached.thread_count
                };

                events.push_back({ProcessEventKind::Updated, create_process_info(pid, meta, m)});
            }
        
        } else if (auto meta = ProcfsReader::fetch_status_meta(pid)) {
            // Cold path: Freshly discovered process
            cache.emplace(pid, CachedState{meta->name, *metrics, meta->ppid, meta->thread_count});
            events.push_back({ProcessEventKind::Created, create_process_info(pid, *meta, *metrics)});
        }
    }

    // Transform kernel types into standardized telemetry
    ProcessInfo ProcessMonitor::create_process_info(
        pid_t pid, const ProcfsReader::StatusMeta& meta, 
        const scrapers::StatmEntry& statm) const noexcept {
        
        return {
            .pid = pid,
            .ppid = meta.ppid,
            .thread_count = meta.thread_count,
            .name = meta.name,
            .memory = {
                .vm_size_bytes  = statm.size.bytes(),
                .rss_bytes      = statm.resident.bytes(),
                .shared_bytes   = statm.shared.bytes(),
                .text_bytes     = statm.text.bytes(),
                .data_bytes     = statm.data.bytes()
            }
        };
    }
}