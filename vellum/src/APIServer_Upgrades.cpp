/*
 * APIServer_Upgrades.cpp — Additional API endpoints for major upgrades:
 * - Task 1: VM Networking endpoints
 * - Task 2: Persistence endpoints
 * - Task 3: Authentication endpoints
 * - Task 4: Cgroup management endpoints
 *
 * Add these route definitions to APIServer::setupRoutes()
 */

// ═════════════════════════════════════════════════════════════════════════════
// TASK 3: AUTHENTICATION ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// POST /api/auth/login
/*
    CROW_ROUTE(app_, "/api/auth/login")
    .methods("POST"_method)([&hm](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        std::string username = json.has("username") ? json["username"].s() : "";
        std::string password = json.has("password") ? json["password"].s() : "";

        if (username.empty() || password.empty()) {
            return jsonErr(400, "username and password required");
        }

        HypervisorManager::AuthToken token;
        if (!hm.authenticateUser(username, password, token)) {
            return jsonErr(401, "Invalid credentials");
        }

        crow::json::wvalue j;
        j["success"] = true;
        j["token"] = token.token;
        j["expiresIn"] = HypervisorManager::TOKEN_EXPIRY_SECONDS;
        
        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // POST /api/auth/refresh
    CROW_ROUTE(app_, "/api/auth/refresh")
    .methods("POST"_method)([&hm](const crow::request& req) {
        // Extract token from Authorization header
        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            return jsonErr(401, "Missing or invalid Authorization header");
        }

        std::string token = auth_header.substr(7);
        if (!hm.validateToken(token)) {
            return jsonErr(401, "Invalid or expired token");
        }

        HypervisorManager::AuthToken newToken;
        if (!hm.refreshToken(token, newToken)) {
            return jsonErr(500, "Failed to refresh token");
        }

        crow::json::wvalue j;
        j["success"] = true;
        j["token"] = newToken.token;
        j["expiresIn"] = HypervisorManager::TOKEN_EXPIRY_SECONDS;
        
        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // POST /api/auth/logout
    CROW_ROUTE(app_, "/api/auth/logout")
    .methods("POST"_method)([&hm](const crow::request& req) {
        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            return jsonErr(401, "Missing or invalid Authorization header");
        }

        std::string token = auth_header.substr(7);
        hm.revokeToken(token);

        return jsonOK("Logged out successfully");
    });
*/

// ═════════════════════════════════════════════════════════════════════════════
// TASK 1: NETWORKING ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// POST /api/vm/{id}/network/configure
/*
    CROW_ROUTE(app_, "/api/vm/<string>/network/configure")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        // Check auth token
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        VMInstance::NetworkConfig config;
        config.tapInterface = json.has("tapInterface") ? json["tapInterface"].s() : "";
        config.macAddress = json.has("macAddress") ? json["macAddress"].s() : "";
        config.ipAddress = json.has("ipAddress") ? json["ipAddress"].s() : "";
        config.gateway = json.has("gateway") ? json["gateway"].s() : "";
        config.bridgeName = json.has("bridgeName") ? json["bridgeName"].s() : "velbr0";
        config.dhcpEnabled = json.has("dhcpEnabled") ? json["dhcpEnabled"].b() : true;
        config.mtu = json.has("mtu") ? static_cast<int>(json["mtu"].i()) : 1500;

        if (config.tapInterface.empty()) {
            return jsonErr(400, "tapInterface is required");
        }

        if (vm->configureNetwork(config)) {
            return jsonOK("Network configured for VM: " + id);
        }
        return jsonErr(500, "Failed to configure network");
    });

    // GET /api/vm/{id}/network
    CROW_ROUTE(app_, "/api/vm/<string>/network")
    .methods("GET"_method)([&hm](const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        auto netCfg = vm->getNetworkConfig();
        crow::json::wvalue j;
        j["tapInterface"] = netCfg.tapInterface;
        j["macAddress"] = netCfg.macAddress;
        j["ipAddress"] = netCfg.ipAddress;
        j["gateway"] = netCfg.gateway;
        j["bridgeName"] = netCfg.bridgeName;
        j["dhcpEnabled"] = netCfg.dhcpEnabled;
        j["mtu"] = netCfg.mtu;

        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // DELETE /api/vm/{id}/network
    CROW_ROUTE(app_, "/api/vm/<string>/network")
    .methods("DELETE"_method)([&hm](const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->removeNetwork()) {
            return jsonOK("Network removed for VM: " + id);
        }
        return jsonErr(500, "Failed to remove network");
    });
*/

