#include "process/process_data.hpp"
#include "scrapers/kv_scraper.hpp"
#include "scrapers/statm_scraper.hpp"
#include <unordered_map>
#include <unordered_set>
#include <numeric>

namespace process {

    std::optional<ProcfsReader::StatusMeta> ProcfsReader::fetch_status_meta(pid_t pid) noexcept {
        const StackPath status_path(pid, "/status");
        auto res = scrapers::KVScraper::scrape_file(status_path.c_str());
        if (!res) [[unlikely]] return std::nullopt;

        return StatusMeta{
            .name = res->get<std::string>("Name", 0, "Unknown"),
            .ppid = static_cast<pid_t>(res->get<long>("PPid", 0, 0)),
            .thread_count = static_cast<size_t>(res->get<long>("Threads", 0, 1))
        };
    }

    std::optional<scrapers::StatmEntry> ProcfsReader::fetch_metrics(pid_t pid) noexcept {
        const StackPath statm_path(pid, "/statm");
        auto res = scrapers::StatmScraper::scrape_file(statm_path.c_str());
        return res ? std::make_optional(*res) : std::nullopt;
    }

    uint64_t ProcessTreeNode::aggregate_rss() const noexcept {
        return std::accumulate(children.begin(), children.end(), info.memory.rss_bytes,
            [](uint64_t sum, const ProcessTreeNode& child) noexcept { return sum + child.aggregate_rss(); });
    }

    uint64_t ProcessTreeNode::aggregate_vm() const noexcept {
        return std::accumulate(children.begin(), children.end(), info.memory.vm_size_bytes,
            [](uint64_t sum, const ProcessTreeNode& child) noexcept { return sum + child.aggregate_vm(); });
    }

    size_t ProcessTreeNode::aggregate_threads() const noexcept {
        return std::accumulate(children.begin(), children.end(), info.thread_count,
            [](size_t sum, const ProcessTreeNode& child) noexcept { return sum + child.aggregate_threads(); });
    }

    // ==============================================================================
    // Сборка дерева с использованием Move-семантики
    // ==============================================================================
    std::vector<ProcessTreeNode> ProcessTreeBuilder::build(std::vector<ProcessInfo> flat_processes) noexcept {
        std::unordered_map<pid_t, std::vector<ProcessInfo>> parent_to_children;
        parent_to_children.reserve(flat_processes.size());

        std::unordered_set<pid_t> active_pids;
        active_pids.reserve(flat_processes.size());
        for (const auto& p : flat_processes) {
            active_pids.insert(p.pid);
        }

        std::vector<ProcessInfo> root_infos;
        root_infos.reserve(flat_processes.size() / 4);

        // Перемещаем данные (std::move) вместо копирования
        for (auto& p : flat_processes) {
            if (p.ppid <= 1 || !active_pids.contains(p.ppid)) {
                root_infos.push_back(std::move(p));
            } else {
                parent_to_children[p.ppid].push_back(std::move(p));
            }
        }

        auto build_node = [&](auto& self, ProcessInfo&& info) noexcept -> ProcessTreeNode {
            pid_t current_pid = info.pid;
            ProcessTreeNode node{ .info = std::move(info), .children = {} };
            
            if (auto it = parent_to_children.find(current_pid); it != parent_to_children.end()) {
                node.children.reserve(it->second.size());
                for (auto& child_info : it->second) {
                    node.children.push_back(self(self, std::move(child_info)));
                }
            }
            return node;
        };

        std::vector<ProcessTreeNode> roots;
        roots.reserve(root_infos.size());
        for (auto& r : root_infos) {
            roots.push_back(build_node(build_node, std::move(r)));
        }

        return roots;
    }
}