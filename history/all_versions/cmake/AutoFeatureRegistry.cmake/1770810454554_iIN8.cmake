# =============================================================================
# AutoFeatureRegistry.cmake — CMake pre-build auto-registration integration
# =============================================================================
# Include this in your CMakeLists.txt to auto-regenerate command registrations
# whenever source IDM_* defines or handler declarations change.
#
# Usage in CMakeLists.txt:
#   include(cmake/AutoFeatureRegistry.cmake)
#   setup_auto_feature_registry(RawrEngine)
#   setup_auto_feature_registry(RawrXD-Win32IDE)
#
# Architecture: C++20, Win32, no Qt, no exceptions
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

# Find Python
find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_FOUND)
    find_program(PYTHON_EXECUTABLE python python3)
    if(NOT PYTHON_EXECUTABLE)
        message(WARNING "[AutoFeatureRegistry] Python not found — auto-registration disabled")
        set(AUTO_FEATURE_REGISTRY_ENABLED OFF)
        return()
    endif()
else()
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
endif()

set(AUTO_FEATURE_REGISTRY_ENABLED ON)
set(AUTO_REG_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/auto_register_commands.py")
set(AUTO_REG_OUTPUT_HPP "${CMAKE_CURRENT_SOURCE_DIR}/src/core/auto_feature_registry.hpp")
set(AUTO_REG_OUTPUT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/src/core/auto_feature_registry.cpp")

# Source files that trigger regeneration when modified
set(AUTO_REG_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/win32app/Win32IDE.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/feature_handlers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/feature_registration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/shared_feature_dispatch.h"
    "${AUTO_REG_SCRIPT}"
)

# Custom command: regenerate auto_feature_registry.{hpp,cpp}
add_custom_command(
    OUTPUT "${AUTO_REG_OUTPUT_HPP}" "${AUTO_REG_OUTPUT_CPP}"
    COMMAND ${PYTHON_EXECUTABLE} "${AUTO_REG_SCRIPT}"
            --src-root "${CMAKE_CURRENT_SOURCE_DIR}/src"
    DEPENDS ${AUTO_REG_DEPENDS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "[AutoReg] Regenerating command registrations..."
    VERBATIM
)

# Custom target for manual regeneration: cmake --build . --target regenerate-registry
add_custom_target(regenerate-registry
    COMMAND ${PYTHON_EXECUTABLE} "${AUTO_REG_SCRIPT}"
            --src-root "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --verbose
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "[AutoReg] Force-regenerating command registrations..."
    VERBATIM
)

# Custom target for dry-run/audit: cmake --build . --target audit-registry
add_custom_target(audit-registry
    COMMAND ${PYTHON_EXECUTABLE} "${AUTO_REG_SCRIPT}"
            --src-root "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --dry-run --verbose
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "[AutoReg] Auditing command registry coverage..."
    VERBATIM
)

# Function to add auto-registry to a target
function(setup_auto_feature_registry TARGET_NAME)
    if(NOT AUTO_FEATURE_REGISTRY_ENABLED)
        message(STATUS "[AutoReg] Skipped for ${TARGET_NAME} (Python not available)")
        return()
    endif()
    
    # Add generated sources to the target
    target_sources(${TARGET_NAME} PRIVATE
        "${AUTO_REG_OUTPUT_HPP}"
        "${AUTO_REG_OUTPUT_CPP}"
    )
    
    # Ensure regeneration happens before compilation
    add_dependencies(${TARGET_NAME} regenerate-registry)
    
    message(STATUS "[AutoReg] Enabled for target: ${TARGET_NAME}")
endfunction()
