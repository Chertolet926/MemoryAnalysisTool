#pragma once

#include <sys/types.h>
#include <string>
#include <cstdint>

namespace process {
    // @brief Standardized snapshot of a process's memory footprint.
    struct MemoryMetrics {
        uint64_t vm_size_bytes  = 0; // <-- Total virtual memory allocated (VmSize)
        uint64_t rss_bytes      = 0; // <-- Resident Set Size: physical memory currently in use (VmRSS)
        uint64_t shared_bytes   = 0; // <-- Shared memory pages (RssFile + RssShmem)
        uint64_t text_bytes     = 0; // <-- Executable code size in memory
        uint64_t data_bytes     = 0; // <-- Size of the data segment (BSS + heap)

        // C++20: Automatically generates all comparison operators (==, !=, <, >, <=, >=)
        auto operator<=>(const MemoryMetrics&) const = default;
    };

    // @brief Core telemetry data for a single OS process.
    struct ProcessInfo {
        pid_t pid           = 0;    // <-- Unique Process ID
        pid_t ppid          = 0;    // <-- Parent Process ID
        size_t thread_count = 1;    // <-- Number of active threads in the process
        std::string name;           // <-- Process executable name
        MemoryMetrics memory;       // <-- Current memory utilization metrics
    };

    // @brief Defines the lifecycle state change of an observed process.
    enum class ProcessEventKind {
        Created,    // <-- A new process was discovered
        Updated,    // <-- An existing process changed its structural memory boundaries
        Terminated  // <-- A previously tracked process has exited or died
    };

    // @brief A discrete telemetry event emitted by the background monitor.
    struct ProcessEvent {
        ProcessEventKind kind;  // <-- The nature of the lifecycle event
        ProcessInfo info;       // <-- The process state at the time of the event
    };

    // @brief Ruleset configuration for filtering the process telemetry feed.
    struct FilterConfig {
        bool exclude_kernel_threads = true;     // <-- Ignore threads spawned directly by the kernel
        bool exclude_zombies        = true;     // <-- Ignore processes in the 'Z' (zombie) state
        bool exclude_self           = true;     // <-- Ignore the monitor's own PID to prevent self-observation loop
        bool only_current_user      = false;    // <-- Restrict observation to processes owned by the current UID

        // C++20: Compiler-generated comparisons for easy config equality checks
        auto operator<=>(const FilterConfig&) const = default;
    };
}