# Vellum Advanced VM Mechanics

This document describes the advanced VM mechanics implemented in Vellum, providing enterprise-grade virtualization features beyond basic lifecycle management.

## Overview

The advanced VM mechanics upgrade adds the following capabilities:

- **Snapshot/CoW Support**: qcow2-based copy-on-write disk cloning for instant VM duplication
- **Auto-restart/Recovery**: Automatic restart on failure with configurable policies
- **Live Migration**: Move running VMs between hosts using KVM migration
- **Enhanced Performance Profiling**: Detailed CPU/memory/disk/network profiling with real-time analytics
- **Memory Ballooning**: Dynamic memory adjustment while VM is running
- **CPU Hotplug**: Add/remove vCPUs dynamically (KVM hotplug support)
- **QoS/Throttling**: Quality of service controls for resource management

## 1. Snapshot/CoW Support

### Overview
Vellum now supports qcow2-based copy-on-write (CoW) disk snapshots, enabling instant VM cloning and efficient disk space usage.

### API Endpoints

#### Create Snapshot
```http
POST /api/vm/{id}/snapshot/{name}
Authorization: Bearer <token>
```

#### Restore Snapshot
```http
POST /api/vm/{id}/snapshot/{name}/restore
Authorization: Bearer <token>
```

#### Clone VM
```http
POST /api/vm/{id}/clone/{newId}
Authorization: Bearer <token>
Content-Type: application/json

{
  "snapshot": "base_snapshot"
}
```

### Implementation Details

- **qcow2 Format**: Uses standard qcow2 format with backing file support
- **Copy-on-Write**: Only modified blocks are stored in overlay files
- **Directory Structure**:
  ```
  /var/lib/vellum/snapshots/{vm_id}/
  ├── base_snapshot.qcow2
  ├── base_snapshot.state
  ├── clone_snapshot.qcow2
  └── clone_snapshot.state
  ```

### Usage Examples

```bash
# Create a snapshot
curl -X POST http://localhost:8080/api/vm/myvm/snapshot/base \
  -H "Authorization: Bearer $TOKEN"

# Clone from snapshot
curl -X POST http://localhost:8080/api/vm/myvm/clone/myvm-clone \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"snapshot": "base"}'

# Restore to snapshot
curl -X POST http://localhost:8080/api/vm/myvm/snapshot/base/restore \
  -H "Authorization: Bearer $TOKEN"
```

## 2. Auto-restart/Recovery

### Overview
Automatic restart functionality monitors VM health and restarts failed VMs based on configurable policies.

### Configuration

```json
{
  "maxAttempts": 3,
  "delaySeconds": 30,
  "maxRuntimeSeconds": 3600,
  "failureConditions": ["KVM_RUN", "memory", "disk"]
}
```

### API Endpoints

#### Enable Auto-restart
```http
POST /api/vm/{id}/auto-restart/enable
Authorization: Bearer <token>
Content-Type: application/json

{
  "maxAttempts": 5,
  "delaySeconds": 60,
  "maxRuntimeSeconds": 7200
}
```

#### Disable Auto-restart
```http
POST /api/vm/{id}/auto-restart/disable
Authorization: Bearer <token>
```

### Monitoring Logic

The auto-restart monitor:
1. Checks VM state every 5 seconds
2. Detects error states or unexpected stops
3. Applies exponential backoff for restart attempts
4. Respects maximum attempt and runtime limits
5. Logs all restart activities

## 3. Live Migration

### Overview
Live migration allows moving running VMs between Vellum hosts with minimal downtime using KVM's migration capabilities.

### Architecture

- **Source Host**: Initiates migration, sends VM state
- **Destination Host**: Receives VM state, resumes execution
- **Migration Protocol**: TCP-based with metadata and memory transfer
- **State Synchronization**: Ensures consistent VM state across hosts

### API Endpoint

```http
POST /api/vm/{id}/migrate
Authorization: Bearer <token>
Content-Type: application/json

{
  "destinationHost": "192.168.1.100",
  "port": 4444
}
```

### Migration Process

