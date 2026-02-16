# SourceManifest.cmake
# Globs all source files under src/ and Ship/ (same set as Verify-Build.ps1 ~1449 files),
# sets RAWRXD_ALL_SOURCES, and provides a target to generate source_manifest.json + RAWRXD_SOURCE_LIST.cmake.

set(_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(_SHIP_DIR "${CMAKE_SOURCE_DIR}/Ship")

file(GLOB_RECURSE _SRC_CPP "${_SRC_DIR}/*.cpp" "${_SRC_DIR}/*.h" "${_SRC_DIR}/*.hpp")
file(GLOB_RECURSE _SHIP_CPP "${_SHIP_DIR}/*.cpp" "${_SHIP_DIR}/*.h" "${_SHIP_DIR}/*.hpp")

# Relative paths for portability (optional: use for manifest; build targets use absolute via their own CMakeLists)
list(LENGTH _SRC_CPP _SRC_COUNT)
list(LENGTH _SHIP_CPP _SHIP_COUNT)
math(EXPR _TOTAL "${_SRC_COUNT} + ${_SHIP_COUNT}")

set(RAWRXD_ALL_SOURCES ${_SRC_CPP} ${_SHIP_CPP})
list(LENGTH RAWRXD_ALL_SOURCES RAWRXD_SOURCE_COUNT)

message(STATUS "[SourceManifest] src: ${_SRC_COUNT}  Ship: ${_SHIP_COUNT}  total: ${RAWRXD_SOURCE_COUNT}")

# Custom target: run Digest-SourceManifest.ps1 to generate source_manifest.json and RAWRXD_SOURCE_LIST.cmake in build dir
find_program(PWSH pwsh PATHS "C:/Program Files/PowerShell/7" "C:/Program Files (x86)/PowerShell/7" DOC "PowerShell 7")
if(NOT PWSH)
    find_program(PWSH powershell.exe DOC "PowerShell")
endif()
if(PWSH)
    add_custom_target(source_manifest
        COMMAND ${PWSH} -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/Digest-SourceManifest.ps1"
            -RepoRoot "${CMAKE_SOURCE_DIR}"
            -OutDir "${CMAKE_BINARY_DIR}"
            -Format both
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Digesting all source files (src + Ship) into source_manifest.json and RAWRXD_SOURCE_LIST.cmake"
        VERBATIM
    )
    message(STATUS "[SourceManifest] Target 'source_manifest' available: run to generate manifest in build dir")
else()
    message(STATUS "[SourceManifest] PowerShell not found; target 'source_manifest' not added. Run scripts/Digest-SourceManifest.ps1 manually.")
endif()
