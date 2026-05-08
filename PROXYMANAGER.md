# Vellum v2.0: ProxyManager Integration Guide

## 📋 Overview

The **ProxyManager** is a new C++ component in Vellum v2.0 that provides advanced proxychains management and runtime proxy command execution. It enables users to configure, validate, and orchestrate proxy chains directly from the REST API or CLI.

## 🎯 Features

### 1. **Binary Detection**
- Automatically detects installed proxychains binaries (`proxychains4` or `proxychains`)
- Searches system PATH for executable location
- Provides fallback detection methods

### 2. **Configuration Management**
- Load existing proxychains configuration files
- Parse and validate proxy chain entries
- Write new configurations programmatically
- Support for multiple proxy types (SOCKS5, SOCKS4, HTTP)
- Support for authenticated proxies (username/password)

### 3. **Runtime Execution**
- Execute arbitrary commands through the proxy chain
- Capture command output and exit codes
- Thread-safe operation with error handling
- Configurable proxy chain modes (dynamic, strict)

### 4. **Configuration Options**
```cpp
struct ProxyConfig {
    bool dynamic_chain = false;      // Dynamic proxy selection
    bool strict_chain = true;        // Fallback to next proxy on failure
    bool proxy_dns = true;           // Route DNS through proxy
    bool quiet_mode = false;         // Suppress proxychains output
    std::vector<ProxyChainEntry> proxies;
};
```

### 5. **Proxy Chain Entry**
```cpp
struct ProxyChainEntry {
    std::string type;                // socks5, socks4, http
    std::string host;                // proxy hostname/IP
    uint16_t port = 0;              // proxy port
    std::optional<std::string> username;
    std::optional<std::string> password;
};
```

## 📁 Files Added

```
include/ProxyManager.h               # Header file with class definition
src/ProxyManager.cpp                 # Implementation (~280 lines)
```

## 🔌 Integration Points

### 1. **Build System (CMakeLists.txt)**
- Added `src/ProxyManager.cpp` to SOURCES list
- No additional dependencies required (uses standard C++ library)

### 2. **Main Runtime (src/main.cpp)**
```cpp
// Initialize proxy manager on startup
g_proxy_manager = std::make_unique<ProxyManager>();
std::cout << "Proxy manager initialized: "
          << (g_proxy_manager->isInstalled() ? "installed" : "not installed")
          << " (" << g_proxy_manager->getBinaryPath() << ")" << std::endl;
```

### 3. **Global Instance**
```cpp
// Global proxy manager singleton available throughout the application
extern std::unique_ptr<ProxyManager> g_proxy_manager;
```

## 🌐 REST API Endpoints

### GET /api/proxy/status
Retrieve proxychains installation status and configuration paths.

**Response:**
```json
{
  "installed": true,
  "binaryPath": "/usr/bin/proxychains4",
  "defaultConfigPath": "/etc/vellum/proxychains.conf",
  "configExists": true
}
```

### GET /api/proxy/config
Load and return the current proxychains configuration.

**Response:**
```json
{
  "dynamicChain": false,
  "strictChain": true,
  "proxyDNS": true,
  "quietMode": false,
  "proxies": [
    {
      "type": "socks5",
      "host": "127.0.0.1",
      "port": 9050,
      "username": "",
      "password": ""
    }
  ]
}
```

### POST /api/proxy/config
Write a new proxychains configuration.

**Request:**
```json
{
  "dynamicChain": true,
  "strictChain": false,
  "proxyDNS": true,
  "quietMode": false,
  "proxies": [
    {
      "type": "socks5",
      "host": "proxy1.example.com",
      "port": 1080,
      "username": "user",
      "password": "pass"
    },
    {
      "type": "http",
      "host": "proxy2.example.com",
      "port": 3128
    }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "message": "Proxychains configuration saved."
}
```

### POST /api/proxy/run
Execute a command through the proxychains proxy.

**Request:**
```json
{
  "command": ["curl", "https://ifconfig.me"],
  "configPath": "/etc/vellum/proxychains.conf"
}
```

**Response:**
```json
{
  "exitCode": 0,
  "output": "203.0.113.42\n"
}
```

## 💻 CLI Usage

### Run Vellum with proxychains (shell scripts)
```bash
# Use default system proxychains config
./run.sh --proxychains

# Use custom config file
./run.sh --proxychains /path/to/custom/proxychains.conf

# Run frontend through proxy
./start-frontend.sh --proxychains
```

### Windows (WSL2)
```cmd
run.bat --proxychains
```

## 🔧 Implementation Details

### Key Methods

#### `isInstalled() const`
Returns whether proxychains binary was found on the system.

#### `getBinaryPath() const`
Returns the full path to the detected proxychains binary.

