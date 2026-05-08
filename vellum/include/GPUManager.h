#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>

namespace Vellum {

// GPU Vendor enumeration
enum class GPUVendor {
    NVIDIA,
    AMD,
    INTEL,
    UNKNOWN
};

// GPU Device information
struct GPUDevice {
    std::string id;                    // Unique identifier (e.g., "0000:01:00.0")
    std::string name;                  // Human-readable name
    GPUVendor vendor;                  // GPU vendor
    std::string vendor_id;             // PCI vendor ID
    std::string device_id;             // PCI device ID
    uint64_t memory_mb;                // Total memory in MB
    bool supports_passthrough;         // Whether device supports IOMMU passthrough
    bool currently_attached;           // Whether currently attached to a VM
    std::optional<std::string> vm_id;  // ID of VM it's attached to (if any)
    std::string driver;                // Current driver (nvidia, amdgpu, etc.)
};

// GPU Metrics for monitoring
struct GPUMetrics {
    std::string gpu_id;
    double utilization_percent;        // GPU utilization (0-100)
    double memory_utilization_percent; // Memory utilization (0-100)
    uint64_t memory_used_mb;           // Memory used in MB
    uint64_t memory_total_mb;          // Total memory in MB
    double temperature_celsius;        // GPU temperature
    uint64_t power_draw_watts;         // Power consumption
    uint64_t fan_speed_percent;        // Fan speed (0-100)
    uint64_t clock_speed_mhz;          // Current clock speed
};

// GPU Configuration for VM attachment
struct GPUConfig {
    std::string gpu_id;                // GPU to attach
    bool enable_vgpu;                  // Enable virtual GPU sharing
    std::optional<uint32_t> vgpu_memory_mb; // Memory allocation for vGPU
    std::optional<uint32_t> vgpu_profiles;  // Number of vGPU profiles
};

// GPU Manager class for handling GPU operations
class GPUManager {
public:
    GPUManager();
    ~GPUManager();

    // GPU Discovery and Enumeration
    std::vector<GPUDevice> enumerateGPUs();
    std::optional<GPUDevice> getGPUById(const std::string& gpu_id);
    bool isGPUSupported(const std::string& gpu_id);

    // GPU Attachment/Detachment
    bool attachGPUToVM(const std::string& vm_id, const GPUConfig& config);
    bool detachGPUFromVM(const std::string& vm_id, const std::string& gpu_id);

    // GPU State Management
    std::vector<std::string> getAttachedGPUs(const std::string& vm_id);
    bool isGPUAvailable(const std::string& gpu_id);

    // GPU Metrics and Monitoring
    std::optional<GPUMetrics> getGPUMetrics(const std::string& gpu_id);
    std::vector<GPUMetrics> getAllGPUMetrics();

    // GPU Driver Management
    bool validateGPUDriver(const std::string& gpu_id);
    bool switchGPUDriver(const std::string& gpu_id, const std::string& driver);

    // IOMMU/VFIO Setup
    bool checkIOMMUEnabled();
    bool enableIOMMU();
    bool setupVFIOForGPU(const std::string& gpu_id);

    // vGPU Management (for sharing)
    bool createVGPUProfile(const std::string& gpu_id, const std::string& profile_name,
                          uint32_t memory_mb, uint32_t max_instances);
    bool destroyVGPUProfile(const std::string& gpu_id, const std::string& profile_name);
    std::vector<std::string> listVGPUProfiles(const std::string& gpu_id);

private:
    // Internal helper methods
    bool loadGPUDevices();
    bool parsePCIDevice(const std::string& pci_path, GPUDevice& device);
    bool bindGPUToVFIO(const std::string& gpu_id);
    bool unbindGPUFromVFIO(const std::string& gpu_id);
    std::string getGPUDriver(const std::string& gpu_id);
    bool readGPUMetricsFromSysfs(const std::string& gpu_id, GPUMetrics& metrics);

    // GPU device cache
    std::unordered_map<std::string, GPUDevice> gpu_devices_;
    std::unordered_map<std::string, std::vector<std::string>> vm_gpu_attachments_;

    // Mutex for thread safety
    std::mutex gpu_mutex_;
};

// Global GPU manager instance
extern std::unique_ptr<GPUManager> g_gpu_manager;

} // namespace Vellum