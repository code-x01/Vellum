#include "HypervisorManager.h"
#include <iostream>

HypervisorManager& HypervisorManager::getInstance() {
    static HypervisorManager instance;
    return instance;
}

HypervisorManager::HypervisorManager() : kvm_fd_(-1) {
    if (!initializeKVM()) {
        std::cerr << "Failed to initialize KVM" << std::endl;
    }
}

HypervisorManager::~HypervisorManager() {
    vms_.clear();
    if (kvm_fd_ >= 0) close(kvm_fd_);
}

bool HypervisorManager::initializeKVM() {
    kvm_fd_ = open("/dev/kvm", O_RDWR);
    return kvm_fd_ >= 0;
}

std::shared_ptr<VMInstance> HypervisorManager::createVM(const std::string& id, const std::string& kernelPath,
                                                        const std::string& initrdPath, const std::string& diskPath,
                                                        const std::string& kernelCmdline, size_t memoryMB, int vcpus) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (vms_.find(id) != vms_.end()) {
        return nullptr; // VM already exists
    }

    auto vm = std::make_shared<VMInstance>(id, kernelPath, initrdPath, diskPath, kernelCmdline, memoryMB, vcpus);
    // Set console callback if available
    // This would be set by APIServer
    vms_[id] = vm;
    return vm;
}

bool HypervisorManager::destroyVM(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = vms_.find(id);
    if (it == vms_.end()) return false;

    it->second->stop();
    vms_.erase(it);
    return true;
}

std::shared_ptr<VMInstance> HypervisorManager::getVM(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = vms_.find(id);
    return it != vms_.end() ? it->second : nullptr;
}

std::vector<std::string> HypervisorManager::listVMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    for (const auto& pair : vms_) {
        ids.push_back(pair.first);
    }
    return ids;
}

HypervisorManager::GlobalMetrics HypervisorManager::getGlobalMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    GlobalMetrics metrics = {0, 0, 0, 0};
    for (const auto& pair : vms_) {
        auto vm = pair.second;
        metrics.totalMemoryMB += vm->getMemoryMB();
        metrics.totalVCPUs += vm->getVCPUs();
        if (vm->getState() == VMInstance::State::Running) {
            auto vm_metrics = vm->getMetrics();
            metrics.usedMemoryMB += vm_metrics.memoryUsage / 1024; // Convert KB to MB
            metrics.usedVCPUs += vm->getVCPUs(); // Simplified
        }
    }
    return metrics;
}

bool HypervisorManager::setupCgroups() {
    // Setup cgroups for resource management
    // Implementation needed
    return true;
}

void HypervisorManager::setVMConsoleCallback(const std::string& id, std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = vms_.find(id);
    if (it != vms_.end()) {
        it->second->setConsoleCallback(callback);
    }
}

void HypervisorManager::setVMTelemetryCallback(const std::string& id, std::function<void(const VMInstance::Metrics&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = vms_.find(id);
    if (it != vms_.end()) {
        it->second->setTelemetryCallback(callback);
    }
}