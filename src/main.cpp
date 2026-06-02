#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include "utils/logger.hpp"
#include "sys_io/fs_reader.hpp"
#include "sys_io/fs_directory.hpp"
#include "scrapers/kv_scraper.hpp"
#include "scrapers/statm_scraper.hpp"
#include "scrapers/maps_scraper.hpp"
#include "scrapers/smaps_scraper.hpp"

// Обновленные и объединенные заголовочные файлы подсистемы маппинга процессов
#include "process/process_discovery.hpp"
#include "process/process_data.hpp"
#include "process/process_monitor.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include <algorithm>
#include <functional> // Для поддержки современных функциональных объектов

using namespace std::chrono_literals;
namespace fs = std::filesystem;

void create_test_files() {
    fs::create_directories("test_data");

    std::ofstream file1("test_data/sample.txt");
    file1 << "Line 1: Hello, World!\n";
    file1 << "Line 2: Testing sys_io component\n";
    file1 << "Line 3: File reading functionality\n";
    file1.close();

    std::ofstream file2("test_data/info.txt");
    file2 << "Test file 2 content\n";
    file2.close();

    std::ofstream kv_file("test_data/kv_data.txt");
    kv_file << "Name:\tMemoryAnalysisTool\n";
    kv_file << "State:\tS (sleeping)\n";
    kv_file << "Pid:\t1234\n";
    kv_file << "VmSize:\t12345 kB\n";
    kv_file << "VmRSS:\t6789 kB\n";
    kv_file << "VmData:\t5000 kB\n";
    kv_file.close();

    std::ofstream statm_file("test_data/statm_data.txt");
    statm_file << "1234 567 89 100 0 200 0\n";
    statm_file.close();

    std::ofstream maps_file("test_data/maps_data.txt");
    maps_file << "00400000-00452000 r-xp 00000000 08:02 123456 /usr/bin/cat\n";
    maps_file << "00652000-00653000 rw-p 00052000 08:02 123456 /usr/bin/cat\n";
    maps_file.close();

    std::ofstream smaps_file("test_data/smaps_data.txt");
    smaps_file << "00400000-00452000 r-xp 00000000 08:02 123456 /usr/bin/cat\n";
    smaps_file << "Size:\t132 kB\n";
    smaps_file << "Rss:\t12 kB\n";
    smaps_file << "Pss:\t6 kB\n";
    smaps_file << "Shared_Clean:\t0 kB\n";
    smaps_file << "\n";
    smaps_file << "00652000-00653000 rw-p 00052000 08:02 123456 /usr/bin/cat\n";
    smaps_file << "Size:\t4 kB\n";
    smaps_file.close();
}

bool test_read_all() {
    spdlog::info("=== Test 1: Read All File Content ===");

    auto reader = sys_io::FileReader::open("test_data/sample.txt");
    if (!reader) {
        spdlog::error("Failed to open file: {}", reader.error());
        return false;
    }

    auto content = reader->read_all();
    if (!content) {
        spdlog::error("Failed to read file: {}", content.error());
        return false;
    }

    spdlog::info("File size: {} bytes", content->size());
    spdlog::info("Content:\n{}", *content);
    return true;
}

bool test_read_lines() {
    spdlog::info("=== Test 2: Read Line by Line ===");

    auto reader = sys_io::FileReader::open("test_data/sample.txt");
    if (!reader) {
        spdlog::error("Failed to open file: {}", reader.error());
        return false;
    }

    std::string line;
    int line_num = 1;

    while (true) {
        auto result = reader->read_line(line);
        if (!result) {
            spdlog::error("Error reading line: {}", result.error());
            return false;
        }

        if (!*result) break;
        spdlog::info("Line {}: {}", line_num++, line);
    }

    spdlog::info("Total lines read: {}", line_num - 1);
    return true;
}

bool test_enumerate_directory() {
    spdlog::info("=== Test 3: Enumerate Directory ===");

    int file_count = 0;
    auto result = sys_io::enumerate_dir("test_data", [&](std::string_view filename) {
        spdlog::info("Found file: {}", filename);
        file_count++;
    });

    if (!result) {
        spdlog::error("Failed to enumerate directory: {}\n", result.error());
        return false;
    }

    spdlog::info("Total files found: {}", file_count);
    return true;
}

bool test_error_handling() {
    spdlog::info("=== Test 4: Error Handling ===");

    spdlog::info("Attempting to open non-existent file...");
    auto reader = sys_io::FileReader::open("test_data/nonexistent.txt");

    if (!reader) {
        spdlog::warn("Expected error caught! {}", reader.error());
        return true;
    }

    spdlog::error("Should have received an error for non-existent file!");
    return false;
}

bool test_kv_scraper() {
    spdlog::info("=== Test 5: KV Scraper (Key-Value Parser) ===");

    auto result = scrapers::KVScraper::scrape_file("test_data/kv_data.txt");
    if (!result) {
        spdlog::error("Failed to parse KV file: {}", result.error());
        return false;
    }

    std::string test = result->get<std::string>("Name");
    spdlog::info("Extracted name: {}", test);

    const auto& registry = *result;
    spdlog::info("Parsed {} key-value entries", registry.size());
    return true;
}

