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
#include <linux/kvm.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

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

    // ── Internal helpers ─────────────────────────────────────────────────────
    bool initializeKVM();
    bool loadKernel();
    bool setupVirtio();
    bool setupVCPUs();
    void vcpuRunLoop(int vcpu_id);
    void handleIO(struct kvm_run* run);
    void handleMMIO(struct kvm_run* run);
    void updateMetrics();

    friend class HypervisorManager;
};

#endif // VELLUM_VMINSTANCE_H