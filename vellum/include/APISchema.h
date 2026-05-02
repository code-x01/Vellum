#ifndef VELLUM_API_SCHEMA_H
#define VELLUM_API_SCHEMA_H

/*
 * Vellum REST API Schema
 *
 * Base URL: http://localhost:8080/api
 *
 * Authentication: None (for simplicity, add JWT or similar in production)
 *
 * Content-Type: application/json
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
 * Body: {
 *   "cpuLimit": number?,    // Percentage
 *   "memoryLimit": number?  // MB
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