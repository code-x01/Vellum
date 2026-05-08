#ifndef VELLUM_HYPERVISORMANAGER_H
#define VELLUM_HYPERVISORMANAGER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "VMInstance.h"

class HypervisorManager {
public:
    static HypervisorManager& getInstance();

    // ═════════════════════════════════════════════════════════════════════
    // TASK 3: API AUTHENTICATION – JWT Token Management
    // ═════════════════════════════════════════════════════════════════════
    struct AuthToken {
        std::string token;
        std::chrono::system_clock::time_point expiresAt;
        std::string userId;
    };

    // User credentials (in production, use proper hashing & database)
    bool authenticateUser(const std::string& username, const std::string& password, AuthToken& outToken);
    bool validateToken(const std::string& token) const;
    bool refreshToken(const std::string& token, AuthToken& outToken);
    void revokeToken(const std::string& token);

    // ═════════════════════════════════════════════════════════════════════
    // TASK 2: PERSISTENCE – VM Configuration Save/Load
    // ═════════════════════════════════════════════════════════════════════
    bool saveAllVMConfigs(const std::string& filePath) const;
    bool loadAllVMConfigs(const std::string& filePath);
    bool saveVMConfig(const std::string& vmId, const std::string& filePath) const;
    bool loadVMConfig(const std::string& filePath);

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

    // ═════════════════════════════════════════════════════════════════════
    // AUTHENTICATION STATE (Task 3)
    // ═════════════════════════════════════════════════════════════════════
    std::map<std::string, AuthToken> active_tokens_;
    mutable std::mutex auth_mutex_;

    // Default credentials (change in production!)
    static constexpr const char* DEFAULT_USER = "admin";
    static constexpr const char* DEFAULT_PASS = "vellum2024";  // Change immediately!
    static constexpr int TOKEN_EXPIRY_SECONDS = 3600;  // 1 hour

    // ═════════════════════════════════════════════════════════════════════
    // PERSISTENCE STATE (Task 2)
    // ═════════════════════════════════════════════════════════════════════
    static constexpr const char* CONFIG_DIR = "/etc/vellum/configs";
    static constexpr const char* CONFIG_FILENAME = "vms.json";

    bool initializeKVM();
    std::string generateToken() const;
};

#endif // VELLUM_HYPERVISORMANAGER_H