#### `getDefaultConfigPath() const`
Returns the default proxychains config path, checking:
1. `/etc/vellum/proxychains.conf`
2. `/etc/proxychains4.conf`
3. `/etc/proxychains.conf`

#### `loadConfigFile(const std::string& path, std::string& err) const`
Parses and loads a proxychains configuration file with validation.

#### `writeConfigFile(const std::string& path, const ProxyConfig& config, std::string& err) const`
Generates and writes a new proxychains configuration file with proper formatting.

#### `runCommand(const std::vector<std::string>& command, const std::string& config_path, std::string& output, int& exit_code, std::string& err) const`
Executes a command through proxychains, capturing output and exit code.

### Thread Safety
- All public methods use internal synchronization where needed
- Safe for concurrent API calls
- No global state modifications (only reads from ProxyConfig)

### Error Handling
```cpp
std::string err;
auto config = g_proxy_manager->loadConfigFile(path, err);
if (!config) {
    // Handle error: err contains reason
}
```

## 🚀 Use Cases

### 1. **Corporate Network Access**
Configure Vellum to route all VM management traffic through corporate proxies.

### 2. **Development Behind NAT**
Build and run Vellum in restricted network environments using proxy chains.

### 3. **Multi-Hop Security**
Route traffic through multiple proxies for enhanced anonymity/security.

### 4. **Automated Deployment**
Script proxy configuration changes for different network environments.

### 5. **Testing Proxy Behavior**
Run diagnostic commands through the proxy chain to test connectivity.

## 📊 Architecture Diagram

```
┌─────────────────────────────────────────┐
│         REST API Client                 │
│         (curl, React, etc.)             │
└──────────────┬──────────────────────────┘
               │
               │ HTTP/JSON
               ↓
┌─────────────────────────────────────────┐
│         APIServer::setupRoutes()        │
│  (/api/proxy/status, /config, /run)     │
└──────────────┬──────────────────────────┘
               │
               │ C++ Function Calls
               ↓
┌─────────────────────────────────────────┐
│     ProxyManager (g_proxy_manager)      │
│  - detectBinary()                       │
│  - loadConfigFile()                     │
│  - writeConfigFile()                    │
│  - runCommand()                         │
└──────────────┬──────────────────────────┘
               │
               │ system() / popen()
               ↓
┌─────────────────────────────────────────┐
│     /usr/bin/proxychains4               │
│         (external binary)               │
└─────────────────────────────────────────┘
               │
               │ TCP/IP
               ↓
┌─────────────────────────────────────────┐
│   Proxy Chain (SOCKS5/HTTP/etc.)        │
└─────────────────────────────────────────┘
```

## 🧪 Testing

### Test Proxy Manager Status
```bash
curl http://localhost:8080/api/proxy/status
```

### Test Proxy Command Execution
```bash
curl -X POST http://localhost:8080/api/proxy/run \
  -H "Content-Type: application/json" \
  -d '{"command":["ping","-c","1","8.8.8.8"]}'
```

### Test Configuration Management
```bash
# Write a test config
curl -X POST http://localhost:8080/api/proxy/config \
  -H "Content-Type: application/json" \
  -d '{"proxies":[{"type":"socks5","host":"127.0.0.1","port":9050}]}'

# Read it back
curl http://localhost:8080/api/proxy/config
```

## 📝 Configuration File Format

Vellum generates proxychains configuration in standard format:

```
# Vellum-generated proxychains configuration
dynamic_chain
proxy_dns
verbose_mode
tcp_read_time_out 15000
tcp_connect_time_out 8000

[ProxyList]
socks5 127.0.0.1 9050
http proxy.example.com 3128 username password
```

## ⚠️ Security Considerations

1. **Password Storage**: Passwords in config files are stored as plain text. Restrict file permissions:
   ```bash
   chmod 600 /etc/vellum/proxychains.conf
   ```

2. **API Access**: Protect proxy API endpoints with authentication in production.

3. **Command Injection**: The ProxyManager properly escapes command arguments to prevent injection.

4. **Network Exposure**: Don't expose proxy configuration endpoints on untrusted networks.

## 🔮 Future Enhancements

- [ ] Support for proxy authentication via separate credential store
- [ ] Encrypted configuration file storage
- [ ] Proxy health checking and failover
- [ ] Per-VM proxy routing configuration
- [ ] Frontend UI for proxy management
- [ ] Proxy chain metrics and monitoring

## 📚 References

- [Proxychains Documentation](https://github.com/rofl0r/proxychains-ng)
- [SOCKS Protocol](https://www.rfc-editor.org/rfc/rfc1928.html)
- [HTTP CONNECT Tunneling](https://www.rfc-editor.org/rfc/rfc7231#section-4.3.6)

---

**Version**: v2.0  
**Date**: May 2026  
**Status**: ✅ Complete & Integrated