// ═════════════════════════════════════════════════════════════════════════════
// TASK 2: PERSISTENCE ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// POST /api/vm/save-config
/*
    CROW_ROUTE(app_, "/api/vm/save-config")
    .methods("POST"_method)([&hm](const crow::request& req) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        std::vector<std::string> vmIds;
        
        if (json && json.has("vmIds") && json["vmIds"].is_list()) {
            for (auto& vm_id : json["vmIds"]) {
                vmIds.push_back(vm_id.s());
            }
        }

        std::string configPath = "/etc/vellum/configs/vms.json";
        // Create directory if needed
        std::system("mkdir -p /etc/vellum/configs");

        bool success = hm.saveAllVMConfigs(configPath);
        if (!success) {
            return jsonErr(500, "Failed to save VM configs");
        }

        crow::json::wvalue j;
        j["success"] = true;
        j["savedPath"] = configPath;
        j["count"] = vmIds.empty() ? hm.listVMs().size() : vmIds.size();

        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // POST /api/vm/load-config
    CROW_ROUTE(app_, "/api/vm/load-config")
    .methods("POST"_method)([&hm](const crow::request& req) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json || !json.has("filePath")) {
            return jsonErr(400, "filePath is required");
        }

        std::string filePath = json["filePath"].s();
        if (!hm.loadAllVMConfigs(filePath)) {
            return jsonErr(500, "Failed to load VM configs from " + filePath);
        }

        crow::json::wvalue j;
        j["success"] = true;
        j["loadedVMs"] = hm.listVMs();
        j["count"] = hm.listVMs().size();

        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // GET /api/vm/{id}/config
    CROW_ROUTE(app_, "/api/vm/<string>/config")
    .methods("GET"_method)([&hm](const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        // Parse VM's serialized config and return as JSON
        std::string configJson = vm->serializeConfig();
        
        crow::response res(configJson);
        res.add_header("Content-Type", "application/json");
        addAPIHeaders(res);
        return res;
    });
*/

// ═════════════════════════════════════════════════════════════════════════════
// TASK 4: CGROUP MANAGEMENT ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// GET /api/vm/{id}/cgroup-status
/*
    CROW_ROUTE(app_, "/api/vm/<string>/cgroup-status")
    .methods("GET"_method)([&hm](const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        crow::json::wvalue j;
        j["cgroupPath"] = vm->getCgroupPath();
        j["cpuLimit"] = -1;    // TODO: Read from cgroup
        j["memoryLimit"] = -1; // TODO: Read from cgroup

        auto metrics = vm->getMetrics();
        j["currentCpuUsage"] = metrics.cpuUsage;
        j["currentMemoryUsage"] = metrics.memoryUsage;

        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // POST /api/vm/{id}/cgroup-update
    CROW_ROUTE(app_, "/api/vm/<string>/cgroup-update")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        bool success = true;
        if (json.has("cpuLimit")) {
            double cpuLimit = json["cpuLimit"].d();
            if (!vm->setCPULimit(cpuLimit)) success = false;
        }

        if (json.has("memoryLimit")) {
            size_t memLimit = static_cast<size_t>(json["memoryLimit"].i());
            if (!vm->setMemoryLimit(memLimit)) success = false;
        }

        if (success) {
            return jsonOK("Cgroup updated for VM: " + id);
        }
        return jsonErr(500, "Failed to update cgroup");
    });
*/

