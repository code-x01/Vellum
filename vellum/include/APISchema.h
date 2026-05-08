#ifndef VELLUM_API_SCHEMA_H
#define VELLUM_API_SCHEMA_H

/*
 * Vellum REST API Schema
 *
 * Base URL: http://localhost:8080/api
 *
 * Authentication: JWT Bearer token (required for all endpoints except /api/auth/login)
 * Authorization Header: Authorization: Bearer <token>
 *
 * Content-Type: application/json
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * UPGRADES:
 * - Task 1: VM Networking (TAP/Bridge support)
 * - Task 2: Persistence (Save/Load configurations)
 * - Task 3: API Authentication (JWT tokens)
 * - Task 4: Cgroup Enforcement (Resource limits)
 * - Task 5: Scalability (Async event loop)
 * ═════════════════════════════════════════════════════════════════════════════
 */

// ═════════════════════════════════════════════════════════════════════════════
// AUTHENTICATION ENDPOINTS (Task 3)
// ═════════════════════════════════════════════════════════════════════════════
/*
 * POST /api/auth/login
 * Body: {
 *   "username": "string",
 *   "password": "string"
 * }
 * Response: {
 *   "success": boolean,
 *   "token": "string (JWT)",
 *   "expiresIn": number (seconds)
 * }
 */

/*
 * POST /api/auth/refresh
 * Headers: Authorization: Bearer <token>
 * Response: {
 *   "success": boolean,
 *   "token": "string (new JWT)",
 *   "expiresIn": number (seconds)
 * }
 */

/*
 * POST /api/auth/logout
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

// VM Management Endpoints
/*
 * POST /api/vm/create
 * Body: {
 *   "id": "string",           // Unique VM identifier
 *   "kernelPath": "string",   // Path to kernel image
 *   "initrdPath": "string?",  // Optional initrd path
 *   "memoryMB": number?,      // Memory in MB (default: 256)
 *   "vcpus": number?          // Number of vCPUs (default: 1)
 * }
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * DELETE /api/vm/{id}
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/start
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/stop
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/pause
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/resume
 * Response: { "success": boolean, "message": "string" }
 */

// Monitoring Endpoints
/*
 * GET /api/vm/{id}/metrics
 * Response: {
 *   "cpuUsage": number,     // Percentage
 *   "memoryUsage": number,  // KB
 *   "diskUsage": number     // KB
 * }
 */

/*
 * GET /api/vm/metrics/global
 * Response: {
 *   "totalMemoryMB": number,
 *   "usedMemoryMB": number,
 *   "totalVCPUs": number,
 *   "usedVCPUs": number
 * }
 */

/*
 * GET /api/vm/list
 * Response: [ { "id": "string", "state": "string" }, ... ]
 */

// Console Endpoints
/*
 * WebSocket: /ws/console/{id}
 * Messages:
 * From client: { "type": "input", "data": "string" }
 * To client: { "type": "output", "data": "string" }
 */

// Snapshot Endpoints
/*
 * POST /api/vm/{id}/snapshot
 * Body: { "name": "string" }
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/restore
 * Body: { "name": "string" }
 * Response: { "success": boolean, "message": "string" }
 */

// Resource Management
/*
 * POST /api/vm/{id}/limits
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "cpuLimit": number?,    // Percentage
 *   "memoryLimit": number?  // MB
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// ═════════════════════════════════════════════════════════════════════════════
// NETWORKING ENDPOINTS (Task 1: VM Networking)
// ═════════════════════════════════════════════════════════════════════════════
/*
 * POST /api/vm/{id}/network/configure
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "tapInterface": "string",   // e.g., "tap0"
 *   "macAddress": "string",     // e.g., "52:54:00:12:34:56"
 *   "ipAddress": "string?",     // Optional static IP
 *   "gateway": "string?",       // Optional gateway
 *   "bridgeName": "string?",    // Default: "velbr0"
 *   "dhcpEnabled": boolean      // Default: true
 * }
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * GET /api/vm/{id}/network
 * Headers: Authorization: Bearer <token>
 * Response: {
 *   "tapInterface": "string",
 *   "macAddress": "string",
 *   "ipAddress": "string",
 *   "gateway": "string",
 *   "bridgeName": "string",
 *   "dhcpEnabled": boolean,
 *   "mtu": number
 * }
 */

