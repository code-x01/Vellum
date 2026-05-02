#ifndef VELLUM_APISERVER_H
#define VELLUM_APISERVER_H

#include <crow.h>
#include "VMInstance.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>

class APIServer {
public:
    APIServer(const std::string& frontend_build_dir);
    ~APIServer();

    void run();

private:
    crow::SimpleApp app_;
    std::string frontend_build_dir_;
    std::mutex connections_mutex_;
    std::unordered_map<std::string, std::unordered_set<crow::websocket::connection*>> console_connections_;
    std::unordered_map<crow::websocket::connection*, std::string> connection_vm_map_;
    std::unordered_set<crow::websocket::connection*> telemetry_connections_;

    void setupRoutes();
    void broadcastConsoleOutput(const std::string& vm_id, const std::string& data);
    void broadcastTelemetry(const std::string& vm_id, const VMInstance::Metrics& metrics);
    std::function<void(const std::string&, const std::string&)> console_broadcast_callback_;
};

#endif // VELLUM_APISERVER_H