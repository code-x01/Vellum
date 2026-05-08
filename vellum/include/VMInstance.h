#ifndef VELLUM_VMINSTANCE_H
#define VELLUM_VMINSTANCE_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <linux/kvm.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>

// Forward declarations
class HypervisorManager;

class VMInstance {
public:
    enum class State { Stopped, Starting, Running, Paused, Error };

    VMInstance(const std::string& id, const std::string& kernelPath,
               const std::string& initrdPath = "", const std::string& diskPath = "",
               const std::string& kernelCmdline = "",
               size_t memoryMB = 256, int vcpus = 1);
    ~VMInstance();

    // Metrics
    struct Metrics {
        double cpuUsage;     // Host CPU percentage (approximation)
        size_t memoryUsage;  // KB
        size_t diskUsage;    // KB
    };
    Metrics getMetrics() const;

    // Callbacks – set by APIServer before starting
    void setConsoleCallback(std::function<void(const std::string&)> cb) {
        console_callback_ = std::move(cb);
    }
    void setTelemetryCallback(std::function<void(const VMInstance::Metrics&)> cb) {
        telemetry_callback_ = std::move(cb);
    }

    // Lifecycle
    bool start();
    bool stop();
    bool pause();
    bool resume();

    // Resource management (requires cgroup v2)
    bool setCPULimit(double percentage);
    bool setMemoryLimit(size_t mb);

    // Cgroup management (called internally; exposed so HypervisorManager can query)
    bool setupCgroup();
    void cleanupCgroup();
    std::string getCgroupPath() const { return cgroup_path_; }

    // Console I/O
    std::string readConsoleOutput();
    bool sendConsoleInput(const std::string& input);

    // Snapshot / CoW (stubs – future implementation)
    bool createSnapshot(const std::string& snapshotName);
    bool restoreSnapshot(const std::string& snapshotName);

    // ═════════════════════════════════════════════════════════════════════
    // ADVANCED VM MECHANICS - Enhanced functionality beyond basic lifecycle
    // ═════════════════════════════════════════════════════════════════════

    // ── Snapshot/CoW Support ─────────────────────────────────────────────
    bool cloneVM(const std::string& newId, const std::string& snapshotName);

    // ── Auto-restart/Recovery ───────────────────────────────────────────
    void enableAutoRestart(const AutoRestartConfig& config);
    void disableAutoRestart();

    // ── Live Migration ───────────────────────────────────────────────────
    bool startMigration(const std::string& destinationHost, int port = MIGRATION_PORT_BASE);

    // ── Enhanced Performance Profiling ───────────────────────────────────
    struct PerformanceProfile {
        double cpu_user_percent;
        double cpu_system_percent;
        double cpu_idle_percent;
        size_t memory_rss_kb;
        size_t memory_vsz_kb;
        size_t disk_read_bytes;
        size_t disk_write_bytes;
        double network_rx_bytes_per_sec;
        double network_tx_bytes_per_sec;
        std::chrono::steady_clock::time_point timestamp;
    };
    PerformanceProfile getDetailedPerformanceProfile();
    void startPerformanceMonitoring(std::function<void(const PerformanceProfile&)> callback);
    void stopPerformanceMonitoring();

    // ── Memory Ballooning ────────────────────────────────────────────────
    bool adjustMemory(size_t new_memory_mb);

    // ── CPU Hotplug ──────────────────────────────────────────────────────
    bool addVCPU();
    bool removeVCPU();

    // ── QoS/Throttling ───────────────────────────────────────────────────
    struct QoSConfig {
        int cpu_shares = 1024;
        int io_weight = 500;
        int network_priority = 0;
        size_t memory_soft_limit = 0;
    };
    bool configureQoS(const QoSConfig& config);

    // ═════════════════════════════════════════════════════════════════════
    // UPGRADE 1: VM NETWORKING – TAP/Bridge support for external connectivity
    // ═════════════════════════════════════════════════════════════════════
    struct NetworkConfig {
        std::string tapInterface;      // e.g., "tap0"
        std::string macAddress;        // e.g., "52:54:00:12:34:56"
        std::string ipAddress;         // Optional DHCP or static IP
        std::string gateway;           // Optional gateway
        std::string bridgeName;        // Bridge to attach to (default: "velbr0")
        bool dhcpEnabled = true;
        int mtu = 1500;
    };

    bool configureNetwork(const NetworkConfig& config);
    bool removeNetwork();
    NetworkConfig getNetworkConfig() const;

    // ═════════════════════════════════════════════════════════════════════
    // UPGRADE 2: PERSISTENCE – Save/load VM configuration & state
    // ═════════════════════════════════════════════════════════════════════
    std::string serializeConfig() const;
    bool deserializeConfig(const std::string& json);
    bool saveStateToDisk(const std::string& filePath) const;
    bool loadStateFromDisk(const std::string& filePath);

