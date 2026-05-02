/*
 * APIServer.cpp — REST + WebSocket API server for Vellum.
 *
 * Improvements over original:
 *  - Correct JSON Content-Type on all responses.
 *  - CORS headers added so the React dev-server at :3000 can call :8080.
 *  - Added pause / resume endpoints (per APISchema.h).
 *  - Added global metrics endpoint GET /api/vm/metrics/global.
 *  - Added resource-limit endpoint POST /api/vm/{id}/limits.
 *  - VM list now returns full state string (Starting / Running / Paused / Error).
 *  - Static file serving uses an SPA fallback (any unknown path → index.html).
 *  - WebSocket telemetry connection cleanup is more robust.
 *  - Console WebSocket now sends welcome banner on connect.
 */

#include "APIServer.h"
#include "HypervisorManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* CT_JSON = "application/json";

static std::string guessMimeType(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".json")) return "application/json";
    if (path.ends_with(".ico"))  return "image/x-icon";
    if (path.ends_with(".svg"))  return "image/svg+xml";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".woff2"))return "font/woff2";
    if (path.ends_with(".woff")) return "font/woff";
    if (path.ends_with(".ttf"))  return "font/ttf";
    return "application/octet-stream";
}

static crow::response serveFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return crow::response(404);
    std::ostringstream buf;
    buf << file.rdbuf();
    crow::response res(buf.str());
    res.add_header("Content-Type", guessMimeType(path));
    return res;
}

static std::string stateToString(VMInstance::State s) {
    switch (s) {
        case VMInstance::State::Stopped:  return "Stopped";
        case VMInstance::State::Starting: return "Starting";
        case VMInstance::State::Running:  return "Running";
        case VMInstance::State::Paused:   return "Paused";
        case VMInstance::State::Error:    return "Error";
    }
    return "Unknown";
}

// Adds common headers (CORS + Content-Type) to every API response
static void addAPIHeaders(crow::response& res) {
    res.add_header("Content-Type", CT_JSON);
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
}

static crow::response jsonOK(const std::string& message) {
    crow::response res(200, "{\"success\":true,\"message\":\"" + message + "\"}");
    addAPIHeaders(res);
    return res;
}

static crow::response jsonErr(int code, const std::string& message) {
    crow::response res(code, "{\"success\":false,\"message\":\"" + message + "\"}");
    addAPIHeaders(res);
    return res;
}

// ── APIServer ─────────────────────────────────────────────────────────────────

APIServer::APIServer(const std::string& frontend_build_dir)
    : frontend_build_dir_(frontend_build_dir) {
    setupRoutes();
}

APIServer::~APIServer() {}

