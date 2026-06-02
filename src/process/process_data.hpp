#pragma once

#include "process/process_discovery.hpp"
#include "scrapers/models/statm_entry.hpp"
#include <optional>
#include <string>
#include <vector>

namespace process {

    class ProcfsReader {
    public:
        struct StatusMeta {
            std::string name = "Unknown";
            pid_t ppid = 0;
            size_t thread_count = 1;
        };

        [[nodiscard]] static std::optional<StatusMeta> fetch_status_meta(pid_t pid) noexcept;
        [[nodiscard]] static std::optional<scrapers::StatmEntry> fetch_metrics(pid_t pid) noexcept;
    };

    struct ProcessTreeNode {
        ProcessInfo info;
        std::vector<ProcessTreeNode> children;

        [[nodiscard]] uint64_t aggregate_rss() const noexcept;
        [[nodiscard]] uint64_t aggregate_vm() const noexcept;
        [[nodiscard]] size_t aggregate_threads() const noexcept;
    };

    class ProcessTreeBuilder {
    public:
        // Передача по значению (sink-argument) позволяет избежать копирования тяжелых строк
        [[nodiscard]] static std::vector<ProcessTreeNode> build(std::vector<ProcessInfo> flat_processes) noexcept;
    };
}