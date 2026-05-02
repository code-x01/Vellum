#include <filesystem>
#include <iostream>
#include "HypervisorManager.h"
#include "APIServer.h"

int main(int argc, char* argv[]) {
    std::cout << "Starting Vellum..." << std::endl;

    // Determine frontend build directory relative to the executable
    std::filesystem::path exe_path = std::filesystem::canonical(argv[0]);
    std::filesystem::path frontend_build_dir = exe_path.parent_path() / ".." / "frontend" / "build";
    frontend_build_dir = std::filesystem::weakly_canonical(frontend_build_dir);

    // Initialize hypervisor manager
    auto& hm = HypervisorManager::getInstance();

    // Use the frontend build folder as the current working directory so Crow's default
    // static route can serve /static/<path> assets from the build output.
    std::filesystem::current_path(frontend_build_dir);

    // Start API server
    APIServer server(frontend_build_dir.string());
    server.run();

    return 0;
}