bool test_statm_scraper() {
    spdlog::info("=== Test 6: Statm Scraper (Memory Statistics Parser) ===");

    auto result = scrapers::StatmScraper::scrape_file("test_data/statm_data.txt");
    if (!result) {
        spdlog::error("Failed to parse Statm file: {}", result.error());
        return false;
    }

    const auto& entry = *result;
    // Используем хелпер преобразования страниц памяти в байты
    spdlog::info("Memory Statistics (Total size: {} bytes)", scrapers::pages_to_bytes(entry.size));
    return true;
}

bool test_maps_scraper() {
    spdlog::info("=== Test 6b: Maps Scraper (Memory Map Parser) ===");

    auto result = scrapers::MapsScraper::scrape_file("test_data/maps_data.txt");
    if (!result) {
        spdlog::error("Failed to parse Maps file: {}", result.error());
        return false;
    }

    const auto& entries = *result;
    spdlog::info("Parsed {} map entries", entries.size());
    return true;
}

bool test_smaps_scraper() {
    spdlog::info("=== Test 6c: Smaps Scraper (Detailed Memory Stats Parser) ===");

    auto result = scrapers::SmapsScraper::scrape_file("test_data/smaps_data.txt");
    if (!result) {
        spdlog::error("Failed to parse Smaps file: {}", result.error());
        return false;
    }

    const auto& entries = *result;
    spdlog::info("Parsed {} smaps entries", entries.size());
    return true;
}

bool test_scraper_error_handling() {
    spdlog::info("=== Test 7: Scraper Error Handling ===");

    std::ofstream invalid_file("test_data/invalid_statm.txt");
    invalid_file << "not a number invalid data format\n";
    invalid_file.close();

    auto result = scrapers::StatmScraper::scrape_file("test_data/invalid_statm.txt");
    if (!result) {
        spdlog::warn("Expected parse error caught! \n{}", result.error());
        return true;
    } else {
        spdlog::error("Should have failed to parse invalid data!");
        return false;
    }
}

bool test_pid_scanner() {
    spdlog::info("=== Test 8: PID Scanner (Process Discovery) ===");

    auto result = process::scan_pids();
    if (!result) {
        spdlog::error("Failed to scan PIDs: {}", result.error());
        return false;
    }

    spdlog::info("Found {} running processes", result->size());
    return true;
}

bool test_process_list() {
    spdlog::info("=== Test 9: Process List (PID, Name, Metrics) ===");

    auto pids_result = process::scan_pids();
    if (!pids_result) {
        spdlog::error("Failed to scan PIDs: {}", pids_result.error());
        return false;
    }

    const auto& pids = *pids_result;
    int displayed = 0;
    int skipped = 0;

    for (pid_t pid : pids) {
        if (displayed >= 5) break;

        // Переведено на использование нового публичного ProcfsReader
        auto status_meta = process::ProcfsReader::fetch_status_meta(pid);
        if (!status_meta) { skipped++; continue; }

        auto metrics_opt = process::ProcfsReader::fetch_metrics(pid);
        if (!metrics_opt) { skipped++; continue; }

        const auto& m = *metrics_opt;
        
        // Переводим страницы ядра Linux в Килобайты для корректного логирования
        uint64_t size_kb = scrapers::pages_to_bytes(m.size) / 1024;
        uint64_t rss_kb  = scrapers::pages_to_bytes(m.resident) / 1024;

        spdlog::info("PID: {:>6} | PPID: {:>6} | Name: {:<16} | Size: {:>8} KB | RSS: {:>8} KB | Threads: {}",
            pid, status_meta->ppid, status_meta->name, size_kb, rss_kb, status_meta->thread_count);
        displayed++;
    }

    spdlog::info("Displayed {} processes ({} skipped)", displayed, skipped);
    return true;
}

// Рекурсивный вывод дерева с псевдографикой
void print_process_hierarchy(const process::ProcessTreeNode& node, 
                             std::string indent = "", 
                             bool is_last = true) 
{
    std::string branch = is_last ? "└── " : "├── ";
    // Исправлено обращение к внутренней структуре MemoryMetrics
    double rss_mb = static_cast<double>(node.info.memory.rss_bytes) / (1024.0 * 1024.0);
    
    spdlog::info("{}{}[PID: {:5}] Name: {:<16} | RSS: {:>6.2f} MB | Threads: {}", 
                 indent, 
                 branch, 
                 node.info.pid, 
                 node.info.name, 
                 rss_mb, 
                 node.info.thread_count);

    std::string child_indent = indent + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < node.children.size(); ++i) {
        bool last_child = (i == node.children.size() - 1);
        print_process_hierarchy(node.children[i], child_indent, last_child);
    }
}

