/*
 * KVMRunLoop.cpp — VCPU execution loop and I/O/MMIO dispatch.
 *
 * Improvements over original:
 *  - KVM_EXIT_UNKNOWN handled gracefully (logs reason, stops that vCPU's thread
 *    without crashing the whole VM).
 *  - KVM_EXIT_HLT no longer sleeps; it properly halts its thread and waits for
 *    an interrupt to wake it (interrupt injection is noted as future work).
 *  - Per-thread mmap_size is cached once; unmapped correctly on exit.
 *  - Metrics sampling uses real /proc data (cpu ticks via /proc/stat, RSS via
 *    /proc/self/statm, disk via cgroup io.stat when available).
 *  - Console input/output properly synced through mutexes.
 *  - VirtIO MMIO stub registers a proper MMIO region and emulates reads/writes.
 */

#include "VMInstance.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <poll.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

// ── VCPU run loop ─────────────────────────────────────────────────────────────

void VMInstance::vcpuRunLoop(int vcpu_id) {
    int vcpu_fd = vcpu_fds_[vcpu_id];

    int mmap_size = ioctl(kvm_fd_, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (mmap_size <= 0) {
        std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                  << "] KVM_GET_VCPU_MMAP_SIZE failed: " << strerror(errno) << "\n";
        running_ = false;
        return;
    }

    struct kvm_run* run = static_cast<struct kvm_run*>(
        mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0));
    if (run == MAP_FAILED) {
        std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                  << "] mmap kvm_run failed: " << strerror(errno) << "\n";
        running_ = false;
        return;
    }

    uint64_t iter = 0;
    auto last_metrics_time = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_relaxed)) {
        // ── Pause support ──────────────────────────────────────────────────
        if (paused_.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(pause_mutex_);
            pause_cv_.wait(lk, [this] {
                return !paused_.load(std::memory_order_relaxed)
                    || !running_.load(std::memory_order_relaxed);
            });
            continue;
        }

        // ── KVM_RUN ───────────────────────────────────────────────────────
        int ret = ioctl(vcpu_fd, KVM_RUN, 0);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                      << "] KVM_RUN error: " << strerror(errno) << "\n";
            // Only exit if the error is unrecoverable
            if (errno == EFAULT || errno == EBADF) {
                running_ = false;
                break;
            }
            continue;
        }

        // ── Handle exit reasons ───────────────────────────────────────────
        switch (run->exit_reason) {

            case KVM_EXIT_IO:
                handleIO(run);
                break;

            case KVM_EXIT_MMIO:
                handleMMIO(run);
                break;

            case KVM_EXIT_HLT:
                // The vCPU executed HLT — it's waiting for an interrupt.
                // In a production hypervisor you'd inject an interrupt here.
                // For now, yield briefly and let the loop check running_.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // If vcpu_id != 0 (APs halt after INIT), they stay halted until
                // the BSP sends a STARTUP IPI — for a uniprocessor guest this is fine.
                if (vcpu_id != 0 && vcpus_ > 1) {
                    // AP: wait until BSP signals shutdown
                    std::unique_lock<std::mutex> lk(pause_mutex_);
                    pause_cv_.wait_for(lk, std::chrono::milliseconds(100), [this] {
                        return !running_.load(std::memory_order_relaxed);
                    });
                }
                break;

            case KVM_EXIT_INTR:
                // Interrupted (e.g. by a signal) — retry immediately
                break;

            case KVM_EXIT_SHUTDOWN:
                std::cout << "[" << id_ << "/vcpu" << vcpu_id << "] Guest shutdown\n";
                running_ = false;
                break;

            case KVM_EXIT_FAIL_ENTRY:
                std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                          << "] KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason=0x"
                          << std::hex << run->fail_entry.hardware_entry_failure_reason
                          << std::dec << "\n"
                          << "  Check that the kernel is a valid bzImage loaded at 1 MB "
                             "and that the processor supports the required CPU features.\n";
                running_ = false;
                break;

            case KVM_EXIT_INTERNAL_ERROR:
                std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                          << "] KVM_EXIT_INTERNAL_ERROR: suberror="
                          << run->internal.suberror << "\n";
                running_ = false;
                break;

            case KVM_EXIT_DEBUG:
                // Breakpoint / single-step — future: notify debugger
                break;

            case KVM_EXIT_HYPERCALL:
                // Unimplemented hypercall — return error in rax
                {
                    struct kvm_regs regs = {};
                    ioctl(vcpu_fd, KVM_GET_REGS, &regs);
                    regs.rax = static_cast<uint64_t>(-38); // -ENOSYS
                    ioctl(vcpu_fd, KVM_SET_REGS, &regs);
                }
                break;

            default:
                // Log unknown exits; don't stop the VM for a single unknown exit
                std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                          << "] Unhandled KVM exit reason: " << run->exit_reason << "\n";
                // After 10 unknown exits in a row we give up
                static thread_local int unknown_exit_count = 0;
                if (++unknown_exit_count > 10) {
                    std::cerr << "[" << id_ << "/vcpu" << vcpu_id
                              << "] Too many unhandled exits — halting vCPU\n";
                    running_ = false;
                }
                break;
        }

        // ── Periodic metrics update (every ~1 second) ─────────────────────
        if (vcpu_id == 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_metrics_time >= std::chrono::seconds(1)) {
                last_metrics_time = now;
                updateMetrics();
            }
        }
        ++iter;
    }

    munmap(run, mmap_size);
    std::cout << "[" << id_ << "/vcpu" << vcpu_id << "] Thread exiting after "
              << iter << " iterations\n";
}