void APIServer::setupRoutes() {
    auto& hm = HypervisorManager::getInstance();

    // ── CORS pre-flight ────────────────────────────────────────────────────
    CROW_ROUTE(app_, "/api/<path>")
    .methods("OPTIONS"_method)([](const crow::request&, const std::string&) {
        crow::response res(204);
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // ── POST /api/vm/create ───────────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/create")
    .methods("POST"_method)([this, &hm](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON body");

        std::string id          = json.has("id")            ? json["id"].s()            : "";
        std::string kernelPath  = json.has("kernelPath")    ? json["kernelPath"].s()    : "";
        std::string initrdPath  = json.has("initrdPath")    ? json["initrdPath"].s()    : "";
        std::string diskPath    = json.has("diskPath")      ? json["diskPath"].s()      : "";
        std::string cmdline     = json.has("kernelCmdline") ? json["kernelCmdline"].s() : "";
        size_t memMB  = json.has("memoryMB") ? static_cast<size_t>(json["memoryMB"].i()) : 256;
        int    vcpus  = json.has("vcpus")    ? static_cast<int>(json["vcpus"].i())        : 1;

        if (id.empty())         return jsonErr(400, "id is required");
        if (kernelPath.empty()) return jsonErr(400, "kernelPath is required");
        if (memMB < 64)         return jsonErr(400, "memoryMB must be >= 64");
        if (vcpus < 1)          return jsonErr(400, "vcpus must be >= 1");

        auto vm = hm.createVM(id, kernelPath, initrdPath, diskPath, cmdline, memMB, vcpus);
        if (!vm) return jsonErr(409, "VM already exists or creation failed");

        // Wire up real-time streaming callbacks
        hm.setVMConsoleCallback(id, [this, id](const std::string& data) {
            broadcastConsoleOutput(id, data);
        });
        hm.setVMTelemetryCallback(id, [this, id](const VMInstance::Metrics& m) {
            broadcastTelemetry(id, m);
        });

        return jsonOK("VM created: " + id);
    });

    // ── POST /api/vm/{id}/start ───────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/start")
    .methods("POST"_method)([&hm](const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        if (vm->start()) return jsonOK("VM started: " + id);
        return jsonErr(500, "Failed to start VM: " + id);
    });

    // ── POST /api/vm/{id}/stop ────────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/stop")
    .methods("POST"_method)([&hm](const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        if (vm->stop()) return jsonOK("VM stopped: " + id);
        return jsonErr(500, "Failed to stop VM: " + id);
    });

    // ── POST /api/vm/{id}/pause ───────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/pause")
    .methods("POST"_method)([&hm](const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        if (vm->pause()) return jsonOK("VM paused: " + id);
        return jsonErr(409, "VM is not running: " + id);
    });

    // ── POST /api/vm/{id}/resume ──────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/resume")
    .methods("POST"_method)([&hm](const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        if (vm->resume()) return jsonOK("VM resumed: " + id);
        return jsonErr(409, "VM is not paused: " + id);
    });

    // ── DELETE /api/vm/{id} ───────────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>")
    .methods("DELETE"_method)([&hm](const std::string& id) {
        if (hm.destroyVM(id)) return jsonOK("VM destroyed: " + id);
        return jsonErr(404, "VM not found: " + id);
    });

    // ── GET /api/vm/{id}/metrics ──────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/metrics")
    .methods("GET"_method)([&hm](const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        auto m = vm->getMetrics();
        crow::json::wvalue j;
        j["cpuUsage"]    = m.cpuUsage;
        j["memoryUsage"] = m.memoryUsage;
        j["diskUsage"]   = m.diskUsage;
        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // ── GET /api/vm/metrics/global ────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/metrics/global")
    .methods("GET"_method)([&hm]() {
        auto gm = hm.getGlobalMetrics();
        crow::json::wvalue j;
        j["totalMemoryMB"] = gm.totalMemoryMB;
        j["usedMemoryMB"]  = gm.usedMemoryMB;
        j["totalVCPUs"]    = gm.totalVCPUs;
        j["usedVCPUs"]     = gm.usedVCPUs;
        crow::response res(j);
        addAPIHeaders(res);
        return res;
    });

    // ── GET /api/vm/list ──────────────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/list")
    .methods("GET"_method)([&hm]() {
        auto ids = hm.listVMs();
        crow::json::wvalue arr = crow::json::wvalue::list();
        for (size_t i = 0; i < ids.size(); ++i) {
            auto vm = hm.getVM(ids[i]);
            if (!vm) continue;
            crow::json::wvalue entry;
            entry["id"]     = vm->getId();
            entry["state"]  = stateToString(vm->getState());
            entry["memory"] = vm->getMemoryMB();
            entry["vcpus"]  = vm->getVCPUs();
            arr[i]          = std::move(entry);
        }
        crow::response res(arr);
        addAPIHeaders(res);
        return res;
    });

    // ── POST /api/vm/{id}/limits ──────────────────────────────────────────
    CROW_ROUTE(app_, "/api/vm/<string>/limits")
    .methods("POST"_method)([&hm](const crow::request& req, const std::string& id) {
        auto vm = hm.getVM(id);
        if (!vm) return jsonErr(404, "VM not found: " + id);
        auto json = crow::json::load(req.body);
        if (!json) return jsonErr(400, "Invalid JSON");
        if (json.has("cpuLimit"))    vm->setCPULimit(json["cpuLimit"].d());
        if (json.has("memoryLimit")) vm->setMemoryLimit(static_cast<size_t>(json["memoryLimit"].i()));
        return jsonOK("Limits applied");
    });

    // ── WebSocket /ws/console/{id} ────────────────────────────────────────
    CROW_WEBSOCKET_ROUTE(app_, "/ws/console/<string>")
    .onopen([this, &hm](crow::websocket::connection& conn, const std::string& vm_id) {
        {
            std::lock_guard<std::mutex> lk(connections_mutex_);
            console_connections_[vm_id].insert(&conn);
            connection_vm_map_[&conn] = vm_id;
        }
        // Send initial banner
        crow::json::wvalue banner;
        banner["type"] = "output";
        banner["data"] = "\r\n\x1B[1;36m[Vellum] Console attached to VM: " + vm_id + "\x1B[0m\r\n";
        conn.send_text(banner.dump());

        // Flush any buffered console output
        auto vm = hm.getVM(vm_id);
        if (vm) {
            std::string backlog = vm->readConsoleOutput();
            if (!backlog.empty()) {
                crow::json::wvalue msg;
                msg["type"] = "output";
                msg["data"] = backlog;
                conn.send_text(msg.dump());
            }
        }
        std::cout << "[ws] Console connected: " << vm_id << "\n";
    })
    .onmessage([this, &hm](crow::websocket::connection& conn,
                            const std::string& data, bool /*is_binary*/) {
        std::string vm_id;
        {
            std::lock_guard<std::mutex> lk(connections_mutex_);
            auto it = connection_vm_map_.find(&conn);
            if (it == connection_vm_map_.end()) return;
            vm_id = it->second;
        }
        auto json = crow::json::load(data);
        if (json && json["type"] == "input") {
            auto vm = hm.getVM(vm_id);
            if (vm) vm->sendConsoleInput(json["data"].s());
        }
    })
    .onclose([this](crow::websocket::connection& conn,
                    const std::string& /*reason*/, unsigned short /*code*/) {
        std::lock_guard<std::mutex> lk(connections_mutex_);
        auto cit = connection_vm_map_.find(&conn);
        if (cit != connection_vm_map_.end()) {
            const std::string& vm_id = cit->second;
            auto sit = console_connections_.find(vm_id);
            if (sit != console_connections_.end()) {
                sit->second.erase(&conn);
                if (sit->second.empty()) console_connections_.erase(sit);
            }
            connection_vm_map_.erase(cit);
        }
        std::cout << "[ws] Console disconnected\n";
    });

    // ── WebSocket /ws/telemetry ───────────────────────────────────────────
    CROW_WEBSOCKET_ROUTE(app_, "/ws/telemetry")
    .onopen([this](crow::websocket::connection& conn) {
        std::lock_guard<std::mutex> lk(connections_mutex_);
        telemetry_connections_.insert(&conn);
        std::cout << "[ws] Telemetry client connected\n";
    })
    .onclose([this](crow::websocket::connection& conn,
                    const std::string& /*reason*/, unsigned short /*code*/) {
        std::lock_guard<std::mutex> lk(connections_mutex_);
        telemetry_connections_.erase(&conn);
        std::cout << "[ws] Telemetry client disconnected\n";
    });

    // ── Static frontend ───────────────────────────────────────────────────
    // Serve index.html for the root and any unknown path (SPA routing)
    CROW_ROUTE(app_, "/")([this]() {
        return serveFile(frontend_build_dir_ + "/index.html");
    });

    CROW_ROUTE(app_, "/favicon.ico")([this]() {
        return serveFile(frontend_build_dir_ + "/favicon.ico");
    });

    // Serve built static assets
    CROW_ROUTE(app_, "/static/<path>")([this](const std::string& path) {
        return serveFile(frontend_build_dir_ + "/static/" + path);
    });

    // SPA catch-all — any non-API path serves index.html
    CROW_ROUTE(app_, "/<path>")([this](const std::string& path) {
        std::string full = frontend_build_dir_ + "/" + path;
        if (std::filesystem::exists(full))
            return serveFile(full);
        return serveFile(frontend_build_dir_ + "/index.html");
    });
}

// ── Broadcast helpers ─────────────────────────────────────────────────────────

void APIServer::broadcastConsoleOutput(const std::string& vm_id, const std::string& data) {
    std::lock_guard<std::mutex> lk(connections_mutex_);
    auto it = console_connections_.find(vm_id);
    if (it == console_connections_.end() || it->second.empty()) return;

    crow::json::wvalue msg;
    msg["type"] = "output";
    msg["data"] = data;
    std::string s = msg.dump();
    for (auto* conn : it->second)
        conn->send_text(s);
}

void APIServer::broadcastTelemetry(const std::string& vm_id, const VMInstance::Metrics& m) {
    std::lock_guard<std::mutex> lk(connections_mutex_);
    if (telemetry_connections_.empty()) return;

    crow::json::wvalue msg;
    msg["type"]        = "metrics";
    msg["vmId"]        = vm_id;
    msg["cpuUsage"]    = m.cpuUsage;
    msg["memoryUsage"] = m.memoryUsage;
    msg["diskUsage"]   = m.diskUsage;
    std::string s = msg.dump();
    for (auto* conn : telemetry_connections_)
        conn->send_text(s);
}

// ── Server run ────────────────────────────────────────────────────────────────

void APIServer::run() {
    std::cout << "[Vellum] API server listening on http://0.0.0.0:8080\n";
    app_.port(8080).multithreaded().run();
}