1. **Preparation**: Pause VM temporarily
2. **Metadata Transfer**: Send VM configuration
3. **Memory Transfer**: Iterative copy of dirty pages
4. **Device State**: Transfer device configurations
5. **Final Sync**: Atomic handover to destination
6. **Cleanup**: Remove VM from source host

### Requirements

- Shared storage or disk migration
- Network connectivity between hosts
- Compatible KVM versions
- Sufficient resources on destination

## 4. Enhanced Performance Profiling

### Overview
Detailed performance monitoring provides comprehensive metrics for CPU, memory, disk, and network usage.

### Metrics Collected

```json
{
  "cpu": {
    "userPercent": 15.2,
    "systemPercent": 8.7,
    "idlePercent": 76.1
  },
  "memory": {
    "rssKb": 524288,
    "vszKb": 1048576
  },
  "disk": {
    "readBytes": 1048576,
    "writeBytes": 2097152
  },
  "network": {
    "rxBytesPerSec": 125000,
    "txBytesPerSec": 98000
  },
  "timestamp": 1640995200000
}
```

### API Endpoints

#### Get Current Performance
```http
GET /api/vm/{id}/performance
Authorization: Bearer <token>
```

#### Start Monitoring
```http
POST /api/vm/{id}/performance/start
Authorization: Bearer <token>
```

#### Stop Monitoring
```http
POST /api/vm/{id}/performance/stop
Authorization: Bearer <token>
```

### Real-time Streaming

Performance data can be streamed via WebSocket:

```javascript
const ws = new WebSocket('ws://localhost:8080/ws/telemetry');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  if (data.type === 'performance') {
    updateCharts(data.data);
  }
};
```

## 5. Memory Ballooning

### Overview
Dynamic memory adjustment allows changing VM memory allocation without restarting.

### API Endpoint

```http
POST /api/vm/{id}/memory/adjust
Authorization: Bearer <token>
Content-Type: application/json

{
  "memoryMB": 1024
}
```

### Requirements

- **Guest Support**: Requires virtio-balloon driver in guest OS
- **VM State**: Only works on running VMs
- **Limits**: 64MB minimum, 2x current maximum

### Implementation

- Negotiates with guest balloon driver
- Gradually inflates/deflates balloon
- Updates internal memory tracking
- Maintains performance during adjustment

## 6. CPU Hotplug

### Overview
Dynamic vCPU management allows adding or removing virtual CPUs while the VM is running.

### API Endpoints

#### Add vCPU
```http
POST /api/vm/{id}/vcpu/add
Authorization: Bearer <token>
```

#### Remove vCPU
```http
POST /api/vm/{id}/vcpu/remove
Authorization: Bearer <token>
```

### Limitations

- **Minimum**: Cannot remove the last vCPU
- **Maximum**: Limited to 32 vCPUs
- **Guest Support**: Requires hotplug-capable guest OS
- **KVM Support**: Uses KVM CPU hotplug ioctls

## 7. QoS/Throttling

### Overview
Quality of Service controls provide fine-grained resource management and prioritization.

### Configuration

```json
{
  "cpuShares": 2048,
  "ioWeight": 800,
  "networkPriority": 10,
  "memorySoftLimit": 512
}
```

### API Endpoint

```http
POST /api/vm/{id}/qos/configure
Authorization: Bearer <token>
Content-Type: application/json

{
  "cpuShares": 2048,
  "ioWeight": 800,
  "networkPriority": 10,
  "memorySoftLimit": 512
}
```

### QoS Parameters

- **CPU Shares**: Relative CPU allocation (default: 1024)
- **IO Weight**: Block I/O priority (10-1000, default: 500)
- **Network Priority**: Traffic prioritization (-15 to 15, default: 0)
- **Memory Soft Limit**: Soft memory limit in MB (0 = disabled)

## Implementation Files

### Core Implementation
- `VMInstance_Mechanics.cpp`: Main implementation of advanced features
- `VMInstance.h`: Updated header with new method declarations

### API Integration
- `APIServer_Upgrades.cpp`: REST endpoints for advanced mechanics
- `APISchema.h`: API documentation and schemas

### Build System
- `CMakeLists.txt`: Updated to include new source files

## Usage Examples

### Complete VM Lifecycle with Advanced Features