// ═════════════════════════════════════════════════════════════════════════════
// ADVANCED VM MECHANICS ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// POST /api/vm/{id}/snapshot/{name}
/*
    CROW_ROUTE(app_, "/api/vm/<string>/snapshot/<string>")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id, const std::string& snapshotName) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->createSnapshot(snapshotName)) {
            return jsonOK("Snapshot created: " + snapshotName + " for VM: " + id);
        }
        return jsonErr(500, "Failed to create snapshot");
    });
*/

// POST /api/vm/{id}/snapshot/{name}/restore
/*
    CROW_ROUTE(app_, "/api/vm/<string>/snapshot/<string>/restore")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id, const std::string& snapshotName) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->restoreSnapshot(snapshotName)) {
            return jsonOK("Snapshot restored: " + snapshotName + " for VM: " + id);
        }
        return jsonErr(500, "Failed to restore snapshot");
    });
*/

// POST /api/vm/{id}/clone/{newId}
/*
    CROW_ROUTE(app_, "/api/vm/<string>/clone/<string>")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id, const std::string& newId) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        std::string snapshotName = json.has("snapshot") ? json["snapshot"].s() : "latest";

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->cloneVM(newId, snapshotName)) {
            return jsonOK("VM cloned: " + id + " -> " + newId + " using snapshot: " + snapshotName);
        }
        return jsonErr(500, "Failed to clone VM");
    });
*/

// POST /api/vm/{id}/auto-restart/enable
/*
    CROW_ROUTE(app_, "/api/vm/<string>/auto-restart/enable")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        VMInstance::AutoRestartConfig config;
        config.enabled = true;
        config.max_attempts = json.has("maxAttempts") ? static_cast<int>(json["maxAttempts"].i()) : 3;
        config.delay_between_attempts = std::chrono::seconds(
            json.has("delaySeconds") ? static_cast<int>(json["delaySeconds"].i()) : 30);
        config.max_total_runtime = std::chrono::seconds(
            json.has("maxRuntimeSeconds") ? static_cast<int>(json["maxRuntimeSeconds"].i()) : 3600);

        vm->enableAutoRestart(config);
        return jsonOK("Auto-restart enabled for VM: " + id);
    });
*/

// POST /api/vm/{id}/auto-restart/disable
/*
    CROW_ROUTE(app_, "/api/vm/<string>/auto-restart/disable")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        vm->disableAutoRestart();
        return jsonOK("Auto-restart disabled for VM: " + id);
    });
*/

// POST /api/vm/{id}/migrate
/*
    CROW_ROUTE(app_, "/api/vm/<string>/migrate")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        std::string destinationHost = json.has("destinationHost") ? json["destinationHost"].s() : "";
        int port = json.has("port") ? static_cast<int>(json["port"].i()) : 4444;

        if (destinationHost.empty()) {
            return jsonErr(400, "destinationHost required");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->startMigration(destinationHost, port)) {
            return jsonOK("Migration started for VM: " + id + " to " + destinationHost + ":" + std::to_string(port));
        }
        return jsonErr(500, "Failed to start migration");
    });
*/

// GET /api/vm/{id}/performance
/*
    CROW_ROUTE(app_, "/api/vm/<string>/performance")
    ([&hm](const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        auto profile = vm->getDetailedPerformanceProfile();

        crow::json::wvalue j;
        j["cpu"]["userPercent"] = profile.cpu_user_percent;
        j["cpu"]["systemPercent"] = profile.cpu_system_percent;
        j["cpu"]["idlePercent"] = profile.cpu_idle_percent;
        j["memory"]["rssKb"] = static_cast<int>(profile.memory_rss_kb);
        j["memory"]["vszKb"] = static_cast<int>(profile.memory_vsz_kb);
        j["disk"]["readBytes"] = static_cast<int>(profile.disk_read_bytes);
        j["disk"]["writeBytes"] = static_cast<int>(profile.disk_write_bytes);
        j["network"]["rxBytesPerSec"] = profile.network_rx_bytes_per_sec;
        j["network"]["txBytesPerSec"] = profile.network_tx_bytes_per_sec;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            profile.timestamp.time_since_epoch()).count();

        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });
*/

