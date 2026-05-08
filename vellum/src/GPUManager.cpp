#include "GPUManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

namespace fs = std::filesystem;

namespace Vellum {

// Global GPU manager instance
std::unique_ptr<GPUManager> g_gpu_manager;

GPUManager::GPUManager() {
    loadGPUDevices();
}

GPUManager::~GPUManager() {
    // Cleanup any attached GPUs on shutdown
    std::lock_guard<std::mutex> lock(gpu_mutex_);
    for (auto& [vm_id, gpu_ids] : vm_gpu_attachments_) {
        for (const auto& gpu_id : gpu_ids) {
            detachGPUFromVM(vm_id, gpu_id);
        }
    }
}

std::vector<GPUDevice> GPUManager::enumerateGPUs() {
    std::lock_guard<std::mutex> lock(gpu_mutex_);
    loadGPUDevices(); // Refresh device list

    std::vector<GPUDevice> devices;
    for (const auto& [id, device] : gpu_devices_) {
        devices.push_back(device);
    }
    return devices;
}

std::optional<GPUDevice> GPUManager::getGPUById(const std::string& gpu_id) {
    std::lock_guard<std::mutex> lock(gpu_mutex_);
    auto it = gpu_devices_.find(gpu_id);
    if (it != gpu_devices_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool GPUManager::isGPUSupported(const std::string& gpu_id) {
    auto device = getGPUById(gpu_id);
    if (!device) return false;

    // Check if device supports passthrough
    return device->supports_passthrough;
}

bool GPUManager::attachGPUToVM(const std::string& vm_id, const GPUConfig& config) {
    std::lock_guard<std::mutex> lock(gpu_mutex_);

    // Validate GPU exists and is available
    auto device_opt = getGPUById(config.gpu_id);
    if (!device_opt) {
        std::cerr << "GPU " << config.gpu_id << " not found" << std::endl;
        return false;
    }

    auto& device = *device_opt;
    if (device.currently_attached) {
        std::cerr << "GPU " << config.gpu_id << " already attached to VM " << device.vm_id.value_or("unknown") << std::endl;
        return false;
    }

    // Setup VFIO for the GPU
    if (!setupVFIOForGPU(config.gpu_id)) {
        std::cerr << "Failed to setup VFIO for GPU " << config.gpu_id << std::endl;
        return false;
    }

    // Mark GPU as attached
    device.currently_attached = true;
    device.vm_id = vm_id;
    gpu_devices_[config.gpu_id] = device;

    // Track attachment
    vm_gpu_attachments_[vm_id].push_back(config.gpu_id);

    std::cout << "Successfully attached GPU " << config.gpu_id << " to VM " << vm_id << std::endl;
    return true;
}

bool GPUManager::detachGPUFromVM(const std::string& vm_id, const std::string& gpu_id) {
    std::lock_guard<std::mutex> lock(gpu_mutex_);

    // Find the GPU device
    auto device_opt = getGPUById(gpu_id);
    if (!device_opt) {
        std::cerr << "GPU " << gpu_id << " not found" << std::endl;
        return false;
    }

    auto& device = *device_opt;
    if (!device.currently_attached || device.vm_id != vm_id) {
        std::cerr << "GPU " << gpu_id << " not attached to VM " << vm_id << std::endl;
        return false;
    }

    // Unbind from VFIO
    if (!unbindGPUFromVFIO(gpu_id)) {
        std::cerr << "Warning: Failed to unbind GPU " << gpu_id << " from VFIO" << std::endl;
    }

    // Mark GPU as detached
    device.currently_attached = false;
    device.vm_id = std::nullopt;
    gpu_devices_[gpu_id] = device;

    // Remove from VM attachments
    auto& vm_gpus = vm_gpu_attachments_[vm_id];
    vm_gpus.erase(std::remove(vm_gpus.begin(), vm_gpus.end(), gpu_id), vm_gpus.end());

    std::cout << "Successfully detached GPU " << gpu_id << " from VM " << vm_id << std::endl;
    return true;
}

std::vector<std::string> GPUManager::getAttachedGPUs(const std::string& vm_id) {
    std::lock_guard<std::mutex> lock(gpu_mutex_);
    auto it = vm_gpu_attachments_.find(vm_id);
    if (it != vm_gpu_attachments_.end()) {
        return it->second;
    }
    return {};
}

bool GPUManager::isGPUAvailable(const std::string& gpu_id) {
    auto device = getGPUById(gpu_id);
    if (!device) return false;
    return !device->currently_attached;
}

std::optional<GPUMetrics> GPUManager::getGPUMetrics(const std::string& gpu_id) {
    std::lock_guard<std::mutex> lock(gpu_mutex_);

    auto device = getGPUById(gpu_id);
    if (!device) return std::nullopt;

    GPUMetrics metrics;
    metrics.gpu_id = gpu_id;

    if (!readGPUMetricsFromSysfs(gpu_id, metrics)) {
        // Try alternative methods based on vendor
        if (device->vendor == GPUVendor::NVIDIA) {
            // Could integrate with nvidia-ml library here
            std::cerr << "NVIDIA GPU metrics not implemented yet" << std::endl;
            return std::nullopt;
        } else if (device->vendor == GPUVendor::AMD) {
            // Could integrate with AMD GPU metrics
            std::cerr << "AMD GPU metrics not implemented yet" << std::endl;
            return std::nullopt;
        }
    }

    return metrics;
}

std::vector<GPUMetrics> GPUManager::getAllGPUMetrics() {
    std::vector<GPUMetrics> all_metrics;
    for (const auto& [gpu_id, device] : gpu_devices_) {
        auto metrics = getGPUMetrics(gpu_id);
        if (metrics) {
            all_metrics.push_back(*metrics);
        }
    }
    return all_metrics;
}

bool GPUManager::validateGPUDriver(const std::string& gpu_id) {
    auto device = getGPUById(gpu_id);
    if (!device) return false;

    std::string driver = getGPUDriver(gpu_id);
    if (driver.empty()) return false;

    // For passthrough, we want vfio-pci driver
    if (device->currently_attached) {
        return driver == "vfio-pci";
    }

    // For available GPUs, check vendor-specific drivers
    if (device->vendor == GPUVendor::NVIDIA) {
        return driver == "nvidia" || driver == "nouveau";
    } else if (device->vendor == GPUVendor::AMD) {
        return driver == "amdgpu" || driver == "radeon";
    }

    return true;
}

bool GPUManager::switchGPUDriver(const std::string& gpu_id, const std::string& driver) {
    // This would require unbinding and rebinding the device
    // Implementation would depend on the specific driver management system
    std::cerr << "GPU driver switching not implemented yet" << std::endl;
    return false;
}

bool GPUManager::checkIOMMUEnabled() {
    std::ifstream cmdline("/proc/cmdline");
    std::string line;
    if (std::getline(cmdline, line)) {
        return line.find("iommu=pt") != std::string::npos ||
               line.find("intel_iommu=on") != std::string::npos ||
               line.find("amd_iommu=on") != std::string::npos;
    }
    return false;
}

bool GPUManager::enableIOMMU() {
    // IOMMU needs to be enabled in kernel parameters
    // This would require a system reboot, so we just check status
    return checkIOMMUEnabled();
}

bool GPUManager::setupVFIOForGPU(const std::string& gpu_id) {
    // Check if VFIO is available
    if (access("/dev/vfio/vfio", F_OK) != 0) {
        std::cerr << "VFIO not available, trying to load vfio-pci module" << std::endl;
        system("modprobe vfio-pci");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (access("/dev/vfio/vfio", F_OK) != 0) {
        std::cerr << "VFIO not available" << std::endl;
        return false;
    }

    return bindGPUToVFIO(gpu_id);
}

bool GPUManager::createVGPUProfile(const std::string& gpu_id, const std::string& profile_name,
                                  uint32_t memory_mb, uint32_t max_instances) {
    // vGPU implementation would depend on vendor-specific tools
    // NVIDIA: nvidia-vgpu-mgr, AMD: amdgpu vGPU support
    std::cerr << "vGPU profile creation not implemented yet" << std::endl;
    return false;
}

bool GPUManager::destroyVGPUProfile(const std::string& gpu_id, const std::string& profile_name) {
    std::cerr << "vGPU profile destruction not implemented yet" << std::endl;
    return false;
}

std::vector<std::string> GPUManager::listVGPUProfiles(const std::string& gpu_id) {
    // Would query vendor-specific vGPU management tools
    return {};
}

// Private helper methods

bool GPUManager::loadGPUDevices() {
    gpu_devices_.clear();

    // Scan PCI devices for GPUs
    const std::string pci_path = "/sys/bus/pci/devices/";

    try {
        for (const auto& entry : fs::directory_iterator(pci_path)) {
            std::string device_path = entry.path().string();
            std::string pci_id = entry.path().filename().string();

            GPUDevice device;
            if (parsePCIDevice(device_path, device)) {
                device.id = pci_id;
                gpu_devices_[pci_id] = device;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning PCI devices: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool GPUManager::parsePCIDevice(const std::string& device_path, GPUDevice& device) {
    try {
        // Read vendor ID
        std::ifstream vendor_file(device_path + "/vendor");
        if (!vendor_file) return false;
        std::string vendor_str;
        std::getline(vendor_file, vendor_str);
        device.vendor_id = vendor_str.substr(2); // Remove 0x prefix

        // Read device ID
        std::ifstream device_file(device_path + "/device");
        if (!device_file) return false;
        std::string device_str;
        std::getline(device_file, device_str);
        device.device_id = device_str.substr(2);

        // Determine vendor
        if (device.vendor_id == "10de") {
            device.vendor = GPUVendor::NVIDIA;
        } else if (device.vendor_id == "1002") {
            device.vendor = GPUVendor::AMD;
        } else if (device.vendor_id == "8086") {
            device.vendor = GPUVendor::INTEL;
        } else {
            device.vendor = GPUVendor::UNKNOWN;
        }

        // Check if it's a GPU (class 0x03)
        std::ifstream class_file(device_path + "/class");
        if (class_file) {
            std::string class_str;
            std::getline(class_file, class_str);
            if (class_str.substr(2, 2) != "03") {
                return false; // Not a display controller
            }
        }

        // Get device name from modalias or create generic name
        device.name = "GPU " + device.vendor_id + ":" + device.device_id;

        // Get current driver
        device.driver = getGPUDriver(device_path);

        // Basic passthrough support check (simplified)
        device.supports_passthrough = (device.vendor != GPUVendor::UNKNOWN);

        // Initialize attachment state
        device.currently_attached = false;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error parsing PCI device " << device_path << ": " << e.what() << std::endl;
        return false;
    }
}

bool GPUManager::bindGPUToVFIO(const std::string& gpu_id) {
    std::string vfio_id = "vfio-pci";
    std::string device_override = "/sys/bus/pci/devices/" + gpu_id + "/driver_override";

    // Set driver override
    std::ofstream override_file(device_override);
    if (!override_file) {
        std::cerr << "Cannot set driver override for " << gpu_id << std::endl;
        return false;
    }
    override_file << vfio_id << std::endl;
    override_file.close();

    // Unbind from current driver
    std::string unbind_path = "/sys/bus/pci/devices/" + gpu_id + "/driver/unbind";
    std::ofstream unbind_file(unbind_path);
    if (unbind_file) {
        unbind_file << gpu_id << std::endl;
    }

    // Bind to VFIO
    std::string bind_path = "/sys/bus/pci/drivers/" + vfio_id + "/bind";
    std::ofstream bind_file(bind_path);
    if (!bind_file) {
        std::cerr << "Cannot bind " << gpu_id << " to VFIO" << std::endl;
        return false;
    }
    bind_file << gpu_id << std::endl;

    return true;
}

bool GPUManager::unbindGPUFromVFIO(const std::string& gpu_id) {
    std::string vfio_id = "vfio-pci";
    std::string unbind_path = "/sys/bus/pci/drivers/" + vfio_id + "/unbind";

    std::ofstream unbind_file(unbind_path);
    if (unbind_file) {
        unbind_file << gpu_id << std::endl;
        return true;
    }

    return false;
}

std::string GPUManager::getGPUDriver(const std::string& device_path) {
    std::string driver_path = device_path + "/driver";
    if (fs::exists(driver_path) && fs::is_symlink(driver_path)) {
        return fs::read_symlink(driver_path).filename().string();
    }
    return "";
}

bool GPUManager::readGPUMetricsFromSysfs(const std::string& gpu_id, GPUMetrics& metrics) {
    // This is a simplified implementation
    // Real implementation would read from hwmon, drm, or vendor-specific interfaces

    std::string hwmon_path = "/sys/bus/pci/devices/" + gpu_id + "/hwmon/";

    try {
        if (fs::exists(hwmon_path)) {
            for (const auto& entry : fs::directory_iterator(hwmon_path)) {
                if (entry.is_directory()) {
                    std::string hwmon_dir = entry.path().string();

                    // Try to read temperature
                    std::ifstream temp_file(hwmon_dir + "/temp1_input");
                    if (temp_file) {
                        int temp_millidegrees;
                        temp_file >> temp_millidegrees;
                        metrics.temperature_celsius = temp_millidegrees / 1000.0;
                    }

                    // Try to read fan speed
                    std::ifstream fan_file(hwmon_dir + "/fan1_input");
                    if (fan_file) {
                        int fan_rpm;
                        fan_file >> fan_rpm;
                        // Convert RPM to percentage (rough estimate)
                        metrics.fan_speed_percent = std::min(100.0, fan_rpm / 3000.0 * 100.0);
                    }

                    break;
                }
            }
        }

        // Set some default/placeholder values
        metrics.utilization_percent = 0.0; // Would need vendor-specific tools
        metrics.memory_utilization_percent = 0.0;
        metrics.memory_used_mb = 0;
        metrics.power_draw_watts = 0;
        metrics.clock_speed_mhz = 0;

        // Get total memory from device info
        auto device = getGPUById(gpu_id);
        if (device) {
            metrics.memory_total_mb = device->memory_mb;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error reading GPU metrics: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Vellum