```bash
# 1. Create and start VM
curl -X POST http://localhost:8080/api/vm/create \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "id": "advanced-vm",
    "kernelPath": "/boot/vmlinuz-linux",
    "memoryMB": 512,
    "vcpus": 2
  }'

curl -X POST http://localhost:8080/api/vm/advanced-vm/start \
  -H "Authorization: Bearer $TOKEN"

# 2. Configure networking
curl -X POST http://localhost:8080/api/vm/advanced-vm/network/configure \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "tapInterface": "tap0",
    "macAddress": "52:54:00:12:34:56",
    "bridgeName": "br0"
  }'

# 3. Enable auto-restart
curl -X POST http://localhost:8080/api/vm/advanced-vm/auto-restart/enable \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "maxAttempts": 3,
    "delaySeconds": 30
  }'

# 4. Configure QoS
curl -X POST http://localhost:8080/api/vm/advanced-vm/qos/configure \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "cpuShares": 2048,
    "ioWeight": 800
  }'

# 5. Start performance monitoring
curl -X POST http://localhost:8080/api/vm/advanced-vm/performance/start \
  -H "Authorization: Bearer $TOKEN"

# 6. Create snapshot
curl -X POST http://localhost:8080/api/vm/advanced-vm/snapshot/base \
  -H "Authorization: Bearer $TOKEN"

# 7. Clone VM
curl -X POST http://localhost:8080/api/vm/advanced-vm/clone/advanced-vm-clone \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"snapshot": "base"}'

# 8. Adjust memory dynamically
curl -X POST http://localhost:8080/api/vm/advanced-vm/memory/adjust \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"memoryMB": 1024}'

# 9. Add vCPU
curl -X POST http://localhost:8080/api/vm/advanced-vm/vcpu/add \
  -H "Authorization: Bearer $TOKEN"
```

## Performance Considerations

### Resource Overhead
- **Snapshots**: Minimal overhead, CoW prevents duplication
- **Auto-restart**: Lightweight monitoring thread
- **Performance Monitoring**: Configurable sampling rate
- **Migration**: Network bandwidth intensive during transfer

### Scalability
- **Concurrent Operations**: Thread-safe implementations
- **Resource Limits**: Configurable limits prevent abuse
- **Cleanup**: Automatic resource cleanup on failures

## Security Considerations

### Access Control
- All advanced endpoints require authentication
- Token-based authorization for sensitive operations
- Audit logging for all state changes

### Network Security
- Migration uses authenticated connections
- Network configuration validates parameters
- QoS prevents resource exhaustion attacks

## Troubleshooting

### Common Issues

1. **Snapshot Creation Fails**
   - Check disk space in `/var/lib/vellum/snapshots/`
   - Verify VM is in appropriate state (running/paused)

2. **Migration Fails**
   - Verify network connectivity between hosts
   - Check KVM version compatibility
   - Ensure destination has sufficient resources

3. **Memory Ballooning Not Working**
   - Verify guest OS has virtio-balloon driver
   - Check kernel module is loaded: `lsmod | grep balloon`

4. **CPU Hotplug Fails**
   - Verify KVM hotplug support: `grep hotplug /sys/module/kvm*/parameters`
   - Check guest OS hotplug capabilities

### Debug Logging

Enable verbose logging by setting environment variable:
```bash
export VELLUM_DEBUG=1
```

Check logs for detailed error information:
```bash
tail -f /var/log/vellum/vellum.log
```

## Future Enhancements

### Planned Features
- **Advanced Scheduling**: NUMA-aware CPU pinning
- **Storage Migration**: Live storage migration
- **Nested Virtualization**: VMs inside VMs
- **GPU Passthrough**: Direct GPU access for VMs
- **Container Integration**: Docker/Podman support

### API Extensions
- **Bulk Operations**: Multi-VM management
- **Policy Engine**: Automated resource policies
- **Metrics Export**: Prometheus integration
- **Event Streaming**: Real-time event notifications

---

This advanced VM mechanics implementation transforms Vellum from a basic hypervisor into a production-ready virtualization platform with enterprise-grade features.</content>
<parameter name="filePath">d:\WD\Vellum\vellum\VM_MECHANICS.md