# Script to remove 32-bit and debug DLLs after windeployqt
# Called from CMakeLists.txt deploy_runtime_dependencies function

# Get current working directory (bin/Release)
set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

message(STATUS "Scanning for 32-bit and debug DLLs in: ${DEPLOY_DIR}")

# Find all DLLs in current directory (non-recursive)
file(GLOB all_dlls "${DEPLOY_DIR}/*.dll")

foreach(dll ${all_dlls})
    get_filename_component(dll_name "${dll}" NAME)
    
    # Skip directories, only check files
    if(NOT IS_DIRECTORY "${dll}")
        # Read PE header to check architecture
        file(READ "${dll}" hex_data OFFSET 0 LIMIT 64 HEX)
        
        # Check for debug suffix patterns
        if(dll_name MATCHES "d\\.dll$|d_.*\\.dll$|_codecvt|_clr|_threads|_atomic|_2\\.dll$")
            message(STATUS "Removing debug/codec DLL: ${dll_name}")
            file(REMOVE "${dll}")
        endif()
    endif()
endforeach()

message(STATUS "DLL cleanup complete")
