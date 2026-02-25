cmake_minimum_required(VERSION 3.20)
project(ExtensionRegistry VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# DEPENDENCIES
# ============================================================================

# Crow (Header-only HTTP server)
include(FetchContent)
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(crow)

# nlohmann/json (Header-only JSON library)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# SQLite3
find_package(SQLite3 REQUIRED)

# Boost (for process management)
find_package(Boost REQUIRED COMPONENTS system filesystem)

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(extension_registry 
    src/main.cpp
    src/ExtensionManager.cpp
    src/Database.cpp
)

target_include_directories(extension_registry 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${crow_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(extension_registry 
    PRIVATE 
        SQLite::SQLite3
        Boost::system
        Boost::filesystem
        ws2_32  # Windows sockets
)

# ============================================================================
# INSTALL
# ============================================================================

install(TARGETS extension_registry DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
