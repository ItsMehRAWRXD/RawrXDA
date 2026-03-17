# MASM Win32 IDE - Minimal Build for Phase 5 & 6
# Only includes main.asm and its direct dependencies

cmake_minimum_required(VERSION 3.20)
project(RawrXDWin32MASM LANGUAGES ASM_MASM)

# Ensure linker is set when only ASM is present
set(CMAKE_LINKER "link.exe")
set(CMAKE_ASM_MASM_STANDARD_LIBRARIES "user32.lib gdi32.lib kernel32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib shlwapi.lib advapi32.lib psapi.lib")

# Essential MASM sources (enterprise foundations + enhanced GGUF + stubs)
set(MASM_SOURCES
    src/main_stub.asm
)

# Create executable
add_executable(${PROJECT_NAME} ${MASM_SOURCES})

# Add Windows SDK lib paths for MASM linker
if(MSVC)
    # Get Windows SDK path from registry or environment
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # 32-bit
        set(WINDOWS_SDK_LIB_PATH "C:/Program Files (x86)/Windows Kits/10/Lib")
    else()
        # 64-bit
        set(WINDOWS_SDK_LIB_PATH "C:/Program Files/Windows Kits/10/Lib")
    endif()
    
    # Try to find the actual SDK lib directory
    file(GLOB SDK_VERSIONS "${WINDOWS_SDK_LIB_PATH}/*")
    if(SDK_VERSIONS)
        list(SORT SDK_VERSIONS REVERSE)
        list(GET SDK_VERSIONS 0 LATEST_SDK)
        set(WINDOWS_SDK_LIB_PATH "${LATEST_SDK}/um/x86")
    endif()
    
    target_link_directories(${PROJECT_NAME} PRIVATE "${WINDOWS_SDK_LIB_PATH}")
endif()

# Link Windows libraries with proper formatting
target_link_libraries(${PROJECT_NAME} PRIVATE
    user32.lib kernel32.lib gdi32.lib shell32.lib 
    comctl32.lib comdlg32.lib ole32.lib shlwapi.lib 
    advapi32.lib psapi.lib
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Output
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

message(STATUS "✅ MASM IDE Configured: ${PROJECT_NAME}")
message(STATUS "📦 Phase 5 & 6: Logging + Performance")
