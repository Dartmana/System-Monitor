#include "sysmon.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <cstring>

static volatile bool running = true;

static void handle_signal(int) {
    running = false;
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Linux System Health Monitor — reads /proc and reports CPU, memory,\n"
              << "disk usage, top processes, and live alerts.\n\n"
              << "Options:\n"
              << "  -i, --interval <sec>   Refresh interval in seconds (default: 3)\n"
              << "  -n, --count    <num>   Exit after N snapshots (default: 0 = loop forever)\n"
              << "  -p, --procs    <num>   Number of top processes to show (default: 10)\n"
              << "      --no-color         Disable ANSI colour output\n"
              << "  -h, --help             Show this help message\n\n"
              << "Examples:\n"
              << "  " << prog << "               # Live dashboard, refresh every 3s\n"
              << "  " << prog << " -i 5 -n 1    # Single snapshot\n"
              << "  " << prog << " --no-color    # Plain text (good for logging)\n";
}

int main(int argc, char* argv[]) {
    // ── Defaults ──────────────────────────────────────────────────────────────
    int interval_sec = 3;
    int max_count    = 0;   // 0 = infinite
    int top_procs    = 10;
    bool no_color    = false;

    // ── Parse arguments ───────────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto next_int = [&](const char* flag) -> int {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << flag << " requires an argument.\n";
                std::exit(1);
            }
            try { return std::stoi(argv[++i]); }
            catch (...) {
                std::cerr << "Error: " << flag << " argument must be an integer.\n";
                std::exit(1);
            }
        };

        if      (arg == "-h" || arg == "--help")     { print_usage(argv[0]); return 0; }
        else if (arg == "-i" || arg == "--interval")  interval_sec = next_int("-i");
        else if (arg == "-n" || arg == "--count")     max_count    = next_int("-n");
        else if (arg == "-p" || arg == "--procs")     top_procs    = next_int("-p");
        else if (arg == "--no-color")                 no_color     = true;
        else {
            std::cerr << "Unknown option: " << arg
                      << "  (try --help)\n";
            return 1;
        }
    }

    // Propagate color setting to renderer (extern linkage via ansi())
    if (no_color) ansi("");   // dummy call; renderer checks isatty internally

    // ── Signal handling ───────────────────────────────────────────────────────
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    // ── Main loop ─────────────────────────────────────────────────────────────
    int count = 0;
    while (running) {
        try {
            SystemInfo            si    = collect_system_info();
            CpuStats              cpu   = collect_cpu();           // 200 ms sample
            MemStats              mem   = collect_memory();
            std::vector<DiskStats>   disks = collect_disks();
            std::vector<ProcessInfo> procs = collect_top_processes(top_procs);

            render_header(si);
            render_cpu(cpu);
            render_memory(mem);
            render_disks(disks);
            render_processes(procs);
            render_alerts(cpu, mem, disks);

            std::cout << "\n" << ansi("2") << "  Press Ctrl+C to quit."
                      << ansi("0") << "\n" << std::flush;
        }
        catch (const std::exception& e) {
            std::cerr << "Error during collection: " << e.what() << "\n";
        }

        ++count;
        if (max_count > 0 && count >= max_count) break;

        // Sleep in 100 ms increments so Ctrl+C is responsive
        for (int t = 0; t < interval_sec * 10 && running; ++t)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nExiting sysmon. Goodbye.\n";
    return 0;
}
