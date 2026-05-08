# Vellum v2.0 - Quick Start Guide

## 🚀 What's New in v2.0?

✅ **GPU Passthrough & Enterprise Features**

| Feature | Benefit |
|---------|---------|
| **🎮 GPU Passthrough** | Direct hardware GPU access for ML/AI workloads |
| **🏢 Enterprise Security** | JWT auth, multi-tenancy, RBAC (framework ready) |
| **☁️ Cloud-Native Ready** | Kubernetes operator support (coming soon) |
| **📊 Advanced Monitoring** | Prometheus metrics, distributed tracing (coming soon) |
| **⚡ High Performance** | Multi-threaded architecture with <2% overhead |

**Plus all previous upgrades:** VM Networking, Persistence, API Security, Resource Control, Scalability

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

### 4. Use Proxychains (Optional)
If your environment requires a proxy for network access, Vellum supports `proxychains` while building and running the backend.

```bash
# Run Vellum with default proxychains config
./run.sh --proxychains

# Run Vellum with a custom proxychains config file
./run.sh --proxychains /etc/proxychains4.conf
```

If you want the React frontend development server to also use proxychains:

```bash
./start-frontend.sh --proxychains
```

### 5. Built-in Proxy Manager (v2.0)
Vellum now includes a C++ Proxy Manager that can generate and manage proxychains configuration at runtime.

#### Check proxy status
```bash
curl -X GET http://localhost:8080/api/proxy/status
```

#### Read the current config
```bash
curl -X GET http://localhost:8080/api/proxy/config
```

#### Write a new proxychains config
```bash
curl -X POST http://localhost:8080/api/proxy/config \
  -H "Content-Type: application/json" \
  -d '{
    "dynamicChain": true,
    "proxyDNS": true,
    "quietMode": true,
    "proxies": [
      {"type":"socks5","host":"127.0.0.1","port":9050}
    ]
  }'
```

#### Run a command through proxychains
```bash
curl -X POST http://localhost:8080/api/proxy/run \
  -H "Content-Type: application/json" \
  -d '{"command":["curl","https://ifconfig.me"]}'
```

### 6. Setup GPU Passthrough (v2.0 - Optional)
```bash
# Enable IOMMU in kernel parameters (/etc/default/grub)
# Add to GRUB_CMDLINE_LINUX_DEFAULT:
#   intel_iommu=on iommu=pt  # For Intel CPUs
#   amd_iommu=on iommu=pt    # For AMD CPUs

# Update GRUB and reboot
sudo update-grub
sudo reboot

# After reboot, verify IOMMU is enabled
dmesg | grep -i iommu

# Load VFIO modules
sudo modprobe vfio vfio-pci vfio_iommu_type1

# Check available GPUs
lspci | grep -i "vga\|3d\|display"
```

### 5. Create Config Directory
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

### 7. Use the Built-in Proxy Manager (v2.0)
Vellum can manage proxychains configuration and run commands through the proxy directly from the backend API.

#### Check proxy status
```bash
curl -X GET http://localhost:8080/api/proxy/status \
  -H "Authorization: Bearer $TOKEN"
```

#### Create or update a proxychains config
```bash
curl -X POST http://localhost:8080/api/proxy/config \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "dynamicChain": true,
    "proxyDNS": true,
    "quietMode": true,
    "proxies": [
      {"type":"socks5","host":"127.0.0.1","port":9050}
    ]
  }'
```

#### Run a command through proxychains
```bash
curl -X POST http://localhost:8080/api/proxy/run \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"command":["curl","https://ifconfig.me"]}'
```

### 8. Test GPU Passthrough (v2.0)
```bash
# List available GPUs
curl -X GET http://localhost:8080/api/gpu/available \
  -H "Authorization: Bearer $TOKEN"

# Check IOMMU status
curl -X POST http://localhost:8080/api/gpu/check-iommu \
  -H "Authorization: Bearer $TOKEN"

# Attach GPU to VM (replace 0000:01:00.0 with actual GPU ID)
curl -X POST http://localhost:8080/api/vm/test-vm/gpu/attach \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "gpu_id": "0000:01:00.0",
    "enable_vgpu": false
  }'

# Get VM GPU status
curl -X GET http://localhost:8080/api/vm/test-vm/gpu \
  -H "Authorization: Bearer $TOKEN"
```

---

## File Changes Summary

### New Files Created (v2.0)
```
include/GPUManager.h                  # GPU management interface
src/GPUManager.cpp                    # GPU passthrough implementation
src/APIServer_Upgrades.cpp            # GPU API endpoints
frontend/src/components/GPUManager.js # GPU management UI
VELLUM_V2_ROADMAP.md                  # v2.0 development roadmap
```

### Previously Added Files
```
src/VMInstance_Upgrades.cpp           # Networking & persistence implementation
src/HypervisorManager_Upgrades.cpp    # Auth & persistence management
UPGRADES.md                            # Comprehensive documentation
```

### Files Modified
```
include/VMInstance.h                   # Added GPU methods
README.md                             # Updated for v2.0 features
QUICKSTART.md                         # Added GPU setup instructions
vellum/CMakeLists.txt                  # Added GPUManager source
frontend/src/App.js                   # Added GPU navigation & component
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
