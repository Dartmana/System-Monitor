#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ─── Data Structures ────────────────────────────────────────────────────────

struct CpuStats {
    double usage_percent;
    uint64_t user, nice, system, idle, iowait, irq, softirq;
};

struct MemStats {
    uint64_t total_kb;
    uint64_t free_kb;
    uint64_t available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
    double used_percent;
};

struct DiskStats {
    std::string mount;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    double used_percent;
};

struct ProcessInfo {
    int     pid;
    std::string name;
    char    state;
    long    rss_kb;   // resident set size
    double  cpu_pct;  // approximate
};

struct SystemInfo {
    std::string hostname;
    std::string os_release;
    std::string kernel_version;
    double uptime_seconds;
    double load_avg_1, load_avg_5, load_avg_15;
    int    cpu_count;
};

// ─── Collector Functions ─────────────────────────────────────────────────────

CpuStats        collect_cpu();
MemStats        collect_memory();
std::vector<DiskStats>    collect_disks();
std::vector<ProcessInfo>  collect_top_processes(int n = 10);
SystemInfo      collect_system_info();

// ─── Renderer Functions ──────────────────────────────────────────────────────

void render_header(const SystemInfo& si);
void render_cpu(const CpuStats& cs);
void render_memory(const MemStats& ms);
void render_disks(const std::vector<DiskStats>& disks);
void render_processes(const std::vector<ProcessInfo>& procs);
void render_alerts(const CpuStats& cs, const MemStats& ms,
                   const std::vector<DiskStats>& disks);

// ─── Utilities ───────────────────────────────────────────────────────────────

std::string format_bytes(uint64_t bytes);
std::string progress_bar(double pct, int width = 30);
std::string read_file(const std::string& path);
std::string ansi(const std::string& code);
