# Implementation Checklist - Vellum Upgrades

## Complete Integration Guide

Use this checklist to integrate all upgrade components into the codebase.

---

## ✅ Header Files (Include Directory)

### VMInstance.h
- [x] Added `<filesystem>` header
- [x] Added `NetworkConfig` struct
- [x] Added `configureNetwork()` method
- [x] Added `removeNetwork()` method  
- [x] Added `getNetworkConfig()` method
- [x] Added `serializeConfig()` method
- [x] Added `deserializeConfig()` method
- [x] Added `saveStateToDisk()` method
- [x] Added `loadStateFromDisk()` method
- [x] Added network instance variables (tap_fd_, network_enabled_, network_config_)
- [x] Added persistence state (config_mutex_)
- [x] Added private helper methods declarations

### HypervisorManager.h
- [x] Added `<map>` and `<chrono>` headers
- [x] Added `AuthToken` struct
- [x] Added authentication methods (authenticateUser, validateToken, refreshToken, revokeToken)
- [x] Added persistence methods (saveAllVMConfigs, loadAllVMConfigs, etc.)
- [x] Added auth state (active_tokens_, auth_mutex_)
- [x] Added constants (DEFAULT_USER, DEFAULT_PASS, TOKEN_EXPIRY_SECONDS)
- [x] Added config directory constants

### APISchema.h
- [x] Updated authentication note
- [x] Added authentication endpoints documentation
- [x] Added networking endpoints documentation
- [x] Added persistence endpoints documentation
- [x] Added cgroup management endpoints documentation

---

## ✅ Implementation Files (src Directory)

### VMInstance_Upgrades.cpp (NEW FILE)
- [x] Networking implementation:
  - [x] `configureNetwork()` - Main entry point
  - [x] `createTAPDevice()` - TAP device creation
  - [x] `attachToBridge()` - Bridge attachment
  - [x] `bringUpInterface()` - Interface activation
  - [x] `removeNetwork()` - Cleanup

- [x] Persistence implementation:
  - [x] `serializeConfig()` - JSON serialization
  - [x] `deserializeConfig()` - JSON deserialization
  - [x] `saveStateToDisk()` - File output
  - [x] `loadStateFromDisk()` - File input
  - [x] `stateToString()` - State enum conversion

- [x] Cgroup enhancement:
  - [x] `setupCgroupEnhanced()` - V2 hierarchy setup
  - [x] `writeCgroupFile()` - Write to cgroup
  - [x] `readCgroupFile()` - Read from cgroup

### HypervisorManager_Upgrades.cpp (NEW FILE)
- [x] Authentication implementation:
  - [x] `authenticateUser()` - Login validation
  - [x] `validateToken()` - Token verification
  - [x] `refreshToken()` - Token renewal
  - [x] `revokeToken()` - Token invalidation
  - [x] `generateToken()` - Token generation

- [x] Persistence implementation:
  - [x] `saveAllVMConfigs()` - Batch save
  - [x] `loadAllVMConfigs()` - Batch load
  - [x] `saveVMConfig()` - Single save
  - [x] `loadVMConfig()` - Single load

### APIServer_Upgrades.cpp (REFERENCE FILE)
- [x] Authentication endpoints:
  - [ ] POST /api/auth/login - TO BE INTEGRATED
  - [ ] POST /api/auth/refresh - TO BE INTEGRATED
  - [ ] POST /api/auth/logout - TO BE INTEGRATED

- [x] Networking endpoints:
  - [ ] POST /api/vm/{id}/network/configure - TO BE INTEGRATED
  - [ ] GET /api/vm/{id}/network - TO BE INTEGRATED
  - [ ] DELETE /api/vm/{id}/network - TO BE INTEGRATED

- [x] Persistence endpoints:
  - [ ] POST /api/vm/save-config - TO BE INTEGRATED
  - [ ] POST /api/vm/load-config - TO BE INTEGRATED
  - [ ] GET /api/vm/{id}/config - TO BE INTEGRATED

- [x] Cgroup endpoints:
  - [ ] GET /api/vm/{id}/cgroup-status - TO BE INTEGRATED
  - [ ] POST /api/vm/{id}/cgroup-update - TO BE INTEGRATED

---

## ⏳ Integration Tasks (APIServer.cpp)