// Тест 10: Тест асинхронного гибридного монитора процессов с иерархическим группированием
bool test_process_monitoring() {
    spdlog::info("========== TEST 10: PROCESS MONITORING (HIERARCHICAL GROUPING & SEAMLESS OPTION) ==========");

    std::vector<process::ProcessInfo> tracked_processes;
    std::mutex tracked_mutex;

    auto on_changed = [&](std::vector<process::ProcessEvent> events) {
        std::lock_guard<std::mutex> lock(tracked_mutex);
        for (const auto& ev : events) {
            if (ev.kind == process::ProcessEventKind::Created) {
                tracked_processes.push_back(ev.info);
            } else if (ev.kind == process::ProcessEventKind::Updated) {
                auto it = std::find_if(tracked_processes.begin(), tracked_processes.end(),
                    [pid = ev.info.pid](const auto& item) { return item.pid == pid; });
                if (it != tracked_processes.end()) {
                    *it = ev.info;
                }
            } else if (ev.kind == process::ProcessEventKind::Terminated) {
                std::erase_if(tracked_processes, [pid = ev.info.pid](const auto& item) { return item.pid == pid; });
            }
        }
    };

    process::ProcessMonitor monitor(on_changed, 500ms, process::MonitorMode::Auto);
    monitor.start();

    spdlog::info("Monitor started. Observing and building process tree for 3 seconds...");
    std::this_thread::sleep_for(3s);

    {
        std::lock_guard<std::mutex> lock(tracked_mutex);
        spdlog::info("Currently tracking {} active user-space processes.", tracked_processes.size());

        // Метод build принимает данные по значению, безопасно передаем копию вектора под защитой мьютекса
        auto roots = process::ProcessTreeBuilder::build(tracked_processes);
        spdlog::info("Built {} distinct process root groups.", roots.size());

        // Опция 1: Свернутый/Агрегированный список (Общие метрики приложений)
        spdlog::info("=== OPTION 1: COLLAPSED / AGGREGATED VIEW (Master Processes) ===");
        int count = 0;
        for (const auto& app : roots) {
            if (++count > 5) break; // Лимитируем вывод
            double total_rss_mb = static_cast<double>(app.aggregate_rss()) / (1024.0 * 1024.0);
            size_t total_threads = app.aggregate_threads();
            spdlog::info("Application: {:<16} | Total RSS: {:>7.2f} MB | Sub-processes: {:>2} | OS Threads: {}", 
                         app.info.name, total_rss_mb, app.children.size(), total_threads);
        }

        // Опция 2: Развернутый вид (Полноценное дерево процессов по требованию)
        if (!roots.empty()) {
            auto max_it = std::max_element(roots.begin(), roots.end(),
                [](const auto& a, const auto& b) { return a.aggregate_rss() < b.aggregate_rss(); });
            
            spdlog::info("=== OPTION 2: HIERARCHICAL TREE VIEW (Largest App Tree Rooted at PID {}) ===", max_it->info.pid);
            print_process_hierarchy(*max_it);
        }
    }

    spdlog::info("Stopping monitor...");
    monitor.stop();
    return true;
}

int main() {
    logger_utils::initLogger();
    spdlog::set_level(spdlog::level::debug); // Включаем отладку для просмотра частых обновлений памяти
    spdlog::info("=== Memory Analysis Tool - Component Tests ===\n");

    struct TestResult {
        const char* name;
        bool passed;
    };

    std::vector<TestResult> results;
    create_test_files();

    spdlog::info("========== SYS_IO COMPONENT TESTS ==========\n");
    results.push_back({"Read All File Content", test_read_all()});
    results.push_back({"Read Line by Line", test_read_lines()});
    results.push_back({"Enumerate Directory", test_enumerate_directory()});
    results.push_back({"Error Handling", test_error_handling()});
    spdlog::info("");

    spdlog::info("========== SCRAPER COMPONENT TESTS ==========\n");
    results.push_back({"Statm Scraper", test_statm_scraper()});
    results.push_back({"Maps Scraper", test_maps_scraper()});
    results.push_back({"Smaps Scraper", test_smaps_scraper()});
    results.push_back({"Scraper Error Handling", test_scraper_error_handling()});
    spdlog::info("");

    spdlog::info("========== PROCESS COMPONENT TESTS ==========\n");
    results.push_back({"PID Scanner", test_pid_scanner()});
    results.push_back({"Process List", test_process_list()});
    results.push_back({"Process Monitor (Async)", test_process_monitoring()});
    spdlog::info("");

    int passed = 0, failed = 0;
    spdlog::info("========== TEST SUMMARY ==========\n");

    for (const auto& result : results) {
        if (result.passed) {
            spdlog::info("[PASS] {}", result.name);
            passed++;
        } else {
            spdlog::error("[FAIL] {}", result.name);
            failed++;
        }
    }

    spdlog::info("\nTotal: {} tests | Passed: {} | Failed: {}\n", results.size(), passed, failed);

    if (failed == 0) {
        spdlog::info("=== All tests completed successfully ===");
        return 0;
    } else {
        spdlog::error("=== {} test(s) failed ===", failed);
        return 1;
    }
}