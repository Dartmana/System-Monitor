# System Monitor

A college project I built to monitor Linux system health in real time.

The purpose of this project was to learn how Linux exposes system information through the `/proc` filesystem and display it in a clean terminal interface. It shows CPU usage, memory, disk space, and top processes — all without any external libraries.

## Requirements

- Linux
- g++ with C++17 support
- make

## Install

```bash
git clone https://github.com/Dartmana/System-Monitor.git
cd System-Monitor
make
sudo make install
```

Then run it from anywhere:

```bash
sysmon
```

## Uninstall

```bash
sudo make uninstall
```
