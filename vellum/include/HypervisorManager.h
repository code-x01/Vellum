#ifndef VELLUM_HYPERVISORMANAGER_H
#define VELLUM_HYPERVISORMANAGER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include "VMInstance.h"

class HypervisorManager {
public:
    static HypervisorManager& getInstance();

    // VM Management
    std::shared_ptr<VMInstance> createVM(const std::string& id, const std::string& kernelPath,
                                        const std::string& initrdPath = "", const std::string& diskPath = "",
                                        const std::string& kernelCmdline = "", size_t memoryMB = 256, int vcpus = 1);
    bool destroyVM(const std::string& id);
    std::shared_ptr<VMInstance> getVM(const std::string& id) const;
    std::vector<std::string> listVMs() const;
    void setVMConsoleCallback(const std::string& id, std::function<void(const std::string&)> callback);
    void setVMTelemetryCallback(const std::string& id, std::function<void(const VMInstance::Metrics&)> callback);

    // Global resource management
    struct GlobalMetrics {
        size_t totalMemoryMB;
        size_t usedMemoryMB;
        int totalVCPUs;
        int usedVCPUs;
    };
    GlobalMetrics getGlobalMetrics() const;

    // Cgroup management
    bool setupCgroups();
    bool assignVMToCgroup(const std::string& vmId, const std::string& cgroupPath);

private:
    HypervisorManager();
    ~HypervisorManager();
    HypervisorManager(const HypervisorManager&) = delete;
    HypervisorManager& operator=(const HypervisorManager&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<VMInstance>> vms_;

    // KVM global fd
    int kvm_fd_;

    bool initializeKVM();
};

#endif // VELLUM_HYPERVISORMANAGER_H