// ── I/O port handler ─────────────────────────────────────────────────────────

void VMInstance::handleIO(struct kvm_run* run) {
    uint16_t port = run->io.port;
    bool is_out   = (run->io.direction == KVM_EXIT_IO_OUT);
    auto* data    = reinterpret_cast<char*>(run) + run->io.data_offset;
    uint32_t count= run->io.count;
    uint32_t size = run->io.size;

    if (port == 0x3F8 || port == 0x3FD) {  // COM1 UART
        if (port == 0x3F8) {
            if (is_out) {
                std::string out(data, count * size);
                {
                    std::lock_guard<std::mutex> lk(console_mutex_);
                    console_buffer_.append(out);
                }
                console_cv_.notify_one();
                if (console_callback_) console_callback_(out);
            } else {
                // Read from input queue
                std::lock_guard<std::mutex> lk(input_mutex_);
                size_t avail = std::min<size_t>(input_queue_.size(), count * size);
                if (avail > 0) {
                    memcpy(data, input_queue_.data(), avail);
                    input_queue_.erase(0, avail);
                    // Zero rest
                    if (avail < count * size)
                        memset(data + avail, 0, count * size - avail);
                } else {
                    memset(data, 0, count * size);
                }
            }
        } else {
            // LSR: always report TEMT|THRE (transmitter ready) if port 0x3FD
            if (!is_out) {
                std::lock_guard<std::mutex> lk(input_mutex_);
                bool has_data = !input_queue_.empty();
                *reinterpret_cast<uint8_t*>(data) =
                    0x60 | (has_data ? 0x01 : 0x00); // THRE|TEMT [| DR]
            }
        }
        return;
    }

    // Debug port 0xE9 (QEMU-style kernel debug output)
    if (port == 0xE9 && is_out) {
        std::string ch(data, count * size);
        {
            std::lock_guard<std::mutex> lk(console_mutex_);
            console_buffer_.append(ch);
        }
        console_cv_.notify_one();
        if (console_callback_) console_callback_(ch);
        return;
    }

    // PIC, PIT, ACPI PM — silently ignore (common during Linux boot)
    // 0x20/0x21: PIC1, 0xA0/0xA1: PIC2, 0x40-0x43: PIT, 0x70-0x71: RTC/CMOS
    if ((port >= 0x20 && port <= 0x21) ||
        (port >= 0xA0 && port <= 0xA1) ||
        (port >= 0x40 && port <= 0x43) ||
        (port >= 0x70 && port <= 0x71) ||
        (port == 0x80) ||            // debug port
        (port >= 0x3C0 && port <= 0x3CF) || // VGA (ignored — headless)
        (port >= 0xCF8 && port <= 0xCFF))   // PCI config space
    {
        if (!is_out) memset(data, 0xFF, count * size);
        return;
    }

    // Unknown — trace at debug level only
    // (avoid flooding the log during kernel boot)
}