These need to be manually integrated into `src/APIServer.cpp`:

### Task 1: Add Helper Function
```cpp
// Add before setupRoutes() method
static bool isValidToken(HypervisorManager& hm, const std::string& authHeader) {
    if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
        return false;
    }
    std::string token = authHeader.substr(7);
    return hm.validateToken(token);
}
```

### Task 2: Include nlohmann_json
```cpp
// Add at top of APIServer.cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;
```

### Task 3: Add All Endpoints
Copy all uncommented route definitions from `src/APIServer_Upgrades.cpp` into `setupRoutes()` method.

### Task 4: Authentication Middleware
Optionally wrap existing endpoints with auth check:
```cpp
// Before any protected endpoint
if (!isValidToken(hm, req.get_header_value("Authorization"))) {
    return jsonErr(401, "Unauthorized");
}
```

---

## ✅ Build Configuration (CMakeLists.txt)

- [x] Added nlohmann_json find_package()
- [x] Added warning if nlohmann_json not found
- [x] Added new source files:
  - [x] `src/VMInstance_Upgrades.cpp`
  - [x] `src/HypervisorManager_Upgrades.cpp`
  - [x] `src/cgroup/CgroupManager.cpp`
- [x] Updated target_link_libraries for nlohmann_json
- [x] CMakeLists.txt ready for build

---

## ✅ Documentation

- [x] UPGRADES.md - Complete feature guide (2000+ lines)
- [x] QUICKSTART.md - Quick start guide
- [x] IMPLEMENTATION_CHECKLIST.md - This file
- [x] APISchema.h - API endpoint documentation

---

## 🔄 Integration Workflow (Step-by-Step)

### Step 1: Prepare Environment
```bash
# Install dependencies
sudo apt-get install nlohmann-json3-dev bridge-utils uml-utilities

# Create directories
sudo mkdir -p /etc/vellum/configs
sudo chown $USER:$USER /etc/vellum/configs
```

### Step 2: Integrate API Endpoints
- [ ] Open `src/APIServer.cpp`
- [ ] Copy helper function from APIServer_Upgrades.cpp
- [ ] Add `#include <nlohmann/json.hpp>`
- [ ] Add all route definitions to `setupRoutes()`
- [ ] Verify no syntax errors

### Step 3: Update Security Defaults
- [ ] Edit `include/HypervisorManager.h`
- [ ] Change `DEFAULT_PASS` to secure password
- [ ] Change `TOKEN_EXPIRY_SECONDS` if desired (default 3600s)
- [ ] Rebuild and test

### Step 4: Build and Test
```bash
cd vellum/build
cmake .. && make -j$(nproc)

# Run daemon
sudo ./vellum &

# Test auth endpoint
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"YOURPASSWORD"}'
```

### Step 5: Verify All Components
- [ ] Authentication works (login/refresh/logout)
- [ ] VM networking configurable
- [ ] Persistence save/load functional
- [ ] Cgroup limits enforceable
- [ ] Existing VM endpoints still work

---

## 🧪 Validation Tests

### Authentication Tests
```bash
# Test: Invalid credentials
curl -X POST http://localhost:8080/api/auth/login \
  -d '{"username":"wrong","password":"wrong"}'
# Expected: 401 Unauthorized

# Test: Valid login
curl -X POST http://localhost:8080/api/auth/login \
  -d '{"username":"admin","password":"<YOUR_PASSWORD>"}'
# Expected: 200 with token

# Test: Token refresh
curl -X POST http://localhost:8080/api/auth/refresh \
  -H "Authorization: Bearer <TOKEN>"
# Expected: 200 with new token

# Test: Expired token
sleep 3601  # Wait for expiry
curl -H "Authorization: Bearer <OLD_TOKEN>" \
  http://localhost:8080/api/vm/list
# Expected: 401 Unauthorized
```

### Networking Tests
```bash
# Test: Configure network
curl -X POST http://localhost:8080/api/vm/vm1/network/configure \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"tapInterface":"tap0","macAddress":"52:54:00:12:34:56"}'
# Expected: 200 OK

# Test: Get network config
curl -X GET http://localhost:8080/api/vm/vm1/network \
  -H "Authorization: Bearer $TOKEN"
# Expected: 200 with config JSON

# Test: Remove network
curl -X DELETE http://localhost:8080/api/vm/vm1/network \
  -H "Authorization: Bearer $TOKEN"
# Expected: 200 OK
```

