# =============================================================================
# CommandTableAudit.cmake — CMake integration for COMMAND_TABLE coverage audit
# =============================================================================
# Provides build targets for validating COMMAND_TABLE coverage against
# Win32IDE.h IDM_* defines and handler declarations.
#
# Usage in CMakeLists.txt:
#   include(cmake/AutoFeatureRegistry.cmake)
#
# Targets:
#   audit-registry    — Run coverage audit, generate reports
#   audit-registry-ci — Same but exits non-zero if gaps found (for CI)
#
# The SSOT for command registration is:
#   src/core/command_registry.hpp  → COMMAND_TABLE X-macro
#   src/core/unified_command_dispatch.cpp → AutoRegistrar
#
# Architecture: C++20, Win32, no Qt, no exceptions
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

# Find Python
find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_FOUND)
    find_program(PYTHON_EXECUTABLE python python3)
    if(NOT PYTHON_EXECUTABLE)
        message(WARNING "[CommandAudit] Python not found — audit targets disabled")
        return()
    endif()
else()
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
endif()

set(AUDIT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/audit_command_table.py")

# Custom target: audit coverage and generate reports
add_custom_target(audit-registry
    COMMAND ${PYTHON_EXECUTABLE} "${AUDIT_SCRIPT}"
            --src-root "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --generate
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "[Audit] Running COMMAND_TABLE coverage audit..."
    VERBATIM
)

# CI-friendly target: fails if gaps exist
add_custom_target(audit-registry-ci
    COMMAND ${PYTHON_EXECUTABLE} "${AUDIT_SCRIPT}"
            --src-root "${CMAKE_CURRENT_SOURCE_DIR}/src"
            --quiet
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "[Audit] CI coverage gate..."
    VERBATIM
)

message(STATUS "[CommandAudit] Audit targets available: audit-registry, audit-registry-ci")