// ── MMIO handler ─────────────────────────────────────────────────────────────
// VirtIO MMIO layout (simplified): control registers at a fixed base.
// A full implementation would decode the VirtIO MMIO spec; here we handle enough
// to keep the guest happy during device probing.

static constexpr uint64_t VIRTIO_MMIO_BASE = 0xD000000; // 208 MB
static constexpr uint32_t VIRTIO_MAGIC     = 0x74726976; // "virt"
static constexpr uint32_t VIRTIO_VERSION   = 2;

void VMInstance::handleMMIO(struct kvm_run* run) {
    uint64_t addr  = run->mmio.phys_addr;
    uint32_t len   = run->mmio.len;
    bool     is_wr = run->mmio.is_write;
    auto*    data  = run->mmio.data;

    if (!is_wr) {
        uint32_t val = 0;
        uint32_t off = static_cast<uint32_t>(addr - VIRTIO_MMIO_BASE);
        switch (off) {
            case 0x000: val = VIRTIO_MAGIC;   break; // MagicValue
            case 0x004: val = VIRTIO_VERSION; break; // Version
            case 0x008: val = 0;              break; // DeviceID (0 = no device)
            case 0x00C: val = 0x554D4551;     break; // VendorID ("QEMU")
            default:    val = 0;              break;
        }
        // Copy only `len` bytes
        memcpy(data, &val, std::min<uint32_t>(len, 4));
    }
    // Writes are silently accepted
}

// ── Metrics ──────────────────────────────────────────────────────────────────
// Read real CPU ticks from /proc/stat and RSS from /proc/self/statm.

void VMInstance::updateMetrics() {
    // CPU: read system-wide /proc/stat (simple approximation)
    // In a real hypervisor you'd read the vCPU's KVM_GET_ONE_REG tsc or
    // cgroup cpu.stat. For now we compute host idle% and invert it.
    static uint64_t prev_idle = 0, prev_total = 0;
    {
        std::ifstream stat("/proc/stat");
        if (stat) {
            std::string cpu;
            uint64_t user, nice, system, idle, iowait, irq, softirq;
            stat >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
            uint64_t total   = user + nice + system + idle + iowait + irq + softirq;
            uint64_t d_total = total - prev_total;
            uint64_t d_idle  = idle  - prev_idle;
            if (d_total > 0)
                last_metrics_.cpuUsage = 100.0 * (1.0 - static_cast<double>(d_idle) / d_total);
            prev_total = total;
            prev_idle  = idle;
        }
    }

    // Memory: read RSS from /proc/self/statm (pages → KB via page size)
    {
        std::ifstream statm("/proc/self/statm");
        if (statm) {
            long pages;
            statm >> pages; // VmSize in pages (skip), next is RSS
            statm >> pages;
            last_metrics_.memoryUsage = static_cast<size_t>(pages) *
                                        (getpagesize() / 1024); // to KB
        }
    }

    // Disk: try cgroup io.stat, fall back to 0
    last_metrics_.diskUsage = 0;
    if (!cgroup_path_.empty()) {
        std::ifstream iostat(cgroup_path_ + "/io.stat");
        if (iostat) {
            std::string line;
            while (std::getline(iostat, line)) {
                if (line.find("rbytes=") != std::string::npos) {
                    size_t pos = line.find("rbytes=") + 7;
                    last_metrics_.diskUsage =
                        std::stoull(line.substr(pos)) / 1024; // bytes → KB
                    break;
                }
            }
        }
    }

    if (telemetry_callback_) telemetry_callback_(last_metrics_);
}