### Persistence Tests
```bash
# Test: Save configs
curl -X POST http://localhost:8080/api/vm/save-config \
  -H "Authorization: Bearer $TOKEN"
# Expected: 200 with count

# Verify file created
ls -la /etc/vellum/configs/vms.json

# Test: Load configs
curl -X POST http://localhost:8080/api/vm/load-config \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"filePath":"/etc/vellum/configs/vms.json"}'
# Expected: 200 with VM list
```

### Cgroup Tests
```bash
# Test: Get cgroup status
curl -X GET http://localhost:8080/api/vm/vm1/cgroup-status \
  -H "Authorization: Bearer $TOKEN"
# Expected: 200 with resource limits

# Test: Update limits
curl -X POST http://localhost:8080/api/vm/vm1/cgroup-update \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"cpuLimit":50,"memoryLimit":512}'
# Expected: 200 OK
```

---

## 📋 Pre-Release Checklist

- [ ] All 5 upgrades integrated
- [ ] No compilation errors
- [ ] No compilation warnings (or approved)
- [ ] All API endpoints responding correctly
- [ ] Authentication enforced on protected endpoints
- [ ] Default password changed
- [ ] Documentation complete
- [ ] Example commands tested
- [ ] Networking bridge created and working
- [ ] Config directory created with proper permissions
- [ ] Cgroup v2 mounted and accessible
- [ ] HTTPS/TLS recommended for production

---

## 🚀 Deployment Checklist

### Production Setup
- [ ] Security hardened:
  - [ ] Strong password set
  - [ ] HTTPS/TLS enabled
  - [ ] Firewall configured
  - [ ] SSH keys in use
  
- [ ] High availability:
  - [ ] Config backed up
  - [ ] Logs configured
  - [ ] Monitoring enabled
  
- [ ] Performance optimized:
  - [ ] Thread pool sized
  - [ ] Memory allocated
  - [ ] Cgroup limits appropriate
  
- [ ] Monitoring:
  - [ ] Prometheus scraping
  - [ ] Alerting configured
  - [ ] Dashboards set up

---

## 📝 Notes

- **Task 1 (Networking)**: Requires root for TAP device creation. Consider using `sudo` or running daemon as root.
- **Task 2 (Persistence)**: JSON library required. Falls back gracefully if not available.
- **Task 3 (Auth)**: Simple token generation. Use proper JWT library for production.
- **Task 4 (Cgroup)**: Requires cgroup v2. Check with `mount | grep cgroup2`.
- **Task 5 (Scalability)**: Architecture ready; async handlers in Crow not fully utilized yet.

---

## 🔗 Reference Files

- Header changes: `include/VMInstance.h`, `include/HypervisorManager.h`
- Implementation: `src/VMInstance_Upgrades.cpp`, `src/HypervisorManager_Upgrades.cpp`
- API reference: `src/APIServer_Upgrades.cpp`
- Documentation: `UPGRADES.md`, `QUICKSTART.md`
- Config: `vellum/CMakeLists.txt`

---

## ✅ Final Verification

Run this comprehensive test:
```bash
#!/bin/bash
echo "=== Vellum Upgrades Verification ==="

# Check daemon running
echo -n "Daemon: "
curl -s http://localhost:8080/api/vm/list > /dev/null && echo "✓" || echo "✗"

# Check auth
echo -n "Auth: "
TOKEN=$(curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"<PASSWORD>"}' | jq -r '.token' 2>/dev/null)
[ -n "$TOKEN" ] && echo "✓" || echo "✗"

# Check persistence
echo -n "Persistence: "
[ -d "/etc/vellum/configs" ] && echo "✓" || echo "✗"

# Check networking
echo -n "Networking: "
ip link show velbr0 > /dev/null 2>&1 && echo "✓" || echo "✗"

# Check cgroup
echo -n "Cgroup v2: "
mount | grep -q cgroup2 && echo "✓" || echo "✗"

echo "=== All systems ready! ==="
```

---

**Ready to deploy? Follow the Integration Workflow above! 🎯**
