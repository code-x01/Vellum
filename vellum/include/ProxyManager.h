#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

struct ProxyChainEntry {
    std::string type;
    std::string host;
    uint16_t port = 0;
    std::optional<std::string> username;
    std::optional<std::string> password;
};

struct ProxyConfig {
    bool dynamic_chain = false;
    bool strict_chain = true;
    bool proxy_dns = true;
    bool quiet_mode = false;
    std::vector<ProxyChainEntry> proxies;
};

class ProxyManager {
public:
    ProxyManager();
    ~ProxyManager();

    bool isInstalled() const;
    std::string getBinaryPath() const;
    std::string getDefaultConfigPath() const;
    bool configExists() const;

    bool validateConfigFile(const std::string& path, std::string& err) const;
    std::optional<ProxyConfig> loadConfigFile(const std::string& path, std::string& err) const;
    bool writeConfigFile(const std::string& path, const ProxyConfig& config, std::string& err) const;

    bool runCommand(const std::vector<std::string>& command,
                    const std::string& config_path,
                    std::string& output,
                    int& exit_code,
                    std::string& err) const;

private:
    void detectBinary();
    bool fileExists(const std::string& path) const;
    std::string quoteArgument(const std::string& arg) const;

    std::string binary_path_;
};

extern std::unique_ptr<ProxyManager> g_proxy_manager;
