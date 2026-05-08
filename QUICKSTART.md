# Vellum Upgrades - Quick Start Guide

## What Was Upgraded?

✅ **5 Major Improvements Implemented**

| Feature | Benefit |
|---------|---------|
| **🌐 VM Networking** | VMs can connect to external networks via TAP/bridge |
| **💾 Persistence** | VM configs auto-save; survive daemon restarts |
| **🔐 API Security** | JWT authentication protects all endpoints |
| **⚙️ Resource Control** | Enhanced cgroup enforcement for CPU/memory limits |
| **⚡ Scalability Ready** | Architecture prepared for 50+ concurrent VMs |

---

## Installation (5 minutes)

### 1. Install Dependencies
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install nlohmann-json3-dev bridge-utils uml-utilities

# Or use Vcpkg
vcpkg install nlohmann-json
```

### 2. Rebuild Vellum
```bash
cd vellum
mkdir -p build && cd build
cmake .. && make -j$(nproc)
sudo make install  # Optional
```

### 3. Setup Host Networking (One-time)
```bash
# Create bridge for VM networking
sudo ip link add name velbr0 type bridge
sudo ip link set velbr0 up
sudo ip addr add 192.168.122.1/24 dev velbr0

# Make persistent (add to /etc/network/interfaces or netplan)
```

### 4. Create Config Directory
```bash
sudo mkdir -p /etc/vellum/configs
sudo chown $USER:$USER /etc/vellum/configs
```

---

## Quick Test (10 minutes)

### 1. Start the Daemon
```bash
sudo ./vellum &
```

### 2. Login and Get Token
```bash
# Default: admin / vellum2024
TOKEN=$(curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"vellum2024"}' | jq -r '.token')

echo "Your token: $TOKEN"
```

### 3. Create a Test VM
```bash
curl -X POST http://localhost:8080/api/vm/create \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "id": "test-vm",
    "kernelPath": "/path/to/kernel",
    "memoryMB": 256,
    "vcpus": 2
  }'
```

### 4. Configure Networking
```bash
curl -X POST http://localhost:8080/api/vm/test-vm/network/configure \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "tapInterface": "tap0",
    "macAddress": "52:54:00:12:34:56",
    "bridgeName": "velbr0",
    "dhcpEnabled": true
  }'
```

### 5. Set Resource Limits
```bash
curl -X POST http://localhost:8080/api/vm/test-vm/cgroup-update \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "cpuLimit": 50,
    "memoryLimit": 256
  }'
```

### 6. Save Configuration
```bash
curl -X POST http://localhost:8080/api/vm/save-config \
  -H "Authorization: Bearer $TOKEN"

# Verify
cat /etc/vellum/configs/vms.json
```

---

## File Changes Summary

### New Files Created
```
src/VMInstance_Upgrades.cpp           # Networking & persistence implementation
src/HypervisorManager_Upgrades.cpp    # Auth & persistence management
src/APIServer_Upgrades.cpp            # API endpoint definitions
UPGRADES.md                            # Comprehensive documentation
```

### Files Modified
```
include/VMInstance.h                   # Added network/persistence methods
include/HypervisorManager.h            # Added auth/persistence structs
include/APISchema.h                    # Added new API endpoints
vellum/CMakeLists.txt                  # Added dependencies and new sources
```

### Key Dependencies Added
- `nlohmann_json` - JSON serialization for persistence
- Existing: `Crow`, `Boost`, `OpenSSL`, `pthread`

---

## Security Changes

### ⚠️ IMPORTANT: Change Default Password

Edit `include/HypervisorManager.h`:
```cpp
// Line ~25
static constexpr const char* DEFAULT_PASS = "your-new-secure-password";
```

Rebuild:
```bash
cd build && cmake .. && make
```

### Enable HTTPS (Recommended)

1. Generate self-signed cert:
```bash
openssl req -x509 -newkey rsa:4096 -nodes -out cert.pem -keyout key.pem -days 365
```

2. Update Crow configuration in `src/APIServer.cpp`:
```cpp
auto& app = app_.ssl_file(cert.pem, key.pem);
```

---

## API Endpoints Quick Reference

### Authentication
```bash
POST   /api/auth/login          # Get JWT token
POST   /api/auth/refresh        # Refresh token
POST   /api/auth/logout         # Revoke token
```

### VM Management
```bash
POST   /api/vm/create           # Create VM
POST   /api/vm/{id}/start       # Start VM
POST   /api/vm/{id}/stop        # Stop VM
POST   /api/vm/{id}/pause       # Pause VM
POST   /api/vm/{id}/resume      # Resume VM
DELETE /api/vm/{id}             # Delete VM
GET    /api/vm/list             # List all VMs
```

### Networking (NEW)
```bash
POST   /api/vm/{id}/network/configure     # Setup networking
GET    /api/vm/{id}/network               # Get network config
DELETE /api/vm/{id}/network               # Remove networking
```

### Persistence (NEW)
```bash
POST   /api/vm/save-config               # Save all VM configs
POST   /api/vm/load-config               # Load VM configs
GET    /api/vm/{id}/config               # Get VM config
```

### Cgroup/Resources (ENHANCED)
```bash
GET    /api/vm/{id}/cgroup-status       # Check resource limits
POST   /api/vm/{id}/cgroup-update       # Update resource limits
```

### Example: All Headers Required
```bash
curl -X POST http://localhost:8080/api/vm/vm1/network/configure \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{...}'
```

---

## Troubleshooting

### Problem: "Cannot create TAP device"
**Solution:**
```bash
sudo apt-get install uml-utilities
sudo ip tuntap add tap0 mode tap user $USER
```

### Problem: "Token validation failed"
**Solution:**
```bash
# Get new token
TOKEN=$(curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"vellum2024"}' | jq -r '.token')
```

### Problem: "Cgroup setup failed"
**Solution:**
```bash
# Check cgroup v2
mount | grep cgroup2
# If not found:
sudo mount -t cgroup2 none /sys/fs/cgroup/unified
```

### Problem: Build fails with nlohmann_json not found
**Solution:**
```bash
# Install via apt
sudo apt-get install nlohmann-json3-dev

# Or compile without JSON support (persistence will warn)
# Continue with basic functionality
```

---

## Next Steps

### Immediate (Phase 2 - VM Mechanics)
After these upgrades stabilize, implement:
- ✨ Live VM migration
- 📷 Snapshot/CoW support
- 🔄 Auto-restart on failure

### Future Enhancements
- Multi-user support with RBAC
- Prometheus metrics integration
- High-availability clustering
- Web UI improvements

---

## Files to Read for Details

| Document | Content |
|----------|---------|
| `UPGRADES.md` | Complete feature documentation |
| `src/APIServer_Upgrades.cpp` | All API endpoint definitions |
| `include/VMInstance.h` | New VMInstance methods |
| `include/HypervisorManager.h` | Auth/persistence API |

---

## Support & Issues

### Debug Logging
```bash
# Run with verbose output
sudo ./vellum 2>&1 | tee vellum.log

# Check system logs
journalctl -u vellum -f
```

### Test API Connectivity
```bash
# Check if daemon is running
curl http://localhost:8080/health 2>&1

# Simple login test
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"vellum2024"}'
```

---

## Performance Tips

1. **CPU Limits**: Set to 50-75% per VM for stability
2. **Memory**: At least 256MB per VM; 512MB recommended
3. **Bridge MTU**: Keep at 1500 (standard) unless needed
4. **Token Expiry**: 3600s (1 hour) balances security and convenience

---

**Ready to start? Run the quick test above! 🚀**