/*
 * DELETE /api/vm/{id}/network
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

// ═════════════════════════════════════════════════════════════════════════════
// PERSISTENCE ENDPOINTS (Task 2: Persistence Layer)
// ═════════════════════════════════════════════════════════════════════════════
/*
 * POST /api/vm/save-config
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "vmIds": ["string"]  // VMs to save; if empty, save all
 * }
 * Response: {
 *   "success": boolean,
 *   "savedPath": "string",
 *   "count": number
 * }
 */

/*
 * POST /api/vm/load-config
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "filePath": "string"
 * }
 * Response: {
 *   "success": boolean,
 *   "loadedVMs": ["string"],
 *   "count": number
 * }
 */

/*
 * GET /api/vm/{id}/config
 * Headers: Authorization: Bearer <token>
 * Response: {
 *   JSON serialized VM configuration including:
 *   - id, kernelPath, initrdPath, diskPath
 *   - memoryMB, vcpus, kernelCmdline
 *   - networkConfig, state, createdAt
 * }
 */

// ═════════════════════════════════════════════════════════════════════════════
// CGROUP MANAGEMENT ENDPOINTS (Task 4: Resource Enforcement)
// ═════════════════════════════════════════════════════════════════════════════
/*
 * GET /api/vm/{id}/cgroup-status
 * Headers: Authorization: Bearer <token>
 * Response: {
 *   "cgroupPath": "string",
 *   "cpuLimit": number,      // percentage or null
 *   "memoryLimit": number,   // MB or null
 *   "currentCpuUsage": number,
 *   "currentMemoryUsage": number
 * }
 */

/*
 * POST /api/vm/{id}/cgroup-update
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "cpuLimit": number?,     // percentage
 *   "memoryLimit": number?   // MB
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// ═════════════════════════════════════════════════════════════════════════════
// ADVANCED VM MECHANICS ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// Snapshot/CoW Support
/*
 * POST /api/vm/{id}/snapshot/{name}
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/snapshot/{name}/restore
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/clone/{newId}
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "snapshot": "string?"  // Snapshot name to clone from (default: "latest")
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// Auto-restart/Recovery
/*
 * POST /api/vm/{id}/auto-restart/enable
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "maxAttempts": number?,       // Default: 3
 *   "delaySeconds": number?,     // Default: 30
 *   "maxRuntimeSeconds": number? // Default: 3600
 * }
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/auto-restart/disable
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

// Live Migration
/*
 * POST /api/vm/{id}/migrate
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "destinationHost": "string",  // Required
 *   "port": number?               // Default: 4444
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// Enhanced Performance Profiling
/*
 * GET /api/vm/{id}/performance
 * Headers: Authorization: Bearer <token>
 * Response: {
 *   "cpu": {
 *     "userPercent": number,
 *     "systemPercent": number,
 *     "idlePercent": number
 *   },
 *   "memory": {
 *     "rssKb": number,
 *     "vszKb": number
 *   },
 *   "disk": {
 *     "readBytes": number,
 *     "writeBytes": number
 *   },
 *   "network": {
 *     "rxBytesPerSec": number,
 *     "txBytesPerSec": number
 *   },
 *   "timestamp": number  // milliseconds since epoch
 * }
 */

/*
 * POST /api/vm/{id}/performance/start
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/performance/stop
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

// Memory Ballooning
/*
 * POST /api/vm/{id}/memory/adjust
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "memoryMB": number  // New memory size in MB
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// CPU Hotplug
/*
 * POST /api/vm/{id}/vcpu/add
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

/*
 * POST /api/vm/{id}/vcpu/remove
 * Headers: Authorization: Bearer <token>
 * Response: { "success": boolean, "message": "string" }
 */

// QoS/Throttling
/*
 * POST /api/vm/{id}/qos/configure
 * Headers: Authorization: Bearer <token>
 * Body: {
 *   "cpuShares": number?,      // Default: 1024
 *   "ioWeight": number?,       // Default: 500 (10-1000)
 *   "networkPriority": number?, // Default: 0 (-15 to 15)
 *   "memorySoftLimit": number? // MB, default: 0 (disabled)
 * }
 * Response: { "success": boolean, "message": "string" }
 */

// Telemetry WebSocket
/*
 * WebSocket: /ws/telemetry
 * To client: {
 *   "type": "metrics",
 *   "data": {
 *     "vmId": "string",
 *     "cpuUsage": number,
 *     "memoryUsage": number,
 *     "diskUsage": number
 *   }
 * }
 */

#endif // VELLUM_API_SCHEMA_H