// POST /api/vm/{id}/performance/start
/*
    CROW_ROUTE(app_, "/api/vm/<string>/performance/start")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        vm->startPerformanceMonitoring([](const VMInstance::PerformanceProfile& profile) {
            // In a real implementation, this would stream to WebSocket clients
            std::cout << "Performance update: CPU " << profile.cpu_user_percent << "% user, "
                      << profile.memory_rss_kb << " KB RSS\n";
        });

        return jsonOK("Performance monitoring started for VM: " + id);
    });
*/

// POST /api/vm/{id}/performance/stop
/*
    CROW_ROUTE(app_, "/api/vm/<string>/performance/stop")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        vm->stopPerformanceMonitoring();
        return jsonOK("Performance monitoring stopped for VM: " + id);
    });
*/

// POST /api/vm/{id}/memory/adjust
/*
    CROW_ROUTE(app_, "/api/vm/<string>/memory/adjust")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        size_t newMemoryMB = static_cast<size_t>(json["memoryMB"].i());

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->adjustMemory(newMemoryMB)) {
            return jsonOK("Memory adjusted to " + std::to_string(newMemoryMB) + " MB for VM: " + id);
        }
        return jsonErr(500, "Failed to adjust memory");
    });
*/

// POST /api/vm/{id}/vcpu/add
/*
    CROW_ROUTE(app_, "/api/vm/<string>/vcpu/add")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->addVCPU()) {
            return jsonOK("vCPU added to VM: " + id);
        }
        return jsonErr(500, "Failed to add vCPU");
    });
*/

// POST /api/vm/{id}/vcpu/remove
/*
    CROW_ROUTE(app_, "/api/vm/<string>/vcpu/remove")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        if (vm->removeVCPU()) {
            return jsonOK("vCPU removed from VM: " + id);
        }
        return jsonErr(500, "Failed to remove vCPU");
    });
*/

// POST /api/vm/{id}/qos/configure
/*
    CROW_ROUTE(app_, "/api/vm/<string>/qos/configure")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto auth_header = req.get_header_value("Authorization");
        if (!isValidToken(hm, auth_header)) {
            return jsonErr(401, "Unauthorized");
        }

        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);

        VMInstance::QoSConfig config;
        config.cpu_shares = json.has("cpuShares") ? static_cast<int>(json["cpuShares"].i()) : 1024;
        config.io_weight = json.has("ioWeight") ? static_cast<int>(json["ioWeight"].i()) : 500;
        config.network_priority = json.has("networkPriority") ? static_cast<int>(json["networkPriority"].i()) : 0;
        config.memory_soft_limit = json.has("memorySoftLimit") ? static_cast<size_t>(json["memorySoftLimit"].i()) : 0;

        if (vm->configureQoS(config)) {
            return jsonOK("QoS configured for VM: " + id);
        }
        return jsonErr(500, "Failed to configure QoS");
    });
*/

// ═════════════════════════════════════════════════════════════════════════════
// HELPER FUNCTION FOR TOKEN VALIDATION
// ═════════════════════════════════════════════════════════════════════════════

/*
bool isValidToken(HypervisorManager& hm, const std::string& authHeader) {
    if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
        return false;
    }

    std::string token = authHeader.substr(7);
    return hm.validateToken(token);
}
*/

// ═════════════════════════════════════════════════════════════════════════════
// NOTES FOR INTEGRATION
// ═════════════════════════════════════════════════════════════════════════════

