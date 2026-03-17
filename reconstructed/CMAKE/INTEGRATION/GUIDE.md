# CMakeLists.txt Integration Guide for Universal Model Router

## Adding to CMakeLists.txt

Add these sources to your `set(CORE_SOURCES ...)` section:

```cmake
# Core library sources
set(CORE_SOURCES
    # ... existing sources ...
    
    # Universal Model Router Components (NEW)
    universal_model_router.cpp
    cloud_api_client.cpp
    model_interface.cpp
    
    # ... existing sources ...
)
```

## Complete Updated CMakeLists.txt Section

```cmake
# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# Core library sources
set(CORE_SOURCES
    # Existing autonomous components
    autonomous_model_manager.cpp
    intelligent_codebase_engine.cpp
    autonomous_feature_engine.cpp
    error_recovery_system.cpp
    performance_monitor.cpp
    
    # New Universal Model Router
    universal_model_router.cpp
    cloud_api_client.cpp
    model_interface.cpp
    
    # UI components
    autonomous_widgets.cpp
    ide_main_window.cpp
    main.cpp
)

# Headers
set(CORE_HEADERS
    # Existing headers
    autonomous_model_manager.h
    intelligent_codebase_engine.h
    autonomous_feature_engine.h
    error_recovery_system.h
    performance_monitor.h
    
    # New headers (NO .cpp, just headers for inclusion)
    universal_model_router.h
    cloud_api_client.h
    model_interface.h
    
    # UI headers
    autonomous_widgets.h
    ide_main_window.h
)

# Create the main executable
add_executable(AutonomousIDE
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Link libraries
target_link_libraries(AutonomousIDE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(AutonomousIDE wsock32 ws2_32)
endif()

# Test executable (optional)
add_executable(AutonomousIDETests
    ${CORE_SOURCES}
    test_agent_hot_patcher_qtest.cpp
)

target_link_libraries(AutonomousIDETests
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    Qt6::Test
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)
```

## Building from Command Line

### Windows (PowerShell)
```powershell
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -G "Visual Studio 17 2022" -A x64 ..

# Build
cmake --build . --config Release

# Run
.\Release\AutonomousIDE.exe
```

### Linux/macOS
```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Run
./AutonomousIDE
```

## Compile Verification

To verify all components compile correctly:

```bash
# Just compile without linking (syntax check)
cmake --build . --target <filename> --verbose

# Build specific target
cmake --build . --target model_interface --config Release
```

## Dependencies

The new components require:

1. **Qt6 Network** (for HTTP requests)
   - Included by `find_package(Qt6 ... REQUIRED COMPONENTS Network)`

2. **OpenSSL** (for HTTPS)
   - Included by `find_package(OpenSSL REQUIRED)`

3. **C++20 Features**
   - std::unique_ptr
   - std::shared_ptr
   - std::function
   - std::map
   - Already enabled: `set(CMAKE_CXX_STANDARD 20)`

4. **Qt6 JSON Support**
   - Built into Qt6::Core

## No Additional Dependencies Needed

The implementation uses only standard Qt6 libraries already in your project:
- ✅ Qt6::Core (JSON, threading, QObject)
- ✅ Qt6::Network (QNetworkAccessManager, QNetworkRequest)
- ✅ OpenSSL (HTTPS support)
- ✅ Standard C++ (memory, functional, map)

## Configuration Files

Ensure these files are in the build output directory:

```
build/
├── Release/
│   ├── AutonomousIDE.exe (or AutonomousIDE on Linux)
│   └── model_config.json  ← COPY THIS FILE
└── model_config.json      ← OR HERE
```

Copy the configuration file:

```bash
# Windows
copy ..\model_config.json Release\

# Linux/macOS
cp ../model_config.json ./
```

## Build Type Specific Settings

The CMakeLists.txt already has optimized settings:

```cmake
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
```

For the new components:
- Debug builds: Full symbols, ASan enabled (catches memory errors)
- Release builds: Full optimization, native instruction set

## Optional: Structured Logging with nlohmann_json

If you want prettier JSON logging, optionally add:

```cmake
# Add nlohmann_json header-only library (optional, for pretty printing)
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp)
FetchContent_MakeAvailable(json)

target_link_libraries(AutonomousIDE nlohmann_json::nlohmann_json)
```

## Performance Build Options

For maximum performance on specific hardware:

```cmake
# AVX2 support (modern CPUs)
if(MSVC)
    target_compile_options(AutonomousIDE PRIVATE /arch:AVX2)
else()
    target_compile_options(AutonomousIDE PRIVATE -mavx2 -mfma)
endif()

# LTO (Link Time Optimization) - Release only
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
```

## Incremental Compilation

For faster development builds:

```bash
# After first build, subsequent builds are much faster:
cmake --build . --parallel 8

# Or for Ninja (faster):
cmake -G Ninja ..
cmake --build .
```

## Troubleshooting Build Issues

### Qt6 not found
```cmake
# Explicitly set Qt path
set(Qt6_DIR "C:/Qt/6.5.0/msvc2022_64/lib/cmake/Qt6")
```

### Missing headers
```
Make sure all .h files are in CMAKE_CURRENT_SOURCE_DIR
Verify #include paths use relative paths or full paths
```

### Link errors
```
Check all target_link_libraries() calls
Verify library order (some linkers are sensitive to order)
Use --verbose flag to see full link command
```

### Runtime errors
```
Ensure model_config.json is in working directory
Check file permissions
Verify Qt plugins are in right location
```

## Next Steps After Build

1. **Run the IDE**
   ```bash
   ./AutonomousIDE
   ```

2. **Verify Models Load**
   - Check console for "Initialized with X models"
   - Check model_config.json is accessible

3. **Test a Model**
   - Use local model first (no API key needed)
   - Then test cloud models with real API keys

4. **Monitor Performance**
   - Check console logs for latency
   - Monitor memory usage
   - Track cost if using cloud models

## Continuous Integration

For CI/CD pipelines (GitHub Actions, GitLab CI):

```yaml
# Example GitHub Actions
name: Build and Test
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install Dependencies
        run: sudo apt-get install -y qt6-base-dev libssl-dev zlib1g-dev
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          cmake --build . --parallel 4
      - name: Test
        run: cd build && ctest --output-on-failure
```

## Summary

The Universal Model Router integrates seamlessly into your existing CMake build system:

1. ✅ No new external dependencies required
2. ✅ Uses existing Qt6 and OpenSSL libraries
3. ✅ Follows your existing C++20 standard setting
4. ✅ Compiles with your existing optimization flags
5. ✅ Works on Windows, Linux, and macOS
6. ✅ Supports both Debug and Release builds
7. ✅ Compatible with your existing code structure
