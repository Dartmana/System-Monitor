#include "sysmon.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <ctime>

// ─── ANSI helpers ─────────────────────────────────────────────────────────────

static bool use_color = true;   // set to false for plain output / piping

std::string ansi(const std::string& code) {
    if (!use_color) return "";
    return "\033[" + code + "m";
}

static const std::string RESET  = "\033[0m";
static const std::string BOLD   = "\033[1m";
static const std::string DIM    = "\033[2m";
static const std::string RED    = "\033[31m";
static const std::string GREEN  = "\033[32m";
static const std::string YELLOW = "\033[33m";
static const std::string BLUE   = "\033[34m";
static const std::string CYAN   = "\033[36m";
static const std::string WHITE  = "\033[97m";

static std::string col(const std::string& c, const std::string& s) {
    if (!use_color) return s;
    return c + s + RESET;
}

// ─── Utilities ────────────────────────────────────────────────────────────────

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B","KiB","MiB","GiB","TiB"};
    double val = bytes;
    int i = 0;
    while (val >= 1024.0 && i < 4) { val /= 1024.0; ++i; }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << val << " " << units[i];
    return ss.str();
}

std::string progress_bar(double pct, int width) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    int filled = static_cast<int>(std::round(pct / 100.0 * width));
    std::string color = (pct < 60) ? GREEN : (pct < 85) ? YELLOW : RED;

    std::string bar = "[";
    if (use_color) bar += color;
    // Use simple ASCII characters for maximum portability
    for (int i = 0; i < filled; ++i)      bar += '#';
    for (int i = 0; i < width - filled; ++i) bar += '-';
    if (use_color) bar += RESET;
    bar += "]";
    return bar;
}

static std::string uptime_str(double seconds) {
    int s = (int)seconds;
    int d = s / 86400; s %= 86400;
    int h = s / 3600;  s %= 3600;
    int m = s / 60;    s %= 60;
    std::ostringstream ss;
    if (d) ss << d << "d ";
    ss << std::setw(2) << std::setfill('0') << h << ":"
       << std::setw(2) << std::setfill('0') << m << ":"
       << std::setw(2) << std::setfill('0') << s;
    return ss.str();
}

static void section(const std::string& title) {
    int dashes = (int)(50 - title.size());
    if (dashes < 0) dashes = 0;
    std::string sep(dashes, '-');
    std::cout << "\n" << col(BOLD + CYAN, "+- " + title + " ")
              << col(DIM, sep)
              << "\n";
}

// ─── Renderers ────────────────────────────────────────────────────────────────

void render_header(const SystemInfo& si) {
    // Clear screen
    if (use_color) std::cout << "\033[2J\033[H";

    std::time_t now = std::time(nullptr);
    char tbuf[32];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::cout << col(BOLD + WHITE, "╔══════════════════════════════════════════════════════╗") << "\n"
              << col(BOLD + WHITE, "║") << col(BOLD + CYAN, "  🖥  Linux System Health Monitor")
              << col(DIM, "  v1.0")
              << col(BOLD + WHITE, std::string(16,' ') + "║") << "\n"
              << col(BOLD + WHITE, "╚══════════════════════════════════════════════════════╝") << "\n";

    std::cout << col(DIM, "  Host   : ") << col(BOLD, si.hostname)
              << "  │  " << col(DIM, "OS: ") << si.os_release << "\n"
              << col(DIM, "  Kernel : ") << si.kernel_version
              << "  │  " << col(DIM, "CPUs: ") << si.cpu_count << "\n"
              << col(DIM, "  Uptime : ") << uptime_str(si.uptime_seconds)
              << "  │  " << col(DIM, "Load: ")
              << std::fixed << std::setprecision(2)
              << si.load_avg_1 << " " << si.load_avg_5 << " " << si.load_avg_15 << "\n"
              << col(DIM, "  Time   : ") << tbuf << "\n";
}

void render_cpu(const CpuStats& cs) {
    section("CPU");
    std::cout << "  Usage  " << progress_bar(cs.usage_percent)
              << "  " << col(BOLD, std::to_string((int)cs.usage_percent) + "%") << "\n";
}

void render_memory(const MemStats& ms) {
    section("Memory");
    uint64_t used_kb = ms.total_kb - ms.available_kb;
    std::cout << "  RAM    " << progress_bar(ms.used_percent)
              << "  " << col(BOLD, std::to_string((int)ms.used_percent) + "%")
              << "  (" << format_bytes(used_kb * 1024)
              << " / " << format_bytes(ms.total_kb * 1024) << ")\n";

    // Buffers/cache breakdown
    std::cout << col(DIM, "         Buffers: " + format_bytes(ms.buffers_kb * 1024)
              + "   Cached: " + format_bytes(ms.cached_kb * 1024)) << "\n";
}

void render_disks(const std::vector<DiskStats>& disks) {
    section("Disk");
    for (const auto& d : disks) {
        std::string mount = d.mount;
        if (mount.size() > 18) mount = "…" + mount.substr(mount.size() - 17);
        std::cout << "  " << std::left << std::setw(18) << mount
                  << " " << progress_bar(d.used_percent, 24)
                  << "  " << col(BOLD, std::to_string((int)d.used_percent) + "%")
                  << "  (" << format_bytes(d.used_bytes)
                  << " / " << format_bytes(d.total_bytes) << ")\n";
    }
}

void render_processes(const std::vector<ProcessInfo>& procs) {
    section("Top Processes  (by memory)");
    std::cout << col(DIM, "       PID  Name                  State  RSS\n");

    for (const auto& p : procs) {
        std::string name = p.name;
        if (name.size() > 20) name = name.substr(0, 17) + "...";

        std::string state_str;
        switch (p.state) {
            case 'R': state_str = col(GREEN,  "run  "); break;
            case 'S': state_str = col(BLUE,   "sleep"); break;
            case 'D': state_str = col(YELLOW, "wait "); break;
            case 'Z': state_str = col(RED,    "zombi"); break;
            default:  state_str = col(DIM,    std::string(1, p.state) + "    "); break;
        }

        std::cout << "  " << std::right << std::setw(8) << p.pid
                  << "  " << std::left  << std::setw(20) << name
                  << "  " << state_str
                  << "  " << std::right << std::setw(8) << format_bytes(p.rss_kb * 1024)
                  << "\n";
    }
}

void render_alerts(const CpuStats& cs, const MemStats& ms,
                   const std::vector<DiskStats>& disks) {
    bool any = false;
    auto alert = [&](const std::string& msg) {
        if (!any) { section("⚠  Alerts"); any = true; }
        std::cout << "  " << col(RED + BOLD, "WARN") << "  " << msg << "\n";
    };

    if (cs.usage_percent > 90)
        alert("CPU usage critical: " + std::to_string((int)cs.usage_percent) + "%");

    if (ms.used_percent > 90)
        alert("Memory usage critical: " + std::to_string((int)ms.used_percent) + "%");

    for (const auto& d : disks)
        if (d.used_percent > 85)
            alert("Disk " + d.mount + " at " + std::to_string((int)d.used_percent) + "% capacity");

    if (!any) {
        section("✓  Alerts");
        std::cout << "  " << col(GREEN, "All systems nominal.\n");
    }
}
