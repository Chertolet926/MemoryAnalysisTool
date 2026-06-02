#pragma once

#include "process/types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <numeric>
#include <vector>

namespace process {
    // A recursive hierarchical representation of a system process and its descendants.
    struct ProcessTreeNode {
        ProcessInfo info;                       // <-- Telemetry data for the current process
        std::vector<ProcessTreeNode> children;  // <-- Child processes spawned by this node

        // @brief Recursively calculates the total Resident Set Size (RSS) of this process and all its children.
        [[nodiscard]] uint64_t aggregate_rss() const noexcept {
            return std::accumulate(children.begin(), children.end(), info.memory.rss_bytes,
                [](uint64_t sum, const ProcessTreeNode& child) noexcept {
                    return sum + child.aggregate_rss();
                });
        }

        // @brief Recursively calculates the total Virtual Memory (VM) size of this process and all its children.
        [[nodiscard]] uint64_t aggregate_vm() const noexcept {
            return std::accumulate(children.begin(), children.end(), info.memory.vm_size_bytes,
                [](uint64_t sum, const ProcessTreeNode& child) noexcept {
                    return sum + child.aggregate_vm();
                });
        }

        // @brief Recursively calculates the total number of threads spawned by this process and all its children.
        [[nodiscard]] size_t aggregate_threads() const noexcept {
            return std::accumulate(children.begin(), children.end(), info.thread_count,
                [](size_t sum, const ProcessTreeNode& child) noexcept {
                    return sum + child.aggregate_threads();
                });
        }
    };

    // Utility class to reconstruct a hierarchical process forest from a flat list of process states.
    class ProcessTreeBuilder {
        public:
            /**
            * @brief Constructs a forest (multiple root nodes) of process trees from a flat snapshot.
            * @param flat_processes A one-dimensional list of process telemetry objects. 
            * Passed by value to allow destructive moving (zero-copy overhead) during assembly.
            * @return A vector of root nodes (typically processes with PPID <= 1 or missing parents).
            */
            [[nodiscard]] static std::vector<ProcessTreeNode> build(std::vector<ProcessInfo> flat_processes) noexcept {
                // Map to efficiently group and look up children by their parent PID
                std::unordered_map<pid_t, std::vector<ProcessInfo>> parent_to_children;
                parent_to_children.reserve(flat_processes.size());

                // Fast lookup set to verify if a parent process actually exists in the current snapshot
                std::unordered_set<pid_t> active_pids;
                active_pids.reserve(flat_processes.size());
                for (const auto& p : flat_processes)
                    active_pids.insert(p.pid);


                // Temporary storage for detected root processes
                std::vector<ProcessInfo> root_infos;
                root_infos.reserve(flat_processes.size() / 4); // Heuristic capacity reservation

                // Pass 1: Categorize flat nodes into roots and nested children
                for (auto& p : flat_processes) {
                    // Treat as root if PPID is init/systemd (<=1) OR if the parent is missing/terminated
                    if (p.ppid <= 1 || !active_pids.contains(p.ppid)) {
                        root_infos.push_back(std::move(p));
                    } else {
                        parent_to_children[p.ppid].push_back(std::move(p));
                    }
                }

                // Recursive closure to assemble the tree bottom-up
                auto build_node = [&](auto& self, ProcessInfo&& info) noexcept -> ProcessTreeNode {
                    pid_t current_pid = info.pid;
                    ProcessTreeNode node{ .info = std::move(info), .children = {} };

                    // If this process has children mapped to it, recursively build and attach them
                    if (auto it = parent_to_children.find(current_pid); it != parent_to_children.end()) {
                        node.children.reserve(it->second.size());
                        for (auto& child_info : it->second)
                            node.children.push_back(self(self, std::move(child_info)));
                    }

                    return node;
                };

                // Pass 2: Final tree assembly starting from the identified roots
                std::vector<ProcessTreeNode> roots;
                roots.reserve(root_infos.size());

                for (auto& r : root_infos)
                    roots.push_back(build_node(build_node, std::move(r)));
            
                return roots;
            }
    };
}