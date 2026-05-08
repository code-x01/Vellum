cmake_minimum_required(VERSION 3.16)
project(Vellum VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find Crow ──────────────────────────────────────────────────────────────────
# Crow can be installed via apt (libcrow-dev) or built from source.
# If installed via pkg-config:
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CROW QUIET crow)
endif()

# Fallback: look for crow.h in common locations
if(NOT CROW_FOUND)
    find_path(CROW_INCLUDE_DIRS crow.h
        PATHS /usr/include /usr/local/include
              /usr/include/crow /usr/local/include/crow
    )
    if(CROW_INCLUDE_DIRS)
        set(CROW_FOUND TRUE)
        message(STATUS "Found Crow headers at ${CROW_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR
            "Crow C++ web framework not found.\n"
            "Install with: sudo apt install libcrow-dev\n"
            "or: git clone https://github.com/CrowCpp/Crow.git && "
            "cp Crow/include/crow.h /usr/local/include/")
    endif()
endif()

# ── Find Boost (Crow dependency) ───────────────────────────────────────────────
find_package(Boost REQUIRED COMPONENTS system)

# ── Find OpenSSL (optional, for Crow HTTPS/WSS) ───────────────────────────────
find_package(OpenSSL QUIET)

# ═════════════════════════════════════════════════════════════════════════════
# UPGRADES: Find nlohmann_json for configuration persistence (Task 2)
# ═════════════════════════════════════════════════════════════════════════════
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING
        "nlohmann_json not found. Install with: "
        "sudo apt install nlohmann-json3-dev (Ubuntu/Debian) or "
        "vcpkg install nlohmann-json (Vcpkg)")
    # Continue anyway - JSON parsing is optional but recommended
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/VMInstance.cpp
    src/VMInstance_Upgrades.cpp
    src/VMInstance_Mechanics.cpp
    src/HypervisorManager.cpp
    src/HypervisorManager_Upgrades.cpp
    src/KVMRunLoop.cpp
    src/APIServer.cpp
    src/cgroup/CgroupManager.cpp
    # WebSocketBridge.cpp is documentation-only, not compiled
    # APIServer_Upgrades.cpp: Endpoints defined in this file for integration
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(vellum ${SOURCES})

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(vellum PRIVATE
    include
    ${CROW_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(vellum PRIVATE
    ${Boost_LIBRARIES}
    pthread
)

# Link nlohmann_json if available
if(nlohmann_json_FOUND)
    target_link_libraries(vellum PRIVATE nlohmann_json::nlohmann_json)
endif()

if(OpenSSL_FOUND)
    target_link_libraries(vellum PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
target_compile_options(vellum PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
    # Suppress known noisy warnings from Crow/Boost headers
    -Wno-unused-parameter
)

# ── KVM availability check ────────────────────────────────────────────────────
if(NOT EXISTS "/usr/include/linux/kvm.h")
    message(WARNING
        "/usr/include/linux/kvm.h not found.\n"
        "Install with: sudo apt install linux-headers-$(uname -r)")
endif()

# ── Build info ────────────────────────────────────────────────────────────────
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Vellum version: ${PROJECT_VERSION}")