/*
 * To integrate these endpoints into APIServer.cpp:
 *
 * 1. Add helper function isValidToken() before setupRoutes()
 *
 * 2. Uncomment all the route definitions above and add them to setupRoutes()
 *
 * 3. Add this to the top of APIServer.cpp:
 *    #include <nlohmann/json.hpp>  // For JSON parsing
 *
 * 4. For the advanced VM mechanics endpoints, also include:
 *    #include "VMInstance_Mechanics.cpp"  // For advanced VM functionality
 */
 *
 * 4. Update CMakeLists.txt to include nlohmann_json package:
 *    find_package(nlohmann_json REQUIRED)
 *    target_link_libraries(vellum_daemon nlohmann_json::nlohmann_json)
 *
 * 5. Rebuild the project:
 *    cd vellum && mkdir -p build && cd build
 *    cmake .. && make
 */

// ═════════════════════════════════════════════════════════════════════════════
// V2.0: GPU PASSTHROUGH & ACCELERATION ENDPOINTS
// ═════════════════════════════════════════════════════════════════════════════

// GET /api/gpu/available - List all available GPUs
/*
CROW_ROUTE(app_, "/api/gpu/available")
.methods("GET"_method)([&hm](const crow::request& req) {
    extern std::unique_ptr<GPUManager> g_gpu_manager;
    if (!g_gpu_manager) {
        return jsonErr(500, "GPU manager not initialized");
    }

    auto devices = g_gpu_manager->enumerateGPUs();

    crow::json::wvalue j;
    j["success"] = true;
    j["devices"] = crow::json::wvalue::list();

    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        crow::json::wvalue device_json;
        device_json["id"] = device.id;
        device_json["name"] = device.name;
        device_json["vendor"] = device.vendor == GPUVendor::NVIDIA ? "nvidia" :
                               device.vendor == GPUVendor::AMD ? "amd" :
                               device.vendor == GPUVendor::INTEL ? "intel" : "unknown";
        device_json["vendor_id"] = device.vendor_id;
        device_json["device_id"] = device.device_id;
        device_json["memory_mb"] = device.memory_mb;
        device_json["supports_passthrough"] = device.supports_passthrough;
        device_json["currently_attached"] = device.currently_attached;
        device_json["driver"] = device.driver;
        j["devices"][i] = std::move(device_json);
    }

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});

// GET /api/gpu/{id}/metrics - Get GPU metrics
/*
CROW_ROUTE(app_, "/api/gpu/<string>")
.methods("GET"_method)([&hm](const crow::request& req, const std::string& gpu_id) {
    extern std::unique_ptr<GPUManager> g_gpu_manager;
    if (!g_gpu_manager) {
        return jsonErr(500, "GPU manager not initialized");
    }

    auto metrics = g_gpu_manager->getGPUMetrics(gpu_id);
    if (!metrics) {
        return jsonErr(404, "GPU not found or metrics unavailable");
    }

    crow::json::wvalue j;
    j["success"] = true;
    j["gpu_id"] = metrics->gpu_id;
    j["utilization_percent"] = metrics->utilization_percent;
    j["memory_utilization_percent"] = metrics->memory_utilization_percent;
    j["memory_used_mb"] = metrics->memory_used_mb;
    j["memory_total_mb"] = metrics->memory_total_mb;
    j["temperature_celsius"] = metrics->temperature_celsius;
    j["power_draw_watts"] = metrics->power_draw_watts;
    j["fan_speed_percent"] = metrics->fan_speed_percent;
    j["clock_speed_mhz"] = metrics->clock_speed_mhz;

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});

// POST /api/vm/{id}/gpu/attach - Attach GPU to VM
/*
CROW_ROUTE(app_, "/api/vm/<string>/gpu/attach")
.methods("POST"_method)([&hm](const crow::request& req, const std::string& vm_id) {
    auto json = crow::json::load(req.body);
    if (!json) return jsonErr(400, "Invalid JSON body");

    std::string gpu_id = json.has("gpu_id") ? json["gpu_id"].s() : "";
    bool enable_vgpu = json.has("enable_vgpu") ? json["enable_vgpu"].b() : false;

    if (gpu_id.empty()) {
        return jsonErr(400, "gpu_id is required");
    }

    auto vm = hm.getVM(vm_id);
    if (!vm) {
        return jsonErr(404, "VM not found");
    }

    VMInstance::GPUConfig config;
    config.gpu_id = gpu_id;
    config.enable_vgpu = enable_vgpu;

    if (json.has("vgpu_memory_mb")) {
        config.vgpu_memory_mb = json["vgpu_memory_mb"].i();
    }
    if (json.has("vgpu_profiles")) {
        config.vgpu_profiles = json["vgpu_profiles"].i();
    }

    if (!vm->attachGPU(config)) {
        return jsonErr(500, "Failed to attach GPU to VM");
    }

    crow::json::wvalue j;
    j["success"] = true;
    j["message"] = "GPU attached successfully";
    j["gpu_id"] = gpu_id;
    j["vm_id"] = vm_id;

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});

// DELETE /api/vm/{id}/gpu/{gpu_id} - Detach GPU from VM
/*
CROW_ROUTE(app_, "/api/vm/<string>/gpu/<string>")
.methods("DELETE"_method)([&hm](const crow::request& req, const std::string& vm_id, const std::string& gpu_id) {
    auto vm = hm.getVM(vm_id);
    if (!vm) {
        return jsonErr(404, "VM not found");
    }

    if (!vm->detachGPU(gpu_id)) {
        return jsonErr(500, "Failed to detach GPU from VM");
    }

    crow::json::wvalue j;
    j["success"] = true;
    j["message"] = "GPU detached successfully";
    j["gpu_id"] = gpu_id;
    j["vm_id"] = vm_id;

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});

// GET /api/vm/{id}/gpu - Get attached GPUs for VM
/*
CROW_ROUTE(app_, "/api/vm/<string>/gpu")
.methods("GET"_method)([&hm](const crow::request& req, const std::string& vm_id) {
    auto vm = hm.getVM(vm_id);
    if (!vm) {
        return jsonErr(404, "VM not found");
    }

    auto attached_gpus = vm->getAttachedGPUs();
    auto gpu_metrics = vm->getGPUMetrics();

    crow::json::wvalue j;
    j["success"] = true;
    j["vm_id"] = vm_id;
    j["attached_gpus"] = crow::json::wvalue::list();

    for (size_t i = 0; i < attached_gpus.size(); ++i) {
        j["attached_gpus"][i] = attached_gpus[i];
    }

    j["gpu_metrics"] = crow::json::wvalue::list();
    for (size_t i = 0; i < gpu_metrics.size(); ++i) {
        const auto& metrics = gpu_metrics[i];
        crow::json::wvalue metrics_json;
        metrics_json["gpu_id"] = metrics.gpu_id;
        metrics_json["utilization_percent"] = metrics.utilization_percent;
        metrics_json["memory_utilization_percent"] = metrics.memory_utilization_percent;
        metrics_json["memory_used_mb"] = metrics.memory_used_mb;
        metrics_json["memory_total_mb"] = metrics.memory_total_mb;
        metrics_json["temperature_celsius"] = metrics.temperature_celsius;
        metrics_json["power_draw_watts"] = metrics.power_draw_watts;
        metrics_json["fan_speed_percent"] = metrics.fan_speed_percent;
        metrics_json["clock_speed_mhz"] = metrics.clock_speed_mhz;
        j["gpu_metrics"][i] = std::move(metrics_json);
    }

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});

// POST /api/gpu/check-iommu - Check IOMMU status
/*
CROW_ROUTE(app_, "/api/gpu/check-iommu")
.methods("POST"_method)([&hm](const crow::request& req) {
    extern std::unique_ptr<GPUManager> g_gpu_manager;
    if (!g_gpu_manager) {
        return jsonErr(500, "GPU manager not initialized");
    }

    bool iommu_enabled = g_gpu_manager->checkIOMMUEnabled();

    crow::json::wvalue j;
    j["success"] = true;
    j["iommu_enabled"] = iommu_enabled;
    j["message"] = iommu_enabled ? "IOMMU is enabled" : "IOMMU is not enabled - GPU passthrough may not work";

    crow::response res(j);
    addAPIHeaders(res);
    return res;
});
