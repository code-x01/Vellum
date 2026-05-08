#include "ProxyManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdio>
#include <array>
#include <unistd.h>

namespace fs = std::filesystem;

std::unique_ptr<ProxyManager> g_proxy_manager;

ProxyManager::ProxyManager() {
    detectBinary();
}

ProxyManager::~ProxyManager() = default;

bool ProxyManager::isInstalled() const {
    return !binary_path_.empty();
}

std::string ProxyManager::getBinaryPath() const {
    return binary_path_;
}

std::string ProxyManager::getDefaultConfigPath() const {
    const std::vector<std::string> candidates = {
        "/etc/vellum/proxychains.conf",
        "/etc/proxychains4.conf",
        "/etc/proxychains.conf"
    };
    for (const auto& path : candidates) {
        if (fileExists(path)) {
            return path;
        }
    }
    return std::string("/etc/vellum/proxychains.conf");
}

bool ProxyManager::configExists() const {
    return fileExists(getDefaultConfigPath());
}

bool ProxyManager::validateConfigFile(const std::string& path, std::string& err) const {
    if (!fileExists(path)) {
        err = "Proxychains config file does not exist: " + path;
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        err = "Failed to open proxychains config file: " + path;
        return false;
    }

    std::string line;
    bool hasProxyList = false;
    while (std::getline(file, line)) {
        if (line.rfind("[ProxyList]", 0) == 0) {
            hasProxyList = true;
            break;
        }
    }

    if (!hasProxyList) {
        err = "Invalid proxychains config: missing [ProxyList] section.";
        return false;
    }

    return true;
}

std::optional<ProxyConfig> ProxyManager::loadConfigFile(const std::string& path, std::string& err) const {
    if (!fileExists(path)) {
        err = "Config file does not exist: " + path;
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        err = "Unable to read proxychains config file: " + path;
        return std::nullopt;
    }

    ProxyConfig config;
    std::string line;
    bool inProxyList = false;
    while (std::getline(file, line)) {
        std::string trimmed;
        for (char ch : line) {
            if (ch != '\r' && ch != '\n') trimmed.push_back(ch);
        }
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        if (trimmed == "[ProxyList]") {
            inProxyList = true;
            continue;
        }

        if (!inProxyList) {
            if (trimmed == "dynamic_chain") {
                config.dynamic_chain = true;
                config.strict_chain = false;
            } else if (trimmed == "strict_chain") {
                config.strict_chain = true;
                config.dynamic_chain = false;
            } else if (trimmed == "proxy_dns") {
                config.proxy_dns = true;
            } else if (trimmed == "no_proxy_dns") {
                config.proxy_dns = false;
            } else if (trimmed == "quiet_mode") {
                config.quiet_mode = true;
            }
            continue;
        }

        std::istringstream iss(trimmed);
        ProxyChainEntry entry;
        if (!(iss >> entry.type >> entry.host >> entry.port)) {
            continue;
        }
        std::string user;
        if (iss >> user) {
            entry.username = user;
            std::string pass;
            if (iss >> pass) {
                entry.password = pass;
            }
        }
        config.proxies.push_back(std::move(entry));
    }

    if (config.proxies.empty()) {
        err = "No proxy entries found in config file: " + path;
        return std::nullopt;
    }

    return config;
}

bool ProxyManager::writeConfigFile(const std::string& path, const ProxyConfig& config, std::string& err) const {
    fs::path config_path(path);
    if (config_path.has_parent_path()) {
        fs::create_directories(config_path.parent_path());
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        err = "Failed to write proxychains config at: " + path;
        return false;
    }

    file << "# Vellum-generated proxychains configuration\n";
    if (config.dynamic_chain) {
        file << "dynamic_chain\n";
    } else {
        file << "strict_chain\n";
    }
    file << (config.proxy_dns ? "proxy_dns\n" : "no_proxy_dns\n");
    file << (config.quiet_mode ? "quiet_mode\n" : "verbose_mode\n");
    file << "tcp_read_time_out 15000\n";
    file << "tcp_connect_time_out 8000\n\n";
    file << "[ProxyList]\n";
    for (const auto& proxy : config.proxies) {
        file << proxy.type << " " << proxy.host << " " << proxy.port;
        if (proxy.username) {
            file << " " << *proxy.username;
            if (proxy.password) {
                file << " " << *proxy.password;
            }
        }
        file << "\n";
    }

    if (!file) {
        err = "Failed while writing proxychains config file.";
        return false;
    }
    return true;
}

bool ProxyManager::runCommand(const std::vector<std::string>& command,
                              const std::string& config_path,
                              std::string& output,
                              int& exit_code,
                              std::string& err) const {
    if (!isInstalled()) {
        err = "Proxychains is not installed on this system.";
        return false;
    }
    if (command.empty()) {
        err = "Command list cannot be empty.";
        return false;
    }

    std::string configFile = config_path.empty() ? getDefaultConfigPath() : config_path;
    if (!fileExists(configFile)) {
        err = "Proxychains config file does not exist: " + configFile;
        return false;
    }

    std::ostringstream cmdline;
    cmdline << '"' << binary_path_ << '"' << " -f '" << configFile << "'";
    for (const auto& arg : command) {
        cmdline << ' ' << quoteArgument(arg);
    }
    cmdline << " 2>&1";

    FILE* pipe = popen(cmdline.str().c_str(), "r");
    if (!pipe) {
        err = "Failed to execute proxychains command.";
        return false;
    }

    std::array<char, 512> buffer;
    output.clear();
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
    }

    return true;
}

void ProxyManager::detectBinary() {
    const std::vector<std::string> candidates = {"proxychains4", "proxychains"};
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        return;
    }

    std::vector<std::string> paths;
    std::istringstream env(path_env);
    std::string fragment;
    while (std::getline(env, fragment, ':')) {
        if (!fragment.empty()) {
            paths.push_back(fragment);
        }
    }

    for (const auto& binary : candidates) {
        for (const auto& dir : paths) {
            fs::path p = fs::path(dir) / binary;
            if (fs::exists(p) && access(p.c_str(), X_OK) == 0) {
                binary_path_ = p.string();
                return;
            }
        }
    }
}

bool ProxyManager::fileExists(const std::string& path) const {
    if (path.empty()) {
        return false;
    }
    return fs::exists(path);
}

std::string ProxyManager::quoteArgument(const std::string& arg) const {
    std::string quoted = "'";
    for (char ch : arg) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += "'";
    return quoted;
}
