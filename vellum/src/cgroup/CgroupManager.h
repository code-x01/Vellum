#pragma once
#include <string>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace vellum {

class CgroupManager {
public:
    explicit CgroupManager(const std::string& vmId);
    ~CgroupManager();

    bool createCgroup();                     // creates /sys/fs/cgroup/vellum/<vmId>
    bool setMemoryLimit(uint64_t bytes);     // memory.max
    bool setCpuQuota(int micro_seconds);     // cpu.max
    bool setCpuPeriod(uint64_t micro_seconds);
    bool addProcess(pid_t pid);              // adds vCPU and vmm threads
    bool cleanup();                          // remove cgroup on VM stop

private:
    std::string vmId_;
    std::filesystem::path cgroupPath_;
    bool created_ = false;

    bool writeControlFile(const std::string& filename, const std::string& value);
};

} // namespace vellum