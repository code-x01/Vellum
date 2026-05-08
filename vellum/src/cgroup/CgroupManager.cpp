#include "CgroupManager.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

namespace vellum {

CgroupManager::CgroupManager(const std::string& vmId) : vmId_(vmId) {
    cgroupPath_ = std::filesystem::path("/sys/fs/cgroup") / "vellum" / vmId;
}

CgroupManager::~CgroupManager() {
    cleanup();
}

bool CgroupManager::createCgroup() {
    if (created_) return true;
    std::error_code ec;
    std::filesystem::create_directories(cgroupPath_, ec);
    if (ec) {
        std::cerr << "Failed to create cgroup dir " << cgroupPath_ << ": " << ec.message() << std::endl;
        return false;
    }
    created_ = true;
    return true;
}

bool CgroupManager::writeControlFile(const std::string& filename, const std::string& value) {
    if (!created_) return false;
    auto path = cgroupPath_ / filename;
    std::ofstream f(path);
    if (!f) return false;
    f << value;
    return f.good();
}

bool CgroupManager::setMemoryLimit(uint64_t bytes) {
    return writeControlFile("memory.max", std::to_string(bytes));
}

bool CgroupManager::setCpuQuota(int micro_seconds) {
    // cpu.max format: "$quota $period"
    std::string val = std::to_string(micro_seconds) + " 1000000";
    return writeControlFile("cpu.max", val);
}

bool CgroupManager::setCpuPeriod(uint64_t micro_seconds) {
    // In cgroup v2, period is part of cpu.max; we can ignore separate setter for simplicity
    // but we implement as a no‑op that returns true for API completeness.
    return true;
}

bool CgroupManager::addProcess(pid_t pid) {
    if (!created_) return false;
    auto procsPath = cgroupPath_ / "cgroup.procs";
    std::ofstream f(procsPath);
    if (!f) return false;
    f << pid;
    return f.good();
}

bool CgroupManager::cleanup() {
    if (!created_) return true;
    // Remove the cgroup directory (requires all processes moved out)
    std::error_code ec;
    std::filesystem::remove_all(cgroupPath_, ec);
    created_ = false;
    return !ec;
}

} // namespace vellum