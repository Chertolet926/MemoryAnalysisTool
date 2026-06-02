#include "process/process_monitor.hpp"
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace process {

    ProcessMonitor::ProcessMonitor(Callback cb, 
                                   std::chrono::milliseconds interval,
                                   MonitorMode mode) noexcept
        : callback_(std::move(cb)), interval_(interval), mode_(mode) {}

    ProcessMonitor::~ProcessMonitor() {
        stop();
    }

    void ProcessMonitor::start() noexcept {
        std::lock_guard lock(mutex_);
        if (worker_.joinable()) return;

        worker_ = std::jthread([this](std::stop_token st) noexcept { run(st); });
    }

    void ProcessMonitor::stop() noexcept {
        {
            std::lock_guard lock(mutex_);
            if (!worker_.joinable()) return;
        }
        worker_.request_stop();
        cv_.notify_all();
        worker_.join();
    }

    void ProcessMonitor::update_filter(FilterConfig config) noexcept {
        filter_.set_config(config);
    }

    void ProcessMonitor::run(std::stop_token st) noexcept {
        spdlog::info("ProcessMonitor: Loop initialized.");
        run_polling(st);
    }

    void ProcessMonitor::run_polling(std::stop_token st) noexcept {
        std::unordered_map<pid_t, CachedState> cache;
        
        // Выделяем память под буферы ОДИН раз. Внутри цикла делаем только clear().
        std::vector<ProcessEvent> events;
        std::unordered_set<pid_t> active_set;
        
        events.reserve(128);
        active_set.reserve(1024);

        while (!st.stop_requested()) {
            auto active_pids_res = scan_pids(filter_); 
            if (!active_pids_res) {
                std::this_thread::yield();
                continue;
            }

            const auto& active_pids = *active_pids_res;
            
            active_set.clear();
            active_set.insert(active_pids.begin(), active_pids.end());
            events.clear();

            // 1. Детекция завершенных процессов
            for (auto it = cache.begin(); it != cache.end();) {
                if (!active_set.contains(it->first)) {
                    events.push_back({
                        ProcessEventKind::Terminated,
                        ProcessInfo{
                            .pid = it->first, 
                            .ppid = it->second.ppid, 
                            .thread_count = it->second.thread_count,
                            .name = std::move(it->second.name)
                        }
                    });
                    it = cache.erase(it);
                } else {
                    ++it;
                }
            }

            // 2. Опрос и обновление существующих
            for (pid_t pid : active_pids) {
                process_single_pid(pid, cache, events);
            }

            if (!events.empty() && callback_) {
                callback_(std::move(events));
            }

            // Реактивное ожидание C++20: мгновенно просыпается при stop_requested()
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, st, interval_, [&st] { return st.stop_requested(); });
        }
    }

    void ProcessMonitor::process_single_pid(pid_t pid, 
                                            std::unordered_map<pid_t, CachedState>& cache, 
                                            std::vector<ProcessEvent>& events) noexcept {
        auto cache_it = cache.find(pid);
        
        if (cache_it == cache.end()) {
            auto status_meta = ProcfsReader::fetch_status_meta(pid);
            auto metrics_opt = ProcfsReader::fetch_metrics(pid);

            if (!status_meta || !metrics_opt) [[unlikely]] return;

            cache.emplace(pid, CachedState{
                .name = status_meta->name, 
                .last_metrics = *metrics_opt,
                .ppid = status_meta->ppid,
                .thread_count = status_meta->thread_count
            });
            
            events.push_back({ProcessEventKind::Created, create_process_info(pid, *status_meta, *metrics_opt)});
        } else {
            auto metrics_opt = ProcfsReader::fetch_metrics(pid);
            if (metrics_opt) [[likely]] {
                auto& cached = cache_it->second;
                
                // Оператор <=> автоматически сгенерирован компилятором для MemoryMetrics
                if (cached.last_metrics.size != metrics_opt->size || 
                    cached.last_metrics.resident != metrics_opt->resident ||
                    cached.last_metrics.shared != metrics_opt->shared ||
                    cached.last_metrics.text != metrics_opt->text ||
                    cached.last_metrics.data != metrics_opt->data) {
                    
                    cached.last_metrics = *metrics_opt;
                    
                    ProcfsReader::StatusMeta meta{
                        .name = cached.name, .ppid = cached.ppid, .thread_count = cached.thread_count
                    };
                    events.push_back({ProcessEventKind::Updated, create_process_info(pid, meta, *metrics_opt)});
                }
            }
        }
    }

    ProcessInfo ProcessMonitor::create_process_info(pid_t pid, const ProcfsReader::StatusMeta& meta, const scrapers::StatmEntry& statm) const noexcept {
        return ProcessInfo{
            .pid = pid,
            .ppid = meta.ppid,
            .thread_count = meta.thread_count,
            .name = meta.name,
            .memory = MemoryMetrics{
                .vm_size_bytes  = statm.size.bytes(),
                .rss_bytes      = statm.resident.bytes(),
                .shared_bytes   = statm.shared.bytes(),
                .text_bytes     = statm.text.bytes(),
                .data_bytes     = statm.data.bytes()
            }
        };
    }
}