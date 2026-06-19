# 🖥 sysmon — Linux System Health Monitor

A lightweight, real-time system monitoring tool written in **C++17**.  
Reads directly from the Linux `/proc` filesystem — no external dependencies.

```
╔══════════════════════════════════════════════════════╗
║  🖥  Linux System Health Monitor  v1.0               ║
╚══════════════════════════════════════════════════════╝
  Host   : my-server  │  OS: Ubuntu 24.04 LTS
  Kernel : Linux 6.8.0-41-generic  │  CPUs: 8
  Uptime : 3d 12:44:07  │  Load: 0.45 0.61 0.58
  Time   : 2025-06-18 14:32:01

┌─ CPU ──────────────────────────────────────────────────
  Usage  [████████░░░░░░░░░░░░░░░░░░░░░░]  27%

┌─ Memory ───────────────────────────────────────────────
  RAM    [████████████████░░░░░░░░░░░░░░]  53%  (8.4 GiB / 15.9 GiB)
         Buffers: 512.0 MiB   Cached: 3.1 GiB

┌─ Disk ─────────────────────────────────────────────────
  /                 [████████████████░░░░░░░░]  64%  (118.2 GiB / 183.7 GiB)
  /boot             [████░░░░░░░░░░░░░░░░░░░░]  18%  (85.3 MiB / 487.5 MiB)

┌─ Top Processes  (by memory) ───────────────────────────
       PID  Name                  State  RSS
       823  postgres              sleep     412.0 MiB
      1204  node                  sleep     238.5 MiB
       556  python3               run       194.2 MiB

┌─ ✓  Alerts ────────────────────────────────────────────
  All systems nominal.
```

---

## Features

- **CPU usage** — sampled over a 200 ms delta for accuracy
- **Memory** — total, used, available, buffers, and cached
- **Disk** — all real mounted filesystems with used/free/total
- **Top processes** — ranked by RSS memory usage, with PID, name, and state
- **Live alerts** — automatic warnings when CPU > 90%, RAM > 90%, or disk > 85%
- **ANSI colour** — colour-coded bars (green → yellow → red) with `--no-color` fallback
- **No external libraries** — reads `/proc` and uses only POSIX/glibc

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Linux       | Any kernel with `/proc` (2.6+) |
| GCC / Clang | C++17 support |
| `make`      | Any version |

> Tested on Ubuntu 22.04, Ubuntu 24.04, Debian 12, Arch Linux.

---

## Build & Run

```bash
# Clone
git clone https://github.com/YOUR_USERNAME/sysmon.git
cd sysmon

# Build (release)
make

# Run
./build/sysmon

# Options
./build/sysmon --help
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `-i, --interval <sec>` | 3 | Refresh interval in seconds |
| `-n, --count <num>` | 0 (∞) | Exit after N snapshots |
| `-p, --procs <num>` | 10 | Number of top processes to show |
| `--no-color` | — | Disable ANSI colour (for piping/logging) |
| `-h, --help` | — | Show usage |

### Examples

```bash
# Live dashboard, refresh every 5 seconds
./build/sysmon -i 5

# Single snapshot, plain text, save to file
./build/sysmon -n 1 --no-color > snapshot.txt

# Show top 20 processes
./build/sysmon -p 20
```

### Install system-wide

```bash
sudo make install     # copies to /usr/local/bin/sysmon
sysmon                # run from anywhere
sudo make uninstall   # remove it
```

---

## Project Structure

```
sysmon/
├── include/
│   └── sysmon.h          # Structs, function declarations
├── src/
│   ├── main.cpp          # Entry point, arg parsing, main loop
│   ├── collectors.cpp    # /proc readers: CPU, memory, disk, processes
│   └── renderer.cpp      # Terminal output with ANSI colour
├── Makefile
└── README.md
```

---

## How It Works

| Metric | Source |
|--------|--------|
| CPU usage | `/proc/stat` — two samples, 200 ms apart |
| Memory | `/proc/meminfo` |
| Disk | `/proc/mounts` + `statvfs(2)` syscall |
| Processes | `/proc/[pid]/stat` for every numeric PID directory |
| System info | `/proc/uptime`, `/proc/loadavg`, `uname(2)`, `/etc/os-release` |

---

## License

MIT — free to use, modify, and distribute.
