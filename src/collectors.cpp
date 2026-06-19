#include "sysmon.h"

#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <mntent.h>
#include <cstring>

// ─── Utilities ───────────────────────────────────────────────────────────────

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ─── CPU ─────────────────────────────────────────────────────────────────────

static CpuStats read_raw_cpu() {
    CpuStats s{};
    std::ifstream f("/proc/stat");
    std::string label;
    f >> label >> s.user >> s.nice >> s.system >> s.idle
      >> s.iowait >> s.irq >> s.softirq;
    return s;
}

CpuStats collect_cpu() {
    // Sample twice, 200 ms apart, to compute usage delta
    CpuStats a = read_raw_cpu();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CpuStats b = read_raw_cpu();

    uint64_t idle_a  = a.idle + a.iowait;
    uint64_t idle_b  = b.idle + b.iowait;
    uint64_t total_a = a.user + a.nice + a.system + idle_a + a.irq + a.softirq;
    uint64_t total_b = b.user + b.nice + b.system + idle_b + b.irq + b.softirq;

    uint64_t d_idle  = idle_b  - idle_a;
    uint64_t d_total = total_b - total_a;

    b.usage_percent = (d_total == 0) ? 0.0
                      : 100.0 * (1.0 - static_cast<double>(d_idle) / d_total);
    return b;
}

// ─── Memory ──────────────────────────────────────────────────────────────────

MemStats collect_memory() {
    MemStats ms{};
    std::ifstream f("/proc/meminfo");
    std::string key;
    uint64_t val;
    std::string unit;

    while (f >> key >> val) {
        f >> unit;   // "kB"
        if      (key == "MemTotal:")     ms.total_kb     = val;
        else if (key == "MemFree:")      ms.free_kb      = val;
        else if (key == "MemAvailable:") ms.available_kb = val;
        else if (key == "Buffers:")      ms.buffers_kb   = val;
        else if (key == "Cached:")       ms.cached_kb    = val;
    }
    uint64_t used = ms.total_kb - ms.available_kb;
    ms.used_percent = (ms.total_kb == 0) ? 0.0
                      : 100.0 * used / ms.total_kb;
    return ms;
}

// ─── Disk ────────────────────────────────────────────────────────────────────

std::vector<DiskStats> collect_disks() {
    std::vector<DiskStats> result;
    FILE* mtab = setmntent("/proc/mounts", "r");
    if (!mtab) return result;

    struct mntent* entry;
    while ((entry = getmntent(mtab)) != nullptr) {
        std::string fstype(entry->mnt_type);
        // Only real filesystems
        if (fstype == "tmpfs" || fstype == "devtmpfs" ||
            fstype == "sysfs" || fstype == "proc"     ||
            fstype == "cgroup" || fstype == "cgroup2" ||
            fstype == "devpts" || fstype == "securityfs" ||
            fstype == "pstore" || fstype == "debugfs")
            continue;

        struct statvfs st{};
        if (statvfs(entry->mnt_dir, &st) != 0) continue;

        uint64_t total = (uint64_t)st.f_blocks * st.f_frsize;
        uint64_t free_ = (uint64_t)st.f_bfree  * st.f_frsize;
        uint64_t used  = total - free_;

        if (total == 0) continue;

        DiskStats ds;
        ds.mount       = entry->mnt_dir;
        ds.total_bytes = total;
        ds.used_bytes  = used;
        ds.free_bytes  = free_;
        ds.used_percent = 100.0 * used / total;
        result.push_back(ds);
    }
    endmntent(mtab);
    return result;
}

// ─── Processes ───────────────────────────────────────────────────────────────

std::vector<ProcessInfo> collect_top_processes(int n) {
    std::vector<ProcessInfo> procs;
    DIR* dir = opendir("/proc");
    if (!dir) return procs;

    long page_size = sysconf(_SC_PAGE_SIZE);
    long clock_hz  = sysconf(_SC_CLK_TCK);

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Only numeric directories = PIDs
        bool all_digits = true;
        for (char* p = entry->d_name; *p; ++p)
            if (!isdigit(*p)) { all_digits = false; break; }
        if (!all_digits) continue;

        int pid = atoi(entry->d_name);
        std::string stat_path = "/proc/" + std::string(entry->d_name) + "/stat";
        std::ifstream sf(stat_path);
        if (!sf.is_open()) continue;

        ProcessInfo pi;
        pi.pid = pid;

        std::string line;
        std::getline(sf, line);
        // Format: pid (name) state ...
        size_t lp = line.find('(');
        size_t rp = line.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos) continue;

        pi.name  = line.substr(lp + 1, rp - lp - 1);
        std::istringstream rest(line.substr(rp + 2));

        long utime, stime, rss_pages;
        long dummy_l;
        rest >> pi.state;
        for (int i = 0; i < 10; ++i) rest >> dummy_l;  // ppid–cmajflt (fields 4–13)
        rest >> utime >> stime;                          // fields 14–15
        for (int i = 0; i < 8; ++i) rest >> dummy_l;   // cutime–vsize (fields 16–23)
        rest >> rss_pages;                               // field 24

        pi.rss_kb  = rss_pages * page_size / 1024;
        pi.cpu_pct = clock_hz > 0 ? (double)(utime + stime) / clock_hz : 0.0;

        procs.push_back(pi);
    }
    closedir(dir);

    // Sort by RSS descending (proxy for resource usage)
    std::sort(procs.begin(), procs.end(),
              [](const ProcessInfo& a, const ProcessInfo& b){
                  return a.rss_kb > b.rss_kb;
              });
    if ((int)procs.size() > n) procs.resize(n);
    return procs;
}

// ─── System Info ─────────────────────────────────────────────────────────────

SystemInfo collect_system_info() {
    SystemInfo si;

    // Hostname
    char hbuf[256]{};
    gethostname(hbuf, sizeof(hbuf));
    si.hostname = hbuf;

    // Kernel
    struct utsname uts{};
    uname(&uts);
    si.kernel_version = std::string(uts.sysname) + " " + uts.release;

    // OS release (best-effort)
    std::ifstream osr("/etc/os-release");
    std::string line;
    while (std::getline(osr, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            si.os_release = line.substr(12);
            // Strip quotes
            si.os_release.erase(
                std::remove(si.os_release.begin(), si.os_release.end(), '"'),
                si.os_release.end());
        }
    }
    if (si.os_release.empty()) si.os_release = uts.version;

    // Uptime
    std::ifstream upf("/proc/uptime");
    upf >> si.uptime_seconds;

    // Load average
    std::ifstream laf("/proc/loadavg");
    laf >> si.load_avg_1 >> si.load_avg_5 >> si.load_avg_15;

    // CPU count
    si.cpu_count = (int)sysconf(_SC_NPROCESSORS_ONLN);

    return si;
}