    // Getters
    std::string getId()     const { return id_; }
    State       getState()  const { return state_.load(std::memory_order_acquire); }
    size_t      getMemoryMB()  const { return memoryMB_; }
    int         getVCPUs()     const { return vcpus_; }

private:
    // ── Identity ────────────────────────────────────────────────────────────
    std::string id_;
    std::string kernelPath_;
    std::string initrdPath_;
    std::string diskPath_;
    std::string kernelCmdline_;
    size_t      memoryMB_;
    int         vcpus_;

    // ── State ───────────────────────────────────────────────────────────────
    std::atomic<State> state_;
    std::string cgroup_path_;

    // ── KVM handles ─────────────────────────────────────────────────────────
    int              kvm_fd_;
    int              vm_fd_;
    std::vector<int> vcpu_fds_;
    void*            guest_memory_;
    size_t           guest_memory_size_;

    // ── VCPU threads ────────────────────────────────────────────────────────
    std::vector<std::jthread> vcpu_threads_;
    std::atomic<bool>         running_;

    // ── Network configuration ────────────────────────────────────────────
    NetworkConfig network_config_;
    int           tap_fd_ = -1;
    bool          network_enabled_ = false;

    // ── Persistence support ─────────────────────────────────────────────
    mutable std::mutex config_mutex_;  // Protects serialization

    // ─────────────────────────────────────────────────────────────────────
    // PRIVATE HELPER METHODS FOR NETWORKING (Task 1)
    // ─────────────────────────────────────────────────────────────────────
    int createTAPDevice(const std::string& tapName);
    bool attachToBridge(const std::string& tapName, const std::string& bridgeName);
    bool bringUpInterface(const std::string& ifName);

    // ─────────────────────────────────────────────────────────────────────
    // PRIVATE HELPER METHODS FOR PERSISTENCE (Task 2)
    // ─────────────────────────────────────────────────────────────────────
    std::string stateToString(State s) const;

    // ─────────────────────────────────────────────────────────────────────
    // PRIVATE HELPER METHODS FOR CGROUP (Task 4)
    // ─────────────────────────────────────────────────────────────────────
    bool setupCgroupEnhanced();
    bool writeCgroupFile(const std::string& filename, const std::string& value);
    std::string readCgroupFile(const std::string& filename) const;

    // ── Console ─────────────────────────────────────────────────────────────
    int         console_fd_;
    std::string console_buffer_;
    std::mutex  console_mutex_;
    std::condition_variable console_cv_;
    std::string input_queue_;
    std::mutex  input_mutex_;
    std::function<void(const std::string&)>       console_callback_;
    std::function<void(const VMInstance::Metrics&)> telemetry_callback_;

    // ── Pause / resume ───────────────────────────────────────────────────────
    std::atomic<bool>       paused_;
    std::mutex              pause_mutex_;
    std::condition_variable pause_cv_;

    // ── Metrics cache ────────────────────────────────────────────────────────
    mutable Metrics last_metrics_;

    // ── Advanced VM Mechanics ────────────────────────────────────────────────
    struct AutoRestartConfig {
        bool enabled = false;
        int max_attempts = 3;
        std::chrono::seconds delay_between_attempts{30};
        std::chrono::seconds max_total_runtime{3600};
        std::vector<std::string> failure_conditions = {"KVM_RUN", "memory", "disk"};
    };

    // Auto-restart
    AutoRestartConfig auto_restart_config_;
    std::jthread auto_restart_thread_;

    // Migration
    std::jthread migration_thread_;

    // Performance monitoring
    std::function<void(const PerformanceProfile&)> performance_callback_;
    std::jthread performance_monitor_thread_;

    // QoS
    QoSConfig qos_config_;

    // ── Internal helpers ─────────────────────────────────────────────────────
    bool initializeKVM();
    bool loadKernel();
    bool setupVirtio();
    bool setupVCPUs();
    void vcpuRunLoop(int vcpu_id);
    void handleIO(struct kvm_run* run);
    void handleMMIO(struct kvm_run* run);
    void updateMetrics();

    // ── Advanced mechanics helpers ───────────────────────────────────────────
    bool createQcow2Overlay(const std::string& overlayPath, const std::string& backingPath);
    void startAutoRestartMonitor();
    bool shouldAttemptRestart(int attempts, std::chrono::steady_clock::time_point start_time);
    bool restart();
    void performMigration(const std::string& destinationHost, int port, std::stop_token stop_token);
    int createMigrationSocket(const std::string& host, int port);
    bool sendVMMetadata(int sock_fd);
    bool sendVMMemory(int sock_fd, std::stop_token stop_token);
    bool sendVMDevices(int sock_fd);
    bool finalizeMigration(int sock_fd);
    void getCPUStats(PerformanceProfile& profile);
    void getMemoryStats(PerformanceProfile& profile);
    void getDiskStats(PerformanceProfile& profile);
    void getNetworkStats(PerformanceProfile& profile);
    bool performMemoryBallooning(size_t new_memory_mb);
    bool performVCPUHotplug(int new_vcpu_count);
    bool setCPUShares(int shares);
    bool setIOWeight(int weight);
    bool setNetworkPriority(int priority);
    bool setMemorySoftLimit(size_t mb);

    friend class HypervisorManager;
};

#endif // VELLUM_VMINSTANCE_H